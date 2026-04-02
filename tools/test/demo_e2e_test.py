#!/usr/bin/env python3
"""
demo_e2e_test.py — End-to-end test for Zacus Demo May 1st.

Validates all services are up and the voice pipeline works.

Usage:
    python3 tools/test/demo_e2e_test.py
    python3 tools/test/demo_e2e_test.py --skip-audio  # skip Audio Kit checks
"""

import argparse
import json
import sys
import time
import urllib.request
import urllib.error

# ── Configuration ──────────────────────────────────────
KXKM_AI = "http://kxkm-ai:11434"
TOWER_TTS = "http://192.168.0.120:8001"
STT_SERVER = "http://kxkm-ai:8901"
AUDIO_KIT = "http://192.168.0.50:8300"
VOICE_BRIDGE = "http://localhost:8080"

PASS = "\033[92mPASS\033[0m"
FAIL = "\033[91mFAIL\033[0m"
SKIP = "\033[93mSKIP\033[0m"

results = []


def check(name, url, path="/health", timeout=5):
    """Check if a service is healthy."""
    try:
        req = urllib.request.Request(f"{url}{path}", method="GET")
        resp = urllib.request.urlopen(req, timeout=timeout)
        data = resp.read().decode()
        results.append((name, True, f"HTTP {resp.status}"))
        print(f"  {PASS}  {name}: {url}{path} → {resp.status}")
        return True
    except Exception as e:
        results.append((name, False, str(e)))
        print(f"  {FAIL}  {name}: {url}{path} → {e}")
        return False


def test_ollama_model():
    """Check devstral model is loaded on KXKM-AI."""
    try:
        req = urllib.request.Request(f"{KXKM_AI}/api/tags")
        resp = urllib.request.urlopen(req, timeout=10)
        data = json.loads(resp.read())
        models = [m["name"] for m in data.get("models", [])]
        has_devstral = any("devstral" in m for m in models)
        if has_devstral:
            results.append(("Ollama devstral", True, "loaded"))
            print(f"  {PASS}  Ollama devstral: loaded ({len(models)} models)")
        else:
            results.append(("Ollama devstral", False, f"not found in {models}"))
            print(f"  {FAIL}  Ollama devstral: not found in {models}")
        return has_devstral
    except Exception as e:
        results.append(("Ollama devstral", False, str(e)))
        print(f"  {FAIL}  Ollama devstral: {e}")
        return False


def test_tts_synthesis():
    """Test Piper TTS can synthesize French speech."""
    try:
        payload = json.dumps({
            "text": "Test du professeur Zacus",
            "voice": "tom-medium",
        }).encode()
        req = urllib.request.Request(
            f"{TOWER_TTS}/api/tts",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        resp = urllib.request.urlopen(req, timeout=15)
        audio_bytes = len(resp.read())
        if audio_bytes > 1000:
            results.append(("TTS synthesis", True, f"{audio_bytes} bytes"))
            print(f"  {PASS}  TTS synthesis: {audio_bytes} bytes audio")
            return True
        else:
            results.append(("TTS synthesis", False, f"only {audio_bytes} bytes"))
            print(f"  {FAIL}  TTS synthesis: only {audio_bytes} bytes")
            return False
    except Exception as e:
        results.append(("TTS synthesis", False, str(e)))
        print(f"  {FAIL}  TTS synthesis: {e}")
        return False


def test_llm_chat():
    """Test Ollama devstral responds in character as Zacus."""
    try:
        payload = json.dumps({
            "model": "devstral",
            "messages": [
                {"role": "system", "content": "Tu es le Professeur Zacus, scientifique excentrique. Reponds en 1 phrase."},
                {"role": "user", "content": "Bonjour professeur !"},
            ],
            "stream": False,
        }).encode()
        req = urllib.request.Request(
            f"{KXKM_AI}/v1/chat/completions",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        t0 = time.time()
        resp = urllib.request.urlopen(req, timeout=30)
        elapsed = time.time() - t0
        data = json.loads(resp.read())
        text = data.get("choices", [{}])[0].get("message", {}).get("content", "")
        if text and elapsed < 10:
            results.append(("LLM chat", True, f"{elapsed:.1f}s"))
            print(f"  {PASS}  LLM chat ({elapsed:.1f}s): {text[:60]}...")
            return True
        else:
            results.append(("LLM chat", False, f"{elapsed:.1f}s, text={bool(text)}"))
            print(f"  {FAIL}  LLM chat: {elapsed:.1f}s, response={'yes' if text else 'empty'}")
            return False
    except Exception as e:
        results.append(("LLM chat", False, str(e)))
        print(f"  {FAIL}  LLM chat: {e}")
        return False


def test_scenario_compile():
    """Test scenario v3 compiles."""
    import subprocess
    try:
        result = subprocess.run(
            ["python3", "tools/scenario/compile_runtime3.py", "game/scenarios/zacus_v3.yaml"],
            capture_output=True, text=True, timeout=10,
        )
        if result.returncode == 0:
            results.append(("Scenario compile", True, "v3 OK"))
            print(f"  {PASS}  Scenario v3 compiles")
            return True
        else:
            results.append(("Scenario compile", False, result.stderr[:80]))
            print(f"  {FAIL}  Scenario compile: {result.stderr[:80]}")
            return False
    except Exception as e:
        results.append(("Scenario compile", False, str(e)))
        print(f"  {FAIL}  Scenario compile: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Zacus Demo E2E Test")
    parser.add_argument("--skip-audio", action="store_true", help="Skip Audio Kit checks")
    args = parser.parse_args()

    print("\n=== Zacus Demo May 1st — E2E Test ===\n")

    print("[1/6] Service Health Checks")
    check("Ollama", KXKM_AI, "/api/tags")
    check("STT Server", STT_SERVER, "/health")
    check("Piper TTS", TOWER_TTS, "/api/tts")
    if not args.skip_audio:
        check("Audio Kit", AUDIO_KIT, "/health")
    else:
        print(f"  {SKIP}  Audio Kit (--skip-audio)")

    print("\n[2/6] Ollama Model Check")
    test_ollama_model()

    print("\n[3/6] TTS Synthesis Test")
    test_tts_synthesis()

    print("\n[4/6] LLM Chat Test")
    test_llm_chat()

    print("\n[5/6] Scenario Compilation")
    test_scenario_compile()

    print("\n[6/6] Voice Pipeline Latency")
    # Combined: STT would be tested here with a WAV file
    # For now, just sum the individual latencies
    print(f"  (Manual test required with real microphone)")

    # Summary
    total = len(results)
    passed = sum(1 for _, ok, _ in results if ok)
    failed = total - passed

    print(f"\n{'='*50}")
    print(f"Results: {passed}/{total} passed, {failed} failed")
    if failed == 0:
        print(f"\n{PASS} All checks passed — demo ready!")
    else:
        print(f"\n{FAIL} {failed} checks failed — fix before demo")
        for name, ok, detail in results:
            if not ok:
                print(f"  - {name}: {detail}")
    print()

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()

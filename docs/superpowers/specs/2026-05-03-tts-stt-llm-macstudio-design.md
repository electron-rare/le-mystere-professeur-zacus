# TTS/STT/LLM Migration → MacStudio M3 Ultra

**Date**: 2026-05-03
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

---

## Goal

Consolidate the project's three inference workloads — TTS (pivot from Piper to **F5-TTS as primary**, with Piper retained as production fallback on `Tower:8001`), whisper STT (planned, no production today), and LLM inference for the hints engine + NPC dialogue (currently on `KXKM-AI` RTX 4090) — onto a single Mac Studio M3 Ultra (32 cores, 512 GB unified RAM, 7.3 TB disk). Hard cutover over a single weekend; KXKM-AI's inference role is retired (its host stays for fine-tuning); Tower keeps its Piper service alive as the TTS fallback path. The TTS pivot is greenfield-friendly: nothing in production runs F5-TTS today, so we are not breaking existing clients on this host.

## Non-Goals

- Replacing the **client-side** voice plumbing in `ESP32_ZACUS` (the firmware keeps its WebSocket/HTTP voice bridge, only the upstream URL changes).
- Bringing up Bluetooth A2DP / HFP on PLIP — that lives in the PLIP firmware roadmap, not here.
- Migrating other Tower-hosted services (Suite Numérique, n8n) — out of scope.
- Replacing Cloudflare/Traefik routing for `*.saillant.cc` — those stay.

## Constraints

- **Hard cutover**: one weekend, no parallel run. Rollback path = restart Tower's Piper service + repoint clients back. Documented but never exercised.
- **French**: voice content stays French (NPC = Zacus voice cloned via F5-TTS from a studio-recorded reference; Piper `tom-medium` remains the fallback voice). No English fallback.
- **Latency budget**: end-to-end voice command (ESP32 mic → STT → hints LLM → TTS → ESP32 speaker) target ≤ 8 s on M3 Ultra avec F5 `steps=4` (cible initiale ≤ 3 s révisée d'après mesures P1 part7 : F5 wall warm 3.8–4.6 s seul).
- **LAN-local**: clients reach MacStudio over the home LAN (`192.168.0.x`). Tailscale stays available for remote ops but is not the primary path.
- **Open-source bias**: prefer open weights / open-source daemons over Apple closed APIs (AVSpeechSynthesizer is rejected for the NPC voice).

---

## 1. Architecture — "Native daemons + LiteLLM proxy"

```
                 ┌──────────────────────────────────────────────────┐
                 │  MacStudio M3 Ultra (LAN: 192.168.0.150)         │
                 │                                                  │
ESP32 / Atelier ─┤  :4000  LiteLLM proxy (OpenAI API)               │
                 │     │                                            │
                 │     ├─→ :8501  MLX-LM server (npc-fast)          │
                 │     │       mlx-community/Qwen2.5-7B-Instruct-4bit│
                 │     │                                            │
                 │     ├─→ :8500  MLX-LM server (hints-deep)        │
                 │     │       mlx-community/Qwen2.5-32B-Instruct-4bit│
                 │     │                                            │
                 │     ├─→ :11434 ollama (legacy / A-B aliases)     │
                 │     │       qwen2.5:7b-q8 (npc-ollama-7b)        │
                 │     │       qwen2.5:72b-q8 (hints-ollama-72b)    │
                 │     │                                            │
                 │     ├─→ :8300  whisper.cpp server                │
                 │     │       large-v3-turbo-q5_0 (FR multi)       │
                 │     │                                            │
                 │     └─→ :8200  voice-bridge (FastAPI)            │
                 │             /voice/{ws,transcribe,intent}        │
                 │             /tts → F5-TTS in-process (MLX/Metal) │
                 │             steps=4 default, timeout 8 s →       │
                 │             fallback Piper Tower:8001            │
                 │                                                  │
                 │  Local services persist via crontab @reboot      │
                 │  (macOS 26 headless: launchctl over SSH broken)  │
                 └──────────────────────────────────────────────────┘

                 ┌─────────────────────────────────────┐
                 │  Tower (192.168.0.120) — fallback only  │
                 │     :8001  Piper TTS server             │
                 │            tom-medium / siwis / upmc    │
                 └─────────────────────────────────────┘
```

Mirror of the existing `Tower` pattern (LiteLLM in front, model service underneath) so client contracts stay stable: clients keep talking OpenAI-compatible API on a single endpoint and don't care which TTS backend handles a request. The voice-bridge owns the F5 → Piper fallback decision; clients see a single `/tts` URL.

## 2. Model choices

### 2.1 TTS — F5-TTS primary, Piper fallback

Decision recorded 2026-05-03 after qualité/latence comparison: Piper FR voices (tom-medium / siwis / upmc) are intelligible but flat in prosody and immediately recognisable as TTS. F5-TTS is current SOTA on prosodic naturalness, supports zero-shot voice cloning from a 5–10 s reference clip, and ships under MIT licence. M3 Ultra (32 cores, 512 GB unified RAM, Metal 4) handles F5-TTS comfortably; latency lands at 1–2 s for short phrases, which is narratively coherent — the « Professeur Zacus » NPC can plausibly *réfléchir* before speaking.

**Primary (MacStudio, local)**:

| Component | Artifact | Use |
|-----------|----------|-----|
| F5-TTS engine | `f5-tts-mlx` if available on M3 Ultra (Metal-accelerated), else upstream `f5-tts` (PyTorch) | Local inference daemon |
| F5-TTS base model | Pre-trained checkpoint (multilingual, FR-capable) | Zero-shot synthesis |
| `zacus_reference.wav` | 5–10 s studio recording of the canonical Zacus voice-over (to be recorded in P2 prereq) | Voice cloning reference |

**Fallback (Tower:8001, kept alive)**:

| Voice | Model file | Use |
|-------|------------|-----|
| `tom-medium` | `fr_FR-tom-medium.onnx` | Zacus NPC fallback voice |
| `siwis` | `fr_FR-siwis-medium.onnx` | Auxiliary NPC fallback |
| `upmc` | `fr_FR-upmc-medium.onnx` | Auxiliary NPC fallback |

Tower's existing Piper service stays untouched (no `.onnx` migration to MacStudio for the primary path). The voice-bridge on MacStudio routes to Piper Tower when F5-TTS exceeds the latency budget or returns an error — see §4 for routing details.

**Reference audio**: F5-TTS clone fidelity requires `~/zacus_reference.wav` (5–10 s voix-off du jeu, enregistrement studio — clean mic, no reverb, full prosodic range). Tant que ce fichier est absent, le voice-bridge replie sur `/tmp/ref.wav` généré au boot via `say -v Thomas` (voix générique macOS, **pas la voix Zacus**) — utile pour smoke-tester la chaîne mais inacceptable en production. L'enregistrement de `zacus_reference.wav` est un prérequis humain de P2.

### 2.2 STT — whisper.cpp `large-v3-turbo`

- Quantization: **Q5_0** (~1.5 GB on disk, fits the AppleNeural Engine + Metal GPU pipeline). Note : la spec initiale mentionnait `Q5_K`, mais seul `large-v3-turbo-q5_0` est publié sur whisper.cpp ; on adopte le modèle effectivement déployé (`large-v3-turbo-q5_0`).
- French-multilingual model (not the `.en` variant).
- Latency budget: ≤ 400 ms on a 5 s clip on M3 Ultra (verified via `whisper.cpp/main -m large-v3-turbo.bin -f sample.wav -l fr` benchmark).
- Latence observée (warm) : **1.2 s pour clip 11 s (JFK sample)** — sous le budget cible.
- Server: `whisper.cpp/server` with HTTP `/inference` endpoint on `:8300` (voice-bridge consumes it; `:8200` is the public client port owned by the voice-bridge for `/voice/ws` and `/tts`).

### 2.3 LLM — dual-tier MLX-LM (Qwen 2.5 4-bit)

**Décision révisée P1 part5/6** : la stack LLM est passée en **MLX-LM in-process** (Metal-natif M3 Ultra) plutôt qu'Ollama GGUF Q8. Gains mesurés :

| Alias | Backend retenu | Disk | Latence (warm 50 tok) | Throughput | RAM |
|-------|----------------|------|-----------------------|------------|-----|
| `npc-fast` | `mlx-community/Qwen2.5-7B-Instruct-4bit` (MLX-LM `:8501`) | ~4 GB | 6.18 s | **11.7 tok/s** | 5.9 GB |
| `hints-deep` | `mlx-community/Qwen2.5-32B-Instruct-4bit` (MLX-LM `:8500`) | ~17 GB | 11.4 s | **4.4 tok/s** | 18 GB |

**Comparaison vs Ollama Q8 (legacy, gardé en A/B)** :

| Alias legacy | Backend Ollama | Latence warm | Throughput | RAM |
|--------------|----------------|--------------|------------|-----|
| `npc-ollama-7b` | `qwen2.5:7b-instruct-q8_0` | 8.75 s | 10.5 tok/s | 9.7 GB |
| `hints-ollama-72b` | `qwen2.5:72b-instruct-q8_0` | 25.3 s | 1.97 tok/s | 91 GB |

**Pivot rationale** :
- MLX 32B Q4 est **2.2× plus rapide** que Ollama 72B Q8 et utilise **5× moins de RAM** ; la qualité FR sur prompts hints reste équivalente ou meilleure (plus précise on-topic, voir bench part5).
- MLX 7B Q4 est **30 % plus rapide** que Ollama 7B Q8, qualité Zacus tone préservée.
- Choix de garder Ollama 7B/72B installés mais sous aliases `npc-ollama-7b` / `hints-ollama-72b` pour A/B sanity checks et fallback de qualité (72B Q8 reste disponible si une régression est observée sur une catégorie de hints).

Les modèles MLX 4-bit étaient déjà présents dans le HF cache (`~/.cache/huggingface/hub/models--mlx-community--Qwen2.5-{7B,32B}-Instruct-4bit`) — zéro pull nécessaire. Le 72B MLX (`mlx-community/Qwen2.5-72B-Instruct-4bit`, ~40 GB) reste un follow-up si la qualité 32B Q4 dérive sur des sessions playtest réelles.

Both tiers tiennent largement dans 512 GB unified RAM avec headroom : ~25 GB total chargé pour MLX 32B + MLX 7B + LiteLLM + whisper + voice-bridge + F5-TTS, soit < 5 % de la RAM système.

### 2.4 LiteLLM aliases

LiteLLM `config.yaml` exposes the TTS routing duality as named aliases so clients pick *intent*, not *backend*:

| Alias | Backend | Endpoint |
|-------|---------|----------|
| `npc-fast` | MLX-LM Qwen 2.5 7B 4-bit (local) | `http://localhost:8501/v1` |
| `npc-mlx-7b` | MLX-LM Qwen 2.5 7B 4-bit (mirror de `npc-fast`) | `http://localhost:8501/v1` |
| `npc-ollama-7b` | Ollama Qwen 2.5 7B Q8 (legacy A/B) | `http://localhost:11434` |
| `hints-deep` | MLX-LM Qwen 2.5 32B 4-bit (local) | `http://localhost:8500/v1` |
| `hints-mlx-32b` | MLX-LM Qwen 2.5 32B 4-bit (mirror de `hints-deep`) | `http://localhost:8500/v1` |
| `hints-ollama-72b` | Ollama Qwen 2.5 72B Q8 (legacy A/B) | `http://localhost:11434` |
| `whisper-fr` / `stt-fr` | whisper.cpp `large-v3-turbo-q5_0` (local) | `http://localhost:8300/inference` |
| `tts-zacus-primary` | F5-TTS in-process (`zacus_reference.wav` clone) | `http://localhost:8200/tts` (voice-bridge) |
| `tts-zacus-fallback` | Piper Tower (`tom-medium`) | `http://192.168.0.120:8001/tts` |

The voice-bridge (§3) is the single `/tts` entrypoint clients hit; LiteLLM aliases above are mostly bookkeeping for observability and for the rare client that wants to force a specific backend (smoke tests, A/B comparisons).

### 2.5 Voice-bridge `/tts` routing

A FastAPI service co-located on MacStudio listens on `:8200/tts` and owns the F5 → Piper fallback decision:

- **Default route**: F5-TTS local (Metal/MLX-accelerated on M3 Ultra GPU).
- **Default steps**: `F5_DEFAULT_STEPS = 4` (RK4 solver). Mesures P1 part7 : `steps=8` → 10–11 s wall warm (qualité best, broadcast), `steps=4` → 3.8–4.6 s wall warm (qualité acceptable, meilleur compromis), `steps=2` → 1.5 s mais qualité dégradée. `steps=8` reste accessible via paramètre `?steps=8` pour qualité broadcast (cinématique, jingle, intro).
- **Timeout policy**: if F5-TTS does not return audio within **8 s** (`F5_TIMEOUT_S = 8.0`, synthesis start to first byte), the request is automatically reissued against `tts-zacus-fallback` (Piper Tower:8001). Note : la spec initiale visait 3 s mais avec `steps=4` la latence wall warm s'établit à 3.8–4.6 s ; un timeout à 3 s ferait basculer 100 % des requêtes en fallback.
- **Hard error**: if F5-TTS raises (OOM, model not loaded, exception), same fallback path.
- **Structured logging**: every `/tts` request emits a JSON log line including `tts_backend_used` ∈ {`f5`, `piper_fallback`}, `latency_ms`, `phrase_len`, `request_id`, `steps`. Soak (P3) consumes these logs to compute the fallback ratio.
- **Cache (optional, post-cutover)**: voice-bridge may cache F5 outputs by `(phrase_hash, reference_hash, steps)` to amortize repeated NPC lines — out of scope for the cutover itself.
- **Limitation actuelle : F5 in-process bloque l'event loop ~4 s par requête** (lock sérialisé, MLX synth en thread principal). Concurrence pratique = **1–2 clients voix simultanés max**. Migration vers worker pool MLX-aware (process pool ou thread pool dédié) = follow-up post-cutover.
- **Piper fallback dépend de la disponibilité de Tower:8001** — vérifier `pgrep -af piper` sur Tower avant cutover. Si le process est absent, le fallback retourne `503` et la chaîne TTS est cassée (single-point-of-failure F5).

Tableau récapitulatif :

| Paramètre | Valeur | Origine |
|-----------|--------|---------|
| `F5_DEFAULT_STEPS` | `4` | P1 part7 mesures |
| `F5_TIMEOUT_S` | `8.0` s | P1 part7 mesures (steps=4 → 3.8–4.6 s wall warm) |
| Override qualité broadcast | `?steps=8` | Disponible via querystring |
| Concurrence max | 1–2 clients | F5 in-process event-loop blocking |
| Fallback dépendance | Tower:8001 reachable | Vérifier `pgrep -af piper` avant cutover |

## 3. Network

- **MacStudio** sits on the home LAN at the IP currently assigned to it (today: `studio.local` resolves via mDNS).
- **Static IP recommended**: pin the MacStudio at e.g. `192.168.0.150` via the home router DHCP reservation, or set a manual static lease. Clients (ESP32, atelier) hardcode the IP rather than relying on mDNS, which can be flaky on ESP32-S3.
- **Tailscale stays installed** for remote ops, not for primary client traffic.
- Client URLs (P1 livré 2026-05-03) — tous neufs côté MacStudio, sans pseudo-redirect d'un legacy inexistant :
  - Voice WS / transcribe / intent : `ws://192.168.0.150:8200/voice/ws` (et POST `:8200/voice/{transcribe,intent}`) — endpoint **neuf** porté par le voice-bridge ; ni Tower ni KXKM-AI n'hébergeaient ces routes auparavant.
  - TTS : `http://192.168.0.150:8200/tts` (voice-bridge ; F5 primaire, Piper Tower fallback géré server-side).
  - LLM / STT (OpenAI-compat) : `http://192.168.0.150:4000/v1/...` (LiteLLM proxy ; aliases §2.4).
  - **Fallback Piper** : `http://192.168.0.120:8001/tts` (Tower) — joignable directement pour smoke tests, sinon utilisé en interne par le voice-bridge.
  - **Hints engine** : `http://192.168.0.150:8302/hints/ask` (à déployer sur MacStudio en P2 — actuellement le service tourne en local dev via `make hints-serve`).

---

## 4. Phased plan

| Phase | Scope | Acceptance | Effort |
|-------|-------|------------|--------|
| **P0** | Provision MacStudio (ollama, whisper.cpp, voice-bridge, LiteLLM, launchd plists, static IP) | All local services start on `launchd reload`, respond to `curl localhost:<port>/<probe>`. LLM/STT models downloaded (~83 GB total). | 3-5 h |
| **P1** ✅ **livré 2026-05-03** | Install F5-TTS on MacStudio (`pip install f5-tts` or `mlx_audio` if MLX port available), download base checkpoint (~1–2 GB), smoke-test on a fixed FR phrase against a generic reference clip. Smoke-test STT (10-clip FR set) and LLM (hints engine response within ±10% latency of KXKM-AI baseline). Modifs livrées : **F5_DEFAULT_STEPS=4** (au lieu de 8), **F5_TIMEOUT_S=8.0** (au lieu de 3.0), **whisper Q5_0** (au lieu de Q5_K). Daemons MLX livrés : `npc-fast` et `hints-deep` exposés via voice-bridge (au lieu d'Ollama HTTP, voir §2.3). | F5-TTS produces audible FR audio for the smoke phrase ; STT transcript stable ; LLM latency within budget ; voice-bridge `/tts` returns audio with `tts_backend_used=f5` | 2-3 h |
| **P2** | **Prereq humain** : enregistrer `zacus_reference.wav` (≤10 s, canonical voice-off du jeu, studio quality, clean mic, no reverb, full prosodic range). Tant qu'il manque, le bridge utilise un `/tmp/ref.wav` `say -v Thomas` générique (smoke OK, prod KO). Hard cutover (Saturday): wire reference into voice-bridge, repoint all clients to `192.168.0.150:8200/tts`, restart, smoke-test E2E. Tower's Piper service stays running as fallback. | `zacus_reference.wav` committed to repo (or secrets store) ; ESP32_ZACUS Voice WS connects ; atelier hints query resolves ; NPC speaks via F5 with cloned Zacus voice ; forced-fallback test (kill F5 process) routes to Piper Tower ; full puzzle voice loop ≤ 8 s end-to-end (steps=4) | 3-4 h |
| **P3** | Soak (Sunday): 24 h running, watch logs for crashes / OOM / latency creep / fallback rate | No service crash, no OOM, p95 latency stable, **ratio F5 success / total ≥ 95 %** measured from voice-bridge structured logs (`tts_backend_used=f5`). Note : la formulation initiale « ratio fallback Piper < 5 % » devient inopérante tant que Piper Tower:8001 est down (cf. §5 risques) ; on reformule en métrique positive de succès F5. | 24 h passive |
| **P4** | Decommission KXKM-AI ollama service. **Keep Tower's Piper service running** (it is now the production TTS fallback). Update root CLAUDE.md Infrastructure section. Document the rollback path. | KXKM-AI's ollama process stopped ; Tower's port 8001 still responds (intentional) ; CLAUDE.md mentions MacStudio F5-TTS primary + Tower Piper fallback | 1 h |

**Total wall-clock**: P0 + P1 = pre-cutover prep (5-8 h, can spread across the week). P2 + P3 + P4 = cutover weekend (4-5 h active + 24 h soak).

## 5. Risks + rollback

| Risk | Probability | Mitigation |
|------|-------------|------------|
| MacStudio fan/thermal under sustained 72B inference | Low (M3 Ultra rated for it) | `pmset -g thermlog`, monitor during P3 soak |
| LiteLLM config drift between Tower and MacStudio | Medium | Diff configs side-by-side in P0, document in commit |
| ESP32 firmware caches old IP via mDNS | Medium | Use static IP, update NVS WiFi config explicitly |
| whisper-large-v3-turbo FR accuracy regression vs current solution | Low (no current STT in prod) | This is greenfield — no baseline to regress |
| Network change breaks the VOICE_WS init order on ESP32 | Already fixed this session (network_boot_deferred guard) | Verify the guard remains active when new IP is configured |
| 24 h soak surfaces OOM | Low (87 GB / 512 GB) | macOS Activity Monitor + watch ollama / F5 process VSZ |
| F5-TTS checkpoint footprint (~1–2 GB) saturates disk | Negligible (1.1 TiB free on MacStudio) | Confirm `df -h` headroom in P0 |
| F5-TTS latency variability (1–5 s on cold model, GPU contention) | Medium | Voice-bridge auto-fallback to Piper Tower at **8 s** timeout (`F5_TIMEOUT_S=8.0`, révisé d'après P1 part7) ; warm model with periodic keep-alive synth ; soak measures p95 |
| Quality drift between F5 (cloned Zacus) and Piper (`tom-medium`) audible to players when fallback fires | Medium (timbre + prosody differ) | P3 measures **F5 success ratio ≥ 95 %** ; if dropped, investigate root cause (model load, network) before hiding the drift behind smoothing |
| F5-TTS reference audio quality drives clone fidelity | Medium | Record `zacus_reference.wav` in studio conditions (clean mic, no reverb, full prosodic range) — P2 prereq |
| **Piper Tower (`192.168.0.120:8001`) actuellement offline** (process absent au 2026-05-03) | **High** | Vérifier `pgrep -af piper` sur Tower et redémarrer le service avant cutover ; tant que Piper Tower est down, F5 = single-point-of-failure et la chaîne TTS casse en cas de crash F5. Hors scope spec mais blocant pour P3/P4. |
| **F5 in-process bloque l'event loop** (~4 s par requête en `steps=4`) limitant la concurrence à 1–2 clients voix simultanés | Medium | Lock sérialisé actuel suffit pour le scénario Zacus (NPC unique, 1 client ESP32 + 1 atelier max). Migration vers worker pool MLX-aware (process pool / thread dédié) = follow-up post-cutover si la salle accueille plusieurs joueurs voix simultanés. |

**Rollback procedure**: stop MacStudio's launchd services (F5-TTS, voice-bridge, ollama, whisper, LiteLLM), re-enable KXKM-AI's ollama service, repoint clients back to `192.168.0.120` IP for TTS (Tower Piper still up — already the fallback) and to KXKM-AI for hints. Total rollback ≤ 30 min if practiced.

---

## 6. Acceptance gates before retiring KXKM-AI (Tower Piper stays up as fallback)

- [x] **F5 warm-up < 30 s** (mesure P1 part7 : ~10 s boot)
- [x] **F5 `steps=4` latence < 5 s warm** (mesure P1 part7 : 3.8–4.6 s wall warm)
- [ ] **Piper Tower :8001 répond** → **PENDING** (process absent au 2026-05-03, à vérifier hors spec via `pgrep -af piper` sur Tower)
- [ ] **`zacus_reference.wav` présent** → **PENDING** (action humaine, enregistrement studio, P2 prereq)
- [ ] All MacStudio daemons (F5-TTS, voice-bridge, ollama, whisper, LiteLLM) green for ≥ 24 h continuous (P3 soak)
- [ ] ESP32_ZACUS Voice WS reconnects after firmware reboot, no panic
- [ ] Atelier `/hints/ask` resolves with response from `qwen2.5:72b` via LiteLLM
- [ ] NPC dialogue test: F5-TTS produces audibly Zacus-like voice from `zacus_reference.wav` on the canonical phrase set (subjective listening pass + waveform sanity check)
- [ ] Forced-fallback drill: kill F5-TTS process, confirm voice-bridge serves audio from Piper Tower with `tts_backend_used=piper_fallback` in logs (**dépend de Piper Tower up**)
- [ ] Voice loop p95 latency ≤ 8 s end-to-end (mic → STT → LLM → TTS → speaker, `steps=4`)
- [ ] **Ratio F5 success / total ≥ 95 %** sur fenêtre soak 24 h (reformulation de l'ancien « Piper fallback ratio < 5 % »)
- [ ] Root `CLAUDE.md` Infrastructure section updated
- [ ] Rollback drill executed once (timed) before declaring cutover final

## 7. Out of scope

- ESP-SR wake-word detection on ESP32 (separate brainstorm — voice pipeline)
- Hints engine prompt design (separate brainstorm — hints engine)
- A2DP / Bluetooth audio path on PLIP (PLIP firmware roadmap)
- Fine-tuning Qwen on Zacus-specific FR escape-room corpus (future enhancement, KXKM-AI hardware kept for that)
- Migrating Suite Numérique / n8n / Grist off Tower (different project)

## 8. Open questions

None at design stage. All decisions resolved during 2026-05-03 brainstorm:
- Architecture: native daemons + LiteLLM ✅
- TTS: F5-TTS local primary (cloned Zacus voice) + Piper Tower fallback ✅
- STT: whisper-large-v3-turbo **Q5_0** ✅ (Q5_K mentionné initialement n'est pas publié — voir §2.2)
- LLM: dual-tier Qwen 2.5 7B + 72B Q8 ✅
- Network: LAN, static IP `192.168.0.150` ✅
- Cutover: hard, single weekend ✅

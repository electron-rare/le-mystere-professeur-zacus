# TTS/STT/LLM Migration → MacStudio M3 Ultra

**Date**: 2026-05-03
**Status**: Approved (brainstorm session)
**Author**: L'électron rare (with Claude facilitation)

---

## Goal

Consolidate the project's three inference workloads — TTS (pivot from Piper to **F5-TTS as primary**), whisper STT (planned, no production today), and LLM inference for the hints engine + NPC dialogue (currently on `KXKM-AI` RTX 4090) — onto a single Mac Studio M3 Ultra (32 cores, 512 GB unified RAM, 7.3 TB disk). Hard cutover over a single weekend; KXKM-AI's inference role is retired (its host stays for fine-tuning). The TTS pivot is greenfield-friendly: nothing in production runs F5-TTS today, so we are not breaking existing clients on this host. Resilience strategy retires the previously planned Piper Tower fallback (broken in practice — see §5 — and timbre incompatible with cloned Zacus voice) in favour of three combined patterns: F5 watchdog, pre-rendered NPC phrase pool, and graceful `service_down.wav` degradation (see §2.1).

## Non-Goals

- Replacing the **client-side** voice plumbing in `ESP32_ZACUS` (the firmware keeps its WebSocket/HTTP voice bridge, only the upstream URL changes).
- Bringing up Bluetooth A2DP / HFP on PLIP — that lives in the PLIP firmware roadmap, not here.
- Migrating other Tower-hosted services (Suite Numérique, n8n) — out of scope.
- Replacing Cloudflare/Traefik routing for `*.saillant.cc` — those stay.

## Constraints

- **Hard cutover**: one weekend, no parallel run. Rollback path = revert clients to KXKM-AI for hints + temporary `service_down.wav` for TTS while F5 is restored. Documented but never exercised.
- **French**: voice content stays French (NPC = Zacus voice cloned via F5-TTS from a studio-recorded reference). No English fallback.
- **Latency budget**: end-to-end voice command (ESP32 mic → STT → hints LLM → TTS → ESP32 speaker) target ≤ 8 s on M3 Ultra avec F5 `steps=4` (cible initiale ≤ 3 s révisée d'après mesures P1 part7 : F5 wall warm 3.8–4.6 s seul).
- **LAN-local**: clients reach MacStudio over the home LAN (`192.168.0.x`). Tailscale stays available for remote ops but is not the primary path.
- **Open-source bias**: prefer open weights / open-source daemons over Apple closed APIs (AVSpeechSynthesizer is rejected for the NPC voice).
- **Resilience**: F5 watchdog (crontab `*/2 *`) + pre-rendered NPC phrase pool (cache disque indexé par `sha256(text+ref+steps)`) + `service_down.wav` graceful degradation. No backend voice fallback (Piper Tower retired from the live chain — voix incohérente avec clone F5, et réseau MacStudio ↔ Tower non routable en pratique).

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
                 │             /tts → cache lookup → F5-TTS         │
                 │                    in-process (MLX/Metal),       │
                 │                    steps=4 default, timeout 8 s  │
                 │                    → service_down.wav on error   │
                 │             /tts/service_down → hardcoded WAV    │
                 │                                                  │
                 │  Local services persist via crontab @reboot      │
                 │  (macOS 26 headless: launchctl over SSH broken)  │
                 │  F5 watchdog : crontab */2 * * * *               │
                 │  (relance voice-bridge si pgrep absent)          │
                 └──────────────────────────────────────────────────┘
```

Mirror of the existing `Tower` pattern (LiteLLM in front, model service underneath) so client contracts stay stable: clients keep talking OpenAI-compatible API on a single endpoint and don't care which TTS backend handles a request. The voice-bridge owns the cache → F5 → `service_down.wav` decision; clients see a single `/tts` URL.

## 2. Model choices

### 2.1 TTS — F5-TTS in-process

Decision recorded 2026-05-03 after qualité/latence comparison: Piper FR voices (tom-medium / siwis / upmc) are intelligible but flat in prosody and immediately recognisable as TTS. F5-TTS is current SOTA on prosodic naturalness, supports zero-shot voice cloning from a 5–10 s reference clip, and ships under MIT licence. M3 Ultra (32 cores, 512 GB unified RAM, Metal 4) handles F5-TTS comfortably; latency lands at 1–2 s for short phrases, which is narratively coherent — the « Professeur Zacus » NPC can plausibly *réfléchir* before speaking.

**Primary (MacStudio, local)**:

| Component | Artifact | Use |
|-----------|----------|-----|
| F5-TTS engine | `f5-tts-mlx` if available on M3 Ultra (Metal-accelerated), else upstream `f5-tts` (PyTorch) | Local inference daemon |
| F5-TTS base model | Pre-trained checkpoint (multilingual, FR-capable) | Zero-shot synthesis |
| `zacus_reference.wav` | 5–10 s studio recording of the canonical Zacus voice-over (to be recorded in P2 prereq) | Voice cloning reference |

**Reference audio**: F5-TTS clone fidelity requires `~/zacus_reference.wav` (5–10 s voix-off du jeu, enregistrement studio — clean mic, no reverb, full prosodic range). Tant que ce fichier est absent, le voice-bridge replie sur `/tmp/ref.wav` généré au boot via `say -v Thomas` (voix générique macOS, **pas la voix Zacus**) — utile pour smoke-tester la chaîne mais inacceptable en production. L'enregistrement de `zacus_reference.wav` est un prérequis humain de P2.

**Resilience strategy** (replaces the original Piper Tower fallback, retired 2026-05-03):

Backend voice fallback to Piper Tower a été abandonné après bench du 2026-05-03 pour deux raisons indépendantes : (1) le container Docker `openedai-speech` sur Tower:8001 n'expose aucune voix FR (mapping `voice_to_speaker.yaml` ne contient que des voix EN, donc le « fallback » ne produit pas de FR de toute façon), et (2) la route MacStudio → `192.168.0.120` n'est pas joignable depuis le LAN MacStudio (sous-réseaux distincts, vraisemblablement Tailscale-only). Surtout, même si Piper Tower fonctionnait, basculer F5 cloné Zacus → Piper `tom-medium` change radicalement le timbre — le joueur d'escape room entendrait « la voix du prof a changé », rupture narrative inacceptable.

Trois patterns combinés couvrent les vrais modes de défaillance de la chaîne TTS :

**(a) Watchdog F5 / voice-bridge** — `crontab` ligne `*/2 * * * * /Users/clems/voice-bridge/watchdog.sh` qui vérifie `pgrep -f "voice-bridge"` et relance via `nohup` si le process est mort. Couvre le seul scénario réel d'échec : crash runtime du voice-bridge ou du process F5. Livrable concret : script déposé en P1 part9 (voir §4).

**(b) Pre-rendered phrase pool** — `tools/tts/generate_npc_pool.py` (existant, Piper-based pour batch historique) adapté pour appeler `:8200/tts` et générer les ~200 phrases canoniques NPC en WAV mis en cache. Le voice-bridge consulte un cache disque `~/voice-bridge/cache/` indexé par `sha256(text + ref_hash + steps)` avant d'appeler F5. **Latence cache hit ≈ 0 ms** (lecture disque locale). Couvre 80–90 % des cues NPC scriptés (intro, transitions, réactions génériques) ; seul le LLM-rewrite dynamique passe par F5 live. Pré-requis : `~/zacus_reference.wav` (action humaine, sinon pool généré avec voix `say -v Thomas`).

**(c) Graceful degradation `service_down.wav`** — un fichier WAV unique pré-rendu (« Le Professeur Zacus est temporairement indisponible, un instant ») servi si F5 timeout (>8 s) ou exception. Évite le silence côté joueur (pattern « si tout casse, dis-le »). Servi en interne par le voice-bridge et exposé sur `:8200/tts/service_down` pour smoke tests + diagnostic.

| Pattern | Couvre | Livrable | Latence |
|---------|--------|----------|---------|
| Watchdog F5 | Process crash runtime | `~/voice-bridge/watchdog.sh` + ligne crontab | Détection ≤ 2 min |
| Pre-rendered pool | 80–90 % phrases scriptées | `generate_npc_pool.py` + cache `~/voice-bridge/cache/` | ≈ 0 ms (cache hit) |
| `service_down.wav` | Timeout / exception F5 | Fichier WAV pré-rendu + route `/tts/service_down` | Lecture disque |

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
| `tts-zacus-down` | Hardcoded `service_down.wav` (graceful degradation, smoke + diagnostic) | `http://localhost:8200/tts/service_down` |

The voice-bridge (§3) is the single `/tts` entrypoint clients hit; LiteLLM aliases above are mostly bookkeeping for observability and for the rare client that wants to force a specific backend (smoke tests, A/B comparisons). `tts-zacus-down` is intentionally exposed so opérateurs can vérifier la résilience sans tuer le process F5.

### 2.5 Voice-bridge `/tts` routing

A FastAPI service co-located on MacStudio listens on `:8200/tts` and owns the cache → F5 → `service_down.wav` decision:

- **Pipeline** :
  1. `cache_lookup(sha256(text + ref_hash + steps))` → si hit, sert le WAV depuis `~/voice-bridge/cache/` (~0 ms, lecture disque locale).
  2. Sinon, `f5_tts.generate(...)` avec timeout `F5_TIMEOUT_S = 8.0`.
  3. Si timeout / exception → sert `service_down.wav` + log `tts_backend_used="service_down"`.
  4. Cache miss + F5 OK → écriture cache pour réutilisation future (clé : `sha256(text + ref_hash + steps)`).
- **Default steps**: `F5_DEFAULT_STEPS = 4` (RK4 solver). Mesures P1 part7 : `steps=8` → 10–11 s wall warm (qualité best, broadcast), `steps=4` → 3.8–4.6 s wall warm (qualité acceptable, meilleur compromis), `steps=2` → 1.5 s mais qualité dégradée. `steps=8` reste accessible via paramètre `?steps=8` pour qualité broadcast (cinématique, jingle, intro).
- **Timeout policy**: if F5-TTS does not return audio within **8 s** (`F5_TIMEOUT_S = 8.0`, synthesis start to first byte), the request degrades to `service_down.wav`. Note : la spec initiale visait 3 s mais avec `steps=4` la latence wall warm s'établit à 3.8–4.6 s ; un timeout à 3 s ferait dégrader 100 % des requêtes.
- **Hard error**: if F5-TTS raises (OOM, model not loaded, exception), same `service_down.wav` path.
- **Structured logging**: every `/tts` request emits a JSON log line including `tts_backend_used` ∈ {`cache`, `f5`, `service_down`}, `latency_ms`, `phrase_len`, `request_id`, `steps`. Soak (P3) consumes these logs to compute cache hit ratio + F5 timeout ratio.
- **Limitation actuelle : F5 in-process bloque l'event loop ~4 s par requête** (lock sérialisé, MLX synth en thread principal). Concurrence pratique = **1–2 clients voix simultanés max**. Migration vers worker pool MLX-aware (process pool ou thread pool dédié) = follow-up post-cutover.

Tableau récapitulatif :

| Paramètre | Valeur | Origine |
|-----------|--------|---------|
| `F5_DEFAULT_STEPS` | `4` | P1 part7 mesures |
| `F5_TIMEOUT_S` | `8.0` s | P1 part7 mesures (steps=4 → 3.8–4.6 s wall warm) |
| Override qualité broadcast | `?steps=8` | Disponible via querystring |
| Concurrence max | 1–2 clients | F5 in-process event-loop blocking |
| Cache key | `sha256(text + ref_hash + steps)` | Pool pré-rendu + amortissement runtime |
| Dégradation gracieuse | `service_down.wav` | Sur timeout >8 s ou exception F5 |

## 3. Network

- **MacStudio** sits on the home LAN at the IP currently assigned to it (today: `studio.local` resolves via mDNS).
- **Static IP recommended**: pin the MacStudio at e.g. `192.168.0.150` via the home router DHCP reservation, or set a manual static lease. Clients (ESP32, atelier) hardcode the IP rather than relying on mDNS, which can be flaky on ESP32-S3.
- **Tailscale stays installed** for remote ops, not for primary client traffic.
- Client URLs (P1 livré 2026-05-03) — tous neufs côté MacStudio, sans pseudo-redirect d'un legacy inexistant :
  - Voice WS / transcribe / intent : `ws://192.168.0.150:8200/voice/ws` (et POST `:8200/voice/{transcribe,intent}`) — endpoint **neuf** porté par le voice-bridge ; ni Tower ni KXKM-AI n'hébergeaient ces routes auparavant.
  - TTS : `http://192.168.0.150:8200/tts` (voice-bridge ; cache → F5 → `service_down.wav` géré server-side).
  - LLM / STT (OpenAI-compat) : `http://192.168.0.150:4000/v1/...` (LiteLLM proxy ; aliases §2.4).
  - **Service down WAV** : `http://192.168.0.150:8200/tts/service_down` — diagnostic + smoke tests de la dégradation gracieuse.
  - **Hints engine** : `http://192.168.0.150:8302/hints/ask` (à déployer sur MacStudio en P2 — actuellement le service tourne en local dev via `make hints-serve`).

> Note historique : Tower (`192.168.0.120:8001`) reste référencé dans `~/CLAUDE.md` racine pour son usage batch hors-ligne — `tools/tts/generate_npc_pool.py` Piper-based génère encore des MP3 pool en arrière-plan. Cette chaîne batch reste valide tant qu'elle ne croise pas le runtime live de l'escape room. Aucun client live MacStudio ne tape Tower:8001 (route non routable + voix incompatible avec le clone F5).

---

## 4. Phased plan

| Phase | Scope | Acceptance | Effort |
|-------|-------|------------|--------|
| **P0** | Provision MacStudio (ollama, whisper.cpp, voice-bridge, LiteLLM, launchd plists, static IP) | All local services start on `launchd reload`, respond to `curl localhost:<port>/<probe>`. LLM/STT models downloaded (~83 GB total). | 3-5 h |
| **P1** ✅ **livré 2026-05-03** | Install F5-TTS on MacStudio (`pip install f5-tts` or `mlx_audio` if MLX port available), download base checkpoint (~1–2 GB), smoke-test on a fixed FR phrase against a generic reference clip. Smoke-test STT (10-clip FR set) and LLM (hints engine response within ±10% latency of KXKM-AI baseline). Modifs livrées : **F5_DEFAULT_STEPS=4** (au lieu de 8), **F5_TIMEOUT_S=8.0** (au lieu de 3.0), **whisper Q5_0** (au lieu de Q5_K). Sous-tâches livrées 2026-05-03 : **part5** MLX-LM 32B Q4 `hints-deep` ✅ ; **part6** MLX-LM 7B Q4 `npc-fast` ✅ ; **part7** voice-bridge daemon F5 in-process ✅ ; **part9** F5 watchdog crontab `*/2 *` + cache disque `~/voice-bridge/cache/` + `service_down.wav` pré-rendu et routé sur `/tts/service_down` ✅ ; **part10** bornage `steps`/`text` + IP canonique `100.116.92.12` ✅ ; **part11** voice-bridge 0.4.0 (Pydantic, `/health/ready`, slowapi rate-limit `/tts`, **endpoint `/voice/ws` STT streaming**) ✅. **part13** ✅ **livré 2026-05-04** — commit `d59282a` — voice-bridge 0.5 : **CORSMiddleware** (origines configurables via env `CORS_ORIGINS`) + injection **`NPC_SYSTEM_PROMPT`** persona (« Tu es le Professeur Zacus », accepte les variantes Zaku/Zacusse, FR ≤ 2 phrases) avant npc-fast dans `/voice/intent` et `/voice/ws`. Caller-supplied `messages` ou `system` override le default — back-compat préservée. | F5-TTS produces audible FR audio for the smoke phrase ; STT transcript stable ; LLM latency within budget ; voice-bridge `/tts` returns audio with `tts_backend_used=f5` ; watchdog cron actif ; `service_down.wav` jouable ; `/voice/ws` end-to-end OK ; **CORS configurable + persona Zacus injecté en amont npc-fast** | 2-3 h |
| **P2** | **Prereq humain** : enregistrer `zacus_reference.wav` (≤10 s, canonical voice-off du jeu, studio quality, clean mic, no reverb, full prosodic range). Tant qu'il manque, le bridge utilise un `/tmp/ref.wav` `say -v Thomas` générique (smoke OK, prod KO). Hard cutover (Saturday): wire reference into voice-bridge, repoint all clients to `192.168.0.150:8200/tts`, restart, smoke-test E2E. **Regen pool** : exécuter `tools/tts/generate_npc_pool.py` (adapté `:8200/tts`) une fois la voix Zacus disponible pour pré-remplir `~/voice-bridge/cache/` avec les ~200 phrases canoniques de `npc_phrases.yaml`. | `zacus_reference.wav` committed to repo (or secrets store) ; ESP32_ZACUS Voice WS connects ; atelier hints query resolves ; NPC speaks via F5 with cloned Zacus voice ; forced-degradation test (kill F5 process) sert `service_down.wav` puis watchdog relance le bridge < 2 min ; pool cache pré-rempli ≥ 80 % de `npc_phrases.yaml` ; full puzzle voice loop ≤ 8 s end-to-end (steps=4) | 3-4 h |
| **P3** | Soak (Sunday): 24 h running, watch logs for crashes / OOM / latency creep / cache hit ratio / F5 timeout ratio | No service crash, no OOM, p95 latency stable, **ratio cache hits ≥ 80 %** + **ratio F5 timeouts < 5 %** measured from voice-bridge structured logs (`tts_backend_used` ∈ {`cache`, `f5`, `service_down`}). Reformulation de l'ancien « ratio fallback Piper < 5 % » désormais sans objet (Piper retiré du chemin live). | 24 h passive |
| **P4** | Decommission KXKM-AI ollama service. Update root CLAUDE.md Infrastructure section. Document the rollback path. **Confirmer** que le watchdog F5 a redémarré le service ≥ 1 fois en 24 h sans crash visible côté client (preuve : log soak + sortie crontab). | KXKM-AI's ollama process stopped ; CLAUDE.md mentions MacStudio F5-TTS primary + watchdog + cache + `service_down.wav` (no Piper Tower in the live chain) ; watchdog evidence captured | 1 h |

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
| F5-TTS latency variability (1–5 s on cold model, GPU contention) | Medium | Voice-bridge dégrade vers `service_down.wav` à **8 s** timeout (`F5_TIMEOUT_S=8.0`, révisé d'après P1 part7) ; warm model with periodic keep-alive synth ; cache hit absorbe la majorité des cues scriptés ; soak measures p95 |
| F5-TTS reference audio quality drives clone fidelity | Medium | Record `zacus_reference.wav` in studio conditions (clean mic, no reverb, full prosodic range) — P2 prereq |
| **F5 in-process bloque l'event loop** (~4 s par requête en `steps=4`) limitant la concurrence à 1–2 clients voix simultanés | Medium | Lock sérialisé actuel suffit pour le scénario Zacus (NPC unique, 1 client ESP32 + 1 atelier max). Migration vers worker pool MLX-aware (process pool / thread dédié) = follow-up post-cutover si la salle accueille plusieurs joueurs voix simultanés. |
| **F5 watchdog dépend de `crond`** — si le démon cron macOS est mort, le watchdog ne tourne pas et un crash F5 reste invisible jusqu'à diagnostic manuel | Low (macOS `cron` runs by default, rarement tué) | Vérifier `sudo launchctl list \| grep cron` au déploiement ; alternative future = `launchd` user agent dédié (mais bloqué sur macOS 26 headless via SSH) |
| **Pool pré-rendu obsolète si `zacus_reference.wav` change** (clé cache inclut `ref_hash`, donc nouvelle ref = 100 % cache miss tant que pool pas régénéré) | Medium | Invalider tout le cache à chaque nouvelle ref (`rm -rf ~/voice-bridge/cache/*` puis re-run `tools/tts/generate_npc_pool.py`). Documenter dans le runbook P4. |
| **Surfaces no-auth exposées si MacStudio sort du LAN** (voice-bridge `/tts`, `/voice/{ws,transcribe,intent}`, `/voice/hook`, LiteLLM `:4000`, MLX-LM `:8500`/`:8501`, whisper `:8300` n'imposent aucune authentification — modèle de menace = LAN-only) | Medium | Documenté dans `tools/macstudio/README.md` Security section (commit `695b7f7`) : LAN-only assumption explicite, **checklist pre-deployment 4 étapes** (vérifier interface bind `127.0.0.1`/LAN-only, `pf`/firewall règle, rotation `MASTER_KEY` LiteLLM, `CORS_ORIGINS` restrictif), boot WARN si default master key détectée. À auditer avant toute exposition Tailscale ou WAN. |

**Rollback procedure**: stop MacStudio's services (F5-TTS, voice-bridge, ollama, whisper, LiteLLM via crontab @reboot lines disabled), re-enable KXKM-AI's ollama service, repoint clients back to KXKM-AI for hints. Pendant la fenêtre de rollback côté TTS, les clients reçoivent `service_down.wav` (pas de voix dynamique le temps de restaurer F5). Total rollback ≤ 30 min if practiced.

---

## 6. Acceptance gates before retiring KXKM-AI

- [x] **F5 warm-up < 30 s** (mesure P1 part7 : ~10 s boot)
- [x] **F5 `steps=4` latence < 5 s warm** (mesure P1 part7 : 3.8–4.6 s wall warm)
- [x] **F5 watchdog test** : `kill $(pgrep -f voice-bridge)` → relance auto < 2 min via crontab `*/2 *` ✅ (livré 2026-05-03, part9)
- [x] **Pool pre-render fonctionnel** (smoke 3 phrases F5 OK, cache `~/voice-bridge/cache/` peuplé sur 3 entrées de référence) ✅ (livré 2026-05-03, part9). Reste à étendre à ≥ 80 % de `npc_phrases.yaml` une fois `zacus_reference.wav` enregistré → **PENDING (P2 prereq humain)**
- [x] **`service_down.wav` jouable** + accepté narrativement (« Le Professeur Zacus est temporairement indisponible, un instant ») ✅ (livré 2026-05-03, part9)
- [x] **Steps clamp** (bornage `steps` ∈ [2, 16]) + **RateLimit slowapi `/tts`** ✅ (livré 2026-05-03, part10/part11)
- [x] **`/voice/ws` end-to-end** (hello → stream PCM → STT whisper → intent dispatch) ✅ (livré 2026-05-03, part11)
- [x] **CORSMiddleware actif** (origines via env `CORS_ORIGINS`) ✅ (livré 2026-05-04, part13, commit `d59282a`)
- [x] **NPC system prompt persona Zacus** injecté avant npc-fast (`/voice/intent` + `/voice/ws`, override caller-supplied préservé) ✅ (livré 2026-05-04, part13, commit `d59282a`)
- [x] **`.env.example` template + boot warns** (default master key détectée → WARN visible) ✅ (livré 2026-05-04, voir `tools/macstudio/README.md` Security)
- [x] **Pre-deployment security checklist** (LAN-only assumption, no-auth surfaces, 4 étapes audit) ✅ (livré 2026-05-04, commit `695b7f7`)
- [ ] **`zacus_reference.wav` présent** → **PENDING** (action humaine, enregistrement studio, P2 prereq)
- [ ] All MacStudio daemons (F5-TTS, voice-bridge, ollama, whisper, LiteLLM) green for ≥ 24 h continuous (P3 soak)
- [ ] ESP32_ZACUS Voice WS reconnects after firmware reboot, no panic
- [ ] Atelier `/hints/ask` resolves with response from `qwen2.5:72b` via LiteLLM
- [ ] NPC dialogue test: F5-TTS produces audibly Zacus-like voice from `zacus_reference.wav` on the canonical phrase set (subjective listening pass + waveform sanity check)
- [ ] Forced-degradation drill: kill F5-TTS process, confirm voice-bridge serves `service_down.wav` with `tts_backend_used=service_down` in logs, then watchdog relance le bridge < 2 min
- [ ] Voice loop p95 latency ≤ 8 s end-to-end (mic → STT → LLM → TTS → speaker, `steps=4`)
- [ ] **Ratio cache hits ≥ 80 %** + **ratio F5 timeouts < 5 %** sur fenêtre soak 24 h (reformulation de l'ancien « Piper fallback ratio < 5 % ») → **PENDING** (P3, pas encore mesuré sur 24 h)
- [ ] **Watchdog evidence**: confirmer que le watchdog F5 a redémarré le service ≥ 1 fois en 24 h sans crash visible côté client (preuve : log soak + sortie crontab)
- [ ] Root `CLAUDE.md` Infrastructure section updated
- [ ] Rollback drill executed once (timed) before declaring cutover final

## 7. Out of scope

- ESP-SR wake-word detection on ESP32 (separate brainstorm — voice pipeline)
- Hints engine prompt design (separate brainstorm — hints engine)
- A2DP / Bluetooth audio path on PLIP (PLIP firmware roadmap)
- Fine-tuning Qwen on Zacus-specific FR escape-room corpus (future enhancement, KXKM-AI hardware kept for that)
- Migrating Suite Numérique / n8n / Grist off Tower (different project)

## 8. Open questions

Résolues pendant le brainstorm 2026-05-03 :
- Architecture: native daemons + LiteLLM ✅
- TTS: F5-TTS local primary (cloned Zacus voice) + résilience watchdog/pool/`service_down.wav` ✅ (Piper Tower retiré du chemin live, voir §2.1)
- STT: whisper-large-v3-turbo **Q5_0** ✅ (Q5_K mentionné initialement n'est pas publié — voir §2.2)
- LLM: dual-tier Qwen 2.5 7B + 72B Q8 ✅
- Network: LAN, static IP `192.168.0.150` ✅
- Cutover: hard, single weekend ✅

Encore ouvertes :
- **Pool cache invalidation policy** quand `npc_phrases.yaml` change : full reset (`rm -rf ~/voice-bridge/cache/*` + re-run `generate_npc_pool.py`) ou diff-only (hash chaque phrase, ne re-render que les nouvelles / modifiées) ? Diff-only économise ~80 % du temps de pool regen mais demande un index `phrase_id → sha256` persistant côté outil.

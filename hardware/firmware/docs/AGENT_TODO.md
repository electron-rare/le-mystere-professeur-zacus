## [2026-02-24] SCENE_WIN_ETAPE demoscene runtime hardening (B1/B2 + morph + TXT/JSON)

- Skills chain appliquee pour la scene:
  1) `demoscene-demomaking-generic`
  2) `firmware-graphics-stack`
  3) `firmware-scene-ui-editor`
- Correctifs firmware (`ui_manager.h/.cpp`):
  - phase B split runtime: `B1` crash court (ms clamp 700..1000) puis `B2` interlude jusqu'a `B=15000`.
  - copper rendu circulaire/wavy via rings animees (pool existant reutilise, sans alloc per-frame).
  - wirecube: morph geometrique cube<->sphere (interpolation vertices + projection perspective existante).
  - scroller C: ping-pong wavy borne sur bande verticale demi-ecran (centre).
  - overrides runtime etendus: TXT+JSON (`MID_A_SCROLL`, `BOT_A_SCROLL`, `B1_MS`, `SPEED_MID_A`, `SPEED_BOT_A`, etc.).
  - instrumentation start/phase gardee et enrichie (B1, vitesses scroll).
- Docs/livrables:
  - calibration refraichie: `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md`.
  - exemple override TXT: `data/ui/scene_win_etape.txt`.
  - README scene mis a jour (A/B/C + B1/B2, contrat overrides TXT+JSON, notes perf).
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_021234_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_021234_status.txt`

## [2026-02-24] SCENE_WIN_ETAPE timing lock update (A/B/C long format)

- Timings Freenove verrouilles pour la sequence intro:
  - A = `30000 ms`
  - B = `15000 ms`
  - C = `20000 ms` (puis boucle C infinie tant que la scene reste active)
- Alignement doc runtime:
  - `hardware/firmware/ui_freenove_allinone/README.md` (section Intro Amiga92)
  - `docs/ui/SCENE_WIN_ETAPE_demoscene_calibration.md` (table calibration + defaults)
- Correctif build LVGL:
  - `LV_OPA_35` remplace par `LV_OPA_30` dans `ui_manager.cpp` pour compat LVGL 8.4.
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_014020_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_014020_status.txt`
- Verification:
  - `pio run -e freenove_esp32s3` ✅

## [2026-02-24] Skill pack demoscene v1.2 + UI skill sync

- Creation du skill global `demoscene-demomaking-generic` dans `~/.codex/skills/`:
  - `SKILL.md`, `agents/openai.yaml`, `references/*`, `templates/*.schema.json`,
  - scripts: `build_reference_pack.py`, `validate_reference_pack.py`, `validate_demo_spec.py`, `emit_trackset_example.py`.
- Contrat skill v1.2 aligne:
  - pipeline obligatoire `References -> ReferencePack -> StyleSheet/Timeline/TrackSet/FxGraph`,
  - baseline parametres chiffrés cracktro/clean/glitch/fireworks,
  - mode clean scroller par defaut: rollback `ping_pong`.
- Skills UI synchronises:
  - `~/.codex/skills/firmware-graphics-stack/SKILL.md` mis a jour (adapter workflow ReferencePack -> runtime LVGL).
  - `~/.codex/skills/firmware-scene-ui-editor/SKILL.md` mis a jour (scene tuning via tracks + validation `SCENE_GOTO`).
  - ajout metadata UI: `~/.codex/skills/firmware-scene-ui-editor/agents/openai.yaml`.
- Playbook repo synchronise:
  - `docs/AGENTS_COPILOT_PLAYBOOK.md` ajoute le nouveau skill et l'ordre recommande:
    1) `$demoscene-demomaking-generic`, 2) skill adapter UI, 3) validation hardware.
- Checkpoint securite execute:
  - `/tmp/zacus_checkpoint/20260224_011756_wip.patch`
  - `/tmp/zacus_checkpoint/20260224_011756_status.txt`

## [2026-02-24] SCENE_WIN_ETAPE intro Amiga92 JSON-first

- UiManager: intro A/B/C dediee a `SCENE_WIN_ETAPE` (state machine + `lv_timer` 33 ms), skip bouton/touch, cleanup strict des anims/objets intro.
- Runtime override JSON-only ajoute:
  - chemin canonique: `/SCENE_WIN_ETAPE.json`
  - compat lecture: `/ui/SCENE_WIN_ETAPE.json`
  - schema flexible: `logo_text`, `crack_scroll`, `clean_title`, `clean_scroll` (+ aliases legacy).
- Assets/exemple:
  - fichier exemple ajoute: `data/SCENE_WIN_ETAPE.json`.
- Documentation:
  - section `Intro Amiga92` + `References consulted` ajoutees dans `hardware/firmware/ui_freenove_allinone/README.md`.
- Verification:
  - `pio run -e freenove_esp32s3` OK.
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` OK.
  - `pio run -e freenove_esp32s3 -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` OK.
  - serial: `SCENE_GOTO SCENE_WIN_ETAPE` -> `ACK SCENE_GOTO ok=1`, override JSON charge depuis `/SCENE_WIN_ETAPE.json`.

## [2026-02-24] SCENE_WIN_ETAPE UI sequence (cracktro -> crash -> demo clean)

- UI sequence ajoutee pour `SCENE_WIN_ETAPE` en mode Freenove fireworks:
  - phase A (4-5s): raster/copper bars + starfield multi-couches + logo `PROFESSEUR ZACUS` en overshoot + scrolltext bas.
  - phase B (~0.9s): crash/glitch (blink + jitter + fade) avec burst firework.
  - phase C (6-8s): fond sobre type degrade, reveal titre `BRAVO BRIGADE Z`, puis scroll lent avec oscillation sinus.
- Implementation principale: `hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp` + nouveaux widgets/etat dans `include/ui_manager.h`.
- Verification:
  - `pio run -e freenove_esp32s3` OK
  - `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` OK
  - passage direct scene valide via serie: `SCENE_GOTO SCENE_WIN_ETAPE` (`ACK SCENE_GOTO ok=1`).

## [2026-02-23] Audit coherence global (source de verite: firmware Freenove)

- Validation story spec OK: `./tools/dev/story-gen validate`.
- Regeneration C++ story/apps OK: `./tools/dev/story-gen generate-cpp` (spec hash `a4e034ba637f`).
- Build firmware cible Freenove OK: `pio run -e freenove_esp32s3`.
- Contrat ESP-NOW corrige:
  - `SCENE` documente comme `SCENE <scenario_id>` (chargement scenario),
  - `SCENE_GOTO <scene_id>` documente comme extension one-shot Freenove.
- Drift checker cross-repo aligne:
  - `SCENE_GOTO` parsee explicitement,
  - marquee comme extension `OPTIONAL Zacus-only` pour eviter les faux positifs de drift.
- Coordination cross-repo rendue portable:
  - docs mises a jour pour utiliser `RTC_BL_PHONE_REPO` (ou `RTC_REPO`) au lieu d'un chemin machine hardcode.

## [2026-02-23] Coherence lock Freenove (Codex)

- Canonical hardware target: `freenove_esp32s3` on **ESP32-S3-WROOM-1-N16R8**.
- Canonical upload port for this setup: `/dev/cu.usbmodem5AB90753301` (usbmodem flow).
- Canonical one-shot scene jump command: `SCENE_GOTO <SCENE_ID>` (serial + API control action).
- API parity for one-shot jump: `POST /api/control` with `action=SCENE_GOTO SCENE_WIN_ETAPE`.
- UI text safety lock: UTF-8 to ASCII fallback is applied before label rendering in `ui_manager.cpp` to avoid missing-glyph artifacts on the Freenove display.
- Skill sync lock: `firmware-scene-ui-editor` uses `SCENE_GOTO` as primary visual validation path.

## [2026-02-23] Contrat ESP-NOW compact (1-pager)

- Ajout d'une fiche de synthèse pour intégration externe: `hardware/firmware/docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_QUICK.md` (format recommandé v1, réponses ACK, endpoints, erreurs).
- Ajout d'une version ultra-légère `hardware/firmware/docs/ESP_NOW_API_CONTRACT_FREENOVE_V1_MINI_SCHEMA.md` pour le second repo.
- Coordination API confirmée: la carte pair ESP-NOW Freenove est verrouillée sur `RTC_BL_PHONE` branche `esp32_RTC_ZACUS` (Freenove en broadcast-only).
- Ajout d'un document d'orchestration API: `docs/ESP_NOW_API_COORDINATION_AGENT.md`.
- Ajout d'une fiche interne de coordination cross-repo: `docs/CROSS_REPO_INTELLIGENCE.md` (contrôle bi-directionnel sans écriture RTC tant qu'autorisé).
- Ajout d'un script de contrôle drift: `tools/dev/check_cross_repo_espnow_contract.py`.
- Intégration du contrôle contractuel cross-repo dans `tools/dev/run_matrix_and_smoke.sh` via l'étape `cross_repo_contract_check` (env: `RTC_BL_PHONE_REPO`, `ZACUS_SKIP_CONTRACT_CHECK`).


## [2026-02-23] ESP-NOW Freenove (commandes runtime)

- Ajouté support ESP-NOW de `RING` (dispatch event story) et `SCENE <scenario_id>` (charge scénario par id) dans `executeEspNowCommandPayload` du firmware Freenove.
- Mise à jour du contrat ESP-NOW Freenove (`ESP_NOW_API_CONTRACT_FREENOVE_V1*`) pour documenter ces commandes côté pair intégré.
- Envoi ESP-NOW simplifié: `ESPNOW_SEND` force désormais l’envoi en broadcast (argument de target ignoré pour la compatibilité existante, WebUI simplifiée).


## [2026-02-23] Contrat ESP-NOW Freenove (v1)

- Ajout doc contrat API ESP-NOW Freenove: `hardware/firmware/docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md` (commandes, endpoints, exemples, erreurs/compatibilité).


## [2026-02-23] Enchainement scène LA → etape gagnée (Freenove)

- `STEP_ETAPE2` utilise désormais `SCENE_WIN_ETAPE` dans `data/story/scenarios/DEFAULT.json` (côté runtime actuel); `SCENE_SIGNAL_SPIKE` reste conservée en flux legacy/compat.
- Déploiement provisoire embarqué aligné: mise à jour de `hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp` (asset scenario par défaut + ajout `SCENE_WIN_ETAPE.json` intégré).
- Registre runtime mis à jour: `hardware/libs/story/src/resources/screen_scene_registry.cpp` et fallback C++ de scénario compilé `hardware/libs/story/src/generated/scenarios_gen.cpp`.
- Fichier scène ajouté: `data/story/screens/SCENE_WIN_ETAPE.json` (clone visuel de `SCENE_WIN` avec nouvel ID).
- Convergence des sources de vérité: `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` aligné sur `SCENE_WIN_ETAPE` + `SCENE_MEDIA_ARCHIVE`.
- Générateur story: ajout de `SCENE_WIN_ETAPE` dans `lib/zacus_story_gen_ai/src/zacus_story_gen_ai/generator.py` pour conserver le mapping canonical et régression C++ alignée.
- UI `SCENE_WIN_ETAPE`: texte dynamique (`Validation en cours...` / `BRAVO! vous avez eu juste`) selon l'état audio pour piloter l'attente de validation côté UI.

### Procédure smoke test Freenove

1. Vérifier la configuration du port série (résolution dynamique via cockpit.sh ports).
2. Flasher la cible Freenove avec cockpit.sh flash (ou build_all.sh).
3. Lancer le smoke test avec tools/dev/run_matrix_and_smoke.sh.
4. Vérifier le verdict UI_LINK_STATUS connected==1 (fail strict si absent),
   sauf en mode carte combinée (`ZACUS_ENV=freenove_esp32s3`) où la gate doit être `SKIP: not needed for combined board`.
5. Vérifier l’absence de panic/reboot dans les logs.
6. Vérifier la santé des WebSockets (logs, auto-recover).
7. Consigner les logs et artefacts produits (`logs/rc_live/*.log`, `artifacts/rc_live/<env>_<timestamp>/{meta.json,commands.txt,summary.md,...}`).
8. Documenter toute anomalie ou fail dans AGENT_TODO.md.

## TODO -- LA 440Hz en musique (en cours)

- [x] Script `verify_la_music_chain.py` aligné aux délais réels scène + statut MP3 + `--bg-music` optionnel.
- [x] Lissage DSP + matching LA en fenêtres courtes implémentés (cumul de stabilité).
- [x] Defaults runtime appliqués (`la_stable_ms=3000`, `la_release_ms=180`).
- [x] Run terrain 20 s sans `--bg-music` (musique externe déjà lancée), récupérer log et JSON:  
      ✅ `max_stable_ms=3000` obtenu en cumulé, no panic (log `/tmp/zacus_la_music_20s_retry5.log`, json `/tmp/zacus_la_music_20s_retry5.json`).
- [x] Run terrain 30 s + JSON, valider `max_stable_ms >= 3000` et `panic_seen=false`:  
      ✅ `max_stable_ms=3000` obtenu en cumulé, no panic (log `/tmp/zacus_la_music_30s_retry5.log`, json `/tmp/zacus_la_music_30s_retry5.json`).
- [x] Vérification script renforcée côté scènes: alias `SCENE_LA_DETECT` accepté, et attente de `audio`/`AUDIO_STATUS` plus robuste.
- [x] Vérifier visuellement la montée continue de `Stabilite ...` sur l’écran LA.
- [x] Contrôle build ciblé post-ajustements: `pio run -e freenove_esp32s3` ✅.
- [x] Ajustements fins de génération Python pour forcer davantage de LA cumulatif (anchor 70%, interruptions courtes) → passes 20/30 s.
- [ ] Gate finale: build + smoke + run terrain validés.

## [2026-02-23] Ajustement détection LA musique (Freenove)

- Mission: rendre la progression `la_stable_ms` cumulative sur `SCENE_LA_DETECTOR` (musique bruyante) sans relancer la fenêtre à chaque coupure courte.
- Implémentation:
  - ajout d'un lissage court pitch/confidence (3 trames) dans `HardwareManager` pour lisser `mic_freq_hz`, `mic_pitch_cents`, `mic_pitch_confidence`.
  - logique `LaTriggerService::update` adaptée pour cumuler `stable_ms` sur plusieurs fenêtres + exigence de continuité série courte.
  - état LA étendu avec `la_consecutive_match_count` / `la_match_start_ms` (`LaTriggerRuntimeState`).
  - fenêtre de continuité élargie côté config: `la_release_ms` portée à `180` dans config runtime embarquée (`APP_HARDWARE`).
  - nouveau script `tools/dev/verify_la_music_chain.py` ajouté:
    - génération LA longue + segments hors-LA/noise,
    - options: `--target-hz`, `--stable-ms`, `--duration`, `--bg-music`, `--bg-volume`, `--la-volume`, `--seed`, `--out-json`, `--log-file`,
    - logs automatiques dans `hardware/firmware/logs/verify_la_music_chain_*.log`,
    - sortie JSON métriques (`first_lock_ms`, `latency_to_lock`, `max_stable_ms`, `freq_at_lock`, `conf_max`, `panic_seen`),
    - musique de fond externe pilotée uniquement via `--bg-music` (facultatif).
- Vérification:
  - `pio run -e freenove_esp32s3` ✅
  - smoke/validation hardware en chaîne de test musique à lancer avec carte branchée (`tools/dev/verify_la_music_chain.py --duration 20 --target-hz 440`).
- Ajustement script en cours:
  - options ajoutées au test `verify_la_music_chain.py` pour les délais réels de scène: `--scene-stabilize-ms`, `--audio-warmup-ms`, `--first-tuner-sample-ms`.
  - logique de démarrage:
    - attente post-`SCENE_LA_DETECTOR`,
    - détection d'audio scène via `STATUS` (`audio=1` ou `track!=n/a`),
    - première capture de `MIC_TUNER_STATUS` avant chronométrage lock.
  - amélioration livrée:
    - polling `STATUS` + `AUDIO_STATUS` pour détecter le vrai démarrage audio scène,
    - capture de `audio_ready_ms` (et `audio_ready_scene_ms` en JSON),
    - logs de `stable_ms` à chaque variation (montées + reset),
    - métriques JSON enrichies (`first_tuner_sample_ms`, `audio_ready_ms`).

### [2026-02-23] Suite script LA musique — adaptation startup hardware

- Script ciblé ajusté:
  - `wait_screen()` renvoie désormais le temps réel d'entrée d'une scène (et exige des échantillons stables).
  - `wait_scene_audio()` attend explicitement le démarrage piste/lecture en scène (`audio`, `media_play`, `track`).
  - parseur `STATUS` renforcé pour capturer `media_play`, `track`, `pack` sans sur-généralisation.
  - métriques enrichies: `scene_locked_ms`, `scene_la_entry_ms`, `expected_stable_ms`, `duration_s`.
- Navigation renforcée:
  - ajout de `ensure_scene_loaded()` pour garantir `SCENE_LA_DETECTOR` avant démarrage des mesures LA.
  - enchaînement automatique BTN_NEXT (avec fallback `SC_LOAD DEFAULT`) si le script démarre depuis un écran inattendu.
- Comportement:
  - délai de démarrage scène/audio calculé à l'exécution, avec sauvegarde en JSON et logs détaillés.
  - compatible avec son externe déjà actif: `--bg-music` reste optionnel, le script ne dépend plus d'un lancement forcé d'une piste locale.
- Validation récente:
  - `python3 tools/dev/verify_la_music_chain.py --help` ✅
  - `python3 -m py_compile tools/dev/verify_la_music_chain.py` ✅
  - `pio run -e freenove_esp32s3` ✅

### [2026-02-23] PSRAM Freenove (Codex)

- Ajout des toggles build Freenove pour activer des allocations PSRAM ciblées (`FREENOVE_PSRAM_UI_DRAW_BUFFER`, `FREENOVE_PSRAM_CAMERA_FRAMEBUFFER`) et macro de profil (`FREENOVE_USE_PSRAM`).
- UI (`ui_manager.cpp`) : allocation dynamique du draw buffer LVGL en PSRAM avec fallback DRAM + logs de source/mémoire.
- Caméra (`camera_manager.cpp`) : framebuffer PSRAM activable via macro dédiée.
- Runtime (`main.cpp`) : logs de profil mémoire/PSRAM en boot pour valider capacité disponible.

### [2026-02-23] Alignement cible Freenove mémoire

- Cible Freenove corrigée vers **ESP32-S3-WROOM-1-N16R8**.
- Ajout d’un board custom `boards/freenove_esp32_s3_wroom_n16r8.json` pour refléter le profil 16MB/16MB (flash+PSRAM), avec alias utilisé par `env:freenove_esp32s3_full_with_ui`.
- Docs de mapping mises à jour (`docs/RC_FINAL_BOARD.md`, `hardware/firmware/ui_freenove_allinone/RC_FINAL_BOARD.md`).

### Verrou opératoire (demande utilisateur)

- Mode de travail verrouillé: utiliser uniquement les commandes `pio` pour build/flash/FS (`pio run ...`).
- Ne pas exécuter les scripts `tools/dev/*` tant que ce verrou est actif.

## [2026-02-23] SCENE_LA_DETECT: migration contrôlée vers `SCENE_LA_DETECTOR` (Codex)

- Source de vérité des scènes: `hardware/libs/story/src/resources/screen_scene_registry.cpp` garde le schéma canonique.
- Tolérance migration activée:
  - alias unique: `SCENE_LA_DETECT -> SCENE_LA_DETECTOR` (runtime + générateur).
  - `storyNormalizeScreenSceneId` accepte l’alias et le mappe vers le canonical.
- Alignement scénarios/payloads:
  - la plupart des sources de scenario/payload ont été migrées sur `SCENE_LA_DETECTOR`;
- les tests documentent la tolérance/normalisation (`generator.py`, `test_generator.py`, `test_story_fs_manager.cpp`).
- Build hygiene:
  - suppression du warning `SUPPORT_TRANSACTIONS redefined` en retirant les redéfinitions CLI en doublon pour les envs qui incluent `ui_freenove_config.h`.

## [2026-02-23] Freenove boutons + triggers (Codex)

- Refactor lecture boutons Freenove vers tâche dédiée + queue RTOS:
  - `ButtonManager` scanne les entrées en tâche dédiée (`scan_task`) et place les événements dans une queue.
  - garde-fou fallback synchrone si création queue/tâche RTOS échoue.
- Ajustement mapping runtime boutons:
  - `STEP_WAIT_UNLOCK` : tout appui (court/long) 1..5 => `BTN_NEXT` (fallback `NEXT` via `dispatchEvent`).
  - `SCENE_LA_DETECT` / `SCENE_LA_DETECTOR` (ou `STEP_WAIT_ETAPE2`) : réinitialisation du timeout scène LA sur tout appui 1..5.
- Nettoyage du revalidation scénario (suppression des cas de test physiques non actifs).
- Validation: `pio run -e freenove_esp32s3` ✅.

## [2026-02-23] Freenove audit + refactor agressif (RTOS/audio/wifi/esp-now/UI/story) (Codex)

- Checkpoint sécurité exécuté avant modifs:
  - branche: `main`
  - patch/status: `/tmp/zacus_checkpoint/20260223-011928_wip.patch`, `/tmp/zacus_checkpoint/20260223-011928_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun match.
- Audit runtime (focus Freenove) et constats:
  - `main.cpp` concentrait bootstrap + orchestration loop + parsing commandes série + bridge ESP-NOW + routes web + actions story (`hardware/firmware/ui_freenove_allinone/src/main.cpp:3137`, `hardware/firmware/ui_freenove_allinone/src/main.cpp:3241`, `hardware/firmware/ui_freenove_allinone/src/main.cpp:1532`).
  - callback ESP-NOW RX faisait trop de travail en chemin critique; allègement effectué: enqueue brut en callback, parsing envelope au `consume` (`hardware/firmware/ui_freenove_allinone/src/network_manager.cpp:910`, `hardware/firmware/ui_freenove_allinone/src/network_manager.cpp:564`).
  - duplication parsing JSON multi-clés dans ScenarioManager; factorisé via helper canonique (`hardware/firmware/ui_freenove_allinone/src/scenario_manager.cpp:82`, `hardware/firmware/ui_freenove_allinone/src/scenario_manager.cpp:506`).
  - provisioning LittleFS embarqué du bundle story répétitif; factorisé avec `provisionEmbeddedAsset` (`hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp:667`, `hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp:703`).
  - UI LVGL présentait des séquences style/hide/show répétées sur rings + overlay LA; extraction helpers `SceneElement`/`SceneState` (`hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp:312`, `hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp:1516`).
- Refactor/optimisations appliqués:
  - extraction config runtime APP_* dans service dédié (`hardware/firmware/ui_freenove_allinone/src/runtime/runtime_config_service.cpp:102`, usage `hardware/firmware/ui_freenove_allinone/src/main.cpp:3167`).
  - extraction logique LA/matching/timeout dans service dédié (`hardware/firmware/ui_freenove_allinone/src/runtime/la_trigger_service.cpp:183`, usage `hardware/firmware/ui_freenove_allinone/src/main.cpp:248`).
  - `HardwareManager` expose `snapshotRef()` + palette LED centralisée scène->RGB (`hardware/firmware/ui_freenove_allinone/src/hardware_manager.cpp:172`, `hardware/firmware/ui_freenove_allinone/src/hardware_manager.cpp:875`).
  - `UiManager` lit le snapshot hardware par référence stable (moins de copies hot-loop) via `setHardwareSnapshotRef` (`hardware/firmware/ui_freenove_allinone/src/main.cpp:3236`, `hardware/firmware/ui_freenove_allinone/src/ui_manager.cpp:252`).
  - action story `queue_audio_pack` branchée (ACTION_QUEUE_SONAR) pour audio hint piloté par scénario (`hardware/firmware/ui_freenove_allinone/src/main.cpp:1589`).
- Update scénario DEFAULT:
  - suppression redondance audio sur `STEP_U_SON_PROTO` (action media file doublonnait `audio_pack_id`).
  - ajout `ACTION_QUEUE_SONAR` sur `STEP_WAIT_ETAPE2`.
  - synchronisation fallback embarqué LittleFS avec le même contenu (`data/story/scenarios/DEFAULT.json:33`, `data/story/scenarios/DEFAULT.json:60`, `hardware/firmware/ui_freenove_allinone/src/storage_manager.cpp:69`).
- Fichiers manifests demandés:
  - `zacus_v1_audio.yaml` et `zacus_v1_printables.yaml` introuvables dans ce scope `hardware/firmware`; ignorés conformément à la consigne utilisateur.
- Validation build demandée par verrou utilisateur:
  - commande unique exécutée: `pio run -e freenove_esp32s3_full_with_ui`
  - run #1 (01:39): FAIL sur `scenario_manager.cpp` (`JsonObjectConst::as<>` invalide), corrigé via passage `JsonVariantConst`.
  - run #2 (01:40): PASS (`freenove_esp32s3_full_with_ui SUCCESS`, RAM 55.3%, Flash 73.1%).
  - timestamp evidence: `2026-02-23 01:40:39 +0100`.
- Ajustements audio + test ciblé (suite au passage précédent):
  - `audio_manager.cpp` passe en mode pump dédié FreeRTOS + protections d'accès concurrents (mutex, queue d'événements `done`) afin de réduire les latences de lecture.
  - `ui_freenove_config.h` stabilisé pour éviter la redéfinition `SUPPORT_TRANSACTIONS` (`#ifndef` avant `#define`).
  - calibration runtime: passage `vTaskDelay(1)` -> `taskYIELD()` quand le playback est actif (réduction de jitter potentielle de boucle decode).
  - build/flash vérifiés: `pio run -e freenove_esp32s3` ✅, `pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅.

## [2026-02-23] Migration Freenove vers ESP32-audioI2S (Codex)

- Checkpoint sécurité exécuté avant modifs:
  - branche: `main`
  - patch/status: `/tmp/zacus_checkpoint/20260223_151405_wip.patch`, `/tmp/zacus_checkpoint/20260223_151405_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun.
- Audio backend Freenove migré:
  - `ui_freenove_allinone/src/audio_manager.cpp` réécrit sur `ESP32-audioI2S` (`Audio.h`) avec maintien de l’API publique (`play/stop/update/status/profiles/fx/callback`).
  - normalisation des chemins audio (`/littlefs/...`, `/sd/...`) + validation de présence de fichier avant lecture.
  - détection codec/bitrate conservée pour le reporting (`AUDIO_STATUS`) avec scan MP3 header.
- Dépendances PlatformIO (env Freenove):
  - ajout `esphome/ESP32-audioI2S@^2.3.0`.
  - retrait de `earlephilhower/ESP8266Audio` sur l’env Freenove.
  - `build_src_filter` Freenove réduit aux unités Story réellement utilisées (`core/scenario_def`, `generated/scenarios_gen`, `resources/screen_scene_registry`, `scenarios/default_scenario_v2`, `ui/player_ui_model`) pour découpler le build des modules legacy audio.
- Validation:
  - `pio run -e freenove_esp32s3_full_with_ui` ✅ (`SUCCESS`, RAM 56.2%, Flash 80.1%).
  - `pio run -e freenove_esp32s3_full_with_ui` ✅ après découplage build Story (`SUCCESS`, RAM 56.2%, Flash 79.9%).
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅.
  - `pio device monitor -e freenove_esp32s3_full_with_ui --port /dev/cu.usbmodem5AB90753301` ✅.
- Incident runtime (triage RTOS/audio):
  - Repro stable d’un panic `Guru Meditation Error: Core 1 panic'ed (Double exception)` sur séquence série `AUDIO_STOP` puis `AUDIO_TEST_FS`.
  - Correctif `audio_manager.cpp`:
    - suppression du `stopSong()` redondant juste avant `connecttoFS`,
    - ajout d’un démarrage différé (file d’attente + cooldown 80ms) pour éviter stop/reopen immédiat sur le backend `ESP32-audioI2S`.
    - ajout d’un pump RTOS dédié audio (`audio_pump`, coeur 1, loop 1ms actif) avec mutex d’état + file d’événements `done` pour maintenir `Audio::loop()` hors charge UI/réseau et éviter la saccade.
  - Vérification post-correctif:
    - séquences répétées `AUDIO_STATUS`, `AUDIO_STOP`, `AUDIO_TEST_FS`, `AUDIO_STATUS` sans reboot/panic,
    - profils `AUDIO_FX` / `AUDIO_PROFILE` + `STATUS` valides après arrêt/reprise audio.
    - reflash + retest matériel validés (`pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301`) ; boot log confirme `pump task=1`.

## [2026-02-22] Verrou transition boutons -> LA detector (Codex)

- [x] Contrat verrouillé: depuis `STEP_WAIT_UNLOCK` (`SCENE_LOCKED`), tout appui bouton (`1..5`, court/long) déclenche `BTN_NEXT` et enchaîne vers `STEP_WAIT_ETAPE2`.
- [x] Spécification canonique alignée:
  - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` annoté + `screen_scene_id: SCENE_LA_DETECTOR`.
- [x] Data Story alignée:
  - `data/story/scenarios/DEFAULT.json`: `button_short_1..5 = BTN_NEXT`.
  - `data/story/scenarios/DEFAULT.json`: `STEP_WAIT_ETAPE2.screen_scene_id = SCENE_LA_DETECTOR`.
  - `data/story/screens/SCENE_LA_DETECTOR.json` ajouté (alias écran LA detector).
- [x] Validation hardware série (usbmodem Freenove):
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 3.0 --settle 3.0` ✅
  - `NEXT` direct validé: `STEP_WAIT_UNLOCK/SCENE_LOCKED -> STEP_WAIT_ETAPE2/SCENE_LA_DETECTOR` ✅
- [x] Correctif robustesse bouton:
  - `ScenarioManager::notifyButton`: en `STEP_WAIT_UNLOCK`, tout appui bouton (`1..5`, court **ou** long) déclenche `BTN_NEXT`.
  - revalidation runtime: `SC_REVALIDATE_HW` => `changed=1` pour `BTN1_SHORT`, `BTN3_LONG`, `BTN4_LONG`, `BTN5_SHORT`, `BTN5_LONG` ✅
- [x] Outillage test aligné:
  - `tools/dev/verify_story_default_flow.py`: attente `SCENE_LA_DETECTOR` + fallback robuste si `SC_LOAD` ne renvoie pas d'ACK.

## [2026-02-22] Focus validation micro + détection (hardware Freenove)

- [x] Correctifs runtime micro/tuner appliqués dans `hardware/firmware/ui_freenove_allinone/src/hardware_manager.cpp`:
  - capture I2S micro en `32-bit` puis conversion PCM16 (`>>8`) pour éviter les lectures saturées parasites,
  - détection LA recentrée sur bande utile (`320..560 Hz`) pour réduire les faux positifs hors contexte,
  - sortie tuner instantanée (`mic_freq_hz/mic_pitch_cents/mic_pitch_confidence`) sans lissage décroissant trompeur.
- [x] Validation firmware sur carte (port USB modem):
  - `pio run -e freenove_esp32s3_full_with_ui` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- [x] Validation live micro:
  - `./tools/dev/verify_mic_live.py --port /dev/cu.usbmodem5AB90753301 --samples 25 --duration 12` ✅
  - verdict observé: `mic_ready_all=1`, `conf_non_zero>0`, `freq_non_zero>0`.
- [x] Validation comportement détection fréquentielle (campagne locale via `afplay`):
  - 392 Hz -> médiane ~391 Hz (conf élevée),
  - 440 Hz -> médiane ~441 Hz (conf élevée),
  - 466 Hz -> médiane ~468 Hz (conf élevée),
  - 523 Hz -> médiane ~521 Hz (détection présente),
  - baseline silencieuse/ambiante: médiane confiance à `0` (quelques détections isolées restantes).
- [x] Non-régression Story:
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200 --timeout 2.2` ✅
  - flux validé: `SCENE_LOCKED -> SCENE_BROKEN -> SCENE_LA_DETECT -> SCENE_SIGNAL_SPIKE -> SCENE_MEDIA_ARCHIVE`.

## [2026-02-22] SCENE_LA_DETECTOR — UI instrumentée + trigger match micro strict (Codex)

- [x] UI `SCENE_LA_DETECT` enrichie (dans `ui_manager`):
  - waveform micro circulaire autour du noyau (input réel micro),
  - indicateur niveau micro (barre),
  - analyseur fréquentiel simplifié (barres),
  - indicateur de justesse (aiguille) + statut (`AUCUNE NOTE` / `PRESQUE JUSTE` / `LA VALIDE`),
  - compteur LA détecté et compteur timeout visibles à l’écran.
  - accordeur autour du LA au niveau des LED (WS2812) du freenove
- [x] Trigger gameplay LA ajusté:
  - validation LA continue à `3s` (`la_stable_ms=3000`) -> passage scène suivante,
  - timeout global à `60s` (`la_timeout_ms=60000`) -> retour direct `SCENE_LOCKED`.
- [x] Garde-fous “match micro obligatoire” sur l’étape LA:
  - blocage des raccourcis `NEXT/UNLOCK` manuels en `STEP_WAIT_ETAPE2` (série/web/control/boutons),
  - seule la validation micro peut déclencher la transition nominale.
- [x] Stabilité runtime:
  - correction stack canary loopTask (buffers DSP micro déplacés hors pile dans `HardwareManager`).
- [x] Validation exécutée (USB modem Freenove):
  - `pio run -e freenove_esp32s3_full_with_ui` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - logs série auto: absence `Guru Meditation`/panic ✅
  - logs série auto: `NEXT` bloqué en scène LA (`ACK NEXT ok=0`) ✅
  - logs série auto: timeout LA ~60s puis reset `STEP_WAIT_UNLOCK / SCENE_LOCKED` ✅
- [ ] Validation visuelle matérielle à confirmer:
  - lisibilité des 4 widgets UI (waveform/level/analyzer/timers),
  - calibration terrain pour atteindre `LA VALIDE` en 3s sur ton setup micro.

## [2026-02-22] Freenove lockscreen glitch + LEDs cassées (Codex)

- Renforcement de l'écran `SCENE_LOCKED`:
  - nouvel effet runtime dédié `kGlitch` (secousses aléatoires X/Y, flicker aléatoire opacité, particules aléatoires, contraste renforcé).
  - boucle d'effet enrichie pour les particules en mode `glitch` et `celebrate` (mode cassé) avec jitter non périodique.
  - reset explicite des translations LVGL entre scènes pour éviter les résidus d'animation.
  - waveform micro ajoutée sur `SCENE_LOCKED` en mode glitch mobile (scan vertical sur l'écran + distorsions aléatoires).
- Renforcement LEDs WS2812 (4 LEDs Freenove) en scène cassée:
  - pattern “broken hardware” aléatoire sur `SCENE_LOCKED` / `SCENE_BROKEN` / `SCENE_SIGNAL_SPIKE` (flashs brefs, non simultanés entre LEDs, coupures, sparks, dérive couleur).
  - vérifié en série via `HW_STATUS` (LED#1 majoritairement OFF avec flashs brefs intermittents).
- Data Story/FS alignées:
  - `data/story/screens/SCENE_LOCKED.json` + `data/screens/locked.json` mis à jour (timeline plus agressive, strobe 100, contraste élevé, texte: `Module U-SON PROTO` / `VERIFICATION EN COURS`).
  - fallback embarqué `/story/screens/SCENE_LOCKED.json` synchronisé dans `storage_manager.cpp`.
- Correctif stabilité:
  - régression stack canary détectée avec `StaticJsonDocument<4096>` dans `UiManager::renderScene`.
  - corrigé via `DynamicJsonDocument(4096)` (plus de panic observé sur rechargement scénario).
- Validations exécutées:
  - `pio run -e freenove_esp32s3_full_with_ui` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --baud 115200` ✅
  - vérif série: `[UI] scene=SCENE_LOCKED effect=6 speed=90 ... timeline=8` + absence `Guru Meditation` sur `SC_LOAD DEFAULT` ✅
  - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --timeout 2.2` ✅
    (flux attendu: `SCENE_LOCKED -> SCENE_BROKEN -> SCENE_LA_DETECT -> SCENE_SIGNAL_SPIKE -> SCENE_MEDIA_ARCHIVE`)
  - waveform micro déplacée sur un tracé circulaire autour du noyau écran (`scene_core_/scene_ring_outer_`) pour `SCENE_LA_DETECT`.
  - validation micro Freenove série en condition runtime (`STEP_WAIT_ETAPE2`, `SCENE_LA_DETECT`):
    - `mic_ready=1` sur tous les échantillons.
    - `mic_level_pct` variable (span 45), `mic_peak` non nul (max 32768), `mic_freq_hz>0` et `mic_pitch_confidence>0`.
  - variante scope renforcée:
    - double tracé circulaire (anneau principal + anneau externe) autour du noyau (`scene_core_`) pour visualiser la forme d'onde directement sur le cercle.
    - couleur/épaisseur pilotées par la justesse (`mic_pitch_cents`) et la confiance (`mic_pitch_confidence`) pour la détection LA.
  - nouvel outil de validation opérateur:
    - `tools/dev/verify_mic_live.py` (auto-load scénario, passage `SCENE_LA_DETECT`, sampling `HW_STATUS_JSON`, verdict PASS/FAIL).
    - run validé: `./tools/dev/verify_mic_live.py --port /dev/cu.usbmodem5AB90753301 --samples 20 --duration 10` ✅ (`mic_ready_all=1`, `level span=40`, `peak_max=32768`, `freq_non_zero=17`).
  - nettoyage anti-doublons:
    - purge des artefacts `* 2*` non trackés (copies parasites), restauration des fichiers trackés détectés par erreur hors scope firmware.
    - règles préventives ajoutées:
      - `.gitignore` racine: `**/* 2` + `**/* 2.*`
      - `hardware/firmware/.gitignore`: `* 2` + `* 2.*`
  - évolution accordeur:
    - LEDs (4 WS2812) en mode tuner plus lisible: `flat` à gauche (bleu/cyan), `in-tune` au centre (vert), `sharp` à droite (orange/rouge), avec flash extrême en forte dérive.
    - debug série live ajouté: `MIC_TUNER_STATUS [ON|OFF|<period_ms>]`.
  - évolution écran LA_DETECT:
    - rendu “vibromètre” accentué sur anneaux circulaires (amplitude/punch renforcés, warp de phase léger, boost niveau micro).
  - validations additionnelles:
    - `pio run -e freenove_esp32s3_full_with_ui` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - série: `MIC_TUNER_STATUS` one-shot + `MIC_TUNER_STATUS ON 180` (flux live observé) + `MIC_TUNER_STATUS OFF` ✅
    - `./tools/dev/verify_mic_live.py --port /dev/cu.usbmodem5AB90753301 --samples 20 --duration 10` ✅ (fallback `MIC_TUNER_STATUS` si `HW_STATUS_JSON` non décodable)
    - `python3 tools/dev/verify_story_default_flow.py --port /dev/cu.usbmodem5AB90753301 --timeout 2.2` ✅

## [2026-02-21] Firmware workflow hardening (Codex)

- PR de correction et merge: `feat/fix-firmware-story-workflow` → PR #105 → squash merge (main).
- Fichier touché: `.github/workflows/firmware-story-v2.yml`
- Correctifs:
  - indentation YAML corrigée sur `concurrency.cancel-in-progress` (anciennement bloquant l’exécution du workflow).
  - correction du répertoire de travail du job `story-toolchain` (`hardware/firmware` au lieu de `hardware/firmware/esp32_audio` inexistant).
  - alignement des artefacts de job `story-toolchain` vers les chemins existants.
- Validation:
  - PR checks: `story toolchain`, `validate`, `build env` (esp32dev/esp32_release/esp8266_oled/ui_rp2040_*) pass.
  - PR #105 mergeée (commit `04e4e50`) avec workflow OK.

## [2026-02-21] Coordination Orchestrator binôme (Kill_LIFE + Zacus + RTC)

- PR Kill_LIFE `#11` a été vérifiée et validée manuellement (`scope`):
  - scripts/tools d'orchestration (`tools/ai/zeroclaw_*`, y compris `zeroclaw_stack_up.sh`, `zeroclaw_hw_firmware_loop.sh`, `zeroclaw_watch_1min.sh`);
  - docs techniques `specs/*`.
- Vérification stricte:
  - `gh pr checks 11 --repo electron-rare/Kill_LIFE` : tous ✅ (`api_contract`, `lint-and-contract`, `ZeroClaw Dual Orchestrator`, etc.).
  - aucun artefact hors-scope ajouté dans le PR.
- Merge status:
  - PR fermé via squash (commit `e8e44048a8b36b7debcb12788183b34045dde7f2`) sur `main`.
  - commentaire de clôture codex publié (PASS) dans PR #11.
- Correctif suite: PR Kill_LIFE `#12` créé pour durcir le fallback mismatch:
  - `tools/ai/zeroclaw_hw_firmware_loop.sh` vérifie maintenant la stabilité du port USB avant fallback env.
  - merge PR #12 (squash) validé, commit `5f517ae82b95c1988d6c07e888194dcc28e02ff3`.
- Coordination inter-repos:
  - PR Zacus déjà alignée côté AP/ESP-NOW/WebUI (PRs/faits et mergeés précédemment dans ce chantier).
  - `RTC_BL_PHONE`: aucun PR ouvert en cours au moment du contrôle (`gh pr list --state open` vide).
- Actions de traçabilité:
  - pas de dette bloquante détectée.
  - prochaine étape: continuer surveillance post-merge (runbook ZeroClaw/CI + vérification liaison `watcher`).

## [2026-02-21] Freenove AP fallback dédié (anti-oscillation hors Les cils) (Codex)

- Contexte coordination merge train: PR Zacus #101 mergée, puis patch dédié AP fallback.
- Patch runtime réseau:
  - séparation SSID local vs AP fallback (`local_ssid=Les cils`, `ap_default_ssid=Freenove-Setup`)
  - nouveau flag config: `pause_local_retry_when_ap_client`
  - retry STA vers `Les cils` mis en pause si client connecté à l'AP fallback
  - nouveaux indicateurs exposés:
    - série `NET_STATUS`: `ap_clients`, `local_retry_paused`
    - WebUI `/api/status` + `/api/network/wifi`: `ap_clients`, `local_retry_paused`
- Fichiers touchés:
  - `data/story/apps/APP_WIFI.json`
  - `hardware/firmware/ui_freenove_allinone/include/network_manager.h`
  - `hardware/firmware/ui_freenove_allinone/src/network_manager.cpp`
  - `hardware/firmware/ui_freenove_allinone/src/main.cpp`
- Notes:
  - pas de constantes hardcodées nouvelles pour credentials AP en dehors de la config runtime/littlefs
  - comportement historique conservé si `pause_local_retry_when_ap_client=false`.
  - validations exécutées (PIO only):
    - `pio run -e freenove_esp32s3_full_with_ui` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
    - série `NET_STATUS`, `SC_REVALIDATE`, `SC_REVALIDATE_ALL`, `ESPNOW_STATUS_JSON` ✅
    - WebUI `GET /api/status`, `GET /api/network/wifi` (champs `ap_clients`, `local_retry_paused`) ✅
  - limite de validation:
    - le cas `local_retry_paused=1` nécessite un client connecté sur l'AP fallback pendant l'absence de `Les cils` (non reproduit sur ce run).

## [2026-02-21] Freenove story/ui/network pass final + revalidate step(x) (Codex)

- Checkpoint sécurité exécuté:
  - branche: `feat/freenove-webui-network-ops-parity`
  - patch/status: `/tmp/zacus_checkpoint/20260221_021417_wip.patch`, `/tmp/zacus_checkpoint/20260221_021417_status.txt`
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun.
- Correctif incohérence revalidate:
  - `SC_REVALIDATE_STEP2` renommé en `SC_REVALIDATE_STEPX`
  - logs enrichis avec `anchor_step=<step courant>` pour éviter l'ambiguïté multi-scénarios.
- Validations PIO:
  - `pio run -e freenove_esp32s3_full_with_ui` PASS (x2)
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS (x2)
- Evidence série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
  - `SC_COVERAGE scenario=DEFAULT unlock=1 audio_done=1 timer=1 serial=1 action=1`
  - `SC_REVALIDATE` OK avec `SC_REVALIDATE_STEPX ... anchor_step=STEP_WAIT_ETAPE2 ... step_after=STEP_ETAPE2`
  - `ESPNOW_STATUS_JSON` OK (`ready=true`, compteurs cohérents)
  - `ESPNOW_SEND broadcast ping` OK (`ACK ... ok=1`)
  - `NET_STATUS` local connecté validé (`sta_ssid=Les cils`, `local_match=1`, `fallback_ap=0`).
- Evidence WebUI:
  - `GET /api/status`, `GET /api/network/wifi`, `GET /api/network/espnow` OK
  - `POST /api/control` avec `SC_EVENT unlock UNLOCK` OK (`ok=true` + transition observée en série).

## [2026-02-20] GH binôme Freenove AP local + alignement ESP-NOW RTC (Codex)

- Branche de travail: `feat/freenove-ap-local-espnow-rtc-sync`
- Coordination GitHub:
  - issue firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/issues/91
  - issue RTC (binôme, branche `audit/telephony-webserver`): https://github.com/electron-rare/RTC_BL_PHONE/issues/6
  - PR firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/pull/92
- Runtime Freenove mis à jour:
  - AP fallback piloté par cible locale (`Les cils`) + retry périodique (`local_retry_ms`)
  - règle appliquée: AP actif si aucun WiFi connu n'est connecté (fallback), AP coupé quand la STA est reconnectée à `Les cils`
  - indicateurs réseau série ajoutés: `local_target`, `local_match`
  - commande série ajoutée: `ESPNOW_STATUS_JSON` (format RTC: ready/peer_count/tx_ok/tx_fail/rx_count/last_rx_mac/peers)
  - bootstrap peers ESP-NOW au boot via `APP_ESPNOW.config.peers`
  - WebUI embarquée (`http://<ip>/`) avec endpoints:
    - `GET /api/status`
    - `POST /api/wifi/connect`, `POST /api/wifi/disconnect`
    - `POST /api/espnow/send`
    - `POST /api/scenario/unlock`, `POST /api/scenario/next`
    - endpoints alignés RTC:
      - `GET /api/network/wifi`
      - `GET /api/network/espnow`
      - `GET/POST/DELETE /api/network/espnow/peer`
      - `POST /api/network/espnow/send`
      - `POST /api/network/wifi/connect`, `POST /api/network/wifi/disconnect`
      - `POST /api/control` (dispatch d'actions)
  - correction WebUI:
    - `WIFI_DISCONNECT` est maintenant différé (~250 ms) pour laisser la réponse HTTP sortir avant la coupure STA
  - Data story apps mises à jour:
    - `data/story/apps/APP_WIFI.json`: `local_ssid`, `local_password`, `ap_policy=if_no_known_wifi`, `local_retry_ms`
    - `data/story/apps/APP_ESPNOW.json`: `peers` + contrat payload enrichi
- Validations exécutées (PIO only):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - monitor série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
    - boot config: `local=Les cils ... ap_policy=0 retry_ms=15000`
    - `NET_STATUS ... local_target=Les cils local_match=1 ... fallback_ap=0` (local connecté)
    - `ESPNOW_STATUS_JSON` OK
    - `WIFI_DISCONNECT` => `fallback_ap=1` puis retry local
    - après reconnect WiFi: `ESPNOW_SEND broadcast ping` => recovery auto ESP-NOW + `ACK ... ok=1`
  - WebUI:
    - `GET /api/status` OK (`network/local_match`, `espnow`, `story`, `audio`)
    - `POST /api/scenario/unlock` et `POST /api/scenario/next` OK (transitions observées)
    - `POST /api/wifi/connect` OK
    - `POST /api/network/wifi/disconnect` => réponse HTTP `200` reçue avant coupure STA (plus de timeout systématique)
    - `POST /api/network/espnow/send` (payload JSON) OK
    - `POST /api/control` (`SC_EVENT unlock`, `WIFI_DISCONNECT`) OK
- Note d'incohérence traitée:
  - si AP fallback et cible locale partagent le même SSID (`Les cils`), le retry local coupe brièvement l'AP fallback pour éviter l'auto-association.

## [2026-02-21] Freenove WebUI parity réseau avec RTC (Codex)

- Branche de travail: `feat/freenove-webui-network-ops-parity`
- Tracking GitHub:
  - issue firmware: https://github.com/electron-rare/le-mystere-professeur-zacus/issues/93
  - coordination RTC: https://github.com/electron-rare/RTC_BL_PHONE/issues/6
  - PR RTC liée (ops web): https://github.com/electron-rare/RTC_BL_PHONE/pull/8
- Runtime WebUI enrichi (parité endpoints réseau):
  - `POST /api/network/wifi/reconnect`
  - `POST /api/network/espnow/on`
  - `POST /api/network/espnow/off`
  - routes existantes conservées (`/api/status`, `/api/network/wifi`, `/api/network/espnow`, `/api/network/espnow/peer`, `/api/network/espnow/send`, `/api/control`)
- Validation exécutée:
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - HTTP (`curl`):
    - `POST /api/network/espnow/off` => `{"action":"ESPNOW_OFF","ok":true}`
    - `POST /api/network/espnow/on` => `{"action":"ESPNOW_ON","ok":true}`
    - `POST /api/network/wifi/reconnect` => `{"action":"WIFI_RECONNECT","ok":true}`
    - `POST /api/control` (`ESPNOW_OFF`, `ESPNOW_ON`, `WIFI_RECONNECT`) => `ok=true`
  - monitor série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`):
    - `NET_STATUS` cohérent (`sta_ssid=Les cils`, `fallback_ap=0`)
    - `ESPNOW_STATUS_JSON` cohérent (`ready=1`)
    - logs observés: `[NET] ESP-NOW off` puis `[NET] ESP-NOW ready`

## [2026-02-20] Freenove WiFi/AP fallback local + ESP-NOW bridge/story + UI FX (Codex)

- Exigence appliquée: l'AP ne sert qu'en fallback si la carte n'est pas sur le WiFi local (`Les cils`).
- Runtime modifié:
  - boot auto-connect vers `Les cils`/`mascarade`
  - AP fallback auto uniquement en absence de connexion STA (et stop auto quand STA connecté)
- commandes série enrichies: `WIFI_CONNECT`, `WIFI_DISCONNECT`, `ESPNOW_PEER_ADD/DEL/LIST`, `ESPNOW_SEND <text|json>` (broadcast)
  - bridge ESP-NOW -> events scénario durci (texte + JSON `cmd/raw/event/event_type/event_name`)
- Story/UI:
  - intégration `APP_WIFI` + `APP_ESPNOW` dans les scénarios/story specs YAML
  - génération C++ story régénérée (`spec_hash=1834bcdab734`)
  - écran Freenove en mode effect-first (symboles/effets, titres cachés par défaut, vitesses d'effet pilotables par payload JSON)
- Vérifications exécutées:
  - `./tools/dev/story-gen validate` PASS
  - `./tools/dev/story-gen generate-cpp` PASS
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - contenu repo root via `.venv/bin/python`: validate/export scenario + audio manifest + printables manifest PASS
- Evidence série (115200):
  - boot: `[NET] wifi connect requested ssid=Les cils` + `[NET] boot wifi target=Les cils started=1`
  - statut validé: `NET_STATUS state=connected mode=STA ... sta_ssid=Les cils ... ap=0 fallback_ap=0`

## [2026-02-20] Freenove audio + écran + chaîne scénario/écran (Codex)

- Checkpoint sécurité exécuté: branche `main`, `git diff --stat` capturé, patch/status sauvegardés:
  - `/tmp/zacus_checkpoint/20260220_203024_wip.patch`
  - `/tmp/zacus_checkpoint/20260220_203024_status.txt`
- Scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun chemin tracké.
- Build/flash Freenove validés (`/dev/cu.usbmodem5AB90753301`):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS
- Validation série (`pio device monitor --port /dev/cu.usbmodem5AB90753301 --baud 115200`) :
  - `AUDIO_TEST` PASS (tonalité intégrée RTTTL, sans dépendance LittleFS)
  - `AUDIO_TEST_FS` PASS (`/music/boot_radio.mp3`)
  - `AUDIO_PROFILE 0/1/2` PASS (switch runtime pinout I2S)
  - `UNLOCK` -> transition `STEP_U_SON_PROTO`, audio pack `PACK_BOOT_RADIO`, puis `audio_done` -> transition `STEP_WAIT_ETAPE2` PASS.
- Incohérence restante observée au boot: warning HAL SPI `spiAttachMISO(): HSPI Does not have default pins on ESP32S3!` (non bloquant en exécution actuelle).

## [2026-02-20] Freenove story step2 hardware + YAML GitHub + stack WiFi/AP + ESP-NOW (Codex)

- Checkpoint sécurité exécuté: branche `main`, `git diff --stat` capturé, patch/status sauvegardés:
  - `/tmp/zacus_checkpoint/20260220_211024_wip.patch`
  - `/tmp/zacus_checkpoint/20260220_211024_status.txt`
- Scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`): aucun chemin tracké.
- Story specs YAML mis à jour (source of truth):
  - `docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml` enrichi avec transitions hardware pour l’étape 2:
    - `serial:BTN_NEXT`
    - `unlock:UNLOCK`
    - `action:ACTION_FORCE_ETAPE2`
  - génération revalidée:
    - `./tools/dev/story-gen validate` PASS
    - `./tools/dev/story-gen generate-cpp` PASS
    - `./tools/dev/story-gen generate-bundle` PASS
  - fichiers générés synchronisés (`hardware/libs/story/src/generated/*`), spec hash: `6edd9141d750`.
- GitHub pipeline Story restauré:
  - `.github/workflows/firmware-story-v2.yml` remplacé par un workflow fonctionnel (validate + generate-cpp + generate-bundle + check diff + artifact bundle).
- Runtime Freenove enrichi:
  - stack réseau ajoutée (`network_manager.{h,cpp}`) avec:
    - WiFi STA + AP management
    - ESP-NOW init/send/receive counters
    - bridge ESP-NOW payload -> événement scénario (`SERIAL:<event>`, `TIMER:<event>`, `ACTION:<event>`, `UNLOCK`, `AUDIO_DONE`)
  - commandes série ajoutées:
    - `NET_STATUS`, `WIFI_STATUS`, `WIFI_TEST`, `WIFI_STA`, `WIFI_AP_ON`, `WIFI_AP_OFF`
    - `ESPNOW_ON`, `ESPNOW_OFF`, `ESPNOW_STATUS`, `ESPNOW_SEND`
    - `SC_LIST`, `SC_LOAD`, `SC_REVALIDATE_ALL`, `SC_EVENT_RAW`
  - credentials test appliqués:
    - SSID test/AP par défaut: `Les cils`
    - mot de passe: `mascarade`
- Revalidation transitions via ScenarioSnapshot:
  - `SC_REVALIDATE` couvre events + hardware probes + vérifs ciblées step2:
    - `SC_REVALIDATE_STEP2 event=timer ... changed=1`
    - `SC_REVALIDATE_STEP2 event=action name=ACTION_FORCE_ETAPE2 ... changed=1`
    - `SC_REVALIDATE_STEP2 event=button label=BTN5_SHORT ... changed=1`
  - `SC_REVALIDATE_ALL` exécuté sur tous les scénarios built-in.
- Build/flash Freenove validés (`/dev/cu.usbmodem5AB90753301`):
  - `pio run -e freenove_esp32s3_full_with_ui` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` PASS
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` PASS (x2 après patch final)
- Validation série réseau:
  - `WIFI_AP_ON` PASS (`ssid=Les cils`, `mode=AP_STA`, `ip=192.168.4.1`)
  - `WIFI_AP_OFF` PASS
  - `WIFI_TEST` PASS (requête de connexion STA vers `Les cils`)
  - `ESPNOW_SEND` PASS (émission OK), pas de loopback RX local observé (`rx=0`) sur test mono-carte.

## TODO Freenove ESP32-S3 (contrat agent)

### [2026-02-17] Log détaillé des étapes réalisées – Freenove ESP32-S3

- Audit mapping hardware exécuté (artifacts/audit/firmware_20260217/validate_mapping_report.txt)
- Suppression des macros UART TX/RX pour UI Freenove all-in-one (platformio.ini, ui_freenove_config.h)
- Documentation détaillée des périphériques I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
- Synchronisation des macros techniques SPI/Serial/Driver dans ui_freenove_config.h (SPI_FREQUENCY, UI_LCD_SPI_HOST, etc.)
- Ajout d’une section multiplexage dans RC_FINAL_BOARD.md (partage BL/BTN3, CS TFT/BTN4)
- Ajout d’un commentaire explicite multiplexage dans ui_freenove_config.h
- Evidence synchronisée : tous les artefacts et logs sont tracés (artifacts/audit/firmware_20260217, logs/rc_live/freenove_esp32s3_YYYYMMDD.log)
- Traçabilité complète : chaque étape, correction, mapping, doc et evidence sont référencés dans AGENT_TODO.md

### [2026-02-17] Rapport d’audit mapping hardware Freenove ESP32-S3

**Correspondances parfaites** :
  - TFT SPI : SCK=18, MOSI=23, MISO=19, CS=5, DC=16, RESET=17, BL=4
  - Touch SPI : CS=21, IRQ=22
  - Boutons : 2, 3, 4, 5
  - Audio I2S : WS=25, BCK=26, DOUT=27
  - LCD : WIDTH=480, HEIGHT=320, ROTATION=1

**Écarts ou incohérences** :
  - UART : platformio.ini (TX=43, RX=44), ui_freenove_config.h (TX=1, RX=3), RC_FINAL_BOARD.md (non documenté)
  - I2C, LED, Buzzer, DHT11, MPU6050 : présents dans ui_freenove_config.h, absents ou non alignés ailleurs
  - SPI Host, Serial Baud, Driver : macros techniques non synchronisées
  - Multiplexage : BL/Bouton3 et CS TFT/Bouton4 partagent la broche, à clarifier

**Recommandations** :
  - Aligner UART TX/RX sur une valeur unique et documenter
  - Documenter I2C, LED, Buzzer, DHT11, MPU6050 dans RC_FINAL_BOARD.md
  - Synchroniser macros techniques dans ui_freenove_config.h et platformio.ini
  - Ajouter une note sur le multiplexage dans RC_FINAL_BOARD.md et ui_freenove_config.h
  - Reporter toute évolution dans AGENT_TODO.md

**Evidence** :
  - Rapport complet : artifacts/audit/firmware_20260217/validate_mapping_report.txt
  [2026-02-17] Synthèse technique – Détection dynamique des ports USB
    - Tous les ports USB série sont scannés dynamiquement (glob /dev/cu.*).
    - Attribution des rôles (esp32, esp8266, rp2040) selon mapping, fingerprint, VID:PID, fallback CP2102.
    - Esp32-S3 : peut apparaître en SLAB_USBtoUART (CP2102) ou usbmodem (mode bootloader, DFU, flash initial).
    - RP2040 : typiquement usbmodem.
    - Si un seul CP2102 détecté, fallback mono-port (esp32+esp8266 sur le même port).
    - Si plusieurs ports, mapping par location, fingerprint, ou VID:PID.
    - Aucun port n’est hardcodé : la détection s’adapte à chaque run (reset, flash, bootloader).
    - Les scripts cockpit.sh, resolve_ports.py, run_matrix_and_smoke.sh exploitent cette logique.
    - Traçabilité : logs/ports_debug.json, logs/ports_resolve.json, artifacts/rc_live/...
    - Documentation onboarding mise à jour pour refléter la procédure.
    - Recommandation : toujours utiliser la détection dynamique, ne jamais forcer un port sauf cas extrême (override manuel).

  [2026-02-17] Validation RC Live Freenove ESP32-S3
    - Port USB corrigé : /dev/cu.SLAB_USBtoUART
    - Flash ciblé relancé : pio run -e freenove_esp32s3 -t upload --upload-port /dev/cu.SLAB_USBtoUART
    - Evidence synchronisée : summary.md, ui_link.log, logs/rc_live/20260217-211606
    - Résultat : UI link FAIL (connected=1 mais status unavailable)
    - Artefacts : artifacts/rc_live/20260217-211606/summary.md, ui_link.log
    - Log détaillé ajouté dans AGENT_TODO.md

- [ ] Vérifier et compléter le mapping hardware dans platformio.ini ([env:freenove_esp32s3])
  - Pins, UART, SPI, I2C, etc. : vérifier cohérence avec la doc.
  - Exemple : GPIO22=I2S_WS, GPIO23=I2S_DATA, UART1=TX/RX.
- [ ] Aligner ui_freenove_config.h avec platformio.ini et la documentation RC_FINAL_BOARD.md
  - Vérifier que chaque pin, bus, périphérique est documenté et codé.
  - Ajouter commentaires explicites pour chaque mapping.
- [ ] Mettre à jour docs/RC_FINAL_BOARD.md
  - Décrire le mapping complet, schéma, photo, table des pins.
  - Ajouter procédure de flash/test spécifique Freenove.
  - Mentionner les différences avec ESP32Dev.
- [ ] Adapter build_all.sh, cockpit.sh, run_matrix_and_smoke.sh, etc.
  - Ajouter/valider la cible freenove_esp32s3 dans les scripts.
  - Vérifier que le build, flash, smoke, logs fonctionnent sur Freenove.
  - Exemple : ./build_all.sh doit inclure freenove_esp32s3.
- [ ] Vérifier la production de logs et artefacts lors des tests sur Freenove
  - Chemin : logs/rc_live/freenove_esp32s3_YYYYMMDD.log
  - Artefacts : artifacts/rc_live/freenove_esp32s3_YYYYMMDD.html
  - Référencer leur existence (chemin, timestamp, verdict) dans docs/AGENT_TODO.md.
- [ ] Mettre à jour l’onboarding (docs/QUICKSTART.md, docs/AGENTS_COPILOT_PLAYBOOK.md)
  - Ajouter section « Flash Freenove », « Test Freenove ».
  - Préciser les ports série, baud, procédure de résolution dynamique.
- [ ] Vérifier que les gates smoke et stress test sont compatibles et valident strictement la cible Freenove
  - Fail sur panic/reboot, verdict UI_LINK_STATUS connected==1.
  - Tester WebSocket health, stress I2S, scénario LittleFS.
- [ ] Documenter toute évolution ou correction dans docs/AGENT_TODO.md
  - Détailler les étapes réalisées, artefacts produits, impasses matérielles.
  - Exemple : « Test flash Freenove OK, logs produits dans logs/rc_live/freenove_esp32s3_20260217.log ».
## [2026-02-17] ESP32 pin remap test kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-185336_wip.patch` and `/tmp/zacus_checkpoint/20260217-185336_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link handshake still FAIL (`connected=0`) on latest cross-monitor evidence (`artifacts/ui_link_diag/20260217-174822/`), LittleFS fallback status unchanged, I2S stress status unchanged (last 30 min PASS).

## [2026-02-17] ESP32 UI pin remap execution (Codex)

- Applied ESP32 UI UART remap in firmware config: `TX=GPIO23`, `RX=GPIO18` (OLED side stays `D4/D5`).
- Rebuilt and reflashed ESP32 (`pio run -e esp32dev`, then `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9`).
- Cross-monitor rerun: `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-175606/`.
- Result moved from FAIL to WARN: ESP32 now receives HELLO frames (`esp32 tx/rx=48/20`, `UI_LINK_STATUS connected=1` seen), but ESP8266 still receives nothing from ESP32 (`esp8266 tx/rx=19/0`), so handshake remains incomplete.

## [2026-02-17] UI link diagnostic patch kickoff (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-181730_wip.patch` and `/tmp/zacus_checkpoint/20260217-181730_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Runtime status at kickoff: UI Link strict gate still FAIL (`connected=0`), LittleFS scenario load now mitigated by V2 fallback, I2S stress status currently PASS on last 30 min run.

## [2026-02-17] Cross-monitor boot capture (Codex)

- Built and flashed diagnostics on both boards: `pio run -e esp32dev -e esp8266_oled`, then upload on `/dev/cu.SLAB_USBtoUART9` (ESP32) and `/dev/cu.SLAB_USBtoUART` (ESP8266).
- Captured synchronized boot monitors with `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22`.
- Evidence: `artifacts/ui_link_diag/20260217-173005/` (`esp32.log`, `esp8266.log`, `merged.log`, `summary.md`, `ports_resolve.json`, `meta.json`).
- Verdict remains FAIL: no discriminant RX observed on either side (`ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`), indicating traffic still not seen on D4/D5 at runtime.

## [2026-02-17] Cross-monitor rerun after pin inversion (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after manual pin inversion test.
- Evidence: `artifacts/ui_link_diag/20260217-174330/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] Cross-monitor rerun after inverted wiring confirmation (Codex)

- Re-ran `python3 tools/dev/capture_ui_link_boot_diag.py --seconds 22` after updated wiring check.
- Evidence: `artifacts/ui_link_diag/20260217-174822/`.
- Verdict unchanged: FAIL with `ESP32 tx/rx=48/0`, `ESP8266 tx/rx=19/0`, no `connected=1`.

## [2026-02-17] D4/D5 handshake + strict firmware_tests rerun (Codex)

- Safety checkpoint re-run via cockpit wrappers: `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-173556_wip.patch` and `/tmp/zacus_checkpoint/20260217-173556_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths.
- Build gate rerun: `./build_all.sh` PASS (5/5 envs) after firmware updates.
- Reflash completed: `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART9` and `pio run -e esp8266_oled -t upload --upload-port /dev/cu.SLAB_USBtoUART`.
- `firmware_tooling` rerun PASS (`plan-only` + full run); all `--help` paths stay non-destructive.
- `firmware_tests` strict rerun (`ZACUS_REQUIRE_HW=1`) still FAIL at gate 1: `artifacts/rc_live/20260217-164154/summary.md` (`UI_LINK_STATUS connected=0`, esp8266 monitor FAIL).
- Strict smoke rerun uses repo-root resolver (`tools/test/resolve_ports.py`) and strict ESP32+ESP8266 mapping; gate FAIL remains UI link only (`artifacts/smoke_tests/20260217-164237/smoke_tests.log`), while `STORY_LOAD_SCENARIO DEFAULT` now succeeds through fallback (`STORY_LOAD_SCENARIO_FALLBACK V2 DEFAULT` + `STORY_LOAD_SCENARIO_OK`).
- `audit_coherence.py` rerun PASS (`artifacts/audit/20260217-164243/summary.md`).
- Content validators rerun PASS from repo root: scenario validate/export + audio manifest + printables manifest.
- Stress rerun completed PASS (`python3 tools/dev/run_stress_tests.py --hours 0.5`): `artifacts/stress_test/20260217-164248/summary.md` (`87` iterations, success rate `100.0%`, no panic/reboot markers in log).
- Runtime status after reruns: UI Link = FAIL (`connected=0`), LittleFS default scenario = mitigated by V2 fallback in serial path, I2S stability = PASS over 30 min stress run (no panic/reboot markers).

## [2026-02-17] Fix handshake/UI smoke strict kickoff (Codex)

- Safety checkpoint executed via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-155753_wip.patch` and `/tmp/zacus_checkpoint/20260217-155753_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Runtime status at kickoff: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S stability = FAIL/intermittent panic in stress recovery path.

## [2026-02-17] Copilot sequence checkpoint (firmware_tooling -> firmware_tests)

- Safety checkpoint executed before edits via cockpit wrappers: branch `story-V2`, `git status`, `git diff --stat`, `git branch`.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-162509_wip.patch` and `/tmp/zacus_checkpoint/20260217-162509_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths; no untrack action required.
- Kickoff runtime status: UI Link still failing in prior smoke evidence, default LittleFS scenario still missing in prior QA notes, I2S panic already known in stress path.

## [2026-02-17] Copilot sequence execution + alignment fixes

- Plan/runner alignment applied: root `tools/dev/plan_runner.sh` now delegates to `hardware/firmware/tools/dev/plan_runner.sh`; firmware runner now executes from repo root and resolves active agent IDs recursively under `.github/agents/` (excluding `archive/`).
- `run_matrix_and_smoke.sh` now supports `--help`/`-h` without side effects and returns explicit non-zero on unknown args (`--bad-arg` -> rc=2).
- Agent briefs synchronized in root + firmware copies (`domains/firmware-tooling.md`, `domains/firmware-tests.md`) with repo-root commands and venv-aware PATH (`PATH=$(pwd)/hardware/firmware/.venv/bin:$PATH`).
- `firmware_tooling` sequence: PASS (`bash hardware/firmware/tools/dev/plan_runner.sh --agent firmware_tooling`).
- `firmware_tests` strict sequence (`ZACUS_REQUIRE_HW=1`): blocked at step 1 because `run_matrix_and_smoke` reports UI link failure; evidence `artifacts/rc_live/20260217-153129/summary.md` (`UI_LINK_STATUS connected=0`).
- Remaining test gates executed manually after the blocked step:
  - `run_smoke_tests.sh` strict: FAIL (port resolution), evidence `artifacts/smoke_tests/20260217-153214/summary.md`.
  - `run_stress_tests.py --hours 0.5`: FAIL, scenario does not complete (`DEFAULT`), evidence `artifacts/stress_test/20260217-153220/summary.md`; earlier run also captured I2S panic evidence `artifacts/stress_test/20260217-153037/stress_test.log`.
  - `audit_coherence.py`: initially FAIL on missing runbook refs, then PASS after `cockpit_commands.yaml` runbook fixes + regenerated commands doc; evidence `artifacts/audit/20260217-153246/summary.md` (latest PASS).
- RP2040 docs/config audit: no env naming drift detected (`ui_rp2040_ili9488`, `ui_rp2040_ili9486`) across `platformio.ini`, `build_all.sh`, `run_matrix_and_smoke.sh`, `docs/QUICKSTART.md`, and `docs/TEST_SCRIPT_COORDINATOR.md`.
- Runtime status after this pass: UI Link = FAIL (`connected=0`), LittleFS default scenario = missing (`/story/scenarios/DEFAULT.json`), I2S panic = intermittent (observed in stress evidence above).

## [2026-02-17] Audit kickoff checkpoint (Codex)

- Safety checkpoint run from `hardware/firmware`: branch `story-V2`, working tree dirty (pre-existing changes), `git diff --stat` captured before edits.
- Checkpoint files saved: `/tmp/zacus_checkpoint/20260217-141814_wip.patch` and `/tmp/zacus_checkpoint/20260217-141814_status.txt`.
- Tracked artifact scan (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) returned no tracked paths, so no untrack action was needed.
- Runtime status at kickoff: UI Link `connected=0` in previous smoke evidence, LittleFS default scenario still flagged as missing by prior QA notes, I2S panic previously reported in stress recovery path.

## [2026-02-17] Audit + coherence pass (Codex)

- Tooling/scripts corrected: `tools/dev/run_matrix_and_smoke.sh` (broken preamble/functions restored, non-interactive USB path now resolves ports, accepted `learned-map` reasons, story screen skip evidence), `tools/dev/serial_smoke.py` (ports map loading re-enabled, dead duplicate role code removed), `tools/dev/cockpit.sh` (`rc` no longer forces `ZACUS_REQUIRE_HW=1`, debug banner removed), `tools/dev/plan_runner.sh` (portable paths/date and safer arg parsing), UI OLED HELLO/PONG now built with protocol CRC via `uiLinkBuildLine`.
- Command registry/doc sync fixed: `tools/dev/cockpit_commands.yaml` normalized under one `commands:` key (including `wifi-debug`) and regenerated `docs/_generated/COCKPIT_COMMANDS.md` now lists full evidence columns without blank rows.
- Protocol/runtime check: `protocol/ui_link_v2.md` expects CRC frames; OLED runtime now emits CRC on HELLO/PONG. Remaining runtime discrepancy logged: Story V2 controller still boots from generated scenario IDs (`DEFAULT`) rather than directly from `game/scenarios/zacus_v1.yaml`; root YAML remains validated/exported as canonical content source.
- Build gate result: `./build_all.sh` PASS (5/5 envs) with log `logs/run_matrix_and_smoke_20260217-143000.log` and build artifacts in `.pio/build/`.
- Smoke gate result: `./tools/dev/run_matrix_and_smoke.sh` FAIL (`artifacts/rc_live/20260217-143000/summary.json`) — ports resolved, then `smoke_esp8266_usb` and `ui_link` failed (`UI_LINK_STATUS missing`, ESP32 log in bootloader/download mode). Evidence: `artifacts/rc_live/20260217-143000/ui_link.log`, `artifacts/rc_live/20260217-143000/smoke_esp8266_usb.log`.
- Content gates result (run from repo root): scenario validate/export PASS (`game/scenarios/zacus_v1.yaml`), audio manifest PASS (`audio/manifests/zacus_v1_audio.yaml`), printables manifest PASS (`printables/manifests/zacus_v1_printables.yaml`).
- Runtime status after this pass: UI Link = FAIL on latest smoke evidence (`connected` unavailable), LittleFS default scenario = not revalidated in this run (known previous blocker remains), I2S panic = not retested in this pass (stress gate pending).

## [2026-02-17] Rapport d’erreur automatisé – Story V2

- Correction du bug shell (heredoc Python → script temporaire).
- Échec de la vérification finale : build FAIL, ports USB OK, smoke/UI/artefacts SKIPPED.
- Rapport d’erreur généré : voir artifacts/rapport_erreur_story_v2_20260217.md
- Recommandation : analyser le log de build, corriger, relancer la vérification.

# Agent TODO & governance

## 1. Structural sweep & merge
- [x] Commit the pending cleanup described in `docs/SPRINT_RECOMMENDATIONS.md:18-80` (structure/tree fixes + PR #86 merge/tag) so the repo is back on main.
- [x] Consolidation 2026-02-21 exécutée: `feat/fix-firmware-story-workflow`, `feat/freenove-ap-fallback-stable`, `feat/freenove-webui-network-ops-parity` intégrés dans l’historique de `main`; CI GitHub Actions désormais limité à `main` seulement.
- [x] Branches de travail supprimées (local + remote): `feat/freenove-ap-local-espnow-rtc-sync`, `origin/feat/fix-firmware-story-workflow`, `origin/feat/freenove-ap-fallback-stable`, `origin/feat/freenove-webui-network-ops-parity`.
- [x] Tags de sauvegarde créés avant suppression: `backup/20260221_212813/feat-fix-firmware-story-workflow-1b4c328`, `backup/20260221_212813/feat-fix-firmware-story-workflow-b530be3`, `backup/20260221_212813/feat-freenove-ap-fallback-stable`, `backup/20260221_212813/feat-freenove-webui-network-ops-parity`, `backup/20260221_212813/feat-freenove-ap-local-espnow-rtc-sync`.

## 2. Build/test gates
- [x] Re-run `./build_all.sh` (`build_all.sh:6`); artifacts landed under `artifacts/build/` and logs live in `logs/run_matrix_and_smoke_*.log` if rerun again via the smoke gate.
- [x] Re-launch `./tools/dev/run_matrix_and_smoke.sh` (`tools/dev/run_matrix_and_smoke.sh:9-200`) – run completed 2026-02-16 14:35 (artifact `artifacts/rc_live/20260216-143539/`), smoke scripts and serial monitors succeeded but UI link still reports `connected=0` (no UI handshake). Need to plug in/validate UI firmware before closing gate.
- [x] Capture evidence for HTTP API, WebSocket, and WiFi/Health steps noted as blocked or TODO in `docs/TEST_SCRIPT_COORDINATOR.md:13-20` – `tools/dev/healthcheck_wifi.sh` created `artifacts/rc_live/healthcheck_20260216-154709.log` (ping+HTTP fail) and the HTTP API script logged connection failures under `artifacts/http_ws/20260216-154945/http_api.log` (ESP_URL=127.0.0.1:9). WebSocket skipped (wscat missing). All failures logged to share evidence.

## 3. QA + automation hygiene
- [x] Execute the manual serial smoke path (`python3 tools/dev/serial_smoke.py --role auto --baud 115200 --wait-port 3 --allow-no-hardware`) – passed on /dev/cu.SLAB_USBtoUART + /dev/cu.SLAB_USBtoUART9, reporting UI link still down (same failure as matrix run).
- [ ] Run the story QA suite (`tools/dev/run_smoke_tests.sh`, `python3 tools/dev/run_stress_tests.py ...`, `make fast-*` loops) documented in `esp32_audio/TESTING.md:36-138` and capture logs (smoke_tests failed: DEFAULT scenario missing `/story/scenarios/DEFAULT.json`; run_stress_tests failed with I2S panic during recovery; `make fast-esp32` / `fast-ui-oled` built & flashed but monitor commands quit in non-interactive mode, `fast-ui-tft` not run because no RP2040 board connected). Need scenario files/UI recovery to unblock.
- [x] Ensure any generated artifacts remain untracked per agent contract (no logs/artifacts added to git).

## 4. Documentation & agent contracts
- [ ] Update `AGENTS.md` and `tools/dev/AGENTS.md` whenever scripts/gates change, per their own instructions (`AGENTS.md`, `tools/dev/AGENTS.md`).
- [ ] Keep `tools/dev/cockpit_commands.yaml` in sync with `docs/_generated/COCKPIT_COMMANDS.md` via `python3 tools/dev/gen_cockpit_docs.py` after edits, and confirm the command registry is reflected in `docs/TEST_SCRIPT_COORDINATOR.md` guidance.
- [ ] Review `docs/INDEX.md`, `docs/ARCHITECTURE_UML.md`, and `docs/QUICKSTART.md` after significant changes so the onboarding picture matches the agent constraints.

## 5. Reporting & evidence
- [ ] When publishing smoke/baseline runs, include the required artifacts (`meta.json`, `commands.txt`, `summary.md`, per-step logs) under `artifacts/…` as demanded by `docs/TEST_SCRIPT_COORDINATOR.md:160-199`.
- [ ] Document any pipeline/test regressions in `docs/RC_AUTOFIX_CICD.md` or similar briefing docs and flag them for the Test & Script Coordinator.

## Traçabilité build/smoke 17/02/2026

- Succès : esp32dev, esp32_release, esp8266_oled
- Échec : ui_rp2040_ili9488, ui_rp2040_ili9486
- Evidence : logs et artefacts dans hardware/firmware/artifacts/build, logs/
- Actions tentées : correction du filtre build_src_filter, création de placeholder, relance build/smoke
- Problème persistant : échec RP2040 (sources/configuration à investiguer)
- Prochaine étape : escalade à un agent expert RP2040 ou hand-off

## [2026-02-18] Story portable + V3 serial migration

- [x] Added host-side story generation library `lib/zacus_story_gen_ai` (Yamale + Jinja2) with CLI:
  - `story-gen validate`
  - `story-gen generate-cpp`
  - `story-gen generate-bundle`
  - `story-gen all`
- [x] Replaced legacy generator entrypoint `hardware/libs/story/tools/story_gen/story_gen.py` with compatibility wrapper delegating to `zacus_story_gen_ai`.
- [x] Migrated portable runtime internals to tinyfsm-style state handling in `lib/zacus_story_portable/src/story_portable_runtime.cpp` while keeping `StoryPortableRuntime` facade.
- [x] Introduced Story serial V3 JSON-lines handlers (`story.status`, `story.list`, `story.load`, `story.step`, `story.validate`, `story.event`) and removed `STORY_V2_*` routing from canonical command list.
- [x] Updated run matrix/log naming to include environment label:
  - log: `logs/rc_live/<env_label>_<ts>.log`
  - artifacts: `artifacts/rc_live/<env_label>_<ts>/summary.md`
- [x] `run_matrix_and_smoke.sh` now supports single-board mode (`ZACUS_ENV="freenove_esp32s3"`): ESP8266/UI-link/story-screen checks are emitted as `SKIP` with detail `not needed for combined board`.
- [x] `tools/test/resolve_ports.py` now honors `--need-esp32` and `--need-esp8266` explicitly via required roles, while still emitting both `esp32` and `esp8266` fields in JSON output.
[20260218-020041] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-020041/summary.md
[20260218-021042] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260218-021042/summary.md
[20260221-222358] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-222358, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-222358/summary.md
[20260221-223450] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-223450, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-223450/summary.md

## [2026-02-22] Freenove story modular + SD + ESP-NOW v1 envelope + WebUI push

- [x] Story storage rendu modulaire avec fallback intégré:
  - bundle Story par défaut provisionné automatiquement en LittleFS (`/story/{scenarios,screens,audio,apps,actions}`),
  - support SD_MMC Freenove activé (`/sd/story/...`) avec sync SD -> LittleFS au boot et via commande.
- [x] Commandes/runtime ajoutés pour opérabilité story:
  - serial: `STORY_SD_STATUS`, `STORY_REFRESH_SD`,
  - WebUI/API: bouton + endpoint `POST /api/story/refresh-sd`.
- [x] Timeline JSON enrichie sur les scènes Story (`data/story/screens/SCENE_*.json`) avec keyframes `at_ms/effect/speed_ms/theme`.
- [x] Audio crossfade consolidé:
  - correction callback fin de piste (track réel reporté),
  - lecture des packs audio depuis LittleFS **ou SD** (`/sd/...`).
- [x] ESP-NOW aligné avec `docs/ESP_NOW_API_CONTRACT_FREENOVE_V1.md`:
  - enveloppe v1 supportée (`msg_id`, `seq`, `type`, `payload`, `ack`),
  - extraction metadata + statut exposé,
  - trames `type=command` exécutées côté runtime et réponse corrélée `type=ack` renvoyée si `ack=true`.
- [x] WebUI passée en push temps réel:
  - endpoint SSE `GET /api/stream` (statut Story/WiFi/ESP-NOW),
  - fallback refresh conservé côté front.
- [x] Générateur Story (`zacus_story_gen_ai`) amélioré:
  - bundle Story injecte désormais le contenu réel des ressources JSON (`data/story/*`) au lieu de placeholders minimalistes.

### Vérifications exécutées

- `pio run -e freenove_esp32s3` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
- `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
- Vérification série post-flash (`STORY_SD_STATUS`, `WIFI_STATUS`, `ESPNOW_STATUS_JSON`) ✅, IP observée: `192.168.0.91`
- `ZACUS_ENV=freenove_esp32s3 ZACUS_PORT_ESP32=/dev/cu.usbmodem5AB90753301 ZACUS_REQUIRE_HW=1 ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` ✅
- `./tools/dev/story-gen validate` ✅
- `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_test` ✅
- `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅
- `curl http://192.168.0.91/api/status` + `curl http://192.168.0.91/api/stream` ✅ (payloads UTF-8 validés après fix snapshot JSON)
- Frontend dev UI: `npm run build` ✅ (fix TS no-return sur `src/lib/deviceApi.ts` pour débloquer le flux front)
[20260221-234147] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-234147, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-234147/summary.md
[20260221-235139] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-235139, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260221-235139/summary.md
[20260222-000305] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-000305, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-000305/summary.md

## [2026-02-22] Suite autonome — installations + vérifications complètes

- [x] Préflight sécurité refait avant action:
  - branch/diff/status affichés,
  - checkpoint enregistré: `/tmp/zacus_checkpoint/20260222-005615_wip.patch` et `/tmp/zacus_checkpoint/20260222-005615_status.txt`,
  - scan artefacts trackés (`.pio`, `.platformio`, `logs`, `dist`, `build`, `node_modules`, `.venv`) = aucun trouvé.
- [x] Installations/outillage:
  - `./tools/dev/bootstrap_local.sh` exécuté (venv + dépendances Python + `zacus_story_gen_ai` editable),
  - validation outillage: `pio --version`, `python3 -m serial.tools.list_ports -v`,
  - `tools/dev/check_env.sh` durci pour fallback `pip3` et dépendances optionnelles en `WARN` (exécution validée).
- [x] Vérifications story/content:
  - `.venv/bin/python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml` ✅
  - `.venv/bin/python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` ✅
  - `.venv/bin/python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` ✅
  - `./tools/dev/story-gen validate` ✅
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅
- [x] Vérifications firmware/hardware Freenove USB modem:
  - `pio run -e freenove_esp32s3` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅
  - `ZACUS_ENV=freenove_esp32s3 ZACUS_PORT_ESP32=/dev/cu.usbmodem5AB90753301 ZACUS_REQUIRE_HW=1 ZACUS_NO_COUNTDOWN=1 ./tools/dev/run_matrix_and_smoke.sh` ✅ (`UI_LINK_STATUS=SKIP` justifié combined board).
- [x] Gates build contractuels:
  - `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅ (5/5).
- [x] Cohérence doc/registry:
  - `.venv/bin/python3 tools/dev/gen_cockpit_docs.py` ✅
  - `.venv/bin/python3 tools/test/audit_coherence.py` ✅ (`RESULT=PASS`).
- [x] Statut réseau et API runtime Freenove:
  - série `WIFI_STATUS`/`ESPNOW_STATUS_JSON`/`STORY_SD_STATUS` ✅,
  - IP actuelle observée: `192.168.0.91`,
  - `curl /api/status` et `curl /api/stream` ✅ (payloads Story/WiFi/ESP-NOW valides).

## [2026-02-22] Clarification doc Story V2 vs protocole série V3

- [x] Alignement documentaire réalisé pour éviter l'ambiguïté "V2/V3":
  - `docs/protocols/GENERER_UN_SCENARIO_STORY_V2.md`: V2 explicitée (moteur/spec), commandes de test série basculées en JSON-lines V3 (`story.*`), chemins outillage alignés (`./tools/dev/story-gen`).
  - `docs/protocols/story_v3_serial.md`: section `Scope` ajoutée (V3 = interface série, V2 = génération/runtime).
  - `docs/protocols/story_README.md`: section commandes réorganisée avec recommandation V3 et rappel legacy `STORY_V2_*` pour debug uniquement.

## [2026-02-22] Story UI — génération écrans/effets/transitions (suite)

- [x] Générateur `zacus_story_gen_ai` renforcé pour les ressources écran:
  - normalisation automatique d'un profil écran (fallback par `SCENE_*`),
  - timeline normalisée en objet `timeline = {loop, duration_ms, keyframes[]}`,
  - keyframes consolidées (`at_ms`, `effect`, `speed_ms`, `theme`) avec garde-fous (ordre, bornes, minimum 2 keyframes),
  - transition normalisée (`transition.effect`, `transition.duration_ms`).
- [x] Runtime UI Freenove enrichi:
  - parse `timeline.keyframes` (et compat ancien format array),
  - support `timeline.loop` + `timeline.duration_ms`,
  - transitions de scène ajoutées (`fade`, `slide_left/right/up/down`, `zoom`, `glitch`) avec durée pilotable.
- [x] Données Story alignées:
  - `data/story/screens/SCENE_*.json` migrés vers format timeline keyframes + transition,
  - fallback embarqué (`storage_manager.cpp`) synchronisé sur le même contrat JSON.
- [x] Vérifications exécutées:
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (4 tests),
  - `./tools/dev/story-gen validate` ✅,
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_fx_<ts>` ✅ (payloads écran keyframes+transition vérifiés),
  - `pio run -e freenove_esp32s3` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t uploadfs --upload-port /dev/cu.usbmodem5AB90753301` ✅,
  - `pio run -e freenove_esp32s3_full_with_ui -t upload --upload-port /dev/cu.usbmodem5AB90753301` ✅,
  - `ZACUS_ENV=freenove_esp32s3 ... ./tools/dev/run_matrix_and_smoke.sh` ✅ (artefact: `artifacts/rc_live/freenove_esp32s3_20260222-002556/`),
  - validation série live (`SC_LOAD/SC_EVENT`) avec logs UI montrant `transition=<type>:<ms>` sur changements de scène.
[20260222-002556] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-002556, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-002556/summary.md
[20260222-025923] Run artefacts: `artifacts/rc_live/freenove_esp32s3_20260222-025923/`, logs: `logs/rc_live/`, summary: `artifacts/rc_live/freenove_esp32s3_20260222-025923/summary.md`

## [2026-02-22] Intégration complète Story + options Freenove (phases 1→3)

- [x] Runtime Freenove branché en modules:
  - `HardwareManager` intégré au cycle principal (`init/update/noteButton/setSceneHint`),
  - nouveaux modules `CameraManager` et `MediaManager` ajoutés et initialisés au boot.
- [x] Options Story (`data/story/apps`) ajoutées + fallback embarqué synchronisé:
  - `APP_HARDWARE.json`, `APP_CAMERA.json`, `APP_MEDIA.json`,
  - fallback LittleFS mis à jour dans `storage_manager.cpp` (bundle embarqué complet).
- [x] Interfaces publiques ajoutées:
  - série: `HW_*`, `CAM_*`, `MEDIA_*`, `REC_*`,
  - API WebUI: `/api/hardware*`, `/api/camera*`, `/api/media*`,
  - `/api/status` enrichi (`hardware`, `camera`, `media`),
  - commandes ESP-NOW `type=command` dispatchées sur les nouveaux contrôles.
- [x] Couplage Story actions au changement d'étape:
  - exécuteur d'`action_ids` branché,
  - nouvelles actions supportées (`ACTION_HW_LED_*`, `ACTION_CAMERA_SNAPSHOT`, `ACTION_MEDIA_PLAY_FILE`, `ACTION_REC_*`, etc.),
  - snapshot action `SERIAL:CAMERA_CAPTURED` émis en succès.
- [x] Évolution écran/effets Story:
  - effets ajoutés: `radar`, `wave`,
  - aliases transition ajoutés: `wipe -> slide_left`, `camera_flash -> glitch`,
  - scènes ajoutées: `SCENE_CAMERA_SCAN`, `SCENE_SIGNAL_SPIKE`, `SCENE_MEDIA_ARCHIVE`,
  - docs alignées: `docs/protocols/story_screen_palette_v2.md`, `docs/protocols/story_README.md`.
- [x] Contrôle de flux: ajout d'un déclencheur `BTN_NEXT` depuis `STEP_WAIT_UNLOCK` vers `STEP_WAIT_ETAPE2` (scène LA détection), pour permettre le passage immédiat via `NEXT` / `/api/scenario/next`/`SC_EVENT serial BTN_NEXT`; artefact build: `pio run -e freenove_esp32s3` (success 2026-02-22) et fichiers générés mis à jour (`docs/protocols/story_specs/scenarios/default_unlock_win_etape2.yaml`, `hardware/libs/story/src/generated/*`).
- [x] Contrôle tactile/physique Freenove: adaptation de `ScenarioManager::notifyButton` pour que tout bouton court (`1..5`) sur `STEP_WAIT_UNLOCK` déclenche `BTN_NEXT` si disponible (fallback `NEXT`), donc passage direct vers `STEP_WAIT_ETAPE2` depuis n'importe quel bouton.
- [x] Validation réalisée (session courante):
  - builds: `pio run -e esp32dev -e esp32_release -e esp8266_oled -e ui_rp2040_ili9488 -e ui_rp2040_ili9486` ✅,
  - build Freenove: `pio run -e freenove_esp32s3` ✅,
  - FS Freenove: `pio run -e freenove_esp32s3_full_with_ui -t buildfs` ✅,
  - smoke orchestré: `ZACUS_ENV=freenove_esp32s3 ./tools/dev/run_matrix_and_smoke.sh` ✅ (run sans hardware -> smoke/UI link `SKIPPED`, justif. combined board + port non résolu; artefact `artifacts/rc_live/freenove_esp32s3_20260222-025923/`),
  - Story tooling: `./tools/dev/story-gen validate` ✅, `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_options_20260222T035914` ✅,
  - tests Python: `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (5 passed),
  - scénario canonique: `.venv/bin/python3 ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅,
  - export brief: `.venv/bin/python3 ../../tools/scenario/export_md.py ../../game/scenarios/zacus_v1.yaml` ✅.
- [!] Limites constatées hors scope firmware:
  - `.venv/bin/python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ❌ (`game/prompts/audio/intro.md` manquant),
  - `.venv/bin/python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ❌ (prompts `printables/src/prompts/*.md` manquants),
  - aucune validation live `curl /api/*` ou cycle série ACK HW/CAM/MEDIA possible sans carte connectée pendant cette session.

## [2026-02-22] Correctif validateurs contenu + checks intégration API (local/GH/Codex/ChatGPT)

- [x] Correctif robustesse chemins pour exécution depuis `hardware/firmware/` **et** repo root:
  - `tools/audio/validate_manifest.py`: résolution `source` maintenant tolérante (`cwd`, dossier manifeste, repo root).
  - `tools/printables/validate_manifest.py`: résolution manifeste/prompt indépendante du `cwd` (fallbacks repo root + dossier manifeste).
  - `hardware/firmware/tools/dev/check_env.sh`: check env enrichi avec vérifications d'intégration API (`gh` auth, `codex login status`, reachability OpenAI) et diagnostic `MCP_DOCKER` (daemon Docker).
- [x] Revalidation post-correctif:
  - depuis `hardware/firmware`:  
    `.venv/bin/python3 ../../tools/audio/validate_manifest.py ../../audio/manifests/zacus_v1_audio.yaml` ✅  
    `.venv/bin/python3 ../../tools/printables/validate_manifest.py ../../printables/manifests/zacus_v1_printables.yaml` ✅
  - depuis repo root:  
    `hardware/firmware/.venv/bin/python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml` ✅  
    `hardware/firmware/.venv/bin/python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml` ✅
- [x] Cohérence firmware/story toolchain recheck:
  - `python ../../tools/scenario/validate_scenario.py ../../game/scenarios/zacus_v1.yaml` ✅
  - `./tools/dev/story-gen validate` ✅
  - `./tools/dev/story-gen generate-bundle --out-dir /tmp/story_bundle_api_check_<ts>` ✅
  - `.venv/bin/python3 -m pytest lib/zacus_story_gen_ai/tests/test_generator.py` ✅ (`5 passed`)
- [x] Vérification accès API outillage:
  - GitHub CLI: `gh auth status` ✅ (account `electron-rare`), `gh api user` ✅, `gh api rate_limit` ✅.
  - Codex CLI: `codex login status` ✅ (`Logged in using ChatGPT`), `codex exec --ephemeral` ✅ (réponse reçue).
  - OpenAI endpoint reachability: `curl https://api.openai.com/v1/models` => `401 Missing bearer` (connectivité OK, clé API absente côté shell).
- [!] Point d'intégration à traiter côté poste:
  - `codex mcp list` signale `MCP_DOCKER` activé mais non opérationnel si daemon Docker arrêté (`Cannot connect to the Docker daemon ...`).

## [2026-02-22] Réparation workspace — déplacement involontaire `esp8266_oled/src`

- [x] Analyse de l'anomalie:
  - suppressions trackées massives sous `hardware/firmware/ui/esp8266_oled/src/**`,
  - dossier non tracké détecté: `hardware/firmware/ui/esp8266_oled/src 2/`.
- [x] Correctif appliqué:
  - restauration de l'arborescence par renommage du dossier `src 2` vers `src`.
  - résultat: suppressions annulées, structure source ESP8266 OLED rétablie.
- [x] Validation:
  - `pio run -e esp8266_oled` ✅ (build complet OK après réparation).
[20260222-113849] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-113849, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-113849/summary.md
[20260222-204146] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-204146, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/freenove_esp32s3_20260222-204146/summary.md

## Audit test hardware Freenove (2026-02-23)
- Commande: `pio run -e freenove_esp32s3_full_with_ui -t upload`
- Résultat: flash OK vers `/dev/cu.usbmodem5AB90753301`.
- Port auto-détecté, upload réussi (RAM/Flash: 55.3% / 73.1%).
- Monitoring runtime (capture locale via `python3 + pyserial`, 60s) : boot OK, scénario `DEFAULT` exécuté, logs HW/LA/SCENARIO/FS/UI cohérents.
- Événement constaté: erreur caméra récurrente lors de la première action `ACTION_CAMERA_SNAPSHOT` (`cam_dma_config(301): frame buffer malloc failed`, `camera config failed`) sur ce lot matériel.
- Comportement attendu LA: `LA_TRIGGER timeout` puis reset vers `SCENE_LOCKED`.
- Aucune régression panic/reboot observée sur la fenêtre de test.
- Notes: scripts de validation demandés par le contrat firmware sont uniquement exécutés pour build + flash; logs hardware non versionnés et non archivés.

## Test hardware runtime (séquence réseau/ESP-NOW) - 2026-02-23
- Test en condition carte branchée sur `/dev/cu.usbmodem5AB90753301` après `pio run -e freenove_esp32s3_full_with_ui -t upload`.
- Commandes série envoyées: `STATUS`, `NET_STATUS`, `WIFI_TEST`, `ESPNOW_STATUS`, `ESPNOW_ON`, `ESPNOW_STATUS_JSON`, `ESPNOW_PEER_LIST`, `WIFI_AP_ON`, `ESPNOW_SEND ff:ff:ff:ff:ff:ff {...}`.
- Bilan WiFi:
  - Boot initial déjà en `STA` connecté sur SSID `Les cils`, IP `192.168.0.91`.
  - `WIFI_TEST` retourne `ACK WIFI_TEST ... ok=1`.
  - `WIFI_DISCONNECT` provoque bien la logique de reconnect (`[NET] wifi disconnected` puis `[NET] wifi connect requested ...`).
  - `WIFI_STATUS`/`WIFI_RECONN` : la commande `WIFI_RECONN` n’existe pas (retour `UNKNOWN WIFI_RECONN`).
- Bilan ESP-NOW:
  - `ESPNOW_ON` actif, `ESPNOW_STATUS` confirme `espnow=1` et compteur `peers=0` avant ajout.
  - `ESPNOW_SEND` vers `ff:ff:ff:ff:ff:ff` renvoie `ACK ESPNOW_SEND ok=1`.
  - Après send, statut indique `tx_ok=1`, peer broadcast visible via `ESPNOW_PEER_LIST` puis `NET_STATUS`.
  - `ESPNOW_STATUS_JSON` retourne JSON attendu (`ready`, `peer_count`, compteurs).
  - Pas de réception de payload externe observée (pas de `ESPNOW_RECV`, pas de `NET` RX event).
- API test WebSocket/HTTP non testée en ce cycle (focal hardware UART/serial).
- `status` global reste stable, pas de panic/reboot détecté pendant captures.

## Hardening scènes (SCENE) - 2026-02-23
- Scope: firmware Freenove (UI + registry scène).
- Stabilisation demandée:
  - Source de vérité `hardware/libs/story/src/resources/screen_scene_registry.cpp` avec `kScenes` sans alias.
  - Validation runtime: `SCREEN_SCENE_ID_UNKNOWN` sur `screenSceneId` non reconnu.
  - Chargement payload: normalisation stricte `screen_scene_id` avant lookup dans `StorageManager`.
  - UI: règles d'effets/transitions explicites, logs sur token inconnu.
- Tests: cas d'IDs inconnues + alias legacy dans validation de scénario + lookup.
[20260223-210304] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210304, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210304/summary.md
[20260223-210353] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210353, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210353/summary.md
[20260223-210410] Run artefacts: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210410, logs: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/logs/rc_live, summary: /Users/cils/Documents/Enfants/anniv isaac 10a/le-mystere-professeur-zacus/hardware/firmware/artifacts/rc_live/esp32dev_esp32_release_esp8266_oled_ui_rp2040_ili9488_ui_rp2040_ili9486_20260223-210410/summary.md

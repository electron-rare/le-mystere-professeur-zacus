# FW Harmonization 72h (execution backlog)

## Scope
- Stabiliser le build mono-racine PlatformIO.
- Aligner protocoles/UI docs avec le code courant.
- Réduire la dette runtime dans `app_orchestrator`.

## Livré dans ce cycle
- Build mono-racine avec `build_src_filter` par environnement.
- Script `build_all.sh` aligné sur les envs racine.
- Specs `docs/protocols/PROTOCOL.md` et `docs/protocols/UI_SPEC.md` complétées.
- Extraction C1: `LoopBudgetManager` (`esp32_audio/src/runtime/loop_budget_manager.*`).

## Backlog technique restant

### C2 — BootAudioFlow
1. Extraire état/progression `BOOT_*` dans un module dédié.
2. Garder `app_orchestrator` comme orchestrateur d'appels.
3. Préserver commandes/debug existantes.

### C3 — StoryRuntimeCoordinator
1. Isoler les transitions unlock/win/etape2 et hooks story V2.
2. Centraliser garde micro/audio story.
3. Exposer un snapshot runtime pour debug série.

### Serial command registry
1. Créer un registre canonique token -> handler.
2. Uniformiser les retours `OK/BAD_ARGS/OUT_OF_CONTEXT/NOT_FOUND/BUSY/UNKNOWN`.
3. Générer un export doc simple pour QA.

## Mesures de validation recommandées
- Build matrix: `esp32dev`, `esp32_release`, `ui_rp2040_ili9488`, `ui_rp2040_ili9486`, `esp8266_oled`.
- Smoke série: `BOOT_STATUS`, `STORY_STATUS`, `STORY_V2_STATUS`, `CODEC_STATUS`, `MP3_STATUS`.
- Vérifier `SYS_LOOP_BUDGET STATUS/RESET` après extraction C1.

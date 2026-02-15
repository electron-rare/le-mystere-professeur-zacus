## Scope
- [ ] Ticket(s) Jira / sprint references:
- [ ] Base branch cible (`hardware/firmware`, `main` ou `codex/*`):
- [ ] Type de changement: code / docs / tests / infra
- [ ] Commandes publiques impactées (si oui, lister):

## Contrats STV2
- [ ] Contrats/API impactés listés (`Story V2`, `SYSTEM`, `SCREEN`, `MP3`)
- [ ] Compatibilité Story V2 / legacy vérifiée
- [ ] Impact écran (STAT/seq/CRC + recovery) explicité
- [ ] Si changement flag default: justification + plan rollback

## Validation statique obligatoire
- [ ] `python3 -m compileall hardware/firmware/tools/dev`
- [ ] `make story-validate`
- [ ] `make story-gen`
- [ ] `make qa-story-v2`
- [ ] `pio run -e esp32dev`
- [ ] `pio run -e esp32_release`
- [ ] `pio run -e esp8266_oled`
- [ ] `pio run -e ui_rp2040_ili9488`
- [ ] `pio run -e ui_rp2040_ili9486`

## Validation runtime (si applicable)
- [ ] smoke debut sprint: `ZACUS_SKIP_PIO=1 ./tools/dev/run_matrix_and_smoke.sh`
- [ ] runbook complet fin sprint: `tools/qa/live_story_v2_runbook.md`
- [ ] Séquence Story V2 testée (`STEP_DONE` + `gate=1`)
- [ ] Recovery reset croisé ESP32/ESP8266 validé

## Checklist review STV2-41..48
- [ ] Checklist exécutée: `tools/qa/story_v2_review_checklist.md`
- [ ] Preuves (commandes/logs) jointes en commentaire PR

## Rollback
- [ ] Procédure runtime (`STORY_V2_ENABLE OFF`) documentée
- [ ] Procédure release (`kStoryV2EnabledDefault=false`) documentée
- [ ] Aucun fichier parasite versionné (`.DS_Store`, etc.)

## Risques / points d’attention
- [ ] Aucun
- [ ] Oui (détailler):

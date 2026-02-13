## Scope
- [ ] Ticket(s) / sprint references:
- [ ] Base branch cible (`main` ou `codex/*`):
- [ ] Type de changement: code / docs / tests / infra

## Implémentation
- [ ] Contrats/API impactés listés
- [ ] Compatibilité Story V2 / legacy vérifiée
- [ ] Impact écran (STAT/seq/CRC) explicité

## Validation
- [ ] `make story-validate`
- [ ] `make story-gen`
- [ ] `make qa-story-v2`
- [ ] `pio run -e esp32dev`
- [ ] `pio run -e esp8266_oled`
- [ ] `cd screen_esp8266_hw630 && pio run -e nodemcuv2`

## Runtime live (si applicable)
- [ ] Séquence Story V2 testée (`STEP_DONE` + `gate=1`)
- [ ] Recovery reset croisé ESP32/ESP8266 validé

## Rollback
- [ ] Procédure runtime (`STORY_V2_ENABLE OFF`) documentée
- [ ] Procédure release (`kStoryV2EnabledDefault=false`) documentée

## Risques / points d’attention
- [ ] Aucun
- [ ] Oui (détailler):

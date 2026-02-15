## Scope
- [ ] Ticket(s) / Issue references: 
- [ ] Base branch cible: `main` ou autre branch
- [ ] Type de changement: 
  - [ ] Code (firmware, tools, scripts)
  - [ ] Documentation
  - [ ] Tests
  - [ ] Infrastructure / CI/CD
  - [ ] Contenu créatif (game, audio, printables)

## Description
Brève description des changements effectués et de leur motivation.

## Changements
Liste des fichiers/modules modifiés et nature des changements:
- 
- 

## Validation

### Pour les changements de code
- [ ] Code lint/format respecté
- [ ] Tests existants passent
- [ ] Nouveaux tests ajoutés (si applicable)

### Pour les changements de contenu (game/audio/printables)
- [ ] `make scenario-validate` (si scénarios modifiés)
- [ ] `make audio-validate` (si audio modifié)
- [ ] `make printables-validate` (si printables modifiés)

### Pour les changements de firmware
- [ ] `make story-validate`
- [ ] `make story-gen`
- [ ] `make qa-story-v2`
- [ ] `pio run -e esp32dev`
- [ ] `pio run -e esp8266_oled`
- [ ] `cd screen_esp8266_hw630 && pio run -e nodemcuv2`

### Pour les changements de documentation
- [ ] Liens internes vérifiés
- [ ] Orthographe et grammaire vérifiées
- [ ] Structure et navigation cohérentes

## Contrats et compatibilité (si firmware)
- [ ] Contrats/API impactés listés (`Story V2`, `SYSTEM`, `SCREEN`, `MP3`)
- [ ] Compatibilité Story V2 / legacy vérifiée
- [ ] Impact écran (STAT/seq/CRC + recovery) explicité
- [ ] Si changement flag default: justification + plan rollback

## Validation runtime (si applicable)
- [ ] Smoke test: `make qa-story-v2-smoke` (ou `qa-story-v2-smoke-fast`)
- [ ] Runbook complet: `tools/qa/live_story_v2_runbook.md`
- [ ] Séquence Story V2 testée (`STEP_DONE` + `gate=1`)
- [ ] Recovery reset croisé ESP32/ESP8266 validé

## Scope des branches
- [ ] Respect des règles de scope (`CODEX_RULES.md`)
- [ ] Aucun fichier hors scope modifié
- [ ] Aucun fichier parasite versionné (`.DS_Store`, builds, etc.)

## Rollback (si applicable)
- [ ] Procédure de rollback documentée
- [ ] Breaking changes documentés avec plan de migration

## Risques / Points d'attention
- [ ] Aucun
- [ ] Oui (détailler ci-dessous):

## Checklist finale
- [ ] PR title est clair et descriptif
- [ ] Description et motivation sont complètes
- [ ] Changements revus et minimaux
- [ ] Documentation mise à jour (si nécessaire)
- [ ] Ready for review

# Checklist review STORY V2 (STV2-41..48)

A utiliser a chaque sprint pour valider la non-regression Story V2.

## STV2-41 — Routage serie Story V2

- [ ] `serialProcessStoryCommand` est utilise comme point central
- [ ] `app_orchestrator` ne contient pas de parsing Story V2 ad hoc
- Preuve:
```bash
rg -n "serialProcessStoryCommand|makeStorySerialRuntimeContext" src/app/app_orchestrator.cpp src/services/serial/serial_commands_story.cpp
```

## STV2-42 — Snapshot et health

- [ ] `StoryControllerV2Snapshot` expose les champs attendus
- [ ] `STORY_V2_HEALTH` retourne une ligne exploitable
- Preuve:
```bash
rg -n "StoryControllerV2Snapshot|healthLabel|STORY_V2_HEALTH" src/controllers/story src/services/serial/serial_commands_story.cpp
```

## STV2-43 — Hardening events/timers

- [ ] budget events par tick en place
- [ ] comptage drop queue en place
- [ ] garde anti-tempete en place
- Preuve:
```bash
rg -n "EVENT_BUDGET|droppedCount|isDuplicateStormEvent|kEventProcessBudgetPerUpdate" src/story/core src/controllers/story
```

## STV2-44 — story_gen strict + determinisme

- [ ] validation stricte active
- [ ] generation stable (idempotence)
- [ ] hash spec present
- Preuve:
```bash
make story-validate
make story-gen
make qa-story-v2
```

## STV2-45 — Robustesse lien ecran

- [ ] keyframe/watchdog/stats TX actifs
- [ ] parser ESP8266 suit `seq_gap/seq_rb`
- Preuve:
```bash
rg -n "kScreenKeyframePeriodMs|kScreenWatchdogMs|tx_drop|seq_gap|seq_rb" src/services/screen src/screen screen_esp8266_hw630/src/main.cpp
```

## STV2-46 — Non bloquant runtime

- [ ] pas de `delay()` dans transitions metier
- [ ] latence action->feedback conforme
- Preuve:
```bash
rg -n "\\bdelay\\(" src/app/app_orchestrator.cpp src/controllers src/services src/story
```

## STV2-47 — Script QA binaire

- [ ] `tools/qa/story_v2_ci.sh` rejouable
- [ ] verdict clair PASS/FAIL
- Preuve:
```bash
bash tools/qa/story_v2_ci.sh
```

## STV2-48 — Docs et rollback

- [ ] commandes Story V2 et workflow doc alignes
- [ ] rollback runtime/release explicitement documente
- Preuve:
```bash
rg -n "STORY_V2_ENABLE|STORY_V2_HEALTH|STORY_V2_TRACE|story-gen|qa-story-v2|rollback" README.md TESTING.md src/story/README.md
```

## Verdict final

- [ ] PASS global
- [ ] BLOCKED (detail + action corrective)

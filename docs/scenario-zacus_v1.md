# Scenario ZACUS_V1

- Version: 1
- Initial step: STEP_LOCKED

## Steps
### STEP_LOCKED
- screen scene: SCENE_LOCKED
- audio pack: none
- apps: APP_LA, APP_SCREEN, APP_GATE
- transitions:
  - on_event → unlock → STEP_SEARCH

### STEP_SEARCH
- screen scene: SCENE_SEARCH
- audio pack: PACK_SONAR_HINT
- apps: APP_AUDIO, APP_SCREEN, APP_GATE
- transitions:
  - on_event → audio_done → STEP_MORSE
  - on_event → serial → STEP_DONE

### STEP_MORSE
- screen scene: SCENE_CODE
- audio pack: PACK_MORSE_HINT
- apps: APP_AUDIO, APP_SCREEN, APP_GATE
- transitions:
  - on_event → audio_done → STEP_WIN
  - on_event → serial → STEP_DONE

### STEP_WIN
- screen scene: SCENE_REWARD
- audio pack: PACK_WIN
- apps: APP_AUDIO, APP_SCREEN, APP_GATE
- transitions:
  - on_event → audio_done → STEP_DONE
  - on_event → serial → STEP_DONE

### STEP_DONE
- screen scene: SCENE_DONE
- audio pack: none
- apps: APP_SCREEN, APP_GATE

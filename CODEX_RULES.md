
# Codex Rules (repo le-mystere-professeur-zacus)

## Golden rule
Codex MUST NOT edit files outside the branch scope.

## Branch scopes

### hardware/firmware
Allowed:
- hardware/firmware/**
- tools/qa/** (optional)
- .github/workflows/firmware-*.yml
Forbidden:
- game/** audio/** printables/** kit-maitre-du-jeu/** scenario-ai-coherence/** docs/**

### generation/story-esp
Allowed:
- hardware/firmware/esp32/src/story/**
- hardware/firmware/esp32/src/controllers/story/**
- hardware/firmware/esp32/src/services/serial/serial_commands_story.*
- hardware/firmware/esp32/src/la_detector.*
- hardware/firmware/esp32/GENERER_UN_SCENARIO_STORY_V2.md
- hardware/firmware/esp32/RELEASE_STORY_V2.md
Forbidden:
- services wifi/web/mp3 stack, tools/**, game/**

### generation/story-ia
Allowed:
- game/**
- audio/**
- printables/**
- kit-maitre-du-jeu/**
- scenario-ai-coherence/**
Forbidden:
- hardware/** (firmware), tools/**

### scripts/generation
Allowed:
- tools/**
- Makefile
- .github/workflows/validate.yml
Forbidden:
- hardware/** game/** audio/** printables/**

### documentation
Allowed:
- docs/**
- README.md CONTRIBUTING.md LICENSE.md CHANGELOG.md AGENTS.md DISCLAIMER.md SECURITY.md CODE_OF_CONDUCT.md
- CODEX_RULES.md
Forbidden:
- hardware/** tools/** game/** audio/**

## Always do before commit
- git diff --stat (quick sanity)
- If scope breached: revert the files or restart from origin/<branch>.

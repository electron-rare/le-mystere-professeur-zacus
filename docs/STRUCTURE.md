# Repository Structure

## Overview

This repository contains all materials for "Le Mystère du Professeur Zacus" - a complete investigation kit for birthday parties (6-14 children, 60-90 minutes), with a laboratory/scientific campus theme.

## Root Structure

```
le-mystere-professeur-zacus/
├── .github/              # GitHub configuration (workflows, templates)
├── audio/                # Audio manifests and prompts
├── docs/                 # Documentation
├── examples/             # Example usages and demonstrations
├── game/                 # Core game scenarios and rules
├── hardware/             # Firmware for microcontrollers
├── kit-maitre-du-jeu/    # Game master kit (scripts, solutions, setup)
├── ops/                  # Operations scripts and utilities
├── printables/           # Printable materials (badges, cards, clues)
├── scenario-ai-coherence/# AI-generated scenario content (human-validated)
├── tools/                # Validation and export scripts
└── [root config files]   # LICENSE, README, CONTRIBUTING, etc.
```

## Detailed Directory Structure

### `.github/`
GitHub-specific configuration files:
- **`ISSUE_TEMPLATE/`**: Issue templates for bugs and features
- **`PULL_REQUEST_TEMPLATE.md`**: Pull request template
- **`workflows/`**: CI/CD workflows for validation

### `audio/`
Audio content organization:
- **`manifests/`**: YAML manifests describing audio requirements
- **`game/prompts/audio/`**: Audio generation prompts (intro, incidents, solutions)

Validation: `python3 tools/audio/validate_manifest.py`

### `docs/`
Comprehensive documentation:
- **`BRANCHES.md`**: Git branch strategy and workflows
- **`STRUCTURE.md`**: This file - repository architecture
- **`QUICKSTART.md`**: Express setup guide
- **`STYLEGUIDE.md`**: Writing style and tone guidelines
- **`WORKFLOWS.md`**: Development workflows
- **`GLOSSARY.md`**: Terms and definitions
- **`index.md`**: Documentation index
- **`repo-status.md`**: Current repository status
- **`maintenance-repo.md`**: Maintenance procedures
- **`repo-audit.md`**: Audit reports
- **`_generated/`**: Auto-generated documentation
- **`assets/`**: Documentation images and diagrams

### `examples/`
Cross-functional examples and demonstrations. See `examples/README.md` for index.

### `game/`
Core game content (canonical):
- **`scenarios/`**: YAML scenario definitions (single source of truth)
  - `zacus_v1.yaml`: Canon scenario with timeline, clues, solution
- **`prompts/`**: Generation prompts for various content
- **`exports/`**: Generated exports (not versioned)

Validation: `python3 tools/scenario/validate_scenario.py`

### `hardware/`
Microcontroller firmware and hardware integration:
- **`firmware/`**: 
  - `esp32/`: ESP32 firmware (main controller, WiFi, audio, story engine)
  - `arduino/`: Arduino-compatible variants
  - `README.md`: Branch strategy and setup

**Note**: Firmware uses Git branches for development variants instead of separate directories:
- `main:hardware/firmware/` → stable releases
- `hardware/esp32-dev` → active development  
- `hardware/esp32-experimental` → R&D

See `hardware/firmware/README.md` for complete details.

### `kit-maitre-du-jeu/`
Game Master materials:
- Minute-by-minute script (ready-to-play)
- Complete solution (culprit, motive, method, timeline)
- Material checklist and 4-station setup
- Role distribution (modular for 6-14 children)
- Anti-chaos guide and station map
- PDF exports

### `ops/`
Operational scripts:
- **`check_scope.sh`**: Validate branch scope compliance
- **`codex_story_ia.sh`**: Story-IA workflow helper

### `printables/`
Printable materials for the game:
- **`src/`**: Source files and generation prompts
- **`export/pdf/`**: Exported PDFs (locally generated, not versioned)
- **`export/png/`**: Exported PNGs (locally generated, not versioned)
- **`manifests/`**: YAML manifests for printables validation
- **`WORKFLOW.md`**: Generation and export workflow

Validation: `python3 tools/printables/validate_manifest.py`

### `scenario-ai-coherence/`
AI-generated scenario coherence artifacts (human-validated):
- **`version-finale/`**: Validated final content
- Future: intermediate versions, generation logs

This folder contains AI-generated scenario content that has been reviewed and approved by human maintainers.

### `tools/`
Python validation and export scripts:
- **`scenario/`**: Scenario validation and export
  - `validate_scenario.py`: Validate YAML scenarios
  - `export_md.py`: Generate markdown from scenarios
- **`audio/`**: Audio manifest validation
  - `validate_manifest.py`: Validate audio manifests
- **`printables/`**: Printables manifest validation
  - `validate_manifest.py`: Validate printables manifests
- **`qa/`**: Quality assurance utilities (optional for firmware)

### Root Configuration Files

- **`README.md`**: Main project overview
- **`LICENSE.md`**: Dual licensing (MIT for code, CC BY-NC 4.0 for content)
- **`CONTRIBUTING.md`**: Contribution guidelines
- **`CODEX_RULES.md`**: Branch scope rules for AI assistants
- **`CHANGELOG.md`**: Version history
- **`AGENTS.md`**: AI agent descriptions
- **`SECURITY.md`**: Security policy
- **`CODE_OF_CONDUCT.md`**: Community standards
- **`DISCLAIMER.md`**: Legal disclaimer
- **`.gitignore`**: Git ignore patterns
- **`Makefile`**: Build and validation commands

## Data Flow

```
game/scenarios/*.yaml (CANON)
    ↓
tools/scenario/validate_scenario.py → validation
    ↓
tools/scenario/export_md.py → docs/_generated/
    ↓
printables/src/ + audio/prompts/ → printable PDFs + audio files
    ↓
kit-maitre-du-jeu/ → complete game master package
```

## Branch Strategy

See `BRANCHES.md` for complete branch workflow. Key branches:
- **`main`**: Stable integration branch
- **`hardware/firmware`**: Firmware development
- **`generation/story-esp`**: Story engine for ESP32
- **`generation/story-ia`**: Game content and scenarios
- **`scripts/generation`**: Validation and export tools
- **`documentation`**: Documentation updates

## Validation Workflow

```bash
# Validate scenario
python3 tools/scenario/validate_scenario.py game/scenarios/zacus_v1.yaml

# Export markdown
python3 tools/scenario/export_md.py game/scenarios/zacus_v1.yaml

# Validate audio manifest
python3 tools/audio/validate_manifest.py audio/manifests/zacus_v1_audio.yaml

# Validate printables manifest
python3 tools/printables/validate_manifest.py printables/manifests/zacus_v1_printables.yaml

# Or use Makefile
make scenario-validate
make export
make audio-validate
make printables-validate
make all-validate
```

## Licenses

- **Code** (firmware, scripts, tools): MIT License
- **Creative content** (documents, PDFs, assets, prompts): CC BY-NC 4.0
- See `LICENSE.md` and `LICENSES/` for full text

## Contributing

See `CONTRIBUTING.md` for contribution guidelines and review process.

## Status

Current project status tracked in `docs/repo-status.md`.

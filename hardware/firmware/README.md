# Firmware Structure and Branch Strategy

## Overview

This directory contains firmware for ESP32 and ESP8266 microcontrollers used in the "Le Mystère du Professeur Zacus" escape game kit.

## Directory Structure

- **`esp32/`** : ESP32 firmware (main controller, story engine, audio, WiFi)
- **`arduino/`** : Arduino-compatible firmware variants

## Branch Strategy

Instead of maintaining separate directories for development variants (e.g., `esp32dev/`), we use Git branches to manage different firmware development stages:

### Branch Organization

#### `main:hardware/firmware/` 
- **Purpose**: Stable releases only
- **Content**: Production-ready, tested firmware
- **Use case**: Official releases, tagged versions (v1.0, v2.0, etc.)

#### `hardware/esp32-dev`
- **Purpose**: Active development branch
- **Content**: Work-in-progress features, bug fixes, testing
- **Use case**: Daily development, integration testing before merging to main

#### `hardware/esp32-experimental`
- **Purpose**: Research & Development
- **Content**: Experimental features, proof-of-concepts, breaking changes
- **Use case**: New hardware support, major refactoring, early R&D

### Why Branches Instead of Directories?

1. **Cleaner repository**: No duplicate code structures
2. **Better change tracking**: Git history shows evolution clearly
3. **Easier merging**: Standard Git workflows for integration
4. **Reduced conflicts**: Separate development contexts
5. **Disk efficiency**: Only one version checked out at a time

## Development Workflow

### For Stable Development
```bash
# Work on stable features
git checkout hardware/esp32-dev
# Make changes in hardware/firmware/esp32/
# Test and validate
git commit -m "Add feature X"
# When ready for production
git checkout main
git merge hardware/esp32-dev
```

### For Experimental Work
```bash
# Work on experimental features
git checkout hardware/esp32-experimental
# Make changes in hardware/firmware/esp32/
# Experiment and iterate
git commit -m "Experiment with feature Y"
# When stable enough, merge to dev
git checkout hardware/esp32-dev
git merge hardware/esp32-experimental
```

## Setup Instructions

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- Python 3.x for validation tools
- ESP32/ESP8266 development board

### Build and Flash

```bash
# Navigate to firmware directory
cd hardware/firmware/esp32

# Build for ESP32
pio run -e esp32dev

# Upload to device
pio run -e esp32dev --target upload

# Monitor serial output
pio device monitor
```

### Run Tests

```bash
# From firmware directory
make story-validate
make story-gen
make qa-story-v2
```

## Hardware Support

- **ESP32**: Primary controller (WiFi, audio, main story engine)
- **ESP8266**: Display controller (OLED screens, status indicators)

For detailed wiring and hardware setup, see `WIRING.md` in the esp32 directory.

## Contributing

When contributing firmware changes:
1. Always work in appropriate branch (`hardware/esp32-dev` or `hardware/esp32-experimental`)
2. Follow scope rules in `CODEX_RULES.md`
3. Test thoroughly before merging to main
4. Update documentation for any API changes

## Read-Only Notice

The firmware code in this directory is maintained elsewhere and considered read-only for general contributors. Do not edit firmware files, docs, or logs without coordination with the firmware maintainers.

## Licence

Firmware code: **MIT** (see `LICENSES/MIT.txt`)

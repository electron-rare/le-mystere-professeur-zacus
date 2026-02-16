# Story generator (story_gen.py)

Utilities for validating Story V2 YAML and generating deterministic artifacts.

## Commands

From `hardware/firmware/esp32_audio`:

```bash
# Validate all scenarios (strict)
python3 tools/story_gen/story_gen.py validate --strict

# Generate C++ from YAML (strict)
python3 tools/story_gen/story_gen.py generate --strict

# Build JSON + checksums + deploy archive
python3 tools/story_gen/story_gen.py deploy --strict

# Optional: request a serial deploy ack
python3 tools/story_gen/story_gen.py deploy --strict --port /dev/cu.SLAB_USBtoUART7
```

## Deploy output

The deploy command writes deterministic JSON and checksums under:

```
artifacts/story_fs/deploy/story/
```

And produces a tarball:

```
artifacts/story_fs/story_deploy.tar.gz
```

You can deploy a single scenario by id:

```bash
python3 tools/story_gen/story_gen.py deploy --scenario-id DEFAULT
```

## Notes

- JSON files use sorted keys and compact encoding for deterministic output.
- `.sha256` files store SHA256 of the JSON content.
- Optional fields such as `estimated_duration_s` will log warnings when missing.

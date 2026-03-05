# Story generator (story_gen.py)

Utilities for validating Story specs and generating deterministic artifacts.

## Commands

From `hardware/firmware`:

```bash
# Validate all scenarios
./tools/dev/story-gen validate

# Generate C++ from YAML
./tools/dev/story-gen generate-cpp

# Build JSON + checksums + deploy archive
./tools/dev/story-gen generate-bundle

# Full pipeline
./tools/dev/story-gen all
```

## Deploy output

The deploy command writes deterministic JSON and checksums under:

```
artifacts/story_fs/deploy/story/
```

You can also produce a tarball:

```bash
./tools/dev/story-gen generate-bundle --archive artifacts/story_fs/story_deploy.tar.gz
```

Backward-compatible wrapper remains available:

```bash
python3 hardware/libs/story/tools/story_gen/story_gen.py deploy --scenario-id DEFAULT
```

## Notes

- JSON files use sorted keys and compact encoding for deterministic output.
- `.sha256` files store SHA256 of the JSON content.
- Optional fields such as `estimated_duration_s` will log warnings when missing.

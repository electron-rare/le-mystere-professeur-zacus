# Printable image generator

Creates PNG drafts for each printable asset with the OpenAI Images API.

## Requirements
- `OPENAI_API_KEY` in your environment.
- Dependencies installed in `.venv` (`PyYAML`, `openai`).

## Run
```sh
PYTHON=.venv/bin/python tools/images/generate_printables.py \
  --manifest printables/manifests/zacus_v1_printables.yaml
```
Add `--force` to regenerate outputs even if files already exist.

Outputs land in `printables/export/png/zacus_v1/` and mirror the IDs from the manifest.

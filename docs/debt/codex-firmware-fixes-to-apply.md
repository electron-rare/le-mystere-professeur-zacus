# Firmware Fixes from codex/repo-state-zacus-from-lr

These fixes were on a Codex branch with broken paths (double `hardware/firmware/hardware/firmware/`).
The logic is correct but needs to be applied to the correct file paths manually.

## 1. la_detector.cpp — I2S channel consistency
Change `I2S_CHANNEL_FMT_ONLY_LEFT` to `I2S_CHANNEL_MONO` for consistency across the codebase.

## 2. wifi_service.cpp — AP password hardening
Harden the Access Point password (was too simple or empty).

## 3. web_ui_service.cpp — CORS restriction
Restrict CORS to specific origins instead of wildcard `*`.

## Source
Branch: codex/repo-state-zacus-from-lr (deleted 2026-04-02)
Patch saved: docs/debt/codex-fixes.patch

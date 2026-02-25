# FX_VERIFICATOR

## Objectif
Valider la sante runtime FX (intro/direct FX) sur Freenove via telemetrie serie.

## Verifications
- `UI_GFX_STATUS` detecte et parsable.
- `fx_fps > 0` sur scenes FX (`SCENE_WIN_ETAPE`, `SCENE_WINNER`, `SCENE_FIREWORKS`).
- Compteurs de pression (`fx_skip_busy`, `flush_block`, `flush_overflow`) surveilles.
- Aucun marqueur fatal (`PANIC`, `ASSERT`, `ABORT`, `REBOOT`, `rst:`).

## Script global associe
- `~/.codex/skills/fx-verificator/scripts/run_fx_verification.sh`

## Usage
```bash
~/.codex/skills/fx-verificator/scripts/run_fx_verification.sh /dev/cu.usbmodem5AB90753301 115200
```

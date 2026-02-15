# Zacus automation overhaul (one-shot)
Please inspect the firmware automation tooling and enforce the new single-entrypoint model.
Key goals:
- Ensure `tools/dev/zacus.sh` covers bootstrap/build/rc/ports/flash/codex flows.
- Keep RC gate strict with deterministic artifacts and prompt-driven autofix.
- Harden port resolution with learned mappings + fingerprint fallback and always write `ports_resolve.json` into artifacts.
Describe any blockers or missing hardware.

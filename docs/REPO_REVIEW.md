# Repo Review
- **Branch:** feature/MPRC-v3-full-radio-stack
- **Key folders:**
  - `docs` (project documentation, guides, and generated content); `hardware` (firmware sources and board-specific assets, excluding esp32 for this review); `kit-maitre-du-jeu` (scenario content for the game master); `printables` (generated assets/exports); `tools` (generators and validators).
  - `examples` (sample scenarios); `include-humain-IA` (AI helper assets); `LICENSES` (third-party notices alongside primary license files).
- **Red flags:** Ensure `LICENSE.md`, `LICENSES/*`, `CONTRIBUTING.md`, and `README.md` consistently describe attribution/licensing; normalized `include-humain-IA` avoids spaces/colons that can break tooling; keep generated folders (e.g., `printables/export`) gated.
- **Open PRs:** unable to fetch details because `gh pr list --state open --limit 50` failed (error connecting to api.github.com).

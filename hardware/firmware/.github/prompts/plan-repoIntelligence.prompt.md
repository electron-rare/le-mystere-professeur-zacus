## Plan: Align Repo Intelligence

TL;DR: align docs and prompts with the agreed source of truth (docs/protocols/story_specs), remove duplications, and update Story V2 references (default flow + CI workflow). Keep agent contracts intact unless we decide to consolidate. This will tighten the “intelligence” surface without changing code.

**Steps**
1. Normalize Story V2 spec source-of-truth wording across docs: update docs/protocols/README.md, docs/protocols/INDEX.md, docs/protocols/story_specs/README.md, and docs/INDEX.md to point to docs/protocols/story_specs and avoid legacy paths.
2. Deduplicate story prompts/specs: keep canonical prompts under docs/protocols/story_specs/prompts/ and turn duplicates into “moved” stubs or remove (e.g., story_generator/story_specs/prompts/spectre_radio_lab.prompt.md and docs/protocols/spectre_radio_lab.prompt.md). Keep story_generator/story_specs/README.md as a redirect only.
3. Update Story V2 documentation to the agreed default flow and CI workflow: align docs/protocols/story_README.md and esp32_audio/src/story/README.md to UNLOCK→U_SON_PROTO→WAIT_ETAPE2→ETAPE2→DONE and mention firmware-story-v2.yml.
4. Clarify prompt taxonomy: note in README.md and docs/protocols/story_README.md that Story authoring prompts are separate from ops/codex prompts but can be used by Codex tooling if desired.
5. Optional: confirm whether to keep both agent contracts as-is (AGENTS.md, tools/dev/AGENTS.md); if you want consolidation, add a short pointer in one to the other instead of merging.

**Verification**
- Manual doc consistency scan of the updated files for path correctness and consistent Story V2 flow/CI references.

**Decisions**
- Prompts: Story authoring prompts are a separate category but may be used by Codex.
- Default flow: UNLOCK → U_SON_PROTO → WAIT_ETAPE2 → ETAPE2 → DONE.
- CI workflow: firmware-story-v2.yml.

# Custom Agent – CI & Meta

## Conventions
- Follow `.github/agents/core/conventions-pm-ai-agents.md` for structure, risk loop, and reporting.

## Scope
`.github/**` workflows and template files.

## Do
- Keep workflow/template edits minimal and explicit.
- Preserve CI intent and required checks; note any gate implications in the final report.
- Validate syntax and path references before committing.

## Must Not
- Touch licensing text or perform mass deletions/renames outside approved scope.

## References
- `.github/AGENTS.md`

## Plan d’action
1. Vérifier les workflows/tpl modifiés.
   - run: rg --files .github/workflows
2. Générer les docs cockpit si nécessaire.
   - run: python3 tools/dev/gen_cockpit_docs.py


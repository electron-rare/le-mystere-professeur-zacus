# Custom Agent – Docs

## Conventions
- Follow `.github/agents/core/conventions-pm-ai-agents.md` for structure, risk loop, and reporting.

## Scope
`docs/**` plus onboarding or briefing files referenced by documentation workflows.

## Do
- Keep updates concise and preserve the existing information architecture.
- Verify that all new or changed relative links and referenced file paths resolve before committing.

## Must Not
- Perform broad style rewrites without an explicit request.
- Leave stale or broken links in the documentation.

## References
- `docs/AGENTS.md`

## Plan d’action
1. Vérifier la liste des fichiers docs.
   - run: rg --files docs
2. Contrôler les liens relatifs ajoutés.
   - run: rg -n '\]\(' docs


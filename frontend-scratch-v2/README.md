# Zacus Scratch Frontend V2 (greenfield)

Frontend "from scratch" pour designer un scenario Zacus avec des blocs type Scratch,
generer du YAML, puis appeler les endpoints Story V2.

## Objectif

- Ne pas reutiliser le frontend legacy du repo.
- Construire un socle neuf, orientee OSS/libre.
- Garder la compatibilite API avec `STORY_V2_WEBUI.md`.

## Stack OSS retenue

Validation rapide faite le 2026-03-01 (npm registry):

| Brique | Version | Licence | Role |
| --- | --- | --- | --- |
| blockly | 12.4.1 | Apache-2.0 | editeur blocs |
| yaml | 2.8.2 | ISC | serialisation YAML |
| zod | 4.3.6 | MIT | validation locale de schema |
| ajv | 8.18.0 | MIT | JSON schema (next step) |
| @monaco-editor/react | 4.7.0 | MIT | vue YAML |

Note: `scratch-gui` / `scratch-vm` sont AGPL-3.0-only, donc non retenus ici pour garder un front permissif.

## Demarrage

```bash
cd frontend-scratch-v2
npm install
npm run dev
```

Option API:

```bash
VITE_STORY_API_BASE=http://<esp_ip>:8080 npm run dev
```

## Fonctionnalites livrees

- Workspace Blockly avec bloc `step`.
- Generation YAML live depuis les blocs.
- Preview/edit YAML dans Monaco.
- Actions runtime:
  - GET `/api/story/list`
  - GET `/api/story/status`
  - POST `/api/story/validate`
  - POST `/api/story/deploy`

## Structure

- `src/components/BlocklyDesigner.tsx`: editeur blocs + generation YAML.
- `src/components/RuntimeControls.tsx`: actions HTTP Story V2.
- `src/lib/scenario.ts`: mapping blocs -> document scenario -> YAML.
- `src/types.ts`: types du document scenario.

## Prochain lot recommande

1. Ajouter un mapping complet `steps + transitions` (FSM) avec validation croisee.
2. Ajouter import/export Blockly JSON.
3. Ajouter tests unitaires de generation YAML et smoke E2E API mock.

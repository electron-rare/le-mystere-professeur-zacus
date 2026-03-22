# Zacus Story Designer — React 19 + Blockly

Studio auteur visuel pour concevoir des scenarios Zacus avec des blocs type Scratch.
Genere du YAML canonique et communique avec le firmware ESP32 via l'API Story V2.

## Setup

```bash
cd frontend-scratch-v2
npm install
npm run dev
```

Connexion API (optionnel) :

```bash
VITE_STORY_API_BASE=http://<esp_ip>:8080 npm run dev
```

## Scripts disponibles

| Commande | Description |
| --- | --- |
| `npm run dev` | Serveur de dev Vite (HMR) |
| `npm run build` | Build production (`dist/`) |
| `npm test` | Tests Vitest (18 tests) |
| `npm run lint` | ESLint |

## Architecture

L'interface s'organise en 4 onglets :

| Onglet | Composant | Role |
| --- | --- | --- |
| Designer | `BlocklyDesigner.tsx` | Editeur blocs + generation YAML live |
| Dashboard | `Dashboard.tsx` | Vue d'ensemble scenario |
| Media | `MediaManager.tsx` | Gestion assets audio/image |
| Network | `NetworkPanel.tsx` | Monitoring devices terrain |

Autres fichiers cles :
- `src/components/RuntimeControls.tsx` — actions HTTP Story V2 (list, status, validate, deploy)
- `src/lib/scenario.ts` — mapping blocs → document scenario → YAML
- `src/types.ts` — types TypeScript du document scenario

## Stack OSS

| Brique | Version | Licence | Role |
| --- | --- | --- | --- |
| blockly | 12.4.1 | Apache-2.0 | editeur blocs |
| yaml | 2.8.2 | ISC | serialisation YAML |
| zod | 4.3.6 | MIT | validation locale |
| ajv | 8.18.0 | MIT | JSON schema |
| @monaco-editor/react | 4.7.0 | MIT | vue YAML |

Note : `scratch-gui` / `scratch-vm` sont AGPL-3.0, non retenus pour garder un front permissif.

## API Story V2

| Methode | Endpoint | Description |
| --- | --- | --- |
| GET | `/api/story/list` | Liste des scenarios |
| GET | `/api/story/status` | Statut runtime |
| POST | `/api/story/validate` | Validation YAML |
| POST | `/api/story/deploy` | Deploiement sur device |

Variable d'environnement : `VITE_STORY_API_BASE` (defaut : pas de proxy).

## Tests

18 tests passing (Vitest). Lancer avec `npm test`.

## Prochaines etapes

1. Mapping complet steps + transitions (FSM) avec validation croisee
2. Import/export Blockly JSON
3. Tests E2E avec API mock

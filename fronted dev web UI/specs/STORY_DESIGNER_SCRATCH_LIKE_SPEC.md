# Spécification frontend — Story Designer (Svelte/Cytoscape, style Scratch-like)

## 1) Objectif

Spécifier le frontend web en alignement strict avec l’implémentation réelle :
- édition visuelle Story V2,
- pilotage runtime (déploiement, validation, test),
- pages support (dashboard, média, réseau, diagnostics).

Le runtime reste la source de vérité côté exécution.  
`hardware/firmware` est hors-scope frontend.

## 2) Stack réelle (au lieu d’une stack cible théorique)

Le projet web actuel est basé sur **SvelteKit + Svelte stores + Cytoscape**, et non React/XYFlow.

Références implémentées :
- `fronted dev web UI/src/routes/+layout.svelte`
- `fronted dev web UI/src/routes/designer/+page.svelte`
- `fronted dev web UI/src/lib/stores/designer.store.ts`
- `fronted dev web UI/src/lib/stores/runtime.store.ts`
- `fronted dev web UI/src/lib/stores/scenario.store.ts`
- `fronted dev web UI/src/lib/stores/media.store.ts`
- `fronted dev web UI/src/lib/runtimeService.ts`
- `fronted dev web UI/src/lib/deviceApi.ts`
- `fronted dev web UI/src/features/story-designer/{types,storyYaml,validation}.ts`

## 3) Décision UX : rester en style Scratch-like (skin), pas en Blockly

**Décision actuelle:**
- On continue avec **Cytoscape comme moteur**, et un rendu visuel **Scratch-like par skin/UX** (palette de type bloc, badges de statut, panneaux propriétés dédiés, lien visuel orienté nœuds).
- Pas de bascule vers Blockly ou `@xyflow/react` tant que la conversion YAML↔graphe est stable et que la chaîne de build/test fonctionne.

**Raison pragmatique:**
- La base technique existante est déjà opérationnelle et valide (`/api/story/validate`, `/api/story/deploy`, auto-layout, undo/redo, import/export).
- Refaire le moteur (Blockly/XYFlow) ferait sauter la logique métier actuelle : persistance, historique, et contrat `StoryGraphDocument`.
- La feuille de route “nœuds améliorés” est compatible avec le stack Cytoscape et permet d’évoluer visuellement sans refondre le moteur.

**Plan recommandé (phase suivante):**
1. améliorer les nœuds existants (catégories visuelles + validité inline + mini-guide),
2. seulement ensuite évaluer un vrai Blockly si besoin produit par produit.

## 4) Portée synchronisée

- **Designer visuel** : édition d’un graphe, import YAML → graphe, export graphe → YAML.
- **Contrôle runtime** : validate, deploy, test-run, start/pause/resume/skip/unlock selon capacité.
- **Gestion média + réseau + diagnostics** déjà intégrés dans l’app SvelteKit.
- Définition du projet cohérente avec :
  - `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid 2.md`
  - `specs/STORY_RUNTIME_API_JSON_CONTRACT.md`
  - `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`
  - `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_SPEC.md`

## 5) UX réelle du studio Designer

Le routeur expose les onglets :
- Dashboard
- Scénario
- Designer
- Media Manager
- Réseau
- Diagnostics

Sur la page `Designer`, le comportement réel attendu est :
- **Canvas Cytoscape** avec :
  - création/suppression de node,
  - déplacement libre (drag),
  - liaison mode à deux clics (`Mode liaison` puis clic destination),
  - auto-layout (`dagre`),
  - styles visuels (initial, sélection, link mode).
- **Inspector latéral** :
  - édition node (`step_id`, `screen_scene_id`, `audio_pack_id`, initial),
  - édition edge (`event_type`, `event_name`, `priority`),
  - suppression edge/node.
- **Validation locale** en continu (warnings/erreurs).
- **Actions globales** :
  - Undo/Redo (stack max 80),
  - import/export YAML,
  - validate/deploy/test-run via runtime quand disponibles,
  - statut de chargement/erreur moteur.
- **Persist local** :
  - draft YAML `studio:v3:draft`,
  - graphe JSON `studio:v3:graph` (localStorage).
- **Keyboard** :
  - `Ctrl/Cmd+Z`, `Ctrl/Cmd+Y`.

## 6) Modèle de données éditeur (source de vérité front)

`StoryGraphDocument`:
- `scenarioId: string`
- `version: number`
- `initialStep: string`
- `appBindings[]`
- `nodes[]` :
  - `id`, `stepId`, `screenSceneId`, `audioPackId`, `actions`, `apps`, `mp3GateOpen`, `x`, `y`, `isInitial`
- `edges[]` :
  - `id`, `fromNodeId`, `toNodeId`, `trigger`, `eventType`, `eventName`, `afterMs`, `priority`

## 7) Conversion YAML ↔ graph (tolérante)

Le parser actuel supporte :
- `initial_step` ou `initial_step_id` (fallback sur premier step si invalide),
- `target_step_id` ou `target`,
- normalisation tokens par `normalizeToken` (majuscules + `_` + remplacement caractères non standards),
- `app_bindings` absent → bindings par défaut,
- import de `scenario` mal nommé → avertissements non bloquants,
- suppression des transitions avec target inexistant (warning + continue).

La génération YAML :
- produit `id/version/initial_step/app_bindings/steps`,
- normalise les IDs,
- ordonne les nodes pour stabilité,
- `event_type` + `event_name` générés depuis `edge`.

## 8) Validation

Validation locale (`StoryGraphDocument`) :
- `scenarioId` non vide
- `version` valide
- au moins 1 node, exactement 1 initial
- `stepId` unique
- edges avec source/cible existantes
- `trigger`/`event_type` valides, `event_name` non vide, priorités bornées.

Validation API :
- POST `/api/story/validate` via `scenarioStore.validateYaml`.
- Exécution bloquée en mode legacy (garde-fou via capabilities runtime).

Déploiement/test :
- POST `/api/story/deploy` (`scenarioStore.deployYaml`)
- `test-run` = deploy → select → start (uniquement si story_v2).

## 9) Intégration runtime et orchestrations

Runtime store (`runtime.store.ts`) :
- bootstrap au chargement,
- polling toutes les ~3s,
- stream quand disponible (WS/SSE selon découverte),
- refresh global via `/api/status`, `/api/story/status`, `/api/story/list`, `/api/media/record/status`.

Pilotage scénario :
- select/start/pause/resume/skip via `scenario.store.ts`,
- capacité runtime vérifiée (`canSelectScenario`, `canStart`, etc. de `deviceApi`).

Média :
- `/api/media/files?kind=music|picture|recorder`
- `/api/media/play`, `/api/media/stop`
- `/api/media/record/start`, `/api/media/record/stop`
- fallback compatible via `/api/control` en cas d’échec dédié.

Détection Media Manager :
- priorité `SCENE_MEDIA_MANAGER`,
- fallback `STEP_MEDIA_MANAGER`,
- fallback legacy si token `MEDIA_MANAGER` présent ailleurs.

## 10) Mapping FSM runtime cible (DEFAULT)

Le graphe/story édité doit couvrir le contrat du runtime source `fsm_mermaid 2.md` :
- `STEP_U_SON_PROTO` état initial,
- boucles, transitions, timeouts et actions/serial identiques en labels d’événements,
- conservation stricte des noms événementiels (`BTN`, `AUDIO_DONE`, `ACK_*`, `ETAPE*`, `FORCE_*`, `WIN_DUE`, etc.),
- tolérance des IDs mixtes (`STEP_*` + `SCENE_*`) sans blocage.

## 11) Conditions d’acceptation front

- 1) Designer chargé sans erreur de moteur Cytoscape.
- 2) Import YAML → export YAML en roundtrip sans perte fonctionnelle majeure.
- 3) Erreurs de validation locale visibles immédiatement (warnings/errors persistants dans le panneau statut).
- 4) Undo/Redo fonctionne avec historique cohérent (capacité 80).
- 5) Validate/deploy/test-run déclenchés uniquement si support API Story V2.
- 6) Auto-layout non destructif + persist local opérationnelle.
- 7) Détection Media Manager conforme (scene > step > legacy).

## 12) Décision de livraison finale (frontend)

- **On valide un “scratch-like visuel” basé sur `Cytoscape` + `nœuds améliorés`, pas une migration vers Blockly.**
- Les futurs gains UX passent par :
  - amélioration de la peau visuelle des nœuds,
  - éditoriaux contextuels plus guidants,
  - validations inline plus fines,
  - sans changer le moteur ou le contrat `StoryGraphDocument`.

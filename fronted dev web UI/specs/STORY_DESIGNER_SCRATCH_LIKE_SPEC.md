# Spécification Frontend — Story Studio (UX Scratch-like)

## 1) Objectif

Fournir une spécification unique pour le frontend web de création de scénarios qui :
- couvre **la totalité du projet frontend** (pilotage runtime + édition), 
- expose une expérience proche de Scratch (`canvas` visuel + éléments glissables / liens + propriétés),
- reste **compatible Story V2** sans dépendre du firmware.

Le frontend actuel est déjà positionné sur cette cible dans :
- `fronted dev web UI/src/App.tsx`
- `fronted dev web UI/src/components/StoryDesigner.tsx`
- `fronted dev web UI/src/features/story-designer/*`
- `fronted dev web UI/src/lib/deviceApi.ts`
- `fronted dev web UI/src/lib/runtimeService.ts`

---

## 2) Portée

- **Inclut** : dashboard opérationnel, contrôle du scénario, éditeur visuel nodal, API de validation/déploiement, media manager, réseau, diagnostics.
- **Inclut** : génération de scénario (locale + option IA).
- **N’inclut pas** : modification firmware.
- **Exclut** : export PDF/printables hors flux générateur de manifeste déjà existant dans `src/lib/studioService.ts`.

---

## 3) Références obligatoires

- Contrat API runtime story: `specs/STORY_RUNTIME_API_JSON_CONTRACT.md`
- Contrat média: `specs/MEDIA_MANAGER_RUNTIME_SPEC.md`
- Spécifications média frontend:
  - `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_SPEC.md`
  - `fronted dev web UI/specs/MEDIA_MANAGER_FRONTEND_HANDOFF.md`
- Source source de vérité scénario:
  - `scenario-ai-coherence/zacus_conversation_bundle_v3/fsm_mermaid.md`

### 3.1 Stack UI recommandée (inspiration Scratch)

- Moteur nodal: `@xyflow/react` (drag/drop, connexion `node→node`, mini-map, auto-layout, événements utilisateur).
- Skin "Scratch-like" à implémenter en CSS et composants custom (node chrome, couleurs par catégorie, contrôles visuels).
- Blockly / Scratch Blocks possible, mais :
  - Blockly ajoute des contraintes de runtime Script-to-YAML plus complexes,
  - Scratch Blocks est plus proche visuel mais lourd si on part d’un contrat `StoryGraphDocument` existant.
- Décision: conserver `@xyflow/react` pour la base, thème visuel Scratch-like en-dessus.

---

## 4) Modèle UX (Scratch-like)

### 4.1 Structure écran
Le frontend expose des tabs :
1. `Dashboard` — supervision de l’exécution.
2. `Scénario` — mode YAML + actions de base.
3. `Designer` — studio visuel (Scratch-like target).
4. `Media Manager` — médias et enregistrement.
5. `Réseau` — états/reload/reconnexion.
6. `Diagnostics` — logs, stream, payload bruts.

### 4.2 Comportement attendu du Studio
Le **Designer** doit fournir au minimum :
- **Palette** (menu contextuel + commandes rapides) :
  - ajouter un node,
  - auto-layout,
  - ajouter un lien,
  - supprimer node/lien.
- **Canvas** :
  - déplacement libre,
  - connexion node→node,
  - mini-carte + contrôles + fond.
- **Sélection/édition** :
  - panneau propriété par node,
  - panneau propriété par edge,
  - édition locale avec validation immédiate.
- **Retour visuel** :
  - états visuels pour validation locale (`OK/KO`),
  - badges de comptage nodes/liens,
  - statut de mode de liaison.
- **Undo/Redo** (stack max 80).
- **Persist** :
  - sauvegarde locale de `story-draft` (YAML),
  - sauvegarde locale de `story-graph-document`.
- **Thème Scratch-like** :
  - palette latérale catégories colorées (Étapes, Médias, Audio, Contrôles),
  - bloc de mode édition avec contrastes renforcés,
  - badges d’état (`OK`/`WARN`/`KO`) par nœud.
- **Import / Export** :
  - import YAML → graphe,
  - édition nodale → export YAML.

---

## 5) Contrat de données scène (éditeur visuel)

### 5.1 Modèle interne du graphe
Le Studio manipule un modèle `StoryGraphDocument` :
- `scenarioId`, `version`, `initialStep`.
- `appBindings[]`.
- `nodes[]`:
  - `stepId`, `screenSceneId`, `audioPackId`,
  - `actions[]`, `apps[]`, `mp3GateOpen`,
  - position (`x`,`y`), `isInitial`.
- `edges[]`:
  - `fromNodeId`, `toNodeId`,
  - `trigger`: `on_event | after_ms | immediate`,
  - `eventType`: `none | unlock | audio_done | timer | serial | action`,
  - `eventName`, `afterMs`, `priority`.

### 5.2 Conversion YAML ↔ graphe
- Import YAML tolérant :
  - fallback noms de champs (ex: `initial_step`/`initial_step_id`, `target_step_id`/`target`).
  - normalisation token (`[A-Z0-9_]`).
  - correction des références manquantes d’apps.
- Export YAML cohérent :
  - `id`, `version`, `initial_step`, `app_bindings`, `steps`.
- Les erreurs de validation de structure YAML empêchent la conversion mais affichent un message d’erreur lisible.

### 5.3 Contrats de validation frontend
- Vérifications locales (avant API) :
  - présence `scenarioId`,
  - version positive,
  - au moins un node + 1 node initial,
  - step IDs uniques,
  - arêtes avec `from`/`to` existants,
  - transitions valides (`trigger`/`eventType`/`afterMs`/`priority`).
- API Validation :
  - endpoint `/api/story/validate`
  - résultat interprété en `valid + erreurs`.

### 5.4 Déploiement frontend
- API déploiement :
  - endpoint `/api/story/deploy` (story_v2 only).
- Résultat attendu :
  - `deployed` + `ok/status`,
  - rafraîchissement de la liste des scénarios,
  - sélection automatique du scénario déployé.
- Erreurs exposées :
  - messages d’erreur explicites en `error`.

### 5.5 Test run
- Enchaîner :
  1. déploiement,
  2. sélection scénario,
  3. start scenario.
- Refuser explicitement si le runtime ne supporte pas sélection/déploiement.

---

## 6) Mapping vers le FSM runtime cible (DEFAULT)

Le runtime source visuel de base doit refléter la transition suivante :

### 6.1 État initial
- `STEP_U_SON_PROTO` doit être le point d’entrée par défaut.

### 6.2 Transitions attendues (strictement)
- `STEP_U_SON_PROTO` → `STEP_U_SON_PROTO` par `audio_done:LOOP`.
- `STEP_U_SON_PROTO` → `STEP_LA_DETECTOR` par `BTN:ANY` puis `serial:FORCE_ETAPE2`.
- `STEP_LA_DETECTOR` → `STEP_U_SON_PROTO` par `timer:ETAPE2_DUE`.
- `STEP_LA_DETECTOR` → `STEP_RTC_ESP_ETAPE1` par `serial:BTN_NEXT`, `unlock:UNLOCK`, `action:ACTION_FORCE_ETAPE2`, `serial:FORCE_WIN_ETAPE1`.
- `STEP_RTC_ESP_ETAPE1` → `STEP_WIN_ETAPE1` par `esp_now:ACK_WIN1`, `serial:FORCE_DONE`.
- `STEP_WIN_ETAPE1` → `STEP_WARNING` par `serial:BTN_NEXT`, `serial:FORCE_DONE`, `esp_now:ACK_WARNING`.
- `STEP_WARNING` → `STEP_WARNING` par `audio_done:LOOP`.
- `STEP_WARNING` → `STEP_LEFOU_DETECTOR` par `BTN:ANY` puis `serial:FORCE_ETAPE2`.
- `STEP_LEFOU_DETECTOR` → `STEP_WARNING` par `timer:ETAPE2_DUE`.
- `STEP_LEFOU_DETECTOR` → `STEP_RTC_ESP_ETAPE2` par `serial:BTN_NEXT`, `unlock:UNLOCK`, `action:ACTION_FORCE_ETAPE2`, `serial:FORCE_WIN_ETAPE2`.
- `STEP_RTC_ESP_ETAPE2` → `STEP_QR_DETECTOR` par `esp_now:ACK_WIN2`, `serial:FORCE_DONE`.
- `STEP_QR_DETECTOR` → `STEP_RTC_ESP_ETAPE2` par `timer:ETAPE2_DUE` et `event:QR_TIMEOUT`.
- `STEP_QR_DETECTOR` → `STEP_FINAL_WIN` par `serial:BTN_NEXT`, `unlock:UNLOCK_QR`, `action:ACTION_FORCE_ETAPE2`, `serial:FORCE_WIN_ETAPE2`.
- `STEP_FINAL_WIN` → `SCENE_MEDIA_MANAGER` par `timer:WIN_DUE`, `serial:BTN_NEXT`, `unlock:UNLOCK`, `action:FORCE_WIN_ETAPE2`, `serial:FORCE_WIN_ETAPE2`.

### 6.3 Règle de fidélité
- Les noms d’événement (`BTN`, `AUDIO_DONE`, `ACK_*`, `ETAPE*`, `FORCE_*`) ne doivent pas être renommés durant l’édition UX.
- Les transitions `SCENE_MEDIA_MANAGER` peuvent être produites en transition finale via `STEP_FINAL_WIN` et/ou via un target direct de type `SCENE_*`.
- Les IDs mixtes doivent être supportés en édition et en export sans erreur bloquante.

Contrainte d’éditeur :
- conserver les champs textuels **sans reformater sémantique** (laisser les événements tels qu’ils sont fournis en YAML, ex: `FORCE_WIN_ETAPE1`, `ETAPE2_DUE`, `WIN_DUE`, `QR_TIMEOUT`).
- autoriser `target_step_id` avec valeur `SCENE_MEDIA_MANAGER` même si ce n’est pas un `STEP_*`.
- tolérer des IDs mixtes (`STEP_*` + `SCENE_*`) sans crash UX.

---

## 7) Runtime orchestration (onglet dashboard + médias + réseau)

### 7.1 Polling / Stream
- Polling toutes les ~3s + stream websocket/SSE quand disponible.
- `GET /api/status` utilisé pour reconstruire la vue globale.
- Règle Media Manager prioritaire :
  - `story.screen === 'SCENE_MEDIA_MANAGER'`
  - ou fallback `story.step === 'STEP_MEDIA_MANAGER'`.

### 7.2 Contrôles scénario
- Démarrage rapide par clic sur scénario.
- Pause/reprise/skip/unlock selon capacité runtime.

### 7.3 Contrats média
- `GET /api/media/files?kind=music|picture|recorder`
- `POST /api/media/play` avec `{path|file}`
- `POST /api/media/stop`
- `POST /api/media/record/start` `{seconds, filename}`
- `POST /api/media/record/stop`
- Fallback compat `/api/control` en cas d’échec media dédié.
- Affichage de `media.last_error`, `media.record_simulated`, états `playing/recording`.

---

## 8) UI/UX détaillée par rôle

### 8.1 Opérateur terrain
- Voir en un coup d’œil : scénario actif / étape / scene / progression.
- Actions directes : start/stop/pause/reprise/skip.
- Mémoire de statut réseau (Wi-Fi / ESP-NOW / firmware).

### 8.2 Créateur de scénario
- Modifier visuellement le graphe,
- basculer via `template`,
- importer un YAML externe pour enrichissement,
- corriger warnings locales,
- valider + déployer,
- lancer un test run contrôlé.

### 8.3 Réparateur support
- Diagnostiques `ops` en fin de flux :
  - events API,
  - snapshot stream,
  - raw payload.
- Capable de détecter les divergences de contrat runtime.

---

## 9) Contraintes non-fonctionnelles

- Interface FR (labels, notices, boutons).
- Erreurs non silencieuses : toujours affichées.
- Pas de blocage UI au format invalide ; passage en mode erreur contrôlée.
- Résilient aux variations de contrat :
  - champs story/scenario optionnels,
  - tokens non standard nettoyés pour affichage interne.
- Accessibilité minimale :
  - interactions clavier (undo/redo via boutons accessibles),
  - contrastes suffisants sur notices et badges.
- Perf :
  - pas de recalculs complets inutiles sur drag (détection au diff minimal),
  - snapshots de runtime en arrière-plan.

---

## 10) Priorités d’implémentation (frontend)

1. **S1 – Contrat FSM stable**  
   Le graphe ↔ YAML conserve tous les événements du runtime (y compris noms atypiques).
2. **S1 – Import/Export robuste**  
   Roundtrip YAML ↔ graphe sans perte fonctionnelle.
3. **S1 – HMI “Designer” mature**  
   Editeur visuel, Undo/Redo, context menu, liens, validateurs.
4. **S2 – Détection Media Manager**  
   `SCENE_MEDIA_MANAGER` + fallback `STEP_MEDIA_MANAGER`.
5. **S2 – e2e + unitaires dédiés**  
   scénarios import/lien/undo + validation/deploy path.
6. **S3 – Parcours “AI assistée”**  
   génération locale garantie si IA indisponible.

---

## 11) Critères d’acceptation

- `npm run lint`, `npm run build`, `npm run test:unit` passent.
- Dans `Designer`, import d’un YAML valide produit nodes/edges cohérents.
- Création node, ajout lien, annuler/rétablir opérationnels.
- Export YAML puis ré-import = cohérence structurelle.
- Validation API Story V2 OK quand runtime supporte `story_v2`.
- Déploiement crée bien un `deployed` et le sélectionne.

## 12) Plan d’implémentation recommandé (frontend)

1. **Stabiliser le contrat graphique**
   - verrouiller les schémas `StoryGraphDocument` et YAML round-trip (`scenarioId`, `version`, `initial_step`, `app_bindings`, `steps`).
2. **Brique visuelle**
   - terminer le studio nodal Scratch-like (palette + actions rapides + mini-map + propriété node/edge).
3. **Contrôles runtime**
   - déploiement + sélection + test-run + start/skip/unlock selon capacités backend.
4. **Durcir les tolérances**
   - acceptation `target_step_id` mixte (`STEP_*` / `SCENE_*`) sans erreur.
   - préservation stricte des `event_name` (pas de rewrite).
5. **Réalité média**
   - vue Media Manager robuste à `SCENE_MEDIA_MANAGER`, fallback `STEP_MEDIA_MANAGER`, erreurs exposées.
6. **Validation**
   - e2e ciblé : import FSM DEFAULT, roundtrip, ajout/suppression node+edge, undo/redo, déploiement et fallback erreurs.

## 13) Non-fonctionnel prioritaire

- Pas de changement firmware dans cette livraison.
- Aucun refactoring backend/firmware durant cette passe.
- Tous les noms métiers (`BTN`, `ACK_*`, `FORCE_*`, `ETAPE*`, `WIN_DUE`, `QR_TIMEOUT`) sont considérés immuables.
- Test run en 30s refuse proprement quand API non supportée.
- Média manager s’affiche sur `SCENE_MEDIA_MANAGER` et fallback `STEP_MEDIA_MANAGER`.

---

## 12) Artefacts attendus (non-code)

- `fronted dev web UI/specs/` :
  - `STORY_DESIGNER_SCRATCH_LIKE_SPEC.md` (ce document, version finale),
  - `MEDIA_MANAGER_FRONTEND_SPEC.md`,
  - `MEDIA_MANAGER_FRONTEND_HANDOFF.md`.
- Journal de vérifications:
  - `artifacts/runtime-sync/<date>/story-designer-checklist.md`
  - `artifacts/runtime-sync/<date>/story-designer-mapping.md`

---

## 13) Notes de gel

- Pas de modification firmware dans cette spécification.
- Toute régression de contrat runtime (`SCENE_MEDIA_MANAGER`/`STEP_MEDIA_MANAGER`, payload fields manquants) doit être remontée en tâche `SCN-601-MEDIA-BRIDGE`.
- Implémentation frontend alignée: stack `SvelteKit + adapter-node`, Designer `Cytoscape.js`, stores centralisés.

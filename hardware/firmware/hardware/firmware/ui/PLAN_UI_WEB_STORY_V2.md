# Plan d’attaque UI Web Story V2 (Selector / Orchestrator / Designer)

## 1. Architecture générale
- **Framework recommandé** : React (Vite ou équivalent)
- **Entrée** : page unique (SPA), responsive mobile-first
- **Connexion API** : REST (fetch/axios), WebSocket natif
- **Dossier cible** : ui/web/ ou frontend/

## 2. Composants principaux

### A. ScenarioSelector
- Récupère la liste via GET /api/story/list
- Affiche chaque scénario (id, durée, description)
- Bouton "Play" → POST /api/story/select/{id} puis /api/story/start
- Gestion loading/error
- Responsive : grille (desktop), liste (mobile)

### B. LiveOrchestrator
- Affiche l’étape courante, statut, barre de progression
- Contrôles : Pause, Resume, Skip, Retour
- Audit log (scrollable, auto-scroll)
- WebSocket : écoute step_change, transition, status, error
- Reconnexion auto WS, indicateur de connexion
- Responsive : boutons tactiles, log en bas

### C. StoryDesigner
- Éditeur YAML (textarea ou Monaco)
- Boutons : Validate (POST /api/story/validate), Deploy (POST /api/story/deploy)
- Dropdown templates (charger YAML par défaut)
- Affichage erreurs/validations, auto-save localStorage
- Responsive : split vertical (desktop), stack (mobile)

## 3. Flux API & WebSocket
- **Selector** : GET /api/story/list → POST /api/story/select/{id} → POST /api/story/start
- **Orchestrator** : GET /api/story/status (init), WebSocket /api/story/stream (temps réel)
- **Designer** : POST /api/story/validate, POST /api/story/deploy
- **Gestion erreurs** : format d’erreur JSON, statuts HTTP, messages utilisateur clairs

## 4. Responsive & UX
- Mobile : boutons ≥44px, transitions fluides, pas de scroll horizontal
- Desktop : grille, raccourcis clavier, accessibilité ARIA
- Loading states, retry, feedback visuel WS/API

## 5. Priorités d’implémentation
1. Structure projet (Vite/React, routing minimal)
2. ScenarioSelector (API, UI, responsive)
3. LiveOrchestrator (WebSocket, log, contrôles)
4. StoryDesigner (éditeur YAML, validate/deploy)
5. Gestion erreurs, UX, responsive
6. Tests manuels (API, WS, navigation)

## 6. Spécs de référence
- API : docs/protocols/STORY_V2_WEBUI.md
- YAML : docs/protocols/story_specs/schema/story_spec_v1.yaml
- Templates : docs/protocols/story_specs/scenarios/

---

Ce plan garantit la conformité avec les specs et une UX robuste sur mobile et desktop. Prêt à enclencher la structure du projet ou détailler un composant ?

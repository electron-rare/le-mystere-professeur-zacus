# Détail UI/UX des composants WebUI Story V2

## ScenarioSelector
- Affichage liste scénarios (nom, durée, version)
- Bouton "Play" pour chaque scénario
- Indicateur de chargement (spinner)
- Message d’erreur si API KO
- Responsive : grille (desktop), liste (mobile)
- Sélection visuelle du scénario courant
- Accessibilité : navigation clavier, ARIA

## LiveOrchestrator
- Affichage :
  - Statut global (running, paused, idle)
  - Étape courante (nom, description)
  - Barre de progression (progress_pct)
  - Log d’audit scrollable (auto-scroll)
- Contrôles :
  - Boutons Pause, Resume, Skip (état dépendant du statut)
  - Indicateur WebSocket (connecté/déconnecté)
- Responsive : boutons larges, log en bas sur mobile
- Feedback : loading, erreurs, transitions animées

## StoryDesigner
- Éditeur YAML (textarea ou Monaco)
- Boutons Validate (POST /api/story/validate), Deploy (POST /api/story/deploy)
- Dropdown pour charger un template YAML
- Affichage validation/erreur sous l’éditeur
- Auto-save localStorage
- Responsive : split vertical (desktop), stack (mobile)
- Accessibilité : labels, navigation clavier

---

Chacun de ces composants doit offrir :
- Un feedback utilisateur clair (chargement, succès, erreur)
- Une expérience fluide sur mobile et desktop
- Des transitions visuelles pour les changements d’état
- Un design sobre, lisible, adapté à l’usage tactile et desktop

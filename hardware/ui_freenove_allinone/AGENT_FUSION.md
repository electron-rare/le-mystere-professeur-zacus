
# AGENT_FUSION.md – Firmware All-in-One Freenove

## Objectif

Fusionner les rôles audio, scénario, UI, stockage et gestion hardware dans un firmware unique pour le Media Kit Freenove, avec structure modulaire, traçabilité agent, et conformité aux specs suivantes :


### Plan d’intégration complète (couverture specs)

- Fichiers de scènes et écrans individuels, stockés sur LittleFS (data/) ✔️
- Navigation UI dynamique (LVGL, écrans générés depuis fichiers) ✔️
- Exécution de scénarios (lecture, transitions, actions, audio) ✔️
- Gestion audio (lecture/stop, mapping fichiers LittleFS) ✔️
- Gestion boutons et tactile (événements, mapping, callbacks) ✔️
- Fallback robuste si fichier manquant (scénario par défaut) ✔️
- Génération de logs et artefacts (logs/, artifacts/) ✔️
- Validation hardware sur Freenove (affichage, audio, boutons, tactile) ⏳
- Documentation et onboarding synchronisés ⏳

### Structure modulaire

- audio_manager.{h,cpp} : gestion audio
- scenario_manager.{h,cpp} : gestion scénario
- ui_manager.{h,cpp} : gestion UI dynamique (LVGL)
- button_manager.{h,cpp} : gestion boutons
- touch_manager.{h,cpp} : gestion tactile
- storage_manager.{h,cpp} : gestion LittleFS, fallback

### Points critiques à valider

- Robustesse du fallback LittleFS
- Synchronisation UI/scénario/audio
- Mapping dynamique boutons/tactile
- Logs d’évidence et artefacts produits

Voir AGENT_TODO.md pour le suivi détaillé et la progression.

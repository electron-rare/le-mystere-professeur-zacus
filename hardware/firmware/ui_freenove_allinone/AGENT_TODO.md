## Validation hardware – TODO détaillée

- [ ] Compiler et flasher le firmware sur le Freenove Media Kit
- [ ] Préparer les fichiers de test sur LittleFS (/data/scene_*.json, /data/screen_*.json, /data/*.wav)
- [ ] Vérifier l’affichage TFT (boot, écran dynamique, transitions)
- [ ] Tester la réactivité tactile (zones, coordonnées, mapping)
- [ ] Tester les boutons physiques (appui, mapping, transitions)
- [ ] Tester la lecture audio (fichiers présents/absents, fallback)
- [ ] Observer les logs série (initialisation, actions, erreurs)
- [ ] Générer et archiver les logs d’évidence (logs/)
- [ ] Produire un artefact de validation (artifacts/)
- [ ] Documenter toute anomalie ou limitation hardware constatée

## Revue finale – Checklist agents

- [ ] Vérifier la cohérence de la structure (dossiers, modules, scripts)
- [ ] Relire AGENT_FUSION.md (objectifs, couverture specs, structure)
- [ ] Relire README.md (usage, build, validation, modules)
- [ ] Relire la section onboarding (docs/QUICKSTART.md, etc.)
- [ ] Vérifier la présence et la robustesse du fallback LittleFS
- [ ] Vérifier la traçabilité des logs et artefacts
- [ ] Vérifier la synchronisation UI/scénario/audio
- [ ] Vérifier la gestion dynamique des boutons/tactile
- [ ] Vérifier la non-régression sur les autres firmwares (split)

# TODO Agent – Firmware All-in-One Freenove


## Plan d’intégration détaillé (COMPLÉTÉ)

- [x] Vérifier la présence du scénario par défaut sur LittleFS
- [x] Charger la liste des fichiers de scènes et d’écrans (data/)
- [x] Initialiser la navigation UI (LVGL, écrans dynamiques)
- [x] Mapper les callbacks boutons/tactile vers la navigation UI
- [x] Préparer le fallback LittleFS si fichier manquant
- [x] Logger l’initialisation (logs/)

- [x] Boucle principale d’intégration
	- [x] Navigation UI (LVGL, écrans dynamiques)
	- [x] Exécution scénario (lecture, actions, transitions)
	- [x] Gestion audio (lecture, stop, files LittleFS)
	- [x] Gestion boutons/tactile (événements, mapping)
	- [x] Gestion stockage (LittleFS, fallback)
	- [x] Logs/artefacts

## Validation hardware (EN COURS)

- [ ] Tests sur Freenove Media Kit (affichage, audio, boutons, tactile)
- [ ] Génération de logs d’évidence (logs/)
- [ ] Production d’artefacts de validation (artifacts/)

## Documentation

- [ ] Mise à jour README.md (usage, build, structure)
- [ ] Mise à jour AGENT_FUSION.md (règles d’intégration, conventions)
- [ ] Synchronisation avec la doc onboarding principale

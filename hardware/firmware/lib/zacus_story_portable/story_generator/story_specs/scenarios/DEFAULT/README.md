# Spécification du scénario DEFAULT

Ce dossier contient les fichiers individuels pour chaque app écran et chaque scène du scénario par défaut.

- Chaque app écran (ex : APP_SCREEN, APP_GATE, etc.) doit être définie dans un fichier séparé (ex : app_screen.yaml).
- Chaque scène (ex : SCENE_LOCKED, SCENE_BROKEN, etc.) doit être définie dans un fichier séparé (ex : scene_locked.yaml).
- Tous ces fichiers seront stockés sur le LittleFS, dans un dossier par scénario (ici : DEFAULT).

## Structure attendue

- scenarios/DEFAULT/app_screen.yaml
- scenarios/DEFAULT/app_gate.yaml
- scenarios/DEFAULT/app_audio.yaml
- scenarios/DEFAULT/scene_locked.yaml
- scenarios/DEFAULT/scene_broken.yaml
- scenarios/DEFAULT/scene_la_detector.yaml
- scenarios/DEFAULT/scene_win.yaml
- scenarios/DEFAULT/scene_ready.yaml

## Exemple de fichier app écran
```yaml
id: APP_SCREEN
config:
  type: SCREEN_SCENE
  ...
```

## Exemple de fichier scène
```yaml
id: SCENE_LOCKED
content:
  ...
```

## À faire
- Déplacer la définition de chaque app écran et scène dans un fichier dédié.
- Adapter les scripts de génération pour prendre en compte cette nouvelle structure.
- S’assurer que le firmware charge les apps/écrans/scènes depuis LittleFS, dossier par scénario.

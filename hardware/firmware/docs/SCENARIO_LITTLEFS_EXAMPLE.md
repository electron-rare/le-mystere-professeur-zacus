
# Exemple d’arborescence LittleFS (dossier `data/` unique cross-plateforme)

```
data/
  story/
    scenarios/
      DEFAULT.json                # Fichier principal du scénario (index)
    apps/
      app1.json                   # Fichier de config d’une app
      app2.json
    screens/
      screen1.json                # Fichier de config d’un écran/scène
      screen2.json
    audio/
      audio1.json                 # Fichier de config d’un pack audio
    actions/
      action1.json                # Fichier de config d’une action
  audio/
    uson_boot_arcade_lowmono.mp3  # Exemple d’asset audio
    ...
  radio/
    stations.json
  net/
    config.json
```

- Le fichier `DEFAULT.json` référence les IDs des apps/screens/audio/actions à charger.
- Chaque entité (app, screen, audio, action) est dans son propre fichier JSON.
- Les checksums sont vérifiés à l’ouverture de chaque fichier.

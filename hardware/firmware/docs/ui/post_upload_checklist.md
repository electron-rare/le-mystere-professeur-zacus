# Checklist post-upload Freenove (standardisée)

Objectif: après chaque gros commit / flash, valider rapidement le firmware au cycle suivant :
1. upload (optionnel, si déjà flashé pas besoin)
2. détection de port auto
3. capture de logs série (minimum 20 lignes)
4. confirmation visuelle de la scène

## Commande de référence

```bash
./tools/dev/post_upload_checklist.sh --env freenove_esp32s3 --upload --scene SCENE_WIN_ETAPE
```

- `--env` : environnement PlatformIO.
- `--upload` : déclenche `pio run -e <env> -t upload --upload-port <port auto>`.
- `--scene` : envoie `SCENE_GOTO <SCENE_ID>` et vérifie `ACK SCENE_GOTO ok=1`.
- `--lines 20` : nombre minimum de lignes à capturer (défaut 20).
- `--monitor-seconds` : fenêtre d’écoute série (défaut 18 s).
- `--required-regex` : marqueur texte à trouver dans les premières lignes.
- `--no-visual` : saute la confirmation visuelle manuelle.

## Sortie standard

- `artifacts/post_upload/<timestamp>/post_upload_serial.log` : log des lignes capturées.
- `artifacts/post_upload/<timestamp>/upload.log` : log d’upload si `--upload`.
- `artifacts/post_upload/<timestamp>/ports.json` : mapping de port détecté.

## Confirmation visuelle

Le script affiche un prompt:

- **`y`** = visuel OK (texte + FX attendus visibles).
- autre réponse = échec de la checklist (à rejouer après correction).

### Réponse à “peux-tu le faire avec la caméra du PC ?”

- Pas depuis cette session d’exécution (pas d’accès caméra utilisateur).
- Ce qui peut être automatisé depuis ton environnement local: capture photo via outil tiers (ex: `imagesnap` / `ffmpeg`) puis vérification manuelle de l’image.


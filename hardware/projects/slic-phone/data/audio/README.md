# Arborescence audio (USB-MSC / FFAT)

Arborescence de base :

- `/welcome.wav`
- `/la_ok.wav`
- `/la_busy.wav`
- `/bip.wav`
- `/souffle.wav`
- `/radio.wav`
- `/musique.wav`
- `/note.wav`

Structure par scène (selon `SCENE_ID`) :

- `SCENE_LOCKED/1.wav`
- `SCENE_LOCKED/2.wav`
- `SCENE_LOCKED/3.wav`
- `SCENE_LA_DETECTOR/1.wav`
- `SCENE_LA_DETECTOR/2.wav`
- `SCENE_LA_DETECTOR/3.wav`
- `SCENE_WIN_ETAPE/1.wav`
- `SCENE_WIN_ETAPE/2.wav`
- `SCENE_WIN_ETAPE/3.wav`
- `SCENE_LA_OK/1.wav`
- `SCENE_LA_OK/2.wav`
- `SCENE_LA_OK/3.wav`
- `SCENE_WINNER/1.wav`
- `SCENE_WINNER/2.wav`
- `SCENE_WINNER/3.wav`
- `SCENE_FOU_DETECTOR/1.wav`
- `SCENE_FOU_DETECTOR/2.wav`
- `SCENE_FOU_DETECTOR/3.wav`
- `SCENE_CAM/1.wav`
- `SCENE_CAM/2.wav`
- `SCENE_CAM/3.wav`
- Tonalités planifiées (A252 contractuelles):
- `assets/wav/ETSI_EU/<event>.wav`
- `assets/wav/FR_FR/<event>.wav`
- `assets/wav/UK_GB/<event>.wav`
- `assets/wav/NA_US/<event>.wav`

## Profils qualité (`profiles`)

- `gentle` : voix plus posée, tempo plus bas, normalisation douce.
- `aggressive` : voix plus rapide/fermée, normalisation plus poussée.

Vous pouvez forcer un profil global avec `--profile <nom>` ou par entrée audio via :

```json
{
  "text": "Texte...",
  "profile": "aggressive"
}
```

Ajouter d'autres dossiers de scènes au même niveau (`/<SCENE_ID>/...`) avec :

- `1.wav`
- `2.wav`
- `3.wav`

### Génération depuis macOS

Depuis la racine du projet :

- Générer tous les WAV depuis `scripts/audio_tts_prompts.json` :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --overwrite`

- Vérifier la conformité locale (fichiers présents + manifest) :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --verify`

- Prévisualiser la liste à générer avant écriture :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --dry-run`

- Lister les voix macOS disponibles (par défaut en FR) :

  - `python3 scripts/generate_audio_from_tts.py --list-voices`
  - `python3 scripts/generate_audio_from_tts.py --list-voices --allow-non-french` *(toutes voix)*

- Génération batch (ne réécrit pas les fichiers déjà à jour) :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --batch`

## Plan tonal A252 (contrat)

- Les mappings tonals sont référencés dans [docs/audio_tone_plan.md](../../docs/audio_tone_plan.md).
- Chaque fichier suit le contrat commun WAV: PCM16 mono 8 kHz (Little Endian).
- Exemples de test:
  - `PLAY /assets/wav/ETSI_EU/dial.wav`
  - `PLAY /assets/wav/FR_FR/ringback.wav`
  - `PLAY /assets/wav/NA_US/busy.wav`

- Génération rapide sans post-traitement FFmpeg (sinon trim + normalisation RMS auto) :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --batch --skip-post`

- Génération en stéréo (2 canaux / voie 2) :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --channels 2 --batch`

- Filtrer ou choisir une voix :

  - `python3 scripts/generate_audio_from_tts.py --list-voices --voice-filter Thomas`
  - `python3 scripts/generate_audio_from_tts.py --voice-filter Thomas --voice-from-filter --batch`

- Ajuster le post-traitement :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --batch --normalize-mode loudnorm --normalize-target-rms-db -18 --trim-start-threshold-db -42`

- Vérification stricte de FFmpeg (pré-requis si post-processing activé) :

  - `python3 scripts/generate_audio_from_tts.py --audio-root data/audio --prompts scripts/audio_tts_prompts.json --batch --validate-ffmpeg`

Options disponibles (extra) :

- `--channels {1,2}`
- `--french-only` (par défaut actif)
- `--allow-non-french`
- `--voice`, `--voice-filter`, `--voice-filter-regex`, `--voice-from-filter`
- `--post-enable` / `--post-disable`
- `--trim`, `--no-trim`
- `--normalize`, `--no-normalize`
- `--normalize-mode {dynaudnorm,loudnorm}`
- `--normalize-target-rms-db`, `--normalize-peak`, `--normalize-framelen-ms`, `--normalize-max-gain`, `--normalize-compress`
- `--normalize-filter`
- `--trim-start-threshold-db`, `--trim-start-duration`, `--trim-stop-threshold-db`, `--trim-stop-duration`

# Organisation des fichiers TTS pour la hotline

Chaque scène dispose de ses fichiers TTS (text-to-speech) pour les différents états et situations.

Structure recommandée :

hotline_tts/
├── SCENE_U_SON_PROTO/

│   ├── indice_1.mp3
│   ├── indice_2.mp3
│   └── indice_3.mp3
├── SCENE_LA_DETECTOR/
│   ├── indice_1.mp3
│   ├── indice_2.mp3
│   └── indice_3.mp3
├── SCENE_WIN_ETAPE/
│   ├── attente_validation.mp3
│   ├── validation_ok.mp3
│   ├── validation_ko.mp3
├── SCENE_WARNING/
│   ├── indice_1.mp3
│   ├── indice_2.mp3
│   └── indice_3.mp3
├── SCENE_CREDITS/
│   ├── bravo_1.mp3
│   ├── bravo_1.mp3
│   ├── bravo_1.mp3
├── SCENE_WIN_ETAPE1/
│   ├── bravo_1.mp3
│   ├── bravo_1.mp3
│   ├── bravo_1.mp3
├── SCENE_WIN_ETAPE2/
│   ├── bravo_1.mp3
│   ├── bravo_1.mp3
│   ├── bravo_1.mp3
├── SCENE_QR_DETECTOR/
│   ├── indice_1.mp3
│   ├── indice_2.mp3
│   └── indice_3.mp3
├── SCENE_LEFOU_DETECTOR/
│   ├── indice_1.mp3
│   ├── indice_2.mp3
│   └── indice_3.mp3
├── SCENE_POLICE_CHASE_ARCADE/
│   ├── warning_1.mp3
│   ├── warning_2.mp3
│   └── warning_3.mp3

Pour chaque scène, prévoir :
- attente_validation.mp3
- validation_ok.mp3
- validation_ko.mp3
- indice_1.mp3, indice_2.mp3, indice_3.mp3 (si applicable)

Exemple de nommage :
SCENE_LA_DETECTOR/validation_ok.mp3
SCENE_WIN_ETAPE/indice_2.mp3

Tu peux adapter selon les besoins spécifiques de chaque scène.

Astuce : Ajoute un README.txt dans chaque dossier pour décrire le contenu et l’usage des fichiers.

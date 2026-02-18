## Mapping hardware ESP32-S3 Freenove Media Kit

| Fonction         | Broche ESP32-S3 | Signal TFT/Touch/Audio | Remarques                      |
|------------------|-----------------|-----------------------|--------------------------------|
| TFT SCK          | GPIO 18         | SCK                   | SPI écran                      |
| TFT MOSI         | GPIO 23         | MOSI                  | SPI écran                      |
| TFT MISO         | GPIO 19         | MISO                  | SPI écran (si utilisé)         |
| TFT CS           | GPIO 5          | CS                    | Chip Select écran              |
| TFT DC           | GPIO 16         | DC                    | Data/Command écran             |
| TFT RESET        | GPIO 17         | RESET                 | Reset écran                    |
| TFT BL           | GPIO 4          | BL                    | Rétroéclairage (PWM possible)  |
| Touch CS         | GPIO 21         | CS (XPT2046)          | SPI tactile                    |
| Touch IRQ        | GPIO 22         | IRQ (XPT2046)         | Interruption tactile           |
| Bouton 1         | GPIO 2          | BTN1                  | Pull-up interne                |
| Bouton 2         | GPIO 3          | BTN2                  | Pull-up interne                |
| Bouton 3         | GPIO 4          | BTN3                  | Pull-up interne                |
| Bouton 4         | GPIO 5          | BTN4                  | Pull-up interne                |
| Audio I2S WS     | GPIO 25         | WS                    | I2S audio                      |
| Audio I2S BCK    | GPIO 26         | BCK                   | I2S audio                      |
| Audio I2S DOUT   | GPIO 27         | DOUT                  | I2S audio                      |
| Alim écran/audio | 3V3/5V/GND      | -                     | Respecter les tensions         |

**Remarques** :
- Adapter les GPIO selon la version du kit (voir schéma Freenove officiel)
- Certains signaux peuvent être multiplexés selon les options du kit
- Pour l’audio, vérifier la compatibilité du codec (DAC/AMP)
- Pour le tactile, vérifier le contrôleur (XPT2046 ou autre)


# Firmware Freenove Media Kit All-in-One

## Plan d’intégration complète (couverture specs)

- Fichiers de scènes et écrans individuels, stockés sur LittleFS (data/)
- Navigation UI dynamique (LVGL, écrans générés depuis fichiers)
- Exécution de scénarios (lecture, transitions, actions, audio)
- Gestion audio (lecture/stop, mapping fichiers LittleFS)
- Gestion boutons et tactile (événements, mapping, callbacks)
- Fallback robuste si fichier manquant (scénario par défaut)
- Génération de logs et artefacts (logs/, artifacts/)
- Validation hardware sur Freenove (affichage, audio, boutons, tactile)
- Documentation et onboarding synchronisés

## Structure modulaire

- audio_manager.{h,cpp} : gestion audio
- scenario_manager.{h,cpp} : gestion scénario
- ui_manager.{h,cpp} : gestion UI dynamique (LVGL)
- button_manager.{h,cpp} : gestion boutons
- touch_manager.{h,cpp} : gestion tactile
- storage_manager.{h,cpp} : gestion LittleFS, fallback

Ce firmware combine :
- Les fonctions audio/scénario (type ESP32 Audio)
- L’UI locale (affichage, interaction, tactile, boutons)
- Le tout sur un seul microcontrôleur (RP2040 ou ESP32 selon le kit)


## Fonctionnalités
- Lecture audio, gestion scénario, LittleFS
- Affichage TFT tactile (LVGL)
- Boutons physiques, capteurs, extensions
- Mode autonome (pas besoin d’ESP32 séparé)

## Modules principaux
- audio_manager : gestion audio (lecture, stop, état)
- scenario_manager : gestion scénario (étapes, transitions)
- ui_manager : gestion UI (LVGL, écrans dynamiques)
- storage_manager : gestion LittleFS (init, vérification)
- button_manager : gestion boutons physiques
- touch_manager : gestion tactile XPT2046

## Validation hardware
- Compiler et flasher sur le Freenove Media Kit
- Vérifier l’affichage, la réactivité tactile et boutons
- Tester la lecture audio (fichiers dans /data/)
- Consulter les logs série pour le suivi d’exécution

## Artefacts
- Firmware compilé (.bin)
- Logs de test hardware (logs/)
- Rapport de compatibilité assets LittleFS

## Build

Depuis `hardware/firmware` :

```sh
pio run -e freenove_allinone
pio run -e freenove_allinone -t upload --upload-port <PORT>
```

## Mapping hardware
- Voir `include/ui_freenove_config.h` pour l’adaptation des pins
- Schéma de branchement : se référer à la doc Freenove

## Notes
- Ce firmware est expérimental et fusionne les logiques audio + UI.
- Pour la compatibilité UI Link, prévoir un mode optionnel.

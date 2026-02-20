# Multiplexage des broches – Attention

Certaines broches sont partagées entre plusieurs fonctions :
- GPIO 4 : utilisé pour le rétroéclairage (BL) et le bouton 3 (BTN3)
- GPIO 5 : utilisé pour le Chip Select TFT (CS) et le bouton 4 (BTN4)

Ce multiplexage impose :
- De ne pas activer simultanément les deux fonctions sur une même broche
- De documenter explicitement ce partage dans le firmware et la doc
- De vérifier le comportement lors des tests (priorité, conflits)

Recommandation : ajouter des commentaires dans ui_freenove_config.h et tester chaque usage séparément.
# Périphériques additionnels – Détail par fonction

## I2C
- SDA : GPIO 8
- SCL : GPIO 9
- Usage : bus capteurs (ex : MPU6050, DHT11)
- Remarque : vérifier pull-up, adresser chaque périphérique

## LED
- LED : GPIO 13
- Usage : indicateur d’état, feedback utilisateur
- Remarque : pilotable en PWM

## Buzzer
- Buzzer : GPIO 12
- Usage : signal sonore, alerte
- Remarque : pilotable en PWM

## DHT11
- DHT11 : GPIO 14
- Usage : capteur température/humidité
- Remarque : nécessite librairie DHT11, attention au timing

## MPU6050
- MPU6050 : I2C (SDA=8, SCL=9), adresse 0x68
- Usage : capteur IMU (accéléro/gyro)
- Remarque : vérifier alimentation, pull-up I2C
# Synthèse technique & onboarding audio ESP32-S3 (Freenove)

### Absence de DAC matériel et fallback I2S

- **ESP32-S3 ne possède pas de DAC intégré** : toute sortie audio doit passer par l’interface I2S.
- **I2S (Inter-IC Sound)** : protocole série dédié à l’audio, géré par deux périphériques sur ESP32-S3. Permet d’envoyer des samples vers un DAC externe ou un ampli via filtre passe-bas.
- **Modes supportés** : Standard (Philips/MSB/PCM), PDM, TDM. Mode standard ou PDM recommandé.
- **API Espressif** : utiliser les drivers ESP-IDF (`driver/i2s_std.h`, `driver/i2s_pdm.h`).
- **Exemple minimal** : initialiser canal TX, configurer mode standard, activer canal, envoyer samples.
- **Recommandations** :
  - Utiliser un DAC externe (ex : ES8311, PCM5102, ampli + filtre RC)
  - Adapter le mapping GPIO selon le schéma Freenove
  - Pour PDM, exploiter le convertisseur PCM2PDM matériel
  - Consulter les exemples Espressif : [i2s_basic/i2s_std](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s/i2s_basic/i2s_std), [i2s_codec/i2s_es8311](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s/i2s_codec/i2s_es8311)

### Onboarding : Marche à suivre pour l’audio ESP32-S3

1. **Vérifier le schéma hardware** : identifier les broches I2S (BCLK, WS, DOUT) et le DAC/ampli.
2. **Configurer le firmware** : adapter le mapping dans `ui_freenove_config.h` et `platformio.ini`.
3. **Utiliser le fallback I2S** : remplacer tout appel à `dacWrite` par une routine I2S (`i2sWriteSample`).
4. **Initialiser I2S** : suivre l’exemple Espressif pour la configuration du canal, du clock, des slots et des GPIO.
5. **Tester l’audio** : utiliser un signal simple (ex : sinusoïde) pour valider la sortie.
6. **Documenter la traçabilité** : reporter les étapes, patchs et artefacts dans [docs/AGENT_TODO.md](docs/AGENT_TODO.md).

### Liens techniques

- [Espressif I2S API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)
- [Exemples I2S Espressif](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s)
- [Datasheet ESP32-S3](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [Arduino-ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)

---

## Synthèse audit et traçabilité (17/02/2026)

- Audit de cohérence exécuté (artifacts/audit/firmware_20260217) : RESULT=PASS
- Mapping hardware, scripts, onboarding, traçabilité validés
- Correction audio ESP32-S3 : fallback I2S, conditional compilation, documentation
- Evidence : logs et artefacts non versionnés, paths/timestamps tracés dans AGENT_TODO.md et rapport d’audit
- Rapport et synthèse prêts à partager (voir artifacts/audit/firmware_20260217)

---

## Mode ciblé et automatisation (2026)

- Pour cibler un environnement spécifique (ex : Freenove ESP32-S3), utiliser la variable d’environnement :
  - `ZACUS_ENV="freenove_esp32s3" ./tools/dev/run_matrix_and_smoke.sh`
- Ne pas passer d’argument en ligne de commande (option non reconnue)
- Le cockpit détecte automatiquement le hardware et propose le build/smoke ciblé
- Les logs et artefacts sont synchronisés avec AGENT_TODO.md pour traçabilité
- En cas d’échec UI link, consulter les logs et relancer le test (retry automatique en cours d’intégration)

---

# Onboarding : sélection d’environnement

- La sélection de l’environnement hardware (Freenove ESP32-S3, ESP32, ESP8266, etc.) se fait **exclusivement** via la variable d’environnement `ZACUS_ENV`.
- Ne jamais passer l’environnement en argument CLI : tous les scripts (build, flash, smoke) détectent la cible via `ZACUS_ENV`.
- Exemple : pour tester sur Freenove ESP32-S3 :

  ```bash
  export ZACUS_ENV="freenove_esp32s3"
  ./tools/dev/cockpit.sh smoke
  ```

- Les scripts cockpit.sh, build_all.sh, run_matrix_and_smoke.sh, etc. utilisent cette variable pour déterminer la cible, le mapping hardware, et les artefacts.

# UI link : retry et diagnostic

- Le check UI link (connexion UI ESP32/ESP8266) intègre désormais une logique de retry automatique (3 essais par défaut, paramétrable via `ZACUS_UI_LINK_RETRIES`).
- En cas d’échec, le script logue chaque tentative, synchronise l’évidence dans `docs/AGENT_TODO.md`, et produit un rapport détaillé dans les artefacts.
- Toute évolution ou échec du protocole UI link est traçable dans les logs et la documentation.

# cockpit.sh : automatisation smoke test

- cockpit.sh détecte automatiquement la cible hardware via `ZACUS_ENV` et lance le smoke test adapté (Freenove, ESP32, ESP8266, etc.) sans intervention manuelle.
- En mode CI ou batch (`CI=true` ou `ZACUS_BATCH=1`), les prompts USB sont désactivés : la détection est automatique, le countdown est skip, aucun blocage.

# Synchronisation evidence/logs

- Chaque run (succès ou échec) synchronise l’évidence (artefacts, logs, verdict) dans `docs/AGENT_TODO.md`.
- Les artefacts et logs sont produits dans `hardware/firmware/artifacts/` et `hardware/firmware/logs/`.
- La traçabilité est assurée pour chaque run, avec horodatage, chemin, et verdict.

# Prompt USB : optimisation CI/batch

- En mode CI ou batch, les prompts USB sont automatiquement désactivés : aucun countdown, aucune interaction requise.
- Les scripts détectent le mode batch et adaptent le comportement pour garantir la non-interactivité.

---

# Mapping hardware – ESP32-S3 Freenove Media Kit

| Fonction         | Broche ESP32-S3 | Signal TFT/Touch/Audio | Remarques                      |
|------------------|-----------------|-----------------------|--------------------------------|
| TFT SCK          | GPIO 47         | SCK                   | SPI écran (FNK0102B)           |
| TFT MOSI         | GPIO 21         | MOSI                  | SPI écran                      |
| TFT MISO         | -1              | MISO                  | non utilisé                    |
| TFT CS           | -1              | CS                    | câblage board intégré          |
| TFT DC           | GPIO 45         | DC                    | Data/Command écran             |
| TFT RESET        | GPIO 20         | RESET                 | Reset écran                    |
| TFT BL           | GPIO 2          | BL                    | Rétroéclairage                 |
| Touch CS         | GPIO 9          | CS (XPT2046)          | optionnel (`FREENOVE_HAS_TOUCH`) |
| Touch IRQ        | GPIO 15         | IRQ (XPT2046)         | optionnel                      |
| Boutons          | GPIO 19         | ADC ladder (5 touches)| key1..key5 par seuils analogiques |
| Audio I2S WS     | GPIO 41         | WS                    | profil principal Sketch_19     |
| Audio I2S BCK    | GPIO 42         | BCK                   | profil principal Sketch_19     |
| Audio I2S DOUT   | GPIO 1          | DOUT                  | profil principal Sketch_19     |
| Alim écran/audio | 3V3/5V/GND      | -                     | Respecter les tensions         |

**Remarques** :
- Profil audio runtime sélectionnable par série: `AUDIO_PROFILE <idx>` puis `AUDIO_TEST`.
- Profils fournis: `0=sketch19`, `1=swap_bck_ws`, `2=dout2_alt`.
- Le tactile est désactivé par défaut (`FREENOVE_HAS_TOUCH=0`).

// main.cpp - Firmware Freenove Media Kit All-in-One (fusion audio + UI)
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <LittleFS.h>
#include "../include/ui_freenove_config.h"

#include "../include/audio_manager.h"
#include "../include/scenario_manager.h"

TFT_eSPI tft;
XPT2046_Touchscreen touch(FREENOVE_TOUCH_CS, FREENOVE_TOUCH_IRQ);

AudioManager g_audio;
ScenarioManager g_scenario;
UiManager g_ui;
StorageManager g_storage;
ButtonManager g_buttons;
TouchManager g_touch;

// Variables d'état
String currentScene = "/data/scene_default.json";
String currentScreen = "/data/screen_default.json";
unsigned long lastUiUpdate = 0;
const unsigned long UI_UPDATE_INTERVAL = 30; // ms

void logEvent(const char* msg) {
  Serial.println(msg);
  // TODO: écrire dans logs/ si besoin
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("[FREENOVE] All-in-One boot");

  // LittleFS
  if (!LittleFS.begin()) {
    Serial.println("[FREENOVE] LittleFS mount FAIL");
  } else {
    Serial.println("[FREENOVE] LittleFS mount OK");
  }

  // SPI + écran
  SPI.setSCK(FREENOVE_TFT_SCK);
  // TODO: Vérifier la présence du scénario par défaut sur LittleFS
    logEvent("[FREENOVE] All-in-One boot");
  // TODO: Initialiser la navigation UI (LVGL, écrans dynamiques)
  // TODO: Mapper les callbacks boutons/tactile vers la navigation UI
  // TODO: Préparer le fallback LittleFS si fichier manquant
  // TODO: Logger l'initialisation (logs/)
  SPI.setTX(FREENOVE_TFT_MOSI);
  SPI.setRX(FREENOVE_TFT_MISO);
  SPI.begin();
  // === BOUCLE PRINCIPALE D'INTÉGRATION ===

  // 1. Navigation UI (LVGL, écrans dynamiques)
  //    - Afficher l'écran courant (chargé depuis data/)
  //    - Gérer les transitions (boutons/tactile)
  //    - Mettre à jour l'affichage selon l'état du scénario

  // 2. Exécution du scénario
  //    - Lire le fichier de scénario courant (data/scene_xxx.json)
  //    - Exécuter les actions (audio, transitions, effets)
  //    - Gérer les transitions de scène (fin, next, choix UI)

  // 3. Gestion audio
  //    - Jouer les fichiers audio associés à la scène/écran
  //    - Stopper/mettre en pause selon l'état du scénario
  //    - Gérer les erreurs de lecture (fallback, logs)

  // 4. Gestion stockage
  //    - Vérifier la présence des fichiers nécessaires (LittleFS)
  //    - Fallback si fichier manquant (scénario par défaut)
  //    - Logger les accès/erreurs

  // 5. Gestion boutons/tactile
  //    - Lire les événements boutons/tactile
  //    - Mapper vers actions UI/scénario
  //    - Logger les interactions

  // 6. Logs/artefacts
  //    - Générer des logs d'évidence (logs/)
  //    - Produire des artefacts pour validation

  // TODO: Implémenter chaque étape dans les modules correspondants

  // LVGL init
  lv_init();
  // TODO: config LVGL port, buffer, etc.

  // Init audio et scénario
  g_audio.begin();
  g_scenario.begin();
  // TODO: init boutons, capteurs Freenove
}

void loop() {
  // Boucle fusionnée audio + UI + scénario
  // Exécution du scénario (exemple)
  // g_scenario.nextStep();
  // Gestion audio (exemple)
  // if (!g_audio.isPlaying()) g_audio.play("/audio/step1.mp3");
  // TODO: gestion UI, boutons, capteurs
  lv_timer_handler();
  delay(10);
}

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

struct UiScreen {
  String id;
  String description;
};

bool loadUiScreen(const char* filename, UiScreen& outScreen) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.printf("[UI_SCREEN] Fichier introuvable: %s\n", filename);
    return false;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.printf("[UI_SCREEN] Erreur JSON: %s\n", err.c_str());
    return false;
  }
  outScreen.id = doc["id"] | "";
  outScreen.description = doc["content"]["description"] | "";
  return true;
}

// Exemple d'utilisation :
// UiScreen screen;
// if (loadUiScreen("/ready.json", screen)) {
//   Serial.println(screen.id);
//   Serial.println(screen.description);
// }

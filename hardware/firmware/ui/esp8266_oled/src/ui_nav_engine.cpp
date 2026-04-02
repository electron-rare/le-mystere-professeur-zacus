#include "../include/ui_nav_engine.h"
#include <LittleFS.h>

bool UiNavEngine::loadScreen(const char* screenId) {
  char path[64];
  snprintf(path, sizeof(path), "/%s.json", screenId); // Ex: /SCENE_READY.json
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return false;
  current_.id = doc["id"] | "";
  current_.description = doc["content"]["description"] | "";
  current_.actions.clear();
  if (doc.containsKey("actions")) {
    parseActions(doc["actions"].as<JsonArray>());
  }
  // Gère timeout automatique si présent
  timeoutMs_ = 0;
  for (const auto& action : current_.actions) {
    if (action.event == "timeout" && action.delay > 0) {
      timeoutMs_ = millis() + action.delay;
      break;
    }
  }
  return true;
}

void UiNavEngine::parseActions(const JsonArray& arr) {
  for (JsonObject obj : arr) {
    UiScreenAction a;
    a.event = obj["event"] | "";
    a.goto_id = obj["goto"] | "";
    a.delay = obj["delay"] | 0;
    a.audio = obj["play_audio"] | "";
    a.popup = obj["show_popup"] | "";
    a.led = obj["set_led"] | "";
    a.condition = obj["if"] | "";
    a.sync = obj["sync"] | "";
    a.reset = obj["reset"] | "";
    a.end = obj["end"] | "";
    current_.actions.push_back(a);
  }
}

void UiNavEngine::handleEvent(const String& event) {
  for (const auto& action : current_.actions) {
    if (action.event == event) {
      triggerAction(action);
      break;
    }
  }
}

void UiNavEngine::update() {
  if (timeoutMs_ > 0 && millis() > timeoutMs_) {
    for (const auto& action : current_.actions) {
      if (action.event == "timeout") {
        triggerAction(action);
        timeoutMs_ = 0;
        break;
      }
    }
  }
}

void UiNavEngine::triggerAction(const UiScreenAction& action) {
  if (!action.condition.isEmpty()) {
    Serial.printf("[UI_NAV] condition=%s (evaluation deferred)\n", action.condition.c_str());
  }
  if (!action.audio.isEmpty()) {
    Serial.printf("[UI_NAV] play_audio=%s\n", action.audio.c_str());
  }
  if (!action.popup.isEmpty()) {
    Serial.printf("[UI_NAV] show_popup=%s\n", action.popup.c_str());
  }
  if (!action.led.isEmpty()) {
    Serial.printf("[UI_NAV] set_led=%s\n", action.led.c_str());
  }
  if (!action.sync.isEmpty()) {
    Serial.printf("[UI_NAV] sync=%s\n", action.sync.c_str());
  }
  const bool hasReset = !action.reset.isEmpty() && action.reset != "0" && action.reset != "false";
  if (hasReset) {
    Serial.printf("[UI_NAV] reset=%s\n", action.reset.c_str());
    if (!current_.id.isEmpty()) {
      loadScreen(current_.id.c_str());
      return;
    }
  }
  const bool hasEnd = !action.end.isEmpty() && action.end != "0" && action.end != "false";
  if (hasEnd) {
    Serial.printf("[UI_NAV] end=%s\n", action.end.c_str());
    timeoutMs_ = 0;
    current_.actions.clear();
    return;
  }
  if (!action.goto_id.isEmpty()) {
    loadScreen(action.goto_id.c_str());
  }
}

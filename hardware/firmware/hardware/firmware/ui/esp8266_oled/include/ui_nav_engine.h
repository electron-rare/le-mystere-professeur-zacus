#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

struct UiScreenAction {
  String event;
  String goto_id;
  int delay = 0;
  String audio;
  String popup;
  String led;
  String condition;
  String sync;
  String reset;
  String end;
};

struct UiScreenDesc {
  String id;
  String description;
  std::vector<UiScreenAction> actions;
};

class UiNavEngine {
 public:
  bool loadScreen(const char* screenId);
  void handleEvent(const String& event);
  void update(); // à appeler dans loop()
  const UiScreenDesc& currentScreen() const { return current_; }

 private:
  UiScreenDesc current_;
  uint32_t timeoutMs_ = 0;
  void parseActions(const JsonArray& arr);
  void triggerAction(const UiScreenAction& action);
};

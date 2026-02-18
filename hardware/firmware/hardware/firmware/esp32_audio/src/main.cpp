#include <Arduino.h>

#include "app.h"

namespace {
App g_app;
}

void setup() {
  g_app.setup();
}

void loop() {
  g_app.loop();
}

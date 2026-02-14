#include "app.h"

#include "app/app_orchestrator.h"

void App::setup() {
  app_orchestrator::setup();
}

void App::loop() {
  app_orchestrator::loop();
}

#include "app/runtime_web_service.h"

void RuntimeWebService::configure(SetupWebUiFn setup_web_ui) {
  setup_web_ui_ = setup_web_ui;
}

void RuntimeWebService::setupWebUi() const {
  if (setup_web_ui_ == nullptr) {
    return;
  }
  setup_web_ui_();
}

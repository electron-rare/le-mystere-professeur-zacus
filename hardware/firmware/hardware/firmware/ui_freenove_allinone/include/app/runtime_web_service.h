// runtime_web_service.h - Web UI route setup bridge.
#pragma once

class RuntimeWebService {
 public:
  using SetupWebUiFn = void (*)();

  void configure(SetupWebUiFn setup_web_ui);
  void setupWebUi() const;

 private:
  SetupWebUiFn setup_web_ui_ = nullptr;
};

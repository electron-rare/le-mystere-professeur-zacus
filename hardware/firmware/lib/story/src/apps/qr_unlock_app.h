#pragma once

#include "story_app.h"

class QrUnlockApp : public StoryApp {
 public:
  bool begin(const StoryAppContext& context) override;
  void start(const StoryStepContext& stepContext) override;
  void update(uint32_t nowMs, const StoryEventSink& sink) override;
  void stop(const char* reason) override;
  bool handleEvent(const StoryEvent& event, const StoryEventSink& sink) override;
  StoryAppSnapshot snapshot() const override;

 private:
  char binding_id_[24] = {0};
  StoryAppSnapshot snapshot_ = {};
};

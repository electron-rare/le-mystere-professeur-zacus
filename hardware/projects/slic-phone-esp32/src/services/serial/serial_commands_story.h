#pragma once

#include <Arduino.h>

#include "serial_dispatch.h"
#include "serial_router.h"

class StoryController;
class StoryControllerV2;
class StoryFsManager;
class StoryPortableRuntime;

struct StorySerialRuntimeContext {
  bool* storyV2Enabled = nullptr;
  bool uSonFunctional = false;
  bool storyV2Default = false;
  StoryController* legacy = nullptr;
  StoryControllerV2* v2 = nullptr;
  StoryFsManager* fsManager = nullptr;
  StoryPortableRuntime* portable = nullptr;
  void (*armAfterUnlock)(uint32_t nowMs) = nullptr;
  void (*updateStoryTimeline)(uint32_t nowMs) = nullptr;
  void (*printHelp)() = nullptr;
};

bool serialIsStoryCommand(const char* token);
bool serialProcessStoryCommand(const SerialCommand& cmd,
                               uint32_t nowMs,
                               const StorySerialRuntimeContext& ctx,
                               Print& out);
bool serialProcessStoryJsonV3(const char* jsonLine,
                              uint32_t nowMs,
                              const StorySerialRuntimeContext& ctx,
                              Print& out);

// runtime_services.h - dependency container shared by coordinator/router.
#pragma once

#include <Arduino.h>

#include "audio_manager.h"
#include "button_manager.h"
#include "camera_manager.h"
#include "hardware_manager.h"
#include "media_manager.h"
#include "network_manager.h"
#include "runtime/runtime_config_types.h"
#include "scenario_manager.h"
#include "storage_manager.h"
#include "touch_manager.h"
#include "ui_manager.h"

struct RuntimeServices;

using RuntimeTickHook = void (*)(uint32_t now_ms, RuntimeServices* services);
using RuntimeSerialDispatchHook = void (*)(const char* command_line,
                                           uint32_t now_ms,
                                           RuntimeServices* services);

struct RuntimeServices {
  AudioManager* audio = nullptr;
  ScenarioManager* scenario = nullptr;
  UiManager* ui = nullptr;
  StorageManager* storage = nullptr;
  ButtonManager* buttons = nullptr;
  TouchManager* touch = nullptr;
  NetworkManager* network = nullptr;
  HardwareManager* hardware = nullptr;
  CameraManager* camera = nullptr;
  MediaManager* media = nullptr;

  RuntimeNetworkConfig* network_cfg = nullptr;
  RuntimeHardwareConfig* hardware_cfg = nullptr;
  CameraManager::Config* camera_cfg = nullptr;
  MediaManager::Config* media_cfg = nullptr;

  RuntimeTickHook tick_runtime = nullptr;
  RuntimeSerialDispatchHook dispatch_serial = nullptr;
};

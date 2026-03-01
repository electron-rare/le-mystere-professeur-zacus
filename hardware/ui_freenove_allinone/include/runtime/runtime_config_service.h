// runtime_config_service.h - load APP_* runtime configs from story files.
#pragma once

#include "camera_manager.h"
#include "media_manager.h"
#include "runtime/runtime_config_types.h"
#include "storage_manager.h"

class RuntimeConfigService {
 public:
  static void load(StorageManager& storage,
                   RuntimeNetworkConfig* network_cfg,
                   RuntimeHardwareConfig* hardware_cfg,
                   CameraManager::Config* camera_cfg,
                   MediaManager::Config* media_cfg);
};

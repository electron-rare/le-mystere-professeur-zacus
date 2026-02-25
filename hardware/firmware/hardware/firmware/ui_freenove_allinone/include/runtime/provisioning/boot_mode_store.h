// boot_mode_store.h - NVS persistence for startup mode policy.
#pragma once

#include <Arduino.h>

class BootModeStore {
 public:
  enum class StartupMode : uint8_t {
    kStory = 0,
    kMediaManager = 1,
  };

  bool loadMode(StartupMode* out_mode) const;
  bool saveMode(StartupMode mode) const;
  bool clearMode() const;

  bool isMediaValidated() const;
  bool setMediaValidated(bool validated) const;

  static const char* modeLabel(StartupMode mode);
};

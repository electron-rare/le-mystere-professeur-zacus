// boot_mode_store.cpp - NVS startup mode persistence.
#include "runtime/provisioning/boot_mode_store.h"

#include <Preferences.h>

namespace {

constexpr const char* kNamespace = "zacus_boot";
constexpr const char* kKeyStartupMode = "startup_mode";
constexpr const char* kKeyMediaValidated = "media_validated";

}  // namespace

bool BootModeStore::loadMode(StartupMode* out_mode) const {
  if (out_mode == nullptr) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  const String mode_text = prefs.getString(kKeyStartupMode, "story");
  prefs.end();
  if (mode_text.equalsIgnoreCase("media_manager")) {
    *out_mode = StartupMode::kMediaManager;
  } else {
    *out_mode = StartupMode::kStory;
  }
  return true;
}

bool BootModeStore::saveMode(StartupMode mode) const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const size_t written = prefs.putString(kKeyStartupMode, modeLabel(mode));
  prefs.end();
  return written > 0U;
}

bool BootModeStore::clearMode() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.remove(kKeyStartupMode);
  prefs.remove(kKeyMediaValidated);
  prefs.end();
  return true;
}

bool BootModeStore::isMediaValidated() const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }
  const bool validated = prefs.getBool(kKeyMediaValidated, false);
  prefs.end();
  return validated;
}

bool BootModeStore::setMediaValidated(bool validated) const {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  prefs.putBool(kKeyMediaValidated, validated);
  prefs.end();
  return true;
}

const char* BootModeStore::modeLabel(StartupMode mode) {
  return (mode == StartupMode::kMediaManager) ? "media_manager" : "story";
}

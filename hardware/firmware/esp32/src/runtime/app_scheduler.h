#pragma once

#include "runtime_mode.h"

struct AppSchedulerInputs {
  RuntimeMode currentMode = RuntimeMode::kSignal;
  bool uSonFunctional = false;
  bool unlockJingleActive = false;
  bool sdReady = false;
  bool hasTracks = false;
  bool laDetectionEnabled = false;
  bool sineEnabled = false;
  bool bootProtocolActive = false;
};

struct AppBrickSchedule {
  bool runBootProtocol = false;
  bool runSerialConsole = false;
  bool runUnlockJingle = false;
  bool runMp3Service = false;
  bool allowMp3Playback = false;
  bool runSineDac = false;
  bool runLaDetector = false;
};

RuntimeMode schedulerSelectRuntimeMode(const AppSchedulerInputs& input);
AppBrickSchedule schedulerBuildBricks(const AppSchedulerInputs& input);

#include "app_scheduler.h"

RuntimeMode schedulerSelectRuntimeMode(const AppSchedulerInputs& input) {
  if (input.currentMode == RuntimeMode::kSignal &&
      (!input.uSonFunctional || input.unlockJingleActive)) {
    return RuntimeMode::kSignal;
  }

  if (input.sdReady && input.hasTracks) {
    return RuntimeMode::kMp3;
  }
  return RuntimeMode::kSignal;
}

AppBrickSchedule schedulerBuildBricks(const AppSchedulerInputs& input) {
  AppBrickSchedule schedule;
  schedule.runBootProtocol = input.bootProtocolActive;
  schedule.runSerialConsole = !input.bootProtocolActive;
  schedule.runUnlockJingle = (input.currentMode == RuntimeMode::kSignal);
  schedule.runMp3Service =
      (input.currentMode == RuntimeMode::kMp3) ||
      (input.uSonFunctional && !input.unlockJingleActive);
  schedule.allowMp3Playback = (input.currentMode == RuntimeMode::kMp3);
  schedule.runSineDac = (input.currentMode == RuntimeMode::kSignal) && input.sineEnabled;
  schedule.runLaDetector =
      (input.currentMode == RuntimeMode::kSignal) && input.laDetectionEnabled;
  return schedule;
}

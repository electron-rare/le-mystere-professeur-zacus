#include "action_registry.h"

#include <cstring>

namespace {

constexpr StoryActionDef kActions[] = {
    {"ACTION_TRACE_STEP", StoryActionType::kTrace, 0},
    {"ACTION_QUEUE_SONAR", StoryActionType::kQueueSonarCue, 0},
    {"ACTION_REFRESH_SD", StoryActionType::kRequestSdRefresh, 0},
};

}  // namespace

const StoryActionDef* storyFindAction(const char* actionId) {
  if (actionId == nullptr || actionId[0] == '\0') {
    return nullptr;
  }
  for (const StoryActionDef& action : kActions) {
    if (action.id != nullptr && strcmp(action.id, actionId) == 0) {
      return &action;
    }
  }
  return nullptr;
}

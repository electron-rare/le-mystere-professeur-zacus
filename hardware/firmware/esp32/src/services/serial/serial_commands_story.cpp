#include "serial_commands_story.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../../controllers/story/story_controller.h"
#include "../../controllers/story/story_controller_v2.h"

namespace {

bool textEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  return strcmp(lhs, rhs) == 0;
}

bool textEqualsIgnoreCase(const char* lhs, const char* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  while (*lhs != '\0' && *rhs != '\0') {
    if (toupper(static_cast<unsigned char>(*lhs)) != toupper(static_cast<unsigned char>(*rhs))) {
      return false;
    }
    ++lhs;
    ++rhs;
  }
  return (*lhs == '\0' && *rhs == '\0');
}

bool parseTraceLevelToken(const char* token, StoryControllerV2::TraceLevel* outLevel) {
  if (token == nullptr || outLevel == nullptr) {
    return false;
  }
  if (textEqualsIgnoreCase(token, "OFF")) {
    *outLevel = StoryControllerV2::TraceLevel::kOff;
    return true;
  }
  if (textEqualsIgnoreCase(token, "ERR")) {
    *outLevel = StoryControllerV2::TraceLevel::kErr;
    return true;
  }
  if (textEqualsIgnoreCase(token, "INFO")) {
    *outLevel = StoryControllerV2::TraceLevel::kInfo;
    return true;
  }
  if (textEqualsIgnoreCase(token, "DEBUG")) {
    *outLevel = StoryControllerV2::TraceLevel::kDebug;
    return true;
  }
  return false;
}

const char* skipSpaces(const char* text) {
  if (text == nullptr) {
    return "";
  }
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text)) != 0) {
    ++text;
  }
  return text;
}

bool isV2Enabled(const StorySerialRuntimeContext& ctx) {
  return ctx.storyV2Enabled != nullptr && *ctx.storyV2Enabled;
}

void printV2Status(Print& out, uint32_t nowMs, const StorySerialRuntimeContext& ctx, const char* source) {
  if (ctx.v2 == nullptr) {
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "controller_missing");
    return;
  }
  out.printf("[STORY_V2] enabled=%u default=%u\n",
             isV2Enabled(ctx) ? 1U : 0U,
             ctx.storyV2Default ? 1U : 0U);
  ctx.v2->printStatus(nowMs, source);
}

}  // namespace

bool serialIsStoryCommand(const char* token) {
  return token != nullptr && strncmp(token, "STORY_", 6U) == 0;
}

bool serialProcessStoryCommand(const SerialCommand& cmd,
                               uint32_t nowMs,
                               const StorySerialRuntimeContext& ctx,
                               Print& out) {
  if (cmd.token == nullptr || cmd.token[0] == '\0') {
    return false;
  }

  const bool useV2 = isV2Enabled(ctx);
  const char* args = skipSpaces(cmd.args);

  if (serialTokenEquals(cmd, "STORY_HELP")) {
    if (ctx.printHelp != nullptr) {
      ctx.printHelp();
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_TRACE")) {
    if (ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "controller_missing");
      return true;
    }
    if (args[0] == '\0' || textEqualsIgnoreCase(args, "STATUS")) {
      out.printf("[STORY_V2] trace=%u\n", ctx.v2->traceEnabled() ? 1U : 0U);
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "trace_status");
      return true;
    }
    if (textEqualsIgnoreCase(args, "ON")) {
      ctx.v2->setTraceEnabled(true);
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "trace_on");
      return true;
    }
    if (textEqualsIgnoreCase(args, "OFF")) {
      ctx.v2->setTraceEnabled(false);
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "trace_off");
      return true;
    }
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "ON|OFF|STATUS");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_TRACE_LEVEL")) {
    if (ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "controller_missing");
      return true;
    }
    if (args[0] == '\0' || textEqualsIgnoreCase(args, "STATUS")) {
      out.printf("[STORY_V2] trace_level=%s\n", ctx.v2->traceLevelLabel());
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "trace_level_status");
      return true;
    }
    StoryControllerV2::TraceLevel level = StoryControllerV2::TraceLevel::kOff;
    if (!parseTraceLevelToken(args, &level)) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "OFF|ERR|INFO|DEBUG");
      return true;
    }
    ctx.v2->setTraceLevel(level);
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, ctx.v2->traceLevelLabel());
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_ENABLE")) {
    if (ctx.storyV2Enabled == nullptr || ctx.v2 == nullptr || ctx.legacy == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "missing_context");
      return true;
    }
    if (args[0] == '\0' || textEqualsIgnoreCase(args, "STATUS")) {
      printV2Status(out, nowMs, ctx, "serial_story_v2_enable_status");
      return true;
    }
    if (textEqualsIgnoreCase(args, "ON")) {
      if (!useV2) {
        *ctx.storyV2Enabled = true;
        if (!ctx.v2->begin(nowMs)) {
          *ctx.storyV2Enabled = false;
          serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBusy, "begin_failed");
          return true;
        }
        if (ctx.uSonFunctional) {
          ctx.v2->onUnlock(nowMs, "serial_story_v2_enable_sync");
        }
      }
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "enabled");
      ctx.v2->printStatus(nowMs, "serial_story_v2_enable");
      return true;
    }
    if (textEqualsIgnoreCase(args, "OFF")) {
      if (useV2) {
        *ctx.storyV2Enabled = false;
        ctx.legacy->reset("serial_story_v2_disable");
        if (ctx.uSonFunctional) {
          ctx.legacy->onUnlock(nowMs, "serial_story_v2_disable_sync");
        }
      }
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "disabled");
      ctx.legacy->printStatus(nowMs, "serial_story_v2_disable");
      return true;
    }
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "STATUS|ON|OFF");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_STATUS")) {
    printV2Status(out, nowMs, ctx, "serial_story_v2_status");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_HEALTH")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    const StoryControllerV2::StoryControllerV2Snapshot snap = ctx.v2->snapshot(true, nowMs);
    const char* health = ctx.v2->healthLabel(true, nowMs);
    out.printf(
        "[STORY_V2] HEALTH status=%s enabled=%u run=%u scenario=%s step=%s gate=%u queue=%u app_err=%s engine_err=%s due=%lu test=%u\n",
        health,
        snap.enabled ? 1U : 0U,
        snap.running ? 1U : 0U,
        snap.scenarioId != nullptr ? snap.scenarioId : "-",
        snap.stepId != nullptr ? snap.stepId : "-",
        snap.mp3GateOpen ? 1U : 0U,
        static_cast<unsigned int>(snap.queueDepth),
        snap.appHostError != nullptr ? snap.appHostError : "-",
        snap.engineError != nullptr ? snap.engineError : "-",
        static_cast<unsigned long>(snap.etape2DueMs),
        snap.testMode ? 1U : 0U);
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, health);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_METRICS")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    const StoryControllerV2::StoryMetricsSnapshot metrics = ctx.v2->metricsSnapshot();
    out.printf(
        "[STORY_V2] METRICS posted=%lu accepted=%lu rejected=%lu storm_drop=%lu queue_drop=%lu transitions=%lu max_queue=%u app_err=%s engine_err=%s\n",
        static_cast<unsigned long>(metrics.eventsPosted),
        static_cast<unsigned long>(metrics.eventsAccepted),
        static_cast<unsigned long>(metrics.eventsRejected),
        static_cast<unsigned long>(metrics.stormDropped),
        static_cast<unsigned long>(metrics.queueDropped),
        static_cast<unsigned long>(metrics.transitions),
        static_cast<unsigned int>(metrics.maxQueueDepth),
        metrics.lastAppHostError != nullptr ? metrics.lastAppHostError : "-",
        metrics.lastEngineError != nullptr ? metrics.lastEngineError : "-");
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "metrics");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_METRICS_RESET")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    ctx.v2->resetMetrics();
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "metrics_reset");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_LIST")) {
    if (ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "controller_missing");
      return true;
    }
    ctx.v2->printScenarioList("serial_story_v2_list");
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "list");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_VALIDATE")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    if (!ctx.v2->validateActiveScenario("serial_story_v2_validate")) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "invalid");
      return true;
    }
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, "valid");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_EVENT")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "event required");
      return true;
    }
    if (!ctx.v2->postSerialEvent(args, nowMs, "serial_story_v2_event")) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBusy, args);
      return true;
    }
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, args);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_STEP")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "step required");
      return true;
    }
    if (!ctx.v2->jumpToStep(args, nowMs, "serial_story_v2_step")) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kNotFound, args);
      return true;
    }
    ctx.v2->printStatus(nowMs, "serial_story_v2_step");
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, args);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_V2_SCENARIO")) {
    if (!useV2 || ctx.v2 == nullptr) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOutOfContext, "legacy mode");
      return true;
    }
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kBadArgs, "scenario required");
      return true;
    }
    if (!ctx.v2->setScenario(args, nowMs, "serial_story_v2_scenario")) {
      serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kNotFound, args);
      return true;
    }
    ctx.v2->printStatus(nowMs, "serial_story_v2_scenario");
    serialDispatchReply(out, "STORY_V2", SerialDispatchResult::kOk, args);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_STATUS")) {
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->printStatus(nowMs, "serial_story_status");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->printStatus(nowMs, "serial_story_status");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_RESET")) {
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->reset(nowMs, "serial_story_reset");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->reset("serial_story_reset");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_ARM")) {
    if (ctx.armAfterUnlock != nullptr) {
      ctx.armAfterUnlock(nowMs);
    }
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->printStatus(nowMs, "serial_story_arm");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->printStatus(nowMs, "serial_story_arm");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_FORCE_ETAPE2")) {
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->forceEtape2DueNow(nowMs, "serial_story_force");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->forceEtape2DueNow(nowMs, "serial_story_force");
    }
    if (ctx.updateStoryTimeline != nullptr) {
      ctx.updateStoryTimeline(nowMs);
    }
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->printStatus(nowMs, "serial_story_force");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->printStatus(nowMs, "serial_story_force");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_TEST_ON")) {
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->setTestMode(true, nowMs, "serial_story_test_on");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->setTestMode(true, nowMs, "serial_story_test_on");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_TEST_OFF")) {
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->setTestMode(false, nowMs, "serial_story_test_off");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->setTestMode(false, nowMs, "serial_story_test_off");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_TEST_DELAY")) {
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBadArgs, "delay required");
      return true;
    }
    unsigned long delayMs = 0UL;
    if (sscanf(args, "%lu", &delayMs) != 1) {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBadArgs, "delay parse");
      return true;
    }
    if (useV2 && ctx.v2 != nullptr) {
      ctx.v2->setTestDelayMs(static_cast<uint32_t>(delayMs), nowMs, "serial_story_test_delay");
    } else if (ctx.legacy != nullptr) {
      ctx.legacy->setTestDelayMs(static_cast<uint32_t>(delayMs), nowMs, "serial_story_test_delay");
    }
    return true;
  }

  return false;
}

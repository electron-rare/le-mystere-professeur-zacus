#include "serial_commands_story.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include <ArduinoJson.h>
#include <zacus_story_portable/story_portable_runtime.h>

#include "controllers/story/story_controller.h"
#include "controllers/story/story_controller_v2.h"
#include "fs/story_fs_manager.h"

namespace {

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

const char* skipSpaces(const char* text) {
  if (text == nullptr) {
    return "";
  }
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text)) != 0) {
    ++text;
  }
  return text;
}

bool splitArgs(const char* args, char* first, size_t firstLen, const char** rest) {
  if (first == nullptr || firstLen == 0U) {
    return false;
  }
  first[0] = '\0';
  if (rest != nullptr) {
    *rest = "";
  }
  const char* cur = skipSpaces(args);
  if (cur == nullptr || cur[0] == '\0') {
    return false;
  }
  size_t idx = 0U;
  while (cur[idx] != '\0' && !isspace(static_cast<unsigned char>(cur[idx])) && idx + 1U < firstLen) {
    first[idx] = cur[idx];
    ++idx;
  }
  first[idx] = '\0';
  if (rest != nullptr) {
    *rest = skipSpaces(cur + idx);
  }
  return first[0] != '\0';
}

bool isV2Enabled(const StorySerialRuntimeContext& ctx) {
  return ctx.storyV2Enabled != nullptr && *ctx.storyV2Enabled;
}

bool ensureV3RuntimeReady(uint32_t nowMs,
                          const StorySerialRuntimeContext& ctx,
                          Print& out,
                          const char* source,
                          bool emitLegacyReply) {
  if (ctx.v2 == nullptr || ctx.legacy == nullptr || ctx.storyV2Enabled == nullptr) {
    if (emitLegacyReply) {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kOutOfContext, "missing_context");
    }
    return false;
  }

  if (*ctx.storyV2Enabled) {
    return true;
  }

  *ctx.storyV2Enabled = true;
  if (!ctx.v2->begin(nowMs)) {
    *ctx.storyV2Enabled = false;
    if (emitLegacyReply) {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBusy, "begin_failed");
    }
    return false;
  }
  if (ctx.uSonFunctional) {
    ctx.v2->onUnlock(nowMs, source != nullptr ? source : "story_v3_auto_enable");
  }
  return true;
}

void jsonReply(Print& out, bool ok, const char* code, const char* detail = nullptr) {
  StaticJsonDocument<384> doc;
  doc["ok"] = ok;
  doc["code"] = code != nullptr ? code : (ok ? "ok" : "error");
  JsonObject data = doc.createNestedObject("data");
  if (detail != nullptr && detail[0] != '\0') {
    data["detail"] = detail;
  }
  serializeJson(doc, out);
  out.print('\n');
}

void jsonStatusReply(Print& out, const StorySerialRuntimeContext& ctx, uint32_t nowMs) {
  StaticJsonDocument<768> doc;
  doc["ok"] = true;
  doc["code"] = "ok";
  JsonObject data = doc.createNestedObject("data");

  data["protocol"] = "story.v3";
  data["v2_enabled"] = isV2Enabled(ctx);
  if (ctx.portable != nullptr) {
    const StoryPortableSnapshot snap = ctx.portable->snapshot(true, nowMs);
    data["state"] = snap.runtimeState != nullptr ? snap.runtimeState : "idle";
    data["running"] = snap.running;
    data["scenario"] = snap.scenarioId != nullptr ? snap.scenarioId : "";
    data["step"] = snap.stepId != nullptr ? snap.stepId : "";
    data["scenario_from_fs"] = snap.scenarioFromLittleFs;
    data["error"] = snap.lastError != nullptr ? snap.lastError : "";
  } else if (ctx.v2 != nullptr) {
    const auto snap = ctx.v2->snapshot(true, nowMs);
    data["state"] = snap.running ? "running" : "idle";
    data["running"] = snap.running;
    data["scenario"] = snap.scenarioId != nullptr ? snap.scenarioId : "";
    data["step"] = snap.stepId != nullptr ? snap.stepId : "";
    data["error"] = ctx.v2->lastError() != nullptr ? ctx.v2->lastError() : "";
  }

  serializeJson(doc, out);
  out.print('\n');
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

  const char* args = skipSpaces(cmd.args);

  if (serialTokenEquals(cmd, "STORY_HELP")) {
    if (ctx.printHelp != nullptr) {
      ctx.printHelp();
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_STATUS")) {
    if (!ensureV3RuntimeReady(nowMs, ctx, out, "serial_story_status", true)) {
      return true;
    }
    if (ctx.v2 != nullptr) {
      ctx.v2->printStatus(nowMs, "serial_story_status");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_RESET")) {
    if (ctx.v2 != nullptr) {
      ctx.v2->reset(nowMs, "serial_story_reset");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_ARM")) {
    if (ctx.armAfterUnlock != nullptr) {
      ctx.armAfterUnlock(nowMs);
    }
    if (ctx.v2 != nullptr) {
      ctx.v2->printStatus(nowMs, "serial_story_arm");
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_FORCE_ETAPE2")) {
    if (!ensureV3RuntimeReady(nowMs, ctx, out, "serial_story_force_etape2", true)) {
      return true;
    }
    if (ctx.v2 != nullptr) {
      ctx.v2->forceEtape2DueNow(nowMs, "serial_story_force_etape2");
      ctx.v2->printStatus(nowMs, "serial_story_force_etape2");
    }
    if (ctx.updateStoryTimeline != nullptr) {
      ctx.updateStoryTimeline(nowMs);
    }
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_LOAD_SCENARIO")) {
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBadArgs, "scenario required");
      return true;
    }
    if (!ensureV3RuntimeReady(nowMs, ctx, out, "serial_story_load_scenario", true)) {
      return true;
    }
    const bool loaded =
        (ctx.portable != nullptr) ? ctx.portable->loadScenario(args, nowMs, "serial_story_load_scenario")
                                  : (ctx.v2 != nullptr && ctx.v2->setScenario(args, nowMs, "serial_story_load_scenario"));
    serialDispatchReply(out, "STORY", loaded ? SerialDispatchResult::kOk : SerialDispatchResult::kNotFound, args);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_FORCE_STEP")) {
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBadArgs, "step required");
      return true;
    }
    if (!ensureV3RuntimeReady(nowMs, ctx, out, "serial_story_force_step", true)) {
      return true;
    }
    const bool ok = ctx.v2 != nullptr && ctx.v2->jumpToStep(args, nowMs, "serial_story_force_step");
    serialDispatchReply(out, "STORY", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kNotFound, args);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_FS_LIST")) {
    if (args[0] == '\0') {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBadArgs, "type required");
      return true;
    }
    if (ctx.portable != nullptr) {
      ctx.portable->listResources(args);
      serialDispatchReply(out, "STORY", SerialDispatchResult::kOk, args);
      return true;
    }
    if (ctx.fsManager != nullptr) {
      ctx.fsManager->listResources(args);
      serialDispatchReply(out, "STORY", SerialDispatchResult::kOk, args);
      return true;
    }
    serialDispatchReply(out, "STORY", SerialDispatchResult::kOutOfContext, "fs_missing");
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_FS_VALIDATE")) {
    char type[24] = {};
    const char* rest = "";
    if (!splitArgs(args, type, sizeof(type), &rest) || rest[0] == '\0') {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kBadArgs, "type id required");
      return true;
    }
    bool ok = false;
    if (ctx.portable != nullptr) {
      ok = ctx.portable->validateChecksum(type, rest);
    } else if (ctx.fsManager != nullptr) {
      ok = ctx.fsManager->validateChecksum(type, rest);
    } else {
      serialDispatchReply(out, "STORY", SerialDispatchResult::kOutOfContext, "fs_missing");
      return true;
    }
    serialDispatchReply(out, "STORY", ok ? SerialDispatchResult::kOk : SerialDispatchResult::kBadArgs, rest);
    return true;
  }

  if (serialTokenEquals(cmd, "STORY_DEPLOY")) {
    serialDispatchReply(out, "STORY", SerialDispatchResult::kOutOfContext, "use story.deploy.bundle");
    return true;
  }

  return false;
}

bool serialProcessStoryJsonV3(const char* jsonLine,
                              uint32_t nowMs,
                              const StorySerialRuntimeContext& ctx,
                              Print& out) {
  if (jsonLine == nullptr) {
    return false;
  }

  StaticJsonDocument<768> request;
  const DeserializationError err = deserializeJson(request, jsonLine);
  if (err) {
    jsonReply(out, false, "bad_json", err.c_str());
    return true;
  }

  const char* command = request["cmd"] | "";
  if (command[0] == '\0') {
    jsonReply(out, false, "bad_args", "cmd required");
    return true;
  }

  if (!textEqualsIgnoreCase(command, "story.status") &&
      !ensureV3RuntimeReady(nowMs, ctx, out, "story_v3_json", false)) {
    jsonReply(out, false, "out_of_context", "runtime_not_ready");
    return true;
  }

  if (textEqualsIgnoreCase(command, "story.status")) {
    jsonStatusReply(out, ctx, nowMs);
    return true;
  }

  if (textEqualsIgnoreCase(command, "story.list")) {
    StaticJsonDocument<1536> response;
    response["ok"] = true;
    response["code"] = "ok";
    JsonObject data = response.createNestedObject("data");
    JsonArray scenarios = data.createNestedArray("scenarios");

    StoryPortableCatalogEntry entries[24] = {};
    size_t count = 0U;
    if (StoryPortableCatalog::listScenarios(ctx.fsManager, entries, 24U, &count)) {
      for (size_t i = 0U; i < count; ++i) {
        JsonObject row = scenarios.createNestedObject();
        row["id"] = entries[i].id;
        row["from_fs"] = entries[i].fromLittleFs;
        row["from_generated"] = entries[i].fromGenerated;
        row["version"] = entries[i].version;
      }
    }

    serializeJson(response, out);
    out.print('\n');
    return true;
  }

  if (textEqualsIgnoreCase(command, "story.load")) {
    const char* scenarioId = request["data"]["scenario"] | request["scenario"] | "";
    if (scenarioId[0] == '\0') {
      jsonReply(out, false, "bad_args", "scenario required");
      return true;
    }
    const bool ok =
        (ctx.portable != nullptr) ? ctx.portable->setScenario(scenarioId, nowMs, "story_v3_load")
                                  : (ctx.v2 != nullptr && ctx.v2->setScenario(scenarioId, nowMs, "story_v3_load"));
    jsonReply(out, ok, ok ? "ok" : "not_found", scenarioId);
    return true;
  }

  if (textEqualsIgnoreCase(command, "story.step")) {
    const char* stepId = request["data"]["step"] | request["step"] | "";
    if (stepId[0] == '\0') {
      jsonReply(out, false, "bad_args", "step required");
      return true;
    }
    const bool ok = ctx.v2 != nullptr && ctx.v2->jumpToStep(stepId, nowMs, "story_v3_step");
    jsonReply(out, ok, ok ? "ok" : "not_found", stepId);
    return true;
  }

  if (textEqualsIgnoreCase(command, "story.validate")) {
    const bool valid =
        (ctx.portable != nullptr) ? ctx.portable->validateActiveScenario("story_v3_validate")
                                  : (ctx.v2 != nullptr && ctx.v2->validateActiveScenario("story_v3_validate"));
    jsonReply(out, valid, valid ? "valid" : "invalid", nullptr);
    return true;
  }

  if (textEqualsIgnoreCase(command, "story.event")) {
    const char* eventName = request["data"]["event"] | request["event"] | "";
    if (eventName[0] == '\0') {
      jsonReply(out, false, "bad_args", "event required");
      return true;
    }
    const bool ok = ctx.v2 != nullptr && ctx.v2->postSerialEvent(eventName, nowMs, "story_v3_event");
    jsonReply(out, ok, ok ? "ok" : "busy", eventName);
    return true;
  }

  jsonReply(out, false, "unknown_cmd", command);
  return true;
}

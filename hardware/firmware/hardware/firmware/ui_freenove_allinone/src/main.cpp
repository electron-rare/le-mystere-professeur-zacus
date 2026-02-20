// main.cpp - Freenove ESP32-S3 all-in-one runtime loop.
#include <Arduino.h>
#include <cstdlib>
#include <cstring>

#include "audio_manager.h"
#include "button_manager.h"
#include "scenario_manager.h"
#include "storage_manager.h"
#include "touch_manager.h"
#include "ui_manager.h"

namespace {

constexpr const char* kDefaultScenarioFile = "/story/scenarios/DEFAULT.json";
constexpr const char* kDiagAudioFile = "/music/boot_radio.mp3";
constexpr size_t kSerialLineCapacity = 96U;
constexpr bool kBootDiagnosticTone = true;

AudioManager g_audio;
ScenarioManager g_scenario;
UiManager g_ui;
StorageManager g_storage;
ButtonManager g_buttons;
TouchManager g_touch;
char g_serial_line[kSerialLineCapacity] = {0};
size_t g_serial_line_len = 0U;

const char* audioPackToFile(const char* pack_id) {
  if (pack_id == nullptr || pack_id[0] == '\0') {
    return nullptr;
  }
  if (std::strcmp(pack_id, "PACK_BOOT_RADIO") == 0) {
    return "/music/boot_radio.mp3";
  }
  if (std::strcmp(pack_id, "PACK_SONAR_HINT") == 0) {
    return "/music/sonar_hint.mp3";
  }
  if (std::strcmp(pack_id, "PACK_MORSE_HINT") == 0) {
    return "/music/morse_hint.mp3";
  }
  if (std::strcmp(pack_id, "PACK_WIN") == 0) {
    return "/music/win.mp3";
  }
  return "/music/placeholder.mp3";
}

void onAudioFinished(const char* track, void* ctx) {
  (void)ctx;
  Serial.printf("[MAIN] audio done: %s\n", track != nullptr ? track : "unknown");
  g_scenario.notifyAudioDone(millis());
}

void printButtonRead() {
  Serial.printf("BTN mv=%d key=%u\n", g_buttons.lastAnalogMilliVolts(), g_buttons.currentKey());
}

void printRuntimeStatus() {
  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const char* scenario_id =
      (snapshot.scenario != nullptr && snapshot.scenario->id != nullptr) ? snapshot.scenario->id : "n/a";
  const char* step_id = (snapshot.step != nullptr && snapshot.step->id != nullptr) ? snapshot.step->id : "n/a";
  const char* screen_id = (snapshot.screen_scene_id != nullptr) ? snapshot.screen_scene_id : "n/a";
  const char* audio_pack = (snapshot.audio_pack_id != nullptr) ? snapshot.audio_pack_id : "n/a";
  Serial.printf("STATUS scenario=%s step=%s screen=%s pack=%s audio=%u track=%s profile=%u:%s vol=%u key=%u mv=%d\n",
                scenario_id,
                step_id,
                screen_id,
                audio_pack,
                g_audio.isPlaying() ? 1 : 0,
                g_audio.currentTrack(),
                g_audio.outputProfile(),
                g_audio.outputProfileLabel(g_audio.outputProfile()),
                g_audio.volume(),
                g_buttons.currentKey(),
                g_buttons.lastAnalogMilliVolts());
}

void refreshSceneIfNeeded(bool force_render) {
  const bool changed = g_scenario.consumeSceneChanged();
  if (!force_render && !changed) {
    return;
  }

  const ScenarioSnapshot snapshot = g_scenario.snapshot();
  const char* step_id = (snapshot.step != nullptr && snapshot.step->id != nullptr) ? snapshot.step->id : "n/a";
  const String screen_payload = g_storage.loadScenePayloadById(snapshot.screen_scene_id);
  Serial.printf("[UI] render step=%s screen=%s pack=%s playing=%u\n",
                step_id,
                snapshot.screen_scene_id != nullptr ? snapshot.screen_scene_id : "n/a",
                snapshot.audio_pack_id != nullptr ? snapshot.audio_pack_id : "n/a",
                g_audio.isPlaying() ? 1U : 0U);
  g_ui.renderScene(snapshot.scenario,
                   snapshot.screen_scene_id,
                   step_id,
                   snapshot.audio_pack_id,
                   g_audio.isPlaying(),
                   screen_payload.isEmpty() ? nullptr : screen_payload.c_str());
}

void startPendingAudioIfAny() {
  String audio_pack;
  if (!g_scenario.consumeAudioRequest(&audio_pack)) {
    return;
  }

  const String configured_path = g_storage.resolveAudioPathByPackId(audio_pack.c_str());
  const char* mapped_path = audioPackToFile(audio_pack.c_str());
  if (configured_path.isEmpty() && mapped_path == nullptr) {
    if (g_audio.playDiagnosticTone()) {
      Serial.printf("[MAIN] audio pack=%s has no asset mapping, fallback=builtin_tone\n", audio_pack.c_str());
      return;
    }
    Serial.printf("[MAIN] audio pack=%s has no asset mapping and no fallback tone\n", audio_pack.c_str());
    g_scenario.notifyAudioDone(millis());
    return;
  }

  if (!configured_path.isEmpty() && g_audio.play(configured_path.c_str())) {
    Serial.printf("[MAIN] audio pack=%s path=%s source=story_audio_json\n",
                  audio_pack.c_str(),
                  configured_path.c_str());
    return;
  }
  if (mapped_path != nullptr && g_audio.play(mapped_path)) {
    Serial.printf("[MAIN] audio pack=%s path=%s source=pack_map\n", audio_pack.c_str(), mapped_path);
    return;
  }
  if (g_audio.play(kDiagAudioFile)) {
    Serial.printf("[MAIN] audio fallback for pack=%s fallback=%s\n", audio_pack.c_str(), kDiagAudioFile);
    return;
  }
  if (g_audio.playDiagnosticTone()) {
    Serial.printf("[MAIN] audio fallback for pack=%s fallback=builtin_tone\n", audio_pack.c_str());
    return;
  }

  // If audio cannot start (missing/invalid file), unblock scenario transitions.
  Serial.printf("[MAIN] audio fallback failed for pack=%s\n", audio_pack.c_str());
  g_scenario.notifyAudioDone(millis());
}

void handleSerialCommand(const char* command_line, uint32_t now_ms) {
  if (command_line == nullptr || command_line[0] == '\0') {
    return;
  }

  char command[kSerialLineCapacity] = {0};
  std::strncpy(command, command_line, kSerialLineCapacity - 1U);
  char* argument = nullptr;
  for (size_t index = 0; index < kSerialLineCapacity && command[index] != '\0'; ++index) {
    if (command[index] == ' ') {
      command[index] = '\0';
      argument = &command[index + 1U];
      break;
    }
  }
  while (argument != nullptr && *argument == ' ') {
    ++argument;
  }
  if (argument != nullptr && *argument == '\0') {
    argument = nullptr;
  }

  if (std::strcmp(command, "PING") == 0) {
    Serial.println("PONG");
    return;
  }
  if (std::strcmp(command, "HELP") == 0) {
    Serial.println("CMDS PING STATUS BTN_READ NEXT UNLOCK RESET AUDIO_TEST AUDIO_TEST_FS AUDIO_PROFILE <idx> AUDIO_STATUS VOL <0..21> AUDIO_STOP STOP");
    return;
  }
  if (std::strcmp(command, "STATUS") == 0) {
    printRuntimeStatus();
    return;
  }
  if (std::strcmp(command, "BTN_READ") == 0) {
    printButtonRead();
    return;
  }
  if (std::strcmp(command, "NEXT") == 0) {
    g_scenario.notifyButton(5, false, now_ms);
    Serial.println("ACK NEXT");
    return;
  }
  if (std::strcmp(command, "UNLOCK") == 0) {
    g_scenario.notifyUnlock(now_ms);
    Serial.println("ACK UNLOCK");
    return;
  }
  if (std::strcmp(command, "RESET") == 0) {
    g_scenario.reset();
    Serial.println("ACK RESET");
    return;
  }
  if (std::strcmp(command, "AUDIO_TEST") == 0) {
    g_audio.stop();
    const bool ok = g_audio.playDiagnosticTone();
    Serial.printf("ACK AUDIO_TEST %u\n", ok ? 1 : 0);
    return;
  }
  if (std::strcmp(command, "AUDIO_TEST_FS") == 0) {
    g_audio.stop();
    const bool ok = g_audio.play(kDiagAudioFile);
    Serial.printf("ACK AUDIO_TEST_FS %u\n", ok ? 1 : 0);
    return;
  }
  if (std::strcmp(command, "AUDIO_PROFILE") == 0) {
    if (argument == nullptr) {
      Serial.printf("AUDIO_PROFILE current=%u label=%s count=%u\n",
                    g_audio.outputProfile(),
                    g_audio.outputProfileLabel(g_audio.outputProfile()),
                    g_audio.outputProfileCount());
      return;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argument, &end, 10);
    if (end == argument || (end != nullptr && *end != '\0') || parsed > 255UL) {
      Serial.println("ERR AUDIO_PROFILE_ARG");
      return;
    }
    const uint8_t profile = static_cast<uint8_t>(parsed);
    const bool ok = g_audio.setOutputProfile(profile);
    Serial.printf("ACK AUDIO_PROFILE %u %u %s\n",
                  profile,
                  ok ? 1U : 0U,
                  ok ? g_audio.outputProfileLabel(profile) : "invalid");
    return;
  }
  if (std::strcmp(command, "AUDIO_STATUS") == 0) {
    Serial.printf("AUDIO_STATUS playing=%u track=%s profile=%u:%s vol=%u\n",
                  g_audio.isPlaying() ? 1U : 0U,
                  g_audio.currentTrack(),
                  g_audio.outputProfile(),
                  g_audio.outputProfileLabel(g_audio.outputProfile()),
                  g_audio.volume());
    return;
  }
  if (std::strcmp(command, "VOL") == 0) {
    if (argument == nullptr) {
      Serial.printf("VOL %u\n", g_audio.volume());
      return;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(argument, &end, 10);
    if (end == argument || (end != nullptr && *end != '\0') || parsed > 21UL) {
      Serial.println("ERR VOL_ARG");
      return;
    }
    g_audio.setVolume(static_cast<uint8_t>(parsed));
    Serial.printf("ACK VOL %u\n", g_audio.volume());
    return;
  }
  if (std::strcmp(command, "AUDIO_STOP") == 0) {
    g_audio.stop();
    Serial.println("ACK AUDIO_STOP");
    return;
  }
  if (std::strcmp(command, "STOP") == 0) {
    g_audio.stop();
    Serial.println("ACK STOP");
    return;
  }
  Serial.printf("UNKNOWN %s\n", command_line);
}

void pollSerialCommands(uint32_t now_ms) {
  while (Serial.available() > 0) {
    const int raw = Serial.read();
    if (raw < 0) {
      break;
    }
    const char ch = static_cast<char>(raw);
    if (ch == '\r' || ch == '\n') {
      if (g_serial_line_len == 0U) {
        continue;
      }
      g_serial_line[g_serial_line_len] = '\0';
      handleSerialCommand(g_serial_line, now_ms);
      g_serial_line_len = 0U;
      continue;
    }
    if (g_serial_line_len + 1U >= kSerialLineCapacity) {
      g_serial_line_len = 0U;
      Serial.println("ERR CMD_TOO_LONG");
      continue;
    }
    g_serial_line[g_serial_line_len++] = ch;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("[MAIN] Freenove all-in-one boot");

  if (!g_storage.begin()) {
    Serial.println("[MAIN] storage init failed");
  }
  g_storage.ensurePath("/data");
  g_storage.ensurePath("/scenarios");
  g_storage.ensurePath("/scenarios/data");
  g_storage.ensurePath("/screens");
  g_storage.ensurePath("/story");
  g_storage.ensurePath("/story/scenarios");
  g_storage.ensurePath("/story/screens");
  g_storage.ensurePath("/story/audio");
  g_storage.ensurePath("/picture");
  g_storage.ensurePath("/music");
  g_storage.ensurePath("/audio");
  g_storage.ensurePath("/recorder");
  g_storage.ensureDefaultScenarioFile(kDefaultScenarioFile);
  Serial.printf("[MAIN] default scenario checksum=%lu\n",
                static_cast<unsigned long>(g_storage.checksum(kDefaultScenarioFile)));

  g_buttons.begin();
  g_touch.begin();
  g_audio.begin();
  Serial.printf("[MAIN] audio profile=%u:%s count=%u\n",
                g_audio.outputProfile(),
                g_audio.outputProfileLabel(g_audio.outputProfile()),
                g_audio.outputProfileCount());
  g_audio.setAudioDoneCallback(onAudioFinished, nullptr);
  if (kBootDiagnosticTone) {
    g_audio.playDiagnosticTone();
  }

  if (!g_scenario.begin(kDefaultScenarioFile)) {
    Serial.println("[MAIN] scenario init failed");
  }

  g_ui.begin();
  refreshSceneIfNeeded(true);
  startPendingAudioIfAny();
}

void loop() {
  const uint32_t now_ms = millis();
  pollSerialCommands(now_ms);

  ButtonEvent event;
  while (g_buttons.pollEvent(&event)) {
    Serial.printf("[MAIN] button key=%u long=%u\n", event.key, event.long_press ? 1 : 0);
    g_ui.handleButton(event.key, event.long_press);
    g_scenario.notifyButton(event.key, event.long_press, now_ms);
  }

  TouchPoint touch;
  if (g_touch.poll(&touch)) {
    g_ui.handleTouch(touch.x, touch.y, touch.touched);
  } else {
    g_ui.handleTouch(0, 0, false);
  }

  g_audio.update();
  g_scenario.tick(now_ms);
  startPendingAudioIfAny();
  refreshSceneIfNeeded(false);
  g_ui.update();
  delay(5);
}

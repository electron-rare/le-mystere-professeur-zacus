#include <Arduino.h>
#include <LittleFS.h>
#include <unity.h>

#include "core/scenario_def.h"
#include "../src/story/fs/story_fs_manager.h"
#include "resources/screen_scene_registry.h"

namespace {

constexpr StepDef kLegacyAliasSceneSteps[] = {
    {"STEP_BOOT", {"SCENE_LA_DETECT", nullptr, nullptr, 0U, nullptr, 0U}, nullptr, 0U, false},
};

constexpr ScenarioDef kLegacyAliasSceneScenario = {"SCEN_ALIAS", 2U, kLegacyAliasSceneSteps, 1U, "STEP_BOOT"};

constexpr StepDef kUnknownSceneSteps[] = {
    {"STEP_BOOT", {"SCENE_UNKNOWN", nullptr, nullptr, 0U, nullptr, 0U}, nullptr, 0U, false},
};

constexpr ScenarioDef kUnknownSceneScenario = {"SCEN_UNKNOWN", 2U, kUnknownSceneSteps, 1U, "STEP_BOOT"};

}  // namespace

void test_init_creates_cache() {
  StoryFsManager mgr("/story");
  if (!LittleFS.begin(false)) {
    TEST_IGNORE_MESSAGE("LittleFS not mounted");
    return;
  }
  TEST_ASSERT_TRUE(mgr.init());
}

void test_load_scenario_missing() {
  StoryFsManager mgr("/story");
  if (!LittleFS.begin(false)) {
    TEST_IGNORE_MESSAGE("LittleFS not mounted");
    return;
  }
  TEST_ASSERT_TRUE(mgr.init());
  TEST_ASSERT_FALSE(mgr.loadScenario("MISSING_SCENARIO"));
}

void test_validate_checksum_corrupted() {
  StoryFsManager mgr("/story");
  if (!LittleFS.begin(false)) {
    TEST_IGNORE_MESSAGE("LittleFS not mounted");
    return;
  }
  TEST_ASSERT_TRUE(mgr.init());
  TEST_ASSERT_FALSE(mgr.validateChecksum("scenarios", "MISSING_SCENARIO"));
}

void test_story_scene_id_normalization() {
  TEST_ASSERT_EQUAL_STRING("SCENE_LA_DETECTOR", storyNormalizeScreenSceneId("SCENE_LA_DETECTOR"));
  TEST_ASSERT_EQUAL_STRING("SCENE_LA_DETECTOR", storyNormalizeScreenSceneId("SCENE_LA_DETECT"));
  TEST_ASSERT_NULL(storyNormalizeScreenSceneId("SCENE_UNKNOWN"));
}

void test_story_validation_rejects_unknown_scene_id() {
  StoryValidationError out_error{};
  TEST_ASSERT_FALSE(storyValidateScenarioDef(kUnknownSceneScenario, &out_error));
  TEST_ASSERT_EQUAL_STRING("SCREEN_SCENE_ID_UNKNOWN", out_error.code);
  TEST_ASSERT_EQUAL_STRING("SCENE_UNKNOWN", out_error.detail);
}

void test_story_validation_accepts_alias_scene_id() {
  StoryValidationError out_error{};
  TEST_ASSERT_TRUE(storyValidateScenarioDef(kLegacyAliasSceneScenario, &out_error));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_init_creates_cache);
  RUN_TEST(test_load_scenario_missing);
  RUN_TEST(test_validate_checksum_corrupted);
  RUN_TEST(test_story_scene_id_normalization);
  RUN_TEST(test_story_validation_rejects_unknown_scene_id);
  RUN_TEST(test_story_validation_accepts_alias_scene_id);
  UNITY_END();
}

void loop() {}

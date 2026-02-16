#include <Arduino.h>
#include <LittleFS.h>
#include <unity.h>

#include "../src/story/fs/story_fs_manager.h"

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

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_init_creates_cache);
  RUN_TEST(test_load_scenario_missing);
  RUN_TEST(test_validate_checksum_corrupted);
  UNITY_END();
}

void loop() {}

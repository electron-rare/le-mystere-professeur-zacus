#pragma once
#include <Arduino.h>

struct UiScreen {
  String id;
  String description;
};

bool loadUiScreen(const char* filename, UiScreen& outScreen);

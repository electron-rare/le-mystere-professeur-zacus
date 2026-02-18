#!/usr/bin/env bash

# Resolve firmware paths across legacy and migrated layouts.

layout_fw_root() {
  local explicit="${FW_ROOT:-}"
  if [[ -n "$explicit" && -d "$explicit" ]]; then
    printf '%s\n' "$explicit"
    return 0
  fi

  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local candidate
  candidate="$(cd "$script_dir/../.." && pwd)"

  if [[ -d "$candidate/hardware/firmware/esp32_audio/src" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi
  if [[ -d "$candidate/esp32_audio/src" ]]; then
    printf '%s\n' "$candidate"
    return 0
  fi

  printf '%s\n' "$candidate"
}

layout_first_existing() {
  local path
  for path in "$@"; do
    if [[ -e "$path" ]]; then
      printf '%s\n' "$path"
      return 0
    fi
  done
  printf '%s\n' "$1"
  return 1
}

fw_story_src() {
  local fw_root
  fw_root="$(layout_fw_root)"
  layout_first_existing \
    "$fw_root/hardware/libs/story/src" \
    "$fw_root/hardware/firmware/esp32_audio/src/story" \
    "$fw_root/esp32_audio/src/story"
}

fw_ui_oled_src() {
  local fw_root
  fw_root="$(layout_fw_root)"
  layout_first_existing \
    "$fw_root/hardware/firmware/ui/esp8266_oled/src" \
    "$fw_root/ui/esp8266_oled/src"
}

fw_ui_tft_src() {
  local fw_root
  fw_root="$(layout_fw_root)"
  layout_first_existing \
    "$fw_root/hardware/firmware/ui/rp2040_tft/src" \
    "$fw_root/ui/rp2040_tft/src"
}

fw_story_specs_dir() {
  local fw_root
  fw_root="$(layout_fw_root)"
  layout_first_existing \
    "$fw_root/docs/protocols/story_specs/scenarios" \
    "$fw_root/story_generator/story_specs/scenarios"
}

fw_esp32_src_root() {
  local fw_root
  fw_root="$(layout_fw_root)"
  layout_first_existing \
    "$fw_root/hardware/firmware/esp32_audio/src" \
    "$fw_root/esp32_audio/src"
}


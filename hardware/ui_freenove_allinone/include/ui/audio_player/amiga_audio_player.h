#pragma once

#include <Arduino.h>

#include <lvgl.h>

#include "ui/audio_player/audio_player_service.h"

#if defined(USE_AUDIO) && (USE_AUDIO != 0)

#include <FS.h>

namespace ui {
namespace audio {

class AmigaAudioPlayer {
 public:
  struct UiConfig {
    fs::FS* fs = nullptr;
    const char* base_dir = "/music";
    bool start_visible = false;
    bool auto_scan = true;
    bool dim_background = true;
    bool capture_keys_when_visible = false;
  };

  AmigaAudioPlayer();
  ~AmigaAudioPlayer();

  AmigaAudioPlayer(const AmigaAudioPlayer&) = delete;
  AmigaAudioPlayer& operator=(const AmigaAudioPlayer&) = delete;

  bool begin(const UiConfig& ui_cfg, const AudioPlayerService::Config& audio_cfg);
  bool begin();
  void end();

  void show();
  void hide();
  void toggle();
  bool visible() const;

  void tick(uint32_t now_ms);

  AudioPlayerService& service();

 private:
  void createUi();
  void destroyUi();
  void refreshStatusLabel();

  UiConfig ui_cfg_;
  AudioPlayerService svc_;

  lv_obj_t* overlay_ = nullptr;
  lv_obj_t* panel_ = nullptr;
  lv_obj_t* title_label_ = nullptr;
  lv_obj_t* status_label_ = nullptr;

  uint32_t last_ui_update_ms_ = 0U;
  bool inited_ = false;
};

}  // namespace audio
}  // namespace ui

#endif  // USE_AUDIO

#include "ui/audio_player/amiga_audio_player.h"

#if defined(USE_AUDIO) && (USE_AUDIO != 0)

#include <cstdio>
#include <cstring>

#include <LittleFS.h>

namespace ui {
namespace audio {

namespace {

constexpr uint32_t kUiRefreshMs = 120U;

const char* basenameFromPath(const char* path) {
  if (path == nullptr) {
    return "";
  }
  const char* slash = std::strrchr(path, '/');
  return (slash != nullptr) ? (slash + 1) : path;
}

const char* stateText(AudioPlayerService::State state) {
  switch (state) {
    case AudioPlayerService::State::kPlaying:
      return "PLAY";
    case AudioPlayerService::State::kPaused:
      return "PAUSE";
    case AudioPlayerService::State::kError:
      return "ERROR";
    case AudioPlayerService::State::kStopped:
    default:
      return "STOP";
  }
}

}  // namespace

AmigaAudioPlayer::AmigaAudioPlayer() = default;

AmigaAudioPlayer::~AmigaAudioPlayer() {
  end();
}

bool AmigaAudioPlayer::begin(const UiConfig& ui_cfg, const AudioPlayerService::Config& audio_cfg) {
  if (inited_) {
    return true;
  }

  ui_cfg_ = ui_cfg;
  fs::FS* fs = ui_cfg_.fs;

  if (fs == nullptr) {
    if (!LittleFS.begin(true)) {
      Serial.println("[AMP] LittleFS mount failed");
    }
    fs = &LittleFS;
  }

  if (!svc_.begin(fs, ui_cfg_.base_dir, audio_cfg)) {
    Serial.println("[AMP] backend init failed");
    return false;
  }

  if (ui_cfg_.auto_scan) {
    (void)svc_.scanPlaylist();
  }

  createUi();
  if (ui_cfg_.start_visible) {
    show();
  } else {
    hide();
  }

  last_ui_update_ms_ = 0U;
  inited_ = true;
  return true;
}

bool AmigaAudioPlayer::begin() {
  UiConfig ui_cfg{};
  AudioPlayerService::Config audio_cfg{};
  return begin(ui_cfg, audio_cfg);
}

void AmigaAudioPlayer::end() {
  if (!inited_) {
    return;
  }
  hide();
  destroyUi();
  svc_.end();
  inited_ = false;
}

void AmigaAudioPlayer::show() {
  if (overlay_ != nullptr) {
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    refreshStatusLabel();
  }
}

void AmigaAudioPlayer::hide() {
  if (overlay_ != nullptr) {
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
  }
}

void AmigaAudioPlayer::toggle() {
  if (visible()) {
    hide();
  } else {
    show();
  }
}

bool AmigaAudioPlayer::visible() const {
  return (overlay_ != nullptr) && !lv_obj_has_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
}

void AmigaAudioPlayer::tick(uint32_t now_ms) {
  if (!inited_) {
    return;
  }

  if (!svc_.taskMode()) {
    svc_.loopOnce();
  }

  if (!visible()) {
    return;
  }
  if ((now_ms - last_ui_update_ms_) < kUiRefreshMs) {
    return;
  }
  last_ui_update_ms_ = now_ms;
  refreshStatusLabel();
}

AudioPlayerService& AmigaAudioPlayer::service() {
  return svc_;
}

void AmigaAudioPlayer::createUi() {
  if (overlay_ != nullptr) {
    return;
  }

  lv_disp_t* disp = lv_disp_get_default();
  const lv_coord_t w = (disp != nullptr) ? lv_disp_get_hor_res(disp) : 320;
  const lv_coord_t h = (disp != nullptr) ? lv_disp_get_ver_res(disp) : 240;

  overlay_ = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(overlay_);
  lv_obj_set_size(overlay_, w, h);
  lv_obj_center(overlay_);
  lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);

  if (ui_cfg_.dim_background) {
    lv_obj_set_style_bg_color(overlay_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_60, 0);
  } else {
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_TRANSP, 0);
  }

  panel_ = lv_obj_create(overlay_);
  lv_obj_set_size(panel_, (w > 24) ? static_cast<lv_coord_t>(w - 24) : w, 86);
  lv_obj_center(panel_);
  lv_obj_set_style_radius(panel_, 0, 0);
  lv_obj_set_style_bg_color(panel_, lv_color_hex(0x1A1A1A), 0);
  lv_obj_set_style_border_width(panel_, 1, 0);
  lv_obj_set_style_border_color(panel_, lv_color_hex(0xF0F0F0), 0);
  lv_obj_set_style_pad_all(panel_, 4, 0);
  lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);

  title_label_ = lv_label_create(panel_);
  lv_label_set_text(title_label_, "AmigaAMP");
  lv_obj_align(title_label_, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_color(title_label_, lv_color_white(), 0);

  status_label_ = lv_label_create(panel_);
  lv_obj_set_width(status_label_, lv_obj_get_width(panel_) - 8);
  lv_label_set_long_mode(status_label_, LV_LABEL_LONG_CLIP);
  lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 0, 22);
  lv_obj_set_style_text_color(status_label_, lv_color_white(), 0);
  lv_label_set_text(status_label_, "scan...");

  refreshStatusLabel();
}

void AmigaAudioPlayer::destroyUi() {
  if (overlay_ != nullptr) {
    lv_obj_del(overlay_);
  }
  overlay_ = nullptr;
  panel_ = nullptr;
  title_label_ = nullptr;
  status_label_ = nullptr;
}

void AmigaAudioPlayer::refreshStatusLabel() {
  if (title_label_ == nullptr || status_label_ == nullptr) {
    return;
  }

  const AudioPlayerService::Stats stats = svc_.stats();
  const char* path = svc_.currentPath();
  const char* track_name = basenameFromPath((path != nullptr) ? path : "");
  const size_t track_count = svc_.trackCount();
  const size_t index = svc_.currentIndex();

  char title[48] = {0};
  std::snprintf(title, sizeof(title), "AmigaAMP [%s]", stateText(stats.state));
  lv_label_set_text(title_label_, title);

  char status[160] = {0};
  std::snprintf(status,
                sizeof(status),
                "track:%s  %lu/%lus  idx:%u/%u  vol:%u",
                (track_name != nullptr && track_name[0] != '\0') ? track_name : "none",
                static_cast<unsigned long>(stats.position_s),
                static_cast<unsigned long>(stats.duration_s),
                static_cast<unsigned int>(track_count == 0U ? 0U : (index + 1U)),
                static_cast<unsigned int>(track_count),
                static_cast<unsigned int>(svc_.volume()));
  lv_label_set_text(status_label_, status);
}

}  // namespace audio
}  // namespace ui

#endif  // USE_AUDIO

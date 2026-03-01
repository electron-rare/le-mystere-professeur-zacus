#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace ui::effects {

struct SceneGyrophareConfig {
  uint8_t fps = 25U;
  uint16_t speed_deg_per_sec = 180U;
  uint16_t beam_width_deg = 70U;
  const char* message = "ALERTE";
};

class SceneGyrophare {
 public:
  SceneGyrophare() = default;
  ~SceneGyrophare();

  SceneGyrophare(const SceneGyrophare&) = delete;
  SceneGyrophare& operator=(const SceneGyrophare&) = delete;

  bool create(lv_obj_t* parent, int16_t width, int16_t height, const SceneGyrophareConfig& config);
  void destroy();
  bool active() const { return root_ != nullptr; }
  void setMessage(const char* message);
  void setSpeedDegPerSec(uint16_t speed_deg_per_sec);
  void setBeamWidthDeg(uint16_t beam_width_deg);
  lv_obj_t* root() const { return root_; }

 private:
  static void timerCb(lv_timer_t* timer);
  void tick();
  void buildBase();

  lv_obj_t* root_ = nullptr;
  lv_obj_t* canvas_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  lv_color_t* frame_buffer_ = nullptr;
  lv_color_t* base_buffer_ = nullptr;

  int16_t width_ = 0;
  int16_t height_ = 0;
  int16_t center_x_ = 0;
  int16_t center_y_ = 0;
  int16_t radius_outer_ = 0;

  uint32_t started_ms_ = 0U;
  uint16_t speed_a10_per_s_ = 1800U;
  uint16_t beam_width_a10_ = 700U;

  uint16_t color_blue_ = 0U;
  uint16_t color_amber_ = 0U;
};

}  // namespace ui::effects

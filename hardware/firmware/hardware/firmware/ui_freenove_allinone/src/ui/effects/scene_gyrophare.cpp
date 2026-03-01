#include "ui/effects/scene_gyrophare.h"

#include <string.h>

#if defined(ESP32)
#include "esp_heap_caps.h"
#endif

namespace ui::effects {

namespace {

static inline int clampi(int value, int lo, int hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

static inline uint16_t rgb565_from8(int r, int g, int b) {
  r = clampi(r, 0, 255);
  g = clampi(g, 0, 255);
  b = clampi(b, 0, 255);
  const uint16_t r5 = static_cast<uint16_t>(r >> 3);
  const uint16_t g6 = static_cast<uint16_t>(g >> 2);
  const uint16_t b5 = static_cast<uint16_t>(b >> 3);
  return static_cast<uint16_t>((r5 << 11U) | (g6 << 5U) | b5);
}

static void* gyro_alloc(size_t bytes) {
#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
  void* ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ptr != nullptr) {
    return ptr;
  }
  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
  return lv_mem_alloc(bytes);
#endif
}

static void gyro_free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
#if defined(ESP32) && defined(BOARD_HAS_PSRAM)
  heap_caps_free(ptr);
#else
  lv_mem_free(ptr);
#endif
}

static inline void add_pixel565(lv_color_t* dst, uint16_t src565, uint8_t alpha) {
  if (alpha == 0U) {
    return;
  }
  const uint16_t dst565 = dst->full;
  int dr = (dst565 >> 11U) & 31;
  int dg = (dst565 >> 5U) & 63;
  int db = dst565 & 31;

  int sr = (src565 >> 11U) & 31;
  int sg = (src565 >> 5U) & 63;
  int sb = src565 & 31;
  sr = (sr * alpha + 128) >> 8;
  sg = (sg * alpha + 128) >> 8;
  sb = (sb * alpha + 128) >> 8;

  dr = clampi(dr + sr, 0, 31);
  dg = clampi(dg + sg, 0, 63);
  db = clampi(db + sb, 0, 31);
  dst->full = static_cast<uint16_t>((dr << 11U) | (dg << 5U) | db);
}

static void fill_bg(lv_color_t* buffer, int width, int height) {
  const int center_x = width / 2;
  for (int y = 0; y < height; ++y) {
    const int t = (height > 1) ? (y * 255) / (height - 1) : 0;
    int r = (4 * (255 - t)) / 255;
    int g = (6 * (255 - t)) / 255;
    int b = (16 * (255 - t)) / 255;
    if ((y & 1) != 0) {
      r = (r * 220) / 255;
      g = (g * 220) / 255;
      b = (b * 220) / 255;
    }
    for (int x = 0; x < width; ++x) {
      int vx = x - center_x;
      if (vx < 0) {
        vx = -vx;
      }
      const int edge = (center_x > 0) ? (vx * 255) / center_x : 0;
      int rr = r;
      int gg = g;
      int bb = b;
      if (edge > 180) {
        int k = 255 - ((edge - 180) * 120) / 75;
        if (k < 120) {
          k = 120;
        }
        rr = (rr * k) / 255;
        gg = (gg * k) / 255;
        bb = (bb * k) / 255;
      }
      buffer[y * width + x].full = rgb565_from8(rr, gg, bb);
    }
  }
}

static void draw_filled_rect(lv_color_t* buffer,
                             int width,
                             int height,
                             int x0,
                             int y0,
                             int x1,
                             int y1,
                             uint16_t color) {
  if (x0 > x1) {
    const int tmp = x0;
    x0 = x1;
    x1 = tmp;
  }
  if (y0 > y1) {
    const int tmp = y0;
    y0 = y1;
    y1 = tmp;
  }
  x0 = clampi(x0, 0, width - 1);
  x1 = clampi(x1, 0, width - 1);
  y0 = clampi(y0, 0, height - 1);
  y1 = clampi(y1, 0, height - 1);
  for (int y = y0; y <= y1; ++y) {
    lv_color_t* row = &buffer[y * width + x0];
    for (int x = x0; x <= x1; ++x) {
      row[x - x0].full = color;
    }
  }
}

static void draw_circle_shaded(lv_color_t* buffer,
                               int width,
                               int height,
                               int center_x,
                               int center_y,
                               int radius,
                               uint16_t color_center,
                               uint16_t color_edge) {
  const int radius_sq = radius * radius;
  for (int y = center_y - radius; y <= center_y + radius; ++y) {
    if (static_cast<unsigned>(y) >= static_cast<unsigned>(height)) {
      continue;
    }
    const int dy = y - center_y;
    const int dy_sq = dy * dy;
    for (int x = center_x - radius; x <= center_x + radius; ++x) {
      if (static_cast<unsigned>(x) >= static_cast<unsigned>(width)) {
        continue;
      }
      const int dx = x - center_x;
      const int dist_sq = dx * dx + dy_sq;
      if (dist_sq > radius_sq) {
        continue;
      }
      const int t = (dist_sq * 255) / ((radius_sq > 0) ? radius_sq : 1);
      const int inv = 255 - t;
      const int r0 = (color_center >> 11U) & 31;
      const int g0 = (color_center >> 5U) & 63;
      const int b0 = color_center & 31;
      const int r1 = (color_edge >> 11U) & 31;
      const int g1 = (color_edge >> 5U) & 63;
      const int b1 = color_edge & 31;
      int rr = (r0 * inv + r1 * t) / 255;
      int gg = (g0 * inv + g1 * t) / 255;
      int bb = (b0 * inv + b1 * t) / 255;
      if (dx < 0 && dy < 0) {
        rr = clampi(rr + 1, 0, 31);
        gg = clampi(gg + 2, 0, 63);
        bb = clampi(bb + 1, 0, 31);
      }
      buffer[y * width + x].full = static_cast<uint16_t>((rr << 11U) | (gg << 5U) | bb);
    }
  }
}

static void draw_line(lv_color_t* buffer, int width, int height, int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  const int sx = (x0 < x1) ? 1 : -1;
  int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
  const int sy = (y0 < y1) ? 1 : -1;
  int err = (dx > dy ? dx : -dy) / 2;
  while (true) {
    if (static_cast<unsigned>(x0) < static_cast<unsigned>(width) &&
        static_cast<unsigned>(y0) < static_cast<unsigned>(height)) {
      buffer[y0 * width + x0].full = color;
    }
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = err;
    if (e2 > -dx) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dy) {
      err += dx;
      y0 += sy;
    }
  }
}

static void draw_warning_triangle(lv_color_t* buffer, int width, int height, int center_x, int center_y, int tri_w, int tri_h) {
  const int x0 = center_x;
  const int y0 = center_y - tri_h / 2;
  const int x1 = center_x - tri_w / 2;
  const int y1 = center_y + tri_h / 2;
  const int x2 = center_x + tri_w / 2;
  const int y2 = center_y + tri_h / 2;
  const uint16_t fill = rgb565_from8(255, 212, 74);
  const uint16_t edge = rgb565_from8(15, 12, 6);

  const int y_min = clampi(y0, 0, height - 1);
  const int y_max = clampi(y1, 0, height - 1);
  for (int y = y_min; y <= y_max; ++y) {
    int xl = x0 + static_cast<int>((static_cast<int64_t>(x1 - x0) * (y - y0)) / ((y1 - y0) != 0 ? (y1 - y0) : 1));
    int xr = x0 + static_cast<int>((static_cast<int64_t>(x2 - x0) * (y - y0)) / ((y2 - y0) != 0 ? (y2 - y0) : 1));
    if (xl > xr) {
      const int tmp = xl;
      xl = xr;
      xr = tmp;
    }
    xl = clampi(xl, 0, width - 1);
    xr = clampi(xr, 0, width - 1);
    lv_color_t* row = &buffer[y * width + xl];
    for (int x = xl; x <= xr; ++x) {
      row[x - xl].full = fill;
    }
  }

  draw_line(buffer, width, height, x0, y0, x1, y1, edge);
  draw_line(buffer, width, height, x1, y1, x2, y2, edge);
  draw_line(buffer, width, height, x2, y2, x0, y0, edge);

  const int ex_w = clampi(tri_w / 12, 2, 10);
  const int ex_h = clampi(tri_h / 3, 8, 60);
  const int ex_x0 = center_x - ex_w / 2;
  const int ex_y0 = center_y - ex_h / 4;
  draw_filled_rect(buffer, width, height, ex_x0, ex_y0, ex_x0 + ex_w - 1, ex_y0 + ex_h - 1, edge);
  const int dot = clampi(ex_w + 2, 3, 12);
  draw_filled_rect(buffer, width, height, center_x - dot / 2, center_y + tri_h / 4, center_x + dot / 2, center_y + tri_h / 4 + dot, edge);
}

}  // namespace

SceneGyrophare::~SceneGyrophare() {
  destroy();
}

bool SceneGyrophare::create(lv_obj_t* parent, int16_t width, int16_t height, const SceneGyrophareConfig& config) {
  destroy();
  if (parent == nullptr || width <= 0 || height <= 0) {
    return false;
  }

  width_ = width;
  height_ = height;
  center_x_ = width_ / 2;
  center_y_ = (height_ * 60) / 100;
  const int16_t mn = (width_ < height_) ? width_ : height_;
  radius_outer_ = static_cast<int16_t>((mn * 30) / 100);

  setSpeedDegPerSec(config.speed_deg_per_sec);
  setBeamWidthDeg(config.beam_width_deg);
  color_blue_ = rgb565_from8(70, 170, 255);
  color_amber_ = rgb565_from8(255, 165, 60);

  const size_t pixel_count = static_cast<size_t>(width_) * static_cast<size_t>(height_);
  frame_buffer_ = static_cast<lv_color_t*>(gyro_alloc(pixel_count * sizeof(lv_color_t)));
  base_buffer_ = static_cast<lv_color_t*>(gyro_alloc(pixel_count * sizeof(lv_color_t)));
  if (frame_buffer_ == nullptr || base_buffer_ == nullptr) {
    destroy();
    return false;
  }

  root_ = lv_obj_create(parent);
  if (root_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_size(root_, width_, height_);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
#ifdef LV_OBJ_FLAG_CLIP_CHILDREN
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLIP_CHILDREN);
#endif
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);

  canvas_ = lv_canvas_create(root_);
  if (canvas_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_set_pos(canvas_, 0, 0);
  lv_obj_set_size(canvas_, width_, height_);
  lv_canvas_set_buffer(canvas_, frame_buffer_, width_, height_, LV_IMG_CF_TRUE_COLOR);

  label_ = lv_label_create(root_);
  if (label_ != nullptr) {
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(label_, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label_, 2, LV_PART_MAIN);
    lv_obj_set_style_text_opa(label_, LV_OPA_COVER, LV_PART_MAIN);
    setMessage(config.message);
  }

  buildBase();
  memcpy(frame_buffer_, base_buffer_, pixel_count * sizeof(lv_color_t));
  started_ms_ = lv_tick_get();

  uint8_t fps = config.fps;
  if (fps < 12U) {
    fps = 12U;
  } else if (fps > 60U) {
    fps = 60U;
  }
  timer_ = lv_timer_create(timerCb, static_cast<uint32_t>(1000U / fps), this);
  if (timer_ == nullptr) {
    destroy();
    return false;
  }
  lv_obj_move_background(root_);
  return true;
}

void SceneGyrophare::destroy() {
  if (timer_ != nullptr) {
    lv_timer_del(timer_);
    timer_ = nullptr;
  }
  if (root_ != nullptr) {
    lv_obj_del(root_);
    root_ = nullptr;
    canvas_ = nullptr;
    label_ = nullptr;
  }
  gyro_free(frame_buffer_);
  gyro_free(base_buffer_);
  frame_buffer_ = nullptr;
  base_buffer_ = nullptr;
  width_ = 0;
  height_ = 0;
}

void SceneGyrophare::setMessage(const char* message) {
  if (label_ == nullptr) {
    return;
  }
  if (message == nullptr || message[0] == '\0') {
    lv_obj_add_flag(label_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(label_, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(label_, message);
}

void SceneGyrophare::setSpeedDegPerSec(uint16_t speed_deg_per_sec) {
  speed_deg_per_sec = static_cast<uint16_t>(clampi(static_cast<int>(speed_deg_per_sec), 30, 600));
  speed_a10_per_s_ = static_cast<uint16_t>(speed_deg_per_sec * 10U);
}

void SceneGyrophare::setBeamWidthDeg(uint16_t beam_width_deg) {
  beam_width_deg = static_cast<uint16_t>(clampi(static_cast<int>(beam_width_deg), 20, 120));
  beam_width_a10_ = static_cast<uint16_t>(beam_width_deg * 10U);
}

void SceneGyrophare::buildBase() {
  if (base_buffer_ == nullptr) {
    return;
  }
  fill_bg(base_buffer_, width_, height_);

  const int tri_center_x = width_ / 2;
  const int tri_center_y = (height_ * 18) / 100;
  int tri_w = (width_ < height_) ? width_ : height_;
  tri_w = (tri_w * 40) / 100;
  const int tri_h = (tri_w * 9) / 10;
  draw_warning_triangle(base_buffer_, width_, height_, tri_center_x, tri_center_y, tri_w, tri_h);

  const uint16_t dome_center = rgb565_from8(30, 34, 42);
  const uint16_t dome_edge = rgb565_from8(8, 10, 14);
  draw_circle_shaded(base_buffer_, width_, height_, center_x_, center_y_, radius_outer_, dome_center, dome_edge);
  const uint16_t ring = rgb565_from8(60, 64, 74);
  draw_circle_shaded(base_buffer_,
                     width_,
                     height_,
                     center_x_,
                     center_y_,
                     static_cast<int16_t>((radius_outer_ * 83) / 100),
                     ring,
                     dome_edge);
  const int base_w = (radius_outer_ * 14) / 10;
  const int base_h = (radius_outer_ * 28) / 100;
  const int bx0 = center_x_ - base_w / 2;
  const int by0 = center_y_ + (radius_outer_ * 55) / 100;
  draw_filled_rect(base_buffer_, width_, height_, bx0, by0, bx0 + base_w, by0 + base_h, rgb565_from8(28, 28, 32));
  draw_filled_rect(base_buffer_, width_, height_, bx0, by0, bx0 + base_w, by0 + 2, rgb565_from8(52, 52, 58));
}

static void draw_ray(lv_color_t* frame,
                     int width,
                     int height,
                     int x0,
                     int y0,
                     int x1,
                     int y1,
                     uint16_t color,
                     uint8_t intensity) {
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  const int adx = (dx < 0) ? -dx : dx;
  const int ady = (dy < 0) ? -dy : dy;
  const int len = (adx > ady) ? adx : ady;
  if (len <= 0) {
    return;
  }
  int32_t x_fp = x0 << 16;
  int32_t y_fp = y0 << 16;
  const int32_t sx_fp = (static_cast<int32_t>(dx) << 16) / len;
  const int32_t sy_fp = (static_cast<int32_t>(dy) << 16) / len;
  uint16_t alpha_fp = static_cast<uint16_t>(intensity) << 8;
  const uint16_t step_fp = (len > 0) ? static_cast<uint16_t>(alpha_fp / static_cast<uint16_t>(len)) : alpha_fp;
  for (int i = 0; i <= len; ++i) {
    const int x = x_fp >> 16;
    const int y = y_fp >> 16;
    const uint8_t alpha = static_cast<uint8_t>(alpha_fp >> 8);
    if (static_cast<unsigned>(x) < static_cast<unsigned>(width) &&
        static_cast<unsigned>(y) < static_cast<unsigned>(height)) {
      add_pixel565(&frame[y * width + x], color, alpha);
    }
    if ((i & 3) == 0 && alpha_fp > step_fp) {
      alpha_fp = static_cast<uint16_t>(alpha_fp - step_fp);
    }
    x_fp += sx_fp;
    y_fp += sy_fp;
  }
}

static void draw_beam_wedge(lv_color_t* frame,
                            int width,
                            int height,
                            int center_x,
                            int center_y,
                            int radius_outer,
                            uint16_t beam_width_a10,
                            int angle10,
                            uint16_t color,
                            uint8_t alpha) {
  int half_width = static_cast<int>(beam_width_a10 / 2U);
  if (half_width < 10) {
    half_width = 10;
  }
  const int radius = (radius_outer * 18) / 10;
  for (int offset = -half_width; offset <= half_width; offset += 20) {
    int a = angle10 + offset;
    while (a < 0) {
      a += 3600;
    }
    while (a >= 3600) {
      a -= 3600;
    }
    const int edge = half_width - ((offset < 0) ? -offset : offset);
    const int e = (edge * edge) / ((half_width > 0) ? half_width : 1);
    int intensity = (alpha * e) / ((half_width > 0) ? half_width : 1);
    intensity = clampi(intensity, 0, 255);
    const int32_t cs = lv_trigo_cos(a);
    const int32_t sn = lv_trigo_sin(a);
    const int x1 = center_x + static_cast<int>((static_cast<int64_t>(radius) * cs) / LV_TRIGO_SIN_MAX);
    const int y1 = center_y + static_cast<int>((static_cast<int64_t>(radius) * sn) / LV_TRIGO_SIN_MAX);
    draw_ray(frame, width, height, center_x, center_y, x1, y1, color, static_cast<uint8_t>(intensity));
  }
}

static void draw_spot(lv_color_t* frame,
                      int width,
                      int height,
                      int center_x,
                      int center_y,
                      int radius_outer,
                      int angle10,
                      uint16_t color,
                      uint8_t alpha) {
  const int radius = (radius_outer * 55) / 100;
  const int32_t cs = lv_trigo_cos(angle10);
  const int32_t sn = lv_trigo_sin(angle10);
  const int px = center_x + static_cast<int>((static_cast<int64_t>(radius) * cs) / LV_TRIGO_SIN_MAX);
  const int py = center_y + static_cast<int>((static_cast<int64_t>(radius) * sn) / LV_TRIGO_SIN_MAX);
  const int rr = clampi(radius_outer / 10, 3, 10);
  for (int y = py - rr; y <= py + rr; ++y) {
    if (static_cast<unsigned>(y) >= static_cast<unsigned>(height)) {
      continue;
    }
    const int dy = y - py;
    for (int x = px - rr; x <= px + rr; ++x) {
      if (static_cast<unsigned>(x) >= static_cast<unsigned>(width)) {
        continue;
      }
      const int dx = x - px;
      if (dx * dx + dy * dy > rr * rr) {
        continue;
      }
      add_pixel565(&frame[y * width + x], color, alpha);
    }
  }
}

void SceneGyrophare::timerCb(lv_timer_t* timer) {
  if (timer == nullptr || timer->user_data == nullptr) {
    return;
  }
  static_cast<SceneGyrophare*>(timer->user_data)->tick();
}

void SceneGyrophare::tick() {
  if (frame_buffer_ == nullptr || base_buffer_ == nullptr || canvas_ == nullptr) {
    return;
  }
  const uint32_t now = lv_tick_get();
  const uint32_t dt = now - started_ms_;
  const int angle10 = static_cast<int>(((static_cast<uint64_t>(dt) * static_cast<uint64_t>(speed_a10_per_s_)) / 1000ULL) % 3600ULL);
  const uint32_t phase = static_cast<uint32_t>(((static_cast<uint64_t>(now) * 3600ULL) / 850ULL) % 3600ULL);
  const int16_t sine = lv_trigo_sin(static_cast<int>(phase));
  const int pulse = sine + LV_TRIGO_SIN_MAX;
  const int pulse255 = (pulse * 255) / (2 * LV_TRIGO_SIN_MAX);
  const uint8_t base_alpha = static_cast<uint8_t>(clampi(60 + ((pulse255 * 150) >> 8), 0, 255));
  const bool alternate = ((now / 700U) & 1U) != 0U;
  const uint16_t color = alternate ? color_blue_ : color_amber_;

  const size_t pixel_count = static_cast<size_t>(width_) * static_cast<size_t>(height_);
  memcpy(frame_buffer_, base_buffer_, pixel_count * sizeof(lv_color_t));
  constexpr int kTrailCount = 4;
  constexpr int kTrailStep = 120;
  for (int i = 0; i < kTrailCount; ++i) {
    int a = angle10 - (i * kTrailStep);
    while (a < 0) {
      a += 3600;
    }
    const uint8_t alpha = static_cast<uint8_t>((base_alpha * (kTrailCount - i)) / kTrailCount);
    draw_beam_wedge(frame_buffer_,
                    width_,
                    height_,
                    center_x_,
                    center_y_,
                    radius_outer_,
                    beam_width_a10_,
                    a,
                    color,
                    alpha);
  }
  draw_spot(frame_buffer_,
            width_,
            height_,
            center_x_,
            center_y_,
            radius_outer_,
            angle10,
            color,
            static_cast<uint8_t>(clampi(base_alpha + 40, 0, 255)));
  add_pixel565(&frame_buffer_[center_y_ * width_ + center_x_],
               color,
               static_cast<uint8_t>(clampi(base_alpha + 60, 0, 255)));

  if (label_ != nullptr) {
    lv_obj_set_style_text_color(label_, lv_color_hex(alternate ? 0x62B4FF : 0xFFB14A), LV_PART_MAIN);
    lv_obj_set_style_text_opa(label_, static_cast<lv_opa_t>(clampi(120 + (pulse255 / 2), 0, 255)), LV_PART_MAIN);
  }
  lv_obj_invalidate(canvas_);
}

}  // namespace ui::effects

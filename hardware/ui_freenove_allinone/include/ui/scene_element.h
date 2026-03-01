// scene_element.h - light LVGL wrapper for repeated scene operations.
#pragma once

#include <lvgl.h>

class SceneElement {
 public:
  SceneElement() = default;
  explicit SceneElement(lv_obj_t* object) : object_(object) {}

  void bind(lv_obj_t* object) { object_ = object; }
  lv_obj_t* get() const { return object_; }
  bool valid() const { return object_ != nullptr; }

  void hide() const;
  void show() const;
  void clearAnimations() const;
  void setOpa(lv_opa_t opa) const;
  void setSize(lv_coord_t width, lv_coord_t height) const;
  void align(lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) const;

  static void initCircle(lv_obj_t* object,
                         lv_color_t bg_color,
                         lv_opa_t bg_opa,
                         lv_color_t border_color,
                         uint8_t border_width,
                         lv_opa_t border_opa);

 private:
  lv_obj_t* object_ = nullptr;
};

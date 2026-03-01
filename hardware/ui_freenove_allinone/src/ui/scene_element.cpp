// scene_element.cpp - light LVGL wrapper for repeated scene operations.
#include "ui/scene_element.h"

void SceneElement::hide() const {
  if (object_ == nullptr) {
    return;
  }
  lv_obj_add_flag(object_, LV_OBJ_FLAG_HIDDEN);
}

void SceneElement::show() const {
  if (object_ == nullptr) {
    return;
  }
  lv_obj_clear_flag(object_, LV_OBJ_FLAG_HIDDEN);
}

void SceneElement::clearAnimations() const {
  if (object_ == nullptr) {
    return;
  }
  lv_anim_del(object_, nullptr);
}

void SceneElement::setOpa(lv_opa_t opa) const {
  if (object_ == nullptr) {
    return;
  }
  lv_obj_set_style_opa(object_, opa, LV_PART_MAIN);
}

void SceneElement::setSize(lv_coord_t width, lv_coord_t height) const {
  if (object_ == nullptr) {
    return;
  }
  lv_obj_set_size(object_, width, height);
}

void SceneElement::align(lv_align_t align, lv_coord_t x_ofs, lv_coord_t y_ofs) const {
  if (object_ == nullptr) {
    return;
  }
  lv_obj_align(object_, align, x_ofs, y_ofs);
}

void SceneElement::initCircle(lv_obj_t* object,
                              lv_color_t bg_color,
                              lv_opa_t bg_opa,
                              lv_color_t border_color,
                              uint8_t border_width,
                              lv_opa_t border_opa) {
  if (object == nullptr) {
    return;
  }
  lv_obj_remove_style_all(object);
  lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(object, bg_color, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(object, bg_opa, LV_PART_MAIN);
  lv_obj_set_style_border_color(object, border_color, LV_PART_MAIN);
  lv_obj_set_style_border_width(object, border_width, LV_PART_MAIN);
  lv_obj_set_style_border_opa(object, border_opa, LV_PART_MAIN);
}

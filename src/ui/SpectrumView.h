#pragma once
#include <lvgl.h>
#include <cstdint>
#include "ui/MusicMotion.h"

namespace aurageek { namespace ui {
// Persistent RGB565 raster: no per-frame LVGL object creation or heap allocation.
class SpectrumView {
 public:
  enum class Style : unsigned { Reflection, Ring, Count };
  bool attach(lv_obj_t* parent);
  void detach() { image_=nullptr; }
  void update(const float* bands, bool active, uint32_t now);
  void setStyle(Style style) { style_=style;lastFrame_=0;fullRedraw_=true; }
  Style style() const {return style_;}
  uint32_t renderMs() const {return renderMs_;}
  void pulse() {pulse_=1.f;}
 private:
  void pixel(int x,int y,uint32_t color,unsigned alpha=255);
  void light(float x,float y,uint32_t color,unsigned alpha);
  void stroke(int x0,int y0,int x1,int y1,uint32_t color,unsigned alpha=255,int width=1);
  void reflection(float time,float energy,bool active);
  void ring(float time,float energy,bool active);
  lv_obj_t* image_=nullptr;
  uint16_t *frame_=nullptr,*background_=nullptr;
  lv_image_dsc_t descriptor_{};
  uint32_t palette_[320]{};
  uint8_t fade_[102]{};
  float levels_[104]{},peak_[48]{},pulse_=0.f;
  uint32_t lastFrame_=0;
  uint32_t renderMs_=0;
  Style style_=Style::Reflection;
  MusicMotion motion_;
  float interpolationLeft_[104]{},interpolationRight_[104]{};
  float circleX_[97]{},circleY_[97]{};
  bool fullRedraw_=true;
};
}}

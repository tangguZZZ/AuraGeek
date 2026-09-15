#pragma once
#include <lvgl.h>
namespace aurageek::ui {
// Index order is shared with tools/prepare_voice_model.py; null until mapped.
const lv_image_dsc_t* agentAsset(unsigned index);
#ifdef ARDUINO
bool loadAgentAssets();
#endif
}

#ifndef LV_CONF_H
#define LV_CONF_H

// Active project-owned LVGL 9 configuration. Unlike lv_conf_template.h, this
// file has no disabled `#if 0` wrapper. Options omitted here use LVGL defaults.
#define LV_COLOR_DEPTH 16
#define LV_USE_GIF 1
#define LV_MEM_SIZE (1024 * 1024)
#define LV_USE_SNAPSHOT 1
#define LV_DEF_REFR_PERIOD 16
#define LV_MEM_POOL_INCLUDE "config/LvglMemory.h"
#define LV_MEM_POOL_ALLOC aurageek_lvgl_pool_alloc

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

#define LV_FONT_MONTSERRAT_8 1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// AuraGeek owns the TFT flush port in DisplayDriver instead of enabling
// LVGL's optional built-in TFT_eSPI wrapper.
#define LV_USE_TFT_ESPI 0

#endif  // LV_CONF_H

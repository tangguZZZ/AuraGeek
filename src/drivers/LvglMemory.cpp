#include "config/LvglMemory.h"
#include <esp_heap_caps.h>
extern "C" void* aurageek_lvgl_pool_alloc(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

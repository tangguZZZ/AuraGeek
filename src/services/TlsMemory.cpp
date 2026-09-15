#include <esp_heap_caps.h>
#include <cstddef>
#include <cstdint>

// Arduino's prebuilt IDF enables internal-only mbedTLS allocation. TLS runs
// in task context with caches enabled; place its buffers in PSRAM first.
// esp_mbedtls_mem_free uses heap_caps_free and accepts either heap.
extern "C" void* __wrap_esp_mbedtls_mem_calloc(size_t count, size_t size) {
  if (!count || !size || count > SIZE_MAX / size) return nullptr;
  void* memory = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return memory ? memory : heap_caps_calloc(count, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

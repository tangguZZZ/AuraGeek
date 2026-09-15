#pragma once
#include <cstdint>
#include <cstddef>

namespace aurageek::drivers {
// All calls belong to the voice-audio task, never the GUI or USB callback.
class WakeWord {
 public:
  bool begin(bool retry=false);
  bool feed(int16_t* samples, size_t count);
  void reset();
  bool ready() const { return data_!=nullptr; }
 private:
  const void* api_=nullptr;
  void* data_=nullptr;
  int16_t* buffer_=nullptr;
  size_t chunk_=0, used_=0;
};
}

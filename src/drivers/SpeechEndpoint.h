#pragma once
#include <cstdint>
namespace aurageek::drivers {
// Frames are 30 ms. Require sustained speech and retain pauses within a sentence.
class SpeechEndpoint {
 public:
  explicit SpeechEndpoint(uint32_t silenceMs=900,uint32_t noSpeechMs=8000):silenceMs_(silenceMs),noSpeechMs_(noSpeechMs){}
  void feed(bool speech) {
    total_+=30;
    if(speech){speechMs_+=30;quietMs_=0;}else quietMs_+=30;
  }
  bool heardSpeech() const {return speechMs_>=240;}
  bool complete() const {return heardSpeech() && quietMs_>=silenceMs_;}
  bool noSpeechTimeout() const {return !heardSpeech() && total_>=noSpeechMs_;}
 private:
  uint32_t silenceMs_,noSpeechMs_;
  uint32_t total_=0,speechMs_=0,quietMs_=0;
};
}

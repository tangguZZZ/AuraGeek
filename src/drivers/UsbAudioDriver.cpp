#include "drivers/UsbAudioDriver.h"
#include <USB.h>
#include <USBAudioCard.h>
#include <cstring>
#if !CONFIG_TINYUSB_AUDIO_ENABLED
#error AuraGeek USB spectrum requires Arduino-ESP32 with CONFIG_TINYUSB_AUDIO_ENABLED
#endif
namespace aurageek { namespace drivers {
namespace { USBAudioCard audio(48000,UAC_BPS_16,UAC_SPK_STEREO,UAC_MIC_NONE); }
UsbAudioDriver* UsbAudioDriver::instance_=nullptr;
bool UsbAudioDriver::begin(){
  instance_=this;audio.onData(receive);
  USB.productName("AuraGeek Spectrum");USB.manufacturerName("AuraGeek");
  if(!audio.begin())return false;
  bool ok=USB.begin();Serial.printf("[UAC] %s: 48000 Hz / S16LE / stereo / playback only\n",ok?"ready":"failed");return ok;
}
void UsbAudioDriver::receive(void* data,uint16_t bytes){
  auto* s=instance_;if(!s||bytes<4)return;
  audio.applyVolume(data,bytes);
  const auto* pcm=static_cast<const int16_t*>(data);
  portENTER_CRITICAL(&s->lock_);
  for(unsigned i=0;i+1<bytes/2;i+=2){
    unsigned next=(s->write_+1)%2048;
    if(next==s->read_){s->read_=(s->read_+1)%2048;++s->overruns_;}
    // Stereo mean; promote before addition to avoid int16 overflow.
    s->ring_[s->write_]=int16_t((int32_t(pcm[i])+pcm[i+1])/2);s->write_=next;
  }
  s->lastPcm_=millis();++s->packets_;portEXIT_CRITICAL(&s->lock_);
}
bool UsbAudioDriver::active()const{uint32_t last=lastPcm_.load();return last&&millis()-last<500;}
void UsbAudioDriver::process(){
  uint32_t now=millis();if(now-lastFft_<25)return;lastFft_=now;
  int16_t frame[512]{};bool ready=false;
  portENTER_CRITICAL(&lock_);unsigned count=(write_+2048-read_)%2048;
  if(count>=512){read_=(write_+2048-512)%2048;for(auto& v:frame){v=ring_[read_];read_=(read_+1)%2048;}ready=true;}
  uint32_t packets=packets_,overruns=overruns_;portEXIT_CRITICAL(&lock_);
  if(ready||!active())fft_.process(frame);
  if(now-lastLog_>=2000){lastLog_=now;float peak=0;unsigned band=0;for(unsigned i=0;i<48;++i)if(fft_.bands()[i]>peak){peak=fft_.bands()[i];band=i;}Serial.printf("[UAC] pcm_packets=%lu overwritten_samples=%lu active=%d fft_peak=%.3f band=%u peak_hz=%.1f rate=%lu bits=%u\n",(unsigned long)packets,(unsigned long)overruns,active(),peak,band,fft_.peakHz(),(unsigned long)audio.sampleRate(),unsigned(audio.bitsPerSample()));}
}
}}

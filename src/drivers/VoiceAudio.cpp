#include "drivers/VoiceAudio.h"
#include "drivers/AudioLevel.h"
#include "board/BoardPins.h"
#include <esp_heap_caps.h>
#include <cmath>
#include <algorithm>

namespace aurageek::drivers {
namespace {
constexpr unsigned kRate = 16000, kBlock = 256, kRecordSamples = kRate * 3;
}

bool VoiceAudio::begin() {
  pinMode(board::kSpeakerEnable, OUTPUT);
  digitalWrite(board::kSpeakerEnable, LOW);
  commands_ = xQueueCreate(1, sizeof(Command));
  if (!commands_) return false;
  if (xTaskCreate(taskEntry, "voice-audio", 12288, this, 3, nullptr) != pdPASS) {
    vQueueDelete(commands_); commands_ = nullptr; return false;
  }
  return true;
}

bool VoiceAudio::request(Command command) {
  bool expected = false;
  if (!ready_.load() || !busy_.compare_exchange_strong(expected, true)) return false;
  cancelled_.store(false);
  if (xQueueSend(commands_, &command, 0) != pdTRUE) { busy_ = false; return false; }
  return true;
}

void VoiceAudio::taskEntry(void* self) {
  static_cast<VoiceAudio*>(self)->run();
  vTaskDelete(nullptr);
}

bool VoiceAudio::initialize() {
  recording_ = static_cast<int16_t*>(heap_caps_malloc(kRecordSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  usbRing_ = static_cast<int16_t*>(heap_caps_malloc(4096 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!recording_ || !usbRing_) return false;
  i2s_chan_config_t tc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  tc.dma_desc_num = 4; tc.dma_frame_num = kBlock;
  tc.auto_clear = true;
  if (i2s_new_channel(&tc, &tx_, nullptr) != ESP_OK) return false;
  i2s_std_config_t txConfig{};
  txConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kRate);
  txConfig.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  txConfig.gpio_cfg = {.mclk=I2S_GPIO_UNUSED, .bclk=gpio_num_t(board::kSpeakerBclk), .ws=gpio_num_t(board::kSpeakerLrc), .dout=gpio_num_t(board::kSpeakerData), .din=I2S_GPIO_UNUSED, .invert_flags={}};
  return i2s_channel_init_std_mode(tx_, &txConfig) == ESP_OK;
}

bool VoiceAudio::initializeMic() {
  i2s_chan_config_t rc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  rc.dma_desc_num = 3; rc.dma_frame_num = kBlock;
  if (i2s_new_channel(&rc, nullptr, &rx_) != ESP_OK) return false;
  i2s_std_config_t rxConfig{};
  rxConfig.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kRate);
  // INMP441 requires 64 clocks/frame, including the unused right slot.
  rxConfig.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  rxConfig.gpio_cfg = {.mclk=I2S_GPIO_UNUSED, .bclk=gpio_num_t(board::kMicSck), .ws=gpio_num_t(board::kMicWs), .dout=I2S_GPIO_UNUSED, .din=gpio_num_t(board::kMicData), .invert_flags={}};
  if (i2s_channel_init_std_mode(rx_, &rxConfig) == ESP_OK) return true;
  i2s_del_channel(rx_); rx_=nullptr; return false;
}

void VoiceAudio::cleanup() {
  digitalWrite(board::kSpeakerEnable, LOW);
  if (rx_) { if (rxEnabled_) i2s_channel_disable(rx_); i2s_del_channel(rx_); rx_ = nullptr; }
  if (tx_) { if (txEnabled_) i2s_channel_disable(tx_); i2s_del_channel(tx_); tx_ = nullptr; }
  heap_caps_free(recording_); recording_ = nullptr;
  heap_caps_free(usbRing_); usbRing_=nullptr;
}

void VoiceAudio::run() {
  if (!initialize()) { ++errors_; cleanup(); Serial.println("[VOICE] I2S initialization FAILED; amp disabled"); return; }
  ready_ = true;
  Serial.println("[VOICE] ready: INMP441 RX0 WS4/SCK5/SD6, MAX98357A TX1 BCLK7/LRC15/DIN16/EN17; 16k mono; amp muted");
  for (;;) {
    Command command;
    if (xQueueReceive(commands_, &command, 0) != pdTRUE) { processUsb(); processWake(); vTaskDelay(1); continue; }
    releaseMic();wake_.reset();wakePending_=false;
    silence(); usbPlaying_ = false; clearUsb();
    const bool isChat=command==Command::Chat || command==Command::WakeChat;
    bool ok = isChat ? chat(command==Command::WakeChat) : (command == Command::Record ? record() : play(command == Command::Tone));
    if(isChat)chatState_=ok||cancelled_ ? ChatState::Idle : ChatState::Error;
    if (!ok && !cancelled_) ++errors_;
    Serial.printf("[VOICE] command=%u result=%s\n", unsigned(command), cancelled_ ? "cancelled" : (ok ? "OK" : "FAILED"));
    busy_ = false;
    wakeResumeAt_=millis()+1500;
  }
}

void VoiceAudio::releaseMic(){
  if(rx_){if(rxEnabled_)i2s_channel_disable(rx_);rxEnabled_=false;i2s_del_channel(rx_);rx_=nullptr;}
}

void VoiceAudio::processWake(){
  uint32_t lastUsb;
  portENTER_CRITICAL(&usbLock_);lastUsb=lastUsbMs_;portEXIT_CRITICAL(&usbLock_);
  // No acoustic wake while playing USB music: until AEC is validated it could wake itself.
  if(!wakeEnabled_ || busy_ || wakePending_ || usbPlaying_ || (lastUsb && millis()-lastUsb<1000) || millis()<5000 || int32_t(millis()-wakeResumeAt_)<0){
    if(rx_){releaseMic();wake_.reset();}return;
  }
  const bool retry=wakeRetry_.exchange(false);
  if(!wakeAttempted_ || (retry && !wakeReady_)){wakeAttempted_=true;wakeReady_=wake_.begin(retry);if(!wakeReady_)Serial.println("[WAKE] unavailable; manual chat retained");}
  if(!wake_.ready())return;
  if(!rx_){
    if(!initializeMic() || i2s_channel_enable(rx_)!=ESP_OK){releaseMic();wakeResumeAt_=millis()+5000;return;}
    rxEnabled_=true;wakeDc_=0;
    int32_t discard[512];size_t bytes;
    for(unsigned i=0;i<7;++i)i2s_channel_read(rx_,discard,sizeof(discard),&bytes,50);
    wake_.reset();Serial.println("[WAKE] listening locally; no idle audio upload");
  }
  int32_t raw[512];int16_t pcm[256];size_t bytes=0;
  if(i2s_channel_read(rx_,raw,sizeof(raw),&bytes,50)!=ESP_OK || bytes!=sizeof(raw)){releaseMic();wakeResumeAt_=millis()+1000;return;}
  for(unsigned i=0;i<256;++i){float value=float(raw[2*i]>>16);wakeDc_+=0.004f*(value-wakeDc_);pcm[i]=int16_t(std::clamp<int32_t>(int32_t(value-wakeDc_),-32768,32767));}
  if(wake_.feed(pcm,256)){releaseMic();wakePending_=true;wakeResumeAt_=millis()+2500;}
}

bool VoiceAudio::record() {
  recorded_ = 0; samples_ = peak_ = rms_ = clipped_ = 0;
  if (!initializeMic()) return false;
  if (i2s_channel_enable(rx_) != ESP_OK) { i2s_del_channel(rx_); rx_=nullptr; return false; }
  rxEnabled_ = true;
  int32_t raw[kBlock * 2];
  uint64_t squares = 0;
  uint32_t peak = 0, clips = 0;
  // Discard startup transient. Drain through normal DMA reads, not GUI delays.
  unsigned discard = kRate / 10;
  const uint32_t started = millis();
  Serial.println("[VOICE] recording 3 seconds locally (not uploaded)");
  bool ok = true;
  float dc = 0;
  while (recorded_ < kRecordSamples && !cancelled_) {
    size_t bytes = 0;
    if (millis() - started > 5000 || i2s_channel_read(rx_, raw, sizeof(raw), &bytes, 100) != ESP_OK || bytes % 8 || !bytes) { ok = false; break; }
    for (size_t i = 0; i < bytes / sizeof(int32_t) && recorded_ < kRecordSamples; i += 2) {
      if (discard) { --discard; continue; }
      // Left selected by L/R=GND; 24-bit sample is left-aligned in 32 bits.
      const float value = float(raw[i] >> 16);
      dc += 0.004f * (value - dc);
      const int32_t sample = std::clamp<int32_t>(int32_t(value - dc), -32768, 32767);
      recording_[recorded_++] = int16_t(sample);
      peak = std::max(peak, uint32_t(std::abs(sample)));
      squares += int64_t(sample) * sample;
      if (sample >= 32760 || sample <= -32760) ++clips;
    }
  }
  i2s_channel_disable(rx_); rxEnabled_ = false;
  i2s_del_channel(rx_); rx_=nullptr;
  if (!ok || cancelled_) recorded_ = 0;
  samples_ = recorded_; peak_ = peak; clipped_ = clips;
  rms_ = recorded_ ? uint32_t(std::sqrt(double(squares) / recorded_)) : 0;
  printStatus();
  return ok;
}

bool VoiceAudio::output(const int16_t* mono, size_t count) {
  int16_t stereo[kBlock * 2];
  if (count > kBlock) return false;
  const unsigned volume=volumePercent_.load();
  for (size_t i = 0; i < count; ++i) {
    // Slew gain to avoid a discontinuity when changing level or unmuting.
    stereo[i*2] = stereo[i*2+1] = applyVolumeSample(mono[i],gainQ15_,volume);
  }
  size_t done = 0, total = count * 4;
  while (done < total && !cancelled_) {
    size_t written = 0;
    if (i2s_channel_write(tx_, reinterpret_cast<uint8_t*>(stereo) + done, total - done, &written, 100) != ESP_OK || !written) return false;
    done += written;
  }
  return done == total;
}

void VoiceAudio::silence() {
  // Never leave the bridge enabled while stopping clocks.
  digitalWrite(board::kSpeakerEnable, LOW);
  gainQ15_=0;
  if (txEnabled_) { i2s_channel_disable(tx_); txEnabled_ = false; }
}

bool VoiceAudio::play(bool tone) {
  if (!tone && !recorded_) { Serial.println("[VOICE] no valid recording"); return false; }
  if (!setOutputRate(kRate)) return false;
  if (i2s_channel_enable(tx_) != ESP_OK) return false;
  txEnabled_ = true;
  int16_t block[kBlock]{};
  bool ok = true;
  // Fill the DMA queue with zeros before enabling the power amplifier.
  for (unsigned i=0; i<7 && ok; ++i) ok = output(block, kBlock);
  if (ok && !cancelled_) digitalWrite(board::kSpeakerEnable, HIGH);
  const size_t total = tone ? kRate / 2 : recorded_;
  for (size_t pos=0; pos<total && ok && !cancelled_; pos+=kBlock) {
    const size_t count = std::min(size_t(kBlock), total-pos);
    for (size_t i=0; i<count; ++i) {
      const size_t t = pos+i;
      const float fade = std::min(1.0f, float(std::min(t, total-1-t))/320.0f);
      block[i] = int16_t(fade * (tone ? 8192.0f * std::sin(6.2831853f * 1000.0f * t / kRate) : recording_[t]));
    }
    ok = output(block, count);
  }
  std::fill(std::begin(block), std::end(block), 0);
  for (unsigned i=0; i<7 && ok && !cancelled_; ++i) ok = output(block, kBlock);
  silence();
  return ok;
}

void VoiceAudio::printStatus() const {
  Serial.printf("[VOICE] ready=%u busy=%u samples=%lu peak=%lu rms=%lu clipped=%lu errors=%lu volume=%u%% gain_q15=%d\n", unsigned(ready_.load()), unsigned(busy_.load()), (unsigned long)samples_.load(), (unsigned long)peak_.load(), (unsigned long)rms_.load(), (unsigned long)clipped_.load(), (unsigned long)errors_.load(), volume(),volumeGainQ15(volume()));
  Serial.printf("[VOICE USB] enabled=%u played=%lu dropped=%lu underrun_blocks=%lu rate=48000 mono_mix\n", unsigned(usbEnabled_.load()), (unsigned long)usbPlayed_.load(), (unsigned long)usbDropped_.load(), (unsigned long)usbUnderruns_.load());
  Serial.printf("[AI] state=%u (0=idle 1=connecting 2=listening 3=thinking 4=speaking 5=error)\n",unsigned(chatState_.load()));
  Serial.printf("[WAKE] enabled=%u ready=%u pending=%u phrase=ni hao xiao chen\n",unsigned(wakeEnabled_.load()),unsigned(wakeReady_.load()),unsigned(wakePending_.load()));
}

bool VoiceAudio::toggleChat(bool fromWake) {
  if(fromWake && busy_)return false;
  if(chatState_==ChatState::Listening) { finishListening_=true; return true; }
  if(busy_) { cancelChat(); return false; }
  finishListening_=false; emotion_=0;chatState_=ChatState::Connecting;
  if(request(fromWake?Command::WakeChat:Command::Chat))return true;
  chatState_=ChatState::Error; return false;
}

bool VoiceAudio::setOutputRate(unsigned rate) {
  silence();
  i2s_std_clk_config_t clock = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  return i2s_channel_reconfig_std_clock(tx_, &clock) == ESP_OK;
}

void VoiceAudio::acceptUsb(const int16_t* stereo, size_t frames) {
  if (!ready_ || busy_ || !usbEnabled_) return;
  portENTER_CRITICAL(&usbLock_);
  for (size_t i=0; i<frames; ++i) {
    unsigned next=(usbWrite_+1)%4096;
    if (next==usbRead_) { ++usbDropped_; continue; }
    usbRing_[usbWrite_] = int16_t((int32_t(stereo[i*2])+stereo[i*2+1])/2);
    usbWrite_=next;
  }
  lastUsbMs_=millis();
  portEXIT_CRITICAL(&usbLock_);
}

void VoiceAudio::clearUsb() {
  portENTER_CRITICAL(&usbLock_);
  usbRead_=usbWrite_; lastUsbMs_=0;
  portEXIT_CRITICAL(&usbLock_);
}

void VoiceAudio::processUsb() {
  if (!usbEnabled_) { if (usbPlaying_) silence(); usbPlaying_=false; clearUsb(); return; }
  // cancel applies to a diagnostic operation, not the subsequent USB stream.
  if (!busy_) cancelled_=false;
  int16_t block[kBlock]{};
  unsigned count;
  uint32_t last;
  portENTER_CRITICAL(&usbLock_);
  count=(usbWrite_+4096-usbRead_)%4096; last=lastUsbMs_;
  portEXIT_CRITICAL(&usbLock_);
  if (!usbPlaying_) {
    if (count<2048) return;
    if (!setOutputRate(48000) || i2s_channel_enable(tx_) != ESP_OK) { ++errors_; usbEnabled_=false; return; }
    txEnabled_=true;
    bool ok=true;
    for (unsigned i=0; i<7 && ok; ++i) ok=output(block,kBlock);
    if (!ok || !usbEnabled_) { silence(); return; }
    digitalWrite(board::kSpeakerEnable,HIGH); usbPlaying_=true;
    usbRefilling_=false; gainQ15_=0;
  }
  if (millis()-last>300) { silence(); usbPlaying_=false; clearUsb(); return; }
  if (usbRefilling_) {
    if (count<2048) {
      if (!output(block,kBlock)) { silence(); usbPlaying_=false; }
      return;
    }
    usbRefilling_=false; gainQ15_=0;
  }
  unsigned taken=0;
  portENTER_CRITICAL(&usbLock_);
  while (taken<kBlock && usbRead_!=usbWrite_) { block[taken++]=usbRing_[usbRead_]; usbRead_=(usbRead_+1)%4096; }
  portEXIT_CRITICAL(&usbLock_);
  if (taken<kBlock) {
    ++usbUnderruns_; usbRefilling_=true;
    // A short fade is less abrupt than an immediate jump from PCM to zero.
    const int lastSample=taken ? block[taken-1] : 0;
    const unsigned fade=std::min(unsigned(kBlock)-taken,32U);
    for(unsigned i=0;i<fade;++i)block[taken+i]=int16_t(lastSample*int(fade-1-i)/int(fade));
  }
  // Keep the amplifier enabled throughout an active USB stream, including
  // digital silence. No energy-based gating or automatic gain adjustment.
  if (!output(block,kBlock)) { silence(); usbPlaying_=false; if (!cancelled_) ++errors_; }
  else usbPlayed_+=taken;
}
}

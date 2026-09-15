#include "drivers/WakeWord.h"
#include "ui/AgentAssets.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_mn_models.h>
#include <esp_mn_speech_commands.h>
#include <model_path.h>
#include <algorithm>
#include <cstring>

// Pinned Arduino ESP-SR 2.4.6 exports this interface. Selecting only MultiNet7
// avoids linking the unused MultiNet5/6 engines through the general dispatcher.
extern "C" const esp_mn_iface_t esp_sr_multinet7_quantized;
// The bundled Arduino ESP-SR was compiled with English G2P enabled. This
// firmware loads only mn7_cn: preserve its pinyin tokens, including commands
// loaded internally from the FST resource during create().
extern "C" esp_err_t __wrap_esp_mn_commands_add(int id,const char* pinyin) {
  return esp_mn_commands_phoneme_add(id,pinyin,pinyin);
}
namespace aurageek::drivers {
bool WakeWord::begin(bool retry) {
  auto* models=esp_srmodel_init("model");
  if(!models)return false;
  Serial.printf("[AI FACE] mapped original GIF resources: %s\n",ui::loadAgentAssets()?"OK":"FAILED");
  // The prebuilt engine assumes FST resources exist and otherwise enters a filesystem fallback.
  char* fst=esp_srmodel_filter(models,"fst",nullptr);
  if(!fst){Serial.println("[WAKE] missing FST resource; refusing unsafe model initialization");return false;}
  Preferences guard;
  if(!guard.begin("aura-ai",false))return false;
  if(guard.getBool("wake-guard",false) && !retry){Serial.println("[WAKE] previous initialization failed; protected manual mode");return false;}
  if(guard.putBool("wake-guard",true)!=1)return false;
  char* name=esp_srmodel_filter(models,ESP_MN_PREFIX,ESP_MN_CHINESE);
  if(!name || strcmp(name,"mn7_cn"))return false;
  const auto* api=&esp_sr_multinet7_quantized;
  // Model/FST generic allocations need not consume scarce internal SRAM.
  // Explicit DMA/internal-capability allocations are unaffected; restore the
  // Arduino policy on every normal return from this initialization scope.
  struct ModelHeapPolicy {
    ModelHeapPolicy(){heap_caps_malloc_extmem_enable(0);}
    ~ModelHeapPolicy(){heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);}
  } modelHeapPolicy;
  printf("[WAKE] initialize: internal=%u psram=%u\n",unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  fflush(stdout);
  auto* data=api->create(name,3000);
  Serial.printf("[WAKE] model created=%u internal=%u psram=%u\n",unsigned(data!=nullptr),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  guard.putBool("wake-guard",false);guard.end();
  if(!data)return false;
  const int chunk=api->get_samp_chunksize(data);
  if(chunk<=0 || chunk>2048 || api->get_samp_rate(data)!=16000){api->destroy(data);return false;}
  auto* buffer=static_cast<int16_t*>(heap_caps_malloc(chunk*sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!buffer){api->destroy(data);return false;}
  // MultiNet creates the command list internally. Never fall back to an English model.
  esp_mn_commands_clear();
  if(esp_mn_commands_add(1,"ni hao xiao chen")!=ESP_OK || esp_mn_commands_update()!=nullptr){heap_caps_free(buffer);api->destroy(data);return false;}
  api->set_det_threshold(data,0.35f);
  api->print_active_speech_commands(data);
  api_=api;data_=data;buffer_=buffer;chunk_=chunk;
  Serial.printf("[WAKE] ready: ni hao xiao chen; model=%s chunk=%d threshold=0.35\n",name,chunk);
  return true;
}
void WakeWord::reset(){used_=0;if(data_)static_cast<const esp_mn_iface_t*>(api_)->clean(static_cast<model_iface_data_t*>(data_));}
bool WakeWord::feed(int16_t* samples,size_t count){
  if(!data_)return false;
  auto* api=static_cast<const esp_mn_iface_t*>(api_);auto* data=static_cast<model_iface_data_t*>(data_);
  while(count){
    size_t n=std::min(count,chunk_-used_);memcpy(buffer_+used_,samples,n*sizeof(int16_t));used_+=n;samples+=n;count-=n;
    if(used_<chunk_)continue;
    used_=0;auto state=api->detect(data,buffer_);
    if(state==ESP_MN_STATE_DETECTED){
      auto* result=api->get_results(data);
      bool detected=result && result->num>0 && result->command_id[0]==1 && result->prob[0]>=0.35f;
      if(detected)Serial.printf("[WAKE] detected ni hao xiao chen, confidence=%.3f\n",result->prob[0]);
      api->clean(data);if(detected)return true;
    } else if(state==ESP_MN_STATE_TIMEOUT)api->clean(data);
  }
  return false;
}
}

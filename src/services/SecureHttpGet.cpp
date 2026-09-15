#include "services/SecureHttpGet.h"
#include "services/HttpBodyReader.h"
#include "services/StockRequestPolicy.h"
#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <ctime>
#include <cstring>
#include <strings.h>

namespace aurageek::services {
void HttpBufferFree::operator()(char* memory)const{heap_caps_free(memory);}
namespace {
esp_err_t metadata(esp_http_client_event_t* event){
 if(event->event_id!=HTTP_EVENT_ON_HEADER||!event->header_key||!event->header_value)return ESP_OK;
 auto& result=*static_cast<SecureHttpResponse*>(event->user_data);
 if(!strcasecmp(event->header_key,"Retry-After")){
   const size_t length=strnlen(event->header_value,96);
   const uint32_t delay=length==96?86400:StockRequestPolicy::retryAfter(std::string(event->header_value,length),uint32_t(time(nullptr)));
   result.retryAfterSeconds=std::max(result.retryAfterSeconds,delay);
 }
 if(!strcasecmp(event->header_key,"Content-Encoding")&&strcasecmp(event->header_value,"identity"))result.encoded=true;
 return ESP_OK;
}
struct ClientClose {void operator()(esp_http_client* client)const{if(client)esp_http_client_cleanup(client);}};
}
bool secureHttpGet(const char* url,size_t limit,uint32_t timeoutMs,SecureHttpResponse& result){
 result=SecureHttpResponse{};
 const char* sources[]={"https://push2his.eastmoney.com/","https://api.open-meteo.com/"};
 bool sourceAllowed=false;for(const char* source:sources)if(url&&!strncmp(url,source,strlen(source)))sourceAllowed=true;
 if(!sourceAllowed||!limit||limit>900000||timeoutMs<1000||timeoutMs>60000){result.error="REQUEST REJECTED";return false;}
 if(time(nullptr)<1704067200){result.error="CLOCK NOT READY";return false;}
 esp_http_client_config_t config{};config.url=url;config.method=HTTP_METHOD_GET;
 config.timeout_ms=4000;config.crt_bundle_attach=esp_crt_bundle_attach;
 config.disable_auto_redirect=true;config.buffer_size=1024;config.buffer_size_tx=1024;
 config.event_handler=metadata;config.user_data=&result;
 std::unique_ptr<esp_http_client,ClientClose> client(esp_http_client_init(&config));
 if(!client){result.error="CLIENT ALLOCATION";return false;}
 esp_http_client_set_header(client.get(),"User-Agent","AuraGeek/1.0 (ESP32-S3; cached-public-data)");
 esp_http_client_set_header(client.get(),"Accept","application/json");
 esp_http_client_set_header(client.get(),"Accept-Encoding","identity");
 const uint32_t started=millis();
 if(esp_http_client_open(client.get(),0)!=ESP_OK){result.error="TLS OR CONNECT";return false;}
 const int64_t advertised=esp_http_client_fetch_headers(client.get());
 result.status=esp_http_client_get_status_code(client.get());
 if(advertised<0){result.error="RESPONSE HEADERS";return false;}
 if(result.status!=200){result.error="HTTP STATUS";return false;} // Never download error/redirect pages.
 if(result.encoded||uint64_t(advertised)>limit){result.error=result.encoded?"UNSUPPORTED ENCODING":"BODY TOO LARGE";return false;}
 result.body.reset(static_cast<char*>(heap_caps_malloc(limit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
 if(!result.body){result.error="BODY ALLOCATION";return false;} // Do not consume scarce internal SRAM.
 const auto readResult=readHttpBody(result.body.get(),limit,result.length,
   [&](char* target,size_t capacity){return esp_http_client_read(client.get(),target,int(capacity));},
   [&](){return esp_http_client_is_complete_data_received(client.get());},
   [&](){return uint32_t(millis()-started)>=timeoutMs;},[](){vTaskDelay(1);});
 if(readResult!=BodyReadResult::Complete||!result.length){
   result.error=readResult==BodyReadResult::TooLarge?"BODY TOO LARGE":readResult==BodyReadResult::Timeout?"BODY TIMEOUT":readResult==BodyReadResult::InvalidText?"INVALID TEXT":"INCOMPLETE BODY";
   result.body.reset();result.length=0;return false;
 }
 result.error="OK";return true;
}
}

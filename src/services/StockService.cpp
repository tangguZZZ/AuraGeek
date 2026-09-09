#include "services/StockService.h"
#include "services/StockSeed.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
#include <ctime>
#include <cstring>
#include <cctype>

namespace aurageek { namespace services {
namespace {
constexpr size_t maxBars=12000;
const char* tickers[]={"QQQ","VOO"};const char* ids[]={"105.QQQ","107.VOO"};
struct Header {uint32_t magic,count,checksum;};
uint32_t checksum(const StockBar* b,size_t n){uint32_t h=2166136261u;auto* p=reinterpret_cast<const uint8_t*>(b);for(size_t i=0;i<n*sizeof(StockBar);++i)h=(h^p[i])*16777619u;return h;}
bool validBars(const StockBar* b,size_t n){if(!n||n>maxBars)return false;for(size_t i=0;i<n;++i)if(b[i].date<19900101||!std::isfinite(b[i].close)||b[i].close<=0||(i&&b[i].date<=b[i-1].date))return false;return true;}
struct PsramAllocator:ArduinoJson::Allocator {
 void* allocate(size_t n)override{return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
 void deallocate(void* p)override{heap_caps_free(p);}
 void* reallocate(void* p,size_t n)override{return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
uint32_t cooldown(int code,unsigned attempt){return code==403?86400:code==429?3600:attempt==1?60:300;}
}
void StockService::begin(){
 lock_=xSemaphoreCreateMutex();if(!lock_)return;
 storageReady_=LittleFS.begin(false);
 if(!storageReady_){
   // Only initialize a genuinely blank filesystem partition. Never auto-format
   // a corrupt or unrecognized existing user filesystem.
   const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);
   bool blank=part!=nullptr;uint8_t block[256];
   if(part)for(size_t offset=0;offset<part->size&&blank;offset+=sizeof(block)){
     if(esp_partition_read(part,offset,block,sizeof(block))!=ESP_OK){blank=false;break;}
     for(auto v:block)if(v!=0xff){blank=false;break;}
   }
   if(blank){storageReady_=LittleFS.format()&&LittleFS.begin(false);}
 }
 for(unsigned i=0;i<2;++i)load(i);
 if(xTaskCreate(task,"stock_daily",12288,this,1,&worker_)!=pdPASS)worker_=nullptr;
 Serial.printf("[STOCK] persistent cache=%s; fallback QQQ=%u VOO=%u (reference snapshot)\n",storageReady_?"ready":"disabled",unsigned(seedQQQCount),unsigned(seedVOOCount));
}
void StockService::load(unsigned i){
 if(!storageReady_)return;char path[32];snprintf(path,sizeof(path),"/ag-%s.raw.bin",tickers[i]);
 auto f=LittleFS.open(path,"r");if(!f)return;Header h{};
 if(f.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))!=sizeof(h)||h.magic!=0x41475332||h.count>maxBars||!h.count||f.size()!=sizeof(h)+h.count*sizeof(StockBar))return;
 auto* data=static_cast<StockBar*>(heap_caps_malloc(h.count*sizeof(StockBar),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!data)return;
 bool ok=f.read(reinterpret_cast<uint8_t*>(data),h.count*sizeof(StockBar))==h.count*sizeof(StockBar)&&validBars(data,h.count)&&checksum(data,h.count)==h.checksum;
 if(ok){bars_[i]=data;counts_[i]=h.count;snprintf(status_[i],64,"EM RAW / CACHED");Serial.printf("[STOCK] loaded %s rows=%u last=%lu checksum=OK\n",tickers[i],unsigned(h.count),(unsigned long)data[h.count-1].date);}else heap_caps_free(data);
}
bool StockService::save(unsigned i,const StockBar* data,size_t count){
 if(!storageReady_)return false;char path[32],temp[36];snprintf(path,sizeof(path),"/ag-%s.raw.bin",tickers[i]);snprintf(temp,sizeof(temp),"%s.tmp",path);
 auto f=LittleFS.open(temp,"w");if(!f)return false;Header h{0x41475332,uint32_t(count),checksum(data,count)};
 bool ok=f.write(reinterpret_cast<uint8_t*>(&h),sizeof(h))==sizeof(h)&&f.write(reinterpret_cast<const uint8_t*>(data),count*sizeof(StockBar))==count*sizeof(StockBar);f.flush();f.close();
 return ok&&LittleFS.rename(temp,path);
}
void StockService::process(bool connected,uint32_t epoch){
 if(!connected||epoch<1704067200UL||!worker_)return;
 time_t value=epoch;tm t{};gmtime_r(&value,&t);
 // Both DST and standard-time US sessions have closed by Shanghai 09:00.
 if(t.tm_wday<2||t.tm_wday>6||t.tm_hour<9)return;
 uint32_t day=(t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday;
 if(day<=attemptDay_&&millis()-lastWakeMs_<60000)return;
 lastWakeMs_=millis();attemptDay_=pendingDay_=day;xTaskNotifyGive(worker_);
}
StockChart StockService::snapshot(unsigned i,unsigned range)const{
 StockChart result;i%=2;snprintf(result.ticker,8,"%s",tickers[i]);if(!lock_)return result;
 xSemaphoreTake(lock_,portMAX_DELAY);
 makeStockChart(bars_[i]?bars_[i]:(i?seedVOO:seedQQQ),bars_[i]?counts_[i]:(i?seedVOOCount:seedQQQCount),range,result);
 snprintf(result.status,sizeof(result.status),"%s",status_[i]);xSemaphoreGive(lock_);return result;
}
void StockService::setStatus(unsigned i,const char* status){xSemaphoreTake(lock_,portMAX_DELAY);snprintf(status_[i],64,"%s",status);xSemaphoreGive(lock_);}
void StockService::task(void* context){
 auto* s=static_cast<StockService*>(context);
 for(;;){
   ulTaskNotifyTake(pdTRUE,portMAX_DELAY);const uint32_t day=s->pendingDay_;
   for(unsigned i=0;i<2;++i){
     Preferences p;p.begin("ag-stocks",false);char key[12];snprintf(key,sizeof(key),"budget%u",i);
     // Budget is written BEFORE requests. A reset consumes that attempt.
     uint64_t budget=p.getULong64(key,0);unsigned used=uint32_t(budget>>32)==day?unsigned(budget&0xff):0;
     // Preserve the previous firmware's already-consumed daily attempt.
     if(!budget&&p.getUInt("attempt",0)==day)used=1;
     char nextKey[12];snprintf(nextKey,sizeof(nextKey),"next%u",i);
     uint32_t nextEpoch=p.getUInt(nextKey,0);
     while(used<3){
       time_t now=time(nullptr); // Unix UTC; retry timestamp uses the same basis.
       if(now>1704067200&&uint32_t(now)<nextEpoch)break;
       ++used;p.putULong64(key,(uint64_t(day)<<32)|used);
       // Conservative minimum persisted cooldown also covers power loss mid-request.
       nextEpoch=uint32_t(now)+300;p.putUInt(nextKey,nextEpoch);
       int code=0;uint32_t retryAfter=0;
       bool ok=s->fetch(i,day,code,retryAfter);
       if(ok){p.putULong64(key,(uint64_t(day)<<32)|3);break;}
       uint32_t waitSeconds=std::max(cooldown(code,used),retryAfter);
       nextEpoch=uint32_t(time(nullptr))+waitSeconds;p.putUInt(nextKey,nextEpoch);
       if(code==403||code==429||used>=3)break;
       // Background task only. No UI/USB thread sleeps or network concurrency.
       vTaskDelay(pdMS_TO_TICKS(waitSeconds*1000u));
     }
     p.end();vTaskDelay(pdMS_TO_TICKS(2000+esp_random()%501));
   }
 }
}
bool StockService::fetch(unsigned i,uint32_t day,int& code,uint32_t& retryAfter){
 // One source at a time. Reference adjusted prices are never merged into raw prices.
 size_t count=counts_[i];auto* temp=static_cast<StockBar*>(heap_caps_malloc(maxBars*sizeof(StockBar),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!temp){setStatus(i,"CACHE / NO MEMORY");return false;}
 if(count)memcpy(temp,bars_[i],count*sizeof(StockBar));uint32_t last=count?temp[count-1].date:0;
 char url[384];snprintf(url,sizeof(url),"https://push2his.eastmoney.com/api/qt/stock/kline/get?secid=%s&fields1=f1,f2,f3,f4,f5,f6&fields2=f51,f53&klt=101&fqt=0&beg=%lu&end=20500101&lmt=10000",ids[i],(unsigned long)last);
 // HTTPClient decodes chunked responses in getString(). Do not parse raw TCP
 // chunks as JSON. A single worker issues requests with bounded response size.
 WiFiClientSecure client;client.setInsecure();HTTPClient http;http.setTimeout(12000);http.setConnectTimeout(6000);http.setReuse(true);
 bool ok=false;String error="NETWORK";
 if(http.begin(client,url)){
   http.setUserAgent("AuraGeek/1.0 (ESP32-S3; daily-cache)");http.addHeader("Referer","https://quote.eastmoney.com/");http.addHeader("Accept","application/json");http.addHeader("Accept-Encoding","identity");
   const char* headers[]={"Retry-After"};http.collectHeaders(headers,1);code=http.GET();retryAfter=http.header("Retry-After").toInt();
   if(code==200&&http.getSize()<=900000){
     String body=http.getString();PsramAllocator allocator;JsonDocument doc(&allocator),filter;
     filter["rc"]=true;filter["data"]["name"]=true;
     // Validate the JSON envelope while skipping kline string storage. Keeping
     // thousands of unique strings in ArduinoJson's dedup pool was O(n^2) and
     // starved IDLE0 on the initial all-history response.
     auto err=deserializeJson(doc,body,DeserializationOption::Filter(filter));
     const char* cursor=strstr(body.c_str(),"\"klines\"");
     if(cursor){cursor+=8;while(isspace(*cursor))++cursor;if(*cursor++!=':')cursor=nullptr;}
     if(cursor){while(isspace(*cursor))++cursor;if(*cursor++!='[')cursor=nullptr;}
     if(!err&&body.length()<=900000&&doc["rc"].is<int>()&&doc["rc"].as<int>()==0&&cursor){
       ok=true;size_t parsed=0;
       while(ok){
         while(isspace(*cursor))++cursor;
         if(*cursor==']')break;
         if(*cursor++!='"'){ok=false;break;}
         const char* end=strchr(cursor,'"');if(!end||end-cursor>=64){ok=false;break;}
         char row[64];memcpy(row,cursor,end-cursor);row[end-cursor]=0;
         unsigned y,m,d;float close;int consumed=0;
         if(sscanf(row,"%u-%u-%u,%f%n",&y,&m,&d,&close,&consumed)!=4||row[consumed]!=0||m<1||m>12||d<1||d>31||!std::isfinite(close)||close<=0){ok=false;break;}
         const uint32_t date=y*10000+m*100+d;
         if(date<day&&date>=last){
           if(count&&date==temp[count-1].date)temp[count-1].close=close;
           else if(count>=maxBars||(count&&date<temp[count-1].date)){ok=false;break;}
           else temp[count++]={date,close};
         }
         cursor=end+1;while(isspace(*cursor))++cursor;
         if(*cursor==',')++cursor;else if(*cursor!=']'){ok=false;break;}
         if(++parsed%64==0)vTaskDelay(1);
       }
       ok=ok&&validBars(temp,count);if(!ok)error="BAD DAILY ROW";
     }else error=err?err.c_str():"NO DATA";
   }else error=String("HTTP ")+code;
 }
 http.end();
 if(ok){bool persisted=save(i,temp,count);xSemaphoreTake(lock_,portMAX_DELAY);auto* old=bars_[i];bars_[i]=temp;counts_[i]=count;snprintf(status_[i],64,persisted?"EM RAW / DAILY":"EM RAW / RAM ONLY");xSemaphoreGive(lock_);heap_caps_free(old);}
 else{heap_caps_free(temp);char status[64];snprintf(status,sizeof(status),"%s / %s",counts_[i]?"EM STALE":"REF CACHE",error.c_str());setStatus(i,status);}
 Serial.printf("[STOCK] %s HTTP=%d rows=%u %s\n",tickers[i],code,unsigned(ok?count:counts_[i]),ok?"updated":"cached fallback; bounded retry");return ok;
}
}}

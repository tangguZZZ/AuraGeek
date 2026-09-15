#include "services/StockService.h"
#include "services/StockSeed.h"
#include "services/StockRequestPolicy.h"
#include "services/SecureHttpGet.h"
#include "services/StockDailyResponse.h"
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
struct Header {uint32_t magic,count,checksum;};
uint32_t checksum(const StockBar* b,size_t n){uint32_t h=2166136261u;auto* p=reinterpret_cast<const uint8_t*>(b);for(size_t i=0;i<n*sizeof(StockBar);++i)h=(h^p[i])*16777619u;return h;}
bool validBars(const StockBar* b,size_t n){if(!n||n>maxBars)return false;for(size_t i=0;i<n;++i)if(!validStockDate(b[i].date)||!validStockPrice(b[i].close)||(i&&b[i].date<=b[i-1].date))return false;return true;}
struct PsramAllocator:ArduinoJson::Allocator {
 void* allocate(size_t n)override{return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
 void deallocate(void* p)override{heap_caps_free(p);}
 void* reallocate(void* p,size_t n)override{return heap_caps_realloc(p,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
};
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
 for(unsigned i=0;i<preferences_.count;++i){snprintf(status_[i],64,StockPreferences::seedIndex(preferences_.symbols[i])>=0?"REF CACHE / OFFLINE":"NO DATA / OFFLINE");load(i);}
 if(xTaskCreate(task,"stock_daily",12288,this,1,&worker_)!=pdPASS)worker_=nullptr;
 Serial.printf("[STOCK] persistent cache=%s; watchlist=%u MA=%u/%u; references only for QQQ/VOO\n",storageReady_?"ready":"disabled",preferences_.count,preferences_.maFast,preferences_.maSlow);
}
void StockService::load(unsigned i){
 if(!storageReady_)return;char path[40];snprintf(path,sizeof(path),"/ag-%s.raw.bin",preferences_.symbols[i].c_str());
 auto f=LittleFS.open(path,"r");
 // Only the exact two original identities may read the old ticker-only files.
 if(!f&&StockPreferences::seedIndex(preferences_.symbols[i])>=0){snprintf(path,sizeof(path),"/ag-%s.raw.bin",StockPreferences::ticker(preferences_.symbols[i]).c_str());f=LittleFS.open(path,"r");}
 if(!f)return;Header h{};
 if(f.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))!=sizeof(h)||h.magic!=0x41475332||h.count>maxBars||!h.count||f.size()!=sizeof(h)+h.count*sizeof(StockBar))return;
 auto* data=static_cast<StockBar*>(heap_caps_malloc(h.count*sizeof(StockBar),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!data)return;
 bool ok=f.read(reinterpret_cast<uint8_t*>(data),h.count*sizeof(StockBar))==h.count*sizeof(StockBar)&&validBars(data,h.count)&&checksum(data,h.count)==h.checksum;
 if(ok){bars_[i]=data;counts_[i]=h.count;snprintf(status_[i],64,"EM RAW / CACHED");Serial.printf("[STOCK] loaded %s rows=%u last=%lu checksum=OK\n",preferences_.symbols[i].c_str(),unsigned(h.count),(unsigned long)data[h.count-1].date);}else heap_caps_free(data);
}
bool StockService::save(unsigned i,const StockBar* data,size_t count){
 if(!storageReady_)return false;char path[40],temp[44];snprintf(path,sizeof(path),"/ag-%s.raw.bin",preferences_.symbols[i].c_str());snprintf(temp,sizeof(temp),"%s.tmp",path);
 auto f=LittleFS.open(temp,"w");if(!f)return false;Header h{0x41475332,uint32_t(count),checksum(data,count)};
 bool ok=f.write(reinterpret_cast<uint8_t*>(&h),sizeof(h))==sizeof(h)&&f.write(reinterpret_cast<const uint8_t*>(data),count*sizeof(StockBar))==count*sizeof(StockBar);f.flush();f.close();
 return ok&&LittleFS.rename(temp,path);
}
void StockService::process(bool connected,uint32_t epoch){
 if(!connected||epoch<1704067200UL||!worker_)return;
 // Cache refresh uses a fixed Shanghai schedule, independent of the user's clock timezone.
 time_t value=epoch+8*3600u;tm t{};gmtime_r(&value,&t);
 // Both DST and standard-time US sessions have closed by Shanghai 09:00.
 if(t.tm_wday<2||t.tm_wday>6||t.tm_hour<9)return;
 uint32_t day=(t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday;
 if(day<=attemptDay_&&millis()-lastWakeMs_<60000)return;
 lastWakeMs_=millis();attemptDay_=pendingDay_=day;xTaskNotifyGive(worker_);
}
StockChart StockService::snapshot(unsigned i,unsigned range)const{
 StockChart result;i%=preferences_.count;const auto& symbol=preferences_.symbols[i];
 snprintf(result.ticker,sizeof(result.ticker),"%s",StockPreferences::ticker(symbol).c_str());snprintf(result.currency,sizeof(result.currency),"%s",StockPreferences::currency(symbol));
 result.maFast=preferences_.maFast;result.maSlow=preferences_.maSlow;result.showFast=preferences_.showFast;result.showSlow=preferences_.showSlow;
 if(!lock_)return result;
 xSemaphoreTake(lock_,portMAX_DELAY);
 const int seed=StockPreferences::seedIndex(symbol);
 const StockBar* fallback=seed==0?seedQQQ:seed==1?seedVOO:nullptr;
 const size_t fallbackCount=seed==0?seedQQQCount:seed==1?seedVOOCount:0;
 makeStockChart(bars_[i]?bars_[i]:fallback,bars_[i]?counts_[i]:fallbackCount,range,result);
 snprintf(result.status,sizeof(result.status),"%s",status_[i]);xSemaphoreGive(lock_);return result;
}
void StockService::setStatus(unsigned i,const char* status){xSemaphoreTake(lock_,portMAX_DELAY);snprintf(status_[i],64,"%s",status);xSemaphoreGive(lock_);}
void StockService::task(void* context){
 auto* s=static_cast<StockService*>(context);
 bool held=false;
 for(;;){
   ulTaskNotifyTake(pdTRUE,portMAX_DELAY);const uint32_t day=s->pendingDay_;
   Preferences p;if(!p.begin("ag-stocks",false)){for(unsigned i=0;i<s->preferences_.count;++i)s->setStatus(i,"CACHE / BUDGET STORAGE FAIL");continue;}
   const uint64_t global=p.getULong64("globalbudget",0);
   StockRequestPolicy::State policy{uint32_t(global>>32),uint32_t(global),p.getUInt("globalnext",0),held||p.getBool("providerblocked",false)};
   uint32_t now=uint32_t(time(nullptr));
   if(!StockRequestPolicy::allowed(policy,day,now)){
     if(policy.forbidden)for(unsigned i=0;i<s->preferences_.count;++i)s->setStatus(i,"CACHE / PROVIDER PAUSED");
     p.end();continue;
   }
   // One request at most per worker wake. Never sleep a networking task through
   // a provider cooldown; the regular scheduler will re-check persisted gates.
   for(unsigned i=0;i<s->preferences_.count;++i){
     char key[16],nextKey[16];snprintf(key,sizeof(key),"b%s",s->preferences_.symbols[i].c_str());snprintf(nextKey,sizeof(nextKey),"n%s",s->preferences_.symbols[i].c_str());
     uint64_t budget=p.getULong64(key,0);uint32_t nextEpoch=p.getUInt(nextKey,0);
     const int legacy=StockPreferences::seedIndex(s->preferences_.symbols[i]);
     if(legacy>=0){
       char oldKey[12];if(!budget){snprintf(oldKey,sizeof(oldKey),"budget%d",legacy);budget=p.getULong64(oldKey,0);}
       if(!nextEpoch){snprintf(oldKey,sizeof(oldKey),"next%d",legacy);nextEpoch=p.getUInt(oldKey,0);}
     }
     unsigned used=uint32_t(budget>>32)==day?unsigned(uint32_t(budget)):0;
     if(!budget&&p.getUInt("attempt",0)==day)used=1;
     if(used>=3||now<nextEpoch)continue;
     ++used;StockRequestPolicy::reserve(policy,day,now);
     // Charge BOTH global and symbol budgets before any network operation.
     const bool reserved=p.putULong64("globalbudget",(uint64_t(day)<<32)|policy.used)==sizeof(uint64_t)
       &&p.putUInt("globalnext",policy.next)==sizeof(uint32_t)
       &&p.putULong64(key,(uint64_t(day)<<32)|used)==sizeof(uint64_t)
       &&p.putUInt(nextKey,StockRequestPolicy::deadline(now,300))==sizeof(uint32_t);
     if(!reserved){held=true;s->setStatus(i,"CACHE / BUDGET SAVE FAIL");break;}
     int code=0;uint32_t retry=0;const bool ok=s->fetch(i,day,code,retry);
     now=uint32_t(time(nullptr));StockRequestPolicy::finish(policy,now,code,retry,used,ok);
     if(code==403){held=true;if(!p.putBool("providerblocked",true))Serial.println("[STOCK] provider hold persistence failed; runtime requests remain paused");}
     if(!p.putUInt("globalnext",policy.next))held=true;
     if(!p.putUInt(nextKey,policy.next))held=true;
     if(ok&&!p.putULong64(key,(uint64_t(day)<<32)|3u))held=true;
     if(code==403||code==429)for(unsigned j=0;j<s->preferences_.count;++j)s->setStatus(j,code==403?"CACHE / PROVIDER PAUSED":"CACHE / RATE LIMITED");
     break;
   }
   p.end();
 }
}
bool StockService::fetch(unsigned i,uint32_t day,int& code,uint32_t& retryAfter){
 // One source at a time. Reference adjusted prices are never merged into raw prices.
 size_t count=counts_[i];auto* temp=static_cast<StockBar*>(heap_caps_malloc(maxBars*sizeof(StockBar),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!temp){setStatus(i,"CACHE / NO MEMORY");return false;}
 if(count)memcpy(temp,bars_[i],count*sizeof(StockBar));uint32_t last=count?temp[count-1].date:0;
 char url[384];snprintf(url,sizeof(url),"https://push2his.eastmoney.com/api/qt/stock/kline/get?secid=%s&fields1=f1,f2,f3,f4,f5,f6&fields2=f51,f53&klt=101&fqt=0&beg=%lu&end=20500101&lmt=10000",preferences_.symbols[i].c_str(),(unsigned long)last);
 SecureHttpResponse response;const bool received=secureHttpGet(url,900000,30000,response);
 code=response.status;retryAfter=response.retryAfterSeconds;
 bool ok=false;String error=response.error;
 if(received){
     PsramAllocator allocator;const char* parseError=nullptr;
     ok=mergeStockDailyResponse(response.body.get(),response.length,
       StockPreferences::ticker(preferences_.symbols[i]).c_str(),day,temp,maxBars,count,
       parseError,[](){vTaskDelay(1);},&allocator);
     if(ok&&!validBars(temp,count)){ok=false;parseError="BAD MERGED DAILY DATA";}
     if(!ok)error=parseError;
 }else if(code>0)error=String(response.error)+" / HTTP "+code;
 if(ok){bool persisted=save(i,temp,count);xSemaphoreTake(lock_,portMAX_DELAY);auto* old=bars_[i];bars_[i]=temp;counts_[i]=count;snprintf(status_[i],64,persisted?"EM RAW / DAILY":"EM RAW / RAM ONLY");xSemaphoreGive(lock_);heap_caps_free(old);}
 else{heap_caps_free(temp);char status[64];snprintf(status,sizeof(status),"%s / %s",counts_[i]?"EM STALE":StockPreferences::seedIndex(preferences_.symbols[i])>=0?"REF CACHE":"NO DATA",error.c_str());setStatus(i,status);}
 Serial.printf("[STOCK] %s HTTP=%d rows=%u %s\n",preferences_.symbols[i].c_str(),code,unsigned(ok?count:counts_[i]),ok?"updated":"cached fallback; bounded retry");return ok;
}
}}

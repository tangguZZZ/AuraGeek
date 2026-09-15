#include "services/FactoryReset.h"
#include <cassert>
#include <map>
#include <string>
#include <iostream>
struct FakeStore {
  inline static std::map<std::string,std::string> values;
  inline static int failAt=-1,operations=0;
  std::string space;
  bool fail(){return ++operations==failAt;}
  bool begin(const char* name,bool){space=name;return !fail();}
  void end(){}
  std::string key(const char* k){return space+"/"+k;}
  bool isKey(const char* k){return values.count(key(k));}
  bool getBool(const char* k,bool fallback){return isKey(k)?values[key(k)]=="1":fallback;}
  unsigned putBool(const char* k,bool v){if(fail())return 0;values[key(k)]=v?"1":"0";return 1;}
  bool remove(const char* k){if(fail())return false;return values.erase(key(k))==1;}
};
using Reset=aurageek::services::FactoryReset<FakeStore>;
const std::map<std::string,std::string> baseline={
 {"aura-ai/client-id","bound-id"},{"aura-ai/ws-config","official-secret"},
 {"aura-ai/wake-guard","1"},{"ag-stocks/providerblocked","1"},
 {"ag-stocks/globalbudget","12"},{"ag-stocks/globalnext","future"},
 {"ag-stocks/daily-cache","cache"},{"aura-web/config","custom-colors-ma21"},
 {"aura-web/custom-id","custom-identity"},{"aura-net/network","wifi-secret"},
 {"aura-ui/settings","volume42"},{"unrelated/key","untouched"}};
void seed(){FakeStore::values=baseline;FakeStore::operations=0;FakeStore::failAt=-1;}
void preserved(){for(const auto& [k,v]:baseline)if(k.rfind("aura-ai/",0)==0||k.rfind("ag-stocks/",0)==0||k=="unrelated/key")assert(FakeStore::values.at(k)==v);}
void resetDone(){preserved();for(const auto* k:{"aura-web/config","aura-web/custom-id","aura-net/network","aura-ui/settings","aura-web/reset-pending"})assert(!FakeStore::values.count(k));assert(FakeStore::values.at("aura-web/setup")=="1");assert(Reset::userNetworkOnly());}
int main(){
 seed();assert(Reset::resume()==Reset::Result::None);assert(FakeStore::values==baseline);assert(!Reset::userNetworkOnly());
 assert(Reset::request());assert(FakeStore::values.at("aura-web/config")=="custom-colors-ma21");
 FakeStore::operations=0;assert(Reset::resume()==Reset::Result::Completed);const int count=FakeStore::operations;resetDone();
 for(int failure=1;failure<=count;++failure){
   seed();assert(Reset::request());FakeStore::operations=0;FakeStore::failAt=failure;
   assert(Reset::resume()==Reset::Result::Failed);preserved();assert(FakeStore::values.at("aura-web/reset-pending")=="1");
   FakeStore::failAt=-1;assert(Reset::resume()==Reset::Result::Completed);resetDone();
 }
 for(int failure=1;failure<=2;++failure){seed();FakeStore::failAt=failure;assert(!Reset::request());assert(FakeStore::values==baseline);}
 seed();assert(Reset::request());assert(Reset::request());assert(Reset::resume()==Reset::Result::Completed);resetDone();
 const auto done=FakeStore::values;assert(Reset::resume()==Reset::Result::None);assert(FakeStore::values==done);
 std::cout<<"PASS factory reset allow-list, staged intent, every operation failure/resume, retained binding and quotas\n";
}

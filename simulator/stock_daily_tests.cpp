#include "services/StockDailyResponse.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace aurageek::services;

static std::string response(const std::string& rows,const char* code="QQQ"){
 return std::string("{\"rc\":0,\"data\":{\"code\":\"")+code+"\",\"name\":\"Test\",\"klines\":["+rows+"]}}";
}
int main(){
 StockBar row{};
 for(const char* s:{"2024-02-29,123.45","2026-09-11,0.000001","2000-02-29,1000000000"})assert(parseStockRow(s,strlen(s),row));
 for(const char* s:{"2023-02-29,1","2024-02-30,1","2026-04-31,1","1989-12-31,1","2100-01-01,1","2026-00-01,1","2026-01-00,1","2026-1-01,1","2026-09-11,NaN","2026-09-11,inf","2026-09-11,1e30","2026-09-11,-1","2026-09-11,0","2026-09-11,+1","2026-09-11, 1","2026-09-11,1 ","2026-09-11,1,2","2026-09-11,0x10","2026-09-11,.1","2026-09-11,1.","2026-09-11,1.2.3","2026-09-11,0.0000001","2026-09-11,1000000001"})assert(!parseStockRow(s,strlen(s),row));
 auto run=[](const std::string& raw,std::vector<StockBar>& committed,size_t cap=100){
   std::vector<StockBar> staging(cap);std::copy(committed.begin(),committed.end(),staging.begin());size_t count=committed.size();const char* error=nullptr;
   bool ok=mergeStockDailyResponse(raw.data(),raw.size(),"QQQ",20260912,staging.data(),cap,count,error,[](){});
   if(ok){staging.resize(count);committed=staging;}else assert(error&&strcmp(error,"OK"));return ok;
 };
 std::vector<StockBar> bars{{20260910,100}};
 assert(run(response("\"2026-09-10,101\",\"2026-09-11,102\",\"2026-09-12,999\""),bars));
 assert(bars.size()==2&&bars[0].close==101&&bars[1].close==102); // Today never published.
 const std::string good=response("\"2026-09-11,103\"");
 const std::string badCases[]={
   response("\"2026-09-11,103\"","VOO"),response(""),response("\"2026-09-09,99\""),
   response("\"2026-09-12,103\""),response("\"2026-09-11,103\",\"2026-02-30,104\""),
   response("\"2026-09-11,103\",\"2026-09-11,104\""),response("\"2026-09-11,103\",\"2026-09-10,104\""),
   response("\"2026-09-11,103\","),good+"garbage",good+good,
   R"({"rc":0,"data":{"code":"QQQ"},"klines":["2026-09-11,777"]})",
   R"({"rc":0,"data":{"code":"QQQ","nested":{"klines":["2026-09-11,777"]}}})",
   R"({"rc":0,"data":{"code":"QQQ","klines":["2026-09-11,103"]},"data":{"code":"VOO","klines":["2026-09-11,777"]}})",
   R"({"rc":0,"rc":0,"data":{"code":"QQQ","klines":["2026-09-11,103"]}})",
   R"({"rc":1,"data":{"code":"QQQ","klines":["2026-09-11,103"]}})",
   R"({"rc":0,"data":{"code":"VOO","code":"QQQ","klines":["2026-09-11,103"]}})",
   R"({"rc":0,"data":{"code":"QQQ","klines":[],"klines":["2026-09-11,103"]}})",
   R"({"rc":0,"data":{"code":"QQQ\u0000VOO","klines":["2026-09-11,103"]}})",
   R"({"rc":0,"data":{"code":"QQQ","\u006blines":["2026-09-11,103"]}})",
   R"({"rc":0,"data":{"code":"QQQ","klines":["2026-09-11,1\u0030"]}})",
   R"({"rc":0,"data":{"code":"QQQ","klines":"2026-09-11,103"}})",
 };
 for(const auto& raw:badCases){const auto before=bars;assert(!run(raw,bars));assert(bars.size()==before.size()&&!memcmp(bars.data(),before.data(),bars.size()*sizeof(StockBar)));}
 // Same key elsewhere must not displace data.klines; nested braces/escaped quotes skip correctly.
 assert(run(R"({"klines":["2026-09-11,777"],"note":"\"data\":{}","rc":0,"ignored":[{},[],true,null,3],"data":{"nested":{"klines":[]},"code":"QQQ","klines":["2026-09-11,105"]}})",bars));
 assert(bars.back().close==105);
 for(size_t cut=0;cut<good.size();++cut){auto copy=bars;assert(!run(good.substr(0,cut),copy));}
 std::vector<StockBar> limited{{20260910,100}};assert(!run(response("\"2026-09-10,101\",\"2026-09-11,102\""),limited,1));assert(limited[0].close==100);
 // All-history linear scan with periodic yields, no network and no per-row JsonDocument.
 std::string rows;unsigned total=0;
 for(unsigned y=1990;y<2026;++y)for(unsigned m=1;m<=12;++m)for(unsigned d=1;d<=31;++d){
   if(!validStockDate(y*10000+m*100+d))continue;
   char text[64];snprintf(text,sizeof(text),"%s\"%04u-%02u-%02u,123.45\"",total?",":"",y,m,d);rows+=text;++total;
 }
 auto raw=response(rows);std::vector<StockBar> staged(total);size_t count=0,yields=0;const char* error=nullptr;
 assert(mergeStockDailyResponse(raw.data(),raw.size(),"QQQ",20260912,staged.data(),staged.size(),count,error,[&](){++yields;}));
 assert(count==total&&yields==total/64);
 puts("PASS daily response: strict path/code/calendar/price/order, no partial publish, empty/stale rejection, truncation, capacity and linear full history");
}

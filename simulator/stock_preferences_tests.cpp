#include "services/PortalConfig.h"
#include "services/StockChart.h"
#include "services/StockRequestPolicy.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
using namespace aurageek::services;
int main(){
 using Policy=StockRequestPolicy;Policy::State gate;const uint32_t now=1704067200u,day=20240101;
 assert(!Policy::allowed(gate,day,0));
 for(unsigned i=0;i<Policy::dailyLimit;++i){const auto at=now+i*60;assert(Policy::allowed(gate,day,at));Policy::reserve(gate,day,at);assert(!Policy::allowed(gate,day,at+59));}
 assert(!Policy::allowed(gate,day,now+86400)); // Reboot/reordering cannot change persisted daily budget.
 assert(!Policy::allowed(gate,day-1,now+86400)); // Clock rollback cannot reset a consumed budget.
 assert(Policy::allowed(gate,day+1,now+86400));
 Policy::finish(gate,now,429,7200,1,false);assert(gate.next==now+7200&&!gate.forbidden);
 Policy::finish(gate,now,429,0,1,false);assert(gate.next==now+3600);
 Policy::finish(gate,now,403,0,1,false);assert(!Policy::allowed(gate,day+1,now+86400)&&gate.forbidden);
 assert(Policy::retryAfter("120",now)==120);assert(Policy::retryAfter(" 3600 ",now)==3600);
 assert(Policy::retryAfter("Mon, 01 Jan 2024 00:01:00 GMT",now)==60);
 assert(Policy::retryAfter("Sun, 31 Dec 2023 23:59:00 GMT",now)==0);
 assert(Policy::retryAfter("Tue, 30 Feb 2024 00:00:00 GMT",now)==86400);
 assert(Policy::retryAfter("-1",now)==86400);
 assert(Policy::retryAfter("999999999999999999999",now)==UINT32_MAX);
 assert(Policy::deadline(now,UINT32_MAX)==UINT32_MAX);
 PortalConfig value;std::string error;
 auto merge=[&](const char* raw){JsonDocument d;assert(!deserializeJson(d,raw));return value.merge(d.as<JsonObjectConst>(),error);};
 assert(value.stocks.count==2&&value.stocks.symbols[0]=="105.QQQ"&&value.stocks.symbols[1]=="107.VOO");
 for(const char* raw:{R"({"schema":1,"stocks":[]})",R"({"schema":1,"stocks":{"symbols":[]}})",R"({"schema":1,"stocks":{"symbols":["105.QQQ","105.QQQ"]}})",R"({"schema":1,"stocks":{"symbols":["105.QQQ?x=1"]}})",R"({"schema":1,"stocks":{"symbols":["../cache"]}})",R"({"schema":1,"stocks":{"symbols":["105.QQQ\u0000evil"]}})",R"({"schema":1,"stocks":{"ma_fast":20,"ma_slow":20}})",R"({"schema":1,"stocks":{"ma_fast":2.5}})",R"({"schema":1,"stocks":{"show_fast":1}})",R"({"schema":1,"stocks":{"ma_fast\u0000x":5}})",R"({"schema":1,"stocks":{"refresh_seconds":1}})"}){
  assert(!merge(raw));assert(value.stocks.count==2&&value.stocks.symbols[0]=="105.QQQ"&&value.stocks.maFast==20);
 }
 assert(merge(R"({"schema":1,"stocks":{"symbols":["116.00700","1.600519","0.000001","105.QQQ","107.VOO","106.BRK.B"],"ma_fast":5,"ma_slow":120,"show_slow":false}})"));
 assert(value.stocks.count==6&&value.stocks.maFast==5&&!value.stocks.showSlow);
 assert(!strcmp(StockPreferences::currency(value.stocks.symbols[0]),"HKD"));assert(!strcmp(StockPreferences::currency(value.stocks.symbols[1]),"CNY"));
 assert(!strcmp(StockPreferences::currency(value.stocks.symbols[3]),"USD"));
 assert(StockPreferences::seedIndex("105.QQQ")==0&&StockPreferences::seedIndex("107.VOO")==1&&StockPreferences::seedIndex("106.QQQ")==-1);
 for(const auto& id:value.stocks.symbols){assert(StockPreferences::validSymbol(id));assert(id.size()+1<=15);}
 for(const char* id:{"0.1","116.700","1.600519/","105.qqq","105..QQQ","105.Q..Q","105.-QQQ","105.QQQ-","999.QQQ","105.WWWWWWWWWWW"})assert(!StockPreferences::validSymbol(id));
 JsonDocument saved;value.json(saved,true);std::string raw;serializeJson(saved,raw);assert(raw.size()<PortalConfig::kMaxJsonBytes);
 JsonDocument parsed;assert(!deserializeJson(parsed,raw,DeserializationOption::NestingLimit(5)));PortalConfig restored;
 assert(restored.merge(parsed.as<JsonObjectConst>(),error,true));assert(restored.stocks.symbols==value.stocks.symbols&&restored.stocks.maSlow==120&&!restored.stocks.showSlow);
 assert(merge(R"({"schema":1,"identity":{"name":"Test"}})"));assert(value.stocks.count==6); // Old web clients do not reset watchlist.
 JsonDocument delta;value.changesFrom(PortalConfig{},delta.to<JsonArray>());std::string changes;serializeJson(delta,changes);assert(changes.find("stocks.symbols")!=std::string::npos);
 std::vector<StockBar> bars;for(unsigned i=0;i<300;++i)bars.push_back({20200101+i,float(i+1)});
 StockChart chart;chart.maFast=5;chart.maSlow=120;makeStockChart(bars.data(),bars.size(),4,chart);
 assert(std::abs(chart.ma20[chart.count-1]-298.f)<.001f&&std::abs(chart.ma55[chart.count-1]-240.5f)<.001f);
 chart.showFast=chart.showSlow=false;makeStockChart(bars.data(),bars.size(),4,chart);
 assert(std::isnan(chart.ma20[chart.count-1])&&std::isnan(chart.ma55[chart.count-1]));
 assert(chart.low==1&&chart.high==300);
 for(auto& bar:bars)bar.close=.12f;makeStockChart(bars.data(),bars.size(),4,chart);
 assert(chart.low<.12f&&chart.high>.12f&&std::abs((chart.low+chart.high)/2-.12f)<.000001f);
 makeStockChart(bars.data(),1,4,chart);assert(chart.count==1&&chart.high>chart.low);
 puts("PASS stock preferences: strict transaction, 6 symbols, identity isolation, currency, persisted roundtrip, variable MA and hidden-MA scaling");
}

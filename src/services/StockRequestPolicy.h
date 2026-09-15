#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
namespace aurageek::services {
// Local conservative policy, NOT an advertised Eastmoney quota or permission.
struct StockRequestPolicy {
 static constexpr unsigned dailyLimit=12,minSpacing=60;
 struct State {uint32_t day=0,used=0,next=0;bool forbidden=false;};
 static bool allowed(const State& s,uint32_t day,uint32_t now){return now>=1704067200u&&day>=s.day&&!s.forbidden&&now>=s.next&&(s.day!=day||s.used<dailyLimit);}
 static uint32_t deadline(uint32_t now,uint64_t delay){return uint32_t(std::min<uint64_t>(uint64_t(now)+delay,UINT32_MAX));}
 static void reserve(State& s,uint32_t day,uint32_t now){if(s.day!=day){s.day=day;s.used=0;}++s.used;s.next=deadline(now,minSpacing);}
 static uint32_t retryAfter(const std::string& header,uint32_t now){
   if(header.empty())return 0;
   const auto begin=header.find_first_not_of(" \t"),end=header.find_last_not_of(" \t");if(begin==std::string::npos)return 86400;
   const std::string text=header.substr(begin,end-begin+1);
   uint64_t seconds=0;bool numeric=true;
   for(char c:text){if(c<'0'||c>'9'){numeric=false;break;}seconds=std::min<uint64_t>(UINT32_MAX,seconds*10+unsigned(c-'0'));}
   if(numeric)return uint32_t(seconds);
   // IMF-fixdate, the standard HTTP-date format sent by modern HTTP servers.
   char month[4]{},zone[4]{};int d=0,y=0,h=0,m=0,s=0,consumed=0;
   if(text.size()<5||text[3]!=','||text[4]!=' '||sscanf(text.c_str()+5,"%d %3s %d %d:%d:%d %3s%n",&d,month,&y,&h,&m,&s,zone,&consumed)!=7)return 86400;
   consumed+=5;
   const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};int mo=0;for(int i=0;i<12;++i)if(!strcmp(month,months[i]))mo=i+1;
   if(size_t(consumed)!=text.size()||strcmp(zone,"GMT")||!mo||y<1970||y>2106||h<0||h>23||m<0||m>59||s<0||s>59)return 86400;
   const bool leap=y%4==0&&(y%100!=0||y%400==0);const int days[]={31,28+int(leap),31,30,31,30,31,31,30,31,30,31};if(d<1||d>days[mo-1])return 86400;
   int adjusted=y-(mo<=2),era=adjusted/400,yoe=adjusted-era*400;
   const int doy=(153*(mo+(mo>2?-3:9))+2)/5+d-1;
   const int64_t epochDays=int64_t(era)*146097+yoe*365+yoe/4-yoe/100+doy-719468;
   const uint64_t epoch=uint64_t(epochDays*86400+h*3600+m*60+s);
   return epoch<=now?0:uint32_t(std::min<uint64_t>(epoch-now,UINT32_MAX));
 }
 static void finish(State& s,uint32_t now,int code,uint32_t retry,unsigned attempt,bool ok){
   if(code==403){s.forbidden=true;return;} // No automatic retries after explicit refusal.
   const uint32_t base=code==429?3600u:ok?minSpacing:attempt==1?60u:300u;
   s.next=deadline(now,std::max(base,retry));
 }
};
}

#pragma once
#include "services/StockChart.h"
#include <ArduinoJson.h>
#include <cstdlib>
#include <cstring>

namespace aurageek::services {
inline bool validStockDate(uint32_t date){
 const unsigned year=date/10000,month=date/100%100,day=date%100;
 if(year<1990||year>2099||month<1||month>12||day<1)return false;
 const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 return day<=days[month-1]+unsigned(month==2&&year%4==0&&(year%100!=0||year%400==0));
}
inline bool validStockPrice(float price){return std::isfinite(price)&&price>=.000001f&&price<=1000000000.f;}
inline bool parseStockRow(const char* data,size_t length,StockBar& result){
 if(!data||length<12||length>=64||data[4]!='-'||data[7]!='-'||data[10]!=',')return false;
 unsigned date=0;
 for(unsigned i=0;i<10;++i){if(i==4||i==7)continue;if(data[i]<'0'||data[i]>'9')return false;date=date*10+unsigned(data[i]-'0');}
 if(!validStockDate(date))return false;
 // Provider close prices are positive decimal text, not JSON/CSV fragments,
 // hex floats, whitespace, NaN/Inf, exponent tricks, or extra columns.
 bool dot=false,digit=false;
 for(size_t i=11;i<length;++i){if(data[i]=='.'&&!dot){dot=true;continue;}if(data[i]<'0'||data[i]>'9')return false;digit=true;}
 if(!digit||data[11]=='.'||data[length-1]=='.')return false;
 char price[64];memcpy(price,data+11,length-11);price[length-11]=0;
 char* end=nullptr;const double precise=strtod(price,&end);
 if(!end||*end||!std::isfinite(precise)||precise<.000001||precise>1000000000.)return false;
 const float close=float(precise);
 result={date,close};return true;
}

namespace stock_json_detail {
struct Span {const char* begin=nullptr;const char* end=nullptr;};
inline void space(const char*& p,const char* end){while(p<end&&(*p==' '||*p=='\r'||*p=='\n'||*p=='\t'))++p;}
// Structural locator only. ArduinoJson validates the entire envelope first.
// Never store/deduplicate thousands of kline strings in a JsonDocument.
inline bool skip(const char*& p,const char* end,unsigned depth=0){
 space(p,end);if(p==end||depth>8)return false;
 if(*p=='"'){
   ++p;while(p<end){if(*p=='\\'){++p;if(p==end)return false;++p;}else if(*p++=='"')return true;}return false;
 }
 if(*p=='{'||*p=='['){
   const char close=*p++=='{'?'}':']';space(p,end);
   while(p<end&&*p!=close){if(*p==','||*p==':'){++p;continue;}if(!skip(p,end,depth+1))return false;space(p,end);}
   if(p==end)return false;++p;return true;
 }
 const char* start=p;while(p<end&&*p!=','&&*p!='}'&&*p!=']'&&*p!=' '&&*p!='\r'&&*p!='\n'&&*p!='\t')++p;
 return p>start;
}
inline bool member(Span object,const char* name,Span& value){
 const char* p=object.begin;if(!p||p==object.end||*p++!='{')return false;
 bool found=false;space(p,object.end);
 while(p<object.end&&*p!='}'){
   if(*p++!='"')return false;const char* key=p;
   // Field names are deliberately literal ASCII: escaped aliases fail closed.
   while(p<object.end&&*p!='"'){if(*p=='\\')return false;++p;}
   if(p==object.end)return false;const size_t length=size_t(p-key);++p;space(p,object.end);
   if(p==object.end||*p++!=':')return false;space(p,object.end);const char* begin=p;
   if(!skip(p,object.end))return false;
   if(length==strlen(name)&&!memcmp(key,name,length)){if(found)return false;value={begin,p};found=true;}
   space(p,object.end);if(p==object.end)return false;
   if(*p==','){++p;space(p,object.end);}else if(*p!='}')return false;
 }
 return found&&p<object.end&&*p=='}'&&p+1==object.end;
}
inline bool equal(Span value,const char* text){return value.begin&&size_t(value.end-value.begin)==strlen(text)&&!memcmp(value.begin,text,strlen(text));}
}

// Mutates ONLY a disposable staging copy. The caller must not publish/save it
// unless this returns true. On failure, even an already parsed prefix is discarded.
template<class Yield>
bool mergeStockDailyResponse(const char* body,size_t length,const char* ticker,
                            uint32_t today,StockBar* staged,size_t capacity,size_t& count,
                            const char*& error,Yield yield,ArduinoJson::Allocator* allocator=nullptr){
 using namespace stock_json_detail;
 error="BAD DAILY JSON";
 if(!body||!length||!ticker||!staged||count>capacity||!validStockDate(today))return false;
 JsonDocument doc,filter;if(allocator)doc=JsonDocument(allocator);
 filter["rc"]=true;filter["data"]["code"]=true;
 if(deserializeJson(doc,body,length,DeserializationOption::NestingLimit(8),DeserializationOption::Filter(filter)))return false;
 const char* p=body;space(p,body+length);const char* start=p;
 if(!skip(p,body+length))return false;Span root{start,p};space(p,body+length);if(p!=body+length)return false;
 Span rc,data,code,rows;
 if(!member(root,"rc",rc)||!equal(rc,"0")||!member(root,"data",data)||!member(data,"code",code)||!member(data,"klines",rows))return false;
 const size_t codeLength=size_t(code.end-code.begin),tickerLength=strlen(ticker);
 if(codeLength!=tickerLength+2||code.begin[0]!='"'||code.end[-1]!='"'||memcmp(code.begin+1,ticker,tickerLength)){error="WRONG STOCK CODE";return false;}
 if(rows.begin==rows.end||*rows.begin!='['||rows.end[-1]!=']')return false;
 p=rows.begin+1;const char* end=rows.end-1;const uint32_t last=count?staged[count-1].date:0;
 uint32_t previous=0;size_t applied=0,parsed=0;space(p,end);
 while(p<end){
   if(*p++!='"')return false;const char* text=p;while(p<end&&*p!='"')++p;
   if(p==end)return false;StockBar bar;
   if(!parseStockRow(text,size_t(p-text),bar)||bar.date<=previous){error="BAD DAILY ROW";return false;}
   previous=bar.date;
   if(bar.date>=last&&bar.date<today){
     if(count&&bar.date==staged[count-1].date)staged[count-1]=bar;
     else {if(count==capacity){error="DAILY CAPACITY";return false;}staged[count++]=bar;}
     ++applied;
   }
   ++p;space(p,end);
   if(p<end){if(*p++!=',')return false;space(p,end);if(p==end)return false;}
   if(++parsed%64==0)yield();
 }
 if(!applied){error="NO ELIGIBLE DAILY ROWS";return false;}
 error="OK";return true;
}
}

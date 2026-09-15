#pragma once
#include <array>
#include <string>
#include <cstddef>

namespace aurageek::services {
// Boot-time web preferences; range selection remains an on-device gesture.
struct StockPreferences {
  static constexpr size_t capacity=6;
  std::array<std::string,capacity> symbols{{"105.QQQ","107.VOO"}};
  unsigned count=2,maFast=20,maSlow=55;
  bool showFast=true,showSlow=true;
  static bool validSymbol(const std::string& id){
    const auto dot=id.find('.');if(dot==std::string::npos)return false;
    const auto market=id.substr(0,dot),code=id.substr(dot+1);
    const bool numeric=market=="0"||market=="1"||market=="116";
    if(!numeric&&market!="105"&&market!="106"&&market!="107")return false;
    if(numeric){if(code.size()!=(market=="116"?5u:6u))return false;for(char c:code)if(c<'0'||c>'9')return false;return true;}
    if(code.empty()||code.size()>10)return false;
    auto alphaNum=[](char c){return(c>='A'&&c<='Z')||(c>='0'&&c<='9');};
    if(!alphaNum(code.front())||!alphaNum(code.back()))return false;
    for(size_t i=0;i<code.size();++i){if(!alphaNum(code[i])&&code[i]!='.'&&code[i]!='-')return false;if(i&& !alphaNum(code[i])&&!alphaNum(code[i-1]))return false;}
    return true;
  }
  static std::string ticker(const std::string& id){return id.substr(id.find('.')+1);}
  static const char* currency(const std::string& id){return id.compare(0,4,"116.")==0?"HKD":id.compare(0,2,"0.")==0||id.compare(0,2,"1.")==0?"CNY":"USD";}
  static int seedIndex(const std::string& id){return id=="105.QQQ"?0:id=="107.VOO"?1:-1;}
  bool valid(std::string& error)const{
    if(count<1||count>capacity){error="自选列表须包含1到6个标的";return false;}
    for(unsigned i=0;i<count;++i){if(!validSymbol(symbols[i])){error="标的须为支持的东方财富secid，例如105.QQQ、107.VOO、1.600519或116.00700";return false;}for(unsigned j=0;j<i;++j)if(symbols[i]==symbols[j]){error="自选列表不能重复";return false;}}
    if(maFast<2||maFast>120||maSlow<3||maSlow>250||maFast>=maSlow){error="短均线须为2到120日，长均线须为3到250日，且短周期小于长周期";return false;}
    return true;
  }
};
}

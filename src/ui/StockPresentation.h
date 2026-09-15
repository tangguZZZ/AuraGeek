#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace aurageek::ui {
// Keep technical source diagnostics in the serial log; concise, known text on LCD.
inline const char* stockStatusText(const char* status,bool hasData){
 if(strstr(status,"PROVIDER PAUSED"))return "接口拒绝 / 已暂停请求";
 if(strstr(status,"RATE LIMITED"))return "请求受限 / 稍后再试";
 if(strstr(status,"BUDGET"))return "调用记录异常 / 暂不请求";
 if(strstr(status,"NO MEMORY"))return "内存不足 / 暂不更新";
 if(strstr(status,"SIMULATED"))return "模拟数据 / 界面测试";
 if(strstr(status,"REF CACHE"))return "参考快照 / 非实时";
 if(strstr(status,"RAM ONLY"))return "日线已更新 / 未存入缓存";
 if(strstr(status,"EM RAW / DAILY"))return "日线已更新 / 不复权";
 if(strstr(status,"EM RAW / CACHED"))return "缓存日线 / 非实时";
 if(hasData)return "更新未完成 / 保留缓存";
 if(strstr(status,"WAITING"))return "正在读取本地日线";
 if(strstr(status,"OFFLINE"))return "暂无数据 / 等待联网";
 return "暂无数据 / 检查标的编号";
}
inline void formatStockAxis(char* dst,size_t size,float value,float span){
 if(!std::isfinite(value)||!std::isfinite(span)){snprintf(dst,size,"--");return;}
 float divisor=1;const char* suffix="";
 if(std::abs(value)>=1000000){divisor=1000000;suffix="M";}
 else if(std::abs(value)>=10000){divisor=1000;suffix="k";}
 const float step=std::max(span/2/divisor,.00001f);
 const int decimals=std::clamp(int(std::ceil(-std::log10(step)))+1,0,4);
 snprintf(dst,size,"%.*f%s",decimals,value/divisor,suffix);
}
}

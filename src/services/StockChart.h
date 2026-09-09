#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace aurageek { namespace services {
struct StockBar { uint32_t date; float close; };
struct StockChart {
  static constexpr size_t capacity=280;
  float close[capacity]{}, ma20[capacity]{}, ma55[capacity]{};
  size_t count=0, total=0, visible=0;
  float first=0, last=0, low=0, high=0, change=0;
  uint32_t firstDate=0,lastDate=0;
  char ticker[8]="QQQ", status[64]="WAITING FOR DAILY DATA";
};
// Compute moving averages on original daily bars BEFORE range selection/downsampling.
// NaN means insufficient history, never a fabricated zero-price MA.
inline void makeStockChart(const StockBar* bars,size_t count,unsigned range,StockChart& out) {
  out.count=out.total=out.visible=0;
  if(!bars||!count)return;
  const unsigned months[]={6,12,36,60,0}; range=std::min(range,4u);
  uint32_t end=bars[count-1].date;int year=end/10000,month=end/100%100,day=end%100;
  int monthIndex=year*12+month-1-int(months[range]);
  uint32_t cutoff=(monthIndex/12)*10000+(monthIndex%12+1)*100+day;
  size_t begin=0;if(months[range])while(begin+1<count&&bars[begin].date<cutoff)++begin;
  out.total=count;out.visible=count-begin;out.count=std::min(out.visible,StockChart::capacity);
  out.first=bars[begin].close;out.last=bars[count-1].close;
  out.change=(out.last/out.first-1.f)*100.f;
  out.firstDate=bars[begin].date;out.lastDate=bars[count-1].date;
  out.low=out.high=bars[begin].close;
  double sum20=0,sum55=0;size_t target=begin,j=0;
  for(size_t i=0;i<count;++i){
    sum20+=bars[i].close;sum55+=bars[i].close;
    if(i>=20)sum20-=bars[i-20].close;if(i>=55)sum55-=bars[i-55].close;
    if(i<begin)continue;
    out.low=std::min(out.low,bars[i].close);out.high=std::max(out.high,bars[i].close);
    if(i==target&&j<out.count){
      out.close[j]=bars[i].close;out.ma20[j]=i>=19?float(sum20/20):NAN;out.ma55[j]=i>=54?float(sum55/55):NAN;
      if(std::isfinite(out.ma20[j])){out.low=std::min(out.low,out.ma20[j]);out.high=std::max(out.high,out.ma20[j]);}
      if(std::isfinite(out.ma55[j])){out.low=std::min(out.low,out.ma55[j]);out.high=std::max(out.high,out.ma55[j]);}
      ++j;if(j<out.count)target=begin+(j*(out.visible-1))/(out.count-1);
    }
  }
}
}}

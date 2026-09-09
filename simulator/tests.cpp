#include "services/StockChart.h"
#include "services/StockSeed.h"
#include "services/SpectrumAnalyzer.h"
#include <cassert>
#include <cstdio>
using namespace aurageek::services;
int main(){
 StockChart c;StockBar flat[100];for(unsigned i=0;i<100;++i)flat[i]={20200101+i,10.f};
 makeStockChart(flat,100,4,c);assert(c.count==100&&c.change==0&&c.ma20[19]==10&&c.ma55[54]==10);assert(std::isnan(c.ma20[18])&&std::isnan(c.ma55[53]));
 makeStockChart(flat,1,4,c);assert(c.count==1&&c.change==0);makeStockChart(nullptr,0,4,c);assert(c.count==0);
 for(unsigned range=0;range<5;++range){makeStockChart(seedQQQ,seedQQQCount,range,c);assert(c.total==seedQQQCount&&c.count>100&&c.lastDate==seedQQQ[seedQQQCount-1].date);assert(std::fabs(c.last-seedQQQ[seedQQQCount-1].close)<.001f);assert(c.close[c.count-1]==c.last);assert(c.low<=c.high);}
 SpectrumAnalyzer fft;int16_t pcm[512]{};fft.process(pcm);for(unsigned i=0;i<48;++i)assert(fft.bands()[i]==0);
 for(unsigned i=0;i<512;++i)pcm[i]=int16_t(10000*sin(6.28318530718*1000*i/48000));
 for(int n=0;n<10;++n)fft.process(pcm);float peak=0;unsigned band=0;for(unsigned i=0;i<48;++i){assert(std::isfinite(fft.bands()[i]));if(fft.bands()[i]>peak){peak=fft.bands()[i];band=i;}}
 assert(peak>.5f&&band>=18&&band<=23);assert(std::fabs(fft.peakHz()-1031.25f)<.01f);
 for(unsigned i=0;i<512;++i)pcm[i]=int16_t(12000*sin(6.28318530718*3000*i/48000));fft.process(pcm);assert(std::fabs(fft.peakHz()-3000.f)<.01f);
 for(int n=0;n<80;++n){int16_t silence[512]{};fft.process(silence);}for(unsigned i=0;i<48;++i)assert(fft.bands()[i]<.001f);assert(fft.peakHz()==0);
 printf("PASS stock ranges/endpoints/MA warmup + FFT 1kHz/3kHz/silence/decay\n");
}

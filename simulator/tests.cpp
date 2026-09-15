#include "services/StockChart.h"
#include "services/StockSeed.h"
#include "services/SpectrumAnalyzer.h"
#include "ui/MusicMotion.h"
#include "ui/MenuMotion.h"
#include "ui/UserSettings.h"
#include "drivers/AudioLevel.h"
#include <cassert>
#include <cstdio>
using namespace aurageek::services;
int main(){
 using aurageek::ui::UserSettings;
 UserSettings cfg;assert(cfg.valid());cfg.version=3;assert(!cfg.valid());cfg={};cfg.brightness=0;assert(!cfg.valid());cfg={};cfg.volume=101;assert(!cfg.valid());cfg={};cfg.spectrum=2;assert(!cfg.valid());cfg={};cfg.transition=2;assert(!cfg.valid());
 bool migrated=false;uint8_t bytes[]={1,75,200,1,1};
 assert(UserSettings::decode(bytes,5,cfg,migrated)&&migrated&&cfg.volume==100&&cfg.brightness==75&&cfg.transition==1&&cfg.spectrum==1);
 bytes[0]=2;bytes[2]=0;assert(UserSettings::decode(bytes,5,cfg,migrated)&&!migrated&&cfg.volume==0);
 auto beforeCfg=cfg;bytes[2]=101;assert(!UserSettings::decode(bytes,5,cfg,migrated)&&cfg==beforeCfg);
 bytes[2]=50;bytes[0]=3;assert(!UserSettings::decode(bytes,5,cfg,migrated));bytes[0]=2;
 assert(!UserSettings::decode(bytes,4,cfg,migrated)&&!UserSettings::decode(nullptr,5,cfg,migrated));
 bytes[1]=9;assert(!UserSettings::decode(bytes,5,cfg,migrated));bytes[1]=101;assert(!UserSettings::decode(bytes,5,cfg,migrated));
 using namespace aurageek::drivers;
 assert(volumeGainQ15(0)==0&&volumeGainQ15(50)==8192&&volumeGainQ15(100)==32768&&volumeGainQ15(999)==32768);
 for(unsigned v=0;v<=100;++v){int g=volumeGainQ15(v);assert(g>=0&&g<=32768);if(v)assert(g>=volumeGainQ15(v-1));
  assert(applyVolumeSample(32767,g,v)>=0);assert(applyVolumeSample(-32768,g,v)<=0);}
 int g=32768;for(int i=0;i<256;++i){int before=g;applyVolumeSample(32767,g,0);assert(g>=0&&before-g<=128);}assert(g==0&&applyVolumeSample(-32768,g,0)==0);
 for(int i=0;i<256;++i)applyVolumeSample(32767,g,100);assert(g==32768&&applyVolumeSample(32767,g,100)==32767&&applyVolumeSample(-32768,g,100)==-32768);
 printf("PASS settings v1 migration / v2 bounds / volume taper / ramp / unity and mute\n");
 // Rapid one-detent reversals must remain between the current position and target.
 float pos=0,velocity=0,target=0;
 for(int turn=0;turn<1000;++turn){
   target=turn%2?0.f:1.f;
   for(int frame=0;frame<1+turn%7;++frame){
     const float before=pos;
     aurageek::ui::stepMenuMotion(pos,target,velocity,frame%2?.016f:.087f);
     assert(pos>=std::min(before,target)&&pos<=std::max(before,target));
   }
 }
 for(int frame=0;frame<100;++frame)aurageek::ui::stepMenuMotion(pos,0,velocity,.016f);
 assert(pos==0&&velocity==0);
 // Stop within a bounded animation interval, including a backlog of four
 // detents; old momentum must not add a tail or overshoot in either direction.
 for(float distance:{-4.f,-1.f,1.f,4.f})for(float dt:{.008f,.016f,.033f,.087f}){
   pos=0;velocity=100.f;float elapsed=0;
   while(pos!=distance&&elapsed<.3f){
     const float before=pos;aurageek::ui::stepMenuMotion(pos,distance,velocity,dt);elapsed+=dt;
     assert(pos>=std::min(before,distance)&&pos<=std::max(before,distance));assert(velocity==0);
   }
   assert(pos==distance&&elapsed<=.27f);
 }
 // Latest target wins under repeated reversals; no delayed steps after settling.
 pos=0;target=0;velocity=0;
 for(int n=0;n<1000;++n){target+=n%2?-1.f:1.f;aurageek::ui::stepMenuMotion(pos,target,velocity,.016f);}
 for(int n=0;n<20;++n)aurageek::ui::stepMenuMotion(pos,target,velocity,.016f);
 assert(pos==target&&target==0);
 for(int n=0;n<100;++n){aurageek::ui::stepMenuMotion(pos,target,velocity,.033f);assert(pos==target);}
 puts("PASS menu braking: one/four detents settle <=270ms at 8..87ms steps, no overshoot, 1000 reversals");
 aurageek::ui::MusicMotion motion;float input[48]{},art[104]{};
 motion.update(input,true,.033f,1.f,art);for(float v:art)assert(v==0);
 input[0]=.8f;motion.update(input,true,.033f,2.f,art);
 float right=0;for(unsigned i=0;i<104;++i){assert(std::isfinite(art[i])&&art[i]>=0&&art[i]<=1);if(i>75)right=std::max(right,art[i]);}
 assert(right>.1f); // bass-only music still produces full-width visual composition
 motion.update(input,false,.033f,3.f,art);for(float v:art)assert(v==0);
 StockChart c;StockBar flat[100];for(unsigned i=0;i<100;++i)flat[i]={20200101+i,10.f};
 makeStockChart(flat,100,4,c);assert(c.count==100&&c.change==0&&c.ma20[19]==10&&c.ma55[54]==10);assert(std::isnan(c.ma20[18])&&std::isnan(c.ma55[53]));
 makeStockChart(flat,1,4,c);assert(c.count==1&&c.change==0);makeStockChart(nullptr,0,4,c);assert(c.count==0);
 for(unsigned range=0;range<5;++range){makeStockChart(seedQQQ,seedQQQCount,range,c);assert(c.total==seedQQQCount&&c.count>100&&c.lastDate==seedQQQ[seedQQQCount-1].date);assert(std::fabs(c.last-seedQQQ[seedQQQCount-1].close)<.001f);assert(c.close[c.count-1]==c.last);assert(c.low<=c.high);}
 SpectrumAnalyzer fft;int16_t pcm[512]{};fft.process(pcm);for(unsigned i=0;i<48;++i)assert(fft.bands()[i]==0);
 for(unsigned i=0;i<512;++i)pcm[i]=int16_t(10000*sin(6.28318530718*1000*i/48000));
 for(int n=0;n<10;++n)fft.process(pcm);float peak=0;unsigned band=0;for(unsigned i=0;i<48;++i){assert(std::isfinite(fft.bands()[i]));if(fft.bands()[i]>peak){peak=fft.bands()[i];band=i;}}
 assert(peak>.5f&&SpectrumAnalyzer::bandEdge(band)<=11&&SpectrumAnalyzer::bandEdge(band+1)>11);assert(std::fabs(fft.peakHz()-1031.25f)<.01f);
 for(unsigned b=0;b<48;++b)assert(SpectrumAnalyzer::bandEdge(b+1)>SpectrumAnalyzer::bandEdge(b));
 assert(SpectrumAnalyzer::bandEdge(0)==1&&SpectrumAnalyzer::bandEdge(48)==129);
 for(unsigned i=0;i<512;++i)pcm[i]=int16_t(12000*sin(6.28318530718*3000*i/48000));fft.process(pcm);assert(std::fabs(fft.peakHz()-3000.f)<.01f);
 for(int n=0;n<80;++n){int16_t silence[512]{};fft.process(silence);}for(unsigned i=0;i<48;++i)assert(fft.bands()[i]<.001f);assert(fft.peakHz()==0);
 printf("PASS stock ranges/endpoints/MA warmup + FFT 1kHz/3kHz/silence/decay\n");
}

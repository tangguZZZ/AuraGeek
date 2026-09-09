#include <SDL.h>
#include <lvgl.h>
#include <demos/lv_demos.h>
#include <cstdio>
#include <cstring>
#include "ui/AuraUi.h"
#include "services/SpectrumAnalyzer.h"
#include "services/StockSeed.h"
#include <ctime>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

using aurageek::ui::AuraUi;
static std::atomic<int> rotation{0}, action{0};
static int watch(void*,SDL_Event* e){
 if(e->type==SDL_MOUSEWHEEL)rotation+=e->wheel.y>0?1:-1;
 if(e->type==SDL_KEYDOWN){switch(e->key.keysym.sym){case SDLK_LEFT:rotation--;break;case SDLK_RIGHT:rotation++;break;case SDLK_RETURN:action=1;break;case SDLK_ESCAPE:action=2;break;default:break;}}
 if(e->type==SDL_MOUSEBUTTONUP){if(e->button.button==SDL_BUTTON_RIGHT||e->button.clicks==2)action=2;else if(e->button.button==SDL_BUTTON_MIDDLE)action=1;}
 return 1;
}
static bool screenshot(const char* name){
 auto* b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_ARGB8888);if(!b)return false;
 auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(b->data,b->header.w,b->header.h,32,b->header.stride,SDL_PIXELFORMAT_ARGB8888);
 bool ok=surface && SDL_SaveBMP(surface,name)==0; if(surface)SDL_FreeSurface(surface);lv_draw_buf_destroy(b);return ok;
}

int main(int argc, char** argv) {
  SDL_SetMainReady();
  lv_init();
  auto* display = lv_sdl_window_create(320, 240);
  lv_sdl_window_set_title(display, "AuraGeek | wheel: menu | click: select | right-click: back");
  lv_sdl_window_set_zoom(display, 2);
  lv_sdl_mouse_create();
  const bool official=argc>1&&!strcmp(argv[1],"--official");
  AuraUi ui;if(official)lv_demo_widgets();else ui.create();SDL_AddEventWatch(watch,nullptr);
  aurageek::services::SpectrumAnalyzer fft;
  const bool smoke = argc > 1 && !strcmp(argv[1], "--smoke");
  auto start = SDL_GetTicks();
  uint32_t last=0;int stage=-1;
  std::vector<aurageek::services::StockBar> stocks[2];
  for(int i=0;i<2;i++){std::ifstream file(std::string("../../cache/")+(i?"VOO.csv":"QQQ.csv"));std::string row;while(std::getline(file,row)){auto comma=row.find(',');if(comma==std::string::npos)continue;try{float v=std::stof(row.substr(comma+1));unsigned y,m,d;if(sscanf(row.c_str(),"%u-%u-%u",&y,&m,&d)==3&&std::isfinite(v)&&v>0)stocks[i].push_back({y*10000+m*100+d,v});}catch(...){}}}
  while (!smoke || SDL_GetTicks() - start < 13200) {
    int r=rotation.exchange(0),a=action.exchange(0);if(!official){if(r)ui.rotate(r);if(a==1)ui.click();if(a==2)ui.back();}
    uint32_t now=SDL_GetTicks();if(!official&&now-last>=40){last=now;time_t epoch=time(nullptr);tm* local=localtime(&epoch);char clock[16],date[48];strftime(clock,sizeof(clock),"%H:%M",local);strftime(date,sizeof(date),"%a, %d %b %Y",local);ui.setNetwork(true,false,-40);ui.setClock(true,clock,date);ui.setWeather(true,"少云  29℃",2);
      ui.setEpoch(uint32_t(epoch));auto index=ui.stockIndex();aurageek::services::StockChart chart;
      using namespace aurageek::services;
      const bool reference=stocks[index].empty();makeStockChart(reference?(index?seedVOO:seedQQQ):stocks[index].data(),reference?(index?seedVOOCount:seedQQQCount):stocks[index].size(),ui.stockRange(),chart);
      snprintf(chart.ticker,sizeof(chart.ticker),"%s",index?"VOO":"QQQ");snprintf(chart.status,sizeof(chart.status),reference?"REF CACHE / OFFLINE":"EM RAW / CACHED");ui.setStock(chart);
      if(ui.page()==AuraUi::Page::Spectrum){int16_t pcm[512];for(int i=0;i<512;i++){double t=(now*48.+i)/48000.;pcm[i]=int16_t(10000*sin(6.2831853*440*t)+6000*sin(6.2831853*2200*t)*(0.5+0.5*sin(now/300.)));}fft.process(pcm);ui.setSpectrum(fft.bands(),48,true,true);}
    }
    if(smoke){
      int next=(now-start)/1200;
      if(next!=stage){
        stage=next;
        switch(stage){
          case 0:break;
          case 1:ui.rotate(1);break;
          case 2:ui.rotate(1);ui.click();break;
          case 3:ui.setStockRange(2);break;
          case 4:ui.click();break;
          case 5:ui.back();break;
          case 6:ui.rotate(1);ui.click();break;
          case 7:ui.back();break;
          case 8:ui.rotate(1);ui.click();break;
          case 9:ui.back();break;
          case 10:ui.back();break;
          default:break;
        }
      }
      const AuraUi::Page expected[]={AuraUi::Page::Home,AuraUi::Page::Menu,AuraUi::Page::Stocks,AuraUi::Page::Stocks,AuraUi::Page::Stocks,AuraUi::Page::Menu,AuraUi::Page::Spectrum,AuraUi::Page::Menu,AuraUi::Page::Agent,AuraUi::Page::Menu,AuraUi::Page::Home};
      if(stage<11&&(now-start)%1200>=500&&(now-start)%1200<545&&ui.page()!=expected[stage]){fprintf(stderr,"FAIL navigation stage %d\n",stage);return 2;}
      if((now-start)%1200>=900&&(now-start)%1200<945){std::filesystem::create_directories("screenshots");char path[80];snprintf(path,sizeof(path),"screenshots/page-%d.bmp",stage);if(!screenshot(path)){fprintf(stderr,"FAIL screenshot %d\n",stage);return 3;}}
    }
    lv_timer_handler();
    SDL_Delay(5);
  }
  printf("PASS simulator replay: home/menu/QQQ-all/QQQ-3y/VOO-3y/menu/spectrum/menu/agent/menu/home\n");SDL_DelEventWatch(watch,nullptr);
  lv_deinit();
  return 0;
}

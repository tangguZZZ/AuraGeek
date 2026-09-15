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
 if(e->type==SDL_KEYDOWN){switch(e->key.keysym.sym){case SDLK_LEFT:rotation--;break;case SDLK_RIGHT:rotation++;break;case SDLK_RETURN:action=1;break;case SDLK_ESCAPE:action=2;break;case SDLK_d:action=3;break;case SDLK_HOME:action=4;break;default:break;}}
 if(e->type==SDL_MOUSEBUTTONUP){if(e->button.clicks==2)action=3;else if(e->button.button==SDL_BUTTON_RIGHT)action=2;else if(e->button.button==SDL_BUTTON_MIDDLE)action=1;}
 return 1;
}
static bool screenshot(const char* name){
 auto* b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_ARGB8888);if(!b)return false;
 auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(b->data,b->header.w,b->header.h,32,b->header.stride,SDL_PIXELFORMAT_ARGB8888);
 bool ok=surface && SDL_SaveBMP(surface,name)==0; if(surface)SDL_FreeSurface(surface);lv_draw_buf_destroy(b);return ok;
}
static lv_obj_t* findLabel(lv_obj_t* root,const char* text){
 if(lv_obj_check_type(root,&lv_label_class)&&!strcmp(lv_label_get_text(root),text))return root;
 for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* found=findLabel(lv_obj_get_child(root,i),text))return found;
 return nullptr;
}

int main(int argc, char** argv) {
  SDL_SetMainReady();
  lv_init();
  auto* display = lv_sdl_window_create(320, 240);
  if(!display){fprintf(stderr,"SDL display unavailable: %s\n",SDL_GetError());return 7;}
  lv_sdl_window_set_title(display, "AuraGeek | wheel: menu | click: select | right-click: back");
  lv_sdl_window_set_zoom(display, 2);
  lv_sdl_mouse_create();
  const bool official=argc>1&&!strcmp(argv[1],"--official");
  AuraUi ui;if(official)lv_demo_widgets();else ui.create();SDL_AddEventWatch(watch,nullptr);
  if(argc>1&&!strcmp(argv[1],"--stock-config")){
    aurageek::services::StockPreferences pref;pref.count=6;pref.symbols={{"105.QQQ","107.VOO","1.600519","0.000001","116.00700","106.WWWWWWWWWW"}};pref.maFast=5;pref.maSlow=120;pref.showSlow=false;
    ui.configureStocks(pref);ui.settings.transition=0;ui.show(AuraUi::Page::Stocks);std::filesystem::create_directories("screenshots");
    for(unsigned i=0;i<12;++i){
      const unsigned index=i%pref.count;if(ui.stockIndex()!=index)return 40;
      const auto code=aurageek::services::StockPreferences::ticker(pref.symbols[index]);
      auto* title=findLabel(lv_screen_active(),code.c_str());if(!title)return 41;
      lv_obj_update_layout(lv_screen_active());lv_point_t measured{};lv_text_get_size(&measured,code.c_str(),lv_obj_get_style_text_font(title,LV_PART_MAIN),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
      if(measured.x>lv_obj_get_width(title)||lv_obj_get_height(title)>24){fprintf(stderr,"FAIL %s text=%ld width=%ld height=%ld\n",code.c_str(),long(measured.x),long(lv_obj_get_width(title)),long(lv_obj_get_height(title)));return 42;}
      char currency[24];snprintf(currency,sizeof(currency),"日线 %s",aurageek::services::StockPreferences::currency(pref.symbols[index]));if(!findLabel(lv_screen_active(),currency))return 43;
      if(!findLabel(lv_screen_active(),"MA5")||findLabel(lv_screen_active(),"MA120"))return 44;
      char shot[100];snprintf(shot,sizeof(shot),"screenshots/stock-custom-%u.bmp",index);if(!screenshot(shot))return 45;
      ui.click();
    }
    pref.count=1;pref.symbols[0]="116.00700";ui.configureStocks(pref);ui.click();if(ui.stockIndex()!=0)return 46;
    // Deliberately synthetic data, never fetched from a provider or shown as a real quote.
    aurageek::services::StockChart chart;strcpy(chart.ticker,"00700");strcpy(chart.currency,"HKD");strcpy(chart.status,"SIMULATED / UI TEST");
    std::vector<aurageek::services::StockBar> series;for(unsigned i=0;i<180;++i)series.push_back({20260101+(i/28)*100+i%28,100.f+i*.5f+std::sin(i*.12f)*8.f});
    aurageek::services::makeStockChart(series.data(),series.size(),4,chart);ui.setStock(chart);lv_obj_update_layout(lv_screen_active());
    if(!findLabel(lv_screen_active(),"全区间涨跌")||!findLabel(lv_screen_active(),"模拟数据 / 界面测试")||!screenshot("screenshots/stock-audit-data.bmp"))return 47;
    // A detent must not expose a no-data frame or label old data with a new range.
    const char* scopes[]={"半年区间涨跌","1年区间涨跌","3年区间涨跌","5年区间涨跌","全区间涨跌"};
    auto hasChart=[&](){auto* empty=findLabel(lv_screen_active(),"暂无日线数据");return empty&&lv_obj_has_flag(empty,LV_OBJ_FLAG_HIDDEN);};
    unsigned displayed=4;
    for(unsigned n=0;n<100;++n){
      ui.rotate(n%2?-1:1);
      ui.setClock(true,"16:29","Sat, 12 Sep 2026"); // unrelated updates while pending
      lv_timer_handler();
      if(!hasChart()||!findLabel(lv_screen_active(),scopes[displayed]))return 50;
      aurageek::services::makeStockChart(series.data(),series.size(),ui.stockRange(),chart);
      ui.setStock(chart);displayed=ui.stockRange();
      if(ui.stockRangePending()||!hasChart()||!findLabel(lv_screen_active(),scopes[displayed]))return 51;
    }
    ui.rotate(1);ui.rotate(-1);if(ui.stockRangePending()||!hasChart())return 52;
    // Identical short-history snapshots still need to commit their range label.
    aurageek::services::makeStockChart(series.data(),1,4,chart);ui.setStock(chart);
    for(unsigned r=0;r<5;++r){ui.setStockRange(r);ui.setStock(chart);if(ui.stockRangePending()||!hasChart()||!findLabel(lv_screen_active(),scopes[r]))return 53;}
    ui.setStockRange(99);ui.setStock(chart);if(ui.stockRange()!=4||ui.stockRangePending())return 54;
    puts("PASS 100 range changes: old chart retained, atomic range labels, reverse cancellation, identical snapshots");
    for(auto& bar:series)bar.close=.12f;aurageek::services::makeStockChart(series.data(),series.size(),4,chart);ui.setStock(chart);lv_obj_update_layout(lv_screen_active());if(!screenshot("screenshots/stock-audit-flat.bmp"))return 48;
    chart.count=0;strcpy(chart.status,"CACHE / PROVIDER PAUSED");ui.setStock(chart);lv_obj_update_layout(lv_screen_active());if(!findLabel(lv_screen_active(),"接口拒绝 / 已暂停请求")||!screenshot("screenshots/stock-audit-paused.bmp"))return 49;
    puts("PASS stock UI: six symbols cycle twice, currency, long ticker fits, MA preferences, one-symbol wrap");return 0;
  }
  if(argc>1&&!strcmp(argv[1],"--weather-layout")){
    const uint32_t themes[][3]={{0x050505,0xf2f2f2,0xdfff00},{0x071718,0xe8f8f3,0x6cf0ce},{0xeef0ea,0x243339,0x3b6500},{0x120c1f,0xf4edff,0xcfafff}};
    const char* samples[]={"晴  9℃","少云  29℃","毛毛雨  30℃","冻毛毛雨  -40℃","雷暴  50℃","未知 --℃"};
    std::filesystem::create_directories("screenshots");
    for(unsigned theme=0;theme<4;++theme)for(unsigned sample=0;sample<6;++sample){
      ui.configureHome(themes[theme][0],themes[theme][1],themes[theme][2],"SHENZHEN");
      ui.setNetwork(true,false,-40);ui.setClock(true,"16:29","Sat, 12 Sep 2026");ui.setWeather(true,samples[sample],55);ui.setIndoor(true,27.2f,53.f);
      lv_obj_update_layout(lv_screen_active());
      auto* label=findLabel(lv_screen_active(),samples[sample]);if(!label)return 30;
      lv_area_t area{};lv_obj_get_coords(label,&area);lv_point_t textSize{};
      const auto* font=lv_obj_get_style_text_font(label,LV_PART_MAIN);lv_text_get_size(&textSize,samples[sample],font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
      if(lv_label_get_long_mode(label)!=LV_LABEL_LONG_CLIP||area.x1<152||area.x2!=313||area.y2>=30||lv_obj_get_height(label)!=font->line_height||textSize.x>lv_obj_get_width(label)){fprintf(stderr,"FAIL weather theme=%u sample=%u width=%ld text=%ld\n",theme,sample,long(lv_obj_get_width(label)),long(textSize.x));return 31;}
      char path[100];snprintf(path,sizeof(path),"screenshots/weather-theme-%u-case-%u.bmp",theme,sample);if(!screenshot(path))return 32;
    }
    puts("PASS weather layout: 24 theme/condition cases, single line, full text, fixed right edge, no city overlap");return 0;
  }
  aurageek::services::SpectrumAnalyzer fft;
  const bool smoke = argc > 1 && !strcmp(argv[1], "--smoke");
  if(smoke)ui.settings.transition=1; // Exercise queued navigation in optional BLEND too.
  const bool stress = argc > 1 && !strcmp(argv[1], "--stress");
  const bool settingsTest=argc>1&&!strcmp(argv[1],"--settings-test");
  const bool agentTest=argc>1&&!strcmp(argv[1],"--agent-test");
  const bool portalTest=argc>1&&!strcmp(argv[1],"--portal-ui");
  int portalStage=-1;
  int agentStage=-1,agentShot=-1;
  unsigned agentFadeSamples=0,agentFadeShots=0;
  int settingsStage=-1;
  auto start = SDL_GetTicks();
  uint32_t last=0;int stage=-1;
  size_t warmFree=0;int stressStage=-1;
  std::vector<aurageek::services::StockBar> stocks[2];
  for(int i=0;i<2;i++){std::ifstream file(std::string("../../cache/")+(i?"VOO.csv":"QQQ.csv"));std::string row;while(std::getline(file,row)){auto comma=row.find(',');if(comma==std::string::npos)continue;try{float v=std::stof(row.substr(comma+1));unsigned y,m,d;if(sscanf(row.c_str(),"%u-%u-%u",&y,&m,&d)==3&&std::isfinite(v)&&v>0)stocks[i].push_back({y*10000+m*100+d,v});}catch(...){}}}
  while ((!smoke || SDL_GetTicks() - start < 24000)&&(!stress||SDL_GetTicks()-start<72000)&&(!settingsTest||SDL_GetTicks()-start<19200)&&(!agentTest||SDL_GetTicks()-start<28000)) {
    if(portalTest){
      const int next=(SDL_GetTicks()-start)/1300;
      if(next>=9){puts("PASS portal UI: themes, entry confirmation/cancel, board settings preserved");return 0;}
      if(next!=portalStage){portalStage=next;
        if(next==0)ui.configureHome(0x050505,0xf2f2f2,0xdfff00,"SHENZHEN");
        if(next==1)ui.configureHome(0x071718,0xe8f8f3,0x6cf0ce,"TAIPEI");
        if(next==2)ui.configureHome(0xeef0ea,0x243339,0x3b6500,"SHANGHAI");
        if(next==3)ui.configureHome(0x120c1f,0xf4edff,0xcfafff,"TOKYO");
        if(next==4){ui.show(AuraUi::Page::Settings);ui.rotate(5);}
        if(next==5){ui.click();if(ui.takePortalRequest())return 20;}
        if(next==6){ui.back();if(ui.takePortalRequest())return 21;}
        if(next==7){ui.click();ui.click();if(!ui.takePortalRequest()||ui.takePortalRequest())return 22;}
        if(ui.settings!=aurageek::ui::UserSettings{})return 23;
      }
      if((SDL_GetTicks()-start)%1300>=900&&(SDL_GetTicks()-start)%1300<925){std::filesystem::create_directories("screenshots");char name[96];snprintf(name,sizeof(name),"screenshots/portal-ui-%d.bmp",next);if(!screenshot(name))return 24;}
    }
    int r=rotation.exchange(0),a=action.exchange(0);if(!official){if(r)ui.rotate(r);if(a==1)ui.click();if(a==2)ui.back();if(a==3)ui.doubleClick();if(a==4)ui.home();}
    uint32_t now=SDL_GetTicks();if(!official&&now-last>=40){last=now;time_t epoch=time(nullptr);tm* local=localtime(&epoch);char clock[16],date[48];strftime(clock,sizeof(clock),"%H:%M",local);strftime(date,sizeof(date),"%a, %d %b %Y",local);static const float hourly[24]={27.2f,26.9f,26.6f,26.3f,26.1f,26.0f,26.2f,26.8f,27.7f,28.8f,29.7f,30.5f,31.0f,31.3f,31.1f,30.7f,30.2f,29.8f,29.4f,29.1f,28.8f,28.5f,28.2f,29.0f};ui.setNetwork(true,false,-40);ui.setClock(true,clock,date);ui.setWeather(true,"少云  29℃",2,hourly,24);ui.setIndoor(true,25.6f,63.f);
      ui.setEpoch(uint32_t(epoch));auto index=ui.stockIndex();aurageek::services::StockChart chart;
      using namespace aurageek::services;
      const bool reference=stocks[index].empty();makeStockChart(reference?(index?seedVOO:seedQQQ):stocks[index].data(),reference?(index?seedVOOCount:seedQQQCount):stocks[index].size(),ui.stockRange(),chart);
      snprintf(chart.ticker,sizeof(chart.ticker),"%s",index?"VOO":"QQQ");snprintf(chart.status,sizeof(chart.status),reference?"REF CACHE / OFFLINE":"EM RAW / CACHED");ui.setStock(chart);
      if(ui.page()==AuraUi::Page::Spectrum){int16_t pcm[512];for(int i=0;i<512;i++){double t=(now*48.+i)/48000.;pcm[i]=int16_t(10000*sin(6.2831853*440*t)+6000*sin(6.2831853*2200*t)*(0.5+0.5*sin(now/300.)));}fft.process(pcm);ui.setSpectrum(fft.bands(),48,true,true);}
    }
    if(agentTest){
      const int next=(now-start)/1000;
      const unsigned states[]={0,1,2,3,4,5,0};
      if(next!=agentStage){
        agentStage=next;ui.show(AuraUi::Page::Agent);
        ui.setAgentState(next<7?states[next]:4);ui.setAgentEmotion(next<7?0:next-7);
      }
      const unsigned revision=ui.agentFaceRevision();
      ui.setAgentState(next<7?states[next]:4);ui.setAgentEmotion(next<7?0:next-7);
      if(ui.agentFaceRevision()!=revision){fprintf(stderr,"FAIL repeated status restarts GIF\n");return 10;}
      if(!ui.agentFaceLoaded()){fprintf(stderr,"FAIL GIF decode stage %d\n",next);return 11;}
      // buildAgent creates the GIF first, followed by the status label.
      auto* faceObject=lv_obj_get_child(lv_screen_active(),0);
      auto opacity=lv_obj_get_style_opa(faceObject,LV_PART_MAIN);
      if(opacity>0 && opacity<255)++agentFadeSamples;
      if(next==11){
        const unsigned times[]={30,80,140,230};
        if(agentFadeShots<4 && (now-start)%1000>=times[agentFadeShots]){
          char path[96];snprintf(path,sizeof(path),"screenshots/agent-fade-%u.bmp",agentFadeShots++);
          if(!screenshot(path))return 3;
        }
      }
      if((now-start)%1000>=600 && agentShot!=next){
        agentShot=next;std::filesystem::create_directories("screenshots");char path[96];snprintf(path,sizeof(path),"screenshots/agent-%02d.bmp",next);
        if(opacity!=255){fprintf(stderr,"FAIL face fade did not settle\n");return 13;}
        if(!screenshot(path))return 3;
      }
    }
    if(smoke){
      int next=(now-start)/1200;
      if(next!=stage){
        stage=next;
        switch(stage){
          case 0:break;
          case 1:ui.rotate(1);break;
          case 2:ui.rotate(1);ui.click();break;
          case 3:ui.rotate(-2);break;  // EC11: ALL -> 3Y inside the stock page.
          case 4:ui.click();break;
          case 5:ui.back();break;
          case 6:ui.rotate(1);ui.click();break;
          case 7:ui.doubleClick();break;
          case 8:ui.rotate(1);break;
          case 9:ui.click();break;
          case 10:ui.doubleClick();break;
          case 11:ui.rotate(-1);ui.back();break; // cancel keeps ring
          case 12:ui.back();break;
          case 13:ui.rotate(1);ui.click();break;
          case 14:ui.back();break;
          case 15:ui.home();break;
          case 16:ui.show(AuraUi::Page::Stocks);ui.home();break; // queued navigation
          case 17:ui.show(AuraUi::Page::Spectrum);ui.doubleClick();break; // double during transition
          case 18:ui.doubleClick();break; // cancel after deferred open
          case 19:ui.home();break;
          default:break;
        }
      }
      const AuraUi::Page expected[]={AuraUi::Page::Home,AuraUi::Page::Menu,AuraUi::Page::Stocks,AuraUi::Page::Stocks,AuraUi::Page::Stocks,AuraUi::Page::Menu,AuraUi::Page::Spectrum,AuraUi::Page::Spectrum,AuraUi::Page::Spectrum,AuraUi::Page::Spectrum,AuraUi::Page::Spectrum,AuraUi::Page::Spectrum,AuraUi::Page::Menu,AuraUi::Page::Agent,AuraUi::Page::Menu,AuraUi::Page::Home,AuraUi::Page::Home};
      if(stage<20&&(now-start)%1200>=1000&&(now-start)%1200<1045){
        auto expectedPage=stage<17?expected[stage]:(stage<19?AuraUi::Page::Spectrum:AuraUi::Page::Home);
        if(ui.page()!=expectedPage){fprintf(stderr,"FAIL navigation stage %d\n",stage);return 2;}
        bool picker=stage==7||stage==8||stage==10||stage==17;
        if(ui.spectrumPickerOpen()!=picker){fprintf(stderr,"FAIL picker stage %d\n",stage);return 4;}
        if(stage>=9&&ui.spectrumStyle()!=1){fprintf(stderr,"FAIL style commit/cancel\n");return 5;}
        if(stage==3&&ui.stockRange()!=2){fprintf(stderr,"FAIL stock range regression\n");return 6;}
      }
      if(((now-start)%1200>=900&&(now-start)%1200<920)||((now-start)%1200>=130&&(now-start)%1200<145)){
        std::filesystem::create_directories("screenshots");char path[96];snprintf(path,sizeof(path),"screenshots/page-%d-%s.bmp",stage,(now-start)%1200<200?"transition":"settled");
        if(!screenshot(path)){fprintf(stderr,"FAIL screenshot %d\n",stage);return 3;}
      }
    }
    if(settingsTest){
      int next=(now-start)/1200;
      if(next!=settingsStage){settingsStage=next;
        switch(next){
          case 0:ui.show(AuraUi::Page::Menu);ui.rotate(-1);break;
          case 1:ui.click();break;
          case 2:ui.click();ui.rotate(-20);break;
          case 3:ui.back();if(ui.settings.brightness!=100){fprintf(stderr,"FAIL brightness cancel\n");return 9;}break;
          case 4:ui.click();ui.rotate(-10);ui.click();if(!ui.takeSettingsCommit()||ui.settings.brightness!=90){fprintf(stderr,"FAIL settings commit\n");return 9;}ui.settingsSaveResult(true);break;
          case 5:ui.rotate(1);break;
          case 6:ui.click();ui.rotate(-10);if(ui.settings.volume!=50){fprintf(stderr,"FAIL live volume\n");return 9;}break;
          case 7:ui.back();if(ui.settings.volume!=100){fprintf(stderr,"FAIL volume cancel\n");return 9;}ui.click();ui.rotate(-2147483647-1);if(ui.settings.volume!=0)return 9;ui.rotate(2147483647);if(ui.settings.volume!=100)return 9;ui.rotate(-10);ui.click();if(!ui.takeSettingsCommit()||ui.settings.volume!=50)return 9;ui.settingsSaveResult(true);ui.rotate(1);break;
          case 8:ui.click();break;
          case 9:ui.back();ui.rotate(1);ui.click();ui.rotate(1);break;
          case 10:ui.click();if(ui.settings.spectrum!=1||!ui.takeSettingsCommit()){fprintf(stderr,"FAIL spectrum save\n");return 9;}ui.settingsSaveResult(true);ui.rotate(1);break;
          case 11:ui.click();break;
          case 12:ui.click();if(ui.takeSettingsCommit()){fprintf(stderr,"FAIL readonly system\n");return 9;}ui.home();break;
          case 13:ui.show(AuraUi::Page::Settings);ui.click();ui.rotate(10);break;
          case 14:ui.back();ui.rotate(1);ui.click();ui.rotate(-100);break;
          case 15:ui.show(AuraUi::Page::Home);if(ui.settings.volume!=50||ui.settings.brightness!=90){fprintf(stderr,"FAIL external navigation preview cancel\n");return 9;}break;
        }
      }
      if((now-start)%1200>=900&&(now-start)%1200<930){std::filesystem::create_directories("screenshots");char path[96];snprintf(path,sizeof(path),"screenshots/settings-%02d.bmp",settingsStage);if(!screenshot(path))return 3;}
    }
    if(stress){
      int next=(now-start)/600;
      if(next!=stressStage){
        stressStage=next;
        if(next%12==0){
          lv_mem_monitor_t memory{};lv_mem_monitor(&memory);
          if(next==24)warmFree=memory.free_size;
          if(next>=36&&memory.free_size+4096<warmFree){fprintf(stderr,"FAIL repeated-page memory: %zu -> %zu\n",warmFree,memory.free_size);return 8;}
          printf("cycle=%d free=%zu largest=%zu\n",next/12,memory.free_size,memory.free_biggest_size);
        }
        static const AuraUi::Page pages[]={AuraUi::Page::Home,AuraUi::Page::Menu,AuraUi::Page::Stocks,AuraUi::Page::Spectrum,AuraUi::Page::Agent,AuraUi::Page::Menu};
        ui.show(pages[next%6]);
      }
    }
    lv_timer_handler();
    SDL_Delay(5);
  }
  if(agentTest){
    if(agentFadeSamples==0 || agentFadeShots!=4){fprintf(stderr,"FAIL missing intermediate fade frames\n");return 14;}
    printf("PASS fade intermediate samples=%u and settled opacity\n",agentFadeSamples);
    ui.setAgentState(0);lv_timer_handler();lv_mem_monitor_t before{},after{};lv_mem_monitor(&before);
    for(unsigned i=0;i<100;++i){ui.setAgentState(4);ui.setAgentEmotion(i%21);if(!ui.agentFaceLoaded())return 11;ui.setAgentState(0);lv_timer_handler();}
    lv_mem_monitor(&after);if(after.free_size+4096<before.free_size){fprintf(stderr,"FAIL GIF switch memory loss\n");return 12;}
    printf("PASS 100 rapid face changes; free=%zu -> %zu\n",before.free_size,after.free_size);
    ui.setAgentState(4);ui.setAgentEmotion(4);ui.home();
    for(unsigned i=0;i<60;++i){lv_timer_handler();SDL_Delay(5);}
    if(ui.page()!=AuraUi::Page::Home){fprintf(stderr,"FAIL exit during face fade\n");return 15;}
    puts("PASS exit during face fade");
  }
  printf(agentTest?"PASS agent states / 21 emotion mappings / no repeated GIF restart\n":settingsTest?"PASS settings navigation / cancel / save / spectrum default / exit\n":stress?"PASS 120 transitions and bounded warm memory\n":"PASS 20-stage replay: navigation, ranges, picker, queued Home, double during transition\n");SDL_DelEventWatch(watch,nullptr);
  lv_deinit();
  return 0;
}

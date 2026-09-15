#include "ui/AuraUi.h"
#include "ui/MenuMotion.h"
#include "ui/StockPresentation.h"
#include "ui/AgentAssets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" {
LV_IMAGE_DECLARE(ag_home);
LV_IMAGE_DECLARE(ag_stock);
LV_IMAGE_DECLARE(ag_music);
LV_IMAGE_DECLARE(ag_agent);
LV_IMAGE_DECLARE(ag_weather_unknown);
LV_IMAGE_DECLARE(ag_weather_sunny);
LV_IMAGE_DECLARE(ag_weather_cloudy);
LV_IMAGE_DECLARE(ag_weather_rain);
LV_IMAGE_DECLARE(ag_weather_snow);
LV_IMAGE_DECLARE(ag_weather_thunder);
LV_IMAGE_DECLARE(ag_idle);
LV_IMAGE_DECLARE(ag_face_listen);
LV_IMAGE_DECLARE(ag_face_think);
LV_IMAGE_DECLARE(ag_face_happy);
LV_IMAGE_DECLARE(ag_face_laugh);
LV_IMAGE_DECLARE(ag_face_sad);
LV_IMAGE_DECLARE(ag_face_angry);
LV_IMAGE_DECLARE(ag_face_cry);
LV_IMAGE_DECLARE(ag_face_love);
LV_IMAGE_DECLARE(ag_face_shy);
LV_IMAGE_DECLARE(ag_face_confused);
LV_IMAGE_DECLARE(ag_face_sleep);
LV_IMAGE_DECLARE(ag_face_kiss);
LV_IMAGE_DECLARE(ag_face_wink);
LV_IMAGE_DECLARE(ag_wifi_0);
LV_IMAGE_DECLARE(ag_wifi_1);
LV_FONT_DECLARE(aurageek_font_cn_16);
LV_FONT_DECLARE(stock_font_10);
LV_FONT_DECLARE(stock_font_12);
LV_FONT_DECLARE(stock_font_16);
}

namespace aurageek { namespace ui {
namespace {
constexpr uint32_t kBlack=0x050505,kInk=0xF2F2F2,kDim=0x8A8A8A,kFaint=0x4B4B4B,kLine=0x1E1E1E,kSignal=0xDFFF00;
constexpr uint32_t kPaper=0xEEF0EA,kMenuInk=0x2F3E46,kMenuDim=0x96A3A8,kMenuLine=0xD6DBD3;
constexpr float kPi=3.14159265359f;
constexpr uint32_t kStockFast=0xB2BBC1,kStockSlow=0x6B8995;
lv_obj_t* rect(lv_obj_t* p,int x,int y,int w,int h,uint32_t color,uint8_t opa=255){auto* o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,opa,0);lv_obj_set_style_radius(o,0,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;}
lv_obj_t* label(lv_obj_t* p,const char* s,int x,int y,int w,const lv_font_t* f,uint32_t c){auto* o=lv_label_create(p);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_font(o,f,0);lv_obj_set_style_text_color(o,lv_color_hex(c),0);return o;}
lv_obj_t* image(lv_obj_t* p,const lv_image_dsc_t* src,int x,int y,int size){auto* o=lv_image_create(p);lv_image_set_src(o,src);lv_image_set_pivot(o,0,0);lv_image_set_scale(o,256*size/src->header.w);lv_obj_set_pos(o,x,y);return o;}
void separator(lv_obj_t* p,int x,int y,int w,int h,uint32_t c=kLine){rect(p,x,y,w,h,c);}
void update(lv_obj_t* o,const char* s){if(o&&strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
void fitStockLabel(lv_obj_t* o,bool price=false){
 const lv_font_t* fonts[]={&lv_font_montserrat_20,&lv_font_montserrat_16,&lv_font_montserrat_12,&lv_font_montserrat_10,&lv_font_montserrat_8};
 const lv_font_t* chosen=fonts[4];
 for(unsigned i=price?0:1;i<5;++i){lv_point_t size{};lv_text_get_size(&size,lv_label_get_text(o),fonts[i],0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);if(size.x<=lv_obj_get_style_width(o,LV_PART_MAIN)){chosen=fonts[i];break;}}
 lv_obj_set_style_text_font(o,chosen,0);
}
void copy(char* d,size_t n,const char* s){snprintf(d,n,"%s",s?s:"");}
void line(lv_layer_t* l,float x1,float y1,float x2,float y2,uint32_t c,int w=1,int opa=255){lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.p1.x=lroundf(x1);d.p1.y=lroundf(y1);d.p2.x=lroundf(x2);d.p2.y=lroundf(y2);d.color=lv_color_hex(c);d.width=w;d.opa=opa;d.round_start=1;d.round_end=1;lv_draw_line(l,&d);}
void animateValue(void* var,lv_anim_exec_xcb_t exec,int32_t from,int32_t to,uint32_t duration,int16_t x1,int16_t y1,int16_t x2,int16_t y2,lv_anim_completed_cb_t done=nullptr){lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,var);lv_anim_set_exec_cb(&a,exec);lv_anim_set_values(&a,from,to);lv_anim_set_duration(&a,duration);lv_anim_set_path_cb(&a,lv_anim_path_custom_bezier3);lv_anim_set_bezier3_param(&a,x1,y1,x2,y2);if(done)lv_anim_set_completed_cb(&a,done);lv_anim_start(&a);}
const lv_image_dsc_t* weatherImage(bool valid,int code){if(!valid)return &ag_weather_unknown;if(code==0)return &ag_weather_sunny;if(code<=3)return &ag_weather_cloudy;if(code>=95)return &ag_weather_thunder;if((code>=71&&code<=77)||(code>=85&&code<=86))return &ag_weather_snow;if((code>=51&&code<=67)||(code>=80&&code<=82))return &ag_weather_rain;return &ag_weather_cloudy;}
float wrappedOffset(int index,float pos){float off=index-fmodf(pos,5.f);while(off>2.5f)off-=5.f;while(off<-2.5f)off+=5.f;return off;}
}

bool AuraUi::create(){
 transitionSnapshot_=lv_draw_buf_create(320,240,LV_COLOR_FORMAT_RGB565,0);
 load(Page::Home);timer_=lv_timer_create(timer,33,this);return root_!=nullptr;
}
void AuraUi::configureHome(uint32_t bg,uint32_t fg,uint32_t accent,const char* city){
 homeBackground_=bg;homeForeground_=fg;homeAccent_=accent;copy(homeCity_,sizeof(homeCity_),city);
 auto blend=[&](unsigned weight){uint32_t result=0;for(unsigned shift=0;shift<24;shift+=8)result|=(((((fg>>shift)&255)*weight+((bg>>shift)&255)*(255-weight))/255)<<shift);return result;};
 homeDim_=blend(145);homeFaint_=blend(78);homeLine_=blend(27);
 if(root_&&page_==Page::Home)load(Page::Home);
}
void AuraUi::show(Page p){
 if(transitioning_){pendingPage_=p;return;}
 if(root_&&p==page_)return;
 load(p);
}
void AuraUi::configureStocks(const services::StockPreferences& preferences){
 std::string error;if(!preferences.valid(error))return;stockPreferences_=preferences;stockIndex_=0;
 stock_=services::StockChart{};copy(stock_.ticker,sizeof(stock_.ticker),services::StockPreferences::ticker(preferences.symbols[0]).c_str());copy(stock_.currency,sizeof(stock_.currency),services::StockPreferences::currency(preferences.symbols[0]));
 stock_.maFast=preferences.maFast;stock_.maSlow=preferences.maSlow;stock_.showFast=preferences.showFast;stock_.showSlow=preferences.showSlow;
 if(root_&&page_==Page::Stocks)renderData();
}
void AuraUi::transitionStep(void* context,int32_t value){
 auto* s=static_cast<AuraUi*>(context);
 if(!s->transitionCover_)return;
 // No moving clip edge: blend two complete, stationary pictures. The prepared
 // new page remains opaque underneath; this never fades through an empty screen.
 lv_obj_set_style_opa(s->transitionCover_,255-std::min<int32_t>(value,255),0);
}
void AuraUi::transitionCompleted(lv_anim_t* a){
 auto* s=static_cast<AuraUi*>(a->var);
 if(s->transitionCover_)lv_obj_delete(s->transitionCover_);
 s->transitionCover_=s->transitionImage_=nullptr;s->transitioning_=false;
 // Process queued navigation on the next GUI tick, outside the animation callback.
}
void AuraUi::load(Page p){
 auto* old=root_;
 // An external page change (including voice wake) cancels an unconfirmed preview.
 if(old&&page_==Page::Settings&&settingsView_.back())applySettings();
 if(spectrumPicker_)closeSpectrumPicker(false);
 bool snapshot=settings.transition&&old&&transitionSnapshot_&&lv_snapshot_take_to_draw_buf(old,LV_COLOR_FORMAT_RGB565,transitionSnapshot_)==LV_RESULT_OK;
 spectrumView_.detach();settingsView_.detach();
 page_=pendingPage_=p;transitioning_=false;
 lv_anim_delete(this,agentFaceOpacity);
 agentStatus_=nullptr;agentGif_=nullptr;agentGifSource_=nullptr;agentFacePending_=nullptr;agentFacePhase_=0;
 if(timer_)lv_timer_set_period(timer_,(p==Page::Menu||p==Page::Settings)?16:33);
 clock_=date_=weather_=weatherIcon_=visual_=secondsVisual_=weatherFlow_=dateFlow_=status_=nullptr;
 stockCurrency_=stockFast_=stockSlow_=nullptr;
 stockReturnScope_=stockPosition_=stockEmpty_=stockEmptyHelp_=nullptr;
 change_=wifiIcon_=dateStart_=dateEnd_=dateDial_=homeDateDay_=homeDateRest_=homeNetworkMeta_=clockMinute_=clockColon_=nullptr;
 indoorTemp_=indoorHumidity_=menuTitle_=menuCounter_=menuClock_=nullptr;
 for(int i=0;i<5;++i)rangeButtons_[i]=rangeLabels_[i]=nullptr;
 for(int i=0;i<5;++i)menuCards_[i]=menuGlyphs_[i]=menuLabels_[i]=menuDots_[i]=nullptr;
 root_=lv_obj_create(nullptr);lv_obj_remove_style_all(root_);
 lv_obj_set_size(root_,320,240);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_style_bg_color(root_,lv_color_hex(p==Page::Menu?kPaper:p==Page::Home?homeBackground_:kBlack),0);
 lv_obj_set_style_bg_opa(root_,255,0);lv_obj_set_style_text_color(root_,lv_color_hex(kInk),0);
 switch(p){case Page::Home:buildHome();break;case Page::Menu:buildMenu();break;case Page::Stocks:buildStocks();break;case Page::Spectrum:buildSpectrum();break;case Page::Agent:buildAgent();break;case Page::Settings:settingsView_.attach(root_,&settings);break;}
 renderData();
 if(snapshot){
   transitionCover_=rect(root_,0,0,320,240,kBlack);
   transitionImage_=lv_image_create(transitionCover_);
   lv_image_set_src(transitionImage_,transitionSnapshot_);
 }
 lv_screen_load(root_);if(old)lv_obj_delete(old);
 if(snapshot){
   transitioning_=true;
   animateValue(this,transitionStep,0,255,420,256,102,256,1024,transitionCompleted);
 }
}


void AuraUi::buildHome(){
 const uint32_t kBlack=homeBackground_,kInk=homeForeground_,kSignal=homeAccent_,kDim=homeDim_,kFaint=homeFaint_;
 auto separator=[&](lv_obj_t* p,int x,int y,int w,int h){rect(p,x,y,w,h,homeLine_);};
 label(root_,homeCity_,16,10,104,&lv_font_montserrat_10,kDim);weatherIcon_=image(root_,&ag_weather_unknown,208,5,20);weather_=label(root_,"未知 --℃",232,7,82,&aurageek_font_cn_16,kInk);lv_label_set_long_mode(weather_,LV_LABEL_LONG_CLIP);lv_obj_set_height(weather_,aurageek_font_cn_16.line_height);lv_obj_set_style_text_align(weather_,LV_TEXT_ALIGN_RIGHT,0);separator(root_,16,30,288,1);
 clock_=label(root_,"--",20,43,76,&lv_font_montserrat_48,kInk);clockColon_=label(root_,":",91,43,20,&lv_font_montserrat_48,kSignal);clockMinute_=label(root_,"--",111,43,80,&lv_font_montserrat_48,kInk);lv_label_set_long_mode(clock_,LV_LABEL_LONG_CLIP);lv_label_set_long_mode(clockColon_,LV_LABEL_LONG_CLIP);lv_label_set_long_mode(clockMinute_,LV_LABEL_LONG_CLIP);lv_obj_set_height(clock_,62);lv_obj_set_height(clockColon_,62);lv_obj_set_height(clockMinute_,62);lv_obj_set_style_transform_scale(clock_,320,0);lv_obj_set_style_transform_scale(clockColon_,320,0);lv_obj_set_style_transform_scale(clockMinute_,320,0);lv_obj_set_style_transform_pivot_x(clock_,0,0);lv_obj_set_style_transform_pivot_x(clockColon_,0,0);lv_obj_set_style_transform_pivot_x(clockMinute_,0,0);lv_obj_set_style_transform_pivot_y(clock_,0,0);lv_obj_set_style_transform_pivot_y(clockColon_,0,0);lv_obj_set_style_transform_pivot_y(clockMinute_,0,0);
 secondsVisual_=rect(root_,212,44,88,88,kBlack,0);lv_obj_add_event_cb(secondsVisual_,draw,LV_EVENT_DRAW_MAIN,this);dateDial_=label(root_,"--",235,68,42,&lv_font_montserrat_20,kInk);lv_obj_set_style_text_align(dateDial_,LV_TEXT_ALIGN_CENTER,0);auto* sec=label(root_,"SEC",235,93,42,&lv_font_montserrat_8,kFaint);lv_obj_set_style_text_align(sec,LV_TEXT_ALIGN_CENTER,0);homeDateDay_=label(root_,"---",20,121,31,&lv_font_montserrat_10,kInk);homeDateRest_=label(root_,"WAITING FOR NETWORK",53,121,149,&lv_font_montserrat_10,kDim);
 separator(root_,0,149,320,1);auto* tempTitle=label(root_,"TEMP  °C",0,159,88,&lv_font_montserrat_8,kFaint);lv_obj_set_style_text_align(tempTitle,LV_TEXT_ALIGN_CENTER,0);indoorTemp_=label(root_,"--.-°",0,174,88,&lv_font_montserrat_20,kDim);lv_obj_set_style_text_align(indoorTemp_,LV_TEXT_ALIGN_CENTER,0);separator(root_,88,150,1,52);auto* humidityTitle=label(root_,"HUM  %",88,159,88,&lv_font_montserrat_8,kFaint);lv_obj_set_style_text_align(humidityTitle,LV_TEXT_ALIGN_CENTER,0);indoorHumidity_=label(root_,"--%",88,174,88,&lv_font_montserrat_20,kDim);lv_obj_set_style_text_align(indoorHumidity_,LV_TEXT_ALIGN_CENTER,0);separator(root_,176,150,1,52);label(root_,"24H TREND",190,159,70,&lv_font_montserrat_8,kFaint);dateFlow_=label(root_,"18°-31°",258,158,48,&lv_font_montserrat_10,kDim);lv_obj_set_style_text_align(dateFlow_,LV_TEXT_ALIGN_RIGHT,0);weatherFlow_=rect(root_,190,177,116,21,kBlack,0);lv_obj_add_event_cb(weatherFlow_,draw,LV_EVENT_DRAW_MAIN,this);
 separator(root_,0,211,320,1);wifiIcon_=image(root_,&ag_wifi_0,16,217,15);lv_obj_set_style_image_recolor(wifiIcon_,lv_color_hex(kFaint),0);lv_obj_set_style_image_recolor_opa(wifiIcon_,255,0);status_=label(root_,"OFFLINE",37,219,112,&lv_font_montserrat_10,kFaint);homeNetworkMeta_=label(root_,"2.4G  /  NTP",190,219,114,&lv_font_montserrat_10,kDim);lv_obj_set_style_text_align(homeNetworkMeta_,LV_TEXT_ALIGN_RIGHT,0);
}

void AuraUi::buildMenu(){
 label(root_,"AURA",14,8,64,&lv_font_montserrat_12,kMenuInk);menuTitle_=label(root_,"HOME",96,8,128,&lv_font_montserrat_12,kMenuInk);lv_obj_set_style_text_align(menuTitle_,LV_TEXT_ALIGN_CENTER,0);auto* mark=label(root_,LV_SYMBOL_CHARGE,282,7,24,&lv_font_montserrat_14,kMenuInk);lv_obj_set_style_text_align(mark,LV_TEXT_ALIGN_RIGHT,0);separator(root_,14,30,292,1,kMenuLine);
 const char* names[]={"HOME","STOCKS","SPECTRUM","AGENT","SETTINGS"};for(int i=0;i<5;++i){menuCards_[i]=rect(root_,0,0,56,80,kPaper,0);lv_obj_set_style_transform_pivot_x(menuCards_[i],28,0);lv_obj_set_style_transform_pivot_y(menuCards_[i],40,0);menuGlyphs_[i]=rect(menuCards_[i],0,0,56,56,kPaper,0);lv_obj_add_event_cb(menuGlyphs_[i],draw,LV_EVENT_DRAW_MAIN,this);menuLabels_[i]=label(menuCards_[i],names[i],0,63,56,&lv_font_montserrat_8,kMenuDim);lv_obj_set_style_text_align(menuLabels_[i],LV_TEXT_ALIGN_CENTER,0);lv_obj_add_flag(menuCards_[i],LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(menuCards_[i],pressed,LV_EVENT_CLICKED,this);menuDots_[i]=rect(root_,138+i*10,185,4,4,i==selection_?kMenuInk:kMenuLine);}
 separator(root_,14,201,292,1,kMenuLine);menuCounter_=label(root_,"01 / 05",14,216,90,&lv_font_montserrat_10,kMenuDim);menuClock_=label(root_,time_,238,212,68,&lv_font_montserrat_14,kMenuInk);lv_obj_set_style_text_align(menuClock_,LV_TEXT_ALIGN_RIGHT,0);menuPosition_=menuTarget_=float(selection_);menuVelocity_=0.f;menuTick_=lv_tick_get();renderMenuCarousel();
}

void AuraUi::buildStocks(){
 clock_=label(root_,stock_.ticker,16,9,76,&lv_font_montserrat_16,kInk);lv_label_set_long_mode(clock_,LV_LABEL_LONG_CLIP);lv_obj_add_flag(clock_,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(clock_,pressed,LV_EVENT_CLICKED,this);weather_=label(root_,"--",94,7,104,&lv_font_montserrat_20,kInk);lv_label_set_long_mode(weather_,LV_LABEL_LONG_CLIP);change_=label(root_,"-- %",202,3,102,&lv_font_montserrat_16,kDim);lv_label_set_long_mode(change_,LV_LABEL_LONG_CLIP);lv_obj_set_style_text_align(change_,LV_TEXT_ALIGN_RIGHT,0);stockReturnScope_=label(root_,"全区间涨跌",202,23,102,&stock_font_10,kDim);lv_obj_set_style_text_align(stockReturnScope_,LV_TEXT_ALIGN_RIGHT,0);separator(root_,16,40,288,1);label(root_,"收盘",16,44,42,&stock_font_12,kInk);stockFast_=label(root_,"MA20",68,46,44,&lv_font_montserrat_12,kStockFast);stockSlow_=label(root_,"MA55",116,46,48,&lv_font_montserrat_12,kStockSlow);stockCurrency_=label(root_,"日线 USD",216,44,88,&stock_font_12,kDim);lv_obj_set_style_text_align(stockCurrency_,LV_TEXT_ALIGN_RIGHT,0);
 const char* names[]={"半年","1年","3年","5年","全部"};for(int i=0;i<5;++i){auto* button=rect(root_,16+i*58,66,52,21,kBlack);rangeButtons_[i]=button;auto* l=label(button,names[i],0,1,52,&stock_font_12,kDim);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);rangeLabels_[i]=l;lv_obj_add_flag(button,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(button,pressed,LV_EVENT_CLICKED,this);}separator(root_,16,90,288,1);visual_=rect(root_,40,96,264,101,kBlack,0);lv_obj_add_event_cb(visual_,draw,LV_EVENT_DRAW_MAIN,this);for(int i=0;i<3;++i)scaleLabels_[i]=label(root_,"--",2,92+i*44,36,&lv_font_montserrat_10,kDim);dateStart_=label(root_,"--",40,199,112,&lv_font_montserrat_12,kDim);dateEnd_=label(root_,"--",192,199,112,&lv_font_montserrat_12,kDim);lv_obj_set_style_text_align(dateEnd_,LV_TEXT_ALIGN_RIGHT,0);separator(root_,16,219,288,1);status_=label(root_,"",16,222,246,&stock_font_12,kDim);lv_label_set_long_mode(status_,LV_LABEL_LONG_CLIP);stockPosition_=label(root_,"",266,224,38,&lv_font_montserrat_12,kDim);lv_obj_set_style_text_align(stockPosition_,LV_TEXT_ALIGN_RIGHT,0);
 stockEmpty_=label(root_,"暂无日线数据",52,121,240,&stock_font_16,kInk);stockEmptyHelp_=label(root_,"单击切换标的 / 网页管理自选",52,150,240,&stock_font_12,kDim);lv_obj_set_style_text_align(stockEmpty_,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_text_align(stockEmptyHelp_,LV_TEXT_ALIGN_CENTER,0);
}

void AuraUi::buildSpectrum(){
 lv_obj_set_style_bg_color(root_,lv_color_hex(0x05070b),0);
 if(!spectrumView_.attach(root_))LV_LOG_ERROR("Spectrum RGB565 buffer allocation failed");
 spectrumView_.update(bands_,audioActive_,lv_tick_get());
}
void AuraUi::buildAgent(){lv_obj_set_style_bg_color(root_,lv_color_black(),0);agentGif_=lv_gif_create(root_);agentStatus_=label(root_,"",8,216,304,&lv_font_montserrat_12,kSignal);lv_obj_set_style_text_align(agentStatus_,LV_TEXT_ALIGN_CENTER,0);setAgentState(agentState_);}
void AuraUi::setAgentState(unsigned state){agentState_=state;static const char* names[]={"SAY NI HAO XIAO CHEN / CLICK","CONNECTING...","LISTENING - AUTO SEND","THINKING...","SPEAKING...","CHECK ACTIVATION / NETWORK"};if(agentStatus_)update(agentStatus_,names[state<6?state:5]);renderAgentFace();}
void AuraUi::setAgentEmotion(unsigned emotion){if(agentEmotion_==emotion)return;agentEmotion_=emotion;renderAgentFace();}
void AuraUi::renderAgentFace(){
 if(!agentGif_)return;
 const lv_image_dsc_t* face=&ag_idle;
 if(agentState_==1 || agentState_==3)face=agentAsset(1);
 else if(agentState_==2)face=agentAsset(0);
 else if(agentState_==5)face=agentAsset(9);
 else if(agentState_==4){
  static const unsigned faces[]={2,2,3,4,5,6,7,8,0,9,10,11,12,1,12,2,3,12,2,0,3};
  if(agentEmotion_<sizeof(faces)/sizeof(faces[0]))face=agentAsset(faces[agentEmotion_]);
 }
 if(!face)face=&ag_idle;
 // Initial page construction loads immediately. Later changes share one GIF
 // decoder: fade out, change only at zero opacity, then fade in (180 ms total).
 if(!agentGifSource_){agentGifSource_=agentFacePending_=face;++agentFaceRevision_;lv_gif_set_src(agentGif_,face);lv_obj_center(agentGif_);return;}
 if(agentFacePending_==face)return;
 agentFacePending_=face;
 if(agentFacePhase_==1)return; // Coalesce rapid emotions without restarting fade-out.
 lv_anim_delete(this,agentFaceOpacity);agentFacePhase_=1;
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,this);
 lv_anim_set_values(&a,lv_obj_get_style_opa(agentGif_,LV_PART_MAIN),0);
 lv_anim_set_duration(&a,70);lv_anim_set_exec_cb(&a,agentFaceOpacity);
 lv_anim_set_completed_cb(&a,agentFaceFadeOutDone);lv_anim_start(&a);
}
void AuraUi::agentFaceOpacity(void* self,int32_t value){auto* ui=static_cast<AuraUi*>(self);if(ui->agentGif_)lv_obj_set_style_opa(ui->agentGif_,value,0);}
void AuraUi::agentFaceFadeOutDone(lv_anim_t* anim){
 auto* ui=static_cast<AuraUi*>(anim->var);if(!ui->agentGif_)return;
 if(ui->agentGifSource_!=ui->agentFacePending_){ui->agentGifSource_=ui->agentFacePending_;++ui->agentFaceRevision_;lv_gif_set_src(ui->agentGif_,ui->agentGifSource_);lv_obj_center(ui->agentGif_);}
 ui->agentFacePhase_=2;
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,ui);lv_anim_set_values(&a,0,255);
 lv_anim_set_duration(&a,110);lv_anim_set_exec_cb(&a,agentFaceOpacity);
 lv_anim_set_completed_cb(&a,agentFaceFadeInDone);lv_anim_start(&a);
}
void AuraUi::agentFaceFadeInDone(lv_anim_t* anim){static_cast<AuraUi*>(anim->var)->agentFacePhase_=0;}

void AuraUi::animateMenuSelection(int,int){}
void AuraUi::renderMenuCarousel(){
 if(page_!=Page::Menu)return;
 static const char* names[]={"HOME","STOCKS","SPECTRUM","AGENT","SETTINGS"};
 char b[16];snprintf(b,sizeof(b),"%02d / 05",selection_+1);
 update(menuCounter_,b);update(menuTitle_,names[selection_]);
 for(int i=0;i<5;++i){
   const float off=wrappedOffset(i,menuPosition_),d=std::min(std::fabs(off),1.f),scale=1.5f-.78f*d;
   const int opacity=std::fabs(off)>1.62f?0:int(255.f*(1.f-.6f*d));
   // Apply the original pivot transform geometrically: no whole-card snapshot.
   menuScale_[i]=scale;menuOpacity_[i]=uint8_t(opacity);
   const float px=160.f+off*100.f-28.f*scale,py=74.f+d*14.f+40.f*(1.f-scale);
   const int ix=int(lroundf(px)),iy=int(lroundf(py));
   menuFractionX_[i]=px-ix;menuFractionY_[i]=py-iy;
   lv_obj_set_pos(menuCards_[i],ix,iy);
   lv_obj_set_size(menuCards_[i],int(ceilf(56*scale)),int(ceilf(80*scale)));
   lv_obj_set_size(menuGlyphs_[i],int(ceilf(56*scale)),int(ceilf(56*scale)));
   lv_obj_invalidate(menuGlyphs_[i]);
   // Text keeps the same font and scale; only this small label needs a layer.
   lv_obj_set_pos(menuLabels_[i],0,int(63*scale));
   lv_obj_set_style_transform_pivot_x(menuLabels_[i],0,0);
   lv_obj_set_style_transform_pivot_y(menuLabels_[i],0,0);
   lv_obj_set_style_transform_scale(menuLabels_[i],int(256*scale),0);
   lv_obj_set_style_text_opa(menuLabels_[i],std::fabs(off)<.45f?int(opacity*(1.f-std::fabs(off)*2.2f)):0,0);
   lv_obj_set_style_bg_color(menuDots_[i],lv_color_hex(i==selection_?kMenuInk:kMenuLine),0);
 }
}

void AuraUi::rotate(int steps){if(!steps)return;if(page_==Page::Settings){settingsView_.rotate(steps);return;}if(spectrumPicker_){const int count=int(SpectrumView::Style::Count);styleSelection_=(styleSelection_+steps%count+count)%count;renderSpectrumPicker();return;}if(page_==Page::Stocks){int next=(int(stockRange_)+steps)%5;if(next<0)next+=5;if(unsigned(next)==stockRange_)return;setStockRange(unsigned(next));return;}if(transitioning_)return;if(page_!=Page::Menu){show(Page::Menu);return;}menuTarget_+=steps;menuVelocity_=0;selection_=(selection_+steps%5+5)%5;menuTick_=lv_tick_get();renderMenuCarousel();}
void AuraUi::click(){if(page_==Page::Settings){settingsView_.click();applySettings();return;}if(spectrumPicker_){closeSpectrumPicker(true);return;}if(transitioning_)return;if(page_==Page::Spectrum){spectrumView_.pulse();return;}if(page_==Page::Menu){const Page pages[]={Page::Home,Page::Stocks,Page::Spectrum,Page::Agent,Page::Settings};show(pages[selection_]);}else if(page_==Page::Stocks){stockIndex_=(stockIndex_+1)%stockPreferences_.count;stock_.count=0;copy(stock_.ticker,sizeof(stock_.ticker),services::StockPreferences::ticker(stockPreferences_.symbols[stockIndex_]).c_str());copy(stock_.currency,sizeof(stock_.currency),services::StockPreferences::currency(stockPreferences_.symbols[stockIndex_]));copy(stock_.status,sizeof(stock_.status),"WAITING FOR DAILY DATA");renderData();if(visual_)lv_obj_invalidate(visual_);}}
void AuraUi::back(){if(page_==Page::Settings&&settingsView_.back()){applySettings();return;}if(spectrumPicker_){closeSpectrumPicker(false);return;}if(transitioning_)return;if(page_==Page::Menu)show(Page::Home);else show(Page::Menu);}
void AuraUi::home(){if(page_==Page::Settings){settingsView_.back();applySettings();}if(spectrumPicker_)closeSpectrumPicker(false);show(Page::Home);}
void AuraUi::doubleClick(){
 if(transitioning_){deferredDouble_=true;deferredDoublePage_=page_;return;}
 if(page_==Page::Spectrum&&!transitioning_){if(spectrumPicker_)closeSpectrumPicker(false);else openSpectrumPicker();}
 else back();
}
void AuraUi::openSpectrumPicker(){
 styleSelection_=int(spectrumView_.style());stylePosition_=float(styleSelection_);styleVelocity_=0.f;
 spectrumPicker_=rect(root_,16,48,288,144,kBlack);
 separator(spectrumPicker_,0,0,288,1,kLine);
 separator(spectrumPicker_,0,143,288,1,kLine);
 // A single signal-colored locator replaces nested colored cards/borders.
 rect(spectrumPicker_,142,124,4,4,kSignal);
 for(unsigned i=0;i<unsigned(SpectrumView::Style::Count);++i){
   styleCards_[i]=rect(spectrumPicker_,0,0,64,68,kBlack,0);
   lv_obj_set_style_transform_pivot_x(styleCards_[i],32,0);
   lv_obj_set_style_transform_pivot_y(styleCards_[i],34,0);
   lv_obj_add_event_cb(styleCards_[i],pickerDraw,LV_EVENT_DRAW_MAIN,this);
 }
 renderSpectrumPicker();
}
void AuraUi::renderSpectrumPicker(){
 for(unsigned i=0;i<unsigned(SpectrumView::Style::Count);++i){
   float off=float(i)-stylePosition_,distance=std::min(1.f,std::fabs(off));
   lv_obj_set_pos(styleCards_[i],int(144+off*100-32),int(25+distance*14));
   lv_obj_set_style_transform_scale(styleCards_[i],int(256*(1.5f-.78f*distance)),0);
   lv_obj_set_style_opa(styleCards_[i],int(255*(1.f-.6f*distance)),0);
   lv_obj_invalidate(styleCards_[i]);
 }
}
void AuraUi::closeSpectrumPicker(bool confirm){
 if(!spectrumPicker_)return;
 if(confirm)spectrumView_.setStyle(static_cast<SpectrumView::Style>(styleSelection_));
 lv_obj_delete(spectrumPicker_);spectrumPicker_=nullptr;
 for(auto& card:styleCards_)card=nullptr;
}
void AuraUi::pickerDraw(lv_event_t* event){
 auto* s=static_cast<AuraUi*>(lv_event_get_user_data(event));
 auto* target=lv_event_get_target_obj(event);auto* layer=lv_event_get_layer(event);
 lv_area_t a;lv_obj_get_coords(target,&a);
 const uint32_t iconColor=kInk;
 if(target==s->styleCards_[0]){
   for(int i=0;i<11;++i){int x=a.x1+11+i*4,h=5+int(15*std::fabs(sinf(i*1.3f)));
     line(layer,x,a.y1+34,x,a.y1+34-h,iconColor,2);
     line(layer,x,a.y1+38,x,a.y1+38+h*.6f,kDim,2,150);
   }
 }else{
   for(int i=0;i<24;++i){float angle=2*kPi*i/24,r=15,h=3+(i%4)*2;
     line(layer,a.x1+32+cosf(angle)*r,a.y1+34+sinf(angle)*r,a.x1+32+cosf(angle)*(r+h),a.y1+34+sinf(angle)*(r+h),iconColor,2);
   }
 }
}
void AuraUi::pressed(lv_event_t* e){auto* s=static_cast<AuraUi*>(lv_event_get_user_data(e));auto* target=lv_event_get_target_obj(e);if(s->page_==Page::Menu){for(int i=0;i<5;++i)if(target==s->menuCards_[i]){s->selection_=i;s->click();return;}}if(s->page_==Page::Stocks){if(target==s->clock_){s->click();return;}for(unsigned i=0;i<5;++i)if(target==s->rangeButtons_[i]){s->setStockRange(i);return;}}}
void AuraUi::setNetwork(bool c,bool,int rssi){if(connected_==c&&rssi_==rssi)return;connected_=c;rssi_=rssi;if(!c){setClock(false,"--:--","WAITING FOR NETWORK");setWeather(false,"未知 --℃",-1);}if(page_==Page::Home||page_==Page::Menu)renderData();}
void AuraUi::setClock(bool v,const char* t,const char* d){clockValid_=v;copy(time_,sizeof(time_),v?t:"--:--");copy(dateText_,sizeof(dateText_),v?d:"WAITING FOR NETWORK");if(page_==Page::Home||page_==Page::Menu)renderData();}
void AuraUi::setWeather(bool v,const char* t,int c,const float* hourly,size_t count){weatherValid_=v;weatherCode_=v?c:-1;copy(weatherText_,sizeof(weatherText_),v?t:"未知 --℃");weatherTrendCount_=0;weatherTrendMin_=weatherTrendMax_=0.f;if(v&&hourly){count=std::min<size_t>(count,24);for(size_t i=0;i<count;++i){if(!std::isfinite(hourly[i]))continue;const float sample=hourly[i];weatherTrend_[weatherTrendCount_++]=sample;if(weatherTrendCount_==1)weatherTrendMin_=weatherTrendMax_=sample;else{weatherTrendMin_=std::min(weatherTrendMin_,sample);weatherTrendMax_=std::max(weatherTrendMax_,sample);}}}if(page_==Page::Home)renderData();}
void AuraUi::setIndoorUnavailable(){setIndoor(false,0.f,0.f);}
void AuraUi::setIndoor(bool valid,float temperature,float humidity){if(indoorValid_==valid&&(!valid||(std::fabs(indoorTemperature_-temperature)<.01f&&std::fabs(indoorHumidityValue_-humidity)<.01f)))return;indoorValid_=valid;indoorTemperature_=temperature;indoorHumidityValue_=humidity;if(page_==Page::Home)renderData();}
void AuraUi::setAiPending(){}
void AuraUi::setStock(const services::StockChart& c){if(!stockRangePending()&&!memcmp(&stock_,&c,sizeof(c)))return;stock_=c;stockDisplayedRange_=stockRange_;renderData();if(visual_&&page_==Page::Stocks)lv_obj_invalidate(visual_);}
// Fixed 200% visual gain, independent of speaker volume; clamp to the canvas range.
void AuraUi::setSpectrum(const float* p,size_t n,bool a,bool d){constexpr float displayGain=2.0f;audioActive_=a;demoAudio_=d;for(size_t i=0;i<48;++i)bands_[i]=(p&&i<n)?std::max(0.f,std::min(1.f,p[i]*displayGain)):0;}

void AuraUi::renderData(){
 const uint32_t kInk=page_==Page::Home?homeForeground_:0xf2f2f2,kDim=page_==Page::Home?homeDim_:0x8a8a8a,kSignal=page_==Page::Home?homeAccent_:0xdfff00,kFaint=page_==Page::Home?homeFaint_:0x4b4b4b;
 if(page_==Page::Home){
  char b[96],hh[3]="--",mm[3]="--";
  if(clockValid_&&strlen(time_)>=5){hh[0]=time_[0];hh[1]=time_[1];mm[0]=time_[3];mm[1]=time_[4];}
  update(clock_,hh);update(clockMinute_,mm);update(weather_,weatherText_);
  // Keep the right edge fixed; longer conditions expand left, never below the header.
  // City ends at x=120; reserve 8px gap + 20px icon + 4px icon/text spacing.
  lv_point_t weatherSize{};
  lv_text_get_size(&weatherSize,weatherText_,&aurageek_font_cn_16,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  const int weatherWidth=std::min(162,std::max(1,int(weatherSize.x)));
  lv_obj_set_width(weather_,weatherWidth);lv_obj_set_x(weather_,314-weatherWidth);
  if(weatherIcon_)lv_obj_set_x(weatherIcon_,314-weatherWidth-24);
  if(indoorValid_){snprintf(b,sizeof(b),"%.1f°",indoorTemperature_);update(indoorTemp_,b);snprintf(b,sizeof(b),"%.0f%%",indoorHumidityValue_);update(indoorHumidity_,b);}else{update(indoorTemp_,"--.-°");update(indoorHumidity_,"--%");}
  lv_obj_set_style_text_color(indoorTemp_,lv_color_hex(indoorValid_?kInk:kDim),0);lv_obj_set_style_text_color(indoorHumidity_,lv_color_hex(indoorValid_?kInk:kDim),0);
  if(weatherTrendCount_>=2)snprintf(b,sizeof(b),"%.0f°-%.0f°",std::floor(weatherTrendMin_),std::ceil(weatherTrendMax_));
  else copy(b,sizeof(b),"--°--°");
  update(dateFlow_,b);
  const char* split=strstr(dateText_,"  ");size_t skip=2;if(!split){split=strchr(dateText_,',');skip=1;while(split&&split[skip]==' ')++skip;}
  if(clockValid_&&split){char day[8];size_t n=std::min<size_t>(split-dateText_,sizeof(day)-1);memcpy(day,dateText_,n);day[n]=0;update(homeDateDay_,day);update(homeDateRest_,split+skip);}else{update(homeDateDay_,"---");update(homeDateRest_,dateText_);}
  snprintf(b,sizeof(b),connected_?"ONLINE  %d dBm":"OFFLINE",rssi_);update(status_,b);update(homeNetworkMeta_,connected_&&clockValid_?"2.4G  /  NTP":"2.4G  /  --");
  if(lv_image_get_src(wifiIcon_)!=(connected_?&ag_wifi_1:&ag_wifi_0))lv_image_set_src(wifiIcon_,connected_?&ag_wifi_1:&ag_wifi_0);lv_obj_set_style_image_recolor(wifiIcon_,lv_color_hex(connected_?kSignal:kFaint),0);
  snprintf(b,sizeof(b),clockValid_?"%02d":"--",second_);update(dateDial_,b);
  if(weatherIcon_){auto* src=weatherImage(weatherValid_,weatherCode_);if(lv_image_get_src(weatherIcon_)!=src)lv_image_set_src(weatherIcon_,src);}
  if(secondsVisual_)lv_obj_invalidate(secondsVisual_);if(weatherFlow_)lv_obj_invalidate(weatherFlow_);
 }
 if(page_==Page::Menu){update(menuClock_,time_);}
 if(page_==Page::Stocks){update(clock_,stock_.ticker);const bool longTicker=strlen(stock_.ticker)>6;lv_obj_set_width(clock_,longTicker?96:76);lv_obj_set_x(weather_,longTicker?116:94);lv_obj_set_width(weather_,longTicker?82:104);fitStockLabel(clock_);char meta[24];snprintf(meta,sizeof(meta),"日线 %s",stock_.currency);update(stockCurrency_,meta);snprintf(meta,sizeof(meta),"MA%u",stock_.maFast);update(stockFast_,stock_.showFast?meta:"");snprintf(meta,sizeof(meta),"MA%u",stock_.maSlow);update(stockSlow_,stock_.showSlow?meta:"");char b[96]="--";if(stock_.count)snprintf(b,sizeof(b),"%.2f",stock_.last);update(weather_,b);fitStockLabel(weather_,true);if(stock_.count)snprintf(b,sizeof(b),"%+.1f%%",stock_.change);else copy(b,sizeof(b),"-- %");update(change_,b);fitStockLabel(change_);const char* scopes[]={"半年区间涨跌","1年区间涨跌","3年区间涨跌","5年区间涨跌","全区间涨跌"};update(stockReturnScope_,scopes[stockDisplayedRange_]);snprintf(meta,sizeof(meta),"%u/%u",stockIndex_+1,stockPreferences_.count);update(stockPosition_,meta);if(stock_.count){lv_obj_add_flag(stockEmpty_,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(stockEmptyHelp_,LV_OBJ_FLAG_HIDDEN);}else{lv_obj_remove_flag(stockEmpty_,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(stockEmptyHelp_,LV_OBJ_FLAG_HIDDEN);}lv_obj_set_style_text_color(change_,lv_color_hex(stock_.count?(stock_.change>=0?kSignal:kInk):kDim),0);for(unsigned i=0;i<5;++i){bool selected=i==stockDisplayedRange_;lv_obj_set_style_border_width(rangeButtons_[i],selected?2:0,0);lv_obj_set_style_border_side(rangeButtons_[i],LV_BORDER_SIDE_BOTTOM,0);lv_obj_set_style_border_color(rangeButtons_[i],lv_color_hex(kSignal),0);lv_obj_set_style_text_color(rangeLabels_[i],lv_color_hex(selected?kInk:kDim),0);}for(int i=0;i<3;++i){if(stock_.count)formatStockAxis(b,sizeof(b),stock_.high-i*(stock_.high-stock_.low)/2,stock_.high-stock_.low);else copy(b,sizeof(b),"--");update(scaleLabels_[i],b);lv_label_set_long_mode(scaleLabels_[i],LV_LABEL_LONG_CLIP);fitStockLabel(scaleLabels_[i]);}auto formatDate=[](char* dst,uint32_t date){if(date)snprintf(dst,24,"%04u-%02u-%02u",unsigned(date/10000),unsigned(date/100%100),unsigned(date%100));else snprintf(dst,24,"--");};formatDate(b,stock_.count?stock_.firstDate:0);update(dateStart_,b);formatDate(b,stock_.count?stock_.lastDate:0);update(dateEnd_,b);update(status_,stockStatusText(stock_.status,stock_.count!=0));}

}

void AuraUi::timer(lv_timer_t* t){
 auto* s=static_cast<AuraUi*>(lv_timer_get_user_data(t));
 if(s->transitioning_)return;
 if(s->pendingPage_!=s->page_){s->load(s->pendingPage_);return;}
 if(s->deferredDouble_){s->deferredDouble_=false;if(s->deferredDoublePage_==s->page_){s->doubleClick();return;}}
 uint32_t now=lv_tick_get();if(s->page_==Page::Settings){s->settingsView_.update(now);return;}float dt=s->frameTick_?std::min((now-s->frameTick_)/1000.f,.1f):.033f;s->frameTick_=now;
 if(s->spectrumPicker_){
   if(std::fabs(s->stylePosition_-s->styleSelection_)>.001f||std::fabs(s->styleVelocity_)>.01f){
     // Exact damped spring solution for the guide's stiffness=170,damping=22.
     const float delta=s->stylePosition_-s->styleSelection_,velocity=s->styleVelocity_;
     const float decay=expf(-11*dt),c=cosf(7*dt),sn=sinf(7*dt);
     s->stylePosition_=s->styleSelection_+decay*(delta*c+(velocity+11*delta)/7*sn);
     s->styleVelocity_=decay*(velocity*c-(11*velocity+170*delta)/7*sn);
     if(std::fabs(s->stylePosition_-s->styleSelection_)<.001f&&std::fabs(s->styleVelocity_)<.01f){s->stylePosition_=float(s->styleSelection_);s->styleVelocity_=0;}
     s->renderSpectrumPicker();
   }
   return;
 }
 if(s->page_==Page::Menu&&(std::fabs(s->menuTarget_-s->menuPosition_)>=.001f||std::fabs(s->menuVelocity_)>=.01f)){
   // Direct eased tracking closes promptly and never overshoots the latest target.
   stepMenuMotion(s->menuPosition_,s->menuTarget_,s->menuVelocity_,dt);
   s->renderMenuCarousel();
 }
 if(s->page_==Page::Home&&s->clockColon_){
   float phase=fmodf(now/1000.f,1.f);
   lv_obj_set_style_opa(s->clockColon_,uint8_t(31+224*(.5f+.5f*cosf(phase*2*kPi))),0);
 }
 if(s->page_==Page::Spectrum)s->spectrumView_.update(s->bands_,s->audioActive_,now);
 if(s->secondsVisual_)lv_obj_invalidate(s->secondsVisual_);
 if(s->weatherFlow_)lv_obj_invalidate(s->weatherFlow_);
}

void AuraUi::draw(lv_event_t* e){auto* s=static_cast<AuraUi*>(lv_event_get_user_data(e));auto* layer=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);float tm=lv_tick_get()/1000.f;
 const uint32_t kSignal=s->page_==Page::Home?s->homeAccent_:0xdfff00,kLine=s->page_==Page::Home?s->homeLine_:0x1e1e1e,kFaint=s->page_==Page::Home?s->homeFaint_:0x4b4b4b;
 if(s->page_==Page::Home){auto* target=lv_event_get_target_obj(e);if(target==s->secondsVisual_){float sec=s->clockValid_?fmodf(s->second_+std::min(1.f,(lv_tick_get()-s->lastClockTick_)/1000.f),60.f):0.f;for(int i=0;i<60;++i){float angle=2*kPi*i/60-kPi/2,inner=i%5?39.f:36.f;line(layer,a.x1+44+cosf(angle)*inner,a.y1+44+sinf(angle)*inner,a.x1+44+cosf(angle)*42,a.y1+44+sinf(angle)*42,s->clockValid_&&i<=sec?kSignal:kLine,i%5?1:2);}return;}if(target==s->weatherFlow_){const int width=a.x2-a.x1+1,height=a.y2-a.y1+1;if(s->weatherTrendCount_<2)return;const float span=std::max(.5f,s->weatherTrendMax_-s->weatherTrendMin_);auto point=[&](float index,float& x,float& y){const size_t lo=std::min<size_t>(size_t(index),s->weatherTrendCount_-1),hi=std::min(lo+1,s->weatherTrendCount_-1);const float fraction=index-lo,value=s->weatherTrend_[lo]+(s->weatherTrend_[hi]-s->weatherTrend_[lo])*fraction;x=2.f+index*(width-5.f)/(s->weatherTrendCount_-1);y=2.f+(s->weatherTrendMax_-value)*(height-5.f)/span;};float previousX=0,previousY=0;point(0,previousX,previousY);for(size_t i=1;i<s->weatherTrendCount_;++i){float x,y;point(float(i),x,y);line(layer,a.x1+previousX,a.y1+previousY,a.x1+x,a.y1+y,kFaint,1);previousX=x;previousY=y;}const float cursor=fmodf(tm*2.2f,float(s->weatherTrendCount_-1));float x,y;point(cursor,x,y);line(layer,a.x1+x,a.y1+y,a.x1+x+1,a.y1+y,kSignal,3);return;}}
 if(s->page_==Page::Menu){int glyph=-1;for(int i=0;i<5;++i)if(lv_event_get_target_obj(e)==s->menuGlyphs_[i])glyph=i;if(glyph<0)return;const float x=a.x1+s->menuFractionX_[glyph],y=a.y1+s->menuFractionY_[glyph],scale=s->menuScale_[glyph];auto stroke=[&](float x1,float y1,float x2,float y2,int width=2){const float w=std::max(1.f,width*scale);const int base=int(floorf(w)),opa=s->menuOpacity_[glyph];const int edge=int(lroundf((w-base)*opa));if(edge)line(layer,x+x1*scale,y+y1*scale,x+x2*scale,y+y2*scale,kMenuInk,base+1,edge);line(layer,x+x1*scale,y+y1*scale,x+x2*scale,y+y2*scale,kMenuInk,base,opa);};if(glyph==0){stroke(9,27,28,10,3);stroke(28,10,47,27,3);stroke(14,25,14,47,3);stroke(42,25,42,47,3);stroke(14,47,42,47,3);stroke(24,47,24,34,2);stroke(24,34,34,34,2);stroke(34,34,34,47,2);}else if(glyph==1){stroke(10,45,10,12,2);stroke(10,45,47,45,2);stroke(14,38,22,30,3);stroke(22,30,29,34,3);stroke(29,34,38,20,3);stroke(38,20,46,14,3);stroke(42,14,46,14,2);stroke(46,14,46,18,2);}else if(glyph==2){static const int heights[]={9,16,25,34,23,38,29,18,12};for(int i=0;i<9;++i){float px=12+i*4;stroke(px,46-heights[i],px,46,3);}}else if(glyph==3){stroke(14,18,42,18,3);stroke(14,18,14,43,3);stroke(42,18,42,43,3);stroke(14,43,42,43,3);stroke(28,18,28,10,2);stroke(25,9,31,9,3);stroke(21,28,21,31,3);stroke(35,28,35,31,3);stroke(22,37,34,37,2);}else{for(int n=0;n<24;n++){float a0=2*kPi*n/24,a1=2*kPi*(n+1)/24;stroke(28+13*cosf(a0),28+13*sinf(a0),28+13*cosf(a1),28+13*sinf(a1),2);}for(int n=0;n<8;n++){float a0=2*kPi*n/8;stroke(28+14*cosf(a0),28+14*sinf(a0),28+20*cosf(a0),28+20*sinf(a0),3);}stroke(24,28,32,28,2);stroke(28,24,28,32,2);}return;}
 if(s->page_==Page::Stocks){for(int i=0;i<5;++i)line(layer,a.x1,a.y1+2+i*23,a.x2,a.y1+2+i*27,kLine,1);for(int i=1;i<5;++i)line(layer,a.x1+i*52.8f,a.y1,a.x1+i*52.8f,a.y2,kLine,1);auto& c=s->stock_;if(!c.count)return;float span=std::max(c.high-c.low,.000001f);const float* series[]={c.close,c.ma20,c.ma55};const uint32_t colors[]={kInk,kStockFast,kStockSlow};for(int k=0;k<3;++k)for(size_t i=1;i<c.count;++i){if(!std::isfinite(series[k][i-1])||!std::isfinite(series[k][i]))continue;float x1=a.x1+2+(i-1)*260.f/(c.count-1),x2=a.x1+2+i*260.f/(c.count-1),y1=a.y2-4-(series[k][i-1]-c.low)/span*93,y2=a.y2-4-(series[k][i]-c.low)/span*93;line(layer,x1,y1,x2,y2,colors[k],k?1:2,255);}float y=a.y2-4-(c.close[c.count-1]-c.low)/span*93;line(layer,a.x2-3,y,a.x2-1,y,kSignal,4);return;}
}
}}

#include "ui/AuraUi.h"
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
LV_IMAGE_DECLARE(ag_wallpaper);
LV_IMAGE_DECLARE(ag_menu_wallpaper);
LV_IMAGE_DECLARE(ag_temperature);
LV_IMAGE_DECLARE(ag_humidity);
LV_IMAGE_DECLARE(ag_wifi_0);
LV_IMAGE_DECLARE(ag_wifi_1);
LV_FONT_DECLARE(aurageek_font_cn_16);
}
namespace aurageek { namespace ui {
namespace {
constexpr uint32_t bg=0x080E1A, panel=0x111D2F, text=0xEAF2FF, muted=0x8194AC, cyan=0x54DCE8;
constexpr float pi=3.14159265359f;
lv_obj_t* box(lv_obj_t* p,int x,int y,int w,int h,uint32_t color) {
  auto* o=lv_obj_create(p); lv_obj_remove_style_all(o);
  lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
  lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,255,0);
  lv_obj_set_style_radius(o,16,0); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
lv_obj_t* label(lv_obj_t* p,const char* s,int x,int y,int w,const lv_font_t* f,uint32_t c) {
  auto* o=lv_label_create(p); lv_label_set_text(o,s); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
  lv_obj_set_style_text_font(o,f,0); lv_obj_set_style_text_color(o,lv_color_hex(c),0); return o;
}
void update(lv_obj_t* o,const char* s) { if(o && strcmp(lv_label_get_text(o),s)) lv_label_set_text(o,s); }
void copy(char* d,size_t n,const char* s){ snprintf(d,n,"%s",s?s:""); }
uint32_t weatherAccent(bool valid,int code){
 if(!valid)return 0xFFBC83;                         // Unknown: amber fallback.
 if(code==0)return 0xFFD166;                        // Clear: sunlight gold.
 if(code>=1&&code<=3)return 0x8EC5E8;               // Cloud cover: sky blue.
 if(code==45||code==48)return 0xAAB7C4;             // Fog: cool silver.
 if((code>=71&&code<=77)||(code>=85&&code<=86))return 0xC7F0FF; // Snow: ice blue.
 if(code>=95)return 0xC58CFF;                       // Thunderstorm: electric violet.
 if((code>=51&&code<=67)||(code>=80&&code<=82))return 0x42D4F4; // Rain: aqua.
 return 0x8EC5E8;
}
lv_obj_t* icon(lv_obj_t* p,const lv_image_dsc_t* src,int x,int y,int size) {
 auto* o=lv_image_create(p); lv_image_set_src(o,src);
 lv_image_set_scale(o,256*size/src->header.w); lv_image_set_pivot(o,0,0); lv_obj_set_pos(o,x,y); return o;
}
void line(lv_layer_t* layer,float x1,float y1,float x2,float y2,uint32_t color,int width=2,int opa=255) {
 lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d); d.p1.x=x1;d.p1.y=y1;d.p2.x=x2;d.p2.y=y2;
 d.color=lv_color_hex(color);d.width=width;d.opa=opa;d.round_start=1;d.round_end=1;lv_draw_line(layer,&d);
}
void animateValue(void* var,lv_anim_exec_xcb_t exec,int32_t from,int32_t to,uint32_t duration,
                  int16_t x1,int16_t y1,int16_t x2,int16_t y2,lv_anim_completed_cb_t done=nullptr){
 lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,var);lv_anim_set_exec_cb(&a,exec);
 lv_anim_set_values(&a,from,to);lv_anim_set_duration(&a,duration);
 lv_anim_set_path_cb(&a,lv_anim_path_custom_bezier3);lv_anim_set_bezier3_param(&a,x1,y1,x2,y2);
 if(done)lv_anim_set_completed_cb(&a,done);lv_anim_start(&a);
}
constexpr uint8_t digitSegments[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
void sevenDigit(lv_layer_t* layer,int x,int y,int digit,uint32_t active){
 static const int8_t ends[7][4]={{5,1,21,1},{24,5,24,24},{24,30,24,50},{5,54,21,54},{2,30,2,50},{2,5,2,24},{5,27,21,27}};
 uint8_t mask=digit>=0&&digit<=9?digitSegments[digit]:0;
 for(int i=0;i<7;++i)line(layer,x+ends[i][0],y+ends[i][1],x+ends[i][2],y+ends[i][3],
                           mask&(1<<i)?active:0x314052,mask&(1<<i)?5:2,mask&(1<<i)?255:58);
}
}
bool AuraUi::create(){ load(Page::Home); timer_=lv_timer_create(timer,40,this);return root_!=nullptr; }
void AuraUi::show(Page p){
 if(!root_){load(p);return;}
 if(transitioning_){pendingPage_=p;return;}
 if(p==page_)return;
 pendingPage_=p;transitioning_=true;
 animateValue(this,[](void* value,int32_t opa){auto* s=static_cast<AuraUi*>(value);if(s->root_)lv_obj_set_style_opa(s->root_,opa,0);},255,0,180,410,0,1024,1024,leaveCompleted);
 animateValue(root_,[](void* obj,int32_t x){lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj),x,0);},0,-10,160,410,0,1024,1024);
}
void AuraUi::leaveCompleted(lv_anim_t* anim){auto* self=static_cast<AuraUi*>(anim->var);self->load(self->pendingPage_);}
void AuraUi::load(Page p){
 auto* old=root_; page_=p;transitioning_=false;
 clock_=date_=weather_=weatherIcon_=visual_=secondsVisual_=weatherFlow_=dateFlow_=status_=nullptr;
 change_=wifiIcon_=dateStart_=dateEnd_=dateDial_=nullptr;
 for(int i=0;i<5;++i)rangeButtons_[i]=rangeLabels_[i]=nullptr;
 for(auto& card:menuCards_)card=nullptr;
 root_=lv_obj_create(nullptr);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_style_bg_color(root_,lv_color_hex(bg),0);lv_obj_set_style_bg_opa(root_,255,0);
 lv_obj_set_style_text_color(root_,lv_color_hex(text),0);
 switch(p){case Page::Home:buildHome();break;case Page::Menu:buildMenu();break;case Page::Stocks:buildStocks();break;case Page::Spectrum:buildSpectrum();break;case Page::Agent:buildAgent();break;}
 lv_screen_load(root_); if(old) lv_obj_delete(old); renderData();
 lv_obj_set_style_opa(root_,0,0);lv_obj_set_style_translate_x(root_,10,0);
 animateValue(root_,[](void* obj,int32_t value){lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj),value,0);},0,255,220,0,0,205,1024);
 animateValue(root_,[](void* obj,int32_t value){lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj),value,0);},10,0,220,0,0,205,1024);
}
void AuraUi::buildHome(){
 icon(root_,&ag_wallpaper,0,0,320);
 auto glass=[](lv_obj_t* p,int x,int y,int w,int h){auto* c=box(p,x,y,w,h,0x101A26);lv_obj_set_style_bg_opa(c,205,0);lv_obj_set_style_radius(c,12,0);lv_obj_set_style_border_width(c,1,0);lv_obj_set_style_border_color(c,lv_color_hex(0x4B6869),0);lv_obj_set_style_border_opa(c,130,0);return c;};
 auto* t=glass(root_,8,8,66,89);icon(t,&ag_temperature,18,8,30);
 label(t,"TEMP",11,39,55,&lv_font_montserrat_12,0xB3C6CE);label(t,"--.-°C",7,57,59,&lv_font_montserrat_16,0xFFBC83);
 auto* h=glass(root_,8,105,66,89);icon(h,&ag_humidity,18,8,30);
 label(h,"HUM",16,39,46,&lv_font_montserrat_12,0xB3C6CE);label(h,"-- %",12,57,54,&lv_font_montserrat_16,0x79E0B4);
 auto* c=glass(root_,82,8,230,110);
 visual_=box(c,5,5,149,67,0);lv_obj_set_style_bg_opa(visual_,0,0);lv_obj_add_event_cb(visual_,draw,LV_EVENT_DRAW_MAIN,this);
 date_=label(c,dateText_,10,84,143,&lv_font_montserrat_12,0xFFBC83);
 dateFlow_=box(c,157,82,67,17,0);lv_obj_set_style_bg_opa(dateFlow_,0,0);lv_obj_add_event_cb(dateFlow_,draw,LV_EVENT_DRAW_MAIN,this);
 secondsVisual_=box(c,155,4,70,72,0);lv_obj_set_style_bg_opa(secondsVisual_,0,0);lv_obj_add_event_cb(secondsVisual_,draw,LV_EVENT_DRAW_MAIN,this);
 dateDial_=label(c,"--",173,28,34,&lv_font_montserrat_20,0x8EE7C0);lv_obj_set_style_text_align(dateDial_,LV_TEXT_ALIGN_CENTER,0);
 auto* w=glass(root_,82,126,230,68);weatherIcon_=icon(w,&ag_weather_unknown,8,10,46);
 label(w,"SHENZHEN",58,9,91,&lv_font_montserrat_12,0xB3C6CE);weather_=label(w,weatherText_,58,31,91,&aurageek_font_cn_16,cyan);
 weatherFlow_=box(w,150,10,74,48,0);lv_obj_set_style_bg_opa(weatherFlow_,0,0);lv_obj_add_event_cb(weatherFlow_,draw,LV_EVENT_DRAW_MAIN,this);
 auto* net=glass(root_,8,202,304,31);wifiIcon_=icon(net,&ag_wifi_0,8,6,18);
 lv_obj_set_style_image_recolor(wifiIcon_,lv_color_hex(0x8EE7C0),0);lv_obj_set_style_image_recolor_opa(wifiIcon_,255,0);
 status_=label(net,"OFFLINE",36,8,122,&lv_font_montserrat_12,0xAEC7C4);
 label(net,"2.4G / NTP",218,8,78,&lv_font_montserrat_12,muted);
}
void AuraUi::buildMenu(){
 icon(root_,&ag_menu_wallpaper,0,0,320);
 auto* shade=box(root_,0,0,320,240,bg);lv_obj_set_style_radius(shade,0,0);lv_obj_set_style_bg_opa(shade,112,0);
 label(root_,"APPS",16,10,84,&lv_font_montserrat_16,text);
 auto* hint=label(root_,"ROTATE  •  PRESS",106,12,198,&lv_font_montserrat_12,0xBFD7E4);lv_obj_set_style_text_align(hint,LV_TEXT_ALIGN_RIGHT,0);
 const lv_image_dsc_t* icons[]={&ag_home,&ag_stock,&ag_music,&ag_agent};
 const char* names[]={"HOME","STOCKS","MUSIC","AGENT"};
 for(int i=0;i<4;i++){
  int x=12+i*77;auto* c=box(root_,x,50,70,135,i==selection_?0x17364A:0x0B1724);menuCards_[i]=c;
  lv_obj_set_style_bg_opa(c,i==selection_?225:190,0);lv_obj_set_style_border_width(c,i==selection_?2:1,0);lv_obj_set_style_border_color(c,lv_color_hex(i==selection_?cyan:0x4D7180),0);lv_obj_set_style_border_opa(c,210,0);
  lv_obj_set_style_transform_scale(c,i==selection_?274:256,0);lv_obj_set_style_translate_y(c,i==selection_?-4:0,0);
  auto* img=icon(c,icons[i],12,16,46);if(i==0||i==3){lv_obj_set_style_image_recolor(img,lv_color_hex(i==selection_?cyan:text),0);lv_obj_set_style_image_recolor_opa(img,255,0);}auto* l=label(c,names[i],0,92,70,&lv_font_montserrat_12,i==selection_?cyan:0xB8CAD8);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_add_flag(c,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(c,pressed,LV_EVENT_CLICKED,this);
 }
 auto* footer=box(root_,12,205,296,25,0x07121E);lv_obj_set_style_bg_opa(footer,205,0);lv_obj_set_style_radius(footer,12,0);
 auto* l=label(footer,"PRESS OPEN  •  KEY0 BACK",4,5,288,&lv_font_montserrat_12,0xD3E6EF);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
}
void AuraUi::buildStocks(){
 auto* top=box(root_,6,5,308,47,panel);lv_obj_set_style_radius(top,9,0);
 clock_=label(top,stock_.ticker,8,2,64,&lv_font_montserrat_20,text);
 lv_obj_add_flag(clock_,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(clock_,pressed,LV_EVENT_CLICKED,this);
 weather_=label(top,"$--",82,2,129,&lv_font_montserrat_20,text);
 change_=label(top,"-- %",214,6,88,&lv_font_montserrat_14,cyan);
 label(top,"CLOSE",8,28,56,&lv_font_montserrat_12,text);
 label(top,"MA20",85,28,57,&lv_font_montserrat_12,0xEDB25E);
 label(top,"MA55",163,28,58,&lv_font_montserrat_12,0x5EBBB1);
 label(top,"USD",267,28,35,&lv_font_montserrat_12,muted);
 const char* names[]={"6M","1Y","3Y","5Y","ALL"};
 for(int i=0;i<5;++i){auto* b=box(root_,6+i*62,58,59,22,panel);lv_obj_set_style_radius(b,6,0);rangeButtons_[i]=b;auto* l=label(b,names[i],0,3,59,&lv_font_montserrat_12,muted);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);rangeLabels_[i]=l;lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(b,pressed,LV_EVENT_CLICKED,this);}
 visual_=box(root_,40,87,273,114,0x0C1622);lv_obj_set_style_radius(visual_,0,0);lv_obj_add_event_cb(visual_,draw,LV_EVENT_DRAW_MAIN,this);
 for(int i=0;i<3;++i)scaleLabels_[i]=label(root_,"--",1,85+i*51,37,&lv_font_montserrat_12,muted);
 dateStart_=label(root_,"--",40,202,140,&lv_font_montserrat_12,muted);
 dateEnd_=label(root_,"--",205,202,109,&lv_font_montserrat_12,muted);lv_obj_set_style_text_align(dateEnd_,LV_TEXT_ALIGN_RIGHT,0);
 status_=label(root_,stock_.status,7,222,308,&lv_font_montserrat_12,muted);
}
void AuraUi::buildSpectrum(){
 label(root_,"MUSIC / SPECTRUM",16,12,290,&lv_font_montserrat_14,muted);
 visual_=box(root_,0,37,320,166,bg);lv_obj_add_event_cb(visual_,draw,LV_EVENT_DRAW_MAIN,this);
 auto* l=label(root_,"AURA",128,109,64,&lv_font_montserrat_16,text);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
 status_=label(root_,"USB AUDIO NOT CONNECTED",10,215,300,&lv_font_montserrat_14,muted);lv_obj_set_style_text_align(status_,LV_TEXT_ALIGN_CENTER,0);
}
void AuraUi::buildAgent(){
 lv_obj_set_style_bg_color(root_,lv_color_black(),0);
 auto* gif=lv_gif_create(root_);lv_gif_set_src(gif,&ag_idle);lv_obj_center(gif);
}
void AuraUi::animateMenuSelection(int previous,int current){
 for(int i=0;i<4;++i){auto* card=menuCards_[i];if(!card)continue;bool selected=i==current;
  lv_obj_set_style_bg_color(card,lv_color_hex(selected?0x17364A:0x0B1724),0);lv_obj_set_style_bg_opa(card,selected?225:190,0);
  lv_obj_set_style_border_width(card,selected?2:1,0);lv_obj_set_style_border_color(card,lv_color_hex(selected?cyan:0x4D7180),0);
 }
 if(previous>=0&&previous<4&&menuCards_[previous]){
  animateValue(menuCards_[previous],[](void* obj,int32_t v){lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(obj),v,0);},274,256,180,410,0,205,1024);
  animateValue(menuCards_[previous],[](void* obj,int32_t v){lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj),v,0);},-4,0,180,410,0,205,1024);
 }
 if(current>=0&&current<4&&menuCards_[current]){
  lv_obj_move_foreground(menuCards_[current]);
  animateValue(menuCards_[current],[](void* obj,int32_t v){lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(obj),v,0);},256,274,220,410,0,205,1024);
  animateValue(menuCards_[current],[](void* obj,int32_t v){lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj),v,0);},0,-4,220,410,0,205,1024);
 }
}
void AuraUi::rotate(int steps){if(!steps||transitioning_)return;if(page_!=Page::Menu){show(Page::Menu);return;}int previous=selection_;selection_=(selection_+steps%4+4)%4;if(previous!=selection_)animateMenuSelection(previous,selection_);}
void AuraUi::click(){if(transitioning_)return;if(page_==Page::Menu){const Page pages[]={Page::Home,Page::Stocks,Page::Spectrum,Page::Agent};show(pages[selection_]);}else if(page_==Page::Stocks){stockIndex_^=1;stock_.count=0;copy(stock_.ticker,sizeof(stock_.ticker),stockIndex_?"VOO":"QQQ");renderData();}}
void AuraUi::back(){if(transitioning_)return;if(page_==Page::Menu)show(Page::Home);else show(Page::Menu);}
void AuraUi::home(){show(Page::Home);}
void AuraUi::pressed(lv_event_t* e){auto* s=static_cast<AuraUi*>(lv_event_get_user_data(e));auto* target=lv_event_get_target_obj(e);if(s->page_==Page::Menu){for(int i=0;i<4;i++)if(target==s->menuCards_[i]){s->selection_=i;s->click();return;}}if(s->page_==Page::Stocks){if(target==s->clock_){s->click();return;}for(unsigned i=0;i<5;++i)if(target==s->rangeButtons_[i]){s->stockRange_=i;s->stock_.count=0;s->renderData();return;}}}
void AuraUi::setNetwork(bool c,bool,int rssi){if(connected_==c&&rssi_==rssi)return;connected_=c;rssi_=rssi;if(!c){setClock(false,"--:--","WAITING FOR NETWORK");setWeather(false,"未知 --℃",-1);}if(page_==Page::Home)renderData();}
void AuraUi::setClock(bool v,const char* t,const char* d){clockValid_=v;copy(time_,sizeof(time_),v?t:"--:--");copy(dateText_,sizeof(dateText_),v?d:"WAITING FOR NETWORK");if(page_==Page::Home)renderData();}
void AuraUi::setWeather(bool v,const char* t,int c){weatherValid_=v;weatherCode_=v?c:-1;copy(weatherText_,sizeof(weatherText_),v?t:"未知 --℃");if(page_==Page::Home)renderData();}
void AuraUi::setIndoorUnavailable(){}
void AuraUi::setAiPending(){}
void AuraUi::setStock(const services::StockChart& c){if(!memcmp(&stock_,&c,sizeof(c)))return;stock_=c;renderData();if(visual_&&page_==Page::Stocks)lv_obj_invalidate(visual_);}
void AuraUi::setSpectrum(const float* p,size_t n,bool a,bool d){audioActive_=a;demoAudio_=d;for(size_t i=0;i<48;i++)bands_[i]=(p&&i<n)?std::max(0.f,std::min(1.f,p[i])):0;if(page_==Page::Spectrum)renderData();}
void AuraUi::renderData(){
 if(page_==Page::Home){update(date_,dateText_);update(weather_,weatherText_);char b[32];snprintf(b,sizeof(b),connected_?"ONLINE  %d dBm":"OFFLINE",rssi_);update(status_,b);lv_image_set_src(wifiIcon_,connected_?&ag_wifi_1:&ag_wifi_0);if(clockValid_)snprintf(b,sizeof(b),"%02d",second_);else copy(b,sizeof(b),"--");update(dateDial_,b);if(visual_)lv_obj_invalidate(visual_);if(weatherIcon_){const lv_image_dsc_t* src=&ag_weather_unknown;if(weatherValid_){int c=weatherCode_;src=c==0?&ag_weather_sunny:c<=3?&ag_weather_cloudy:c>=95?&ag_weather_thunder:((c>=71&&c<=77)||(c>=85&&c<=86))?&ag_weather_snow:c>=51?&ag_weather_rain:&ag_weather_cloudy;}if(lv_image_get_src(weatherIcon_)!=src)lv_image_set_src(weatherIcon_,src);}}
 if(page_==Page::Stocks){
   update(clock_,stock_.ticker);char b[80]="--";
   if(stock_.count)snprintf(b,sizeof(b),"$%.2f",stock_.last);update(weather_,b);
   if(stock_.count)snprintf(b,sizeof(b),"%+.1f%%",stock_.change);else copy(b,sizeof(b),"-- %");update(change_,b);
   lv_obj_set_style_text_color(change_,lv_color_hex(stock_.change>=0?0x79E0B4:0xF46B71),0);
   for(unsigned i=0;i<5;++i){lv_obj_set_style_bg_color(rangeButtons_[i],lv_color_hex(i==stockRange_?0x314E51:panel),0);lv_obj_set_style_text_color(rangeLabels_[i],lv_color_hex(i==stockRange_?cyan:muted),0);}
   for(int i=0;i<3;++i){if(stock_.count)snprintf(b,sizeof(b),"%.0f",stock_.high-i*(stock_.high-stock_.low)/2);else copy(b,sizeof(b),"--");update(scaleLabels_[i],b);}
   auto formatDate=[](char* dst,uint32_t date){if(date)snprintf(dst,24,"%04u-%02u-%02u",unsigned(date/10000),unsigned(date/100%100),unsigned(date%100));else snprintf(dst,24,"--");};
   formatDate(b,stock_.count?stock_.firstDate:0);update(dateStart_,b);formatDate(b,stock_.count?stock_.lastDate:0);update(dateEnd_,b);
   if(stock_.count)snprintf(b,sizeof(b),"%u/%u DAYS | %s",unsigned(stock_.visible),unsigned(stock_.total),stock_.status);else copy(b,sizeof(b),stock_.status);update(status_,b);
 }
 if(page_==Page::Spectrum)update(status_,demoAudio_?"SIMULATED PCM / FFT PREVIEW":audioActive_?"USB PCM / LIVE FFT":"USB AUDIO NOT CONNECTED");
}
void AuraUi::timer(lv_timer_t* t){auto* s=static_cast<AuraUi*>(lv_timer_get_user_data(t));if(s->page_==Page::Spectrum)for(unsigned i=0;i<48;++i)s->peaks_[i]=std::max(s->bands_[i],s->peaks_[i]-.015f);if(s->visual_&&(s->page_==Page::Home||s->page_==Page::Spectrum))lv_obj_invalidate(s->visual_);if(s->secondsVisual_)lv_obj_invalidate(s->secondsVisual_);if(s->weatherFlow_)lv_obj_invalidate(s->weatherFlow_);if(s->dateFlow_)lv_obj_invalidate(s->dateFlow_);}
void AuraUi::draw(lv_event_t* e){
 auto* s=static_cast<AuraUi*>(lv_event_get_user_data(e));auto* layer=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(lv_event_get_target_obj(e),&a);
 float tm=lv_tick_get()/1000.f;
 if(s->page_==Page::Home){auto* target=lv_event_get_target_obj(e);
  if(target==s->visual_){int digits[4]={-1,-1,-1,-1};if(s->clockValid_&&strlen(s->time_)>=5){digits[0]=s->time_[0]-'0';digits[1]=s->time_[1]-'0';digits[2]=s->time_[3]-'0';digits[3]=s->time_[4]-'0';}sevenDigit(layer,a.x1+1,a.y1+5,digits[0],text);sevenDigit(layer,a.x1+33,a.y1+5,digits[1],text);line(layer,a.x1+66,a.y1+20,a.x1+66,a.y1+25,0xFFBC83,5);line(layer,a.x1+66,a.y1+39,a.x1+66,a.y1+44,0xFFBC83,5);sevenDigit(layer,a.x1+76,a.y1+5,digits[2],text);sevenDigit(layer,a.x1+108,a.y1+5,digits[3],text);return;}
  if(target==s->secondsVisual_){float sec=s->clockValid_?fmodf(s->second_+std::min(1.f,(lv_tick_get()-s->lastClockTick_)/1000.f),60.f):0;for(int i=0;i<60;++i){float angle=2*pi*i/60-pi/2;float r=i%5?29:27;line(layer,a.x1+35+cosf(angle)*r,a.y1+35+sinf(angle)*r,a.x1+35+cosf(angle)*33,a.y1+35+sinf(angle)*33,i<=sec&&s->clockValid_?0x8EE7C0:0x46515E,1);}if(s->clockValid_){float angle=2*pi*sec/60-pi/2;line(layer,a.x1+35+cosf(angle)*27,a.y1+35+sinf(angle)*27,a.x1+35+cosf(angle)*34,a.y1+35+sinf(angle)*34,0xFFBC83,3);}return;}
  if(target==s->dateFlow_){static const uint32_t weekColors[7]={0x54DCE8,0x5F9EFF,0x8A7DFF,0xD174FF,0xFF769B,0xFFBC83,0x79E0B4};for(int i=0;i<7;++i){bool active=s->clockValid_&&i==s->dayIndex_;line(layer,a.x1+5+i*9.2f,a.y1+8,a.x1+7+i*9.2f,a.y1+8,weekColors[i],active?4:2,active?255:125);}return;}
  if(target==s->weatherFlow_){float phase=tm*2.2f;for(int wave=0;wave<2;++wave)for(int i=1;i<18;++i){float x0=(i-1)*4.1f,x1=i*4.1f;float y0=24+sinf(phase+(i-1)*.72f+wave*1.6f)*(wave?6:11);float y1=24+sinf(phase+i*.72f+wave*1.6f)*(wave?6:11);line(layer,a.x1+x0,a.y1+y0,a.x1+x1,a.y1+y1,wave?0x77A9B7:cyan,wave?1:2,wave?120:220);}int dot=int(fmodf(tm*22.f,70.f));float dy=24+sinf(phase+dot*.72f/4.1f)*11;line(layer,a.x1+dot,a.y1+dy,a.x1+dot+1,a.y1+dy,weatherAccent(s->weatherValid_,s->weatherCode_),4,235);return;}
 }
 if(s->page_==Page::Stocks){
   for(int i=0;i<5;++i)line(layer,a.x1,a.y1+4+i*26,a.x2,a.y1+4+i*26,0x253449,1);
   for(int i=1;i<5;++i)line(layer,a.x1+i*54,a.y1,a.x1+i*54,a.y2,0x1B293B,1);
   auto& c=s->stock_;if(c.count<2)return;float span=std::max(c.high-c.low,.01f);
   const float* series[]={c.close,c.ma20,c.ma55};uint32_t colors[]={0xEAE7D7,0xEDB25E,0x5EBBB1};
   for(int k=0;k<3;++k)for(size_t i=1;i<c.count;++i){if(!std::isfinite(series[k][i-1])||!std::isfinite(series[k][i]))continue;float x1=a.x1+2+(i-1)*268.f/(c.count-1),x2=a.x1+2+i*268.f/(c.count-1);float y1=a.y2-5-(series[k][i-1]-c.low)/span*104,y2=a.y2-5-(series[k][i]-c.low)/span*104;if(k==0||i%6<4)line(layer,x1,y1,x2,y2,colors[k],k?1:2);}
   return;
 }
 if(s->page_==Page::Spectrum){for(int i=0;i<48;i++){float angle=2*pi*i/48-pi/2;float r=44;float h=2+s->bands_[i]*32;auto color=lv_color_hsv_to_rgb((i*360/48+int(tm*12))%360,65,95);uint32_t rgb=lv_color_to_u32(color)&0xffffff;line(layer,a.x1+160+cosf(angle)*r,a.y1+83+sinf(angle)*r,a.x1+160+cosf(angle)*(r+h),a.y1+83+sinf(angle)*(r+h),rgb,3,s->audioActive_?255:65);float peak=r+4+s->peaks_[i]*32;line(layer,a.x1+160+cosf(angle)*peak,a.y1+83+sinf(angle)*peak,a.x1+160+cosf(angle)*(peak+1),a.y1+83+sinf(angle)*(peak+1),rgb,2,170);}}
}
}}

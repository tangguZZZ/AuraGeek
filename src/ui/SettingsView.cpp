#include "ui/SettingsView.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
extern "C" { LV_FONT_DECLARE(settings_font_8); LV_FONT_DECLARE(settings_font_12); LV_FONT_DECLARE(settings_font_13); LV_FONT_DECLARE(settings_font_25); LV_FONT_DECLARE(settings_font_48); }
namespace aurageek { namespace ui { namespace {
constexpr uint32_t ink=0xf2f2f2,dim=0x8a8a8a,faint=0x4b4b4b,hair=0x1e1e1e,signal=0xdfff00;
const char* en[]={"DISPLAY","AUDIO","MOTION","SPECTRUM","SYSTEM","WEB CONFIG"};
const char* cn[]={"显示","声音","动效","频谱","系统","WEB"};
const char* desc[]={"屏幕背光强度","喇叭播放音量","界面过渡响应","音频可视化样式","设备与固件信息","WI-FI / PERSONALIZE / AI"};
int boundedStep(int value,int steps,int increment,int low,int high){return int(std::clamp<int64_t>(int64_t(value)+int64_t(steps)*increment,low,high));}
void spring(float& p,float& v,float target,float dt){
 const float d=p-target,a=v+11*d,e=std::exp(-11*dt),c=std::cos(7*dt),s=std::sin(7*dt);
 p=target+e*(d*c+a*s/7);v=e*(v*c-(11*v+170*d)*s/7);
 if(std::fabs(p-target)<.0001f&&std::fabs(v)<.001f){p=target;v=0;}
}
void text(lv_layer_t* l,const char* t,int x,int y,int w,const lv_font_t* f,uint32_t color,int opa=255,lv_text_align_t align=LV_TEXT_ALIGN_LEFT,int spacing=0){
 lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.text=t;d.text_local=1;d.font=f;d.color=lv_color_hex(color);d.opa=opa;d.align=align;
 d.letter_space=spacing;lv_area_t a={x,y,x+w-1,y+f->line_height+2};lv_draw_label(l,&d,&a);
}
void line(lv_layer_t* l,float x,float y,float x2,float y2,uint32_t col,int width=1,int opa=255){
 lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.p1={int(lroundf(x)),int(lroundf(y))};d.p2={int(lroundf(x2)),int(lroundf(y2))};d.color=lv_color_hex(col);d.width=width;d.opa=opa;d.round_start=d.round_end=1;lv_draw_line(l,&d);
}
void valueText(const UserSettings& s,int i,char* out){
 if(i==0)snprintf(out,24,"%u%%",s.brightness);else if(i==1)snprintf(out,24,"%u%%",s.volume);
 else snprintf(out,24,"%s",i==2?(s.transition?"BLEND":"CLEAR"):i==3?(s.spectrum?"RING":"REFLECT"):i==5?"OPEN":"INFO");
}
void icon(lv_layer_t* l,int i,float cx,float cy,float scale,uint32_t col,int opa){
 auto ln=[&](float x,float y,float xx,float yy){line(l,cx+(x-29)*scale,cy+(y-29)*scale,cx+(xx-29)*scale,cy+(yy-29)*scale,col,std::max(1,int(std::round(2.4f*scale))),opa);};
 if(i==0){ln(9,13,49,13);ln(49,13,49,42);ln(49,42,9,42);ln(9,42,9,13);ln(20,49,38,49);ln(29,42,29,49);ln(14,19,44,19);}
 else if(i==1){const int xy[]={8,31,13,31,17,19,23,44,29,10,35,48,41,23,45,31,50,31};for(int n=0;n<16;n+=2)ln(xy[n],xy[n+1],xy[n+2],xy[n+3]);}
 else if(i==2){for(int n=0;n<3;n++){float y=18+n*11;ln(9,y,48,y);float x=n==1?22:39+n*2;ln(x,y-4,x,y+4);}}
 else if(i==3){const int h[]={31,20,27,12,24,17,34};for(int n=0;n<7;n++)ln(9+n*7,43,9+n*7,h[n]);ln(8,47,52,47);}
 else{ln(12,12,46,12);ln(46,12,46,46);ln(46,46,12,46);ln(12,46,12,12);ln(21,21,37,21);ln(37,21,37,37);ln(37,37,21,37);ln(21,37,21,21);for(int n=0;n<3;n++){float q=18+n*11;ln(q,7,q,12);ln(q,46,q,51);ln(7,q,12,q);ln(46,q,51,q);}}
}
}
void SettingsView::attach(lv_obj_t* parent,UserSettings* value){
 value_=value;selected_=0;position_=velocity_=editor_=editorVelocity_=0;editing_=commit_=false;saveState_=0;tick_=0;reading_=value->brightness;
 view_=lv_obj_create(parent);lv_obj_remove_style_all(view_);lv_obj_set_size(view_,320,240);lv_obj_remove_flag(view_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(view_,draw,LV_EVENT_DRAW_MAIN,this);
}
void SettingsView::rotate(int steps){
 if(!editing_){selected_=boundedStep(selected_,steps,1,0,5);if((selected_-position_)*velocity_<0)velocity_=0;}
 else if(selected_==0)value_->brightness=boundedStep(value_->brightness,steps,1,10,100);
 else if(selected_==1)value_->volume=boundedStep(value_->volume,steps,5,0,100);
 else if(selected_==2)value_->transition=boundedStep(value_->transition,steps,1,0,1);
 else if(selected_==3)value_->spectrum=boundedStep(value_->spectrum,steps,1,0,1);
 if(view_)lv_obj_invalidate(view_);
}
void SettingsView::click(){if(!editing_){saved_=*value_;reading_=selected_==0?value_->brightness:value_->volume;readingVelocity_=0;editing_=true;saveState_=0;}else{portalRequest_=selected_==5;commit_=selected_<4&&*value_!=saved_;editing_=false;if(commit_)saveState_=1;}if(view_)lv_obj_invalidate(view_);}
bool SettingsView::back(){if(!editing_)return false;*value_=saved_;editing_=false;if(view_)lv_obj_invalidate(view_);return true;}
void SettingsView::update(uint32_t now){if(!view_)return;float dt=tick_?std::min(.05f,(now-tick_)/1000.f):.016f;tick_=now;
 float old=position_,oe=editor_,orr=reading_;spring(position_,velocity_,float(selected_),dt);spring(editor_,editorVelocity_,editing_?1.f:0.f,dt);
 if(editing_&&selected_<2)spring(reading_,readingVelocity_,selected_==0?value_->brightness:value_->volume,dt);
 if(saveState_>=2&&int32_t(now-saveMessageUntil_)>=0)saveState_=0;
 if(old!=position_||oe!=editor_||orr!=reading_||now/100!= (now-uint32_t(dt*1000))/100)lv_obj_invalidate(view_);
}
void SettingsView::draw(lv_event_t* e){
 auto* s=static_cast<SettingsView*>(lv_event_get_user_data(e));auto* l=lv_event_get_layer(e);lv_area_t a;lv_obj_get_coords(s->view_,&a);const int x=a.x1,y=a.y1;
 float p=std::max(0.f,std::min(1.f,s->editor_));int mo=int(255*(1-p)),eo=int(255*p);char b[24];
 text(l,"SETTINGS · 设置",x+16-int(14*p),y+8,220,&settings_font_8,ink,mo,LV_TEXT_ALIGN_LEFT,2);
 snprintf(b,sizeof(b),"%s · %s",en[s->selected_],cn[s->selected_]);text(l,b,x+16+int(14*(1-p)),y+8,220,&settings_font_8,dim,eo,LV_TEXT_ALIGN_LEFT,2);
 snprintf(b,sizeof(b),"%02d / 06",std::max(0,std::min(5,int(std::round(s->position_))))+1);text(l,b,x+253,y+8,51,&settings_font_8,ink);
 line(l,x,y+29,x+319,y+29,hair);line(l,x,y+208,x+319,y+208,hair);
 // Clip both moving stages to the 178 px main area; no rounded cards.
 const lv_area_t original=l->_clip_area;lv_area_t clip={x,y+30,x+319,y+207};
 clip.x1=std::max(clip.x1,original.x1);clip.y1=std::max(clip.y1,original.y1);clip.x2=std::min(clip.x2,original.x2);clip.y2=std::min(clip.y2,original.y2);
 if(clip.x1<=clip.x2&&clip.y1<=clip.y2){l->_clip_area=clip;
 line(l,x+16,y+30,x+16,y+207,hair);
 int active=std::max(0,std::min(5,int(std::round(s->position_))));
 for(int i=0;i<6;i++){
  float off=i-s->position_,dist=std::fabs(off),d=std::min(dist,1.f),opacity=std::max(0.f,1-.6f*d-std::max(0.f,dist-1)*.42f);
  int op=int(mo*opacity),xx=x-int(28*p),cy=y+119+int(std::round(off*52+14*d));if(!op)continue;
  bool sel=i==active;icon(l,i,xx+57,cy,36.f/58*(1.5f-.78f*d),sel?signal:faint,op);
  text(l,en[i],xx+94,cy-24,174,&settings_font_8,sel?dim:faint,op,LV_TEXT_ALIGN_LEFT,2);
  text(l,cn[i],xx+94,cy-9,105,sel?&settings_font_13:&settings_font_12,sel?ink:dim,op);
  valueText(*s->value_,i,b);text(l,b,xx+220,cy-6,76,&lv_font_montserrat_10,sel?ink:faint,op,LV_TEXT_ALIGN_RIGHT);
  text(l,desc[i],xx+94,cy+13,196,&settings_font_8,sel?dim:faint,op);
  if(sel){line(l,xx+94,cy+31,xx+307,cy+31,hair,1,op);line(l,xx+308,cy-1,xx+308,cy+1,signal,3,op);}
 }
 for(int i=0;i<6;i++)line(l,x+16,y+92+i*11,x+16,y+94+i*11,i==active?signal:faint,3,mo);
 int ex=x+int(28*(1-p));
 text(l,desc[s->selected_],ex+32,y+44,170,&settings_font_8,faint,eo);
 text(l,s->selected_==4?"READ ONLY":s->selected_==5?"RESTART / SETUP":"ROTATE / ADJUST",ex+211,y+44,93,&settings_font_8,faint,eo,LV_TEXT_ALIGN_RIGHT);
 if(s->selected_<2){
  snprintf(b,sizeof(b),"%d",std::clamp(int(std::round(s->reading_)),s->selected_==0?10:0,100));lv_point_t size,unitSize;lv_text_get_size(&size,b,&settings_font_48,0,0,240,LV_TEXT_FLAG_NONE);
  lv_text_get_size(&unitSize,"%",&settings_font_25,0,0,80,LV_TEXT_FLAG_NONE);
  constexpr int unitGap=6;
  const int readingX=ex+(320-size.x-unitGap-unitSize.x)/2;
  const int baseline=y+75+settings_font_48.line_height-settings_font_48.base_line;
  text(l,b,readingX,y+75,size.x+1,&settings_font_48,ink,eo);
  text(l,"%",readingX+size.x+unitGap,baseline-settings_font_25.line_height+settings_font_25.base_line,unitSize.x+1,&settings_font_25,ink,eo);
  float ratio=s->selected_==0?(s->value_->brightness-10)/90.f:s->value_->volume/100.f;
  for(int i=0;i<11;i++)line(l,ex+34+i*26.8f,y+149,ex+34+i*26.8f,y+158+(i%5==0?3:0),faint,1,eo);
  line(l,ex+34,y+153,ex+302,y+153,hair,1,eo);line(l,ex+34,y+153,ex+34+268*ratio,y+153,signal,1,eo);line(l,ex+34+268*ratio,y+152,ex+34+268*ratio,y+154,signal,3,eo);
  text(l,s->selected_==0?"10":"0 / MUTE",ex+32,y+164,80,&settings_font_8,faint,eo);
  text(l,"100",ex+242,y+164,62,&settings_font_8,faint,eo,LV_TEXT_ALIGN_RIGHT);
 }else if(s->selected_<4){
  valueText(*s->value_,s->selected_,b);text(l,b,ex+32,y+88,272,&settings_font_25,ink,eo,LV_TEXT_ALIGN_CENTER);
  line(l,ex+32,y+140,ex+304,y+140,hair,1,eo);line(l,ex+32,y+174,ex+304,y+174,hair,1,eo);
  const char* opts[2]={s->selected_==2?"CLEAR":"REFLECT",s->selected_==2?"BLEND":"RING"};int selected=s->selected_==2?s->value_->transition:s->value_->spectrum;
  for(int i=0;i<2;i++)text(l,opts[i],ex+70+i*105,y+152,95,&settings_font_8,i==selected?signal:faint,eo,LV_TEXT_ALIGN_CENTER);
 }else if(s->selected_==5){
  text(l,"OPEN WEB CONFIG?",ex+24,y+84,272,&lv_font_montserrat_16,ink,eo,LV_TEXT_ALIGN_CENTER);
  text(l,"CHAT PAUSES DURING SETUP",ex+24,y+122,272,&lv_font_montserrat_10,dim,eo,LV_TEXT_ALIGN_CENTER);
  text(l,"CLICK TO RESTART / BACK TO CANCEL",ex+12,y+157,296,&lv_font_montserrat_10,signal,eo,LV_TEXT_ALIGN_CENTER);
 }else{
  const char* keys[]={"MODEL","DISPLAY","LVGL","MEMORY"};const char* vals[]={"ESP32-S3","320 x 240","9.5.0","16M / 8M"};
  for(int i=0;i<4;i++){int yy=y+65+i*31;text(l,keys[i],ex+32,yy,110,&settings_font_8,faint,eo);text(l,vals[i],ex+156,yy,148,&settings_font_8,ink,eo,LV_TEXT_ALIGN_RIGHT);line(l,ex+32,yy+23,ex+304,yy+23,hair,1,eo);}
 }
 if(s->selected_<4)text(l,"CLICK SAVE / BACK CANCEL",ex+32,y+182,272,&settings_font_8,faint,eo,LV_TEXT_ALIGN_CENTER);
 }
 l->_clip_area=original;
 line(l,x+18,y+224,x+18,y+225,signal,3,int(184+51*std::sin(s->tick_*.0052f)));
 const char* feedback=s->saveState_==1?"SAVING...":s->saveState_==2?"SAVED":s->saveState_==3?"SAVE FAILED":nullptr;
 text(l,feedback?feedback:(p>.5?(s->selected_==4?"BACK / CLOSE":"BACK / CANCEL"):"ROTATE / SELECT"),x+27,y+219,160,&settings_font_8,feedback?signal:faint);
 text(l,"CLICK / ENTER",x+180,y+219-int(8*p),124,&settings_font_8,dim,mo,LV_TEXT_ALIGN_RIGHT);
 text(l,s->selected_==4?"CLICK / CLOSE":s->selected_==5?"CLICK / OPEN":"CLICK / SAVE",x+180,y+219+int(8*(1-p)),124,&settings_font_8,dim,eo,LV_TEXT_ALIGN_RIGHT);
}
}}

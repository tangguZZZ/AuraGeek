#include "ui/SpectrumView.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace aurageek { namespace ui {
namespace {
constexpr float pi=3.14159265359f;
uint16_t rgb565(uint32_t c){return ((c>>8)&0xf800)|((c>>5)&0x7e0)|((c>>3)&31);}
uint32_t rgb888(uint16_t c){return ((uint32_t(c&0xf800)<<8)|((c&0x7e0)<<5)|((c&31)<<3));}
uint32_t mix(uint32_t a,uint32_t b,float t){uint32_t result=0;for(int shift=0;shift<=16;shift+=8)result|=uint32_t(int((a>>shift)&255)+(int((b>>shift)&255)-int((a>>shift)&255))*t)<<shift;return result;}
}
bool SpectrumView::attach(lv_obj_t* parent){
  for(unsigned i=0;i<104;++i){float index=i*47.f/103.f,f=index-unsigned(index);interpolationLeft_[i]=powf(1-f,1.8f);interpolationRight_[i]=powf(f,1.8f);}
  for(unsigned i=0;i<=96;++i){circleX_[i]=cosf(2*pi*i/96.f);circleY_[i]=sinf(2*pi*i/96.f);}
  fullRedraw_=true;
  if(!frame_)frame_=static_cast<uint16_t*>(lv_malloc(320*240*2));
  if(!background_)background_=static_cast<uint16_t*>(lv_malloc(320*240*2));
  if(!frame_||!background_)return false;
  for(int x=0;x<320;++x){float t=x/319.f;palette_[x]=t<.5f?mix(0xff2dc8,0x765cff,t*2):mix(0x765cff,0x20e5f6,(t-.5f)*2);}
  for(int d=0;d<102;++d)fade_[d]=uint8_t(92*powf(1.f-d/102.f,1.7f));
  for(int y=0;y<240;++y)for(int x=0;x<320;++x){
    float dx=(x-163)/95.f,dy=(y-137)/76.f;
    float glow=std::max(0.f,1.f-dx*dx-dy*dy);
    uint32_t color=mix(mix(0x070a10,0x020307,y/239.f),0x302258,glow*.27f);
    if(y%4==2)color=mix(color,0x7080a0,.016f);
    background_[y*320+x]=rgb565(color);
  }
  memcpy(frame_,background_,320*240*2);
  descriptor_.header.magic=LV_IMAGE_HEADER_MAGIC;
  descriptor_.header.cf=LV_COLOR_FORMAT_RGB565;
  descriptor_.header.flags=LV_IMAGE_FLAGS_MODIFIABLE;
  descriptor_.header.w=320;descriptor_.header.h=240;descriptor_.header.stride=640;
  descriptor_.data_size=320*240*2;descriptor_.data=reinterpret_cast<uint8_t*>(frame_);
  image_=lv_image_create(parent);lv_image_set_src(image_,&descriptor_);lv_obj_set_pos(image_,0,0);
  lastFrame_=0;
  return true;
}
void SpectrumView::pixel(int x,int y,uint32_t color,unsigned alpha){
  if(!alpha||unsigned(x)>=320||unsigned(y)>=240)return;
  uint16_t& p=frame_[y*320+x];
  if(alpha>=255){p=rgb565(color);return;}
  // Blend directly at the native channel depth, retaining 8-bit opacity. Pack
  // R/B into separate 16-bit lanes so products cannot carry into each other.
  const uint16_t fg=rgb565(color);const unsigned inverse=256-alpha;
  const uint32_t src=((uint32_t(fg)&0xf800)<<5)|(fg&31);
  const uint32_t dst=((uint32_t(p)&0xf800)<<5)|(p&31);
  const uint32_t rb=((src*alpha+dst*inverse)>>8)&0x1f001f;
  const uint32_t g=(((fg&0x7e0)*alpha+(p&0x7e0)*inverse)>>8)&0x7e0;
  p=uint16_t(((rb>>5)&0xf800)|(rb&31)|g);
}
void SpectrumView::stroke(int x0,int y0,int x1,int y1,uint32_t color,unsigned alpha,int width){
  if(x0==x1){
    const int first=std::max(0,std::min(y0,y1)),last=std::min(239,std::max(y0,y1));
    if(!alpha||first>last)return;
    const uint16_t fg=rgb565(color);const unsigned inverse=256-std::min(alpha,255U);
    const uint32_t src=((uint32_t(fg)&0xf800)<<5)|(fg&31);
    for(int x=std::max(0,x0-width/2);x<=std::min(319,x0+width/2);++x){
      for(int y=first;y<=last;++y){uint16_t& p=frame_[y*320+x];
        if(alpha>=255){p=fg;continue;}
        const uint32_t dst=((uint32_t(p)&0xf800)<<5)|(p&31);
        const uint32_t rb=((src*alpha+dst*inverse)>>8)&0x1f001f;
        const uint32_t g=(((fg&0x7e0)*alpha+(p&0x7e0)*inverse)>>8)&0x7e0;
        p=uint16_t(((rb>>5)&0xf800)|(rb&31)|g);
      }
    }
    return;
  }
  int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
  for(;;){for(int w=-(width/2);w<=width/2;++w)pixel(x0+w,y0,color,alpha);
    if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
void SpectrumView::light(float x,float y,uint32_t color,unsigned alpha){
  int ix=int(floorf(x)),iy=int(floorf(y));float fx=x-ix,fy=y-iy;
  pixel(ix,iy,color,unsigned(alpha*(1-fx)*(1-fy)));
  pixel(ix+1,iy,color,unsigned(alpha*fx*(1-fy)));
  if(fy==0.f)return; // reflection scans integer rows; no invisible second-row work
  pixel(ix,iy+1,color,unsigned(alpha*(1-fx)*fy));
  pixel(ix+1,iy+1,color,unsigned(alpha*fx*fy));
}
void SpectrumView::update(const float* bands,bool active,uint32_t now){
  if(!image_)return;
  const uint32_t renderStart=lv_tick_get();
  float dt=lastFrame_?std::min((now-lastFrame_)/1000.f,.15f):.033f;lastFrame_=now;
  float energy=0.f;
  float visualTargets[104];
  if(style_==Style::Reflection)motion_.update(bands,active,dt,now/1000.f,visualTargets);
  const float attack=1.f-powf(.56f,dt/.01667f),release=1.f-powf(.9f,dt/.01667f);
  for(unsigned i=0;i<104;++i){float index=i*47.f/103.f;unsigned lo=unsigned(index),hi=std::min(lo+1,47U);
    // Narrow interpolation keeps measured peaks distinct instead of connecting
    // every frequency into a broad triangular envelope. No synthetic sound.
    float target=style_==Style::Reflection?visualTargets[i]:
      (active?std::max(bands[lo]*interpolationLeft_[i],bands[hi]*interpolationRight_[i]):0.f);
    levels_[i]+=(target-levels_[i])*(target>levels_[i]?attack:release);
    if(i<24)energy+=levels_[i]/24.f;
  }
  for(int i=0;i<48;++i)peak_[i]=std::max(levels_[i*103/47],peak_[i]-dt*.45f);
  pulse_=std::max(0.f,pulse_-dt*2.8f);
  if(style_==Style::Ring&&!fullRedraw_){
    for(int y=12;y<=228;++y)memcpy(frame_+y*320+52,background_+y*320+52,217*2);
  }else memcpy(frame_,background_,320*240*2);
  float time=now/1000.f;
  if(style_==Style::Reflection)reflection(time,energy,active);else ring(time,energy,active);
  // RGB565 has no decoder/cache work; invalidate only the graphic object.
  if(style_==Style::Ring&&!fullRedraw_){lv_area_t area{52,12,268,228};lv_obj_invalidate_area(image_,&area);}
  else lv_obj_invalidate(image_);
  fullRedraw_=false;
  renderMs_=lv_tick_get()-renderStart;
}
void SpectrumView::reflection(float t,float energy,bool active){
  float heights[104];int previousX=9,previousY=135;
  for(int i=0;i<104;++i){int x=9+i*302/103;
    heights[i]=2.5f+levels_[i]*78.f;
    int y=136-int(heights[i]);uint32_t color=palette_[x];
    // Soft falloff around a bright core; avoid the old flat five-pixel outline.
    for(int offset=-3;offset<=3;++offset){
      unsigned glow=unsigned((active?30:4)*(4-abs(offset))/4);
      stroke(x+offset,135,x+offset,y,color,glow);
    }
    stroke(x,135,x,y,color,active?unsigned(160+levels_[i]*85):60);
    if(levels_[i]>.23f)pixel(x,y-1,0xf1dcff,200);
    int cy=136-int(heights[i]*(.20f+.07f*sinf(t*3.3f+i*.34f)));
    if(i){stroke(previousX,previousY,x,cy,color,active?25:5,5);stroke(previousX,previousY,x,cy,color,active?225:50);}
    previousX=x;previousY=cy;
  }
  // Mirror only the spectrum contribution, not the glass background.
  // Every row with fractional displacement: no two-row steps or integer jumps.
  for(int d=0;d<102;++d){float depth=d/102.f;
    float offset=sinf(d*.225f+t*3.35f)*(.38f+depth*1.65f)+sinf(d*.071f-t*2.12f)*(.18f+depth*1.18f)+sinf(d*.61f+t*6.2f)*.22f+sinf(d*.34f-t*8.1f)*pulse_*depth*1.55f;
    // Inverse sampling writes each destination once instead of splatting into
    // the same PSRAM pixels repeatedly. Retain every row and fractional waves.
    const int shift=int(floorf(offset));const unsigned fraction=unsigned((offset-shift)*256);
    const int row=(135-d)*320;
    for(int x=5;x<315;++x){int sx=x-shift;
      if(sx<1||sx>=320)continue;
      uint16_t c0=frame_[row+sx],c1=frame_[row+sx-1];
      if(c0==background_[row+sx]&&c1==background_[row+sx-1])continue;
      uint32_t a=rgb888(c0),b=rgb888(c1),color=0;
      for(unsigned channel=0;channel<24;channel+=8)
        color|=((((a>>channel)&255)*(256-fraction)+((b>>channel)&255)*fraction)>>8)<<channel;
      pixel(x,138+d,color,std::min(255U,unsigned(fade_[d])*5/4));}
  }
  for(int x=8;x<312;++x){unsigned alpha=unsigned((.22f+energy*.12f+pulse_*.15f)*255);pixel(x,136,palette_[x],alpha);pixel(x,137,palette_[x],alpha/2);}
  for(int i=0;i<7;++i){int y=150+i*12+int(sinf(t*1.1f+i)*1.8f),half=30+i*11;
    for(int x=160-half;x<=160+half;++x)pixel(x,y,palette_[x],unsigned(10*(1.f-abs(x-160)/float(half))*(1.f-i/8.f)));}
  for(int i=0;i<14;++i){int x=(17+i*43)%304+8,y=16+(i*37)%84;pixel(x,y,0x84aaff,active?18:8);}
}
void SpectrumView::ring(float t,float energy,bool active){
  // Explicit decorative breathing remains visible at silence; bars still use real PCM only.
  float radius=48.f+2.f*sinf(t*2.1f)+energy*9.f+pulse_*5.f;
  for(int i=0;i<96;++i){
    stroke(int(160+circleX_[i]*(radius-5)),int(120+circleY_[i]*(radius-5)),int(160+circleX_[i+1]*(radius-5)),int(120+circleY_[i+1]*(radius-5)),palette_[i*319/95],70);}
  for(int i=0;i<48;++i){float cx=circleX_[(i*2+72)%96],sy=circleY_[(i*2+72)%96],amplitude=levels_[i*103/47];
    float h=2+amplitude*32;uint32_t color=lv_color_to_u32(lv_color_hsv_to_rgb((i*360/48+int(t*12))%360,65,95))&0xffffff;
    int x=int(160+cx*radius),y=int(120+sy*radius),xx=int(160+cx*(radius+h)),yy=int(120+sy*(radius+h));
    stroke(x,y,xx,yy,color,active?30:12,7);stroke(x,y,xx,yy,color,active?255:80,3);
    float r=radius+4+peak_[i]*32;pixel(int(160+cx*r),int(120+sy*r),color,170);
  }
}
}}

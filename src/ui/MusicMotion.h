#pragma once
#include <algorithm>
#include <cmath>

namespace aurageek { namespace ui {
// Audio-reactive artwork, deliberately NOT a frequency-axis measurement.
// HTML's five envelopes and independent shimmer are gated by real audio energy.
class MusicMotion {
 public:
  void update(const float* bands,bool active,float dt,float time,float* targets) {
    if(!ready_){for(unsigned i=0;i<104;++i){float x=i/103.f;
      envelopes_[i][0]=gauss(x,.055f,.075f);envelopes_[i][1]=gauss(x,.185f,.045f);
      envelopes_[i][2]=gauss(x,.48f,.055f);envelopes_[i][3]=gauss(x,.61f,.032f);envelopes_[i][4]=gauss(x,.82f,.060f);
      phases_[i]=std::fmod(i*2.39996323f,6.2831853f);
    }ready_=true;}
    float bass=0,mid=0,high=0;
    if(active&&bands)for(unsigned i=0;i<48;++i){
      float v=std::max(0.f,std::min(1.f,bands[i]));
      if(i<10)bass=std::max(bass,v);else if(i<30)mid=std::max(mid,v);else high=std::max(high,v);
    }
    const float loudest=std::max(bass,std::max(mid,high));
    const float drive=std::max(0.f,std::min(1.f,(loudest-.035f)*1.6f));
    dt=std::max(0.f,std::min(.15f,dt));
    const float onset=std::max(0.f,bass-bassAverage_)*4.f;
    bassAverage_+=(bass-bassAverage_)*(1.f-std::exp(-dt*3.f));
    beat_=std::max(std::min(1.4f,onset),beat_*std::exp(-dt*5.f));
    const float centerA=.43f+std::sin(time*.52f)*.025f,centerB=.54f+std::sin(time*.41f+1.2f)*.022f;
    for(unsigned i=0;i<104;++i){
      const float x=i/103.f,phase=phases_[i];const float* e=envelopes_[i];
      const float envelope=.08f+
        e[0]*(.78f+beat_*.52f)+e[1]*(.31f+mid*.25f)+e[2]*(.65f+beat_*.78f)+
        e[3]*(.35f+mid*.30f)+e[4]*(.26f+high*.30f+beat_*.18f);
      const float shimmer=.58f+.18f*std::sin(time*(4.8f+x*5.7f)+phase)+
        .12f*std::sin(time*10.5f-x*31.f)+.07f*std::sin(time*19.f+i*.87f);
      const float isolated=
        gauss(x,centerA,.012f)*(.32f+beat_*.58f)+gauss(x,centerB,.010f)*(.29f+mid*.35f);
      targets[i]=drive*std::max(0.f,std::min(1.f,envelope*shimmer+isolated));
    }
  }
 private:
  static float gauss(float x,float center,float width){float v=(x-center)/width;return std::exp(-v*v);}
  float bassAverage_=0,beat_=0;
  float envelopes_[104][5]{},phases_[104]{};
  bool ready_=false;
};
}}

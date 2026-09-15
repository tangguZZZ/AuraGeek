#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace aurageek { namespace services {
class SpectrumAnalyzer {
 public:
  static constexpr unsigned kSize=512, kBands=48;
  void process(const int16_t* pcm) {
    constexpr float pi=3.14159265358979323846f;
    if(!tablesReady_){
      for(unsigned i=0;i<kSize;++i)window_[i]=.5f-.5f*cosf(2*pi*i/(kSize-1));
      for(unsigned i=0;i<kSize/2;++i){twiddleRe_[i]=cosf(-2*pi*i/kSize);twiddleIm_[i]=sinf(-2*pi*i/kSize);}
      tablesReady_=true;
    }
    float mean=0;for(unsigned i=0;i<kSize;i++)mean+=pcm[i];mean/=kSize;
    for(unsigned i=0;i<kSize;i++){re_[i]=(pcm[i]-mean)/32768.f*window_[i];im_[i]=0;}
    for(unsigned i=1,j=0;i<kSize;i++){unsigned bit=kSize>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){std::swap(re_[i],re_[j]);std::swap(im_[i],im_[j]);}}
    for(unsigned len=2;len<=kSize;len<<=1){for(unsigned i=0;i<kSize;i+=len){for(unsigned j=0;j<len/2;j++){unsigned tw=j*(kSize/len);float c=twiddleRe_[tw],s=twiddleIm_[tw];unsigned a=i+j,b=a+len/2;float r=re_[b]*c-im_[b]*s,v=re_[b]*s+im_[b]*c;re_[b]=re_[a]-r;im_[b]=im_[a]-v;re_[a]+=r;im_[a]+=v;}}}
    // Strictly disjoint bins: the former pure-log mapping repeated bin 1 across
    // several low bands. 512 samples cannot resolve 48 independent bass bands.
    // Hybrid linear/log spacing covers 93.75 Hz .. 12 kHz without duplication.
    for(unsigned b=0;b<kBands;b++){
      unsigned low=bandEdge(b),high=bandEdge(b+1);float peak=0;
      for(unsigned j=low;j<high;j++)peak=std::max(peak,sqrtf(re_[j]*re_[j]+im_[j]*im_[j])*4/kSize);
      float db=20*log10f(std::max(peak,0.00001f));
      // Suppress low-level leakage instead of stretching -65 dB noise into a hill.
      float target=std::max(0.f,std::min(1.f,(db+52)/48));
      bands_[b]+=(target-bands_[b])*(target>bands_[b]?0.78f:0.22f);
    }
  }
  const float* bands() const{return bands_;}
  static unsigned bandEdge(unsigned b){return 1+b+unsigned(powf(81.f,b/48.f)-1.f);}
  // Strongest unsmoothed FFT bin, diagnostic only (48 kHz input).
  float peakHz() const{float maxPower=0;unsigned bin=0;for(unsigned j=1;j<kSize/2;++j){float power=re_[j]*re_[j]+im_[j]*im_[j];if(power>maxPower){maxPower=power;bin=j;}}return bin*48000.f/kSize;}
 private: float re_[kSize]{},im_[kSize]{},bands_[kBands]{};
  float window_[kSize]{},twiddleRe_[kSize/2]{},twiddleIm_[kSize/2]{};
  bool tablesReady_=false;
};
}}

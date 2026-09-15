#pragma once
#include <cstdint>
#include <string>

namespace aurageek::services {
struct PortalTextRules {
  template<class Accept> static bool utf8(const std::string& s,Accept accept){
    for(size_t i=0;i<s.size();){
      uint32_t cp=uint8_t(s[i++]);unsigned count=0;uint32_t minimum=0;
      if(cp<0x80){}
      else if(cp>=0xc2&&cp<=0xdf){cp&=31;count=1;minimum=0x80;}
      else if(cp>=0xe0&&cp<=0xef){cp&=15;count=2;minimum=0x800;}
      else if(cp>=0xf0&&cp<=0xf4){cp&=7;count=3;minimum=0x10000;}
      else return false;
      if(i+count>s.size())return false;
      for(unsigned n=0;n<count;++n){uint8_t c=s[i++];if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
      if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||!accept(cp))return false;
    }
    return true;
  }
  static bool letter(uint32_t c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z');}
  static bool digit(uint32_t c){return c>='0'&&c<='9';}
  static bool han(uint32_t c){return(c>=0x3400&&c<=0x9fff)||(c>=0xf900&&c<=0xfaff);}
  static bool printable(const std::string& s,size_t limit){
    return !s.empty()&&s.size()<=limit&&utf8(s,[](uint32_t c){
      return c>=32&&!(c>=0x7f&&c<=0x9f)&&!(c>=0x200b&&c<=0x200f)&&!(c>=0x2028&&c<=0x202f)&&!(c>=0x2060&&c<=0x206f)&&c!=0xfeff&&!(c>=0xfdd0&&c<=0xfdef)&&(c&0xffff)!=0xfffe&&(c&0xffff)!=0xffff;
    });
  }
  static bool label(const std::string& s,bool chinese){
    if(s.empty()||s.size()>(chinese?32U:20U)||s.front()==' '||s.back()==' ')return false;
    bool meaningful=false;
    bool valid=utf8(s,[&](uint32_t c){
      if(letter(c)||digit(c)||(chinese&&han(c))){meaningful=true;return true;}
      return c==' '||c=='-'||c=='_'||c=='.'||c=='('||c==')'||(chinese&&(c==0xb7||c==0xff08||c==0xff09));
    });
    return valid&&meaningful;
  }
  static bool wifiPassword(const std::string& s){
    if(s.empty())return true;
    if(s.size()<8||s.size()>64)return false;
    for(unsigned char c:s){
      if(c<32||c>126)return false;
      if(s.size()==64&&!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
    }
    return true; // No trimming: spaces and symbols can be part of a real password.
  }
};
}

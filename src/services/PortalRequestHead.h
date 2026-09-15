#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace aurageek::services {
struct PortalRequestHead {
  std::string method,path,host,origin,token,contentType,revision;
  size_t length=0;
  // Deliberately accept only the subset used by the portal. Reject ambiguity,
  // chunked transfer, duplicate security headers and oversize input before body allocation.
  int parse(const std::string& header) {
    *this={};
    if(header.size()>3072)return 413;
    if(header.size()<4||header.compare(header.size()-4,4,"\r\n\r\n"))return 400;
    for(unsigned char c:header)if((c<32&&c!='\r'&&c!='\n'&&c!='\t')||c==127)return 400;
    const auto end=header.find("\r\n"),space=header.find(' '),second=header.find(' ',space+1);
    if(space==std::string::npos||second==std::string::npos||second>=end)return 400;
    method=header.substr(0,space);path=header.substr(space+1,second-space-1);
    auto version=header.substr(second+1,end-second-1);
    if((method!="GET"&&method!="POST")||(version!="HTTP/1.1"&&version!="HTTP/1.0")||path.empty()||path[0]!='/'||path.size()>192)return 400;
    for(unsigned char c:path)if(c<33||c>126)return 400;
    bool haveLength=false,haveHost=false,haveOrigin=false,haveToken=false,haveType=false,haveRevision=false;
    auto assign=[](bool& seen,std::string& target,const std::string& value){if(seen)return false;seen=true;target=value;return true;};
    for(size_t pos=end+2;pos<header.size()-2;){
      auto e=header.find("\r\n",pos),colon=header.find(':',pos);
      if(e==std::string::npos||colon==std::string::npos||colon<=pos||colon>=e||colon-pos>64||e-pos>768)return 400;
      auto name=header.substr(pos,colon-pos),value=header.substr(colon+1,e-colon-1);pos=e+2;
      for(unsigned char c:name)if(!(std::isalnum(c)||c=='-'))return 400;
      std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return std::tolower(c);});
      auto first=value.find_first_not_of(" \t"),last=value.find_last_not_of(" \t");value=first==std::string::npos?"":value.substr(first,last-first+1);
      if(value.find_first_of("\r\n")!=std::string::npos)return 400;
      if(name=="content-length"){
        if(haveLength||value.empty()||value.size()>4)return 413;
        haveLength=true;for(char c:value){if(c<'0'||c>'9')return 400;length=length*10+c-'0';}if(length>4096)return 413;
      }else if(name=="transfer-encoding"||name=="expect")return 400;
      else if(name=="host"){if(!assign(haveHost,host,value))return 400;}
      else if(name=="origin"){if(!assign(haveOrigin,origin,value))return 400;}
      else if(name=="x-aurageek-token"){if(!assign(haveToken,token,value))return 400;}
      else if(name=="x-aurageek-revision"){
        if(!assign(haveRevision,revision,value)||value.empty()||value.size()>10)return 400;
        for(char c:value)if(c<'0'||c>'9')return 400;
      }
      else if(name=="content-type"){if(!assign(haveType,contentType,value))return 400;}
    }
    if(!haveHost||host.empty()||host.size()>128||(method=="GET"&&length))return 400;
    if(method=="POST"&&(!haveLength||contentType!="application/json"))return 415;
    return 0;
  }
};
}

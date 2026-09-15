#include "services/PortalHttp.h"
#include "services/PortalRequestHead.h"
#include "services/PortalWriteBudget.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <lwip/sockets.h>

namespace aurageek::services {
void PortalHttp::reject(int status){json(status,"{\"ok\":false,\"error\":\"Invalid, oversized or timed-out HTTP request\"}");}
void PortalHttp::respond(int status,const char* type,const uint8_t* data,size_t size,bool gzip,const String& location){
  const char* reason=status==200?"OK":status==202?"Accepted":status==302?"Found":status==400?"Bad Request":status==403?"Forbidden":status==404?"Not Found":status==409?"Conflict":status==413?"Payload Too Large":status==415?"Unsupported Media Type":"Error";
  char head[768];
  const int length=snprintf(head,sizeof(head),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nX-Frame-Options: DENY\r\nContent-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'\r\n%s%s%s%s\r\n",status,reason,type,unsigned(size),gzip?"Content-Encoding: gzip\r\n":"",location.length()?"Location: ":"",location.c_str(),location.length()?"\r\n":"");
  PortalWriteBudget budget(millis());
  const int socket=client_.fd();
  auto sendNow=[socket](const uint8_t* bytes,size_t count)->int{
    if(socket<0)return -1;
    const int n=lwip_send(socket,bytes,count,MSG_DONTWAIT);
    if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return 0;
    return n;
  };
  auto clock=[](){return millis();};auto pause=[](){delay(1);};
  if(length>0&&size_t(length)<sizeof(head)&&budget.writeAll(reinterpret_cast<const uint8_t*>(head),size_t(length),sendNow,clock,pause))
    budget.writeAll(data,size,sendNow,clock,pause);
  client_.stop();buffer_="";request_=Request{};headersDone_=false;
}
bool PortalHttp::headers(){
  PortalRequestHead head;int error=head.parse(buffer_.c_str());if(error){reject(error);return false;}
  request_.method=head.method.c_str();request_.path=head.path.c_str();request_.host=head.host.c_str();request_.origin=head.origin.c_str();request_.token=head.token.c_str();request_.contentType=head.contentType.c_str();request_.revision=head.revision.c_str();length_=head.length;
  buffer_="";headersDone_=true;return true;
}
void PortalHttp::process(){
  if(!client_){auto incoming=server_.accept();if(!incoming)return;client_=incoming;client_.setTimeout(1000);client_.setConnectionTimeout(1000);
    const IPAddress remote=client_.remoteIP();
    if(client_.localIP()!=ip_||remote[0]!=ip_[0]||remote[1]!=ip_[1]||remote[2]!=ip_[2]){client_.stop();return;}
    started_=millis();headersDone_=false;length_=0;request_=Request{};buffer_="";buffer_.reserve(1024);
  }
  if(millis()-started_>8000){reject(408);return;}
  for(unsigned budget=0;budget<1024&&client_.available();++budget){char c=client_.read();
    if(!headersDone_){if(c=='\0'||buffer_.length()>=3072){reject(413);return;}buffer_+=c;
      if(buffer_.endsWith("\r\n\r\n")){if(!headers())return;if(!length_){handler_(request_);return;}}
    }else{if(c=='\0'){reject(400);return;}request_.body+=c;if(request_.body.length()==length_){handler_(request_);return;}}
  }
  if(!client_.connected()){client_.stop();buffer_="";request_=Request{};}
}
}

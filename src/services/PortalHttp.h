#pragma once
#include <NetworkServer.h>
#include <NetworkClient.h>
#include <functional>

namespace aurageek::services {
// Small bounded HTTP/1.0+1.1 adapter over Espressif's IP-bound TCP server.
// One request per connection; JSON POST only, no chunked/multipart upload.
// Limits are applied BEFORE allocating or accepting the body.
class PortalHttp {
 public:
  struct Request {String method,path,host,origin,token,contentType,body,revision;};
  explicit PortalHttp(IPAddress ip):server_(ip,80,2),ip_(ip){}
  bool begin(std::function<void(const Request&)> handler){
    handler_=handler;server_.begin();
    // NetworkServer::begin() is void, even when socket/bind/listen fails.
    if(!server_){server_.end();handler_={};return false;}
    server_.setNoDelay(true);return true;
  }
  void process();
  void respond(int status,const char* type,const uint8_t* data,size_t size,bool gzip=false,const String& location="");
  void json(int status,const String& data){respond(status,"application/json; charset=utf-8",reinterpret_cast<const uint8_t*>(data.c_str()),data.length());}
 private:
  void reject(int status);
  bool headers();
  NetworkServer server_;
  NetworkClient client_;
  IPAddress ip_;
  String buffer_;
  Request request_;
  uint32_t started_=0;
  size_t length_=0;
  bool headersDone_=false;
  std::function<void(const Request&)> handler_;
};
}

#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace aurageek::services {
struct HttpBufferFree {void operator()(char* memory)const;};
struct SecureHttpResponse {
 std::unique_ptr<char,HttpBufferFree> body;
 size_t length=0;
 int status=-1;
 uint32_t retryAfterSeconds=0;
 bool encoded=false;
 const char* error="NOT STARTED";
};
// Fixed public-data sources only. No credentials, redirects, or hidden retries.
bool secureHttpGet(const char* url,size_t bodyLimit,uint32_t timeoutMs,SecureHttpResponse& response);
}

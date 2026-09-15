#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace aurageek::services {
enum class BodyReadResult { Complete, TooLarge, Truncated, Timeout, InvalidText };
// Read is the transport's decoded HTTP-body read, never raw TCP/chunk framing.
// Storage must have capacity+1 bytes. Even unknown-length/chunked input cannot
// grow the buffer. Probe one extra byte at the limit to distinguish exact fit.
template<class Read,class Complete,class Expired,class Yield>
BodyReadResult readHttpBody(char* storage,size_t capacity,size_t& length,
                           Read read,Complete complete,Expired expired,Yield yield){
 length=0;storage[0]=0;
 for(;;){
   if(expired())return BodyReadResult::Timeout;
   // The transport may have parsed the full body while fetching headers but
   // still hold unread decoded bytes. Drain read() before testing completion.
   char probe=0;const size_t available=std::min<size_t>(1024,capacity-length);
   const int n=read(available?storage+length:&probe,available?available:1);
   if(n<0)return BodyReadResult::Truncated;
   if(n==0)return complete()?BodyReadResult::Complete:BodyReadResult::Truncated;
   if(size_t(n)>(available?available:0))return BodyReadResult::TooLarge;
   if(memchr(storage+length,0,size_t(n)))return BodyReadResult::InvalidText;
   length+=size_t(n);storage[length]=0;yield();
 }
}
}

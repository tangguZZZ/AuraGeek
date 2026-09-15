#include "services/HttpBodyReader.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
using namespace aurageek::services;

int main(){
 auto test=[](const std::string& input,size_t cap,size_t chunk,bool wireComplete,int expireAfter,bool parsedEarly=false){
   std::vector<char> storage(cap+3,'!');size_t pos=0,length=0,reads=0;
   const auto result=readHttpBody(storage.data(),cap,length,
     [&](char* dst,size_t available){++reads;const size_t n=std::min({available,chunk,input.size()-pos});memcpy(dst,input.data()+pos,n);pos+=n;return int(n);},
     [&](){return parsedEarly||(wireComplete&&pos==input.size());},
     [&](){return expireAfter>=0&&int(reads)>=expireAfter;},[](){});
   assert(storage[cap+1]=='!'&&storage[cap+2]=='!');assert(length<=cap);
   if(result==BodyReadResult::Complete){assert(length==input.size());assert(std::string(storage.data(),length)==input);assert(storage[length]==0);}
   return result;
 };
 for(size_t chunk:{1u,7u,1024u,4096u}){
   assert(test("{\"data\":[1,2,3]}",40,chunk,true,-1)==BodyReadResult::Complete);
   assert(test(std::string(40,'a'),40,chunk,true,-1)==BodyReadResult::Complete);
   assert(test(std::string(41,'a'),40,chunk,true,-1)==BodyReadResult::TooLarge);
   assert(test("{}",40,chunk,false,-1)==BodyReadResult::Truncated);
   // Some HTTP clients parse buffered data during headers, before the caller reads it.
   assert(test("{\"buffered\":true}",40,chunk,true,-1,true)==BodyReadResult::Complete);
 }
 assert(test(std::string("a\0b",3),10,10,true,-1)==BodyReadResult::InvalidText);
 assert(test("{}",40,1,true,0)==BodyReadResult::Timeout);
 assert(test("abcdef",40,1,true,2)==BodyReadResult::Timeout);
 assert(test(std::string(900000,'a'),900000,1024,true,-1)==BodyReadResult::Complete);
 assert(test(std::string(900001,'a'),900000,1024,true,-1)==BodyReadResult::TooLarge);
 char data[8]{};size_t length=0;
 assert(readHttpBody(data,7,length,[](char*,size_t){return -1;},[](){return false;},[](){return false;},[](){})==BodyReadResult::Truncated);
 puts("PASS bounded HTTP: fragments, exact limit, overflow, NUL, truncation, timeout, header-buffered body and 900000-byte cap");
}

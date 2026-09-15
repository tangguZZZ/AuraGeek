#include "drivers/QuadratureDecoder.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using aurageek::drivers::QuadratureDecoder;
int main(){
 const unsigned ring[]={3,1,0,2};
 for(unsigned anchor=0;anchor<4;++anchor)for(int dir:{-1,1}){
   QuadratureDecoder q;q.reset(ring[anchor]);int total=0;
   for(int detent=0;detent<1000;++detent){
     unsigned previous=anchor;
     for(int edge=1;edge<=4;++edge){
       const unsigned next=(int(anchor)+dir*edge+8)%4;
       // Bounce BEFORE the final committed edge; a completed edge's reverse
       // bounce remains only a partial next cycle and cannot emit a reverse step.
       for(int bounce=0;bounce<detent%7;++bounce){
         int value=q.sample(ring[next]);assert(value==0||value==dir);total+=value;
         value=q.sample(ring[previous]);assert(value==0); // never a reverse detent
       }
       const int value=q.sample(ring[next]);assert(value==0||value==dir);total+=value;
       assert(q.sample(ring[next])==0);previous=next;
     }
     assert(total==dir*(detent+1));
   }
 }
 QuadratureDecoder q;q.reset(3);
 for(int i=0;i<10000;++i){assert(q.sample(1)==0);assert(q.sample(3)==0);assert(q.sample(2)==0);assert(q.sample(3)==0);}
 // Start then abandon a turn, and take the opposite complete turn.
 assert(q.sample(1)==0);assert(q.sample(0)==0);assert(q.sample(1)==0);assert(q.sample(3)==0);
 assert(q.sample(2)==0);assert(q.sample(0)==0);assert(q.sample(1)==0);assert(q.sample(3)==-1);
 assert(q.sample(0)==0&&q.invalid()==1);assert(q.sample(2)==0);assert(q.sample(3)==0);
 assert(q.sample(1)==0);assert(q.sample(0)==0);assert(q.sample(2)==0);assert(q.sample(3)==1);
 puts("PASS encoder: 8000 full cycles with edge bounce/all anchors/both directions, partial reversal, invalid-transition recovery");
}

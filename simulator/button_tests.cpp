#include "drivers/ButtonGesture.h"
#include <cassert>
#include <cstdio>
#include <utility>
#include <vector>
using Button=aurageek::drivers::ButtonGesture;
using Action=Button::Action;
using Span=std::pair<unsigned,unsigned>;
static void check(const std::vector<Span>& spans,const std::vector<Action>& expected,
                  unsigned hold=1200,unsigned doubleMs=300,uint32_t origin=0){
  Button button(hold,doubleMs);std::vector<Action> queue;
  // Recognition keeps running while the UI consumer is deliberately idle.
  for(unsigned t=0;t<2400;t+=2){
    bool down=false;for(auto span:spans)down|=t>=span.first&&t<span.second;
    auto event=button.sample(down,origin+t);if(event!=Action::None)queue.push_back(event);
  }
  assert(queue==expected);
}
int main(){
  check({},{});
  check({{40,100}},{Action::Single});
  check({{40,100},{200,260}},{Action::Double});
  check({{40,100},{400,700}},{Action::Double}); // second down qualifies, later release still valid
  check({{40,100},{440,500}},{Action::Single,Action::Single});
  check({{40,46},{50,56},{60,140}},{Action::Single}); // contact bounce
  check({{40,52}},{}); // too short to pass 20ms debounce
  check({{40,1500}},{Action::Long});
  check({{40,100},{200,1600}},{Action::Long}); // long second press cancels double
  check({{40,100},{200,260},{700,760},{900,960}},{Action::Double,Action::Double});
  check({{40,100}},{Action::Single},800,0);
  check({{40,1100}},{Action::Long},800,0);
  check({{40,100},{200,260}},{Action::Double},1200,300,0xffffff80U);
  puts("PASS 13 button scenarios: delayed consumer, debounce, second-down window, long, KEY0, rollover");
}

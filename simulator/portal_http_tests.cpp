#include "services/PortalRequestHead.h"
#include "services/PortalExitGuard.h"
#include "services/PortalScanState.h"
#include <cassert>
#include <cstdio>
using aurageek::services::PortalRequestHead;
int main(){
  using Scan=aurageek::services::PortalScanState;
  Scan scan;scan.start(100);
  assert(!scan.update(-1,101)&&scan.running());
  assert(!scan.update(8,1000)&&!scan.running()&&scan.count()==8);
  assert(!scan.update(-1,30999));
  assert(scan.update(-1,31000)&&scan.state()==Scan::State::Idle); // Abandoned results expire.
  scan.start(40000);assert(scan.update(-2,40001)&&!scan.running()); // Driver failure unlocks connect.
  scan.reset();scan.start(50000);assert(scan.update(-1,70000)&&scan.state()==Scan::State::Failed);
  scan.reset();scan.start(0xfffffff0U);assert(!scan.update(0,20)&&scan.state()==Scan::State::Ready);
  assert(scan.update(-1,30020)&&scan.state()==Scan::State::Idle); // Timer wrap, empty scan.
  aurageek::services::PortalExitGuard guard;
  assert(!guard.accepts(false,3000,800)); // Boot long press still held.
  assert(!guard.accepts(true,3100,800));
  assert(!guard.accepts(true,3699,800));
  assert(!guard.accepts(true,3700,800)); // Arming frame is discarded too.
  assert(!guard.accepts(false,3800,800)); // A queued old event cannot exit.
  assert(guard.accepts(true,4200,4190)); // A deliberate new gesture can.
  aurageek::services::PortalExitGuard wrap;
  assert(!wrap.accepts(true,0xffffff00U,0));
  assert(!wrap.accepts(false,0xffffff10U,0)); // Bounce restarts release delay.
  assert(!wrap.accepts(true,0xffffff20U,0));
  assert(!wrap.accepts(true,0x178U,0));
  assert(wrap.accepts(true,0x200U,0x190U)); // millis wraparound.
  PortalRequestHead h;
  assert(!h.parse("GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"));assert(h.method=="GET"&&h.length==0);
  auto post=[](const std::string& more){return "POST /api/config HTTP/1.1\r\nHost: 192.168.4.1\r\nContent-Type: application/json\r\n"+more+"\r\n";};
  assert(!h.parse(post("Content-Length: 4096\r\nX-AuraGeek-Token: abc\r\nOrigin: http://192.168.4.1\r\n")));assert(h.length==4096&&h.token=="abc");
  assert(h.parse(post("Content-Length: 4097\r\n"))==413);
  assert(h.parse(post("Content-Length: 9999999999999999\r\n"))==413);
  assert(h.parse(post("Content-Length: -1\r\n"))==400);
  assert(h.parse(post("Content-Length: 1\r\nContent-Length: 1\r\n"))==413);
  assert(h.parse(post("Content-Length: 1\r\nTransfer-Encoding: chunked\r\n"))==400);
  assert(h.parse(post("Content-Length: 1\r\nOrigin: a\r\nOrigin: b\r\n"))==400);
  assert(h.parse(post("Content-Length: 1\r\nHost: attacker\r\n"))==400);
  assert(h.parse(post("Content-Length: 1\r\nX-AuraGeek-Token: a\r\nX-AuraGeek-Token: b\r\n"))==400);
  assert(!h.parse(post("Content-Length: 1\r\nX-AuraGeek-Revision: 42\r\n")));assert(h.revision=="42");
  assert(h.parse(post("Content-Length: 1\r\nX-AuraGeek-Revision: 42\r\nX-AuraGeek-Revision: 43\r\n"))==400);
  assert(h.parse(post("Content-Length: 1\r\nX-AuraGeek-Revision: -1\r\n"))==400);
  assert(h.parse(post("Content-Length: 1\r\nX-AuraGeek-Revision: abc\r\n"))==400);
  assert(h.parse("POST / HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\n\r\n")==415);
  assert(h.parse("GET / HTTP/1.1\r\nHost: a\r\nContent-Length: 1\r\n\r\n")==400);
  assert(h.parse("GET / HTTP/1.1\r\nHost: a\r\n folding: x\r\n\r\n")==400);
  assert(h.parse("GET http://example.org HTTP/1.1\r\nHost: a\r\n\r\n")==400);
  assert(h.parse(std::string(4000,'x'))==413);
  puts("Portal HTTP: bounded headers/body, framing and duplicate security headers PASS");
}

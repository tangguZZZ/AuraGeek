#include "drivers/SpeechEndpoint.h"
#include <cassert>
#include <cstdio>
using aurageek::drivers::SpeechEndpoint;
int main(){
 SpeechEndpoint silence;for(int i=0;i<267;++i)silence.feed(false);assert(silence.noSpeechTimeout()&&!silence.complete());
 SpeechEndpoint word;for(int i=0;i<8;++i)word.feed(true);assert(word.heardSpeech());
 for(int i=0;i<29;++i)word.feed(false);
 assert(!word.complete());word.feed(false);assert(word.complete());
 SpeechEndpoint pause;for(int i=0;i<8;++i)pause.feed(true);for(int i=0;i<20;++i)pause.feed(false);pause.feed(true);
 for(int i=0;i<29;++i)pause.feed(false);
 assert(!pause.complete());pause.feed(false);assert(pause.complete());
 SpeechEndpoint custom(1200,6000);for(int i=0;i<199;++i)custom.feed(false);assert(!custom.noSpeechTimeout());custom.feed(false);assert(custom.noSpeechTimeout());
 SpeechEndpoint tail(1200,6000);for(int i=0;i<8;++i)tail.feed(true);for(int i=0;i<39;++i)tail.feed(false);assert(!tail.complete());tail.feed(false);assert(tail.complete());
 puts("PASS speech endpoint: silence, minimum speech, within-sentence pause, tail");
}

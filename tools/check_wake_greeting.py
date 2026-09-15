"""Bounded COM9 verification of wake greeting before microphone listening.

--acoustic captures a real user wake; default triggers only the protocol path.
Does not edit NVS or log credentials/transcripts. Does not cancel user speech.
"""
import argparse
import time
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--log', required=True)
parser.add_argument('--seconds', type=int, default=45)
parser.add_argument('--acoustic', action='store_true')
args = parser.parse_args()
with serial.Serial() as uart, Path(args.log).open('w', encoding='utf-8') as log:
    uart.port='COM9';uart.baudrate=115200;uart.timeout=.1;uart.dtr=False;uart.rts=False;uart.open()
    def send(command):
        uart.write((command+'\n').encode());log.write('> '+command+'\n');log.flush()
    send('ai status')
    if not args.acoustic:send('ai wake test')
    print('CAPTURING acoustic wake' if args.acoustic else 'CAPTURING protocol-only greeting', flush=True)
    text='';deadline=time.monotonic()+args.seconds
    while time.monotonic()<deadline:
        line=uart.readline().decode('utf-8', errors='replace')
        if line:
            text+=line;log.write(line);log.flush()
            if any(tag in line for tag in ('[AI]', '[WAKE]', '[VOICE]', 'Guru', 'assert')):print(line,end='',flush=True)
    for failure in ('Guru Meditation','assert failed','CORRUPT HEAP','Stack canary','response incomplete','turn failed'):
        assert failure not in text, failure
    requested=text.find('WAKE GREETING requested')
    if args.acoustic:
        detected=text.find('[WAKE] detected ni hao xiao chen')
        assert 0<=detected<requested, 'Actual microphone wake detection not observed'
    speaking=text.find('[AI] SPEAKING',requested)
    completed=text.find('WAKE GREETING complete',speaking)
    listening=text.find('[AI] LISTENING',completed)
    assert 0<=requested<speaking<completed<listening, 'Greeting/playback/listening sequence not observed'
    assert '[AI] LISTENING' not in text[requested:completed], 'Microphone ready before greeting finished'
    acoustic=text.find('[WAKE] detected ni hao xiao chen')
    actualRequest=text.find('WAKE GREETING requested',acoustic) if acoustic>=0 else -1
    actualComplete=text.find('WAKE GREETING complete',actualRequest) if actualRequest>=0 else -1
    actualListening=text.find('[AI] LISTENING',actualComplete) if actualComplete>=0 else -1
    observed=0<=acoustic<actualRequest<actualComplete<actualListening
    result='PASS greeting request -> voice playback complete -> LISTENING; '+('additional acoustic wake sequence observed' if observed else 'protocol-only, real acoustic wake still pending')
    print(result,flush=True);log.write(result+'\n')

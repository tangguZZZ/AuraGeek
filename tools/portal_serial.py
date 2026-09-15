"""Bounded portal diagnostics with DTR/RTS disabled before opening CH343.

Do not use default pyserial line states on a board with automatic reset wiring.
The status query is read-only. Enter/exit require an explicit command option.
"""
import argparse
import time
from pathlib import Path
import serial

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--port',default='COM9')
    parser.add_argument('--duration',type=float,default=4)
    parser.add_argument('--command',choices=['status','open','exit'],default='status')
    parser.add_argument('--log',required=True)
    args=parser.parse_args()
    if not 1<=args.duration<=60:
        parser.error('duration must be 1..60 seconds')
    target=Path(args.log)
    target.parent.mkdir(parents=True,exist_ok=True)
    with target.open('w',encoding='utf-8') as log,serial.Serial() as uart:
        uart.port=args.port;uart.baudrate=115200;uart.timeout=.1
        uart.dtr=False;uart.rts=False;uart.open()
        uart.reset_input_buffer()
        uart.write(('portal '+args.command+'\n').encode('ascii'))
        until=time.monotonic()+args.duration
        result=''
        while time.monotonic()<until:
            line=uart.readline().decode('utf-8',errors='replace')
            if line:
                result+=line;log.write(line);log.flush();print(line,end='',flush=True)
        if any(s in result for s in ['Guru Meditation','assert failed','CORRUPT HEAP','Stack canary']):
            raise RuntimeError('Firmware fault observed')
        if args.command=='status' and '[PORTAL] mode=' not in result:
            raise RuntimeError('No portal status response; no reboot or retry performed')

if __name__=='__main__':
    main()

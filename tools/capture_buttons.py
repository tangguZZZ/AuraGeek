"""Bounded EC11 capture. Disable modem control before opening; never navigate UI."""
import time
import argparse
from pathlib import Path
import serial

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--log',default='simulator/build/windows-debug/ec11-intermittent.log')
    parser.add_argument('--duration',type=float,default=45)
    parser.add_argument('--encoder',action='store_true',help='Query physical quadrature counters, never navigate')
    args=parser.parse_args()
    target=Path(args.log)
    target.parent.mkdir(parents=True,exist_ok=True)
    with target.open('w',encoding='utf-8') as log, serial.Serial() as uart:
        uart.port='COM9';uart.baudrate=115200;uart.timeout=.1
        uart.dtr=False;uart.rts=False;uart.open()
        start=time.monotonic();next_status=0
        print(f'CAPTURE READY: {args.duration:g} seconds; status queries only',flush=True)
        while time.monotonic()-start<args.duration:
            elapsed=time.monotonic()-start
            if elapsed>=next_status:
                uart.write(b'encoder status\n' if args.encoder else b'stock status\n');next_status=elapsed+10
            line=uart.readline().decode('utf-8',errors='replace')
            if line:
                message=f'{elapsed:7.3f} {line}'
                log.write(message);log.flush();print(message,end='',flush=True)
        print('CAPTURE CLOSED',flush=True)

if __name__=='__main__':main()

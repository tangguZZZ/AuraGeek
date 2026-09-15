"""Exercise real UI settings, applied outputs and NVS reload; restore initial values."""
import argparse
import re
import time
from pathlib import Path
import serial

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--log',required=True)
    args=parser.parse_args()
    with Path(args.log).open('w',encoding='utf-8') as log, serial.Serial() as uart:
        uart.port='COM9';uart.baudrate=115200;uart.timeout=.1;uart.dtr=False;uart.rts=False;uart.open()
        def collect(seconds):
            end=time.monotonic()+seconds;result=''
            while time.monotonic()<end:
                line=uart.readline().decode('utf-8',errors='replace')
                if line:result+=line;log.write(line);log.flush();print(line,end='',flush=True)
            if re.search(r'Guru Meditation|assert failed|CORRUPT HEAP|Stack canary',result):raise RuntimeError('Runtime fault')
            return result
        def command(cmd,expect=None,seconds=.35):
            log.write('> '+cmd+'\n');uart.write((cmd+'\n').encode());result=collect(seconds)
            if expect and not re.search(expect,result):raise RuntimeError('Missing '+expect)
            return result
        collect(.3)
        def status():
            raw=command('settings status',r'current brightness=')
            match=re.search(r'current brightness=(\d+) volume=(\d+) transition=(\d+) spectrum=(\d+) applied_brightness=(\d+) applied_volume=(\d+) stored_volume=(\d+) version=(\d+)',raw)
            if not match:raise RuntimeError('Unexpected settings format')
            values=list(map(int,match.groups()))
            assert values[0]==values[4] and values[1]==values[5] and values[7]==2,values
            return values[:4]
        def select(index):
            command('ui home',r'page=0',.7);command('ui settings',r'page=5',.7)
            for _ in range(index):command('ui rotate 1',r'page=5')
        def rotate(steps):
            for _ in range(abs(steps)):command('ui rotate '+str(1 if steps>0 else -1),r'page=5',.12)
        def set_value(index,value):
            current=status()[index];select(index);command('ui next');rotate((value-current)//(5 if index==1 else 1))
            command('ui next',r'save=OK' if value!=current else None)
            assert status()[index]==value
        command('ai wake off');original=status();print('BASELINE',original,flush=True)
        try:
            # Brightness previews/cancels, and both hard stops.
            select(0);command('ui next');rotate(-100);assert status()[0]==10
            rotate(100);assert status()[0]==100;command('ui back');assert status()==original
            # Volume is the actual speaker output; mute/full are bounded.
            select(1);command('ui next');rotate(-25);assert status()[1]==0
            rotate(25);assert status()[1]==100;rotate(-10);assert status()[1]==50
            command('ui back');assert status()==original
            # Save each configurable parameter, then verify all four across restart.
            changed=[original[0]-1 if original[0]>10 else 11,50 if original[1]!=50 else 55,1-original[2],1-original[3]]
            for index,value in enumerate(changed):set_value(index,value)
            select(4);command('ui next');command('ui next');assert status()==changed
            command('ui reboot',r'\[SETTINGS\] loaded',12);command('ai wake off');assert status()==changed
            print('PASS all settings, live outputs, cancel, bounds, read-only info, NVS reload',flush=True)
        finally:
            for index,value in enumerate(original):set_value(index,value)
            command('ui reboot',r'\[SETTINGS\] loaded',12);assert status()==original
            command('ai wake on');command('ui home',r'page=0',.7)
        result='PASS settings regression; all original values restored. Physical power-cycle and listening pending.'
        print(result,flush=True);log.write(result+'\n')
if __name__=='__main__':main()

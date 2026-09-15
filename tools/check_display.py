"""Bounded on-device UI regression and transport metrics over the CH343 bench port.

No flash, no network refresh, no settings writes. Times describe LVGL batches,
not panel scan FPS. Stop other serial monitors before running.
"""
import argparse
import re
import time
from pathlib import Path
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', default='COM9')
    parser.add_argument('--log', required=True)
    parser.add_argument('--stocks-only', action='store_true', help='Check stock page/ranges only; do not change saved spectrum preferences')
    args = parser.parse_args()
    path = Path(args.log)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', encoding='utf-8') as log, serial.Serial() as uart:
        uart.port = args.port
        uart.baudrate = 115200
        uart.timeout = .1
        uart.dtr = False
        uart.rts = False
        uart.open()

        def collect(seconds):
            result = ''
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                line = uart.readline().decode('utf-8', errors='replace')
                if line:
                    result += line
                    log.write(line)
                    log.flush()
                    print(line, end='', flush=True)
            if re.search(r'Guru Meditation|assert failed|CORRUPT HEAP|Stack canary|watchdog', result, re.I):
                raise RuntimeError('Device runtime fault')
            return result

        def command(text, expect=None, wait=1):
            print('> ' + text, flush=True)
            log.write('> ' + text + '\n')
            uart.write((text + '\n').encode())
            result = collect(wait)
            if expect and not re.search(expect, result):
                raise RuntimeError('Missing response: ' + expect)
            return result

        collect(.3)
        command('ui home', r'page=0')
        if args.stocks_only:
            command('ui stocks', r'page=2')
            command('stock status', r'\[BENCH\].*bars=')
            for n in range(5):
                command('range ' + str(n), 'range=' + str(n), .6)
            command('ui next', r'page=2')
            command('ui next', r'page=2')
            command('display stats', r'\[DISPLAY\]')
            collect(3)
            print('PASS stock page and five ranges; no settings writes or forced provider requests', flush=True)
            log.write('PASS stock page and five ranges; physical typography and watchlist save still require user acceptance\n')
            return
        command('ui menu', r'page=1')
        command('display stats', r'\[DISPLAY\]')  # reset interval
        for _ in range(4):
            command('ui rotate 1', r'page=1', .6)
        command('display stats', r'\[DISPLAY\]')
        command('ui spectrum', r'page=3')
        command('display stats', r'\[DISPLAY\]')
        collect(3)
        command('display stats', r'\[DISPLAY\]')  # reflection/music-motion interval
        command('ui double', r'picker=1')
        command('ui rotate 1', r'picker=1')
        command('ui next', r'spectrum=1 picker=0')
        command('display stats', r'\[DISPLAY\]')
        collect(3)
        command('display stats', r'\[DISPLAY\]')
        command('ui double', r'picker=1')
        command('ui rotate -1', r'picker=1')
        command('ui back', r'spectrum=1 picker=0')  # cancellation
        command('ui double', r'picker=1')
        command('ui rotate -1', r'picker=1')
        command('ui next', r'spectrum=0 picker=0')
        command('ui stocks', r'page=2')
        for n in range(5):
            command('range ' + str(n), 'range=' + str(n), .4)
        command('ui home', r'page=0')
        command('stock status', r'heap=')
        print('PASS device UI navigation, spectrum selection/cancel, stock ranges', flush=True)
        log.write('PASS device UI navigation, spectrum selection/cancel, stock ranges\n')


if __name__ == '__main__':
    main()

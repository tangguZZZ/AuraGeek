"""Bounded UI-only regression. No settings writes, network requests or reset.

Manual encoder input interrupts the test rather than being misreported as a bug.
Serial state proves logical settling, not panel scan quality or physical detents.
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
    args = parser.parse_args()
    path = Path(args.log)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', encoding='utf-8') as log, serial.Serial() as uart:
        uart.port = args.port
        uart.baudrate = 115200
        uart.timeout = .05
        uart.dtr = False
        uart.rts = False
        uart.open()

        def collect(seconds):
            lines = ''
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                line = uart.readline().decode('utf-8', errors='replace')
                if not line:
                    continue
                log.write(line)
                log.flush()
                print(line, end='', flush=True)
                lines += line
            if re.search(r'Guru Meditation|assert failed|CORRUPT HEAP|Stack canary', lines):
                raise RuntimeError('Firmware fault observed')
            if '[INPUT]' in lines:
                raise RuntimeError('Physical input interrupted replay; result is inconclusive')
            return lines

        def command(value, seconds=.25):
            uart.write((value + '\n').encode('ascii'))
            log.write('> ' + value + '\n')
            return collect(seconds)

        def motion():
            value = command('ui motion')
            match = re.search(r'\[MOTION\] selection=(\d+) settled=(\d+) range=(\d+) pending=(\d+)', value)
            if not match:
                raise RuntimeError('No current motion response')
            return tuple(map(int, match.groups()))

        collect(.2)
        command('ui menu', .5)
        start = motion()[0]
        for direction in [1, -1] * 5:
            command('ui rotate ' + str(direction), .4)
            state = motion()
            start = (start + direction) % 5
            if state[:2] != (start, 1):
                raise RuntimeError('Menu selection or settled state mismatch')
        # Queue fast opposing inputs without dropping the expected detent sum.
        uart.write(('ui rotate 1\nui rotate -1\n' * 10).encode('ascii'))
        collect(.7)
        if motion()[:2] != (start, 1):
            raise RuntimeError('Rapid reversal did not return and settle')
        collect(.4)
        if motion()[:2] != (start, 1):
            raise RuntimeError('Selection moved after settling')
        command('ui stocks', .5)
        for index in range(5):
            command('range ' + str(index))
            if motion()[2:] != (index, 0):
                raise RuntimeError('Stock range did not commit')
        command('stock status')
        command('weather status')
        command('portal status')
        command('settings status')
        command('display stats')
        command('ui menu', .5)
        result = 'PASS menu detents, rapid reversal, idle stability and five committed stock ranges; physical feel remains pending'
        print(result)
        log.write(result + '\n')


if __name__ == '__main__':
    main()

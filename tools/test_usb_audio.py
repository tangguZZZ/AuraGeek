"""Play a bounded 48kHz stereo test tone ONLY into the named AuraGeek device.

Does not change Windows default sound device and does not capture user audio.
Uses Windows' public waveOut API, no UI automation or extra Python packages.
"""
import ctypes as C
from ctypes import wintypes as W
import argparse
import math
import struct
import time

class Caps(C.Structure):
    _fields_=[('mid',W.WORD),('pid',W.WORD),('version',W.DWORD),('name',W.WCHAR*32),('formats',W.DWORD),('channels',W.WORD),('reserved',W.WORD),('support',W.DWORD)]
class Format(C.Structure):
    _pack_=1
    _fields_=[('tag',W.WORD),('channels',W.WORD),('rate',W.DWORD),('bytes',W.DWORD),('align',W.WORD),('bits',W.WORD),('extra',W.WORD)]
class Header(C.Structure):
    _fields_=[('data',C.c_void_p),('length',W.DWORD),('recorded',W.DWORD),('user',C.c_size_t),('flags',W.DWORD),('loops',W.DWORD),('next',C.c_void_p),('reserved',C.c_size_t)]

def main():
    p=argparse.ArgumentParser();p.add_argument('--play',action='store_true');p.add_argument('--seconds',type=int,default=5);p.add_argument('--device-name',default='TinyUSB UAC1');p.add_argument('--frequency',type=int,default=1000);p.add_argument('--amplitude',type=int,default=2000);args=p.parse_args()
    api=C.WinDLL('winmm');api.waveOutGetDevCapsW.argtypes=[C.c_size_t,C.POINTER(Caps),W.UINT]
    api.waveOutOpen.argtypes=[C.POINTER(C.c_void_p),W.UINT,C.POINTER(Format),C.c_size_t,C.c_size_t,W.DWORD]
    for name in ('waveOutPrepareHeader','waveOutWrite','waveOutUnprepareHeader'):
        getattr(api,name).argtypes=[C.c_void_p,C.POINTER(Header),W.UINT]
    api.waveOutReset.argtypes=[C.c_void_p];api.waveOutClose.argtypes=[C.c_void_p]
    candidates=[]
    for i in range(api.waveOutGetNumDevs()):
        caps=Caps();result=api.waveOutGetDevCapsW(i,C.byref(caps),C.sizeof(caps))
        if not result:
            print(i,caps.name,flush=True)
            if args.device_name in caps.name:candidates.append(i)
    if not args.play:return
    if len(candidates)!=1:raise RuntimeError('Expected exactly one AuraGeek output; no audio sent')
    if not 1<=args.seconds<=10:raise ValueError('Duration must be 1..10 seconds')
    if not 100<=args.frequency<=10000 or not 0<=args.amplitude<=16000:raise ValueError('Frequency 100..10000 Hz; amplitude 0..16000')
    def check(code):
        if code:raise RuntimeError('waveOut error '+str(code))
    handle=C.c_void_p();fmt=Format(1,2,48000,192000,4,16,0)
    check(api.waveOutOpen(C.byref(handle),candidates[0],C.byref(fmt),0,0,0))
    data=bytearray()
    for i in range(48000*args.seconds):
        sample=int(args.amplitude*math.sin(2*math.pi*args.frequency*i/48000));data+=struct.pack('<hh',sample,sample)
    buffer=C.create_string_buffer(bytes(data));header=Header(C.cast(buffer,C.c_void_p),len(data),0,0,0,0,None,0)
    prepared=False
    try:
        check(api.waveOutPrepareHeader(handle,C.byref(header),C.sizeof(header)));prepared=True
        check(api.waveOutWrite(handle,C.byref(header),C.sizeof(header)));start=time.monotonic()
        while not header.flags&1 and time.monotonic()-start<args.seconds+5:time.sleep(.05)
        if not header.flags&1:raise RuntimeError('Audio output completion timed out')
        print(f'PASS host output: {args.seconds}s / {args.frequency}Hz / amplitude={args.amplitude} / 48000Hz / stereo S16LE',flush=True)
    finally:
        api.waveOutReset(handle)
        if prepared:api.waveOutUnprepareHeader(handle,C.byref(header),C.sizeof(header))
        api.waveOutClose(handle)

if __name__=='__main__':main()

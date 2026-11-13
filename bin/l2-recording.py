#!/usr/bin/env python3
"""Read and analyze a tail of an existing SD recording; never writes to the device."""
import argparse
import json
import math
import runpy
import struct
from pathlib import Path
import serial

query = runpy.run_path(str(Path(__file__).with_name("l2-status.py")))["query"]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", default="COM61")
parser.add_argument("--index", type=int, default=0)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--position", type=float, default=1.0)
args = parser.parse_args()

def read(port, offset, count=32):
    request = bytes([56, 131, args.index]) + struct.pack("<I", offset) + bytes([count])
    for attempt in range(3):
        body = query(port, request, 24)
        if len(body)>=11 and body[1:3]==bytes([131,args.index]) and struct.unpack_from("<I",body,3)[0]==offset:
            return struct.unpack_from("<I",body,7)[0],body[11:]
    raise RuntimeError("Recording response mismatch")

with serial.Serial(args.port,115200,timeout=0.1) as port:
    port.reset_input_buffer()
    query(port,bytes([1])+bytes(7),5)
    total,header=read(port,0,44)
    if total<44 or header[:4]!=b"RIFF":
        raise SystemExit("No recording at this index; open Audio > Recording first")
    start=max(44,min(total-8192,44+int((total-44)*max(0,min(1,args.position))))); start-=((start-44)%2)
    end=min(total,start+8192)
    pcm=bytearray()
    for offset in range(start,end,32):
        _,chunk=read(port,offset,min(32,end-offset)); pcm.extend(chunk)
samples=struct.unpack('<'+'h'*(len(pcm)//2),pcm[:len(pcm)//2*2])
mean=sum(samples)/len(samples) if samples else 0
rms=math.sqrt(sum((x-mean)**2 for x in samples)/len(samples)) if samples else 0
result={"index":args.index,"file_bytes":total,"format":struct.unpack_from('<HHIIHH',header,20),"samples":len(samples),"minimum":min(samples) if samples else 0,"maximum":max(samples) if samples else 0,"mean":mean,"ac_rms":rms,"first_samples":samples[:24]}
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.with_suffix('.pcm').write_bytes(pcm)
args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))

"""双刹车 RS485 只读原始抓包；不会修改地址、波特率或传感器配置。"""
import argparse
import datetime
import json
import math
import pathlib
import time
def crc16(data):
    result=0xFFFF
    for byte in data:
        result^=byte
        for _ in range(8):result=(result>>1)^(0xA001 if result&1 else 0)
    return result

def _serial_factory(**kwargs):
    try: import serial
    except ImportError as error: raise RuntimeError('Install pyserial==3.5 for hardware capture') from error
    return serial.Serial(**kwargs)

def read_request(address, function, register):
    if not 1 <= address <= 247 or function not in (3,4) or register not in (0,1):
        raise ValueError('Only addressed 03/04 single-register 0/1 reads allowed')
    body=bytes((address,function,0,register,0,1))
    return body+crc16(body).to_bytes(2,'little')

def capture(port,address,function,register,baud,stopbits,seconds,output,*,serial_factory=None,clock=time.monotonic_ns):
    request=read_request(address,function,register)
    if not port or baud not in (9600,115200) or stopbits not in (1,2) or not math.isfinite(seconds) or seconds<=0:
        raise ValueError('Explicit confirmed serial profile and positive finite duration required')
    target=pathlib.Path(output);target.mkdir(parents=True,exist_ok=False)
    meta=dict(port=port,address=address,function=function,register=register,baud=baud,stopbits=stopbits,
              started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              hardware_validated=False,status='starting',requests=0,rx_bytes=0)
    device=None;start=clock();next_request=start
    try:
        device=(serial_factory or _serial_factory)(port=port,baudrate=baud,bytesize=8,parity='N',stopbits=stopbits,timeout=.01,write_timeout=.02)
        with (target/'traffic.jsonl').open('x',encoding='utf-8') as traffic:
            def record(direction,b):
                traffic.write(json.dumps(dict(monotonic_ns=clock(),direction=direction,hex=bytes(b).hex()))+'\n');traffic.flush()
            while clock()-start < seconds*1e9:
                now=clock()
                if now>=next_request:
                    if device.write(request)!=8: raise OSError('Short RS485 write')
                    record('tx',request);meta['requests']+=1;next_request=now+200000000
                chunk=device.read(64)
                if chunk: record('rx',chunk);meta['rx_bytes']+=len(chunk)
        meta['status']='captured';return meta
    except BaseException:
        meta['status']='error';raise
    finally:
        try:
            if device is not None:device.close()
        finally:
            meta['elapsed_ns']=max(0,clock()-start)
            (target/'metadata.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    for field in ('address','function','register','baud','stopbits'):p.add_argument('--'+field,type=int,required=True)
    p.add_argument('--seconds',type=float,required=True);p.add_argument('--output',required=True)
    capture(**vars(p.parse_args()))

if __name__=='__main__':main()

"""Direct binary diagnostic for the Yahboom IMU USB serial bridge."""
import argparse
from collections import Counter
from datetime import datetime
import json
from pathlib import Path
import struct
import time

PROJECT=Path(__file__).resolve().parents[2]

class Decoder:
    def __init__(self):self.pending=bytearray();self.errors=0
    def feed(self,data):
        self.pending.extend(data);frames=[]
        while self.pending:
            if self.pending[0]!=0x55:
                del self.pending[0];continue
            if len(self.pending)<11:break
            p=bytes(self.pending[:11])
            if sum(p[:10])&255!=p[10]:
                self.errors+=1;del self.pending[0];continue
            del self.pending[:11]
            row={'type':f'0x{p[1]:02X}','payload_hex':p[2:10].hex()}
            if p[1]==0x5F:
                row['registers']=list(struct.unpack('<4H',p[2:10]))
            if p[1]==0x56:
                pressure,height=struct.unpack('<Ii',p[2:10])
                row.update(pressure_pa=pressure,module_height_m=height/100,
                           plausible=30000<=pressure<=110000)
            frames.append(row)
        return frames

def capture(port,decoder,raw,seconds):
    rows=[];start=time.monotonic()
    while time.monotonic()-start<seconds:
        data=port.read(port.in_waiting or 1)
        if data:
            raw.write(data);raw.flush();rows.extend(decoder.feed(data))
    return rows

def main():
    import serial
    from serial.tools import list_ports
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port')
    parser.add_argument('--baud',type=int,default=9600)
    parser.add_argument('--enable-pressure',action='store_true')
    parser.add_argument('--save',action='store_true',help='Persist only after pressure data and RSW readback succeed')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.save and not args.enable_pressure:parser.error('--save requires --enable-pressure')
    if not args.port:
        candidates=[p.device for p in list_ports.comports() if p.vid==0x10C4 and p.pid==0xEA60]
        if len(candidates)!=1:parser.error(f'Expected one CP210x IMU; found {candidates}. Specify --port.')
        args.port=candidates[0]
    out=args.output or PROJECT/'data/raw'/datetime.now().strftime('%Y%m%d_%H%M%S_%f_imu_usb')
    out.mkdir(parents=True,exist_ok=False)
    port=serial.Serial(port=None,baudrate=args.baud,timeout=.2)
    port.dtr=False;port.rts=False;port.port=args.port;port.open()
    decoder=Decoder();phases={};saved=False
    def command(address,value):
        data=bytes([0xff,0xaa,address,value&255,value>>8])
        count=port.write(data);port.flush()
        if count!=5:raise RuntimeError('Incomplete command write')
        print(f'TX {data.hex(" ")}',flush=True)
    try:
        with (out/'raw.bin').open('wb') as raw:
            phases['baseline']=capture(port,decoder,raw,2)
            command(0x27,0x02)
            phases['read_before']=capture(port,decoder,raw,2)
            if args.enable_pressure:
                command(0x69,0xb588);time.sleep(.02);command(0x02,0x005e)
                phases['configure']=capture(port,decoder,raw,2)
                command(0x27,0x02)
                phases['read_after']=capture(port,decoder,raw,3)
                check=phases['configure']+phases['read_after']
                valid=any(r.get('plausible') for r in check)
                confirmed=any(r.get('registers',[0])[0]==0x005e for r in check)
                if args.save and valid and confirmed:
                    command(0x69,0xb588);time.sleep(.02);command(0x00,0)
                    phases['after_save']=capture(port,decoder,raw,2);saved=True
                elif args.save:
                    print('Not saved: valid pressure and RSW confirmation are both required.')
    finally:port.close()
    summary={}
    for name,rows in phases.items():
        summary[name]={'types':dict(Counter(r['type'] for r in rows)),
                       'register_replies':[r['registers'] for r in rows if 'registers' in r],
                       'pressure_examples':[r for r in rows if 'pressure_pa' in r][:5]}
    report={'port':args.port,'baud':args.baud,'checksum_errors':decoder.errors,
            'saved_to_module':saved,'phases':summary}
    (out/'diagnosis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2));print(f'Direct IMU diagnostic: {out}')

if __name__=='__main__':main()

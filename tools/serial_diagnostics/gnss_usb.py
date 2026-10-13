"""Read-only GNSS USB/NMEA check; does not reconfigure the receiver."""
import argparse
from collections import Counter
from datetime import datetime
import json
import math
from pathlib import Path
import time

PROJECT=Path(__file__).resolve().parents[2]

def coordinate(value,hemisphere):
    if not value or hemisphere not in ('N','S','E','W'):return None
    raw=float(value);degrees=int(raw/100);minutes=raw-degrees*100
    limit=90 if hemisphere in ('N','S') else 180
    if not math.isfinite(raw) or raw<0 or not 0<=minutes<60 or degrees+minutes/60>limit:return None
    result=degrees+minutes/60
    return -result if hemisphere in ('S','W') else result

def decode(line):
    line=line.strip()
    if not line.startswith('$') or '*' not in line:return None
    body,check=line[1:].rsplit('*',1)
    if len(check)!=2:return None
    total=0
    for char in body:total^=ord(char)
    try:
        if total!=int(check,16):return None
    except ValueError:return None
    p=body.split(',');kind=p[0][-3:]
    row={'sentence':p[0],'kind':kind,'checksum_ok':True}
    try:
        if kind=='GGA' and len(p)>=15:
            quality=int(p[6] or 0)
            lat=coordinate(p[2],p[3]);lon=coordinate(p[4],p[5])
            fixed=quality in (1,2,4,5) and lat is not None and lon is not None
            row.update(utc=p[1],fix_quality=quality,fix_valid=fixed,
                       satellites=int(p[7]) if p[7] else None,
                       hdop=float(p[8]) if p[8] else None,
                       latitude_deg=lat if fixed else None,longitude_deg=lon if fixed else None,
                       altitude_m=float(p[9]) if fixed and p[9] and p[10]=='M' else None)
        elif kind=='RMC' and len(p)>=10:
            lat=coordinate(p[3],p[4]);lon=coordinate(p[5],p[6])
            fixed=p[2]=='A' and lat is not None and lon is not None
            row.update(utc=p[1],date_ddmmyy=p[9],status=p[2],fix_valid=fixed,
                       latitude_deg=lat if fixed else None,longitude_deg=lon if fixed else None,
                       speed_kmh=float(p[7])*1.852 if fixed and p[7] else None,
                       course_deg=float(p[8]) if fixed and p[8] else None)
        elif kind=='GSA' and len(p)>=18:
            row.update(fix_type=int(p[2] or 1),pdop=p[15],hdop=p[16],vdop=p[17])
        elif kind=='TXT':row['text']=','.join(p[4:])
    except (ValueError,IndexError,OverflowError):row['field_error']=True
    return row

def main():
    import serial
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--baud',type=int,default=9600)
    parser.add_argument('--seconds',type=float,default=20)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.seconds<=0:parser.error('--seconds must be positive')
    out=args.output or PROJECT/'data/raw'/datetime.now().strftime('%Y%m%d_%H%M%S_%f_gnss_usb')
    out.mkdir(parents=True,exist_ok=False)
    port=serial.Serial(port=None,baudrate=args.baud,timeout=.2)
    port.dtr=False;port.rts=False;port.port=args.port;port.open()
    rows=[];counts=Counter();bad=0;pending=b'';start=time.monotonic();last_print=0
    try:
        with (out/'raw.txt').open('wb') as raw:
            while time.monotonic()-start<args.seconds:
                data=port.read(port.in_waiting or 1)
                if not data:continue
                raw.write(data);pending+=data
                while b'\n' in pending:
                    line,pending=pending.split(b'\n',1)
                    text=line.decode('ascii',errors='replace').strip()
                    row=decode(text)
                    if row:
                        row['pc_receive_monotonic_ns']=time.monotonic_ns()
                        rows.append(row);counts[row['kind']]+=1
                        now=time.monotonic()
                        if row['kind'] in ('GGA','RMC') and now-last_print>=2:
                            print(text,flush=True);last_print=now
                        if row['kind']=='TXT':print(text,flush=True)
                    elif text.startswith('$'):bad+=1
                if len(pending)>8192:pending=b'';bad+=1
    finally:port.close()
    gga=next((r for r in reversed(rows) if r['kind']=='GGA'),None)
    rmc=next((r for r in reversed(rows) if r['kind']=='RMC'),None)
    fixed=sum(bool(r.get('fix_valid')) for r in rows)
    result={'port':args.port,'baud':args.baud,'capture_elapsed_s':time.monotonic()-start,
            'valid_sentence_counts':dict(counts),'bad_nmea_lines':bad,
            'fix_valid_sentence_count':fixed,'last_gga':gga,'last_rmc':rmc,
            'antenna_messages':[r['text'] for r in rows if r['kind']=='TXT' and 'ANTENNA' in r.get('text','')],
            'diagnosis':'FIX_SEEN' if fixed else ('COMMUNICATION_OK_NO_FIX' if rows else 'NO_VALID_NMEA')}
    (out/'nmea.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    (out/'diagnosis.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2));print(f'GNSS capture: {out}')

if __name__=='__main__':main()

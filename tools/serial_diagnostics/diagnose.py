"""Capture serial logs and diagnose IMU pressure/configuration automatically."""
import argparse
from collections import Counter
from datetime import datetime
import json
from pathlib import Path
import re
import sys
import time

PROJECT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(PROJECT/'tools/serial_logger'))
from imu_model import parse_imu,parse_status
from record import parse_ppg

def inspect_lines(lines):
    kinds=Counter();valid_pressure=0;pressure_samples=[];registers=[];statuses=[]
    messages=[];attempts=[];ppg=0;startup=False
    for line in lines:
        line=line.strip()
        if line.startswith('# SummitSense'):startup=True
        if line.startswith('# IMU:'):
            messages.append(line)
            match=re.search(r'configuration attempt (\d+)/3',line)
            if match:attempts.append(int(match.group(1)))
        row=parse_imu(line)
        if row:
            kinds[row['kind']]+=1
            if row['kind']=='PRESS':
                valid_pressure+=row['valid_mask'] & 3==3
                if len(pressure_samples)<3:pressure_samples.append(row)
            if row['kind']=='REG':registers.append(row)
        status=parse_status(line)
        if status:statuses.append(status)
        if parse_ppg(line):ppg+=1
    last=statuses[-1] if statuses else None
    confirmed=bool(last and last['configured']) or any(int(r['v1'])==0x5e and int(r['v2'])==6 for r in registers)
    historically_pressure=bool(last and last['pressure_frames'])
    if valid_pressure:
        diagnosis='PRESSURE_OK'
        explanation='Captured valid pressure and module-height frames. The PC display should receive them.'
    elif kinds['PRESS']:
        diagnosis='PRESSURE_INVALID_VALUES'
        explanation='Pressure frames arrive, but values fail the configured plausibility range. Inspect saved raw values and module status.'
    elif confirmed:
        diagnosis='CONFIG_CONFIRMED_NO_PRESSURE'
        explanation='Output configuration is confirmed but no pressure frame was captured. Check module model/firmware and vendor utility output.'
    elif registers:
        diagnosis='CONFIG_NOT_CONFIRMED'
        explanation='Register replies prove a bidirectional command path, but the requested output configuration is not confirmed.'
    elif kinds:
        diagnosis='NO_REGISTER_REPLY_OR_PRESSURE'
        explanation='Motion receive path works; no register reply or pressure frame was captured. Command acceptance is unverified, not a plotting failure.'
    else:
        diagnosis='NO_IMU_DATA'
        explanation='No decoded IMU data was captured. Check port, flashed firmware, module power, TX->IO18 and baud.'
    return {'diagnosis':diagnosis,'explanation':explanation,'startup_captured':startup,
            'configuration_attempts_captured':attempts,'imu_frames_by_kind':dict(kinds),
            'valid_pressure_frames':valid_pressure,'pressure_seen_in_firmware_counter':historically_pressure,
            'pressure_examples':pressure_samples,'register_replies':registers,'last_status':last,
            'ppg_records':ppg,'imu_messages':messages,
            'limitations':'No register reply in a short capture does not alone prove a wiring fault; replies may have occurred before capture. Use --reset with authorization to capture startup.'}

def capture(args,out):
    import serial
    port=serial.Serial(port=None,baudrate=args.baud,timeout=.2)
    port.dtr=False;port.rts=False;port.port=args.port
    port.open()
    lines=[];pending=b'';started=time.monotonic()
    try:
        if args.reset:
            # Normal restart only: GPIO0 stays released, pulse EN through RTS.
            port.dtr=False;port.rts=True;time.sleep(.1);port.rts=False
            print('ESP32 reset requested; module settings are not saved or changed by this tool.')
        with (out/'raw.txt').open('wb') as raw:
            while time.monotonic()-started<args.seconds:
                chunk=port.read(port.in_waiting or 1)
                if not chunk:continue
                raw.write(chunk);pending+=chunk
                while b'\n' in pending:
                    line,pending=pending.split(b'\n',1)
                    text=line.decode('utf-8',errors='replace').rstrip('\r')
                    lines.append(text)
                    if text.startswith('# IMU:') or text.startswith('IMU,REG,') or text.startswith('IMU,PRESS,'):
                        print(text,flush=True)
                if len(pending)>8192:
                    lines.append('# DIAGNOSTIC: oversized unterminated line discarded')
                    pending=b''
                raw.flush()
            if pending:lines.append(pending.decode('utf-8',errors='replace'))
    except KeyboardInterrupt:
        print('Capture stopped; saving partial report.')
    finally:port.close()
    return lines,time.monotonic()-started

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM6')
    parser.add_argument('--baud',type=int,default=115200)
    parser.add_argument('--seconds',type=float,default=15)
    parser.add_argument('--reset',action='store_true',help='Pulse ESP32 EN through board UART RTS; normal restart, not flashing')
    parser.add_argument('--input',type=Path,help='Analyze an existing raw log without touching hardware')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.seconds<=0:parser.error('--seconds must be positive')
    if args.input and args.reset:parser.error('--reset cannot be used with --input')
    out=args.output or PROJECT/'data/raw'/datetime.now().strftime('%Y%m%d_%H%M%S_%f_diagnostic')
    if args.input:
        lines=args.input.read_text(encoding='utf-8-sig',errors='replace').splitlines()
        out.mkdir(parents=True,exist_ok=False)
        (out/'raw.txt').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        elapsed=None
    else:
        out.mkdir(parents=True,exist_ok=False)
        try:lines,elapsed=capture(args,out)
        except Exception as exc:
            (out/'capture_error.txt').write_text(str(exc),encoding='utf-8')
            raise SystemExit(f'Capture failed: {exc}. Close Monitor/recorder and verify port. Report directory: {out}')
    report=inspect_lines(lines)
    report.update(port=args.port,baud=args.baud,capture_elapsed_s=elapsed,reset_requested=args.reset)
    (out/'diagnosis.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    selected=[x for x in lines if x.startswith(('# IMU:','IMU,REG,','IMU,PRESS,','IMU_STAT,'))]
    (out/'imu_filtered.txt').write_text('\n'.join(selected)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2,ensure_ascii=False))
    print(f'Diagnostic files: {out.resolve()}')

if __name__=='__main__':main()

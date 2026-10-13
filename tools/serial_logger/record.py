"""Record SummitSense serial data and generate a local SVG of the last 10 s.

Live mode requires pyserial; offline --input mode uses only the standard library.
"""
import argparse
import csv
from datetime import datetime
import json
from pathlib import Path
import time

from quality import summarize
from metric import parse_metric, METRIC_FIELDS
from imu_model import parse_imu, parse_status, summarize_imu, IMU_FIELDS

FIELDS = ['seq', 'time_us_est', 'red', 'ir', 'flags', 'fifo_lost_min',
          'queue_lost', 'i2c_errors', 'epoch']

def parse_ppg(line):
    parts = line.strip().split(',')
    if len(parts) != 10 or parts[0] != 'PPG':
        return None
    try:
        values = [int(x) for x in parts[1:]]
    except ValueError:
        return None
    if min(values) < 0 or max(values[2:4]) > 0x3ffff:
        return None
    return dict(zip(FIELDS, values))

def plot_svg(path, rows):
    if not rows:
        return
    session = rows[-1]['session']
    end = rows[-1]['time_us_est']
    rows = [r for r in rows if r['session'] == session and r['time_us_est'] >= end-10_000_000]
    start = rows[0]['time_us_est']
    span = max(1, end-start)
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="560" viewBox="0 0 1200 560">',
           '<rect width="1200" height="560" fill="white"/>',
           '<text x="60" y="30" font-family="sans-serif" font-size="20">SummitSense raw PPG - last 10 seconds (independent channel scales)</text>']
    for channel, color, top in [('red', '#d63031', 65), ('ir', '#1565c0', 310)]:
        lo = min(r[channel] for r in rows)
        hi = max(r[channel] for r in rows)
        svg.append(f'<text x="60" y="{top}" font-family="sans-serif">{channel.upper()}: raw range {lo}..{hi}</text>')
        svg.append(f'<rect x="60" y="{top+15}" width="1080" height="185" fill="#f7f7f7" stroke="#ccc"/>')
        paths, points, previous = [], [], None
        for r in rows:
            if previous and (r['seq'] != previous['seq']+1 or r['epoch'] != previous['epoch'] or r['flags'] & 11):
                if points: paths.append(points)
                points = []
            x = 60 + 1080*(r['time_us_est']-start)/span
            y = top+200 - 180*(r[channel]-lo)/max(1, hi-lo)
            points.append(f'{x:.2f},{y:.2f}')
            previous = r
        if points: paths.append(points)
        for points in paths:
            svg.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{color}" stroke-width="1"/>')
    svg.append(f'<text x="60" y="545" font-family="sans-serif">Time span: {span/1e6:.2f} s; estimated device timestamps; raw values, no HR/SpO2 calculation.</text></svg>')
    path.write_text('\n'.join(svg), encoding='utf-8')

def run(args):
    view = None
    if args.live:
        if args.input:
            raise SystemExit('--live is for live serial capture; use SVG for offline logs')
        try:
            from live_view import LiveView
            view = LiveView()
        except Exception as exc:
            raise SystemExit(f'Live view unavailable: {exc}. Run without --live.')
    out = args.output or Path('data/raw') / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
    out.mkdir(parents=True, exist_ok=False)
    source = None
    if args.input:
        source = args.input.open(encoding='utf-8', errors='replace')
    else:
        try:
            import serial
        except ImportError:
            raise SystemExit('Install pyserial: python -m pip install pyserial')
        source = serial.Serial(port=None, baudrate=args.baud, timeout=0.2)
        source.dtr = False
        source.rts = False
        source.port = args.port
        source.open()
    stats = {'ppg_records': 0, 'malformed_ppg': 0, 'sequence_gaps': 0,
             'sessions': 0, 'flagged_records': 0, 'fifo_lost_min': 0,
             'queue_lost': 0, 'i2c_errors': 0, 'duration_requested_s': args.seconds,
             'port': args.port, 'baud': args.baud, 'metric_records': 0, 'malformed_metrics': 0, 'imu_records': 0, 'malformed_imu': 0}
    rows, previous, session = [], None, 0
    imu_rows=[]; imu_statuses=[]; imu_session=0; imu_previous=None
    last_progress = 0
    started = time.monotonic()
    try:
        with (out/'raw.txt').open('w', encoding='utf-8') as raw, (out/'ppg.csv').open('w', newline='', encoding='utf-8') as ppg, (out/'system.csv').open('w', newline='', encoding='utf-8') as sysfile, (out/'metrics.csv').open('w', newline='', encoding='utf-8') as metricfile, (out/'imu.csv').open('w',newline='',encoding='utf-8') as imufile, (out/'imu_status.csv').open('w',newline='',encoding='utf-8') as imustatfile:
            writer = csv.DictWriter(ppg, fieldnames=['session', 'pc_receive_ns']+FIELDS)
            writer.writeheader()
            metricwriter=csv.DictWriter(metricfile,fieldnames=['session','pc_receive_ns']+METRIC_FIELDS)
            metricwriter.writeheader()
            imuwriter=csv.DictWriter(imufile,fieldnames=['session','pc_receive_ns']+IMU_FIELDS)
            imuwriter.writeheader()
            status_fields=['rx_us','frames','checksum_errors','discarded_bytes','uart_errors','pressure_frames','angle_frames','online','configured']
            statwriter=csv.DictWriter(imustatfile,fieldnames=['pc_receive_ns']+status_fields)
            statwriter.writeheader()
            syswriter = csv.writer(sysfile)
            syswriter.writerow(['pc_receive_ns', 'seq', 'uptime_us', 'internal_free_bytes', 'internal_min_free_bytes', 'psram_free_bytes'])
            print(f'Recording into {out.resolve()}; close VS Code Monitor first. Ctrl+C stops safely.')
            while args.input or time.monotonic()-started < args.seconds:
                if view:
                    view.update(stats)
                    if view.closed: break
                elapsed = time.monotonic()-started
                if not args.input and elapsed-last_progress >= 2:
                    print(f"{elapsed:.0f}s: samples={stats['ppg_records']}, gaps={stats['sequence_gaps']}, malformed={stats['malformed_ppg']}", flush=True)
                    raw.flush(); ppg.flush(); sysfile.flush(); metricfile.flush(); imufile.flush(); imustatfile.flush()
                    last_progress = elapsed
                line = source.readline()
                if isinstance(line, bytes): line = line.decode('utf-8', errors='replace')
                if not line:
                    if args.input: break
                    continue
                raw.write(line)
                receive_ns = time.monotonic_ns()
                sample = parse_ppg(line)
                if sample:
                    if previous is None or sample['seq'] <= previous['seq']:
                        session += 1
                        previous = None
                    if previous: stats['sequence_gaps'] += max(0, sample['seq']-previous['seq']-1)
                    sample.update(session=session, pc_receive_ns=receive_ns)
                    writer.writerow(sample)
                    rows.append(sample)
                    if view: view.add(sample)
                    previous = sample
                    stats['ppg_records'] += 1
                    stats['flagged_records'] += bool(sample['flags'])
                    stats['sessions'] = session
                    for key in ['fifo_lost_min', 'queue_lost', 'i2c_errors']:
                        stats[key] = max(stats[key], sample[key])
                elif line.startswith('PPG,'):
                    stats['malformed_ppg'] += 1
                elif line.startswith('METRIC,'):
                    metric=parse_metric(line)
                    if metric:
                        metric.update(session=session,pc_receive_ns=receive_ns)
                        metricwriter.writerow(metric)
                        stats['metric_records']+=1
                        if view: view.add_metric(metric)
                    else:
                        stats['malformed_metrics']+=1
                elif line.startswith('IMU,'):
                    imu=parse_imu(line)
                    if imu:
                        if imu_previous is None or imu['seq']<=imu_previous['seq']:
                            imu_session+=1
                        imu.update(session=imu_session,pc_receive_ns=receive_ns)
                        imuwriter.writerow(imu);imu_rows.append(imu);imu_previous=imu
                        stats['imu_records']+=1
                        if view:view.add_imu(imu)
                    else:stats['malformed_imu']+=1
                elif line.startswith('IMU_STAT,'):
                    status=parse_status(line)
                    if status:
                        status.update(pc_receive_ns=receive_ns)
                        statwriter.writerow(status);imu_statuses.append(status)
                        if view:view.add_imu_status(status)
                elif line.startswith('SYS,'):
                    parts = line.strip().split(',')
                    if len(parts) == 6:
                        try: syswriter.writerow([receive_ns]+[int(x) for x in parts[1:]])
                        except ValueError: pass
    except KeyboardInterrupt:
        print('Stopped.')
    finally:
        source.close()
        if view and not view.closed: view.close()
        stats['capture_elapsed_s'] = time.monotonic()-started
        (out/'summary.json').write_text(json.dumps(stats, indent=2), encoding='utf-8')
        (out/'quality.json').write_text(json.dumps(summarize(rows), indent=2), encoding='utf-8')
        (out/'imu_quality.json').write_text(json.dumps(summarize_imu(imu_rows,imu_statuses),indent=2),encoding='utf-8')
        plot_svg(out/'ppg.svg', rows)
        print(json.dumps(stats, indent=2))
        print('Open ppg.svg to view raw RED/IR waveforms.' if rows else 'No PPG records captured; inspect raw.txt.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live', action='store_true', help='Open live RED/IR preview while recording')
    parser.add_argument('--port', default='COM6')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--input', type=Path, help='Parse an existing text log without serial hardware')
    args = parser.parse_args()
    if args.seconds <= 0: parser.error('--seconds must be positive')
    run(args)

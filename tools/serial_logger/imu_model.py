"""IMU telemetry, per-kind quality metrics, and ZYX rotation for visualization."""
import math
from collections import defaultdict

IMU_FIELDS=['kind','seq','rx_us','v1','v2','v3','v4','valid_mask']
KINDS={'ACC','GYRO','ANGLE','MAG_RAW','PRESS','QUAT','REG'}

def parse_imu(line):
    p=line.strip().split(',')
    if len(p)!=9 or p[0]!='IMU' or p[1] not in KINDS: return None
    try:
        values=[p[1],int(p[2]),int(p[3])]+[float(x) for x in p[4:8]]+[int(p[8])]
    except ValueError: return None
    if values[1]<0 or values[2]<0 or not 0<=values[7]<=15: return None
    if not all(math.isfinite(x) for x in values[3:7]): return None
    return dict(zip(IMU_FIELDS,values))

def parse_status(line):
    p=line.strip().split(',')
    if len(p)!=10 or p[0]!='IMU_STAT': return None
    try: vals=[int(x) for x in p[1:]]
    except ValueError: return None
    if min(vals)<0 or any(x not in (0,1) for x in vals[-2:]): return None
    return dict(zip(['rx_us','frames','checksum_errors','discarded_bytes','uart_errors',
                     'pressure_frames','angle_frames','online','configured'],vals))

def rotate_vector(vector,roll,pitch,yaw):
    # Rz(yaw) Ry(pitch) Rx(roll), degrees; display convention, not a mounting calibration.
    r,p,y=map(math.radians,(roll,pitch,yaw));x0,y0,z0=vector
    x1=x0; y1=y0*math.cos(r)-z0*math.sin(r); z1=y0*math.sin(r)+z0*math.cos(r)
    x2=x1*math.cos(p)+z1*math.sin(p);y2=y1;z2=-x1*math.sin(p)+z1*math.cos(p)
    return (x2*math.cos(y)-y2*math.sin(y), x2*math.sin(y)+y2*math.cos(y), z2)

def summarize_imu(rows,statuses):
    groups=defaultdict(list)
    for r in rows: groups[(r['session'],r['kind'])].append(r)
    result=[]
    for (session,kind),data in groups.items():
        span=(data[-1]['rx_us']-data[0]['rx_us'])/1e6
        result.append({'session':session,'kind':kind,'records':len(data),'receive_span_s':span,
                       'received_hz':(len(data)-1)/span if span>0 else None,
                       'invalid_records':sum(r['valid_mask']==0 for r in data)})
    return {'per_kind':result,'last_status':statuses[-1] if statuses else None,
            'note':'IMU frame sequence is assigned by the MCU, not by sensor hardware; no sequence gap does not prove no sensor loss. rx_us is frame receive time, not simultaneous measurement time.'}

import math

METRIC_FIELDS=['time_us','hr_bpm','hr_available','spo2_est_pct','spo2_est_available',
               'calibrated','sqi','ratio','pi_ir_pct','reason']

def parse_metric(line):
    parts=line.strip().split(',')
    if len(parts)!=11 or parts[0]!='METRIC': return None
    try:
        values=[int(parts[1]),float(parts[2]),int(parts[3]),float(parts[4]),int(parts[5]),
                int(parts[6]),float(parts[7]),float(parts[8]),float(parts[9]),parts[10]]
    except ValueError: return None
    if not all(math.isfinite(v) for v in values[:-1]): return None
    if values[0]<0 or any(values[i] not in (0,1) for i in [2,4,5]): return None
    # This prototype cannot claim calibration merely because telemetry says so.
    if values[5]!=0: return None
    if values[2] and not 40<=values[1]<=180: return None
    if values[4] and not 0<=values[3]<=100: return None
    if not values[-1]: return None
    return dict(zip(METRIC_FIELDS,values))

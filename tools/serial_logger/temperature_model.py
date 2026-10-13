import math
from collections import defaultdict
import statistics

TEMP_FIELDS=['seq','rx_us','temperature_c','valid','i2c_errors','config','status']

def parse_temperature(line):
    p=line.strip().split(',')
    if len(p)!=8 or p[0]!='TEMP':return None
    try:values=[int(p[1]),int(p[2]),float(p[3]),int(p[4]),int(p[5]),int(p[6],0),p[7]]
    except ValueError:return None
    if not math.isfinite(values[2]) or min(values[0],values[1],values[4])<0 or values[3] not in (0,1):return None
    if not 0<=values[5]<=65535:return None
    if values[3] and not (-40<=values[2]<=125 and values[6]=='OK'):return None
    return dict(zip(TEMP_FIELDS,values))

def summarize_temperature(rows):
    groups=defaultdict(list)
    for row in rows:groups[row['session']].append(row)
    result=[]
    for session,data in groups.items():
        good=[r['temperature_c'] for r in data if r['valid']]
        span=(data[-1]['rx_us']-data[0]['rx_us'])/1e6
        result.append({'session':session,'records':len(data),'valid_records':len(good),
                       'receive_span_s':span,'received_hz':(len(data)-1)/span if span>0 else None,
                       'i2c_errors_at_start':data[0]['i2c_errors'],'i2c_errors_at_end':data[-1]['i2c_errors'],
                       'i2c_errors_increase':max(0,data[-1]['i2c_errors']-data[0]['i2c_errors']),
                       'min_c':min(good) if good else None,'max_c':max(good) if good else None,
                       'mean_c':statistics.fmean(good) if good else None})
    return {'sessions':result,'note':'AS6221 module/contact temperature, not validated core body temperature. rx_us is register receive time.'}

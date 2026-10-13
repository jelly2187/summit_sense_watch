"""Offline acquisition integrity metrics; does not infer physiological values."""
from collections import defaultdict
import statistics

def summarize(rows):
    groups=defaultdict(list)
    for row in rows: groups[row['session']].append(row)
    sessions=[]
    for session, data in groups.items():
        first,last=data[0],data[-1]
        span=(last['time_us_est']-first['time_us_est'])/1e6
        # Effective delivered rate includes gaps; device timestamps are estimated.
        metrics={'session':session,'records':len(data),'device_span_s':span,
                 'effective_received_hz':(len(data)-1)/span if span>0 else None,
                 'sequence_gaps':0,'nonmonotonic_timestamps':0,'epoch_changes':0,
                 'saturated_samples':sum(bool(r['flags'] & 4) or max(r['red'],r['ir'])==262143 for r in data),
                 'timestamp_adjusted_samples':sum(bool(r['flags'] & 16) for r in data)}
        intervals=[]
        for a,b in zip(data,data[1:]):
            metrics['sequence_gaps']+=max(0,b['seq']-a['seq']-1)
            dt=b['time_us_est']-a['time_us_est']
            metrics['nonmonotonic_timestamps']+=dt<=0
            metrics['epoch_changes']+=b['epoch']!=a['epoch']
            if b['seq']==a['seq']+1 and b['epoch']==a['epoch'] and not(b['flags'] & 11) and dt>0:
                intervals.append(dt)
        metrics['median_estimated_interval_us']=statistics.median(intervals) if intervals else None
        for key in ['fifo_lost_min','queue_lost','i2c_errors']:
            metrics[key+'_at_start']=first[key]
            metrics[key+'_at_end']=last[key]
            metrics[key+'_increase']=max(0,last[key]-first[key])
        for channel in ['red','ir']:
            vals=[r[channel] for r in data]
            metrics[channel]={'min':min(vals),'max':max(vals),'mean':statistics.fmean(vals)}
        sessions.append(metrics)
    return {'note':'Estimated timestamps and raw ADC statistics; numeric response is not proof of a usable pulse waveform.',
            'sessions':sessions}

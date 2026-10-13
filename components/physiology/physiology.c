#include "physiology.h"
#include <math.h>
#include <string.h>

const char *physiology_reason_name(physiology_reason_t reason)
{
    static const char *names[] = {"WARMUP", "NO_CONTACT", "DATA_GAP", "SATURATED",
        "TIMING_UNSTABLE", "SIGNAL_UNSTABLE", "WEAK_SIGNAL", "NO_PERIOD",
        "IRREGULAR_PULSE", "OK_EXPERIMENTAL", "SPO2_RATIO_RANGE"};
    return (unsigned)reason < sizeof(names)/sizeof(names[0]) ? names[reason] : "UNKNOWN";
}

void physiology_init(physiology_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->last_report = -1000000;
    ctx->last_reason = PHYS_WARMUP;
}

static void invalidate(physiology_result_t *result, int64_t time_us, physiology_reason_t reason)
{
    memset(result, 0, sizeof(*result));
    result->time_us = time_us;
    result->hr_bpm = result->spo2_est_pct = result->ratio = -1;
    result->reason = reason;
}

static uint32_t raw_at(const physiology_context_t *ctx, bool red, unsigned index)
{
    unsigned i = (ctx->write_index + index) % PHYSIOLOGY_WINDOW;
    return red ? ctx->red[i] : ctx->ir[i];
}

static float autocorrelation(const float *signal, int lag)
{
    double ab=0, aa=0, bb=0;
    for (int i=100; i<700-lag; ++i) {
        float a=signal[i], b=signal[i+lag];
        ab+=a*b; aa+=a*a; bb+=b*b;
    }
    return aa>0 && bb>0 ? (float)(ab/sqrt(aa*bb)) : 0;
}

static float median(float *values, int count)
{
    for (int i=1; i<count; ++i) {
        float value=values[i]; int j=i-1;
        while (j>=0 && values[j]>value) { values[j+1]=values[j]; --j; }
        values[j+1]=value;
    }
    return count%2 ? values[count/2] : (values[count/2-1]+values[count/2])*0.5f;
}

static void analyse(physiology_context_t *ctx, physiology_result_t *out)
{
    const unsigned n=PHYSIOLOGY_WINDOW;
    int64_t span=out->time_us-ctx->times[ctx->write_index];
    if (span<7590000 || span>8390000) { out->reason=PHYS_TIMING; return; }
    double sum_r=0, sum_i=0, cov_r=0, cov_i=0, sum_x2=0;
    uint32_t min_r=262143, min_i=262143, max_r=0, max_i=0;
    const float center=(n-1)*0.5f;
    for (unsigned j=0; j<n; ++j) {
        uint32_t r=raw_at(ctx,true,j), ir=raw_at(ctx,false,j);
        sum_r+=r; sum_i+=ir;
        float x=j-center; cov_r+=x*r; cov_i+=x*ir; sum_x2+=x*x;
        if(r<min_r) min_r=r;
        if(r>max_r) max_r=r;
        if(ir<min_i) min_i=ir;
        if(ir>max_i) max_i=ir;
    }
    float dc_r=sum_r/n, dc_i=sum_i/n;
    if (dc_r<5000 || dc_i<5000) { out->reason=PHYS_NO_CONTACT; return; }
    if (max_r>=262143 || max_i>=262143) { out->reason=PHYS_SATURATED; return; }
    float slope_r=cov_r/sum_x2, slope_i=cov_i/sum_x2;
    // Conservative prototype gates: contact transitions and large baseline changes.
    if ((max_r-min_r)/dc_r>0.10f || (max_i-min_i)/dc_i>0.10f ||
        fabsf(slope_r*n)/dc_r>0.05f || fabsf(slope_i*n)/dc_i>0.05f) {
        out->reason=PHYS_UNSTABLE; return;
    }
    // Linear DC/trend removal followed by symmetric 5-point smoothing.
    for (unsigned j=0; j<n; ++j) {
        float r=0, ir=0; unsigned count=0;
        for (int k=-2; k<=2; ++k) {
            int index=(int)j+k;
            if(index<0 || index>=(int)n) continue;
            r+=raw_at(ctx,true,index)-dc_r-slope_r*(index-center);
            ir+=raw_at(ctx,false,index)-dc_i-slope_i*(index-center);
            ++count;
        }
        ctx->ac_red[j]=r/count; ctx->ac_ir[j]=ir/count;
    }
    double rr=0, ii=0, ri=0;
    for (unsigned j=100; j<700; ++j) {
        float r=ctx->ac_red[j], ir=ctx->ac_ir[j];
        rr+=r*r; ii+=ir*ir; ri+=r*ir;
    }
    float rms_r=sqrt(rr/600), rms_i=sqrt(ii/600);
    float correlation=rr>0 && ii>0 ? ri/sqrt(rr*ii) : 0;
    out->pi_ir_pct=100*rms_i/dc_i; // RMS/DC definition, not a calibrated perfusion index.
    if (rms_r/dc_r<0.0005f || rms_i/dc_i<0.001f) { out->reason=PHYS_WEAK; return; }
    if (correlation<0.8f || rms_i/dc_i>0.05f) { out->reason=PHYS_UNSTABLE; return; }
    float scores[153]={0}, best=-1;
    for (int lag=32; lag<=151; ++lag) {
        scores[lag]=autocorrelation(ctx->ac_ir,lag);
        if(lag>=33 && lag<=150 && scores[lag]>best) best=scores[lag];
    }
    int lag_choice=0;
    // Prefer the first strong local maximum, reducing selection of longer harmonics.
    for (int lag=33; lag<=150; ++lag) {
        if(scores[lag]>=0.6f && scores[lag]>=best-0.08f &&
           scores[lag]>=scores[lag-1] && scores[lag]>=scores[lag+1]) {
            lag_choice=lag; break;
        }
    }
    if(!lag_choice) { out->reason=PHYS_NO_PERIOD; return; }
    int peaks[32], peak_count=0;
    int distance=(int)(lag_choice*0.6f);
    for (int j=101; j<699; ++j) {
        // Optical ADC pulse troughs are easier to isolate than broad maxima.
        float v=-ctx->ac_ir[j];
        if(v<0.35f*rms_i || v<=-ctx->ac_ir[j-1] || v<-ctx->ac_ir[j+1]) continue;
        if(peak_count && j-peaks[peak_count-1]<distance) {
            if(v>-ctx->ac_ir[peaks[peak_count-1]]) peaks[peak_count-1]=j;
        } else if(peak_count<32) peaks[peak_count++]=j;
    }
    if(peak_count<3) { out->reason=PHYS_NO_PERIOD; return; }
    float intervals[31];
    for(int j=1; j<peak_count; ++j) intervals[j-1]=peaks[j]-peaks[j-1];
    float period=median(intervals,peak_count-1);
    double variation=0;
    for(int j=0; j<peak_count-1; ++j) variation+=(intervals[j]-period)*(intervals[j]-period);
    float cv=sqrt(variation/(peak_count-1))/period;
    float bpm=60*PHYSIOLOGY_FS/period;
    if(bpm<40 || bpm>180 || cv>0.2f || fabsf(period-lag_choice)/lag_choice>0.12f) {
        out->reason=PHYS_IRREGULAR; return;
    }
    out->hr_bpm=bpm; out->hr_available=true;
    out->sqi=100*fminf(scores[lag_choice],correlation)*(1-cv);
    out->ratio=(rms_r/dc_r)/(rms_i/dc_i);
    // Experimental reference polynomial, not calibrated for this module/optical path.
    // See docs/physiology.md for provenance, AC definition and validation limitations.
    float spo2=-45.060f*out->ratio*out->ratio+30.354f*out->ratio+94.845f;
    if(out->ratio>=0.2f && out->ratio<=1.8f && isfinite(spo2) && spo2>=0 && spo2<=100) {
        out->spo2_est_pct=spo2; out->spo2_est_available=true;
        out->reason=PHYS_OK_EXPERIMENTAL;
    } else {
        out->reason=PHYS_RATIO_RANGE; // Keep HR; never clamp SpO2 to a plausible value.
    }
}

bool physiology_feed(physiology_context_t *ctx, uint64_t seq, int64_t time_us,
                     uint32_t red, uint32_t ir, uint32_t flags, uint32_t epoch,
                     physiology_result_t *result)
{
    bool gap=ctx->has_last && (seq!=ctx->last_seq+1 || epoch!=ctx->last_epoch || time_us<=ctx->last_time);
    ctx->last_seq=seq; ctx->last_epoch=epoch; ctx->last_time=time_us; ctx->has_last=true;
    physiology_reason_t reason=PHYS_WARMUP;
    bool reject=false;
    if(gap || (flags & 11)) { reason=PHYS_GAP; reject=true; }
    else if((flags & 4) || red>=262143 || ir>=262143) { reason=PHYS_SATURATED; reject=true; }
    else if(red<5000 || ir<5000) { reason=PHYS_NO_CONTACT; reject=true; }
    if(reject) {
        ctx->count=ctx->write_index=0;
        bool publish=time_us-ctx->last_report>=1000000 || reason!=ctx->last_reason;
        if(publish) { invalidate(result,time_us,reason); ctx->last_report=time_us; ctx->last_reason=reason; }
        return publish;
    }
    ctx->red[ctx->write_index]=red; ctx->ir[ctx->write_index]=ir;
    ctx->times[ctx->write_index]=time_us;
    ctx->write_index=(ctx->write_index+1)%PHYSIOLOGY_WINDOW;
    if(ctx->count<PHYSIOLOGY_WINDOW) ++ctx->count;
    if(time_us-ctx->last_report<1000000) return false;
    invalidate(result,time_us,PHYS_WARMUP);
    if(ctx->count==PHYSIOLOGY_WINDOW) analyse(ctx,result);
    ctx->last_report=time_us; ctx->last_reason=result->reason;
    return true;
}

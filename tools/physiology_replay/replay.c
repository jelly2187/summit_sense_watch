#include "physiology.h"
#include <stdio.h>
#include <stdlib.h>

static physiology_context_t ctx;
int main(int argc, char **argv)
{
    if(argc!=2) return 2;
    FILE *input=fopen(argv[1],"r"); if(!input) return 3;
    physiology_init(&ctx);
    puts("session,time_us,hr_bpm,hr_available,spo2_est_pct,spo2_est_available,calibrated,sqi,ratio,pi_ir_pct,reason");
    char line[512]; unsigned previous_session=0;
    while(fgets(line,sizeof(line),input)) {
        unsigned session,red,ir,flags,lost,queue,errors,epoch;
        unsigned long long pc,seq; long long time_us;
        if(sscanf(line,"%u,%llu,%llu,%lld,%u,%u,%u,%u,%u,%u,%u",&session,&pc,&seq,&time_us,&red,&ir,&flags,&lost,&queue,&errors,&epoch)!=11) continue;
        if(session!=previous_session) { physiology_init(&ctx); previous_session=session; }
        physiology_result_t result;
        if(physiology_feed(&ctx,seq,time_us,red,ir,flags,epoch,&result)) {
            printf("%u,%lld,%.2f,%u,%.2f,%u,0,%.1f,%.4f,%.3f,%s\n",session,
                (long long)result.time_us,result.hr_bpm,result.hr_available,
                result.spo2_est_pct,result.spo2_est_available,result.sqi,result.ratio,
                result.pi_ir_pct,physiology_reason_name(result.reason));
        }
    }
    fclose(input); return 0;
}

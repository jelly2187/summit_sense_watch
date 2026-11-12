#include "nmea_rmc.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static int hex(char c)
{
    if (c>='0' && c<='9') return c-'0';
    if (c>='A' && c<='F') return c-'A'+10;
    if (c>='a' && c<='f') return c-'a'+10;
    return -1;
}

static bool coordinate(const char *s,size_t len,unsigned deg_digits,int32_t *out)
{
    if (len<deg_digits+2 || len>deg_digits+10) return false;
    unsigned deg=0,min=0;
    for (unsigned i=0;i<deg_digits+2;++i) {
        if(s[i]<'0'||s[i]>'9')return false;
        if(i<deg_digits)deg=deg*10+s[i]-'0';
        else min=min*10+s[i]-'0';
    }
    unsigned frac=0,divisor=1;
    if(len>deg_digits+2) {
        if(s[deg_digits+2]!='.' || len==deg_digits+3)return false;
        for(size_t i=deg_digits+3;i<len;++i) {
            if(s[i]<'0'||s[i]>'9')return false;
            frac=frac*10+s[i]-'0';divisor*=10;
        }
    }
    unsigned limit=deg_digits==2?90:180;
    if(min>=60 || deg>limit || (deg==limit&&(min||frac)))return false;
    int64_t numerator=((int64_t)min*divisor+frac)*10000000;
    *out=(int32_t)(deg*10000000U+(numerator+30*divisor)/(60*divisor));
    return true;
}

nmea_rmc_result_t nmea_rmc_parse(const char *s,int32_t *lat,int32_t *lon)
{
    if(strlen(s)<12 || s[0]!='$' || memcmp(s+3,"RMC,",4)!=0)return NMEA_IGNORED;
    const char *star=strchr(s,'*');
    if(!star || strlen(star)!=3)return NMEA_IGNORED;
    int hi=hex(star[1]),lo=hex(star[2]);
    if(hi<0||lo<0)return NMEA_IGNORED;
    unsigned sum=0;
    for(const char *p=s+1;p<star;++p)sum^=(unsigned char)*p;
    if(sum!=(unsigned)(hi*16+lo))return NMEA_IGNORED;
    const char *f[6];size_t len[6];
    const char *p=s+7;
    for(unsigned i=0;i<6;++i) {
        f[i]=p;
        const char *end=p;
        while(end<star && *end!=',')++end;
        len[i]=end-p;
        p=end<star?end+1:end;
    }
    if(len[1]!=1)return NMEA_IGNORED;
    if(*f[1]=='V')return NMEA_NO_FIX;
    if(*f[1]!='A'||len[3]!=1||len[5]!=1)return NMEA_IGNORED;
    int32_t a,b;
    if(!coordinate(f[2],len[2],2,&a)||!coordinate(f[4],len[4],3,&b))return NMEA_IGNORED;
    if(*f[3]!='N'&&*f[3]!='S')return NMEA_IGNORED;
    if(*f[5]!='E'&&*f[5]!='W')return NMEA_IGNORED;
    *lat=*f[3]=='S'?-a:a;*lon=*f[5]=='W'?-b:b;
    return NMEA_FIX;
}

#include "djfilter.h"
/* f = 2 sin(pi fc / 48 kHz) in Q12, index = |position|; exponential sweeps. */
static const uint16_t lp_f[64]={0,3623,3363,3119,2891,2678,2480,2295,2124,1964,1817,1679,1552,1435,1326,1225,1132,1046,966,892,824,761,703,649,600,554,511,472,436,403,372,343,317,293,270,250,231,213,197,182,168,155,143,132,122,113,104,96,89,82,76,70,64,59,55,51,47,43,40,37,34,31,29,27};
static const uint16_t hp_f[64]={0,11,12,13,14,16,17,19,21,23,25,28,30,33,37,40,44,49,53,59,65,71,78,86,94,104,114,125,137,151,166,183,201,220,242,266,293,322,354,389,427,469,516,567,623,684,752,826,908,997,1096,1203,1322,1451,1593,1749,1919,2105,2308,2530,2771,3033,3317,3623};
#define LIM (1<<26)
static int32_t clamp(int32_t v){return v>LIM?LIM:v<-LIM?-LIM:v;}
static int32_t sat31(int32_t v26){ /* Q23 state scale -> Q31 output with saturation */
    if(v26>0x7fffff)return 0x7fffffff;
    if(v26<-0x800000)return (int32_t)0x80000000;
    return v26*256;
}
void mc_djfilter_block(int32_t *x,int8_t position,uint8_t resonance,mc_djfilter_state *s) {
    if(!position){s->last=0;return;}
    int lowpass=position<0;unsigned pos=(unsigned)(lowpass?-position:position);
    if(pos>63)pos=63;
    int32_t target=lowpass?lp_f[pos]:hp_f[pos];
    unsigned res=resonance>127?127:resonance;
    if(!res)res=48;
    /* Damping q = 2 (1 - 0.92 res/127) in Q12: 8192 (none) .. 655 (peak ~6x). */
    int32_t q12=8192-(int32_t)(res*7537u/127u);
    if(!s->last||(s->last<0)!=lowpass) {
        /* (Re)engaging or switching mode: start from the current input so the
         * low-pass does not dip to zero; the high-pass then starts at ~dry. */
        int32_t x0=x[0]>>8;
        s->lp=lowpass?x0:0;s->bp=0;s->f=target;
    }
    s->last=position;
    int32_t f=s->f,step=(target-f)/4;f+=step?step:(target>f?1:target<f?-1:0);
    int32_t lp=s->lp,bp=s->bp;
    /* States in Q23 (input >> 8 keeps 24 bits). Products stay within 32 bits:
     * |bp|,|lp| <= 2^26 (clamped), |hp| < 2^28, coefficients <= 2^13. */
    if(lowpass)for(unsigned i=0;i<32;i++) {
        int32_t in=x[i]>>8;
        int32_t hp=in-lp-((q12*(bp>>8))>>4);
        bp=clamp(bp+((f*(hp>>9))>>3));
        lp=clamp(lp+((f*(bp>>8))>>4));
        x[i]=sat31(lp);
    }
    else for(unsigned i=0;i<32;i++) {
        int32_t in=x[i]>>8;
        int32_t hp=in-lp-((q12*(bp>>8))>>4);
        bp=clamp(bp+((f*(hp>>9))>>3));
        lp=clamp(lp+((f*(bp>>8))>>4));
        x[i]=sat31(hp);
    }
    s->lp=lp;s->bp=bp;s->f=f;
}

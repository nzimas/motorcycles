#include "duration.h"
#include <stddef.h>
int mc_duration_codes(unsigned speed,const uint32_t table[128],uint8_t out[13]) {
    static const uint8_t ticks[7]={3,4,6,8,12,24,48};
    uint8_t result[13]={0};
    if(speed>=7||!table||!out||table[0]!=168750u||table[127]!=345600000u)return 0;
    for(unsigned i=1;i<128;i++)if(table[i]<=table[i-1])return 0;
    for(unsigned duration=1;duration<=12;duration++) {
        uint32_t wanted=(uint32_t)ticks[speed]*450000u*duration;
        uint32_t best=UINT32_MAX;
        unsigned code=0;
        for(unsigned i=1;i<128;i++) {
            uint32_t error=table[i]>wanted?table[i]-wanted:wanted-table[i];
            if(error<best){best=error;code=i-1;}
        }
        /* The fixed stock table quantizes triplet durations. Reject any ROM
         * change that exceeds the expected maximum 1/16 relative error. */
        if(best>wanted/16u)return 0;
        result[duration]=(uint8_t)code;
    }
    for(unsigned i=0;i<13;i++)out[i]=result[i];
    return 1;
}

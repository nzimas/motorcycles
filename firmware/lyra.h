#ifndef MC_LYRA_H
#define MC_LYRA_H
#include <stdint.h>
/* LYRA voice (firmware/lyra.c). note: MIDI note << 16 (440 Hz at 69). */
typedef struct { uint32_t ph[4]; int32_t y[4]; uint32_t lfo; int32_t dc_x,dc_y; } lyra_state;
typedef struct { int32_t note; uint8_t spread,sharp,mod,hyper; } lyra_params;
void lyra_block(lyra_state *,const lyra_params *,int32_t out[32]);
#endif

#ifndef MC_DJFILTER_H
#define MC_DJFILTER_H
#include <stdint.h>
/* Per-track DJ filter: one bipolar position and a resonance, on a 32-sample
 * Q31 block in place. position (int8): 0 = off (bit-exact bypass, ~free);
 * -1..-63 = resonant low-pass sweeping 7 kHz -> 50 Hz; +1..+63 = resonant
 * high-pass sweeping 20 Hz -> 7 kHz. resonance 0..127 (0 = default moderate).
 * Integer only, Chamberlin state-variable filter at 16-bit signal precision
 * with saturating states; cutoff glides block to block. */
typedef struct { int32_t lp,bp,f; int8_t last; uint8_t pad[3]; } mc_djfilter_state;
void mc_djfilter_block(int32_t *x,int8_t position,uint8_t resonance,mc_djfilter_state *s);
#endif

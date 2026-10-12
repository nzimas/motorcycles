#ifndef MC_DRIVE_H
#define MC_DRIVE_H
#include <stdint.h>

/* Per-track distortion ("DRIVE") for the six 32-sample Q31 track blocks that
 * follow the stock Volume+Dist stage.  Integer only, freestanding (no libc),
 * one 34-byte table.  See firmware/DRIVE.md for algorithm and measured costs.
 * Processing is done at 16-bit sample resolution (the input's low 16 bits are
 * passed through untouched); amount 0 leaves the block bit-exact.
 *
 * Parameter record: 8 bytes per sound.  Every byte is clamped to 0..127, so
 * any byte pattern is safe.  All-zero = OFF; with amount > 0 the other bytes
 * at 0 are the neutral default (plain soft->hard clipper, no fold/bias/tone).
 *   [0] amount : 0 = bypass (bit-exact, ~free); 1..127 = pre-gain 0.26x..32x,
 *                knee soft -> hard, more density and loudness (never quieter)
 *   [1] drive  : extra pre-gain on top of amount, 0 = x1 .. 127 = x4 (+12 dB)
 *   [2] fold   : 0 = clipper only; 1..127 = triangle wavefolder before the
 *                clipper, fold threshold 128 .. 1 knee(s), plus fold make-up
 *   [3] bias   : 0 = symmetric; 1..127 = offset before the shaper (even
 *                harmonics), up to half the clip level; output DC servoed out
 *   [4] tone   : 0 = open; 1..127 = 1-pole LP down to ~5.3 kHz with up to
 *                +6 dB make-up into the output clip (darker and denser)
 *   [5] low    : 0 = full-band shaping; 1..127 = crossover depth (lows below
 *                ~119 Hz bypass the shaper and return clean) + up to +0.5x
 *                sub boost.  fold also engages the crossover (max(fold, low))
 *   [6],[7]    : reserved, ignored (write 0)
 */
enum {
 MC_DRIVE_AMOUNT = 0, MC_DRIVE_DRIVE = 1, MC_DRIVE_FOLD = 2, MC_DRIVE_BIAS = 3,
 MC_DRIVE_TONE = 4, MC_DRIVE_LOW = 5, MC_DRIVE_RES0 = 6, MC_DRIVE_RES1 = 7,
 MC_DRIVE_PARAMS = 8, MC_DRIVE_BLOCK = 32
};

/* Inner-loop coefficient block.  Field offsets are used by the ColdFire
 * assembly in drive.c: do not reorder. */
typedef struct {
 int32_t gin;   /*  0 pre-gain (u = s * gin) */
 int32_t badd;  /*  4 bias in u units (fold variant: F + bias) */
 int32_t dcy;   /*  8 DC subtracted from shaper output */
 int32_t kt;    /* 12 tone coefficient Q10 (1024 = open) */
 int32_t go;    /* 16 output gain (negative: shaper output is inverted) */
 int32_t lo;    /* 20 clean sub gain Q13 */
 int32_t lp;    /* 24 sub low-pass state, Q15 << 6 */
 int32_t yl;    /* 28 tone low-pass state (output units, Q15) */
 int32_t sh;    /* 32 u -> u' shift */
 int32_t cu;    /* 36 +clip level (u') */
 int32_t ncu;   /* 40 -clip level (u') */
 int32_t c3;    /* 44 3*K'^2 >> 13 */
 int32_t sum;   /* 48 out: sum of output taps (every MC_DRIVE_UNROLL-th sample) */
 int32_t gcl;   /* 52 crossover: u -= lp * gcl */
} mc_drive_coef;

/* smoothed control set */
typedef struct { int32_t gin, p, h, b, kt, mk, lo, mf, xc; } mc_drive_ctl;

typedef struct {
 mc_drive_coef c;
 mc_drive_ctl cur, tgt;
 int32_t dcn;    /* output-DC servo offset, shaper units << 8 */
 int32_t ymax;   /* |shaper output| bound for the current coefficients */
 int32_t y0;     /* static DC (shaper output at zero input) */
 int32_t ydmin, ydmax; /* reachable shaper output range (DC servo limits) */
 uint8_t variant;
 uint8_t active; /* 0 after reset/bypass: next active block re-initialises */
 uint8_t settled;/* cur == tgt: skip smoothing */
 uint8_t pad;
 uint8_t last[MC_DRIVE_PARAMS];
} mc_drive_state;

void mc_drive_reset(mc_drive_state *state);
/* Process one 32-sample Q31 block in place (amount 0: returns untouched). */
void mc_drive_block(int32_t *samples, const uint8_t params[MC_DRIVE_PARAMS], mc_drive_state *state);

#endif

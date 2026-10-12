/* Per-track distortion kernel for the Model:Cycles native firmware.
 * Target: ColdFire MCF54415 (ISA_C, no FPU).  Integer only, freestanding.
 * Design note, parameter ranges and measured costs: firmware/DRIVE.md.
 *
 * Per sample (s = input >> 16, Q15; the result replaces the high 16 bits of
 * the Q31 word, the low 16 bits are left as they were):
 *   [low]  lp = acc >> 6, acc += s - lp        1-pole ~119 Hz (21-bit state)
 *          u  = s * gin [- lp * gcl]           pre-gain [crossover: lows bypass]
 *   [fold] u  = |((u + F + b) mod 4F) - 2F| - F   triangle fold at +-F (= P knees)
 *   [bias] u += b                              (fold variant: b folded in above)
 *          U  = clamp(u >> sh, +-H * 8K')      knee at 8K', K' in (2^12, 2^13]
 *          y  = U * (((U>>3)^2 >> 13) - 3K'^2/2^13) >> 10   = -(1.5u - 0.5u^3)
 *   [bias] y -= dc                             static DC + output-DC servo
 *          o  = (y * go [+ lp * lo]) >> 13     make-up gain [+ clean lows]
 *   [tone] z += (o - z) * kt >> 10 ; o = z     1-pole tone LP, state clipped
 *   [any]  o  = clamp(o, Q15)                  (base variant bounded by design)
 * Loop variants (V_* bits): base, bias, tone, tone+bias, low(+tone),
 * low+bias, fold(+low+tone), fold+bias.  On ColdFire they are hand-written
 * assembly (4x unrolled, ~17 instructions/sample for the base variant);
 * elsewhere (host tests) the bit-identical C reference below is used. */
#include "drive.h"

#define F_FOLD 0x1000000
#define V_BIAS 1   /* bias add + DC removal */
#define V_TONE 2   /* tone low-pass + output clip */
#define V_LOW 4    /* crossover + clean lows (implies V_TONE) */
#define V_FOLD 8   /* wavefolder (implies V_LOW) */

#ifndef MC_DRIVE_UNROLL
#define MC_DRIVE_UNROLL 4  /* 4 or 2: 2 saves ~45% loop code for +0.75 instr/sample */
#endif

/* ------------------------------------------------------------------ loops */
#if defined(__mcoldfire__)
/* registers: a0 samples, a1 end, a2 coef block, a3 DC sum, a4/a5 +-clip,
 * a6 c3, d1 lp, d4 tone state, d5 = 13, d6 = sh, d7 = 10, d0/d2/d3 scratch */
#define L_LOAD(o) "mvs.w " o "(%a0),%d0\n\t"
/* d1 = acc (lp << 6), d3 = lp = acc >> 6 (kept until L_LOW) */
#define L_LP "move.l %d1,%d3\n\tasr.l #6,%d3\n\tadd.l %d0,%d1\n\tsub.l %d3,%d1\n\t"
#define L_GAIN "muls.l (%a2),%d0\n\t"
#define L_XGAIN "muls.l (%a2),%d0\n\tmove.l %d3,%d2\n\tmuls.l 52(%a2),%d2\n\tsub.l %d2,%d0\n\t"
#define L_FOLD "add.l 4(%a2),%d0\n\tand.l #0x3FFFFFF,%d0\n\tsub.l #0x2000000,%d0\n\t" \
 "bpl.s 1f\n\tneg.l %d0\n1:\tsub.l #0x1000000,%d0\n\t"
#define L_BIAS "add.l 4(%a2),%d0\n\t"
#define L_SHIFT "asr.l %d6,%d0\n\t"
#define L_CLAMPU "cmp.l %a4,%d0\n\tble.s 1f\n\tmove.l %a4,%d0\n1:\tcmp.l %a5,%d0\n\tbge.s 1f\n\tmove.l %a5,%d0\n1:\t"
#define L_CUBIC "move.l %d0,%d2\n\tasr.l #3,%d2\n\tmuls.l %d2,%d2\n\tasr.l %d5,%d2\n\tsub.l %a6,%d2\n\tmuls.l %d2,%d0\n\tasr.l %d7,%d0\n\t"
#define L_SUM "adda.l %d0,%a3\n\t"
#define L_SUMZ "adda.l %d4,%a3\n\t"
#define L_DC "sub.l 8(%a2),%d0\n\t"
#define L_OUT "muls.l 16(%a2),%d0\n\t"
#define L_LOW "muls.l 20(%a2),%d3\n\tadd.l %d3,%d0\n\t"
#define L_OSHIFT "asr.l %d5,%d0\n\t"
#define L_CLAMPO "cmp.l #32767,%d0\n\tble.s 1f\n\tmove.l #32767,%d0\n1:\tcmp.l #-32768,%d0\n\tbge.s 1f\n\tmove.l #-32768,%d0\n1:\t"
/* tone: one-pole low-pass in the output domain, state clipped in place */
#define L_TONE "sub.l %d4,%d0\n\tmuls.l 12(%a2),%d0\n\tasr.l %d7,%d0\n\tadd.l %d0,%d4\n\t" \
 "cmp.l #32767,%d4\n\tble.s 1f\n\tmove.l #32767,%d4\n1:\tcmp.l #-32768,%d4\n\tbge.s 1f\n\tmove.l #-32768,%d4\n1:\t"
#define L_STORE(o) "move.w %d0," o "(%a0)\n\t"
#define L_STOREZ(o) "move.w %d4," o "(%a0)\n\t"

/* one sample; disabled features are passed as "" */
#define SAMPLE(o, LP, FOLD, BADD, SUM, DC, LOW, POST, ST) L_LOAD(o) LP FOLD BADD L_SHIFT \
 L_CLAMPU L_CUBIC DC L_OUT LOW L_OSHIFT POST SUM ST(o)
#if MC_DRIVE_UNROLL == 4
#define BODY(LP, FOLD, BADD, SUM, DC, LOW, POST, ST) SAMPLE("0", LP, FOLD, BADD, SUM, DC, LOW, POST, ST) \
 SAMPLE("4", LP, FOLD, BADD, "", DC, LOW, POST, ST) SAMPLE("8", LP, FOLD, BADD, "", DC, LOW, POST, ST) \
 SAMPLE("12", LP, FOLD, BADD, "", DC, LOW, POST, ST) "lea 16(%a0),%a0\n\t"
#define SUM_SHIFT 3
#else
#define BODY(LP, FOLD, BADD, SUM, DC, LOW, POST, ST) SAMPLE("0", LP, FOLD, BADD, SUM, DC, LOW, POST, ST) \
 SAMPLE("4", LP, FOLD, BADD, "", DC, LOW, POST, ST) "addq.l #8,%a0\n\t"
#define SUM_SHIFT 4
#endif

#define LOOP(name, LP, FOLD, BADD, SUM, DC, LOW, POST, ST) \
 "\n\t.text\n\t.even\n\t.globl " name "\n\t.type " name ",@function\n" name ":\n\t" \
 "lea -44(%sp),%sp\n\tmovem.l %d2-%d7/%a2-%a6,(%sp)\n\t" \
 "move.l 48(%sp),%a0\n\tmove.l 52(%sp),%a2\n\tlea 128(%a0),%a1\n\t" \
 "move.l 24(%a2),%d1\n\tmove.l 28(%a2),%d4\n\tmove.l 32(%a2),%d6\n\tmoveq #13,%d5\n\tmoveq #10,%d7\n\t" \
 "move.l 36(%a2),%a4\n\tmove.l 40(%a2),%a5\n\tmove.l 44(%a2),%a6\n\tsuba.l %a3,%a3\n" \
 "2:\t" BODY(LP, FOLD, BADD, SUM, DC, LOW, POST, ST) "cmp.l %a1,%a0\n\tbne.w 2b\n\t" \
 "move.l %d1,24(%a2)\n\tmove.l %d4,28(%a2)\n\tmove.l %a3,48(%a2)\n\t" \
 "movem.l (%sp),%d2-%d7/%a2-%a6\n\tlea 44(%sp),%sp\n\trts\n\t.size " name ",.-" name "\n"

__asm__(
 LOOP("mc_drive_loop0", L_GAIN, "", "", "", "", "", "", L_STORE)
 LOOP("mc_drive_loop1", L_GAIN, "", L_BIAS, L_SUM, L_DC, "", L_CLAMPO, L_STORE)
 LOOP("mc_drive_loop2", L_GAIN, "", "", "", "", "", L_TONE, L_STOREZ)
 LOOP("mc_drive_loop3", L_GAIN, "", L_BIAS, L_SUMZ, L_DC, "", L_TONE, L_STOREZ)
 LOOP("mc_drive_loop6", L_LP L_XGAIN, "", "", "", "", L_LOW, L_TONE, L_STOREZ)
 LOOP("mc_drive_loop7", L_LP L_XGAIN, "", L_BIAS, L_SUMZ, L_DC, L_LOW, L_TONE, L_STOREZ)
 LOOP("mc_drive_loop14", L_LP L_XGAIN, L_FOLD, "", "", "", L_LOW, L_TONE, L_STOREZ)
 LOOP("mc_drive_loop15", L_LP L_XGAIN, L_FOLD, "", L_SUMZ, L_DC, L_LOW, L_TONE, L_STOREZ)
);
void mc_drive_loop0(int32_t *, mc_drive_coef *);
void mc_drive_loop1(int32_t *, mc_drive_coef *);
void mc_drive_loop2(int32_t *, mc_drive_coef *);
void mc_drive_loop3(int32_t *, mc_drive_coef *);
void mc_drive_loop6(int32_t *, mc_drive_coef *);
void mc_drive_loop7(int32_t *, mc_drive_coef *);
void mc_drive_loop14(int32_t *, mc_drive_coef *);
void mc_drive_loop15(int32_t *, mc_drive_coef *);

typedef void (*loop_fn)(int32_t *, mc_drive_coef *);
static loop_fn const loops[16] = {
 mc_drive_loop0, mc_drive_loop1, mc_drive_loop2, mc_drive_loop3, 0, 0, mc_drive_loop6, mc_drive_loop7,
 0, 0, 0, 0, 0, 0, mc_drive_loop14, mc_drive_loop15
};
#define run(x, c, v) loops[v](x, c)

#else /* portable reference, bit-identical to the assembly */
#if MC_DRIVE_UNROLL == 4
#define SUM_SHIFT 3
#else
#define SUM_SHIFT 4
#endif

static void run(int32_t *x, mc_drive_coef *c, int v)
{
 int32_t acc = c->lp, yl = c->yl, sum = 0;
 int i;
 for (i = 0; i < MC_DRIVE_BLOCK; i++) {
  int32_t s = x[i] >> 16, u, y, o, lp = acc >> 6;
  u = s * c->gin;
  if (v & V_LOW) { acc += s - lp; u -= lp * c->gcl; }  /* crossover */
  if (v & V_FOLD) {
   int32_t r = (int32_t)(((uint32_t)u + (uint32_t)c->badd) & 0x3FFFFFFu) - 0x2000000;
   u = (r < 0 ? -r : r) - F_FOLD;
  } else if (v & V_BIAS)
   u += c->badd;
  u >>= c->sh;
  u = u > c->cu ? c->cu : u < c->ncu ? c->ncu : u;
  y = (u * ((((u >> 3) * (u >> 3)) >> 13) - c->c3)) >> 10;
  if (v & V_BIAS) y -= c->dcy;
  o = y * c->go;
  if (v & V_LOW) o += lp * c->lo;
  o >>= 13;
  if (v & V_TONE) {     /* output-domain low-pass, state clipped in place */
   yl += ((o - yl) * c->kt) >> 10;
   o = yl = yl > 32767 ? 32767 : yl < -32768 ? -32768 : yl;
  } else if (v)
   o = o > 32767 ? 32767 : o < -32768 ? -32768 : o;
  if ((v & V_BIAS) && i % MC_DRIVE_UNROLL == 0) sum += o;
  x[i] = (int32_t)(((uint32_t)o << 16) | ((uint32_t)x[i] & 0xFFFFu));
 }
 c->lp = acc; c->yl = yl; c->sum = sum;
}
#endif

/* ------------------------------------------------------------ parameters */

/* shaper on the fine input U (8x the knee scale): the square uses U >> 3,
 * the final product keeps U's extra 3 bits (resolution at low drive) */
static int32_t ycalc(int32_t u, int32_t c3) { return (u * ((((u >> 3) * (u >> 3)) >> 13) - c3)) >> 10; }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* 2^(k/16) * 32768 - 32768, k = 0..16 */
static const uint16_t pow2_tab[17] = {
 0, 1451, 2966, 4548, 6200, 7925, 9727, 11608, 13573, 15625, 17767,
 20005, 22341, 24781, 27329, 29989, 32768
};

/* 2^(e / 65536) * 2^15 for e in [0, 9.99 * 65536] (result < 2^25) */
static int32_t pow2_q15(int32_t e)
{
 int32_t i = e >> 16, k = (e >> 12) & 15, r = e & 0xFFF;
 int32_t a = pow2_tab[k], b = pow2_tab[k + 1];
 return (32768 + a + (((b - a) * r) >> 12)) << i;
}

static void targets(const uint8_t *p, mc_drive_ctl *t)
{
 int32_t a = p[0] > 127 ? 127 : p[0], dr = p[1] > 127 ? 127 : p[1];
 int32_t fo = p[2] > 127 ? 127 : p[2], bi = p[3] > 127 ? 127 : p[3];
 int32_t to = p[4] > 127 ? 127 : p[4], lo = p[5] > 127 ? 127 : p[5], td, b;
 if (a < 1) a = 1;
 /* pre-gain, Q9: 0.26x .. 32x by amount, x1 .. x4 by drive, capped at 64x */
 t->gin = pow2_q15(((7 * a + 2 * dr) << 16) / 127) >> 8;
 if (t->gin > 32768) t->gin = 32768;
 /* clip level H (Q13 of knee): 1.0 (soft) .. 0.4 (hard) with amount^2 */
 t->h = 8192 - (4915 * a * a) / (127 * 127);
 /* fold threshold P (Q8 knees) = 2^(7 (1 - fold)): 128 .. 1; 32768 = off */
 t->p = fo ? pow2_q15((7 * (127 - fo) << 16) / 127) >> 7 : 32768;
 if (t->p > 32768) t->p = 32768;
 /* bias (Q13 of knee), at most half the clip level */
 b = bi * 4096 / 127;
 t->b = b > t->h / 2 ? t->h / 2 : b;
 /* tone: 1-pole LP down to ~5.3 kHz (kt 0.5) + up to +6 dB make-up into the
  * output clip; depth fades in over amount 1..8 */
 td = (to * (16 * a > 127 ? 127 : 16 * a) * 32768) / (127 * 127);
 t->kt = 1024 - ((td * 512) >> 15);
 t->mk = 4096 + ((td * 4096) >> 15);
 /* fold make-up: folded energy moves up the spectrum; push it into the clip */
 t->mf = 4096 + (fo * (2048 + 32 * a)) / 127;
 /* crossover: lows below ~119 Hz bypass the shaper (depth = max(fold, low))
  * and are added back clean (x1.06) plus up to +0.5x boost from low */
 t->xc = ((fo > lo ? fo : lo) * 32768) / 127;
 t->lo = (t->xc >> 2) + (t->xc >> 6) + (4096 * lo * (2 * a > 127 ? 127 : 2 * a)) / (127 * 127);
}

static void coefs(mc_drive_state *st)
{
 const mc_drive_ctl *k = &st->cur;
 mc_drive_coef *c = &st->c;
 uint32_t K = 1u << 24;
 int32_t reach = 0, gu = k->gin, bu, sh = 0, kp, hi, lo, yh, ylo, ymax, span, m, v = 0, y0 = 0, old = st->variant;
 if (k->p < 32768) {
  uint32_t kf = (0x80000000u / (uint32_t)k->p) << 1;   /* 2^32 / P: knee for F = 2^24 */
  int32_t g = (int32_t)(((uint32_t)k->gin * (kf >> 9)) >> 15);
  if (g > 512) { K = kf; gu = g; v = V_FOLD | V_LOW | V_TONE; } /* folding reachable */
  /* fold make-up fades in from "first fold reachable" to "one full fold" */
  reach = g <= 512 ? 0 : g >= 1024 ? 4096 : (g - 512) * 8;
 }
 if (k->lo > 0 || k->xc > 0) v |= V_LOW | V_TONE;
 if ((v & V_LOW) && gu > 32768 - (k->xc >> 1)) gu = 32768 - (k->xc >> 1); /* |s*gin - lp*gcl| < 2^31 */
 if (k->kt < 1024) v |= V_TONE;
 if (k->b != 0 || st->dcn != 0) v |= V_BIAS;
 sh = 11;                                  /* K in [2^17, 2^24]: smallest sh */
 while (sh > 0 && (K >> (sh - 1)) <= 8192) sh--;  /* with K >> sh <= 8192 */
 kp = (int32_t)(K >> sh);                  /* K' in (4096, 8192] */
 sh -= 3;                                  /* fine scale: U = u >> sh, knee 8 K' */
 bu = (int32_t)(((uint32_t)k->b * (K >> 9)) >> 4);
 c->gin = gu;
 c->gcl = (int32_t)(((uint32_t)gu * (uint32_t)k->xc) >> 15);
 if (c->gcl > 32768 - gu) c->gcl = 32768 - gu;
 c->badd = (v & V_FOLD) ? F_FOLD + bu : bu;
 c->sh = sh;
 c->cu = (k->h * kp) >> 10;
 c->ncu = -c->cu;
 c->c3 = (3 * kp * kp) >> 13;
 c->kt = k->kt;
 c->lo = k->lo;
 if ((v & V_LOW) && !(old & V_LOW)) c->lp = 0;
 if ((v & V_TONE) && !(old & V_TONE)) c->yl = 0;
 if (v & V_FOLD) { hi = c->cu; lo = c->ncu; }
 else {
  int32_t b2 = (v & V_BIAS) ? bu : 0;
  hi = clampi((32767 * gu + b2) >> sh, c->ncu, c->cu);
  lo = clampi((-32768 * gu + b2) >> sh, c->ncu, c->cu);
 }
 /* static DC: shaper output for zero input (fold: u = -bias) */
 if (v & V_BIAS) y0 = ycalc(clampi(((v & V_FOLD) ? -bu : bu) >> sh, c->ncu, c->cu), c->c3);
 yh = ycalc(hi, c->c3); ylo = ycalc(lo, c->c3);
 ymax = (iabs(yh) > iabs(ylo) ? iabs(yh) : iabs(ylo)) + 64;
 span = iabs(yh - ylo) + 128;
 st->ydmin = (yh < ylo ? yh : ylo) - 64;
 st->ydmax = (yh < ylo ? ylo : yh) + 64;
 if ((v & (V_BIAS | V_FOLD)) == V_BIAS) {
  /* smallest of: positive swing, negative swing, small-signal slope */
  int32_t b1 = (bu >> sh) >> 3, sw = (32767 * gu) >> sh, ss;
  if (sw > 16 * kp) sw = 16 * kp;
  ss = ((c->c3 - ((3 * b1 * b1) >> 13)) * (sw >> 3)) >> 7;
  m = (iabs(yh - y0) + iabs(y0 - ylo)) >> 1;  /* mean swing: the DC servo re-centres */
  if (ss < m) m = ss;
 } else
  m = iabs(yh) < iabs(ylo) ? iabs(yh) : iabs(ylo);
 if (m < 256) m = 256;
 if (v == 0)
  c->go = -((32767 * 8192 + 8191) / ymax);   /* exact bound: no output clip needed */
 else {
  /* the DC servo keeps dcy inside [ydmin, ydmax], so |y - dcy| <= span;
   * keep |(y - dcy) * go| <= 1.625 * 2^30 so that adding lp * lo
   * (|lp| <= 2^15, lo <= 12288: <= 0.375 * 2^30) cannot overflow */
  int32_t norm = (32767 * 8192) / m, cap = (13 << 27) / ((v & V_BIAS) ? span : ymax);
  int32_t mf = 4096 + (((k->mf - 4096) * reach) >> 12);
  int32_t g = (((norm * k->mk) >> 12) * mf) >> 12;
  if (v & V_BIAS) g += g >> 4;   /* +0.5 dB: the DC servo's LF loss around 30 Hz */
  c->go = g > cap ? cap : g;
  if (!(v & V_FOLD)) c->go = -c->go;         /* the fold maps u -> -u for small u */
 }
 if (st->dcn && st->ymax > 0 && st->ymax != ymax) {
  int32_t r6 = (ymax * 64) / st->ymax;
  st->dcn = clampi((st->dcn / 64) * clampi(r6, 0, 128), -(ymax << 8), ymax << 8);
 }
 st->ymax = ymax; st->y0 = y0; st->variant = (uint8_t)v;
}

static void set_dcy(mc_drive_state *st)
{
 /* servo range: anywhere inside the reachable shaper output range */
 st->c.dcy = clampi(st->y0 + (st->dcn >> 8), st->ydmin, st->ydmax);
}

void mc_drive_reset(mc_drive_state *st)
{
 /* word-wise clear (no memset in a freestanding build) */
 uint32_t *q = (uint32_t *)(void *)st;
 unsigned i;
 for (i = 0; i < sizeof *st / 4; i++) q[i] = 0;
}
typedef char mc_drive_state_size_check[(sizeof(mc_drive_state) & 3) == 0 ? 1 : -1];

/* output-DC servo (bias variants): integral control of the measured output
 * mean (after the output clip, where bias + asymmetric clipping create DC):
 * moves the pre-gain DC offset (Q8) by 1/32 of the block mean -> loop gain
 * <= 1/32 per block, tau >= 21 ms, i.e. a DC blocker with its corner ~7.5 Hz */
static void servo(mc_drive_state *st)
{
 if (st->cur.b != 0) {
  int32_t e = st->c.sum >> SUM_SHIFT, lim = (st->ymax << 7) + (iabs(st->y0) << 8);
  st->dcn = clampi(st->dcn + (e * 65536) / st->c.go, -lim, lim);
 }
}

static __attribute__((noinline)) void block_slow(int32_t *x, const uint8_t *prm, mc_drive_state *st, int changed)
{
 int i, dirty = 0;
 if (!st->active) {
  static const uint8_t neutral[MC_DRIVE_PARAMS] = { 1 };
  mc_drive_reset(st);
  targets(neutral, &st->cur);
  st->active = 1;
  dirty = changed = 1;
 }
 if (changed) {
  targets(prm, &st->tgt);
  for (i = 0; i < 6; i++) st->last[i] = prm[i];
  st->settled = 0;
 }
 if (!st->settled) {
  /* glide: 1/4 of the distance per block, rate-limited (pre-gain ~0.5 dB,
   * fold threshold ~0.27 dB per block: a full 1..127 jump takes ~50-100 ms) */
  mc_drive_ctl *cu = &st->cur;
  const mc_drive_ctl *tg = &st->tgt;
  int32_t moving = 0;
#define GLIDE(f, m) do { int32_t d_ = tg->f - cu->f, s_ = clampi(d_ >> 2, -(m), (m)); \
  if (d_) { cu->f += s_ ? s_ : d_; dirty = 1; moving |= s_; } } while (0)
  GLIDE(gin, (cu->gin >> 4) + 1); GLIDE(p, (cu->p >> 5) + 1); GLIDE(h, 96); GLIDE(b, 48);
  GLIDE(kt, 12); GLIDE(mk, 48); GLIDE(lo, 96); GLIDE(mf, 48); GLIDE(xc, 384);
#undef GLIDE
  st->settled = !moving;
 }
 if (st->dcn != 0 && st->cur.b == 0) {
  /* bias gone: let the servo offset decay, then leave the bias variant */
  st->dcn -= (st->dcn >> 3) + (st->dcn > 0 ? 1 : -1);
  if (st->dcn > -64 && st->dcn < 64) { st->dcn = 0; dirty = 1; }
 }
 if (dirty) coefs(st);
 if (st->variant & V_BIAS) set_dcy(st);
 run(x, &st->c, st->variant);
 if (st->variant & V_BIAS) servo(st);
}

void mc_drive_block(int32_t *x, const uint8_t prm[MC_DRIVE_PARAMS], mc_drive_state *st)
{
 const uint8_t *l = st->last;
 int same;
 if (prm[0] == 0) { st->active = 0; return; }
 same = prm[0] == l[0] && prm[1] == l[1] && prm[2] == l[2] && prm[3] == l[3] && prm[4] == l[4] && prm[5] == l[5];
 /* steady state: nothing to derive or smooth */
 if (same && st->settled && st->active && (st->dcn == 0 || st->cur.b != 0)) {
  if (st->variant & V_BIAS) { set_dcy(st); run(x, &st->c, st->variant); servo(st); }
  else run(x, &st->c, st->variant);
  return;
 }
 block_slow(x, prm, st, !same);
}

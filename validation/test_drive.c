/* Host validation of firmware/drive.c (build with ASan/UBSan):
 * cc -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
 *    firmware/drive.c validation/test_drive.c -lm -o validation/test_drive
 * Checks: bypass bit-exactness, fuzz (no UB/overflow), loudness (RMS and
 * 50 ms short-term) over a signal x parameter grid, monotonic loudness vs
 * amount, low-end (<150 Hz) preservation for kick bursts, bounded DC with
 * bias, and block-boundary smoothness while amount moves. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/drive.h"

#define FS 48000.0
#define N_SIG 14400 /* 0.3 s, multiple of 32 */
#define SKIP 1920   /* 40 ms excluded from steady-state metrics (glide) */

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; if (failures < 30) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static uint64_t rng_s = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rng_s ^= rng_s << 13; rng_s ^= rng_s >> 7; rng_s ^= rng_s << 17; return (uint32_t)(rng_s >> 11); }
static double urand(void) { return (rnd() & 0xFFFFFF) / 16777216.0; }
static double grand(void) { double a = urand() + 1e-12, b = urand(); return sqrt(-2 * log(a)) * cos(2 * M_PI * b); }

static int32_t q31(double v)
{
 double s = v * 2147483648.0;
 if (s > 2147483647.0) s = 2147483647.0;
 if (s < -2147483648.0) s = -2147483648.0;
 return (int32_t)lrint(s);
}

/* ------------------------------------------------------------ signals */
typedef struct { char name[48]; double x[N_SIG]; } tsig;
static tsig sigs[64];
static int nsig;

static void norm_rms(double *x, int n, double dbfs)
{
 double e = 0; int i;
 for (i = 0; i < n; i++) e += x[i] * x[i];
 e = sqrt(e / n);
 for (i = 0; i < n; i++) x[i] *= pow(10, dbfs / 20) / e;
}

/* 2nd-order Butterworth high-pass at 20 Hz */
static void hp20(double *x, int n)
{
 double w = tan(M_PI * 20 / FS), k = 1 / (1 + sqrt(2) * w + w * w);
 double b0 = k, b1 = -2 * k, b2 = k, a1 = 2 * (w * w - 1) * k, a2 = (1 - sqrt(2) * w + w * w) * k, z1 = 0, z2 = 0;
 int i;
 for (i = 0; i < n; i++) {
  double v = b0 * x[i] + z1;
  z1 = b1 * x[i] - a1 * v + z2; z2 = b2 * x[i] - a2 * v; x[i] = v;
 }
}

static void make_signals(void)
{
 static const double freqs[] = { 30, 60, 100, 250, 1000, 2500, 5000 };
 static const double lv[] = { -30, -18, -9, -3 };
 int f, l, i, k;
 for (f = 0; f < 7; f++)
  for (l = 0; l < 4; l++) {
   tsig *s = &sigs[nsig++];
   snprintf(s->name, sizeof s->name, "sine %4.0f Hz %3.0f dBFS", freqs[f], lv[l]);
   for (i = 0; i < N_SIG; i++) s->x[i] = pow(10, lv[l] / 20) * sin(2 * M_PI * freqs[f] * i / FS);
  }
 for (l = 0; l < 3; l++) {
  static const double nl[] = { -30, -18, -9 };
  tsig *w = &sigs[nsig++], *p = &sigs[nsig++];
  double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
  snprintf(w->name, sizeof w->name, "white noise %3.0f dBFS rms", nl[l]);
  snprintf(p->name, sizeof p->name, "pink noise %3.0f dBFS rms", nl[l]);
  for (i = 0; i < N_SIG; i++) {
   double g = grand();
   w->x[i] = g;
   b0 = 0.99886 * b0 + g * 0.0555179; b1 = 0.99332 * b1 + g * 0.0750759; b2 = 0.96900 * b2 + g * 0.1538520;
   b3 = 0.86650 * b3 + g * 0.3104856; b4 = 0.55000 * b4 + g * 0.5329522; b5 = -0.7616 * b5 - g * 0.0168980;
   p->x[i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + g * 0.5362; b6 = g * 0.115926;
  }
  hp20(p->x, N_SIG); /* audio-band pink: no infrasonic content */
  norm_rms(w->x, N_SIG, nl[l]); norm_rms(p->x, N_SIG, nl[l]);
  for (i = 0; i < N_SIG; i++) { /* keep peaks inside full scale */
   if (w->x[i] > 0.99) w->x[i] = 0.99; if (w->x[i] < -0.99) w->x[i] = -0.99;
   if (p->x[i] > 0.99) p->x[i] = 0.99; if (p->x[i] < -0.99) p->x[i] = -0.99;
  }
 }
 /* decaying drum-like bursts, 4 hits of 75 ms: kick, snare, hat, tom; 3 levels */
 for (k = 0; k < 4; k++)
  for (l = 0; l < 3; l++) {
   static const double pk[] = { -20, -8, -1 };
   static const char *nm[] = { "kick", "snare", "hat", "tom" };
   tsig *s = &sigs[nsig++];
   double hp = 0, prev = 0, ph = 0, m = 0;
   snprintf(s->name, sizeof s->name, "%s bursts %3.0f dBFS pk", nm[k], pk[l]);
   for (i = 0; i < N_SIG; i++) {
    double t = (i % 3600) / FS, v = 0, n = grand();
    if (k == 0) { ph += 2 * M_PI * (50 + 150 * exp(-t * 35)) / FS; v = exp(-t * 18) * sin(ph); }
    if (k == 1) { ph += 2 * M_PI * 190 / FS; v = exp(-t * 30) * (0.5 * sin(ph) + 0.5 * n); }
    if (k == 2) { hp = 0.7 * (hp + n - prev); prev = n; v = exp(-t * 60) * hp; }
    if (k == 3) { ph += 2 * M_PI * (110 + 40 * exp(-t * 20)) / FS; v = exp(-t * 12) * sin(ph); }
    if (i % 3600 == 0) ph = 0;
    s->x[i] = v;
    if (fabs(v) > m) m = fabs(v);
   }
   for (i = 0; i < N_SIG; i++) s->x[i] *= pow(10, pk[l] / 20) / m;
  }
}

/* ------------------------------------------------------------ helpers */
static void process(const double *in, double *out, int n, const uint8_t p[8])
{
 mc_drive_state st;
 int32_t blk[32];
 int b, i;
 mc_drive_reset(&st);
 for (b = 0; b < n / 32; b++) {
  for (i = 0; i < 32; i++) blk[i] = q31(in[32 * b + i]);
  mc_drive_block(blk, p, &st);
  for (i = 0; i < 32; i++) out[32 * b + i] = blk[i] / 2147483648.0;
 }
}

static double rms(const double *x, int a, int b)
{
 double e = 0; int i;
 for (i = a; i < b; i++) e += x[i] * x[i];
 return sqrt(e / (b - a) + 1e-30);
}

/* 4th-order Butterworth low-pass at 150 Hz (two biquads), energy after SKIP */
static double lf_energy(const double *x, int n)
{
 static const double q[2] = { 0.54119610, 1.3065630 };
 double y[N_SIG], e = 0;
 int s, i;
 memcpy(y, x, sizeof(double) * n);
 for (s = 0; s < 2; s++) {
  double w = tan(M_PI * 150 / FS), k = 1 / (1 + w / q[s] + w * w);
  double b0 = w * w * k, b1 = 2 * b0, b2 = b0, a1 = 2 * (w * w - 1) * k, a2 = (1 - w / q[s] + w * w) * k;
  double z1 = 0, z2 = 0;
  for (i = 0; i < n; i++) {
   double v = b0 * y[i] + z1;
   z1 = b1 * y[i] - a1 * v + z2; z2 = b2 * y[i] - a2 * v; y[i] = v;
  }
 }
 for (i = SKIP; i < n; i++) e += y[i] * y[i];
 return e;
}

static int cmpd(const void *a, const void *b) { double d = *(const double *)a - *(const double *)b; return d < 0 ? -1 : d > 0; }

/* ------------------------------------------------------------ tests */
static void test_bypass(void)
{
 mc_drive_state st;
 int32_t blk[32], ref[32];
 int it, i, n = 0;
 mc_drive_reset(&st);
 for (it = 0; it < 20000; it++) {
  uint8_t p[8];
  for (i = 0; i < 8; i++) p[i] = (uint8_t)rnd();
  if (it % 3) p[0] = 0;  /* mix bypass with active blocks */
  for (i = 0; i < 32; i++) blk[i] = ref[i] = (int32_t)rnd();
  mc_drive_block(blk, p, &st);
  if (p[0] == 0) { CHECK(!memcmp(blk, ref, sizeof blk), "bypass not bit-exact (iter %d)", it); n++; }
 }
 printf("bypass: %d amount-0 blocks bit-exact (random other bytes, after active blocks)\n", n);
}

static void test_fuzz(void)
{
 mc_drive_state st;
 int32_t blk[32];
 int it, i, mode;
 long blocks = 0;
 for (mode = 0; mode < 6; mode++) {
  uint8_t p[8] = { 0 };
  mc_drive_reset(&st);
  for (it = 0; it < 40000; it++) {
   if (it % (1 + (int)(rnd() % 7)) == 0)
    for (i = 0; i < 8; i++) p[i] = (uint8_t)(mode == 5 ? (rnd() & 1 ? 255 : 127) : rnd());
   for (i = 0; i < 32; i++) {
    uint32_t r = rnd();
    switch (mode) {
    case 0: blk[i] = (int32_t)r; break;                                    /* random words */
    case 1: blk[i] = (r & 1) ? INT32_MAX : INT32_MIN; break;               /* full-scale square */
    case 2: blk[i] = ((it / 40) & 1) ? INT32_MIN : INT32_MAX; break;       /* long DC rails */
    case 3: blk[i] = (int32_t)(r % 5) - 2; break;                          /* near silence */
    default: blk[i] = q31(sin(i * 0.3 + it) * (r & 0x100 ? 1.0 : 0.3)); break;
    }
   }
   mc_drive_block(blk, p, &st);
   blocks++;
   CHECK(st.c.cu >= 0 && st.c.cu <= 65536 && st.c.sh >= 0 && st.c.sh <= 8 && st.c.ncu == -st.c.cu, "coef range");
  }
 }
 printf("fuzz: %ld blocks (random/extreme params and inputs) without UB\n", blocks);
}

#define NA 6
static const uint8_t amounts[NA] = { 1, 16, 32, 64, 96, 127 };

static void test_loudness(void)
{
 static double out[N_SIG];
 static double gains[3][64 * 200];
 int ng[3] = { 0, 0, 0 };
 double worst_rms = 1e9, worst_st = 1e9, worst_mono = 1e9, worst_lf = 1e9;
 char wr[160] = "", ws[160] = "", wm[160] = "", wl[160] = "";
 int d, f, b, t, lo, s, a, k, combos = 0, runs = 0, mono_fail = 0;
 static const uint8_t dv[] = { 0, 127 }, fv[] = { 0, 64, 127 }, bv[] = { 0, 127 }, tv[] = { 0, 127 }, lv[] = { 0, 127 };
 for (d = 0; d < 2; d++) for (f = 0; f < 3; f++) for (b = 0; b < 2; b++) for (t = 0; t < 2; t++) for (lo = 0; lo < 2; lo++) {
  combos++;
  for (s = 0; s < nsig; s++) {
   double prev = 0, rin = rms(sigs[s].x, SKIP, N_SIG), lin = 0;
   int kick = strstr(sigs[s].name, "kick") != NULL;
   if (kick) lin = lf_energy(sigs[s].x, N_SIG);
   for (a = 0; a < NA; a++) {
    uint8_t p[8] = { amounts[a], dv[d], fv[f], bv[b], tv[t], lv[lo], 0, 0 };
    double r, g, gst = 1e9;
    process(sigs[s].x, out, N_SIG, p);
    runs++;
    r = rms(out, SKIP, N_SIG);
    g = 20 * log10(r / rin);
    /* 50 ms short-term windows that carry signal */
    for (k = SKIP; k + 2400 <= N_SIG; k += 1200) {
     double wi = rms(sigs[s].x, k, k + 2400), wo = rms(out, k, k + 2400);
     if (wi > 1e-4) { double gg = 20 * log10(wo / wi); if (gg < gst) gst = gg; }
    }
    if (g < worst_rms) { worst_rms = g; snprintf(wr, sizeof wr, "%s a%d d%d f%d b%d t%d l%d", sigs[s].name, p[0], p[1], p[2], p[3], p[4], p[5]); }
    if (gst < worst_st) { worst_st = gst; snprintf(ws, sizeof ws, "%s a%d d%d f%d b%d t%d l%d", sigs[s].name, p[0], p[1], p[2], p[3], p[4], p[5]); }
    /* monotonic within 0.5 dB; near the ceiling (output RMS >= -4 dBFS, i.e.
     * within ~4 dB of a full-scale square) the folder's waveform-dependent
     * crest factor may move loudness by up to 2 dB */
    if (a > 0 && g - prev < -0.5 && !(20 * log10(r) >= -4 && g - prev >= -2.0)) {
     if (++mono_fail <= 5) printf("  non-monotonic: %s a%d d%d f%d b%d t%d l%d: %+.2f dB -> %+.2f dB (out %.1f dBFS rms)\n",
      sigs[s].name, p[0], p[1], p[2], p[3], p[4], p[5], prev, g, 20 * log10(r));
    }
    if (a > 0 && g - prev < worst_mono) { worst_mono = g - prev; snprintf(wm, sizeof wm, "%s a%d d%d f%d b%d t%d l%d", sigs[s].name, p[0], p[1], p[2], p[3], p[4], p[5]); }
    prev = g;
    if (amounts[a] == 32) gains[0][ng[0]++] = g;
    if (amounts[a] == 64) gains[1][ng[1]++] = g;
    if (amounts[a] == 127) gains[2][ng[2]++] = g;
    if (kick) {
     double lg = 10 * log10(lf_energy(out, N_SIG) / lin);
     if (lg < worst_lf) { worst_lf = lg; snprintf(wl, sizeof wl, "%s a%d d%d f%d b%d t%d l%d", sigs[s].name, p[0], p[1], p[2], p[3], p[4], p[5]); }
    }
   }
  }
 }
 printf("loudness: %d signals x %d param combos x %d amounts = %d runs\n", nsig, combos, NA, runs);
 printf("  worst RMS gain        %+6.2f dB  (%s)\n", worst_rms, wr);
 printf("  worst 50ms gain       %+6.2f dB  (%s)\n", worst_st, ws);
 printf("  worst gain step vs previous amount %+6.2f dB  (%s)\n", worst_mono, wm);
 printf("  worst kick <150 Hz energy change %+6.2f dB  (%s)\n", worst_lf, wl);
 for (k = 0; k < 3; k++) {
  qsort(gains[k], ng[k], sizeof(double), cmpd);
  printf("  amount %3d: min %+6.2f dB  median %+6.2f dB  max %+6.2f dB\n", k == 0 ? 32 : k == 1 ? 64 : 127,
   gains[k][0], gains[k][ng[k] / 2], gains[k][ng[k] - 1]);
 }
 CHECK(worst_rms >= 0.0, "RMS loudness drop %.2f dB", worst_rms);
 CHECK(worst_st >= -0.5, "short-term loudness drop %.2f dB", worst_st);
 printf("  amount steps outside the monotonic tolerance: %d\n", mono_fail);
 CHECK(mono_fail == 0, "loudness not increasing with amount in %d cases (worst %.2f dB)", mono_fail, worst_mono);
 CHECK(worst_lf >= 0.0, "kick low end reduced %.2f dB", worst_lf);
}

static void test_dc(void)
{
 static double in[N_SIG * 4], out[N_SIG * 4];
 double worst = 0, worst_sil = 0;
 int s, a, f, i;
 static const uint8_t fv[] = { 0, 127 };
 for (s = 0; s < 4; s++) {
  for (i = 0; i < N_SIG * 4; i++) {
   double t = i / FS;
   in[i] = s == 0 ? 0.5 * sin(2 * M_PI * 80 * t) : s == 1 ? 0.3 * grand() : s == 2 ? 0.05 * sin(2 * M_PI * 440 * t)
    : ((i % 4800) < 2400 ? exp(-(i % 4800) / 600.0) * sin(2 * M_PI * 55 * t) : 0);
   if (in[i] > 0.99) in[i] = 0.99; if (in[i] < -0.99) in[i] = -0.99;
  }
  for (i = N_SIG * 3; i < N_SIG * 4; i++) in[i] = 0; /* trailing silence */
  for (a = 0; a < 3; a++)
   for (f = 0; f < 2; f++) {
    uint8_t p[8] = { (uint8_t)(a == 0 ? 20 : a == 1 ? 64 : 127), 0, fv[f], 127, 0, 0, 0, 0 };
    double m = 0, ms = 0;
    process(in, out, N_SIG * 4, p);
    for (i = N_SIG; i < N_SIG * 3; i++) m += out[i];
    m /= N_SIG * 2;
    for (i = N_SIG * 3 + 9600; i < N_SIG * 4; i++) ms += out[i];
    ms /= N_SIG - 9600;
    if (fabs(m) > worst) worst = fabs(m);
    if (fabs(ms) > worst_sil) worst_sil = fabs(ms);
   }
 }
 printf("dc: bias 127 worst |mean| while playing %.4f FS (%.1f dBFS), in silence after 200 ms %.5f FS\n",
  worst, 20 * log10(worst + 1e-12), worst_sil);
 CHECK(worst < 0.03, "DC with bias too large: %.4f", worst);
 CHECK(worst_sil < 0.005, "DC in silence too large: %.5f", worst_sil);
}

static void test_smooth(void)
{
 static double in[32 * 600];
 int sc, i, b;
 for (i = 0; i < 32 * 600; i++) in[i] = 0.25 * sin(2 * M_PI * 70 * i / FS);
 for (sc = 0; sc < 5; sc++) {
  mc_drive_state st;
  int32_t blk[32];
  double prev = 0, inner = 0, edge = 0;
  mc_drive_reset(&st);
  for (b = 0; b < 600; b++) {
   uint8_t p[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
   int pos = b % 300;
   int ramp = pos < 127 ? pos + 1 : 254 - pos > 0 ? 254 - pos : 1;
   switch (sc) {
   case 0: p[0] = (uint8_t)ramp; break;                                 /* amount sweep up/down */
   case 1: p[0] = (uint8_t)ramp; p[2] = 80; p[5] = 60; break;           /* with fold + low */
   case 2: p[0] = (uint8_t)ramp; p[3] = 100; p[4] = 90; break;          /* with bias + tone */
   case 3: p[0] = (b / 50) & 1 ? 127 : 1; break;                         /* amount jumps */
   default: p[0] = 100; p[2] = (uint8_t)((b / 40) & 1 ? 127 : 0); break; /* fold jumps */
   }
   for (i = 0; i < 32; i++) blk[i] = q31(in[32 * b + i]);
   mc_drive_block(blk, p, &st);
   for (i = 0; i < 32; i++) {
    double v = blk[i] / 2147483648.0, dlt = fabs(v - prev);
    if (b > 0) { if (i == 0) { if (dlt > edge) edge = dlt; } else if (dlt > inner) inner = dlt; }
    prev = v;
   }
  }
  printf("smooth[%d]: max step at block edges %.4f FS, max step inside blocks %.4f FS\n", sc, edge, inner);
  if (sc < 3) /* parameter ramps: block edges no rougher than the waveform itself */
   CHECK(edge <= inner * 1.25 + 0.002, "block-edge discontinuity in scenario %d (%.4f vs %.4f)", sc, edge, inner);
  else        /* hard jumps 1<->127: glide limits the per-block step */
   CHECK(edge <= 0.125, "block-edge step %.4f FS after a parameter jump (scenario %d)", edge, sc);
 }
}

int main(void)
{
 make_signals();
 test_bypass();
 test_fuzz();
 test_loudness();
 test_dc();
 test_smooth();
 if (failures) { printf("FAIL: %d check(s) failed\n", failures); return 1; }
 printf("PASS drive: bypass bit-exact, fuzz clean, loudness/low-end/DC/smoothness within limits\n");
 return 0;
}

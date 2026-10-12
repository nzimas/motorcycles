# DRIVE: per-track distortion kernel

Files: `firmware/drive.h` and `firmware/drive.c` (kernel), `validation/test_drive.c` (host tests),
`generation/audio/drive_eval.py` (ColdFire cost and target-vs-host bit-exactness under Unicorn).

## Interface

```c
void mc_drive_reset(mc_drive_state *st);                 /* 160 bytes per track */
void mc_drive_block(int32_t *samples /* 32 x Q31, in place */,
                    const uint8_t params[8], mc_drive_state *st);
```

Call it once per track and per block, right after the stock Volume+Dist stage (track `t` at
`0x80001858 + 128*t`). Calling convention: plain C/m68k ABI. It saves d2-d7/a2-a6 and does not
touch EMAC, MACSR or ACC. The deepest stack use measured on the target was 176 bytes. There is no
libc, no division or 64-bit arithmetic in the per-sample loops, and one 34-byte table.

## Parameters (8 bytes per sound)

Every byte is clamped to 0..127. Bytes 128..255 behave like 127; they are never treated as 0. An
all-zero record means OFF. With `amount > 0`, the other bytes at 0 give the neutral default: a
plain soft-to-hard clipper.

| byte | name   | meaning |
|------|--------|---------|
| 0 | amount | 0 = bypass. The block is untouched (bit-exact) and the call costs about 11 instructions. 1..127: pre-gain from 0.26x to 32x (exponential), clip level H from 1.0 (soft cubic knee) down to 0.4 (hard), make-up gain |
| 1 | drive  | Extra pre-gain on top of amount: x1 .. x4 (+12 dB), exponential. The total pre-gain is capped at 64x |
| 2 | fold   | 0 = clipper only. 1..127: triangle wavefolder before the clipper. The fold threshold P goes from 128 knees (fold=1, so it never folds in practice) down to 1 knee (fold=127). Fold also engages the low crossover (see `low`) and adds fold make-up, which fades in once folding becomes reachable: up to x1.5 at amount 1, rising to x2.5 (+7.9 dB) at amount 127 |
| 3 | bias   | 0 = symmetric. 1..127: offset before the shaper (even harmonics), up to 0.5 knee and never more than H/2. The resulting output DC is servoed out |
| 4 | tone   | 0 = open. 1..127: one-pole low-pass on the output, from open down to about 5.3 kHz, with up to +6 dB make-up that drives into the output clip (darker and denser). Its depth fades in over amount 1..8 |
| 5 | low    | 0 = full-band shaping. 1..127: crossover depth. Lows below about 119 Hz (1-pole) bypass the shaper and come back clean at x1.06, plus up to +0.5x sub boost. The effective depth is max(fold, low) |
| 6, 7 | reserved | Ignored. Write 0 |

Deviation from the brief's example: bias uses 0 = symmetric rather than 64 = symmetric. This
keeps the rule "0 bytes from old projects = neutral", and the polarity of the asymmetry makes
almost no audible difference.

## Algorithm

Per sample (s = sample >> 16, Q15):

```
[low]  lp = acc >> 6 ; acc += s - lp             1-pole crossover, 21-bit state
       u  = s*gin [- lp*gcl]                     pre-gain [crossover: subtract lows]
[fold] u  = |((u + F + b) mod 4F) - 2F| - F      triangle fold, mask arithmetic, F = 2^24
[bias] u += b                                    (fold variant: b added before folding)
       U  = clamp(u >> sh, +-H*8K')              clip level H, knee 8K'
       y  = U*(((U>>3)^2 >> 13) - 3K'^2/2^13) >> 10        = -(1.5u - 0.5u^3), soft knee
[bias] y -= dcy                                  static DC + output-DC servo
       o  = (y*go [+ lp*lo]) >> 13               make-up gain [+ clean lows]
[tone] z += (o - z)*kt >> 10 ; o = z             tone LP, state clipped in place
[any]  o  = clamp(o, -32768..32767)              the base variant needs no clip
       sample = (o << 16) | (sample & 0xFFFF)
```

- The shaper is the cubic soft clipper `1.5u - 0.5u^3` on [-1, 1], with the input clamped at
  H <= 1. As amount rises, H falls (1.0 down to 0.4, following amount^2), so the curve moves from
  a soft knee to hard clipping. The square uses a 3-bit coarser copy of U, so the linear term keeps
  3 extra bits. With 16-bit I/O this gives a noise floor around -83 dBFS at low drive.
- The fold uses a power-of-two period in the u domain, so it costs no division or table. A
  variable fold threshold is obtained by moving the knee instead: K = 2^32/P, with `sh`/K'
  renormalising the cubic. That variant runs only when folding is actually reachable
  (`s*gin > F`). Below that, the same transfer runs in the cheaper non-fold variant.
- Block-rate work, done only while parameters change or glide: targets from the bytes (exp2 by a
  17-entry table), then glide and coefficient derivation (divisions are allowed here). Control
  values glide by 1/4 of the remaining distance per block. The step is rate-limited to 0.53 dB per
  block for the pre-gain and 0.27 dB per block for the fold threshold, so a 1 -> 127 amount jump
  glides over about 60 ms and a fold jump over about 100 ms. All loop coefficients, including the
  output gain, are recomputed together from the current glide state, so the overflow bounds hold
  during glides.
- Loop variants: base, bias, tone, tone+bias, low(+tone), low+bias, fold(+low+tone) and
  fold+bias. Each is hand-written ColdFire assembly, 4x unrolled. It uses `mvs.w` to read the
  high word directly, keeps every constant in a register or as a `(d16,a2)` operand, and avoids
  64-bit products. The C reference in `drive.c` is bit-identical and is what the host tests run.
  `drive_eval.py` checks every output word of the target against it.

## Loudness strategy

- Make-up gain is derived per coefficient update from the exact integer transfer, never measured
  per sample. Let `ymax` be the shaper output at the reachable input extremes.
  - Base variant: `go = (32767*8192)/ymax`. A full-scale input then maps to full-scale output.
    Because the clipped cubic is concave, `f(s) >= s` for every sample at every amount, and the
    small-signal gain `1.5*Gin/y(min(Gin,H))` grows monotonically from x1.02 at amount 1. The
    output provably cannot exceed Q15, so this variant needs no output clip.
  - Bias: normalised to the smaller of the mean output swing and the small-signal slope at the
    bias point. Any asymmetric overshoot goes into the output clip.
  - Tone and fold add make-up (up to x2 and x2.5) that drives into the output clip. This is the
    "denser" part.
  - A cap `|go| <= 1.625*2^30 / span` guarantees no int32 overflow in `y*go + lp*lo`.
- Output DC with bias: an integral servo acts on the measured output mean, taken after the clip
  because that is where asymmetric clipping creates DC. It adjusts the pre-gain DC offset by 1/32
  of the block mean per block, which works like a DC blocker with its corner near 7.5 Hz
  (tau >= 21 ms). Static DC is removed at once. +0.5 dB of make-up offsets the servo's loss at
  30 Hz.
- Low end: in the base/bias/tone variants (no crossover), the clipper keeps the fundamental
  (`f(u) >= u`). Folding destroys the fundamental, so fold always engages the crossover: lows
  bypass the folder and return at x1.06.

## Measured cost (Unicorn, UC_CPU_M68K_CFV4E, instructions per 32-sample block)

`validation/venv/bin/python generation/audio/drive_eval.py -O2` (also accepts `-Os` and
`-DMC_DRIVE_UNROLL=2`).

| case (-O2, unroll 4) | steady | /sample | while gliding | /sample |
|---|---|---|---|---|
| bypass (amount 0) | 11 | 0.34 | 11 | 0.34 |
| typical: amount 64 (base variant) | 675 | 21.1 | 1018 | 31.8 |
| drive + tone | 963 | 30.1 | 1353 | 42.3 |
| drive + tone + low | 1251 | 39.1 | 1663 | 52.0 |
| bias only | 952 | 29.8 | 1424 | 44.5 |
| fold only | 1443 | 45.1 | 1853 | 57.9 |
| worst (all bytes 127) | 1526 | 47.7 | 2004 | 62.6 |

- Pure inner-loop cost per sample (from the instruction sequence, plus 0.75 for the loop):
  - base: 17.75
  - bias: +6.25
  - tone: +8
  - low (crossover + clean lows + LP): +10
  - fold: +5.5
- Per-block overhead (call, prologue, fast-path checks) is about 100 instructions, or about 3 per
  sample.
- Activation block: about 2300 instructions. Parameter-change block: about 2000. Fuzz maximum:
  2323. This is a one-block spike, because `targets()` plus coefficient derivation run in that
  block.
- Six tracks at amount only: about 4050 instructions per block, inside the 4800 budget. Six
  tracks with every feature on: about 9200 instructions per block.
- Code size: 8.4 KB at -O2, 7.0 KB at -Os, 5.3 KB at -Os with `-DMC_DRIVE_UNROLL=2` (which costs
  +0.75 instructions/sample).
- Target vs host on 7 cases x 4 signals x 320 blocks, plus 2400 fuzz blocks (random bytes
  changing every 1..8 blocks, random, full-scale-square, sine and near-silent input): 0
  mismatching words.
- Instruction counts are not cycles. `muls.l`, `divs.l` (block rate only), memory operands and
  taken branches all cost more than one cycle on a real V4e.

## Test results (`validation/test_drive.c`, ASan + UBSan)

```
cc -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all \
   firmware/drive.c validation/test_drive.c -lm -o test_drive && ./test_drive
```

- Bypass: 13,364 amount-0 blocks interleaved with active blocks and random other bytes are all
  bit-exact.
- Fuzz: 240,000 blocks with random or extreme bytes and random, rail-to-rail square, long DC rail,
  near-silent or sine input. No UB or overflow, and the coefficient-range invariants hold. The
  same passes with `-DMC_DRIVE_UNROLL=2`.
- Loudness: 46 signals x 48 parameter combinations x amounts {1, 16, 32, 64, 96, 127} = 13,248
  runs.
  - Signals: sines at 30/60/100/250/1k/2.5k/5k Hz at -30/-18/-9/-3 dBFS; white and pink noise
    (pink high-passed at 20 Hz) at -30/-18/-9 dBFS RMS; kick, snare, hat and tom bursts at
    -20/-8/-1 dBFS peak.
  - Parameter grid: drive {0,127} x fold {0,64,127} x bias {0,127} x tone {0,127} x low {0,127}.
  - Results:
    - Worst full-signal RMS gain: +0.11 dB. Output RMS is never below input RMS.
    - Worst 50 ms window: -0.43 dB (tolerance -0.5). This is the bias DC servo on a -3 dBFS
      30 Hz sine.
    - Gain by amount:
      - amount 32: min +0.52 dB, median +6.2 dB, max +24.3 dB
      - amount 64: min +3.62 dB, median +12.2 dB, max +31.9 dB
      - amount 127: min +3.93 dB, median +18.3 dB, max +35.2 dB
    - Monotonic in amount: 0 violations. The tolerance is -0.5 dB, or -2 dB when the output is
      already >= -4 dBFS RMS (the ceiling). The worst observed step was -1.25 dB, with fold near
      the ceiling.
- Low end: on kick bursts, energy below 150 Hz (4th-order Butterworth) is never reduced. The worst
  case is +0.14 dB.
- DC with bias 127 (sine, noise, quiet sine, decaying 55 Hz bursts; with and without fold):
  - |mean| while playing: <= 0.011 FS (-39 dBFS)
  - |mean| 200 ms into silence: 0.00003 FS
- Smoothness:
  - Amount ramps (alone, with fold+low, with bias+tone): the largest step at a block edge is at
    most the largest step inside blocks.
  - Hard jumps (amount 1 <-> 127, fold 0 <-> 127): block-edge steps <= 0.079 FS.
- Spectrum, 1 kHz at -26 dBFS (gain, THD+N): amount 1 +0.2 dB, -57 dB; amount 32 +2.3 dB, -60 dB;
  amount 64 +12.9 dB, -55 dB; amount 96 +24.3 dB, -33 dB; amount 127 +28.5 dB, -9 dB. For
  comparison, stock Dist 127 gives +16.6 dB and -31 dB. Bias 64 at amount 64 makes H2 the
  dominant harmonic (-34 dB).

## Limitations

- **Cost:** with every feature on, a track costs about 48 instructions/sample steady and about 63
  while gliding. Only base, bias and tone stay within the 25-30 target. Glides add about 10-15
  instructions/sample for roughly 60-100 ms after each parameter change, and a parameter-change
  block is a one-off spike of about 2000 instructions.
- **Resolution:** processing is 16-bit; the low 16 bits of the input pass through.
- **Fold:** a folder can quieten near-constant (square-like) input at particular fold settings.
  With high-frequency content near the output ceiling, loudness can move by up to about 1.3 dB
  between amount steps.
- **Bias:** while bias is engaged, the servo high-passes the shaped path near 7.5 Hz. Loud
  asymmetric passages leave a DC tail with a time constant of about 21 ms.
- **Switching amount to and from 0:** this is immediate, as required for a bit-exact bypass.
  Amount 1 is nearly transparent, so sweeping the knob is smooth. A jump from 0 to a high amount
  fades in from neutral over about 60 ms; a jump down to 0 is a hard switch.
- **Loudness:** median gains of +12..18 dB at amounts 64..127 will push the downstream x4 master
  stage. This follows from the brief (never quieter, louder with amount).
- **Instructions used:** `mvs.w` (ISA_B/C) is used. `ff1` is avoided because Unicorn's CFV4E model
  does not implement it.

## Noise gate (RC12)

The stage adds a lot of gain. Without a gate it lifts the noise floor and quiet
tails into constant background static. The gate lives in `firmware/drive_ui.c`
and wraps the kernel for each track. The kernel itself is unchanged.

- **Detector:** the block peak of the stage's input, taken from every 4th sample
  with a branch-free |v|. The envelope halves (−6 dB) each block between peaks.
- **Open:** at −60 dBFS or above (Q31 full scale), instantly, so the first
  transient of a hit is fully distorted.
- **Close:** below −66 dBFS (hysteresis). It holds for 30 blocks (20 ms), then
  fades the distorted signal back to the clean input over 60 blocks (40 ms). The
  fade is a per-sample crossfade at 16-bit precision.
- **Closed:** the clean signal passes bit-for-bit, and the kernel does not run
  at all.
- **Cost (packaged firmware, steady):**
  - plain clipper with the gate: 24.4 instructions per sample per track (21.2
    without the gate);
  - a closed gate: about 100 instructions per block.
- **Proof:** `validation/test_selected_drive.py` checks that quiet input passes
  untouched, that a hit opens the gate instantly, the 20 ms hold, and the 40 ms
  release back to the clean signal.
- **Thresholds are fixed for now.** The record's spare byte 6 could later make
  them adjustable per sound.

## DJ filter (RC13)

**Where the audio reads the settings (RC14).** The encoders write into the
sound record of the UI's sound owner (`owner+16`). The audio engine's kit
pointer (0x800017e4) and per-track sound pointers (0x800017ec) are not
guaranteed to reach that same record, and on the unit they evidently did not:
RC10–RC13 never heard FUNC + SWEEP or FUNC + PITCH changes.
- **Mirror.** `mc_drive_sync()` runs on the UI clock (`mc_clock`). It copies
  the 8 bytes of each track's owner record into `ui_params[6][8]` under an IRQ
  mask. `mc_drive_encoder()` also updates the mirror at once.
- **What the audio reads.** `mc_drive_audio()` reads the mirror for a track
  whose sound in use is the kit's own sound. When a sound lock has put another
  sound on the track, it reads that sound's bytes.
- **On-screen check.** The F overlay shows "ok" when the filter position the
  audio last used equals the value on screen.
- **Proof.** `validation/test_selected_drive.py` checks two things:
  - with stale kit bytes and different owner records, the output matches the
    owner records once the clock has run;
  - a locked sound uses its own bytes.

There is one resonant filter per track (`firmware/djfilter.c`), running after
the gated distortion and before pan, the mixer and the sends. It runs even when
the distortion is off. Its settings live in bytes 6 and 7 of the same per-sound
record, so they persist through the same signed save path.

- **Byte 6, position (int8):**
  - 0 is off: a bit-exact bypass that costs almost nothing;
  - −1 to −63 is a resonant low-pass sweeping from 7 kHz down to 50 Hz;
  - +1 to +63 is a resonant high-pass sweeping from 20 Hz up to 7 kHz.
  - Both sweeps are exponential, and the cutoff glides from block to block.
- **Byte 7, resonance:** 0 means the default (48), and 1–127 scale it. The
  damping q runs from 2.0 down to 0.16, for a peak of about 6×; the measured
  maximum is +15.6 dB.
- **Algorithm:** a Chamberlin state-variable filter with Q12 coefficients and
  Q23 states (input >> 8). The states are clamped to ±2^26 and the output
  saturates to Q31. On engaging or switching mode it starts from the current
  input, so it doesn't click.
- **Host tests** (`validation/test_djfilter.c`):
  - off is bit-exact;
  - at position −40 the low-pass passes 80 Hz (+0.1 dB) and cuts 6 kHz
    (−50.6 dB);
  - at position +40 the high-pass cuts 80 Hz (−39.9 dB) and passes 6 kHz
    (+0.6 dB);
  - at every position and resonance it is stable after full-scale noise and
    settles below −84 dBFS.
- **Packaged firmware:** about 37 instructions per sample per track while
  running.
- **Shared CPU guard:** distortion plus filter across the six tracks stays
  within 240 units per sample (about 7.7k instructions per block). Filters are
  never trimmed; only distortion features are.
- **Controls:**
  - FUNC + PITCH (encoder 2) sets the selected track's position, clamped to
    ±63, and shows an "F" overlay with LP/HP/OFF, the value and a bar that
    grows from the centre. Stock FUNC + PITCH semitone stepping is replaced.
  - The generators set the resonance (Melodic 20–60, Abstract 30–100, Rhythmic
    40 + 0.6 × Brutality and up, Free 0–127) and leave the position off.

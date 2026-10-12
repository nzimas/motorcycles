# LYRA: machine 5, in place of CHORD (RC16)

The user dropped CHORD: selecting a CHORD track made the unit sluggish, and
they had no use for it. LYRA takes its slot and is an economical voice after
SOMA's LYRA-8, by way of Mike Moreno's LIRA-8 and xmacex's LNRA-8. No code
from those projects is used; it is an original fixed-point design.

## Voice (`firmware/lyra.c`)

- **Structure.** Four oscillators in two pairs. Each oscillator morphs from
  triangle to near square.
- **Cross-modulation.** Each oscillator is phase-modulated by its partner and
  by the other pair's previous output: pair B into A at 1/2 depth, A into B at
  1/4. This is feedback FM, from clean to chaotic.
- **Second pair.** Detuned against the first, or set at an interval.
- **Hyper LFO.** Swings the modulation depth by ±50% and the second pair's
  pitch by ±0.4%.
- **Output.** About 3/8 of Q31, through a ~15 Hz DC blocker, then the stock
  amp chain (envelope 0x400a9252, VCA 0x400a9430, PUNCH 0x400a967a), set up as
  TONE/MACRO.

| Knob | Name (short) | Meaning | Default |
|---|---|---|---|
| COLOR | Spread (SPRD) | 0..63: detune 0 to ~2.5 %. 64..127: second pair at a fifth, octave, octave + fifth, two octaves (16 steps each) | 20 |
| SHAPE | Sharpness (SHRP) | triangle (0) to near square (127), gain 2^(x/21) into a clip | 40 |
| SWEEP | Cross Mod (XMOD) | depth ∝ x³: clean to ~30, grit around 60, chaos from ~90 | 20 |
| CONTOUR | Hyper LFO (HYPR) | 0 off; 0.05 Hz to 40 Hz, exponential | 0 |
| DECAY / GATE / PUNCH | — | the stock amp chain | |

**Cost** (packaged firmware, emulated, excluding the amp chain):
- about 4,000 instructions per 32-sample block while sounding;
- about 120 while silent (closed envelope, no trig).

For comparison, upstream measurements give about 7,600 for a stock voice and
about 8,600 for CHORD, which runs every block whether it sounds or not.

## Firmware patches (`firmware/build-selected.py`, `LYRA_*`)

The MACRO tweak keeps the voice-loop tables, names and descriptors in its
boot-copied payload, stored in the image at these file offsets:

| What | Where | Change |
|---|---|---|
| update / render entry 5 | 0x1c1f88 / 0x1c1fa4 | CHORD 0x400aae88 / 0x400ab24c → `lyra_update` / `lyra_render` |
| Name | 0x401300ee | "Chord" → "Lyra" (the only user is the MACHINES table) |
| Descriptors 71..74 | 0x1c2f90 + 0x38·i | long name +0x2c, short name +0x34, default +0x10; SHAPE max +0x0c 37 → 127 |
| SHAPE knob graphic | 0x40046eb4 | scaled by 127 instead of 37 |
| SHAPE detents | 0x4004fd00 / 0x4004fd04 | TONE's list |
| Old-file conversion | 0x4005cc46 | the remap of machine 5 SHAPE through the chord-type table is skipped |

- **Defaults on a machine change** come from the descriptors.
- **Projects with CHORD tracks** load them as LYRA with CHORD's stored values.

## Generators

- The CHORD archetypes become four LYRA archetypes: drone swarm, lyra growl,
  interval bell and chaos hit.
- The chord-type limits (SHAPE ≤ 37, the pitch floor of 52) are gone.
- In a Rhythmic Kit, LYRA can play percussion on tracks 4..6.

## Proof

- `validation/test_lyra.c` (host): pitch within 1 % from note 36 to 84; DC
  and output bounds on a knob grid.
- `validation/test_selected_lyra.py` (packaged firmware): patched image, block
  for block equal to the host reference, amp chain order, silent-voice skip,
  cost.

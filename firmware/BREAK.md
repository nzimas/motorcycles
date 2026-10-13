# Live breaks

`firmware/brk.c`, `firmware/brk_hooks.S` and `firmware/runtime.c`.

While transport runs and generator mode is off, holding PAGE plays the pattern
through a variation drawn fresh at each press. Releasing PAGE ends it. The
press, repeats and release are consumed, so the stock PAGE function is not
available during playback. Nothing is written to the pattern, unless the
break is stored (below).

## Hooks

The stock trig builder `0x400548ce(track, pattern, kit, step, reuse, out)`
reads every per-step field from `step`: trig word, condition, probability,
micro and swing, note, velocity, length, retrig, locks and sound lock. The
sequencer interrupt (level 2) calls it one step ahead of the playhead, with
`reuse = 0` and `out != 0`. The precompute after Play and trig preview use
other arguments, so the hooks leave them alone.

| Site | Stock bytes | Replaced with |
|---|---|---|
| `0x400548ce` | `4fef ffc4 7201` | `jsr mc_brk_entry_hook` |
| `0x40054c0e` | `254e 0048 200a` | `jsr mc_brk_exit_hook` |

**Entry hook.** It replaces the step argument with `mc_break_source(step)`,
replays the two instructions and continues.

**Exit hook.** It replays its instructions, then reshapes the built message:
- **Locks.** Break locks go into its lock buffer (`msg+72`: count at +8,
  entries at +20 + 8i) for pan (k22), COLOR, SHAPE and SWEEP (k11..13) and
  octave pitch (k10).
- **Ratchets.** Flags bit 15 is set, with the retrig rate index (+56), length
  code (+60) and velocity curve (+64).

Retrigs are generated on the audio side. Control All runs later, in the lock
applier, so its overrides still win.

The audio stage (`mc_drive_audio`) applies `mc_break_audio`: filter sweeps
reach their target over about 2 s, and distortion pushes add to the amount.
These overrides are never stored.

## Limits

- The step that is already queued when PAGE goes down plays as written. A
  break therefore starts within one step.
- Swing follows the source step's parity.

## Proof

- `validation/test_brk.c` (host): every recipe and layer, step ranges, lock
  bounds, the kick rules, filter sweep targets and the end of a break.
- `validation/test_selected_brk.py` (packaged firmware): the patched sites,
  remapping only on the sequencer path, the builder frame and registers kept as
  stock, message changes only on the sequencer path, and PAGE behaviour.

## Storing a break (RC21)

`firmware/store.c`, `firmware/store_hooks.S`.

While a break is held, the step keys show the current bank:
- **Lit:** the pattern has trigs.
- **Flashing (about 4 Hz):** the pattern playing.
- **Unlit:** the pattern is empty.

A step key stores the variation into that slot of the bank, which may be the
playing pattern. The press, its repeats and its release are consumed.

**What is stored.** The current pattern and kit are copied to a staging
buffer and the break plan is baked into it:
- **Rhythm.** Each step up to the track length takes every per-step field and
  the 68-byte lock record of the step the recipe plays there. This is
  `mc_break_source`, the same function the live break uses.
- **Movement.** Each trig in step order is hit 0, 1, 2… It gets the pan,
  COLOR/SHAPE/SWEEP and octave locks the live message hook draws for that
  hit, with the step lock count and the track's slot-used flags. A new
  (slot, track) pair is only added while the pattern holds fewer than the
  stock saver's 79.
- **Ratchets.** These are retrig flag bit 15, rate (+384), length code
  (+320: 14 is one step, 30 is two) and velocity curve (+448). They go on note
  trigs without a written retrig, drawn as the live hook does.
- **Kit.** Filter targets go into the sound's filter byte (resonance 70 if it
  was 0). Distortion pushes are added to the amount.

The buffer then goes into the target exactly as the stock Paste Pattern
(`0x40010a4a`) writes the current pattern:
- the pattern (30,710 bytes, with the slot index stamped at +30706);
- the kit (7,094 bytes from the kit container `app+116`, getter `vtable+48`);
- the pattern owner's `vtable+16` notification;
- the kit container's `vtable+92` notification (`app+116, index, 0`).

The undo copy and the stock popup are left out.

## Hooks (RC21)

The store wraps three of RC19's hooks without changing them:

| Site | Now jumps to | Then |
|---|---|---|
| key dispatcher `0x400060fe` | `mc_store_key_hook` | `mc_key_hook` unless consumed |
| LED mode query `0x40005f86` | `mc_store_light_hook` | `mc_light_hook`, with the mode replaced during a break |
| five LED vtable slots | `mc_store_led_hook` | `mc_led_hook` |

The LED pass repaints when the flash phase turns and once when the break
ends, never at interrupt level.

**Where it lives.** The code is not in the payload. It lives in dead CHORD
caves A0, B and C (`adapter/cave.h`, `REPRODUCE-SELECTED.md`). Its state and
the 37,804-byte staging buffer sit in the RAM after the payload's BSS, at
`0x4306F000`. The payload is therefore byte for byte RC19's, so no audio
routine moves (see RC20).

**Proof.** `validation/test_selected_store.py`:
- The package differs from RC19's only at those hook sites and caves.
- The LEDs and their repaint.
- Sixteen stored breaks, covering every recipe and layer:
  - every step matches `mc_brk_step`;
  - every trig's locks and ratchets match `mc_brk_msg` for that hit;
  - the kit edits are as listed above;
  - other slots and the source are untouched.
- The banks, storing over the playing pattern, and the pair budget.

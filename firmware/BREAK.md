# Live breaks

`firmware/brk.c`, `firmware/brk_hooks.S` and `firmware/runtime.c`.

While transport runs and generator mode is off, holding PAGE plays the pattern
through a variation drawn fresh at each press. Releasing PAGE ends it. The
press, repeats and release are consumed, so the stock PAGE function is not
available during playback. Nothing is written to the pattern.

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

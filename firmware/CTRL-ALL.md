# Latched Control All (RC15)

**Gesture.** FUNC + TRK toggles the mode. In stock OS 1.13, FUNC + TRK opens
the Track Setup menu (`TrackMenuView`, 0x4003e020); that shortcut is
replaced. Stock Control All is "TRK held + knob" and still works as before.

## While the mode is on

- **The display** blinks a large CTRL / ALL banner: 500 ms on, 500 ms off. A
  knob turn hides it for 1.5 s, so the value popup stays visible.
- **The TRK LED** (id 31) is requested lit on every LED pass.
- **Every knob** acts as stock Control All:
  - `mc_held()` answers "TRK held" to the stock encoder handlers.
  - Their held-key function register is loaded at 0x4001f55c (`lea
    0x4007faf4,a4`, ParameterPageView) and 0x4001aaa8 (`lea 0x4007faf4,a2`,
    LEVEL). Both loads are retargeted to `mc_held`.
  - While a trig is held (`0x4006bdfe(0x400cf9a8())`), the real key state
    passes through, so p-lock editing works as usual.
- **FUNC + PITCH and FUNC + SWEEP** move the DJ filter and the distortion
  amount of all six tracks by the same step, each clamped. The overlay shows
  "ALL".
- **Parameter locks are overridden** for the parameters turned in the mode:
  - The audio dispatcher's knob-edit handler 0x40059534 `(value, track, k)`
    has its entry replaced by `mc_param_edit_hook`, which records the value
    and replays the 6 overwritten bytes.
  - The lock applier 0x400583da is called at 0x40058f36 (lock-only trigs) and
    0x40059194 (note trigs, after the trig's sound is applied). Both calls
    go to `mc_ca_locks`.
  - `mc_ca_locks` gives every lock entry for a turned k the Control All value.
    It also appends an entry when the trig has none, which covers
    sound-locked trigs, whose pool sound would otherwise bring its own value.
    Then it calls the stock applier.
  - The lock buffer is a 20 + 8 × 66-byte pool block (pool initialised with
    66 at 0x40058a96). A step uses at most 33 entries.

## Turning it off

- **Sounds.** All six sound records (owner data, the same records the stock
  Control All writes through `SoundParameterSet` +4) are restored to the
  snapshot taken when the mode started, and the changed owners are notified.
- **Locks.** Step locks apply again from the next trig.
- **Pattern changes.** A pattern load during the mode
  (`mc_music_context_stamp`) retakes the snapshot from the newly loaded
  sounds and drops the earlier values.
- **Generators.** Entering generator mode turns Control All off first, so a
  generator neither sees nor is undone by provisional values.

**Proof:** `validation/test_selected_ctrlall.py`, packaged firmware in
Unicorn.

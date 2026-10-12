# R1-rc3 — Model:Cycles generative firmware

This revision raises stock/MACRO kick tuning, strengthens envelopes and sequencer
kick velocities, and makes Patch audition use the target track's normal default
note (rather than the old fixed note52). MACRO remains eligible in Patch and Kit;
all47models are reachable across appropriate paths. No automatic transfer is
performed. Offline checks do not measure hardware audio/USB deadlines.

## Configuration

Vendor OS remains 1.13. The combined build preserves the nine feature cards
identified by the user's annotated screenshot: Model-TG, MACRO, Trig preview,
Easier trig removal, Latching mute mode, Sample preview, Scroll long names,
startup animation and six-channel USB audio. The selected upstream commits are
pinned in selected-configuration.json. This reconstructs the user's selected
options; the precise revision already installed cannot be read back independently.
No Syntakt engines, Arpeggiator or extended tempo are included.

## Controls

1. Stop playback. Select the track you want to target.
2. Hold PAGE and press pad 1 for Patch, 2 for Kit, 3 for Phrase, or 4 for Pattern.
   The track is captured when entering; changing generators retains that target.
3. Release PAGE: generator mode stays latched. Press and release step 1 for Abstract, 2 for Rhythmic,
   3 for Melodic, or 4 for Free.
4. Hold that same step again for at least 600 ms to generate once. Release it
   before another hold. The initial selecting press never generates.
5. After successful Patch generation, short-tap the blinking path step to audition
   the current generated sound. A long hold still generates a new patch.
6. Tap PAGE to exit generator mode, then play the result.

Pitch adjusts Density; Amp Decay adjusts Brutality, each 0–100%. Density applies
only to Phrase and Pattern. Brutality is inactive in Melodic and in Abstract
sequence generation. These knobs are consumed while generator mode is
active so they do not adjust the ordinary sound controls. Other knobs retain
ordinary behavior. Each generator/path remembers separate values during the
session; reboot resets them. Defaults by path: Density 25/45/35/40%, Brutality
40/55/0/45% (Abstract/Rhythmic/Melodic/Free).

The selected path LED blinks. Turning a generator knob shows a 12-step value
bar across steps 5–16 for one second. Successful generation lights that bar
for 800 ms and shows PATCH/KIT/PHRASE/PATTERN CREATED. Failure has a distinct
alternating LED pattern and no success popup. A PAGE tap exits, cancels pending holds and
restores ordinary LED ownership. Releasing PAGE after entry leaves mode latched.

Patch replaces the captured track's sound; Kit replaces all six sounds.
Phrase replaces the captured track's sequence and its parameter locks, preserving
its effective length. Pattern replaces all six sequences and their locks, and
chooses tempo and global length; it preserves the sounds and mute mask.
Rhythmic Kit reserves track 1 for kick, track 2 for snare/metallic snare,
track 3 for hat/click, and tracks 4–6 for varied percussion. Rhythmic Pattern
prioritizes those first three roles as Density increases; it preserves the sounds.
Generate a rhythmic Kit first to establish matching sound roles.

Every generated stock Kick or MACRO bass-drum model34 receives PITCH64–76,
PUNCH enabled and nonnegative fine tuning. Stock decay varies40–85 and MACRO
kick decay36–80. Active Brutality changes bounded timbre controls while retaining
these kick tuning and envelope protections. PITCH64 is neutral, so tuning combines with the trigger note;
it is not an absolute MIDI note. Known kick tracks use generated note60 or higher
and velocity96 or higher, with additional note compensation for existing low
sound tuning. This applies to Phrase and Pattern across all paths. Other sounds
retain their path's note/velocity behavior. Existing sounds are preserved by
Phrase/Pattern, so generate Patch/Kit to apply the new kick envelope profiles.

Audition uses the captured track's default note, matching normal pad behavior.
Custom low default notes remain user settings; adjust those if the normal pad is
also too low. The old preview note52 was eight semitones below stock default60.

Generators replace existing content. They have no persistent undo history.

## First device test

Use a disposable project or a saved backup. First check normal boot, sound,
USB audio and the selected mods. Test Patch on one track, then all paths and
fresh repeated holds. Check Phrase leaves other tracks and sounds unchanged;
check Kit and Pattern affect their documented scopes. Confirm short presses,
Patch audition, interrupted holds, PAGE exit and knobs behave as described.

Generation is rejected during playback. Stopped transport still runs audio/USB;
watch for a transient dropout during a large Pattern commit. Its critical-section
instruction count is measured in emulation, but hardware timing is not yet known.
MACRO's six-voice CPU load and Sampler Slice/Granular musical behavior also need
physical testing. Sampler generation reuses an existing source and preserves
fractional start/end/filter controls; it never invents sample identifiers.

The bootloader/updater sections remain byte-identical to official OS1.13. Keep
that official file for recovery through the unit's standard startup update path.
No device or transfer success is claimed by these offline results.

# Selected integration revision — R1-rc3

R1-rc1 installed successfully according to user feedback, with useful generation
and observed latched operation. R1-rc2 added short-tap audition of the active Patch
path after successful generation, explicit PAGE-release latching/PAGE-tap exit,
and fixed rhythmic Kit roles (kick, snare, hat/click, three varied percussion).
Rhythmic Pattern prioritizes the first three roles while preserving sounds.

Audition uses the existing stock note-on/note-off core used by Model-TG preview,
with a stopped-transport guard, 150 ms note-off and immediate release on exit.
It bypasses the recording/MIDI pad wrapper. The native audition test separately
executes those stock core functions with mocked final queues and checks no
canonical sound/sequence writes. Hardware sound and timing remain to be tested.

The selected upstream composition, protected bootloader/updater and vendor OS
version1.13 remain preserved. Exact source/package hashes and current offline
reports are included in release/R1-rc3. Runtime stays in Model-TG's reserved last
MiB, outside the sampler pool and MACRO BSS. Full peripheral boot/cache behavior,
audio/USB deadlines and subjective sound quality remain physical-device checks.
R1-rc3 revises stock and MACRO kick tuning/envelopes, protects known kick sequence
notes and velocities across all paths, and corrects audition to use the captured
track's default note through the same getter as normal pad dispatch. MACRO
eligibility and all47model reachability are independently tested. Stock DSP
pitch proof verifies neutral PITCH64, note60≈261.6Hz on Tone, note52≈164.8Hz,
and Fine=(control−64)/32 semitones. These mapping checks do not establish
subjective kick punch. Active Brutality retains bounded kick timbre variation
without overriding tuning/decay protections. No automatic firmware transfer is performed.

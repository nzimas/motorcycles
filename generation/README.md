# Model:Cycles R1 musical staging core

This is an implemented, host-tested portable C99 algorithm, **not installed firmware**. It constructs a complete caller-owned plan. A proven firmware transaction adapter must convert the semantic events and publish the sounds/sequences. Do not copy `mc_plan` into device RAM or files. No hardware/MIDI access occurred.

Four generators × four paths are implemented. Patch selects one captured sound; Kit six sounds and no sequences; Phrase one captured line and no sounds/tempo; Pattern clears all six prior lines and populates a density-dependent subset. Zero density means one voice/one event; 100 means six voices with full event capacity. Interface defaults are Density Abstract25%/Rhythmic45%/Melodic35%/Free40% and Brutality40%/55%/inactive0%/45%, independent per generator/path; persistence belongs to interface implementation. Seed advancement belongs to successful confirmation integration, not this pure function.

Abstract draws dispersed events and broad machine/contour/decay combinations without percussion assignments. Rhythmic rotates six roles (anchor, impact, detail, texture, pitched, response) per roll and generates repeating seed-selected motifs on 12/16-step cycles. Melodic chooses Tone/Chord when available and one of four pitch collections, varied note lengths and ordered phrases. Free samples all explicitly allowed stock machines and event positions. Roles are shuffled, so no physical track is permanently bass, lead or percussion. Density sets participation/event counts. Brutality affects sound colour/shape/sweep rather than volume; in sequences it affects bounded accents and pitch disturbance while preserving event count. Inactive controls are ignored, including Abstract sequence Brutality and all Melodic Brutality. Pattern tempo ranges vary by path.

`machine_mask` now permits the selected build’s stock0..5, Sampler6 and MACRO7. Syntakt engines remain excluded. Callers must derive this mask from the selected configuration; Sampler sound generation additionally requires an already-Sampler track with valid source metadata. Audibility, output level and actual musical quality require listening; this core does not claim a limiter or audio proof. Sound generation is described in the next section.

Format evidence: pinned upstream `tools/emu/mcengine.py` models engine parameters as big-endian8.8 words, slots9–18 machine,pitch,color,shape,sweep,contour,punch,gate,finetune,decay. Architecture separately verified ModelTG `docs/INTERNALS.md` descriptor pitch40–88 and punch/gate0/1. `mc_encode_engine` stages only these known slots, preserving slots0–8; it is NOT the UI sound/file codec. Remaining controls use bounded0–127 values. Pitch root40–60 and variation remain inside40–88. Firmware must provide engine gain and all other untouched settings from the existing sound. Semantic note/velocity/length are translated by `sequence_encoding.c` and `firmware/preparation.c` into the verified canonical runtime format. Exact packaged scope checks pass; see `firmware/INTEGRATION-STATUS.md` for remaining release gates.

References read: DigitoneII `MEMORY.md`, native-rc17 `RC17.md`, `rhythmic_composer.hpp`, `experimental_sound.hpp`; upstream `AGENTS.md`, `notes/32-arpegiateur.md`. Behavioral ideas (seed diversity, scoped staging, density/roughness separation, returning motifs) informed this implementation. No Digitone addresses or structures were copied.

Build/check:

```sh
cc -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -g generation.c test_generation.c -o test_generation
./test_generation
```

4096 generator/path/seed cases pass with ASan/UBSan, checking fresh results, mutation masks, event bounds, independent inactive controls, density participation, invalid-input preservation and engine slot byte order. These are host synthetic tests, not exact-package boot/input/audio validation. Scratch plan allocation is caller-owned; no heap, floats or recursion are used. All validation finishes before generation writes caller-owned scratch; firmware commit coherence remains unproven. Firmware integration still requires exact-package validation.

## Sound generation (RC5 rewrite)

Patch and Kit sounds come from about 30 archetypes built on the OS 1.13 manual's description of each machine's macros. Examples are overdriven, noise, boom and click kicks; noise, crack and tom snares; out-of-range METAL hats and clangs; PERC signposts, zaps and clicks; TONE noise bursts, hats, nasty bass, feedback screams, FM bells and growls; CHORD stabs, unison smears and cracks; MACRO kick, snare, hat, noise, tonal and any-model sets; and the Sampler. Each archetype lists its machine, the paths and rhythmic roles it suits, a weight, pitch and decay ranges, a b=0 and a b=100 range for COLOR/SHAPE/SWEEP/CONTOUR and Volume+Dist, PUNCH odds, GATE odds and the LFO styles it may use. For the CHORD machine, SHAPE is a chord type and stays within 0..37.

**Seeds.** The request seed passes through a murmur3 finalizer before the xorshift stream, so neighbouring seeds are unrelated. The controller XORs in a hash of the millisecond timing of every input event. A session therefore no longer repeats the previous session's sequence of seeds after power-on, while identical event timings still reproduce identical results in tests.

**Brutality.** A seed picks its archetype, machine, pitch and decay independently of Brutality. Brutality slides each macro range towards its extreme bounds and raises Volume+Dist (distortion begins around 70), PUNCH odds and LFO depth. The number of draws never depends on Brutality. Melodic still ignores Brutality, and Density still has no effect on Patch and Kit.

**Extras** (`mc_sound_ext`, encoded by `mc_encode_extras` into sound words k1..8 and k19..22):
- LFO style, one of:
  - per-trig random sample-and-hold (RND, TRG, speed 0);
  - audio-rate modulation (speed 112..127, ×2k multiplier, which is about 0.5 kHz);
  - a one-shot envelope sweep (EXP);
  - slow drift;
  - a square-wave chop on volume, pan or decay.
- Destinations are limited to the ones a save keeps: Pitch..Contour, Fine, Decay, Volume+Dist, sends and Pan.
- Volume+Dist comes from the archetype's range, interpolated by Brutality.
- Delay and reverb sends depend on the path. Abstract almost always sends to reverb; Rhythmic sends sometimes.
- Kit spreads tracks 3..6 across the stereo field; Abstract and Free Patches are sometimes panned.

**Fixed protections.**
- Kicks (KICK, or MACRO model 34) keep PITCH 64..76, PUNCH on, GATE off and decay of at least 40 (36 on MACRO), and never get audio-rate pitch modulation.
- On the Rhythmic Kit, track 3 is the hat: its decay is at most 30 and its pitch is at least 70.
- Tracks 2..6 of the Rhythmic Kit may now use TONE.
- Sampler sounds keep their source controls and Model-TG words (k23 and up).

`generation/audio/evaluate_sounds.py` renders many generated Patch sounds through the stock voice loop and reports spectra and parameter spread. The voice loop does not include the LFO, Volume+Dist or the sends, so those are checked byte-for-byte by `validation/test_selected_sound_extras.py`. They have not been rendered.

## Sequence articulation (RC6 rewrite)

Phrase and Pattern still place their events as before. Density sets how many events there are and how many tracks play, and each path has its own placement: motif ranks, phrase order or scatter. A new articulation pass then runs on its own random stream. It never moves, adds or removes an event, so Brutality cannot change how busy a sequence is.

**Parameter-lock lanes.** Each phrase gets lanes: Abstract, Rhythmic and Free start with 2 and Melodic with 1. Density adds up to 2 more, and Brutality at 50% or above adds one more on Rhythmic and Free. The total is capped at 4, which is also the most locks a single step can hold.
- **What a lane is.** One sound parameter, chosen by path weight from: COLOR, SHAPE, SWEEP, CONTOUR, DECAY, Volume+Dist, PUNCH, PITCH, PAN, delay send, reverb send, LFO depth and LFO speed.
- **How it moves.** Every locked step gets a value from the lane's shape:
  - random per hit;
  - ramp (1–3 cycles, up or down);
  - accent (loud hits take the top value);
  - sparse spikes (more frequent with Brutality);
  - a cycle of 2–5 values that drifts against the bar.
- **Lane ranges.** Rhythmic and Free ranges widen with Brutality, up to forcing an end of the range to 0 or 127. Abstract uses wide ranges and Melodic narrow ones.
- **Per-parameter rules.**
  - Sends become throws, mostly 0 with spikes.
  - Volume+Dist rises into distortion with Brutality.
  - CHORD SHAPE stays a chord type (0–37), and MACRO SHAPE becomes a model lock (0–46).
  - SHAPE is never locked when the machine is unknown.
  - Kicks never get a PITCH lane.
  - PITCH locks keep the played pitch in key and inside 40–88. The lock is computed against the sound's own PITCH, so the note plus the lock lands on the scale.

**Lock-only steps** (trigless lock trigs, flag 0x6801). They fill some empty steps so that parameters keep moving between hits:
- Abstract: 15–45% of empty steps.
- Free: up to 25%.
- Melodic: up to 10%.
- Rhythmic: up to 8%, only at Brutality 50% or above.

**Microtiming**, from −23 to +23 (1/384 of a note):
- Abstract floats every hit off the grid.
- Rhythmic drags and jitters non-kick hits more with Brutality.
- Melodic breathes ±4 on non-kick notes.
- Free shifts half of the hits.
- Rhythmic and Melodic kicks stay on the grid.

**Retrigs:**
- Rhythmic and Free: rolls and ratchets, with fast rates (1/32–1/80) from Brutality 50%. Their odds rise with Brutality.
- Kicks roll only at Brutality 60% or above.
- Abstract: slow granular smears with long lengths and falling velocity.
- Melodic: rare short ornaments.

**Trig conditions** keep loops from repeating identically. Kicks never get one. The mix:
- probabilities: 25–81% on Abstract, 50–94% elsewhere;
- A:B cycles: 1:2, 1:3, 3:4, 1:5, 2:5, 2:6;
- occasionally FILL or !1ST;
- Free can also use any of the 64 codes.

**Save limit.** The stock saver stores at most 79 locked (track, slot) pairs per pattern. Beyond that it drops them silently, highest slots first.
- Pattern gets the whole budget.
- For Phrase, the runtime counts the pairs the other tracks use and passes that in `mc_request.locks_elsewhere`, the old padding byte. The generator stays within the rest.
- The commit refuses any sequence that would push a pattern over 79, and lets a pattern that the user already locked past 79 only shrink.
- The stager writes each lock as value<<8 in word k, the step's lock count at +66, and the slot-used flags at +4352+k. These match the stock setter and recount exactly, which `validation/test_selected_lock_persistence.py` checks by saving and reloading through the stock routines.

**Lock slot numbering.** The lock slot equals the sound word k: Pitch is slot 10. The UI lock path (`4000bb94`) and playback (`400583da`) both use the descriptor slot. Slot 24 is not Pitch: it saves as file index 1 and reloads as LFO speed. RC4's retune read slot 24; RC6 reads slot 10 and skips lock-only trigs.

## Rhythmic track 1 kick (RC7)

**Track 1 is the kick.** On the Rhythmic path track 1 always takes the kick role, for all four generators. Patch, Kit and Pattern use the full six-track layout: kick, impact, detail, response, texture, impact. Phrase forces only track 1 and still infers the other tracks' roles from their machines.

**Punch, measured.** The rules come from a sweep of the stock KICK machine through the OS 1.13 voice loop (`generation/audio/kick_sweep.py`).
- **Pitch.** At note 60 and PITCH 64 the fundamental is about 65 Hz, and each semitone moves it normally. The body holds from about 42 Hz up (low-end share above 0.95), while at 25 Hz only 5% of the energy is low end.
- **DECAY.** At 20 the kick is a click: 13.9 dB of transient over a 190 ms body. At 40 the low end is a quarter of what it is at 80. 50–80 keeps both the body and the attack.
- **SHAPE.** 25–110 keeps the low end; around 96 it thins to 0.56.
- **COLOR.** At 127 it removes the attack.

**Kick archetypes.** On Rhythmic, a stock KICK punch archetype is the most likely pick, with overdriven and noise kicks as alternatives. The click, long-boom and MACRO kicks are offered only on Abstract and Free, since Braids kick tuning has not been measured.

**`punch_kick`.** Applied to every Rhythmic track-1 kick. It draws nothing, so a seed's identity is unchanged.
- SWEEP at least 30, CONTOUR at least 20, SHAPE 25–110, COLOR at most 100.
- DECAY 50–80.
- PUNCH on, GATE off.
- Volume+Dist at least 96 + 0.31 × Brutality.
- No sends, pan centred.
- The only LFO allowed is a one-shot envelope on SWEEP or CONTOUR.

**Root inside F1–E2.** The kick sounds the kick root (the chosen root, or C) in the window F1–E2 (notes 53–64, about 44–82 Hz): C is 65 Hz, A 55 Hz, F 44 Hz and E 82 Hz.
- Patch and Kit set PITCH = kick note − the track's default note + 64.
- Phrase and Pattern give every hit on track 1 the note that sounds the kick note on the current kick sound, using its integer PITCH even when other controls are fractional, and 64 if it is unknown.
- Kicks never get a PITCH lock, a condition or a timing shift.
- RC3's rule that kicks must sound at note 60 or higher now applies only to kicks outside Rhythmic track 1. It was what pushed RC7 kicks up to about 200 Hz, into clicks.

**No zaps or sirens.**
- SWEEP on SNARE and PERC is a pitch sweep, so it is capped at 50 (at most 40–45 on toms, cracks and clicks). The zap archetype is gone.
- LFOs never move pitch or fine tune with an envelope, a drift or per-hit random steps. Pitch is modulated only at audio rate, where it becomes FM grit.
- Sequence PITCH lanes never ramp.
- `validation/test_no_zaps.c` checks all of this.

**"No choice" is kept.** The runtime passes music 0 for a pattern without a valid scale tag; the LEDs still show the A natural minor default. Generation no longer writes a tag when music is 0, so a pattern stays unchosen until the scale menu is used. Its notes still follow A natural minor, which contains C.

`validation/test_rhythmic_kick.c` checks all of these properties. `generation/audio/evaluate_kicks.py` compares kick renders between two builds.

## Pattern track selection (RC8)

Pattern plays 3 to 6 tracks.
- **How many.** Density sets the centre, from 3 at 0% to 6 at 100%, and a per-seed draw moves it by one either way. Two seeds at the same Density can therefore use different numbers of tracks.
- **Which.** Tracks join in the seed's shuffled order, so any combination is possible. On Rhythmic, track 1 (the kick) always plays first and the others come from tracks 2–6 in shuffled order.
- **Monotonic.** For a given seed, raising Density only adds tracks and events.
- **Events.** Each playing track has at least one event; Density still sets how many.
- **Roles.** Rhythmic roles stay tied to tracks: kick, impact, detail, response, texture, impact.

## RC10: no Model-TG, full volume, distortion stage

**Machines.** Machines are the stock 0–5 plus MACRO = 6 (`MC_MACRO`); the machine mask is 0x7f. The Sampler and every Sampler rule are gone. The Rhythmic Kit role masks are 0x41, 0x56, 0x5c and 0x5e.

**Volume.** Every Patch and Kit sound gets VOLUME+DIST 127, and so do all six tracks of a Pattern (`mc_plan.volume_mask`). Pattern changes only sound word k19. Sequences never lock VOLUME+DIST, and LFO chops no longer target it. Brutality no longer drives volume; on the Rhythmic kick it pushes SHAPE, COLOR and SWEEP within the measured body limits.

**Distortion record.** `mc_sound_ext.drive[8]` holds the per-track distortion stage settings (`firmware/drive.h`), written to sound +76..+83.
- **Amount by path:**

  | Path | Amount |
  |---|---|
  | Melodic | 0–35 |
  | Abstract | 20–70, plus 0.4 × Brutality |
  | Rhythmic | 30–60, plus 0.6 × Brutality |
  | Free | 1–127 |
- **Drive** rises with Brutality.
- **Fold** odds: Melodic 5%, Abstract 35%, Rhythmic 10% plus 0.3 × Brutality, Free 30%.
- **Bias** odds are 20–50%. Melodic is darker (tone 30–90).
- **Rhythmic track-1 kick:** never folded, low crossover of at least 80 so the sub bypasses the shaper, and amount at least 40 + 0.6 × Brutality.
- **Kit CPU budget:** a Kit keeps the measured stage cost of all six tracks within 6 × 30 instructions per sample. It drops fold, then bias, then low from the costliest tracks.
- **Measured cost in the packaged firmware, after the parameter glide settles:** the plain clipper costs 20.6 instructions per sample per track; a mixed kit costs 33.9. The block in which a parameter changes costs more, while the glide runs (about 60–100 ms).

## Target verification and revisions

The core now compiles freestanding for the actual ColdFire54415 target, with its own memcpy/memset and GCC integer support, and no vendor ROM dependencies. `target.ld` uses a **synthetic test address**, not a selected installation cave. `generation-target.elf` is 3436 bytes of text/data constants with no static mutable data. Isolated Unicorn CFV4E execution agrees byte-for-byte with the native host on 2048 cases (128 successive seeds at identical 30%/35% settings for every generator/path). Callee-saved registers, stack restoration, destination-only writes and invalid-request preservation pass; maximum observed stack consumption 1032 bytes. `target-report.json` records the exact ELF hash and evidence. This does not validate firmware packaging, OS input dispatch, scheduling or audio.

Request padding is explicit before the seed so host and ColdFire request layouts agree; this ABI still requires big-endian seed and tempo conversion when crossing host/target. Optional `machine_valid_mask` and `current_machine[6]` carry verified existing sound metadata. Phrase/Pattern use it to infer stock sound roles: Kick anchor, Snare impact, Metal detail, Perc response, Tone/Chord pitched. Unknown roles fall back to shuffled roles. Adapter must supply complete metadata before firmware use; it must reject machine indices outside the selected mask. Sampler is a texture role; MACRO model34 is anchor,33/36 impact,35 detail,41..46 texture and remaining models pitched. Without source model metadata MACRO falls back to pitched. Patch/Kit generate their own rotated palette. Patterns regenerate **sequences only**, preserving all sounds.

Rhythmic composition was expanded to six purposeful ranked motif families: syncopated pulse, half-time, broken rhythm, odd12-step, paired clusters and countercycles (15/12 steps for detail/response roles). Density selects a prefix of a complete ranked motif, preserving monotonic event counts. Brutality rotates non-anchor events through a bounded bijection at 40% and above, altering structure without increasing event count or moving anchor timing. Extra note disturbance and accent range are bounded. This provides variation beyond velocity and combines IDM/noise structural ideas in one path.

Rebuild target and host oracle:

```sh
m68k-elf-gcc -mcpu=54415 -Os -ffreestanding -fno-builtin -fstack-usage -Wall -Wextra -Werror -nostdlib -Wl,-T,target.ld -Wl,-Map,generation-target.map generation.c runtime.c -lgcc -o generation-target.elf
cc -std=c99 -O2 -shared generation.c -o generation-host.dylib
/Users/nzimas/DigitoneII-research/tools/emulation-venv/bin/python verify_target.py
```

Unicorn needs executable-memory access outside the local sandbox; the oracle uses only synthetic mapped RAM and locally compiled open source code. The reference Python environment is read-only; the Digitone project and release files are unchanged. ASan/UBSan tests additionally cover all 101 Density values per path, equal event counts across0/100% Brutality and verified role inference.

The final oracle also checks18 engine-encoder cases (each stock machine at pitch40/64/88). At fixed test-fixture controls (Density30%/Brutality35%, not interface defaults),128 successive seeds yield128 distinct complete outputs per generator/path. Pattern onset topology counts are Abstract128, Rhythmic126, Melodic125 and Free128; Phrase onset topology counts are128,109,32,128 respectively (Melodic additionally varies pitches and articulation). `Makefile` supplies reproducible `all`, `check` and `oracle` targets; override `TARGET_CC`/`PYTHON` for other machines.

Actual stock DSP audio emulation subsequently rendered12 sounds; see `audio/README.md` and `audio/report.json`. The initial pass exposed RMS variation and subaudio-dominant Rhythmic/Free examples. A refinement bounded Chord pitch/envelope and a repeat pass moved those examples to130Hz while retaining measurable variety. Descriptor bounds and CPU parity do **not** establish subjective musical quality, full parameter-space audibility or final-mix safety. No subjective listening or physical-device validation is claimed.

Refined Abstract profiles use machine-specific supported Color/Shape/Sweep ranges; Free explores wider bounded ranges while retaining Brutality-dependent complexity. All six stock machines occur across256 seeds in both Abstract and Free. Short Rhythmic/Free Chord sounds enforce pitch52–88 and decay28–70; Tone decay is at least18. Sequence notes use a conservative40–96 bound for broad paths and Brutality deviations, preventing the most extreme low-note combinations. Host checks cover these conditions. Original audio evidence is preserved under `audio/before-refinement`.

`mc_event.length` is an **integer duration in sequencer steps**, not the raw stock length code. The actual stock trig builder indexes the non-linear length table at4010d814 with `(code+1)*4`; the adapter must translate durations using verified timing anchors. Never copy semantic length numbers to the byte at track+192. MIDI note and velocity remain semantic integer values requiring verified stock setter/publication paths.

The core snapshots its88-byte request and writes directly into caller scratch after input validation, reducing observed ColdFire stack use from2620 to1092 bytes after extension metadata without changing generated outputs. Request/output alias regression passes. Canonical sequence staging and actual-stock reset parity are documented in `sequence_encoding.md`; publication remains integration-owned.

Phrase requests may supply `phrase_length` (1–64) from the current effective stock playback length. Zero retains the random default. Values above64 fail before output mutation; other generators ignore valid values. This occupies request byte19 and retains its position within the88-byte request ABI. Native integration must resolve global versus per-track playback length before generation; the core does not change global timing to extend a Phrase.

Selected extension support uses the verified Model-TG/MACRO mapping in `reference/Modded-Cycles/notes/43-machine-macro.md`. MACRO Shape selects47 Braids models0–46; Color is timbre, Sweep is Braids color, Contour modulates timbre from the envelope. Abstract/Free cover all47 models; Melodic chooses tonal models0–31/37–40; Rhythmic selects model families by generated role. No model outside46 is encoded. The shared envelope has a conservative minimum decay18. These are bounded controls, not a six-voice CPU deadline or listening proof; upstream model costs differ substantially.

The semantic request is88 bytes: bytes0–5 generator/path/target/density/brutality/machine mask;6 source sound validity mask;7 packed root/scale;8–11 seed;12 current machine validity mask;13–18 current machines;19 optional Phrase length;20–79 six10-byte `mc_sound` snapshots;80–85 six actual default pad notes;86 their validity mask;87 padding. Each sound’s bytes are machine,pitch,color,shape,sweep,contour,punch,gate,finetune,decay. Source snapshots must agree with valid current machines and respect bounds. Controller copies both metadata sets; runtime supplies them from canonical sound data.

Sampler generation never invents or changes a sample identifier, name or playback mode. Only an already-Sampler source can produce Sampler sound output. Its source controls Color/Shape/Sweep/Contour/Punch/Gate are copied; pitch, fine and decay vary. Because these integer snapshots cannot represent fractional8.8 Start/End, publication MUST preserve full canonical words11–16 rather than encode these copied integers over them. The adapter retains canonical name/token/mode metadata outside generated slots. Sequence role inference does not identify sample content or distinguish Slice/Loop/Granular modes; mode-aware slice selection and source classification require additional verified metadata. Current sequence generation preserves playback mode but may vary its note-dependent behavior.

Extension regression checks all47 Macro models across4096 seeds, melodic model exclusions, model-specific sequence roles, sampler source eligibility/control preservation, invalid metadata atomicity and machine8 rejection. Isolated ColdFire adds512 extension metadata cases and24 encoder comparisons; stock baseline2048 cases and64 Phrase lengths remain exact. The12 stock DSP examples remain stock-only audio evidence.

RC3 kick refinement: stock Kick0 and MACRO model34 use PITCH64–76, FINE64–68, PUNCH1, GATE0; stock decay40–85 and MACRO decay36–80. Both Patch and Kit apply these profiles on every path when the resulting sound is a kick. Active Brutality changes bounded timbre ranges while retaining these body/pitch protections; Melodic ignores Brutality. MACRO model selection is preserved. Abstract/Free retain all47 models, Melodic retains its tonal subset0–31/37–40.

Actual stock pad defaults are numeric note60 (track+712; getter40015a34). PITCH is relative to64, and FINE’s semitone offset is(control−64)/32, verified in the MACRO bridge and independent stock DSP tests. Numeric note floors should not be confused with a particular display octave label or an estimated kick fundamental. Known stock/MACRO kicks in Phrase/Pattern on all paths have note floor max(60,ceil(124−PITCH−FINEoffset)) and velocity at least96 after Brutality. Missing source metadata on a known stock kick uses conservative note86; unknown rhythmic anchor uses60. Optional source snapshots therefore preserve source tuning while preventing low-note overrides.

Offline current stock-kick comparisons in `audio/rc3-kick/` measure the20–150ms body window. These representative fixtures show stronger sustained body and eliminate the earlier10–20Hz dominant bins in the two tested generated examples; they do not establish subjective sound quality or MACRO/whole-kit mix safety. Final isolated CPU tests remain2048 base/512 extension/64 length/24 encoder comparisons, with1112B observed core stack. These new sound profiles supersede earlier low-pitch Rhythmic Kit kick profiles in prior snapshots.

RC4 root/scale support defaults to A natural minor. Request `music` byte7 is zero for that default, or128|(scale<<4)|root: root0–11 (C through B), scale0major/1naturalminor/2whole-tone/3Japanese Hirajoshi pentatonic. Bit6, unset nonzero bytes and root12–15 are rejected before output mutation. Intervals are0,2,4,5,7,9,11;0,2,3,5,7,8,10;0,2,4,6,8,10; and0,2,3,7,8 respectively. The selected Hirajoshi interval convention is supported by the Japan Council for Music Education worksheet’s A–B–C–E–F notes ([primary educational source](https://www.icme.jp/en/wp-content/uploads/2020/05/jcme08.pdf)); traditional koto tunings have related rotations and naming conventions ([Ando’s research](https://www.jstage.jst.go.jp/article/ast1980/10/5/10_5_279/_article)).

The pure `music.h,c` helpers quantize to the closest allowed note within supplied bounds, breaking ties downward. Retuning transposes by the shortest root delta (a tritone upward), then selects the nearest new-scale note. A→C therefore raises three semitones rather than resetting the octave. Existing-pattern retuning uses raw roots adjusted by the existing sound’s PITCH−64 offset to prevent double transposition. Runtime integration owns which canonical notes are retuned and preserves other fields. PITCH/fine p-locks and intentional fractional detuning remain audible exceptions to the integer base-scale mapping.

Generated sound PITCH is the nearest feasible selected-root pitch class relative to each track’s actual default pad note; absent valid pad-note metadata uses60. Newly tuned sounds use Fine64. Kick64–76, short Chord minimum52, and rhythmic Kit hat minimum70 are retained. Preflight refuses an impossible root before writing any output, such as Kick with custom pad note127 and rootA; the user can lower the normal pad note. Conservative eligibility may refuse extreme116–127 custom notes even when another random machine could otherwise be tunable. All roots are feasible in tested normal pad range36–96.

Generated sequence notes are quantized after Brutality pitch deviations and kick-floor protection, compensating captured source sound PITCH−64. Integer snapshots truncate8.8 words; fractional fine/pitch remain the documented tuning exception. Raw event notes stay40–96, and actual known kicks retain their source-aware effective note60 floor and velocity96 minimum on all paths. The pure existing-pattern retuning helper changes only its note output; integration preserves sound selection, density, durations and velocities. Generator ownership masks and density rules retain their prior scope.

RC4 checks include6144 exhaustive pure root/scale/note nearest/tie/range cases, complete generator bounds/scope regressions, all-root custom pad tuning, compensated sequence scale membership, and atomic impossible-root rejection. Isolated ColdFire checks add768 root/scale×generator/path cases and invalid musical/default-note cases. Prior stock audio reports and RC3 kick renders are historical fixtures; RC4 tuning changes generated parameters, so they do not assert current sound-parameter parity.

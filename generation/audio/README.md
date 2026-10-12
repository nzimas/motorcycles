# Synthetic stock DSP audio evidence

Twelve generated Patch sounds (three seeds 7/19/41 per path) were rendered through the **actual stock Model:Cycles OS1.13 six-voice loop**, soloing one track. The local stock input SHA is verified before extraction. Upstream `mcengine.py` runs the ColdFire instructions with its documented EMAC correction; RAM, timer/peripheral state and trigger delivery are synthetic. No instrument was queried or played.

Each mono 48000Hz WAV contains 14400 frames (300ms), one trigger at note60, and a fixed Q31-to-16bit conversion. There is **no per-file normalization**. Density30%/Brutality35% are fixed test-fixture values; they differ from the actual interface defaults. Matching stock machine defaults are used as applicable to compare raw RMS; these 12 samples happen to cover Tone,Chord,Perc only and are not exhaustive machine/parameter coverage.

All 12 outputs are nonzero, distinct raw waveforms; no unmapped accesses or signed32bit saturation samples occurred. Largest raw peak is 0.486 of Q31 full scale. RMS ranges 0.710–3.260 times the matching machine-default render. This exposes substantial level variation: bounded descriptors do not imply equal perceived loudness. Final track volume, FX, master mixing, all-six-track peaks, hardware latency and audio caches are **not** validated.

Objective spectrum evidence: Abstract centroids 125–7368Hz; Rhythmic 138–8130Hz; Melodic 176–1179Hz; Free 186–8018Hz. The Melodic examples have dominant spectral bins 115/245/570Hz and low spectral flatness, consistent with pitched material. Different seed spectra and envelope/level measurements show measurable variety. This does not establish stylistic quality, expressive phrasing or absence of organ-like timbres.

The first pass (preserved in `before-refinement/`) exposed dominant5Hz energy in Rhythmic/Free seed7Chord examples. A targeted refinement sets Chord pitch≥52 and decay≥28 for those short paths. Re-rendered examples now have dominant130Hz bins and centroids138/186Hz, demonstrating resolution for the observed case. Abstract retains longer decays and freely drawn stock machine profiles; Free now uses wider supported control ranges. Exhaustive audibility and subjective quality are still unproven. No subjective listening evaluation was performed.

`report.json` records sound parameters, raw/WAV hashes, generator-source hash, exact firmware hash, levels, spectra and validation limits. `render_examples.py` reproduces the evidence using the local input firmware, the existing validation Python environment and a local symlink to the installed m68k-elf objdump under the upstream expected command name. Running Unicorn requires executable-memory access outside the sandbox; it performs no USB/MIDI access.

Example files:

- Abstract: `abstract-seed7.wav`, `abstract-seed19.wav`, `abstract-seed41.wav`.
- Rhythmic: `rhythmic-seed7.wav`, `rhythmic-seed19.wav`, `rhythmic-seed41.wav`.
- Melodic: `melodic-seed7.wav`, `melodic-seed19.wav`, `melodic-seed41.wav`.
- Free: `free-seed7.wav`, `free-seed19.wav`, `free-seed41.wav`.

`verify_sound_parameters.py` confirms the final generator core produces exactly the12 sound parameter sets rendered here. The final core additionally raises sequence note lower bounds to40; that sequence-only refinement does not change these sound renders. The report separately hashes the source used for rendering and the final source checked for sound-parameter parity.

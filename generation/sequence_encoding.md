# Canonical sequence staging evidence

`mc_stage_sequence` transforms a caller-supplied **canonical/UI track** into caller-owned scratch. It does not find a pattern, notify observers, write the audio bank, set tempo or publish a transaction. Root integration owns canonical owner binding and commit ordering. The names/layout here come from stock UI setters and the stock initializer, not an assumption that the audio pattern bank is canonical.

Evidence from local official OS1.13 disassembly:

- `40016642` obtains track data through object vtable+40 and writes signed Note at+580; `400166c2` similarly writes Velocity+128; `40016742` Length+192; `400167bc` Micro+256. Notification uses vtable+16.
- `40016402` reads the **signed big-endian word** at+713 for track length. `40016aec` writes that word. Speed+715 is a separate byte.
- `40061480` resets the track's64 step records without touching track configuration. It sets flags0 on even steps and0x10 on odd steps, Velocity/Length-1, Note=track default+712, Micro0, Condition+516=-1, SoundLock+644=-1, RetrigLength+320=46, RetrigRate+384=9, RetrigCurve+448=0.
- The same initializer clears each of64 lock steps:33 big-endian words=-1, followed by a zero word at+66 (lock count). Step stride68. After4352 bytes,33 usage-cache bytes are cleared. Total4385.
- Actual UI lock mutation `4000dece` addresses track×4385 +step×68 +slot×2, slots0..32. `4000db6c` recomputes the count at step+66; `4000dc42` recomputes33 usage-cache bytes at track+4352.
- Trig builder `400549c2` reads Note+580 and `40054a02` reads Length+192. At `40054a18`, bit11 controls whether the note-on bit7 is explicit or inherited from track defaults+710. Staged active events use0x0881 (bit0 trig, bit7 note-on, bit11 explicit-note) plus the stock odd-step0x10 swing membership. Other bits including installed slide marks are cleared. Default envelope/LFO trigger behavior remains inherited; its undocumented names are not assumed here.

The encoder replaces every old step and clears all old locks while preserving+512..515 (including installed arp state) and+708..721, except chance+719 is reset to100 and optional nonzero phrase length writes the word+713. Fields outside the known arrays are preserved from the caller's canonical track. Zero-length phrases clear all events without changing track length, useful for inactive Pattern tracks.

`duration_codes[d]` is deliberately supplied by the adapter. `mc_event.length` means integer sequencer steps1..12; it is not a raw code. Stock trig builder `40054b92..40054bd6` looks up table4010d814 at `(code+1)*4`, with non-linear codes and127 representing infinite. The staging encoder rejects127 and invalid event lengths. Timing anchors and duration-table selection require a separate proof; the unit test's14/30/etc fixture is not presented as that proof.

Stale track chance is deliberately replaced: with per-step condition-1, the builder consults track+719 at `40054988`. Actual `400913a8` returns1 unconditionally for100 without changing RNG state, while0 always returns0. Actual setter `40016d3e` clamps to0..100 and writes+719. The encoder resets this related sequence setting to100 so a previous zero probability cannot suppress new phrases. Every other track remains outside this staging call.

Verification: `verify_sequence_stock.py` ran the **actual stock initializer** against32 varied canonical track inputs and then used the actual stock chance setter to set100 and compared all722 track bytes and4385 lock bytes to this encoder's empty replacement. They match exactly. Owner binding and actual publication remain untested in this isolated proof. `test_sequence_encoding.c` additionally checks active fields, stale-step/lock clearing, preserved arp/defaults, length word byte order, in-place track alias, invalid-input preservation and bounds under ASan/UBSan. No device access occurred.

Freestanding gcc54415 staging execution in Unicorn CFV4E matches64 complete host outputs and rejects9 unsafe alias/pointer-wrap cases without output mutation. ABI registers, stack restoration and write bounds pass; observed staging stack consumption72 bytes. The encoder rejects all input/output overlap except exact old_track==new_track before reading inputs, and guards uintptr range wrapping. See `sequence-target-report.json`. Timing codes remain an externally verified adapter input.

# Exact packaged candidate evidence plan

All tests must open the final `.syx`, verify it, and extract its own stored sections. Rebuilding a different temporary image is not candidate validation. `test_packaged_bootstrap.py` is already implemented this way and passes on the official baseline.

1. `validate_package.py STOCK --candidate CANDIDATE`: pinned official input, transport packet/content checksums, HMAC, section bounds, internal vendor version, protected sections 5/2/4 identical. Also compare the manifest's selected upstream modifications and emitted write sets, not just hashes.
2. `test_packaged_bootstrap.py CANDIDATE --out REPORT`: execute packaged section-2 decoder at0x800006bc with exact compressed section3, full byte-for-byte output, destination guards, no writes outside output/stack, return value/stack, and output before0x40200000 transit region.
3. Run actual packaged startup .data copy0x4000045c and boot detour chain, check copied runtime payload, initialized controller and reservations, untouched unrelated SRAM/registers, no bad writes. Explicitly distinguish these early startup steps from full RTOS/peripheral boot.
4. Use the final hook contract and symbol map to drive actual KeyEvent/PadEvent/encoder objects through the patched dispatch wrappers. Verify capture before stock pad consumer, consumed presses/releases/repeats,600ms timing, PAGE cancel, once per hold, knob inactivity/ordinary control preservation, LED painter ownership and restoration. Test ordinary existing gestures against candidate bytes outside interface.
5. Exercise actual publication callback and canonical sound/track objects, separate lock storage, all registered observers and audio-side mirrors. Failure injection must preserve all prior content. Whole-kit/pattern transactions and playback rejection need explicit observations.
6. Run selected upstream compatibility proofs against candidate bytes for declared supported modifications. Unknown installed configuration cannot be called preserved; stock-based candidate and configured combined builds must be labeled separately.

## Missing test inputs

Final packaged candidate, release manifest, actual hook/payload symbol map, memory reservations and publication adapter are being implemented by integration/adapter owners. Exact candidate tests above cannot report passes before these exist.

## Boot limitations

Upstream implements subsystem rigs, including bootstrap decompression, BSS/payload boot hooks, animation and6voice DSP. It provides no complete device RTOS/peripheral boot emulator. A synthetic harness can run actual early-startup routines and explicitly stub peripheral functions, but cannot certify full boot or USB recovery behavior. Protected-byte preservation and checksums remain distinct from hardware safety.

Official0x40000530 contains a4-byte PC-relative jsr; the next6-byte lea begins0x40000534. Any6-byte replacement at530 crosses that instruction and must account for it; returning at536 would land inside its immediate. Existing Model-TG rewriting must be chained according to its exact build contract.

## Publication timing finding

The current standalone `mc_fw_publish` target core was measured using the maximum64,000-byte transaction. Its final compare plus bytewise copy executes960,094 instructions between synthetic IRQ-save/restore callbacks. At an illustrative one cycle per instruction and135.168MHz this is7.10ms, compared with the upstream0.667ms audio block. This is an instruction-count observation, not cycle-accurate hardware latency or a formal cycle lower bound. See `transaction-target-report.json`. Sequencer stopped does not by itself establish that USB/audio interrupts are quiescent; the final binding must resolve this rather than treating stopped transport as proof of bounded interrupt latency.

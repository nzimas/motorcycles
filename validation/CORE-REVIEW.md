# Independent portable-core review

Host C11 builds use `-Wall -Wextra -Werror -fsanitize=address,undefined`.

`test_interface.c` passes all 17 independent checks: threshold at 599/600 ms; target capture; duplicate presses and continued holds; re-holding after release; another path's selecting hold; PAGE cancellation and LED release; reordered owned releases; generator switching; timer wrap; inactive parameters; extreme relative deltas; parameter independence; acknowledgement visibility; publication failure returning ready.

The review found acknowledgement feedback hidden behind recent knob feedback. Integration corrected this by clearing parameter feedback at publication. The suite passes after correction. Owned pad releases after PAGE and duplicate pad presses are explicitly tracked by integration.

`fuzz_interface.c` passes 1,000,000 randomized ordered events across uint32 clock wrap. It verifies state/parameter bounds, target retention, PAGE cleanup, callback preconditions, and that publication occurs only once from a valid elapsed confirmation hold. Four randomly reached publication transitions were checked; deliberate confirmation cases are covered separately by the directed suite.

`test_generation_independent.c` passes 5,040 combinations spanning all four generators, all four paths, every nonempty mask of the six stock machines, and Density 0/25/50/75/100. It checks selected-track/all-track masks, exact active-track count, cleared inactive/stale events, machine availability, event bounds, tempo scope, engine-buffer preservation, and invalid-input/invalid-engine atomicity.

These tests validate portable semantic staging code. They do not validate a firmware adapter, OS structure mutation, actual dispatch, LED hardware ownership, playback transactions, boot or audible musical quality. No candidate firmware package exists in this validation scope, and package checksum tests are not a hardware-safety proof.

Reproduce from the project directory by compiling each named validation `.c` with the corresponding `interface/interface.c` or `generation/generation.c`, with the host flags above. The logs in this directory retain test results. Toolchain/source hashes for a release should be recorded after integration stops editing.

## Standalone ColdFire interface evidence

`interface-target-build.sh` rebuilds the host and actual MCF54418 ELF using GCC 16.2.0, executes it in Unicorn CFV4E, and compares 64 directed event snapshots (4,160 bytes) exactly. Run outside the sandbox because Unicorn memory mapping requires it. The target callback publishes four requests, including one simulated failure. Observed execution: 29,747 instructions, 6,165 writes, 120 bytes maximum stack use; no writes outside the explicit state/results objects and stack. State prefix/suffix guards remain intact. See `interface-target-report.json` for hashes and evidence.

This is compiled target-core execution, with a synthetic publish callback. It does not execute actual firmware event dispatch or publish into OS pattern structures. Stack use is the maximum observed in these vectors, not a proof of every possible call path or OS interrupt nesting.

## Integrated portable controller

Independent ABI review confirms the interface's `mc_ui_request` and generation's `mc_request` have distinct names and explicit field copying. `test_controller_independent.c` passes with address/undefined sanitizers: verified current-machine metadata forwarding and Rhythmic role selection; captured target despite changing ordinary selection; seed increment rollover fallback; cancellation without seed advance or commit; invalid machine availability and live-machine metadata refusal; absent callback refusal; successful retry; settings retained across PAGE exit and reset on initialization.

The controller stages a plan and invokes a caller-provided commit callback. Preservation of real instrument content on callback failure is a callback contract; these tests do not establish OS-level atomic publication. The lead's `test_controller.c` separately covers all 16 generator/path combinations and simulated commit mutation boundaries.

## Native runtime, resolver and new publication helpers

`native-runtime-build.sh` compiles actual runtime/controller/interface/generation/resolver code for MCF54415 with synthetic publication and OS accessors. `test_native_runtime.py` passes19 checks including raw DMA rollover/fraction accumulation,599/600ms native repeats, captured target, once-per-hold/fresh seed, inactive controls, PAGE cleanup, elevated-IPL LED callback behavior and malformed machine rejection. After removing the redundant local generator plan, observed combined native stack is1,212 bytes (previous2,800). Actual firmware UI taskstack capacity/call depth still needs validation.

`test_native_binding.py` runs the official OS's `4000c5d0` canonical pattern pointer binding and actual `400157aa/bc/c8` field setters, followed by the compiled resolver. All30 canonical pointers agree; nine cases pass, including invalid aliases, indices, parents, null sounds and pointer overflow with output unchanged. The vtables/notifications are synthetic, so this does not prove actual registered audio observers.

`test_fast_blocks.py` executes assembly bulk helpers with534 callee-register-checked calls, copy sizes32/64/96/512/30,720 and every byte mismatch in512 bytes. Guards and callee-saved registers remain intact. The actual31,310-byte aligned transaction core now counts28,880 instructions in its synthetic IRQ section; one-cycle illustration0.214ms, stack128 bytes. This is not an ISR deadline proof: audio scheduling phase, full ISR workload, cache and real cycle costs remain unmodeled. Earlier timing reports are retained as evidence of optimization history, not current implementation performance.

`test_sequence_independent.c` passes17 address/undefined sanitizer checks: guards, preservation of untouched fields, exact in-place/disjoint staging, invalid aliases, events, duration codes and pointer-range overflow preserving both outputs. Source alias/range issues identified in review were corrected by the musical owner before these passes.

There is still no packaged integrated candidate in these tests. `test_packaged_bootstrap.py` and package integrity/protected-region validator are ready to validate the exact final artifact when integration supplies it. The official baseline bootstrap passes; five negative package cases also pass, including a correctly signed protected-section mutation being rejected.

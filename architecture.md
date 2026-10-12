# Model:Cycles R1 architecture evidence

Investigated locally on 2026-10-10. The architecture agent performed no instrument access. Separately, the lead used the user's express authorization for read-only identity/version queries; see `validation/device-identification.json`. No firmware deployment occurred. Modded-Cycles pinned `d520df4e3ba1dcd6724548c6ff5705b5bd8ac178`; Model-TG dependency pinned `70b39dd6787770ebefc7a2d78dea1678ec012679`. Read root, tweaks and notes AGENTS. Findings below are source evidence, not proof of the new candidate.

## Build and preservation

OS 1.13 ColdFire MCF54415 big-endian m68k. Official syx SHA256 `44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c`; MAIN OS SHA256 `cc99d4f0175d34d1e91d046e6ec85a5e8ab58ab9edbb3c24406acd48cb99ee98`. MAIN section3 base0x40000400, length1744192. Metadata5, bootstrap2 and updater4 must remain identical. Existing builder verifies official hash, each old byte, conflicts/requires, cave references, checksums/HMAC after repacking. Do not bypass unknown modified input or build over it. Modification configuration remains unidentified: upstream available options are not evidence of installed ones.

## Input boundary

Upstream notes33 and test_trig_hold.py execute the real matrix/debounce/event path. PAGE code15; steps16–31. KeyEvent+8 timestamp,+12 code,+16 flags,+20 velocity. Flag BIT indices: bit0 press (value1), bit1 FUNC (value2), bit2 double, bit3 held/repeated (value8), bit4 release (value16), bit5 long. DMA clock135168000Hz; matrix1000Hz; release debounce19ms; hold120Hz; first stock repeat24ticks200ms, repeat8ticks66.7ms, long64ticks533ms. Only most recently pressed key gets repeats. Implement an independent deadline for600ms rather than relying on flag5; PAGE release latches generator selection; a subsequent PAGE tap exits. Use wrap-safe elapsed time.

View dispatch chain0x40007f80→constructor0x4007238c→0x400060fe→ViewController handle0x40077720. Grid-only0x40022382 cannot by itself suppress track-pad auditions in other views. Model-TG already patches code accessor0x4007240c to idempotently recognize SETTINGS chords; preserve chain or hook above consumers. Track pads are separate PadEvent objects, not key codes: top dispatch0x4000614c(controller,event) adds64 then tail-jumps0x4007746c. Call sites0x400087a6 and0x400087e8 handle press/release. PadEvent+20 physical index1..6; table0x401001d0 maps these to0..5 (+0 sentinel−1); +16 state1press/0release; +24 velocity; +12 local kind1. Official constructor0x40074072 and accessor functions verified in Unicorn for all six presses/releases by adapter/test_stock_primitives.py. Therefore keys, encoders and pads need separate wrappers.

Existing step-held then PAGE preview patches0x400224ee; Model-TG includes it. Preserve outside-interface gestures and do not claim PAGE ownership on an ordinary PAGE tap. Enter only PAGE-first plus generator pad1–4; capture selected track before consumer can change it. Once active consume assigned press/release/repeat events, including PAGE release. Track capture accessor0x40012412(0x4000eb90(0x400cf866())).

Model-TG INTERNALS establishes encoder event+12 DATA1 and panel parameter knobs2–13. Relative unaccelerated clicks0x4006f73a(event,1,1); encoder predicate0x4006f6ce. Physical names now verified from official OS disassembly: encoder2 is Pitch (descriptor ID42, PTCH, slot10), encoder3 is Amp Decay (ID50, DEC, slot18). Main mapper0x4001e814 subtracts2; startup0x400e1028..0x400e103c writes IDs42/50 into the first two machine-row positions. Choose Pitch for Density, Amp Decay for Brutality. Reproducible adapter/verify_mapping.py checks exact firmware bytes/descriptors. Avoid hooking sound delta0x4000c264 because this is too late and Model-TG fine_hook already owns vtable0x400fd148.

## LED ownership

Model-TG src/model_tg.s sld_led documents grid light setter0x40005f86(lights,0x4000608c(key),mode,0x404a8cb8). First mode assigned in a frame wins; frame turns unset lights off0x40006044; playhead painted after trigs. Root view repaint0x40076c04, flag+33. Thus navigation LEDs must override at the painter boundary before stock assigns them, or use a dedicated top view with verified painter. A one-time direct LED write will be overwritten. Model-TG already owns0x40021f56 for slides and five LedHandler virtual slots for led_hook. Its sld_tick requests repaint; callback guards IPL0. Do not generate, block or write files on high-priority LED clock. Restrained blink can be on/off timed repaint; brightness semantics remain unverified. Promise bar count only, not dimming.

## Data and adapters

Keep semantic generator plan independent of firmware serialization. Upstream arp hardware history explicitly demonstrates UI track objects and audio sequence bank are different copies: writing audio0x406fa040 alone did not change the menu’s live data. UI track object0x4000cfcc(0x4000f208(0x400cf866()),track); vtable+40 returns722B block. Track object notification vtable+16 with &0x400ff5ac used by arp.

722B track: 64 flag words at+2step; velocity+128,length+192,note+580,soundlock+644, signed byte -1 means default. +512 belongs to arp, preserve. Slide bit12 belongs to Model-TG, avoid stale old flags. Track default note-enable+710 bit7; explicit per-step override bit11 makes bit7 govern note versus trigless. Per-track length713,speed715; global pattern length30662,speed30668, scale30667. Audio bank stride30710 with6×722 data followed by separate locks; locks+4332+t×4385+step×68+2word, -1 none. Sequence replacement must clear old locks, soundlocks, conditions, microtiming, retrigs and slide bits across64 steps through verified reset APIs, not merely clear note masks.

Known stock APIs: place0x40017b48(track,step,1), lock trig0x40017bb0, clear0x40017c4e; reset0x400169f0 (NOT complete deletion; resets parameter defaults and selected flags, preserves trig bit0; Model-TG adds slide bit12 reset); flag0x400179ce(track,step,mask,on); note0x40016642; plock set0x4001646a(track,step,slot,value),clear0x400164b0,get0x4001591e; length getter0x40016402. Exact remaining setter signatures and batch notification/atomic commit need proof. Stage and validate plans before mutation; whole-pattern rollback cannot rely only on722B copies because locks are separate. Playback generation remains disabled until coherent UI/audio update path is proven.

Runtime sound100B; parameters at+20+2slot (8.8 big endian). Runtime sound owner singleton+212+68track, vtable+40 per Model-TG sound_obj, verified against0x40023f0a/0x40009c1a. Name16B at+4 (serialized+12), preserve or intentionally replace. Engine params33words66B are smoothed audio copies, not canonical sound serialization. Slot9 machine,10pitch40–88 default64,11color,12shape,13sweep,14contour,15punch0/1,16gate0/1,17fine0–127 default64,18decay; color/shape/sweep/contour/decay0–127. Stock machines0Kick,1Snare,2Metal,3Perc,4Tone,5Chord. Model-TG adds Sampler6; combined Syntakt engine indices shift after Sampler; MACRO family differs. Capability manifest must derive chosen build, not pick machines from maximal catalog. Descriptor table0x4010dce0 stride56, slot+4,min+8,max+12,default+16. Read descriptor bounds for every parameter. Machine commits may reset defaults and engine state; use verified canonical sound path.

## Memory conflicts

Do not assume FF means free. Shared mask0x40154ae4 retained; redirected masks0x4015c044720B,0x4016cae81024B,0x4018a7881024B are heavily occupied by trig-hold/USB/arp/Syntakt boot. sprites.py also supports376B masks requiring explicit redirects. Check every selected tweak interval before allocation. USB descriptor first4bytes can be rewritten at runtime: unsuitable caves. Model-TG payload0x401ab750 occupies filesystem cache5×16KiB and excludes it by patching cache init; boot call0x40000530 owned. Model-TG sample range0x4a800000..0x4e800000 uncached aliases0x42800000..0x46800000; combined build ends0x4e700000 and places Syntakt payload0x46700000. Preserve existing reservations and startup chaining. New large code needs explicitly reserved combined payload/free-cache layout, not guessed0x40700000 (emulator synthetic params).

## Proof boundary

test_trig_hold.py proves scanner through grid but stubs project/trig storage/screen. mcengine.py executes actual6voice DSP32samples48kHz with EMAC emulation; Engine manually initializes/loads state, not full device boot. test_model_tg_syntakt proves payload startup and many existing hooks, not new UI. New tests must exercise exact candidate bytes and canonical observers plus audio copies, rather than writing fabricated bank state. Local audio rendering is possible via mcengine but synthetic evidence must stay distinguished from hardware.

## Additional primitive evidence and publication blockers

`adapter/verify_mapping.py` passes exact official hash, control descriptor and dispatch-byte assertions. `adapter/mapping-evidence.json` records mappings. `adapter/test_stock_primitives.py` executes the actual OS PadEvent constructor/accessors and stock batch sound function0x4001416c in Unicorn2.1.4. Batch function takes owner plus vector{begin,end,capacity}; each8B entry contains32bit slot,16bit8.8 value at+4. It writes runtimeSound+20+2slot and owner.vtable+16(owner,null) once after the batch. Ten slots9..18 were proven to change while other80B remained exact, notification occurred exactly once after all writes. Getter/notify are synthetic in that test: this proves only primitive semantics, not real observers.

Sound setter0x4001477e(owner,machine,send) writes machine+38, resets defaults via0x40014072 when changing machine, then notifies. Model-TG hooks that defaults path; blindly copying machine word or using batch0x4001416c for machine bypasses this machinery. Canonical sound assignment0x40013d88(owner,pointer,bool,bool) changes owner's data pointer+16 and calls vtable+60 for observer suppression/restoration, then notifies; it does NOT copy100B. A pointer to ephemeral generation scratch is unsafe here. Need persistent canonical storage or proven in-place copy+observer route. Whole-kit publication also needs all six sound observer behavior tested, not six unrelated writes declared atomic.

Sequence locks: trackObject+44 lock-owner pointer and+56 track index.0x4001646a forwards to0x4000dece(lockOwner,track,step,slot,value);0x400164b0 to0x4000dcec(lockOwner,track,step,slot). Some getters/clearers use inconsistent step-length comparisons (> versus >=): prevalidate0..63 explicitly. Scalar sequence setters notify per operation; no proven suppression/batch mechanism for pattern+locks identified. A722B snapshot omits locks, audio copies and notification side effects. A raw rollback after notifications is not proof of unchanged previous content. Need lock-store snapshot/reconstruction and publication observer proof before a flashable candidate; playback remains prohibited until separately demonstrated.

Reproduction: use validation/venv/bin/python for test_stock_primitives.py with user-supplied official .syx. Unicorn required execution outside sandbox to avoid host mem_map SIGILL. This grants local emulation only. Proprietary disassembly/image files are local research data (`adapter/*.local.*`) and must not enter distributable R1 artifacts.

## Staged transaction and observable dispatch (follow-up)

`adapter/firmware.h,c` now contain a concrete generic transaction: caller resolves canonical storage, stages persistent before/after regions, supplies transport and IRQ binding, and includes every affected owner. Preflight rejects null/wrapped ranges, live-region overlap, staging/live alias, excessive lengths/counts, transport-running and stale snapshots. It rechecks stopped and all snapshots under IRQ, copies all regions, restores IRQ, then invokes each real owner vtable+16 with NULL (stock sound batch uses this same event). No fallible operation follows the first write. `test_transaction.c` proves zero-write rejection for running transport/stale content/alias and that both post-IRQ notifications observe both committed regions; native and CF54415 compilation pass. This is a transaction core, not a complete firmware binding.

Actual sound notify wrapper `400d5cc8` invokes optional parent hook vtable+68 when owner+36 suppression byte is clear, then `400d5c50`: checks suppression byte+12, clones each16B registered std::function from owner+20..+24 via `400d5c1e`, invokes function+12 with canonical owner+16 data and event, destroys cloned function, then parent observable `400d0f6c`. Track wrapper `400d72be` and body `400d7246` follow the same ABI. Sound suppression `400d563c`, track `400d63ce` write bytes+12 and+36. `test_observable.py` executes the genuine sound traversal and suppression/restore with synthetic registered callbacks and parent; it proves ABI, not actual audio observer registration.

Binding corrections: `4000f208(app)` returns app+5192+732*selectedPattern (index clamped0..95); this is a pattern-owner object with six track owners beginning+112, not a raw30710B pattern. Its getter vtable+40 supplies separate global metadata (stock setter `4000c9f6` writes word+20), so never copy30710B from that getter. `4005481a()` reads OR of globals40a78874 and40a7883c; require entire result0 for strict stopped state.

Critical latency blocker: validation executes max64000B bytewise compare+copy and counts960094 instructions insideIRQ, yielding a1-cycle floor7.10ms at135.168MHz. Sequence stopped does not imply USB/audio ISR quiescence. Thus this maximum-size IRQ memcpy is NOT safe enough for shipping without smaller actual region bound or architectural change. Parent notified to evaluate persistent staged pointer publication or a UI-thread-only copy outsideIRQ; neither is asserted proven here. See `validation/transaction-target-report.json`.

## Actual sound/audio registration proof

App initialization40011186..400111c6 loops six sound owners via40009c1a and registers owner.vtable+44 callback std::function: functor{app,track}, manager4000e572, callback4000e2ec. NULL-event callback goes directly to40059614(track), clearing audio cache pointer800015a0+(147+track)*4. `test_observable.py` now executes genuine sound notification traversal, genuine manager clone/destroy, genuine callback and genuine cache invalidation for all six tracks; it proves only targeted cache invalidated and heap clone freed. Heap allocator/free and unrelated generic parent observer are stubbed. No fabricated callback replaces this audio effect; full boot registration is traced statically and ISR rendering remains separate proof.

Sound current-kit binding40009a1e uses container.vtable+48 current kit pointer, calls40013d88 to bind each owner tokit+28+100*t. ISR400590c4..400590ca chooses800017e4(currentkit)+28+100*t when no trig sound override.400590d6 compares chosen pointer against cache147+t; cache miss calls40058a0a(sound,t),40058c1c(sound,t),400583a4(t); cache hit calls40058308(t,audiostate,sound). Therefore canonical sound data can be read by ISR even with stopped sequencer; copying all600B under a bounded IRQ mask is justified over copying them outside mask, subject to final ISR timing measurements.

Correct actual track class vtable is400ff894 (constructor4001807c); +16→4001908a→400d72be, +36→400156e0→400d63a8(pointer assignment plus notification), +40→400d639e, +60→400d63ce, +68→400180d6 storage callback. Earlier400ff81c identification was16B offset into another vtable; test corrected to actual400ff894 plus actual thunk4001908a and passes. Storage hook400180d6 constructs{owner,destination,tokens} and400156e6 fetches canonical getter+40 then calls serializer4005b642. It may schedule via OSworker40068d0a depending tokens; this is not proof of synchronous audio copying.

Track audio observer registration40011298..400112dc registers, for96patterns×6tracks, functor{patternidx,trackidx,app}, manager4000e696, callback4000f192. Callback checks current pattern through4006a96c then calls40055af4(trackidx). This function reads current sequencer pattern40a7887c, global offsets30662/30667/30668 and pertrack lengths713/speeds715, then regenerates queued trig events via400548ce. This callback does not itself copy a722B track from UI to audio; the relevant bank identity or earlier parent observer still needs proof before claiming UI-copy-outside-IRQ safety.

Full runtime pattern binding4000c5d0 invokes400d2aae(owner,runtime30710), binds global owner+44 todata+30642, six track owners+112+88*t todata+722*t and lock owner+640 todata+4332. Global setter+20 thereby corresponds raw30662; the earlier warning that pattern getter was onlyglobal metadata was mistaken. Distinguish its full pattern getter from globalowner+44 getter, and runtime note+580 from serializer destination note+128. Rebinding invokes track notifications during the loop before lock owner rebinding; cannot declare a whole-pattern atomic pointer change without suppression and persistent lifetime/borrow evidence.

Transaction optimization now uses aligned32bit16B unroll plus tails, matched2byte-prefix alignment and16bit8B fallback; compile with `-fno-strict-aliasing` for byte-backed firmware storage. A preliminary snapshot comparison precedes IRQ, with final comparison retained inside mask. Validation measured aligned31310B case57018 critical instructions, 1-cycle floor0.422ms (not cycle-accurate, not USB/audio continuity proof).30710-byte stride makes odd pattern addresses2mod4, so matching staging alignment or measured16bit fallback is required; byte fallback is unsuitable for a large live pattern. See updated transaction report and preserved initial report.

## Bank identity correction and faster target routines

Do not treat runtime UI pattern and audio pattern as different layouts solely because serializer4005b642 moves note+580→note+128. That serializer writes a storage encoding, not demonstrated audio encoding. Boot40006cae supplies406fa024 to4000ea3a→404d2974; singleton400cf886 passes that pointer to appctor4001134e. Generic appctor400d3dc8 writes input directly toowner+16 (400d3e10). Appctor40011538 calls4000eac4→400d3b00 (pointer assignment)→4000ea46: binds each full runtime pattern fromappdata+28+30710*p. This yields406fa040. Pattern enqueue400083c8/4002018a/400203b4 supplies the same bank;40055c5c writes selected runtime pointer40a7887c. Hence generic runtime bindings alias the audio/sequencer bank, and fullpattern write outside IRQ requires a proven stopped event-queue quiescence condition. This corrects earlier speculative 'different copies' interpretation; Model-TG arp observation requires reconciliation with concrete bindings and installed hooks.

`adapter/fast_blocks.S` supplies CF54415 legal plain-address MOVEM32B copy/equal helpers for aligned positive32B multiples, preserving d2-d7/a2, using28B stack save (ColdFire rejects predecrement/postincrement MOVEM). Target C selects these on __m68k__, with prefix/tail fallback; link this .S source in target builds. Target compilation passes, independent emulator proofs pending. Previous C measurement is not the final optimized assembly measurement.

Machine-default staging trace:40014072 reads machine viaownergetter and40014042; collects constants1..5 plus initialized machine-row descriptor IDs40a71540 via4004df5c, reads defaults4005a65a, converts4005a4e8, writes only canonical getter data slots, thenstocknotify4001413e and local vector cleanup. Model-TG mc_commit_hook at that closingnotify touches onlysound data; stockmachines0..5 jump directly tostocknotify, Sampler6 adjustssound+72,+42..48. Therefore a suppressed cloned owner pointingpersistent soundstage can run defaults afternew machine word is set, preserving patched defaults while avoiding live mutation.4001477e additionally reads liveUI mode40013126 and canMIDI-send40082800 when sendbooltrue, so staging should invoke defaults directly aftersettingmachine. A concrete emulator staging proof still needed before marking this complete.

## Latest target results and staged defaults

Independent validation of `fast_blocks.S` passes534 direct target calls, including sizes32/64/96/512/30720,512 mismatch offsets, source/destination guards, d2-d7/a2-a6 preservation,36B helper stack. Actual transaction on31310B with matched live/staging2mod4 now28880 critical instructions and128B stack. This is an instruction bound, not a cycle deadline or proof of uninterrupted USB/audio. Mismatched2mod4 live/0mod4 staging earlier16bit path128135 instructions; bind stagingalignment to livealignment to get fast prefix. Reports: `validation/fast-blocks-report.json` and `validation/transaction-target-assembly-report.json` (verify report filenames on delivery).

`mc_fw_stage_machine` target helper now clones68B actual soundowner into aligned stack storage, validates stockgetter vtable+40==400d5632, supports machines0..5, rejects scratch alias with liveowner/live100Bdata and requires scratch byte-equal to prior live sound before any staging write. Same machine returns unchanged. Changed machine retargets clone+16 to100B stage, sets both suppression bytes12/36, writes8.8 machine word, invokes actual40014072. No live observer callback can run from suppressed cloned owner. Caller subsequently replaces generated slots10..18 and atomically publishes100B/600B sound region(s).

`test_stage_defaults.py` reproducibly compiles target C+assembly, finds exact symbol, executes actual targethelper plus actual stockdefaults. Machine page rows are synthetic initialized fixtures with valid constantstd::string sentinel and IDs42,50,46,47,48,49,44,45,43 (immutable descriptor defaults actual). All36 previous/new machine combinations pass exact100B comparison; same machine preserves everything. Three rejection tests (alias/unsupported machine/stale scratch) preserve liveowner/live100B and scratch. Fullboot descriptor-row initialization and selected-modification patched-defaults hook remain additional candidate tests.

Correct defaults interpretation: constants1..5 at400feb7c are pageINDEX positions, notglobaldescriptorIDs.40014072 reads17IDpage row and resets five page controls at these positions.4005a4e8 returns descriptorslot; defaults are already8.8 from4005a65a. Proper stockfixture therefore resets decay/color/shape/sweep/contour, while pitch/punch/gate/fine/gain remain prior until generator changes generated parameters. Real row strings may temporarily increment/decrement reference counts through400f8f02/400f7d5c; this has no retained sound/global semantic change, but source should not call the stage routine from an ISR.

Actual current sound class constructor40014588 is called by container constructor40009fe4; installs mostderivedvtable400fef54 at400145dc, +40stockgetter400d5632, +16stocknotify400d5cc8, +68storagecallback4001461c. Observable and staged-defaults tests now use this actualmostderivedvtable and pass (earlier genericfeecc tests separately valid ABI only). Afterstage helper also enforces full100B byte-equality with live prior before any scratch write; rejects stale scratch/alias/machine6 without touching live/scratch.

## Stock-reference native build (not installed-modification release)

`firmware/build-native.py` and `firmware/native.ld` compile the full native generator/interface/binding/preparation/live adapter and fast_blocks.S at401ab750. They accept only officialOS1.13 SHA-verified SysEx as read-only input; emit ELF, rawpayload, padded patchedMAIN and exact manifest; never SysEx/deploy. Sources and linker hash are checked unchanged through compilation. Compiled clearhook is checked for exact payloadstart/end skip and423380b0 clear boundary; filesystem chunkzero buffer401aa140..401ab750 must remain zero.

Current stockreference build text/data11277B, BSS64400B (__bss_start401ae360, _end401bdef0), fileend401ae35d, memorybase401ab750, retiredcachelimit401bf750. Five16KiB filesystemcache blocks retire via two verified original operands:400792f2 LRUhead401eb750→401eb7b4,400792bc prev-link start401eb774→401eb7d8. Their metadata remains outside payloadskip so stockBSSclear still initializes invalid flags to0; by-index cache scanner skips these unlinked invalid entries. Remaining11blocks available.

Manifest entry jump sites:400060fe/key8B,4000614c/pad6B,40006158/encoder8B,40005f86/lightsetter8B,4000053a/boot6B,400004d2/clear8B. Complete overwritten instruction spans verified against official bytes; six-byte JMP with NOPtail. LEDvt slots400ff9b8,400ffa78,40103070,401031b4,40107e18 all exact oldpointer400d7648noopRTS→native mc_led_hook; mc_led_original remainsstocknoop. ExistingModelTG owns these hooks and starts samepayloadbase, so this standalone reference explicitly makes no installedmod preservation claim. Root owns exact selected-modification composition and packaging.

General matchedmod4 byteprefix1..3 added to transaction equal/copy, enabling32Bbulk for odd4385B pertracklockstores. Match all staging buffers' mod4 to live addresses; no pointer swap or outsideIRQ runtimecopy introduced.

### Supplementary popup integration

Native successful publication now calls `mc_show_popup` using the stock
context-free popup ABI traced at `0x4000d750`. The wrapper is adapted from
Model-TG commit70b39dd under MIT (`firmware/LICENSE-TinyGregAudio`). Stock call
addresses: String `400f980c`, Popup ctor `400747c0`, present `4007700e`, holder
release `400cf23c`, String destroy `400f7d5c`; standard stock lifetime is retained.
It runs only after successful publication on ordinary input dispatch, never in
the interrupt-masked section or LED clock. Startup guard `40fe4178` suppresses
allocation before popup machinery exists. Six exact packaged checks prove ABI
and success/failure integration with mocked UI objects; framebuffer/RTOS boot
remains outside this proof. Current native memory bounds/hashes are in the
builder manifest, and `firmware/INTEGRATION-STATUS.md` is the current release
gate list. `firmware/REPRODUCE.md` gives the local reconstruction commands.

### Selected configuration memory correction (10 October 2026)

The selected Model-TG+MACRO combination cannot place native state at
`0x43000000`: its uncached alias `0x4b000000` is inside the Model-TG PCM
region. The selected Model-TG limit is `0x4e700000`, whose cached alias is
`0x46700000`; the final 1 MiB is excluded from PCM allocation. MACRO occupies
`0x46700000..0x4674f2f0`, including BSS: the actual startup stub at
`0x4016cae8` loads `0x13cbc` longwords for its clear, not merely its stored
segment length. Native executable and persistent state now occupy
`0x46780000..0x46792d30`. These intervals do not overlap. This resolves the
PCM alias hazard in the earlier reference placement; it does not prove
hardware cache coherency or ISR deadlines.

Private selected-machine defaults use `mc_stage_defaults_private`: the exact
original `link fp,-144` and `movem d2-d6/a2-a5,(sp)` followed by a jump to
`0x4001407a`. Calling the patched `0x40014072` on a cloned owner is unsafe:
Model-TG derives the live track index from the owner's address and writes its
legacy sampler table. The table is write-only in this pinned source
(`src/track_table.s`); current audio detects machine from track+18. The
private wrapper avoids the global write while retaining selected descriptor
lookups and the closing sampler defaults hook. Sampler source controls with
fractional 8.8 values must be preserved when generating a loaded sampler.

### Pad audition pitch correction (11 October 2026)

RC2's audition note52 came from Model-TG's *slice editor* convention (slice1),
not the normal track pad. Stock initializer40061426 writes60 to track+712.
Normal pad dispatch4001d1fc calls40015a34; its getter40015a58 reads that byte,
then4001d380 sends it to the pad core. Runtime audition now follows that exact
getter for the captured target and retains the note for matching note-off.
The default is60, while customized track notes are preserved; notes>127 reject.

Actual stock Tone DSP measurements in validation/pitch-mapping-report.json
confirm effective semitones = note + (PITCH−64) + (FINE−64)/32, before bounds.
At PITCH64/FINE64: note60→261.555Hz, note52→164.806Hz. PITCH76/note60 gives
523.292Hz. FINE0 is−2semitones (233.030Hz), FINE127 is+1.96875semitones
(293.072Hz). MACRO source macro.cc121 implements this same Q16 equation.
This verifies relative pitch in the actual stock voice loop with upstream
EMAC correction and synthetic note input; it is not a final hardware mix proof.

### Correction (RC6): lock slots

The lock store slot index is the sound word index k (sound+20+2k), so Pitch is slot 10. This was verified on the UI lock path at 4000bb30/4000bb94/4000bc00, which uses the descriptor's +4 slot, and in playback at 40058308/400583da. Table 0x4010ee4c only maps slots to file indexes for saving. Slots 23..32 are unsafe: they save as file index k−23 and reload as that slot. The stock saver 4005b95c writes at most 79 (slot, track) records, chosen by the slot-used flags at +4352+k. The per-step word at +66 counts the locks on that step. Trigless lock trig flags are 0x6801. Retrig is flag bit15, with rate at +384 (0..16), length at +320 and velocity curve at +448. Microtiming is at +256 (−23..23). The condition code is at +516 (0xFF none; 0..7 FILL/PRE/NEI/1ST and their negations; 8..28 probabilities; 29..63 A:B).

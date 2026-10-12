# Reproduce R1-rc13 locally

Use Python 3, GCC 16.2.0 with m68k-elf binutils, and Unicorn 2.1.4. The source
archive does not include vendor firmware. Sources and license notices credit
Modded Cycles, drumkilla, TinyGregAudio and Braids/Émilie Gillet.

RC13 is built on a new selected-mods baseline without Model-TG. Seven upstream
tweaks are applied to official OS 1.13 (SHA256
44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c):
`6ch-usbup, latching-mute, trig-preview, browser-scroll, trig-hold, boot-anim, macro`.
The pinned commits and hashes are in `selected-configuration.json`. The
baseline package and its MAIN hash are pinned there too. Running
`reference/Modded-Cycles/tools/check_overlaps.py` reports no shared writes
between these seven tweaks.

From the project root:

```sh
make check
python3 reference/Modded-Cycles/tools/build.py -i model-cycles_OS1.13.syx -t 6ch-usbup,latching-mute,trig-preview,browser-scroll,trig-hold,boot-anim,macro -o build/selected-notg-baseline.local.syx
python3 firmware/build-selected.py --base build/selected-notg-baseline.local.syx
export MC_STOCK_SYX=$PWD/model-cycles_OS1.13.syx
export MC_CANDIDATE="$PWD/build/selected-native/ModelCycles-generative-selected"
export MC_NATIVE_PAYLOAD="$MC_CANDIDATE.payload.bin"
python3 validation/test_selected_configuration.py --candidate "$MC_CANDIDATE.syx" --manifest "$MC_CANDIDATE.manifest.json"
python3 validation/test_packaged_bootstrap.py "$MC_CANDIDATE.syx" --out validation/selected-bootstrap-report.json
cd validation
for t in test_selected_boot test_native_live test_packaged_hooks test_native_popup test_selected_machines \
         test_patch_audition_native test_selected_music_lifecycle test_selected_retune test_selected_sound_extras \
         test_selected_lock_persistence test_selected_music_default test_selected_drive; do python3 $t.py; done
```

Use the Python interpreter that has Unicorn installed (locally,
`validation/venv/bin/python`). Alternatively, `--stock model-cycles_OS1.13.syx`
rebuilds the baseline from the official file. That gives the same MAIN; only the
compressed container bytes differ.

## Memory and boot

Standalone MACRO is machine 6. Its stub, at the start of the stock BSS clear
(`0x400004b2`), clears and fills `0x43000000..0x4304f2f0`. The native runtime
lives above it, at `0x43060000` (limit `0x430a0000`). No stock code addresses
memory between the BSS end (`0x423380b0`) and the stack at `0x48000000`
(Modded-Cycles notes/17).

The packed payload sits after the image, inside the BSS that the boot clears.
The boot call at `0x40000530` therefore becomes `jsr` to a copier in the image
tail. The copier moves the payload to `0x43060000`, then jumps to
`mc_boot_continue` inside it. That routine runs the stock BSS clear (which first
chains MACRO's copier), replays the overwritten `lea 0xfc044000,a0`, and
returns to `0x4000053a`. The LED callbacks chain the stock no-op `0x400d7648`.

## Distortion stage

- **Audio.** `jsr 0x40056f58` at `0x40059852` (the stock Volume+Dist stage) is
  replaced by `mc_drive_audio_hook`. The hook runs the stock stage, then
  `firmware/drive.c` on the six 32-sample Q31 blocks at `0x80001858`.
- **Parameters.** Each track's record is read from its current sound at
  +76..+83.
- **Persistence.** The stock savers `0x4005afa0`/`0x4005b054` and the loader
  `0x4005aecc` are wrapped to carry those 8 bytes through the serialized record.
- **UI.** FUNC + SWEEP (encoder 6) sets the selected track's amount and shows
  the overlay.

## Release status

A release is for hardware testing after these offline checks. The emulator does
not cover the full RTOS/peripheral boot, real cycle timing or audio/USB
deadlines.

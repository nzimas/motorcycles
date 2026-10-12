# Reproduce the native stock-reference fixture

This is development evidence, not R1 release firmware. It does not identify or
preserve installed Modded Cycles options. Do not transfer its SysEx to the unit.
No command below accesses a physical instrument.

Prerequisites: Python 3, Unicorn 2.1.4, a C compiler with Address/UndefinedBehavior
sanitizers, and m68k-elf GCC 16.2.0/binutils. Native target is MCF54415.
The reference builder's tool prefix defaults to `/opt/homebrew/bin/m68k-elf-`.

Obtain the pinned dependencies separately; vendor firmware is not distributed:

- Modded-Cycles: https://github.com/18nelli18/Modded-Cycles,
  commit `d520df4e3ba1dcd6724548c6ff5705b5bd8ac178`, under
  `reference/Modded-Cycles`.
- Model-TG: https://github.com/TinyGregAudio/Model-TG,
  commit `70b39dd6787770ebefc7a2d78dea1678ec012679`. Popup ABI adaptation is MIT;
  see `firmware/LICENSE-TinyGregAudio`.
- Supply your official `model-cycles_OS1.13.syx`; required SHA-256 is
  `44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c`.

Run from the project root. Select a Python interpreter with Unicorn installed:

```sh
make check
python3 firmware/build-native.py --stock-syx /absolute/path/model-cycles_OS1.13.syx
python3 firmware/package-reference.py --reference-only \
  --stock /absolute/path/model-cycles_OS1.13.syx \
  --manifest build/native-stock-reference/native-stock-reference.manifest.json \
  --payload build/native-stock-reference/native-stock-reference.payload.bin \
  --out build/native-stock-reference/native-stock-reference.syx
python3 validation/validate_package.py /absolute/path/model-cycles_OS1.13.syx \
  --candidate build/native-stock-reference/native-stock-reference.syx \
  --out validation/native-reference-package.json
python3 validation/test_packaged_bootstrap.py \
  build/native-stock-reference/native-stock-reference.syx \
  --out validation/native-reference-bootstrap.json
python3 validation/test_native_boot.py
sh validation/native-runtime-build.sh
sh validation/native-live-build.sh
MC_STOCK_SYX=/absolute/path/model-cycles_OS1.13.syx \
  MC_CANDIDATE=build/native-stock-reference/native-stock-reference \
  python3 validation/test_packaged_hooks.py
MC_STOCK_SYX=/absolute/path/model-cycles_OS1.13.syx \
  MC_CANDIDATE=build/native-stock-reference/native-stock-reference \
  python3 validation/test_native_popup.py
python3 validation/check_native_mod_footprints.py
```

The entry-hook and popup suites also run the 28 exact packaged commit checks.
The small standalone live binary is harness scaffolding; the selected package's
MAIN and native symbols supply the executed native code.
On this laptop Unicorn requires approved unsandboxed execution; the commands
remain local and do not query or transfer to hardware.

The compiler/linker manifest records source hashes, exact replaced stock spans,
payload/BSS bounds and cache retirement. Report hashes must identify the same
package after every production source change. Full peripheral/RTOS boot, true
interrupt timings and combined modification preservation remain release gates.

#!/bin/sh
set -eu
D=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$D/build"
m68k-elf-gcc -mcpu=54418 -nostdlib \
  -Wl,-Ttext=0x43000000,-e,mc_key_hook,--defsym,mc_payload_start=0x401ab750,--defsym,mc_payload_end=0x401bb750 \
  "$D/firmware/hooks.S" "$D/validation/hook-stubs.S" -o "$D/build/hooks-proof.elf"
m68k-elf-objcopy -O binary "$D/build/hooks-proof.elf" "$D/build/hooks-proof.bin"
m68k-elf-nm -n "$D/build/hooks-proof.elf" > "$D/build/hooks-proof-symbols.txt"
"$D/validation/venv/bin/python" "$D/validation/test_native_hooks.py"

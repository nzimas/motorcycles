#!/bin/sh
set -eu
D=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
m68k-elf-gcc -mcpu=54415 -Os -fno-strict-aliasing -ffreestanding -fno-builtin -fno-common -nostdlib -DREAL_LIVE -Wl,-Ttext=0x43000000,-Tdata=0x43100000,-e,mc_native_init,--defsym,mc_payload_start=0x43118000,--defsym,mc_payload_end=0x43119000 "$D/native-runtime-stubs.c" "$D/../firmware/runtime.c" "$D/../firmware/popup.S" "$D/../firmware/binding.c" "$D/../firmware/preparation.c" "$D/../firmware/live.c" "$D/../firmware/duration.c" "$D/../firmware/hooks.S" "$D/../interface/controller.c" "$D/../interface/interface.c" "$D/../generation/generation.c" "$D/../generation/sequence_encoding.c" "$D/../adapter/firmware.c" "$D/../adapter/fast_blocks.S" -lgcc -o "$D/native-live.elf"
m68k-elf-objcopy -O binary "$D/native-live.elf" "$D/native-live.bin"
m68k-elf-nm -n "$D/native-live.elf" > "$D/native-live-symbols.txt"

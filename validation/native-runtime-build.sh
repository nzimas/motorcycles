#!/bin/sh
set -eu
D=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
m68k-elf-gcc -mcpu=54415 -Os -fno-strict-aliasing -ffreestanding -fno-builtin -fno-common -nostdlib -Wl,-Ttext=0x43000000,-Tdata=0x43100000,-e,mc_native_init "$D/native-runtime-stubs.c" "$D/../firmware/runtime.c" "$D/../firmware/popup.S" "$D/../firmware/binding.c" "$D/../interface/controller.c" "$D/../interface/interface.c" "$D/../generation/generation.c" "$D/../adapter/firmware.c" "$D/../adapter/fast_blocks.S" -lgcc -o "$D/native-runtime.elf"
m68k-elf-objcopy -O binary "$D/native-runtime.elf" "$D/native-runtime.bin"
m68k-elf-nm -n "$D/native-runtime.elf" > "$D/native-runtime-symbols.txt"

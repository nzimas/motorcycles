#!/bin/sh
set -eu
D=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cc -std=c11 -Wall -Wextra -Werror -DHOST "$D/interface-target.c" "$D/../interface/interface.c" -o "$D/interface-target-host"
"$D/interface-target-host" > "$D/interface-target-host.bin"
m68k-elf-gcc -mcpu=54418 -Os -ffreestanding -fno-builtin -fno-common -nostdlib -Wl,-Ttext=0x43000000,-Tdata=0x43100000,-e,run_suite "$D/interface-target.c" "$D/../interface/interface.c" -o "$D/interface-target.elf"
m68k-elf-objcopy -O binary "$D/interface-target.elf" "$D/interface-target.bin"
m68k-elf-nm -n "$D/interface-target.elf" > "$D/interface-target-symbols.txt"
"$D/venv/bin/python" "$D/interface-target-proof.py"

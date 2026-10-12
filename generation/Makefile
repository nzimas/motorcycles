CC ?= cc
TARGET_CC ?= m68k-elf-gcc
PYTHON ?= /Users/nzimas/DigitoneII-research/tools/emulation-venv/bin/python
TARGET_FLAGS = -mcpu=54415 -Os -ffreestanding -fno-builtin -fstack-usage -Wall -Wextra -Werror -nostdlib
.PHONY: all check oracle
all: generation-target.elf generation-host.dylib test_generation test_music sequence-host.dylib test_sequence_encoding sequence-target.elf
generation-target.elf: generation.c generation.h music.c music.h runtime.c target.ld
	$(TARGET_CC) $(TARGET_FLAGS) -Wl,-T,target.ld -Wl,-Map,generation-target.map generation.c music.c runtime.c -lgcc -o $@
generation-host.dylib: generation.c generation.h music.c music.h
	$(CC) -std=c99 -O2 -shared generation.c music.c -o $@
test_generation: generation.c generation.h music.c music.h test_generation.c
	$(CC) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -g generation.c music.c test_generation.c -o $@
sequence-target.elf: sequence_encoding.c sequence_encoding.h generation.h runtime.c target.ld
	$(TARGET_CC) $(TARGET_FLAGS) -Wl,-T,target.ld -Wl,-e,mc_stage_sequence sequence_encoding.c runtime.c -lgcc -o $@
sequence-host.dylib: sequence_encoding.c sequence_encoding.h generation.h
	$(CC) -std=c99 -O2 -shared sequence_encoding.c -o $@
test_sequence_encoding: sequence_encoding.c sequence_encoding.h generation.h test_sequence_encoding.c
	$(CC) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -g sequence_encoding.c test_sequence_encoding.c -o $@
test_music: music.c music.h test_music.c
	$(CC) -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined music.c test_music.c -o $@
check: test_generation test_music test_sequence_encoding
	./test_generation
	./test_music
	./test_sequence_encoding
oracle: generation-target.elf generation-host.dylib
	$(PYTHON) verify_target.py

CC ?= cc
BUILD = build/host
CFLAGS = -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g
.PHONY: check
check: $(BUILD)/interface $(BUILD)/fuzz $(BUILD)/generation $(BUILD)/controller $(BUILD)/controller-independent $(BUILD)/rhythmic-roles $(BUILD)/patch-audition $(BUILD)/macro-reachability $(BUILD)/kick-body $(BUILD)/kick-controls $(BUILD)/music-independent $(BUILD)/music-storage-independent $(BUILD)/generation-music-context $(BUILD)/scale-ui-independent $(BUILD)/sequence-locks $(BUILD)/rhythmic-kick $(BUILD)/rhythmic-pan $(BUILD)/rhythmic-recognizable $(BUILD)/rhythmic-texture $(BUILD)/no-zaps $(BUILD)/drive $(BUILD)/djfilter $(BUILD)/lyra
	$(BUILD)/interface
	$(BUILD)/fuzz
	$(BUILD)/generation
	$(BUILD)/controller
	$(BUILD)/controller-independent
	$(BUILD)/rhythmic-roles
	$(BUILD)/patch-audition
	$(BUILD)/macro-reachability
	$(BUILD)/kick-body
	$(BUILD)/kick-controls
	$(BUILD)/music-independent
	$(BUILD)/music-storage-independent
	$(BUILD)/generation-music-context
	$(BUILD)/scale-ui-independent
	$(BUILD)/sequence-locks
	$(BUILD)/rhythmic-kick
	$(BUILD)/rhythmic-pan
	$(BUILD)/rhythmic-recognizable
	$(BUILD)/rhythmic-texture
	$(BUILD)/no-zaps
	$(BUILD)/drive
	$(BUILD)/djfilter
	$(BUILD)/lyra
$(BUILD):
	mkdir -p $@
$(BUILD)/interface: interface/interface.c interface/interface.h validation/test_interface.c | $(BUILD)
	$(CC) $(CFLAGS) interface/interface.c validation/test_interface.c -o $@
$(BUILD)/fuzz: interface/interface.c interface/interface.h validation/fuzz_interface.c | $(BUILD)
	$(CC) $(CFLAGS) interface/interface.c validation/fuzz_interface.c -o $@
$(BUILD)/generation: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_generation_independent.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_generation_independent.c -o $@
$(BUILD)/controller: interface/interface.c interface/interface.h interface/controller.c interface/controller.h generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_controller.c | $(BUILD)
	$(CC) $(CFLAGS) interface/interface.c interface/controller.c generation/generation.c generation/music.c validation/test_controller.c -o $@
$(BUILD)/controller-independent: interface/interface.c interface/interface.h interface/controller.c interface/controller.h generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_controller_independent.c | $(BUILD)
	$(CC) $(CFLAGS) interface/interface.c interface/controller.c generation/generation.c generation/music.c validation/test_controller_independent.c -o $@

$(BUILD)/rhythmic-roles: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_rhythmic_roles.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_rhythmic_roles.c -o $@
$(BUILD)/patch-audition: interface/interface.c interface/interface.h validation/test_patch_audition.c | $(BUILD)
	$(CC) $(CFLAGS) interface/interface.c validation/test_patch_audition.c -o $@

$(BUILD)/macro-reachability: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_macro_reachability.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_macro_reachability.c -o $@
$(BUILD)/kick-body: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_kick_body.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_kick_body.c -o $@

$(BUILD)/kick-controls: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_kick_controls.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_kick_controls.c -o $@

$(BUILD)/music-independent: generation/music.c generation/music.h validation/test_music_independent.c | $(BUILD)
	$(CC) $(CFLAGS) generation/music.c validation/test_music_independent.c -o $@

$(BUILD)/music-storage-independent: firmware/music_storage.c firmware/music_storage.h generation/music.c generation/music.h validation/test_music_storage_independent.c | $(BUILD)
	$(CC) $(CFLAGS) firmware/music_storage.c generation/music.c validation/test_music_storage_independent.c -o $@
$(BUILD)/generation-music-context: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_generation_music_context.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_generation_music_context.c -o $@

$(BUILD)/scale-ui-independent: interface/interface.c interface/interface.h validation/test_scale_ui_independent.c | $(BUILD)
	$(CC) $(CFLAGS) interface/interface.c validation/test_scale_ui_independent.c -o $@

$(BUILD)/sequence-locks: generation/generation.c generation/generation.h generation/music.c generation/music.h generation/sequence_encoding.c generation/sequence_encoding.h validation/test_sequence_locks.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c generation/sequence_encoding.c validation/test_sequence_locks.c -o $@

$(BUILD)/rhythmic-kick: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_rhythmic_kick.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_rhythmic_kick.c -o $@

$(BUILD)/rhythmic-pan: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_rhythmic_pan.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_rhythmic_pan.c -o $@

$(BUILD)/rhythmic-recognizable: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_rhythmic_recognizable.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_rhythmic_recognizable.c -o $@

$(BUILD)/rhythmic-texture: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_rhythmic_texture.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_rhythmic_texture.c -o $@

$(BUILD)/no-zaps: generation/generation.c generation/generation.h generation/music.c generation/music.h validation/test_no_zaps.c | $(BUILD)
	$(CC) $(CFLAGS) generation/generation.c generation/music.c validation/test_no_zaps.c -o $@

$(BUILD)/drive: firmware/drive.c firmware/drive.h validation/test_drive.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all firmware/drive.c validation/test_drive.c -lm -o $@

$(BUILD)/djfilter: firmware/djfilter.c firmware/djfilter.h validation/test_djfilter.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -fno-sanitize-recover=all firmware/djfilter.c validation/test_djfilter.c -lm -o $@

$(BUILD)/lyra: firmware/lyra.c firmware/lyra.h firmware/lyra_tables.h validation/test_lyra.c | $(BUILD)
	$(CC) $(CFLAGS) firmware/lyra.c validation/test_lyra.c -lm -o $@

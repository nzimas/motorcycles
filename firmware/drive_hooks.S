/* Distortion stage hooks (selected build, no Model-TG).
 *
 * Audio: the ISR's audio routine calls the stock Volume+Dist stage as
 *   jsr 0x40056f58  with (0x80001858 track blocks, smoothed params)
 * at 0x40059852. mc_drive_audio_hook replaces that call: it runs the stock
 * stage with the same arguments, then our per-track stage on the six
 * 32-sample Q31 blocks, in place, before pan, mixer, sends and master.
 *
 * Sound records: the stock savers (storage, runtime) at 0x4005afa0 (v2) and
 * 0x4005b054 (v1) and the loader (runtime, storage) at 0x4005aecc share the
 * prologue  lea -12(sp),sp ; movem.l a2-a4,(sp)  (8 bytes). The *_original
 * stubs replay it and continue, so C wrappers can call the stock routine.
 */
.text
.align 2
.globl mc_drive_audio_hook
mc_drive_audio_hook:
    move.l  8(%sp),-(%sp)           | params
    move.l  8(%sp),-(%sp)           | track blocks (0x80001858)
    jsr     0x40056f58              | stock Volume+Dist
    addq.l  #8,%sp
    move.l  4(%sp),-(%sp)
    jsr     mc_drive_audio
    addq.l  #4,%sp
    rts

.globl mc_sound_save2_original
mc_sound_save2_original:
    lea     -12(%sp),%sp
    movem.l %a2-%a4,(%sp)
    jmp     0x4005afa8

.globl mc_sound_save1_original
mc_sound_save1_original:
    lea     -12(%sp),%sp
    movem.l %a2-%a4,(%sp)
    jmp     0x4005b05c

.globl mc_sound_load_original
mc_sound_load_original:
    lea     -12(%sp),%sp
    movem.l %a2-%a4,(%sp)
    jmp     0x4005aed4

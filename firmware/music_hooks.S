.text
.globl mc_music_load_original
mc_music_load_original:
    lea -12(%sp),%sp
    movem.l %d2/%a2-%a3,(%sp)
    jmp 0x4005b3bc
.globl mc_music_clear_original
mc_music_clear_original:
    movea.l 4(%sp),%a0
    tst.l %a0
    jmp 0x4006152c

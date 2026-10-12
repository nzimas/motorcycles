/* Each dispatch wrapper preserves the caller's volatile registers before
 * calling C, then either returns consumed or replays whole stock instructions. */
    .text
    .globl mc_key_hook,mc_pad_hook,mc_encoder_hook,mc_light_hook,mc_led_hook
    .globl mc_boot_init_hook,mc_clear_hook,mc_irq_save,mc_irq_restore
    .macro DISPATCH name,handler
\name:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    move.l 24(%sp),-(%sp)
    move.l 24(%sp),-(%sp)
    jsr \handler
    addq.l #8,%sp
    tst.l %d0
    beq 1f
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    moveq #1,%d0
    rts
1:
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    .endm
    DISPATCH mc_key_hook,mc_key
    lea -16(%sp),%sp
    movem.l %d2-%d3/%a2-%a3,(%sp)
    jmp 0x40006106
    DISPATCH mc_pad_hook,mc_pad
    moveq #64,%d0
    add.l %d0,4(%sp)
    jmp 0x40006152
    DISPATCH mc_encoder_hook,mc_encoder
    lea -72(%sp),%sp
    movem.l %d2-%d4/%a2,(%sp)
    jmp 0x40006160
mc_light_hook:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    move.l 28(%sp),-(%sp)
    move.l 28(%sp),-(%sp)
    jsr mc_led_mode
    addq.l #8,%sp
    move.l %d0,28(%sp)
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    lea -12(%sp),%sp
    movem.l %d2-%d3/%a2,(%sp)
    jmp 0x40005f8e
mc_led_hook:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    jsr mc_led_original
    jsr mc_clock
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    rts
mc_boot_init_hook:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    jsr mc_startup
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    moveq #6,%d1
    move.l (%a0),%d0
    or.l %d1,%d0
    jmp 0x40000540
/* Replaces stock loop after d4-d7 were saved/zeroed. Preserve all original
 * BSS clearing except explicit retired cache payload; retain Syntakt prefix. */
mc_clear_hook:
1:
    cmpa.l #mc_payload_start,%a0
    bne 2f
    movea.l #mc_payload_end,%a0
2:
    cmpa.l #0x423380b0,%a0
    bcc 3f
    movem.l %d4-%d7,(%a0)
    lea 16(%a0),%a0
    bra 1b
3:
    movem.l (%sp),%d4-%d7
    lea 16(%sp),%sp
    rts
mc_irq_save:
    move.w %sr,%d0
    and.l #0xffff,%d0
    move.w #0x2700,%sr
    rts
mc_irq_restore:
    move.l 4(%sp),%d0
    move.w %d0,%sr
    rts

/* Selected build boot: the copier in the image tail has already moved this
 * payload above MACRO (the tail lies inside the BSS the stock clear zeroes).
 * Run the stock BSS clear, which first chains standalone MACRO's copier,
 * replay the lea the boot jsr overwrote, and resume after it (0x4000053a). */
.globl mc_boot_continue
mc_boot_continue:
    jsr 0x400004b2
    lea 0xfc044000,%a0
    addq.l #4,(%sp)
    rts

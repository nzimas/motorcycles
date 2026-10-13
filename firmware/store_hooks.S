/* Store-a-break wrappers (firmware/store.c), in cave A0 (adapter/cave.h).
 * Each runs its C part with the caller's volatile registers preserved, then
 * continues into RC19's own wrapper, which is unchanged:
 *   key   0x400060fe: mc_store_key consumes, or on to mc_key_hook
 *   light 0x40005f86: mc_store_light may replace the mode (third argument),
 *                     then on to mc_light_hook (mc_led_mode keeps it outside
 *                     generator mode)
 *   LED vtable slots: mc_store_tick, then on to mc_led_hook */
    .section .cave0,"ax"
    .align 2
    .globl mc_store_key_hook,mc_store_light_hook,mc_store_led_hook
mc_store_key_hook:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    move.l 24(%sp),-(%sp)
    move.l 24(%sp),-(%sp)
    jsr mc_store_key
    addq.l #8,%sp
    tst.l %d0
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    beq 1f
    moveq #1,%d0
    rts
1:  jmp mc_key_hook
mc_store_light_hook:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    move.l 28(%sp),-(%sp)
    move.l 28(%sp),-(%sp)
    jsr mc_store_light
    addq.l #8,%sp
    move.l %d0,28(%sp)
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    jmp mc_light_hook
mc_store_led_hook:
    lea -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    jsr mc_store_tick
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea 16(%sp),%sp
    jmp mc_led_hook

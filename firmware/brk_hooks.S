/* Live break hooks (firmware/brk.c) on the stock trig builder 0x400548ce.
 * Entry: its first 6 bytes (lea -60(sp),sp ; moveq #1,d1) become jsr
 * mc_brk_entry_hook. On the sequencer path (reuse == 0, out != 0) the step
 * argument is replaced by mc_brk_step(track, pattern, step); then the two
 * instructions are replayed and the builder continues (a0/d0/d1 are dead).
 * Exit: 0x40054c0e (move.l a6,72(a2) ; move.l a2,d0) becomes jsr
 * mc_brk_exit_hook, which replays them and lets mc_brk_msg reshape the
 * message. Inside the builder the arguments sit at +64 (track) .. +84 (out). */
.text
.align 2
.globl mc_brk_entry_hook,mc_brk_exit_hook
mc_brk_entry_hook:
    tst.l   24(%sp)                 | reuse
    bne     1f
    tst.l   28(%sp)                 | out
    beq     1f
    lea     -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    move.l  36(%sp),-(%sp)          | step
    move.l  32(%sp),-(%sp)          | pattern
    move.l  32(%sp),-(%sp)          | track
    jsr     mc_brk_step
    lea     12(%sp),%sp
    move.l  %d0,36(%sp)             | remapped step
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea     16(%sp),%sp
1:  movea.l (%sp)+,%a0
    lea     -60(%sp),%sp
    moveq   #1,%d1
    jmp     (%a0)

mc_brk_exit_hook:
    move.l  %a6,72(%a2)
    lea     -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    tst.l   100(%sp)                | reuse (builder +80, +4 return, +16 saves)
    bne     2f
    tst.l   104(%sp)                | out
    beq     2f
    move.l  %a2,-(%sp)
    jsr     mc_brk_msg
    addq.l  #4,%sp
2:  movem.l (%sp),%d0-%d1/%a0-%a1
    lea     16(%sp),%sp
    move.l  %a2,%d0
    rts

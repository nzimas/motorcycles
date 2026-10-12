/* Control All hooks (firmware/ctrlall.c).
 *
 * 0x40059534 is the audio dispatcher's knob-edit handler, entered as
 *   handler(value, track, k)  (cdecl; 0x4000e348 / 0x4000e3e2 tail-jump to it)
 * and writes the cache word of (track, k). Its first three instructions
 *   move.l a2,-(sp) ; move.l d2,-(sp) ; moveq #6,d2   (6 bytes)
 * are replaced by a jmp here: record the edit, then replay them and continue.
 * The lock applier 0x400583da is reached through jsr sites that the build
 * retargets straight at mc_ca_locks (same C signature), so it needs no stub.
 */
.text
.align 2
.globl mc_param_edit_hook
mc_param_edit_hook:
    lea     -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    move.l  28(%sp),-(%sp)          | k
    move.l  28(%sp),-(%sp)          | track
    move.l  28(%sp),-(%sp)          | value
    jsr     mc_ca_param
    lea     12(%sp),%sp
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea     16(%sp),%sp
    move.l  %a2,-(%sp)
    move.l  %d2,-(%sp)
    moveq   #6,%d2
    jmp     0x4005953a

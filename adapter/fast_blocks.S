/* ColdFire MCF54415; pointers four-byte aligned, bytes a positive multiple32.
 * Preserve SysV d2-d7/a2; a0,a1,d0,d1 caller-saved. No postincrement MOVEM:
 * ColdFire permits plain (An) memory operands. */
.text
.align 2
.globl mc_fw_equal_blocks
mc_fw_equal_blocks:
 lea -28(%sp),%sp
 movem.l %d2-%d7/%a2,(%sp)
 move.l 32(%sp),%a0
 move.l 36(%sp),%a1
 move.l 40(%sp),%a2
1:
 movem.l (%a0),%d0-%d7
 cmp.l (%a1),%d0
 bne 3f
 cmp.l 4(%a1),%d1
 bne 3f
 cmp.l 8(%a1),%d2
 bne 3f
 cmp.l 12(%a1),%d3
 bne 3f
 cmp.l 16(%a1),%d4
 bne 3f
 cmp.l 20(%a1),%d5
 bne 3f
 cmp.l 24(%a1),%d6
 bne 3f
 cmp.l 28(%a1),%d7
 bne 3f
 lea 32(%a0),%a0
 lea 32(%a1),%a1
 suba.l #32,%a2
 cmpa.l #0,%a2
 bne 1b
 moveq #1,%d0
 bra 4f
3:
 moveq #0,%d0
4:
 movem.l (%sp),%d2-%d7/%a2
 lea 28(%sp),%sp
 rts
.globl mc_fw_copy_blocks
mc_fw_copy_blocks:
 lea -28(%sp),%sp
 movem.l %d2-%d7/%a2,(%sp)
 move.l 32(%sp),%a0
 move.l 36(%sp),%a1
 move.l 40(%sp),%a2
1:
 movem.l (%a1),%d0-%d7
 movem.l %d0-%d7,(%a0)
 lea 32(%a0),%a0
 lea 32(%a1),%a1
 suba.l #32,%a2
 cmpa.l #0,%a2
 bne 1b
 movem.l (%sp),%d2-%d7/%a2
 lea 28(%sp),%sp
 rts

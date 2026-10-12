.text
.globl mc_key,mc_pad,mc_encoder,mc_led_mode,mc_led_original,mc_clock,mc_native_init,mc_startup
mc_key:
mc_pad:
mc_encoder:
 move.l 4(%sp),%d0
 move.l %d0,0x43100000
 move.l 8(%sp),%d0
 move.l %d0,0x43100004
 move.l 0x43100008,%d0
 rts
mc_led_mode:
 move.l 4(%sp),%d0
 move.l %d0,0x43100000
 move.l 8(%sp),%d0
 move.l %d0,0x43100004
 moveq #2,%d0
 rts
mc_led_original:
mc_clock:
mc_native_init:
mc_startup:
 rts

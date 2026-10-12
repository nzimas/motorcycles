OUTPUT_ARCH(m68k)
ENTRY(mc_boot_init_hook)
SECTIONS {
 . = 0x401ab750;
 mc_payload_start = .;
 .text : { *(.text .text.*) }
 .rodata : { *(.rodata .rodata.*) }
 .data : { *(.data .data.*) }
 . = ALIGN(16);
 __bss_start = .;
 .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
 . = ALIGN(16);
 _end = .;
 mc_payload_end = .;
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
 ASSERT(mc_payload_end <= 0x401c3750, "native payload exceeds six retired16KiB cache blocks")
}
mc_led_original = 0x400d7648;

ENTRY(mc_generate)
SECTIONS { . = 0x20000000; .text : { *(.text*) } .rodata : { *(.rodata*) } .data : { *(.data*) } .bss : { *(.bss*) *(COMMON) } }

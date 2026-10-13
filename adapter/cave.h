#ifndef MC_CAVE_H
#define MC_CAVE_H
/* Code placed in dead stock code (firmware/build-selected.py CAVES): the stock
 * CHORD engine and its helpers, unreferenced since LYRA replaced CHORD. These
 * live in the OS image itself, so they do not count against the proven
 * boot-copy limit of the payload. UI-context code only: nothing audio-critical.
 * On host builds the attributes vanish. */
#if defined(__m68k__)
#define MC_CAVE_A __attribute__((section(".cave"),noinline))
#define MC_CAVE_A_DATA __attribute__((section(".cave.rodata")))
#define MC_CAVE_B __attribute__((section(".cave2"),noinline))
#define MC_CAVE_C __attribute__((section(".cave3"),noinline))
#else
#define MC_CAVE_A
#define MC_CAVE_A_DATA
#define MC_CAVE_B
#define MC_CAVE_C
#endif
#endif

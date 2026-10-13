#ifndef MC_CAVE_H
#define MC_CAVE_H
/* Code placed in dead stock code (firmware/build-selected.py CAVES): the stock
 * CHORD engine, its helpers and its chord table, unreferenced since LYRA
 * replaced CHORD. These live in the OS image itself, so they neither count
 * against the proven boot-copy limit nor move any payload code. UI-context
 * code only: nothing audio-critical. On host builds the attributes vanish.
 *   A0 0x400AADA4 (228 B)    the small hooks called on every LED query
 *   B  0x400A9A32 (630 B)    code called on key events and LED passes
 *   C  0x40120650 (5,180 B)  rarely called code and its data (the CHORD
 *                            tables, in the OS's read-only data)
 * (Cave A, 0x400AAE88, holds RC19's overlay drawing: firmware/drive_ui.c.) */
#if defined(__m68k__)
#define MC_CAVE_A0 __attribute__((section(".cave0"),noinline))
#define MC_CAVE_B __attribute__((section(".cave2"),noinline))
#define MC_CAVE_C __attribute__((section(".cave3"),noinline))
#define MC_CAVE_C_DATA __attribute__((section(".cave3.rodata")))
#else
#define MC_CAVE_A0
#define MC_CAVE_B
#define MC_CAVE_C
#define MC_CAVE_C_DATA
#endif
#endif

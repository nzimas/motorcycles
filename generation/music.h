#ifndef MC_MUSIC_H
#define MC_MUSIC_H
#include <stdint.h>
enum mc_scale { MC_MAJOR,MC_NATURAL_MINOR,MC_WHOLE_TONE,MC_HIRAJOSHI };
/* Byte0/unset is A natural minor. Explicit value: validbit7, scale4..5,
   root bits0..3 (C=0, valid values0..11). Bit6 and roots12..15 are invalid. */
int mc_music_unpack(uint8_t packed,uint8_t *root,uint8_t *scale);
int mc_music_pack(unsigned root,unsigned scale,uint8_t *packed);
int mc_note_in_scale(unsigned note,unsigned root,unsigned scale);
/* Nearest valid note, ties lower, preserving the supplied octave/range.
   Invalid arguments leave the output byte untouched. */
int mc_quantize_note(unsigned note,unsigned root,unsigned scale,unsigned lower,unsigned upper,uint8_t *out);
/* Shortest root transposition (tritone upward), then nearest new scale. */
int mc_retune_note(unsigned note,unsigned old_root,unsigned new_root,unsigned new_scale,unsigned lower,unsigned upper,uint8_t *out);
#endif

#ifndef MC_MUSIC_STORAGE_H
#define MC_MUSIC_STORAGE_H
#include <stdint.h>
#define MC_MUSIC_OFFSET 40
#define MC_MUSIC_BYTES 8
extern volatile uint32_t mc_music_context_stamp;
int mc_music_owned(const uint8_t *global);
uint8_t mc_music_read(const uint8_t *global);
int mc_music_write(uint8_t *global,uint8_t music);
int mc_music_load_hook(uint8_t *,const uint8_t *);
int mc_music_clear_hook(uint8_t *,unsigned);
int mc_music_current_owned(void);
#endif

#ifndef MC_NATIVE_BINDING_H
#define MC_NATIVE_BINDING_H
#include <stdint.h>
#define MC_UI_PATTERN_BYTES 30710u
#define MC_UI_TRACK_BYTES 722u
#define MC_UI_LOCK_BYTES 4385u
/* Canonical runtime layout; compact serialized storage uses a different order. */
typedef struct {
    void *pattern_owner,*global_owner,*lock_owner;
    void *track_owner[6],*sound_owner[6];
    uint8_t *pattern,*global,*locks,*track[6],*sound[6];
} mc_binding;
typedef void *(*mc_data_getter)(void *);
/* Resolves but does not mutate. Rejects inconsistent canonical aliases. */
int mc_binding_resolve(mc_binding *,uint8_t *root,void *pattern_owner,mc_data_getter);
#endif

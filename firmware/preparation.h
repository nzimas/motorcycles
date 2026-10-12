#ifndef MC_NATIVE_PREPARATION_H
#define MC_NATIVE_PREPARATION_H
#include "binding.h"
#include "../generation/generation.h"
#include "../adapter/firmware.h"
typedef struct {
    uint8_t pattern_before[MC_UI_PATTERN_BYTES+4] __attribute__((aligned(4)));
    uint8_t pattern_after[MC_UI_PATTERN_BYTES+4] __attribute__((aligned(4)));
    uint8_t sound_before[6][104] __attribute__((aligned(4)));
    uint8_t sound_after[6][104] __attribute__((aligned(4)));
} mc_snapshots;
typedef int (*mc_stage_machine)(void *,uint8_t *,uint8_t);
/* Stages canonical bytes only. Caller still owns serialized input, stopped
 * transport, final snapshot revalidation, publication and notification. */
int mc_prepare(mc_binding *,const mc_request *,const mc_plan *,mc_snapshots *,
               const uint32_t durations[128],mc_stage_machine,
               struct mc_fw_transaction *);
#endif

#ifndef MC_CONTROLLER_H
#define MC_CONTROLLER_H
#include "interface.h"
#include "../generation/generation.h"
/* Platform commit must validate every write, reject stale live state and publish
 * all affected data atomically. This prototype supplies no firmware commit. */
typedef int (*mc_commit_plan)(void *, const mc_plan *, const mc_request *);
typedef struct {
    mc_interface ui;
    mc_plan scratch;
    uint32_t seed;
    mc_sound current_sound[MC_TRACKS];
    uint8_t source_sound_valid_mask, machine_mask, machine_valid_mask, current_machine[MC_TRACKS], phrase_length;
    mc_commit_plan commit;
    void *context;
    uint8_t music,default_note[MC_TRACKS],note_valid_mask;
    int (*retune)(void *,uint8_t,uint8_t);
    /* Millisecond timing of every input event, folded in so that each power-on
     * session draws different seeds. */
    uint32_t entropy;
    /* Locked (track, slot) pairs per track in the current pattern, for Phrase budgets. */
    uint8_t lock_pairs[MC_TRACKS];
} mc_controller;
void mc_controller_init(mc_controller *,uint32_t,uint8_t,mc_commit_plan,void *);
int mc_controller_event(mc_controller *,unsigned,unsigned,int,unsigned,uint32_t);
#endif

#ifndef MC_INTERFACE_H
#define MC_INTERFACE_H
#include <stdint.h>
enum mc_state { MC_INACTIVE, MC_GENERATOR, MC_READY, MC_HOLD, MC_GENERATING, MC_SCALE };
enum mc_event { MC_PAGE, MC_PAD, MC_STEP, MC_DENSITY, MC_BRUTALITY, MC_TICK };
typedef struct {
    uint8_t generator, path, target, density, brutality, music;
} mc_ui_request;
/* Firmware adapter must stage and validate the whole transaction before committing.
 * Return 1 only after successful publication; failure must leave content unchanged. */
typedef int (*mc_publish)(void *, const mc_ui_request *);
typedef struct {
    uint8_t state, page, generator, path, target, held;
    uint8_t down, pad_down, armed, fired, feedback, acknowledgement;
    uint8_t density[4][4], brutality[4][4];
    uint32_t since, feedback_until, acknowledgement_until;
    uint8_t page_exit, generated, audition, music;
    uint16_t scale_down;
    /* Level 1..15 per generator (steps 1..15); step 16 is the generate key. */
    uint8_t level[4];
    uint16_t level_down;
} mc_interface;
/* Hold step 16 this long to generate; a shorter tap after a successful Patch
   auditions it. */
#define MC_GENERATE_HOLD_MS 1500u
#define MC_AUDITION_TAP_MS 600u
#define MC_LEVEL_DEFAULT 7
/* Level 1..15 -> 0..100, the generators' Density / Brutality scale. */
static inline uint8_t mc_level_amount(unsigned level){return (uint8_t)(level<1?0:level>15?100:(level-1)*100/14);}
void mc_init(mc_interface *);
/* now is monotonic milliseconds supplied by a verified platform clock.
 * PAD/STEP index is zero based; value is press=1/release=0. Knobs use signed deltas.
 * Returns whether the adapter must consume this event before ordinary dispatch. */
int mc_event_handle(mc_interface *, unsigned event, unsigned index, int value,
                    unsigned selected_track, uint32_t now, mc_publish, void *);
uint16_t mc_leds(const mc_interface *, uint32_t now);
#endif

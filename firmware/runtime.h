#ifndef MC_NATIVE_RUNTIME_H
#define MC_NATIVE_RUNTIME_H
#include <stdint.h>
#include "../interface/controller.h"
extern mc_controller mc_native;
extern uint32_t mc_led_previous;
void mc_native_init(void);
void mc_startup(void);
int mc_key(void *,const uint8_t *);
int mc_pad(void *,const uint8_t *);
int mc_encoder(void *,const uint8_t *);
void mc_clock(void);
unsigned mc_led_mode(unsigned,unsigned);
uint32_t mc_now(void);
int mc_commit_live(void *,const mc_plan *,const mc_request *);
void mc_read_machines(void);
int mc_retune_live(void *,uint8_t,uint8_t);
extern void mc_led_original(void);
#endif

#include "../firmware/runtime.h"
void *memcpy(void*d,const void*s,unsigned long n){uint8_t*a=d;const uint8_t*b=s;while(n--)*a++=*b++;return d;}
void *memset(void*d,int v,unsigned long n){uint8_t*a=d;while(n--)*a++=(uint8_t)v;return d;}
volatile uint32_t publications,last_seed,last_target,last_generator,last_path,last_sound_mask,last_sequence_mask;
#ifndef REAL_LIVE
int mc_commit_live(void *ctx,const mc_plan*p,const mc_request*r){(void)ctx;publications++;last_seed=r->seed;last_target=r->target;last_generator=r->generator;last_path=r->path;last_sound_mask=p->sound_mask;last_sequence_mask=p->sequence_mask;return 1;}
#endif
void mc_led_original(void){}
uint32_t native_seed(void){return mc_native.seed;}

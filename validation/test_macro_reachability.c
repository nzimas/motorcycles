#include "../generation/generation.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void){mc_request q={.target=4,.density=65,.brutality=55,.machine_mask=255,.machine_valid_mask=63};mc_plan p;unsigned cases=0;
 for(unsigned g=MC_PATCH;g<=MC_KIT;g++)for(unsigned path=0;path<4;path++){
  q.generator=g;q.path=path;unsigned macro=0;uint64_t models=0;
  for(unsigned seed=1;seed<=8192;seed++){q.seed=seed;assert(mc_generate(&q,&p));assert(p.sound_mask==(g==MC_PATCH?16:63)&&!p.sequence_mask);for(unsigned t=0;t<6;t++)if(p.sound_mask&(1u<<t)){assert(p.sound[t].machine<MC_MACHINES);if(p.sound[t].machine==MC_MACRO){macro++;assert(p.sound[t].shape<=46);models|=UINT64_C(1)<<p.sound[t].shape;}}cases++;}
  assert(macro>0);
  if(path==MC_ABSTRACT||path==MC_FREE)assert(models==((UINT64_C(1)<<47)-1));
  if(path==MC_MELODIC){assert(!(models&(UINT64_C(15)<<33)));for(unsigned m=0;m<47;m++)if(m<32||(m>=37&&m<=40))assert(models&(UINT64_C(1)<<m));}
 }
 /* Restricting capability to MACRO must still produce valid models for every path. */
 q.machine_mask=MC_MACRO_BIT;q.machine_valid_mask=0;
 for(unsigned path=0;path<4;path++){q.path=path;q.generator=MC_KIT;q.seed=71+path;assert(mc_generate(&q,&p));for(unsigned t=0;t<6;t++)assert(p.sound[t].machine==MC_MACRO);cases++;}
 printf("PASS %u Patch/Kit MACRO eligibility and all47 model reachability cases\n",cases);return 0;}

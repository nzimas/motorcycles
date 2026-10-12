#include "../generation/generation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int kick(const mc_sound *s){return s->machine==0||(s->machine==MC_MACRO&&s->shape==34);}
int main(void){mc_request q={.target=0,.density=80,.brutality=100,.machine_mask=255};mc_plan p;unsigned cases=0,kicks=0;
 for(unsigned g=0;g<2;g++)for(unsigned path=0;path<4;path++)for(unsigned seed=1;seed<=4096;seed++){
 q.generator=g;q.path=path;q.seed=seed;assert(mc_generate(&q,&p));
 for(unsigned t=0;t<6;t++)if((p.sound_mask&(1u<<t))&&kick(&p.sound[t])){const mc_sound *s=&p.sound[t];assert(s->pitch>=64&&s->pitch<=76&&s->finetune>=64&&s->finetune<=68&&s->punch==1&&!s->gate);/* Rhythmic track 1 kicks take a character (short to long); other kicks keep a sustained body. */if(path==MC_RHYTHMIC&&t==0)assert(s->decay>=(s->machine==0?2:22)&&s->decay<=(s->machine==0?40:60));else assert(s->decay>=(s->machine==0?40:36));kicks++;}cases++;
 }
 assert(kicks>100);
 q.machine_valid_mask=q.source_sound_valid_mask=63;
 for(unsigned machine=0;machine<=MC_MACRO;machine+=MC_MACRO)for(unsigned pitch=40;pitch<=88;pitch++)for(unsigned fine=0;fine<=127;fine++){
 for(unsigned t=0;t<6;t++){q.current_machine[t]=machine;q.current_sound[t]=(mc_sound){.machine=machine,.pitch=pitch,.shape=machine==MC_MACRO?34:50,.decay=60,.finetune=fine};}
 for(unsigned g=2;g<4;g++)for(unsigned path=0;path<4;path++){q.generator=g;q.path=path;q.seed=113+pitch+fine+g+path;assert(mc_generate(&q,&p));for(unsigned t=0;t<6;t++)for(unsigned i=0;i<64;i++)if(p.phrase[t].step[i].on){mc_event *e=&p.phrase[t].step[i];assert(e->velocity>=96);/* Rhythmic kick: C (no scale chosen) inside F1..E2; other kicks keep the note-60 floor. */if(path==MC_RHYTHMIC&&t==0){assert((int)pitch-64+(int)e->note==60);}else{assert(e->note>=60);assert(((int)pitch-64+(int)e->note)*32+(int)fine-64>=60*32);}}cases++;}
 }
 /* Known stock kick without captured controls uses a conservative floor. */
 q.source_sound_valid_mask=0;memset(q.current_machine,0,6);
 for(unsigned g=2;g<4;g++)for(unsigned path=0;path<4;path++){q.generator=g;q.path=path;q.seed=61;assert(mc_generate(&q,&p));for(unsigned t=0;t<6;t++)for(unsigned i=0;i<64;i++)if(p.phrase[t].step[i].on){if(path==MC_RHYTHMIC&&t==0){int known=q.current_sound[0].pitch>=40&&q.current_sound[0].pitch<=88?q.current_sound[0].pitch:64;assert((int)p.phrase[t].step[i].note+known-64==60&&p.phrase[t].step[i].velocity>=96);}else{assert(p.phrase[t].step[i].note>=86&&p.phrase[t].step[i].velocity>=96);}}cases++;}
 /* Invalid captured pitch remains an atomic rejection, not a guessed default. */
 q.source_sound_valid_mask=1;q.current_sound[0].machine=0;q.current_sound[0].pitch=39;mc_plan old;memset(&p,0xa5,sizeof p);old=p;assert(!mc_generate(&q,&p)&&!memcmp(&p,&old,sizeof p));cases++;
 /* A tonal non-kick retains the ordinary velocity range rather than receiving
    kick-specific lifting; demonstrated for each path with known metadata. */
 q.source_sound_valid_mask=0;memset(q.current_machine,4,6);q.generator=MC_PHRASE;q.target=1; /* Rhythmic track 1 is always the kick */
 for(unsigned path=0;path<4;path++){unsigned ordinary=0;q.path=path;for(unsigned seed=1;seed<=64;seed++){q.seed=seed;assert(mc_generate(&q,&p));for(unsigned i=0;i<64;i++)if(p.phrase[1].step[i].on){assert(p.phrase[1].step[i].note<=96&&p.phrase[1].step[i].velocity<=127);ordinary+=p.phrase[1].step[i].velocity<96;}cases++;}assert(ordinary);}
 printf("PASS %u kick sound/body and known kick sequence pitch/velocity cases; %u generated kicks\n",cases,kicks);return 0;}

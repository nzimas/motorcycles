/* Rhythmic track 1 is the kick: punchy sound shaping on Patch/Kit, tuned to
   the kick root, and Phrase/Pattern hits that all sound that root (the chosen
   root, or C when no scale was chosen). */
#include "../generation/generation.h"
#include "../generation/music.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int kick(const mc_sound *s){return s->machine==0||(s->machine==MC_MACRO&&s->shape==34);}
/* Root inside F1..E2: where the stock KICK keeps its low-end body. */
static int kick_note(unsigned root){return 53+(int)((root+7)%12);}
int main(void){mc_plan p;unsigned cases=0,kicks=0,hits=0,short_kicks=0,long_kicks=0,tonal=0,chirp=0,noisy=0,light=0,heavy=0;
 /* Sound: Patch on track 1 and Kit, every Brutality and scale choice. */
 for(unsigned g=MC_PATCH;g<=MC_KIT;g++)for(unsigned b=0;b<=100;b+=25)for(unsigned music=0;music<=48;music++){
  mc_request q={.generator=g,.path=MC_RHYTHMIC,.target=0,.brutality=b,.machine_mask=255,.note_valid_mask=1};
  uint8_t root,scale;if(music==48)q.music=0;else assert(mc_music_pack(music%12,music/12,&q.music));
  assert(mc_music_unpack(q.music,&root,&scale));unsigned kick_root=q.music?root:0;
  for(unsigned seed=1;seed<=40;seed++){
   q.seed=seed*2246822519u;q.default_note[0]=(uint8_t)(48+seed%25);assert(mc_generate(&q,&p));
   const mc_sound *s=&p.sound[0];const mc_sound_ext *e=&p.extra[0];
   if(!kick(s))continue;kicks++;
   assert(s->machine==0&&s->punch==1&&!s->gate&&s->decay>=2&&s->decay<=40);
   assert(s->shape<=110&&s->color<=127&&s->sweep<=127&&e->drive[0]>=12);
   /* A clean character never crosses into the feedback-noise region. */
   assert(s->shape<=74||s->shape>=80);
   short_kicks+=s->decay<=8;long_kicks+=s->decay>=22;tonal+=s->sweep<=35;chirp+=s->sweep>=85;noisy+=s->shape>=80;
   light+=e->drive[0]<50;heavy+=e->drive[0]>=110;
   assert(e->volume>=96+b*31/100&&!e->delay&&!e->reverb&&e->pan==64);
   assert(!e->lfo_dest||(e->lfo_wave==4&&(e->lfo_dest==13||e->lfo_dest==14)));
   assert((int)s->pitch-64+(int)q.default_note[0]==kick_note(kick_root));
   cases++;
  }
 }
 assert(kicks>1000);
 /* Variety: every character shows up in a healthy share of seeds. */
  assert(short_kicks*12>kicks&&long_kicks*12>kicks&&tonal*8>kicks&&chirp*8>kicks&&noisy*16>kicks&&light*12>kicks&&heavy*12>kicks);
 printf("kick variety over %u kicks: short %u, long %u, tonal %u, chirp %u, feedback %u, light drive %u, heavy drive %u\n",kicks,short_kicks,long_kicks,tonal,chirp,noisy,light,heavy);
 /* Sequences: every hit on track 1 is the same note, sounding the kick root. */
 for(unsigned g=MC_PHRASE;g<=MC_PATTERN;g++)for(unsigned music=0;music<=48;music++)for(unsigned pitch=40;pitch<=88;pitch+=6)for(unsigned d=0;d<=100;d+=50){
  mc_request q={.generator=g,.path=MC_RHYTHMIC,.target=0,.density=d,.brutality=80,.machine_mask=255,.machine_valid_mask=63,.source_sound_valid_mask=1,.phrase_length=g==MC_PHRASE?32:0};
  uint8_t root,scale;if(music==48)q.music=0;else assert(mc_music_pack(music%12,music/12,&q.music));
  assert(mc_music_unpack(q.music,&root,&scale));unsigned kick_root=q.music?root:0;
  for(unsigned t=0;t<6;t++)q.current_machine[t]=t==0?0:4;
  q.current_sound[0]=(mc_sound){0,(uint8_t)pitch,30,60,60,50,1,0,64,60};
  for(unsigned seed=1;seed<=4;seed++){
   q.seed=seed*40503u+pitch+music;assert(mc_generate(&q,&p));
   const mc_phrase *ph=&p.phrase[0];int note=-1;
   for(unsigned i=0;i<64;i++){const mc_event *e=&ph->step[i];if(!e->on)continue;
    if(note<0)note=e->note;assert(e->note==note);hits++;
    assert(e->velocity>=96&&!e->condition&&!e->micro);
    for(unsigned j=0;j<e->lock_count;j++)assert(e->lock_k[j]!=10);
    int eff=(int)e->note+(int)pitch-64;assert(eff==kick_note(kick_root)&&mc_note_in_scale((unsigned)eff,root,scale));
   }
   if(g==MC_PATTERN||ph->length)assert(note>=0);
   cases++;
  }
 }
 /* No choice made: C. An explicit A natural minor: A. */
 {mc_request q={.generator=MC_PHRASE,.path=MC_RHYTHMIC,.target=0,.density=60,.machine_mask=255,.phrase_length=16,.seed=9};
  assert(mc_generate(&q,&p));for(unsigned i=0;i<64;i++)if(p.phrase[0].step[i].on)assert(p.phrase[0].step[i].note==60);
  q.music=0x99;assert(mc_generate(&q,&p));for(unsigned i=0;i<64;i++)if(p.phrase[0].step[i].on)assert(p.phrase[0].step[i].note==57);cases++;}
 printf("PASS %u Rhythmic track-1 kick cases: %u punch-shaped kicks, %u root-only hits\n",cases,kicks,hits);return 0;}

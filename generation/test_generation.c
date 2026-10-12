#include "generation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned events(mc_plan *p){unsigned i,j,n=0;for(i=0;i<6;i++)for(j=0;j<64;j++)n+=p->phrase[i].step[j].on;return n;}
int main(void){mc_request q={.generator=0,.path=0,.target=3,.density=30,.brutality=35,.machine_mask=63,.seed=1};mc_plan p,other,old;unsigned g,path,k,total=0;
 for(g=0;g<4;g++)for(path=0;path<4;path++)for(k=1;k<=256;k++){
  unsigned i,j; q.generator=g;q.path=path;q.seed=k;assert(mc_generate(&q,&p));q.seed=k+1;assert(mc_generate(&q,&other));assert(memcmp(&p,&other,sizeof p));
  assert(p.sound_mask==(g==0?8:g==1?63:0));assert(p.sequence_mask==(g==2?8:g==3?63:0));
  for(i=0;i<6;i++){assert(p.sound[i].machine<=5);if((p.sound_mask&(1u<<i))&&(path==MC_RHYTHMIC||path==MC_FREE)){if(p.sound[i].machine==5)assert(p.sound[i].pitch>=52&&p.sound[i].shape<=37);}for(j=0;j<64;j++){mc_event *e=&p.phrase[i].step[j];assert(e->note<=127&&e->velocity<=127);if(e->on){assert(j<p.phrase[i].length);assert(e->note>=40);}}}
  if(g<2){q.density=100;assert(mc_generate(&q,&other));q.seed=k;assert(mc_generate(&q,&old));q.density=30;assert(mc_generate(&q,&p));assert(!memcmp(&old,&p,sizeof p));}
  total++;
 }
 for(path=MC_ABSTRACT;path<=MC_FREE;path+=3){unsigned machines=0;q.generator=MC_PATCH;q.path=path;q.target=0;for(k=1;k<=256;k++){q.seed=k;assert(mc_generate(&q,&p));machines|=1u<<p.sound[0].machine;}assert(machines==63);}
 q.generator=MC_PATTERN;q.path=MC_RHYTHMIC;q.seed=12;q.density=0;assert(mc_generate(&q,&p));assert(events(&p)>=3&&events(&p)<=6);q.density=100;assert(mc_generate(&q,&other));assert(events(&other)>events(&p));
 q.path=MC_ABSTRACT;q.density=30;q.brutality=0;assert(mc_generate(&q,&p));q.brutality=100;assert(mc_generate(&q,&other));assert(!memcmp(&p,&other,sizeof p));
 q.path=MC_MELODIC;assert(mc_generate(&q,&p));q.brutality=0;assert(mc_generate(&q,&other));assert(!memcmp(&p,&other,sizeof p));
 for(path=0;path<4;path++){unsigned density,previous=0; q.generator=MC_PATTERN;q.path=path;q.target=3;q.seed=32;for(density=0;density<=100;density++){q.density=density;q.brutality=0;assert(mc_generate(&q,&p));assert(events(&p)>=previous);previous=events(&p);q.brutality=100;assert(mc_generate(&q,&other));assert(events(&other)==events(&p));}}
 q.generator=MC_PHRASE;q.path=MC_RHYTHMIC;q.machine_valid_mask=63;for(k=0;k<6;k++)q.current_machine[k]=k;for(k=0;k<6;k++){static const unsigned roles[6]={MC_ANCHOR,MC_IMPACT,MC_DETAIL,MC_RESPONSE,MC_PITCHED,MC_PITCHED};q.target=k;assert(mc_generate(&q,&p));assert(p.phrase[k].role==roles[k]);}
 memset(&p,0xa5,sizeof p);old=p;q.target=6;assert(!mc_generate(&q,&p));assert(!memcmp(&p,&old,sizeof p));
 {uint8_t buf[38];mc_sound s={5,60,90,37,30,80,1,0,64,70};memset(buf,0xa5,sizeof buf);assert(mc_encode_engine(&s,buf,sizeof buf));assert(buf[17]==0xa5&&buf[18]==5&&buf[19]==0&&buf[36]==70&&buf[37]==0);s.shape=38;assert(!mc_encode_engine(&s,buf,sizeof buf));s.shape=37;s.gate=2;assert(!mc_encode_engine(&s,buf,sizeof buf));}
 {mc_plan alias;mc_request captured={.generator=MC_PATCH,.path=MC_MELODIC,.target=2,.density=30,.brutality=35,.machine_mask=63,.seed=7};mc_plan expected;assert(mc_generate(&captured,&expected));memset(&alias,0xa5,sizeof alias);memcpy(&alias,&captured,sizeof captured);assert(mc_generate((const mc_request *)&alias,&alias));assert(!memcmp(&alias,&expected,sizeof alias));}
 {unsigned len; q.target=2;q.generator=MC_PHRASE;q.path=MC_FREE;q.density=100;
  for(path=0;path<4;path++)for(len=1;len<=64;len++){q.path=path;q.phrase_length=len;assert(mc_generate(&q,&p));assert(p.phrase[2].length==len);assert(events(&p)==len);q.density=0;assert(mc_generate(&q,&p));assert(events(&p)==1);q.density=100;}
  q.phrase_length=65;memset(&p,0xa5,sizeof p);old=p;assert(!mc_generate(&q,&p));assert(!memcmp(&p,&old,sizeof p));
  q.phrase_length=0;q.generator=MC_PATTERN;assert(mc_generate(&q,&p));q.phrase_length=13;assert(mc_generate(&q,&other));assert(!memcmp(&p,&other,sizeof p));
 }
 {
  mc_request m={.generator=MC_PATCH,.path=MC_ABSTRACT,.target=0,.density=30,.brutality=35,.machine_mask=MC_MACRO_BIT};uint64_t models=0;
  for(path=MC_ABSTRACT;path<=MC_FREE;path+=3){models=0;m.path=path;for(k=1;k<=4096;k++){m.seed=k;assert(mc_generate(&m,&p));assert(p.sound[0].machine==MC_MACRO&&p.sound[0].shape<=46);models|=(uint64_t)1<<p.sound[0].shape;}
  assert(models==(((uint64_t)1<<47)-1));}
  m.path=MC_MELODIC;for(k=1;k<=256;k++){m.seed=k;assert(mc_generate(&m,&p));assert(p.sound[0].shape<32||(p.sound[0].shape>=37&&p.sound[0].shape<=40));}
  m.generator=MC_PHRASE;m.machine_valid_mask=1;m.current_machine[0]=MC_MACRO;m.machine_mask=255;assert(mc_generate(&m,&p));assert(p.phrase[0].role==MC_PITCHED); /* MACRO, model unknown */
  m.current_machine[0]=MC_MACRO;m.source_sound_valid_mask=1;m.current_sound[0]=(mc_sound){MC_MACRO,64,40,34,50,30,0,0,64,40};assert(mc_generate(&m,&p));assert(p.phrase[0].role==MC_ANCHOR);
  m.current_sound[0].shape=46;assert(mc_generate(&m,&p));assert(p.phrase[0].role==MC_TEXTURE);
  m.current_sound[0].shape=47;old=p;assert(!mc_generate(&m,&p));assert(!memcmp(&p,&old,sizeof p));
  {uint8_t bytes[38]={0};mc_sound macro={MC_MACRO,64,127,46,127,127,1,0,64,40};assert(mc_encode_engine(&macro,bytes,38));macro.shape=47;assert(!mc_encode_engine(&macro,bytes,38));macro.machine=MC_MACHINES;assert(!mc_encode_engine(&macro,bytes,38));}
 }
 {
  mc_request r={.generator=MC_KIT,.path=MC_RHYTHMIC,.target=0,.density=45,.brutality=55,.machine_mask=255};unsigned snaretypes=0,hattypes=0,perctypes=0;
  for(k=1;k<=1024;k++){r.seed=k;assert(mc_generate(&r,&p));assert(p.sound_mask==63&&p.sequence_mask==0);
   assert(p.sound[0].machine==0||(p.sound[0].machine==MC_MACRO&&p.sound[0].shape==34));
   assert(p.sound[1].machine==1||p.sound[1].machine==2||p.sound[1].machine==4||(p.sound[1].machine==MC_MACRO&&p.sound[1].shape!=34));
   assert(p.sound[2].machine==2||p.sound[2].machine==3||p.sound[2].machine==4||(p.sound[2].machine==MC_MACRO&&p.sound[2].shape!=34));
   assert(p.sound[0].pitch>=64&&p.sound[0].pitch<=76&&p.sound[0].punch==1);assert(p.sound[2].pitch>=70&&p.sound[2].decay<=30);
   snaretypes|=1u<<p.sound[1].machine;hattypes|=1u<<p.sound[2].machine;
   for(g=3;g<6;g++){assert(p.sound[g].machine>=1&&p.sound[g].machine<=4||(p.sound[g].machine==MC_MACRO&&p.sound[g].shape!=34));perctypes|=1u<<p.sound[g].machine;}
  }assert(snaretypes==(0x16|MC_MACRO_BIT)&&hattypes==(0x1c|MC_MACRO_BIT)&&perctypes==(0x1e|MC_MACRO_BIT));
  r.machine_mask=8;old=p;assert(!mc_generate(&r,&p));assert(!memcmp(&p,&old,sizeof p));
  r.machine_mask=255;r.generator=MC_PATTERN;r.machine_valid_mask=63;for(g=0;g<6;g++)r.current_machine[g]=4;
  for(k=1;k<=128;k++){r.seed=k;r.density=0;assert(mc_generate(&r,&p));assert(p.sound_mask==0&&p.sequence_mask==63&&p.phrase[0].role==MC_ANCHOR&&events(&p)>=3&&events(&p)<=6);{unsigned kick=0;for(g=0;g<64;g++)kick+=p.phrase[0].step[g].on;assert(kick==1);}
   r.density=45;assert(mc_generate(&r,&p));assert(p.phrase[0].length);{static const unsigned layout[6]={MC_ANCHOR,MC_IMPACT,MC_DETAIL,MC_RESPONSE,MC_TEXTURE,MC_IMPACT};unsigned playing=0;for(g=0;g<6;g++)if(p.phrase[g].length){playing++;assert(p.phrase[g].role==layout[g]);}assert(playing>=3&&playing<=6);}
   for(g=0;g<6;g++){mc_sound empty={0};assert(!memcmp(&p.sound[g],&empty,sizeof empty));}
  }
 }
 {
  mc_request r={.generator=MC_PATCH,.target=0,.density=45,.brutality=100,.machine_mask=1};
  for(path=0;path<4;path++)for(k=1;k<=256;k++){r.path=path;r.seed=k;assert(mc_generate(&r,&p));assert(p.sound[0].pitch>=64&&p.sound[0].pitch<=76&&p.sound[0].decay>=40&&p.sound[0].punch==1&&p.sound[0].finetune>=64&&p.sound[0].finetune<=68);}
  r.generator=MC_PHRASE;r.path=MC_RHYTHMIC;r.machine_valid_mask=1;r.source_sound_valid_mask=1;r.current_machine[0]=0;r.current_sound[0]=(mc_sound){0,40,20,30,30,20,1,0,64,45};
  for(k=1;k<=256;k++){r.seed=k;assert(mc_generate(&r,&p));for(g=0;g<64;g++)if(p.phrase[0].step[g].on){assert(p.phrase[0].step[g].note>=84&&p.phrase[0].step[g].velocity>=96);}}
 }
 {
  mc_request r={.generator=MC_PATCH,.target=0,.density=45,.machine_mask=1};
  for(path=0;path<4;path++){unsigned changed=0;r.path=path;
   for(k=1;k<=256;k++){r.seed=k;r.brutality=0;assert(mc_generate(&r,&p));r.brutality=100;assert(mc_generate(&r,&other));
    assert(p.sound[0].pitch==other.sound[0].pitch&&p.sound[0].decay==other.sound[0].decay&&p.sound[0].punch==other.sound[0].punch);
    assert(other.sound[0].punch==1&&other.extra[0].volume>=p.extra[0].volume);changed+=memcmp(&p,&other,sizeof p)!=0;
   }assert(path==MC_MELODIC?changed==0:changed>240);
  }
 }
 {
  mc_request r={.generator=MC_PATCH,.target=0,.density=45,.machine_mask=16,.note_valid_mask=1};unsigned tonic,mode;
  for(tonic=0;tonic<12;tonic++)for(mode=0;mode<4;mode++)for(path=0;path<4;path++){
   assert(mc_music_pack(tonic,mode,&r.music));r.path=path;r.seed=31;
   for(k=0;k<128;k+=7){r.default_note[0]=k;assert(mc_generate(&r,&p));assert(p.sound[0].finetune==64);assert(((int)p.sound[0].pitch-64+(int)k)%12==(int)tonic);}
   r.generator=MC_PHRASE;r.machine_valid_mask=1;r.source_sound_valid_mask=1;r.current_machine[0]=4;r.current_sound[0]=(mc_sound){4,73,40,40,20,20,0,0,64,40};
   assert(mc_generate(&r,&p));for(g=0;g<64;g++)if(p.phrase[0].step[g].on)assert(mc_note_in_scale(p.phrase[0].step[g].note+9,tonic,mode));
   r.generator=MC_PATCH;r.machine_valid_mask=0;r.source_sound_valid_mask=0;
  }
  r.default_note[0]=128;old=p;assert(!mc_generate(&r,&p));assert(!memcmp(&p,&old,sizeof p));r.default_note[0]=60;r.music=0xc0;assert(!mc_generate(&r,&p));assert(!memcmp(&p,&old,sizeof p));
 }
 {
  mc_request r={.generator=MC_PATCH,.path=MC_FREE,.target=0,.density=45,.machine_mask=1,.note_valid_mask=1};unsigned tonic;
  r.default_note[0]=127;assert(mc_music_pack(9,1,&r.music));memset(&p,0xa5,sizeof p);old=p;assert(!mc_generate(&r,&p));assert(!memcmp(&p,&old,sizeof p));
  for(tonic=0;tonic<12;tonic++){assert(mc_music_pack(tonic,1,&r.music));for(k=36;k<=96;k++){r.default_note[0]=k;assert(mc_generate(&r,&p));assert((p.sound[0].pitch+k-64)%12==tonic);}}
 }
 printf("PASS: %u generator/path/seed cases; bounds, scope, inactivity, density, failure atomicity, engine slots\n",total);
}

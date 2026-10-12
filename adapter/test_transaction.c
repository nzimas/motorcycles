#include "firmware.h"
#include <assert.h>
#include <string.h>
static int running,locked,calls;static unsigned char a[100],b[722];
static int stop(void*c){(void)c;return !running;}
static uint16_t save(void){assert(!locked);locked=1;return 7;}
static void restore(uint16_t s){assert(locked&&s==7);locked=0;}
static void notify(void*o,const void*e){(void)o;assert(!e&&!locked);assert(a[0]==10&&b[0]==20);calls++;}
int main(void){unsigned char olda[100]={0},oldb[722]={0},newa[100]={10},newb[722]={20};
struct mc_fw_transaction t={0};t.region_count=2;t.owner_count=2;t.stopped=stop;t.irq_save=save;t.irq_restore=restore;
t.region[0]=(struct mc_fw_region){a,olda,newa,100};t.region[1]=(struct mc_fw_region){b,oldb,newb,722};
t.owner[0]=(struct mc_fw_owner){a,notify};t.owner[1]=(struct mc_fw_owner){b,notify};
running=1;assert(!mc_fw_publish(&t));assert(!calls&&a[0]==0&&b[0]==0);running=0;
b[42]=1;assert(!mc_fw_publish(&t));assert(!calls&&a[0]==0);b[42]=0;
t.region[1].after=a;assert(!mc_fw_publish(&t));assert(!calls);t.region[1].after=newb;
assert(mc_fw_publish(&t));assert(calls==2&&!locked);assert(!memcmp(a,newa,100)&&!memcmp(b,newb,722));
return 0;}

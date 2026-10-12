#include "../adapter/firmware.h"
void *memcpy(void *d,const void *s,unsigned long n){unsigned char *a=d;const unsigned char*b=s;while(n--)*a++=*b++;return d;}
#ifndef LIVE_SHIFT
#define LIVE_SHIFT 0
#endif
#ifndef STAGE_SHIFT
#define STAGE_SHIFT 0
#endif
static uint8_t live[64000] __attribute__((aligned(4))),before[64000] __attribute__((aligned(4))),after[64000] __attribute__((aligned(4)));
struct mc_fw_transaction transaction;volatile int result;volatile unsigned notifications;
static int stopped(void *c){(void)c;return 1;}
uint16_t test_irq_save(void){return 0x2000;}
void test_irq_restore(uint16_t sr){(void)sr;}
static void notify(void *o,const void *e){(void)o;(void)e;notifications++;}
void run_transaction(void){unsigned i;for(i=0;i<64000;i++){live[i]=before[i]=0x55;after[i]=0xaa;}
 transaction.region_count=2;transaction.owner_count=1;transaction.stopped=stopped;transaction.irq_save=test_irq_save;transaction.irq_restore=test_irq_restore;
 transaction.region[0]=(struct mc_fw_region){live+LIVE_SHIFT,before+STAGE_SHIFT,after+STAGE_SHIFT,30710};transaction.region[1]=(struct mc_fw_region){live+30712,before+30712,after+30712,600};transaction.region[2]=(struct mc_fw_region){live+61420,before+61420,after+61420,2580};transaction.owner[0]=(struct mc_fw_owner){live,notify};result=mc_fw_publish(&transaction);}

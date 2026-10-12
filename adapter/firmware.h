#ifndef MC_FIRMWARE_TRANSACTION_H
#define MC_FIRMWARE_TRANSACTION_H
#include <stdint.h>
/* Prepared snapshots have persistent storage until publish returns. Resolving
 * canonical objects, playback state and audio publication is the binding's job. */
#define MC_FW_MAX_REGIONS 20
struct mc_fw_region { uint8_t *live; const uint8_t *before,*after; uint32_t size; };
struct mc_fw_owner { void *object; void (*notify)(void *, const void *); };
struct mc_fw_transaction {
 struct mc_fw_region region[MC_FW_MAX_REGIONS];
 struct mc_fw_owner owner[MC_FW_MAX_REGIONS];
 uint8_t region_count,owner_count;
 int (*stopped)(void *); void *context;
 uint16_t (*irq_save)(void); void (*irq_restore)(uint16_t);
};
/* Caller serializes UI mutations throughout preflight/commit/notifications.
 * Regions must include all canonical storage and any synchronous audio mirrors.
 * Fails before any write if bytes changed or transport is running. */
int mc_fw_publish(struct mc_fw_transaction *);
/* OS1.13 observable ABI helpers; actual vtable methods preserve parent/observers. */
void *mc_fw_owner_data(void *);
void mc_fw_owner_notify(void *,const void *);
/* Target-only defaults staging. after must already contain the prior100B sound
 * and must not alias live owner/data. Rejects unrecognized getter ABI. */
int mc_fw_stage_machine(void *live_owner,uint8_t after[100],uint8_t machine);
#endif

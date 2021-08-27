#ifndef SCHED_MS_H

#define SCHED_CORE0_SLOT0 0
#define SCHED_CORE0_SLOT1 1
#define SCHED_CORE0_SLOT2 2
#define SCHED_CORE0_SLOT3 3
#define SCHED_MAX_CORE0_SLOTS (SCHED_CORE0_SLOT3 + 1)

#define SCHED_CORE1_SLOT0 0
#define SCHED_MAX_CORE1_SLOTS (SCHED_CORE1_SLOT0 + 1)

typedef void (*sched_ms_callback_t) (void * sched_ms_argp);

extern void sched_init_slot( uint slot_num, uint32_t schd_period, sched_ms_callback_t slot_callback, void * slot_argp);
extern void sched_init_core( void );
extern void sched_printStats( void );

#define SCHED_MS_H
#endif
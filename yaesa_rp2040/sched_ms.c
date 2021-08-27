#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "sched_ms.h"

#define SCHED_ALARM_NUM0 0
#define SCHED_ALARM_IRQ0 0

#define SCHED_ALARM_NUM1 1
#define SCHED_ALARM_IRQ1 1

#define SCHED_MS_TICK ((uint32_t)1000u)

typedef struct sched_slot_struct {
    uint32_t schdSlot_periodUs;
    sched_ms_callback_t schdSlot_callback;
    void *   schdSlot_contextP;
    uint32_t schdSlot_tprev;
    uint32_t schdSlot_maxDur;
} sched_slot_t;

typedef struct sched_core_struct {
    const uint    schd_alarm_num;
    const uint    schd_alarm_irq;
    const uint8_t schd_max_slots;
    sched_slot_t * const schd_slots;
    uint32_t schd_maxdur;
    uint32_t schd_ovrrun;
} sched_core_t;

sched_slot_t sched_core0_slots[SCHED_MAX_CORE0_SLOTS];
sched_slot_t sched_core1_slots[SCHED_MAX_CORE1_SLOTS];

sched_core_t sched_core_0 = {
    .schd_alarm_num = SCHED_ALARM_NUM0,
    .schd_alarm_irq = SCHED_ALARM_IRQ0,
    .schd_max_slots = SCHED_MAX_CORE0_SLOTS,
    .schd_slots     = &sched_core0_slots[0]
};
sched_core_t sched_core_1 = {
    .schd_alarm_num = SCHED_ALARM_NUM1,
    .schd_alarm_irq = SCHED_ALARM_IRQ1,
    .schd_max_slots = SCHED_MAX_CORE1_SLOTS,
    .schd_slots     = &sched_core1_slots[0]
};

sched_core_t * const sched_cores[2] = { &sched_core_0, &sched_core_1 };

void sched_init_slot( uint slot_num, uint32_t schd_period_ms, sched_ms_callback_t slot_callback, void * slot_argp)
{
    uint core_num = get_core_num();
    sched_slot_t * const schd_slotp = sched_cores[core_num]->schd_slots;
    if (slot_num < sched_cores[core_num]->schd_max_slots) {
        schd_slotp[slot_num].schdSlot_periodUs = schd_period_ms * (uint32_t)1000u;
        schd_slotp[slot_num].schdSlot_callback = slot_callback;
        schd_slotp[slot_num].schdSlot_contextP = slot_argp;
    }
}
void sched_ms_callback( void ) {
    uint core_num = get_core_num();
    uint alarm_num = sched_cores[core_num]->schd_alarm_num;
    hw_clear_bits(&timer_hw->intr, 1u << alarm_num);
    uint32_t tStrtCore = timer_hw->timerawl;
    uint32_t tSchdCore = timer_hw->alarm[alarm_num];
    sched_slot_t * schd_slotp = sched_cores[core_num]->schd_slots;
    for (uint i = 0; i < sched_cores[core_num]->schd_max_slots; i++) {
        if ((tStrtCore - schd_slotp[i].schdSlot_tprev) > schd_slotp[i].schdSlot_periodUs) {
            schd_slotp[i].schdSlot_tprev = tSchdCore;
            uint32_t tStrtSlot = timer_hw->timerawl;
            if ((void *)schd_slotp[i].schdSlot_callback != (void *) 0) {
                void (*fp) (void *) = (void *) schd_slotp[i].schdSlot_callback ;
                fp( schd_slotp[i].schdSlot_contextP );
            }
            uint32_t tDurSlot = timer_hw->timerawl - tStrtSlot;
            if (tDurSlot > schd_slotp[i].schdSlot_maxDur) schd_slotp[i].schdSlot_maxDur = tDurSlot;
        }
    }
    uint32_t tNext   = tSchdCore + SCHED_MS_TICK;
    uint32_t tTstPrd = tNext - timer_hw->timerawl;
    // test if just about to or have already gone past the timer count for next interrupt 
    if ((tTstPrd < (uint32_t)50u) || (tTstPrd > SCHED_MS_TICK)) {
        sched_cores[core_num]->schd_ovrrun++;
        tNext = timer_hw->timerawl + (uint32_t)15u;
    }
    timer_hw->alarm[alarm_num] = tNext;
    uint32_t tDurCore = timer_hw->timerawl - tStrtCore;
    if (tDurCore > sched_cores[core_num]->schd_maxdur) sched_cores[core_num]->schd_maxdur = tDurCore;
}
void sched_init_core( void ) {
    uint core_num = get_core_num();
    hw_set_bits(&timer_hw->inte, 1u << sched_cores[core_num]->schd_alarm_num);
    irq_set_exclusive_handler(sched_cores[core_num]->schd_alarm_irq, sched_ms_callback);
    irq_set_enabled(sched_cores[core_num]->schd_alarm_irq, true);
    uint32_t tNow = timer_hw->timerawl;
    sched_slot_t * schd_slotp = sched_cores[core_num]->schd_slots;
    for (uint i = 0; i < sched_cores[core_num]->schd_max_slots; i++) {
        schd_slotp[i].schdSlot_tprev = tNow - (uint32_t)(i * SCHED_MS_TICK); // add a stagger for first period
    }
    timer_hw->alarm[sched_cores[core_num]->schd_alarm_num] = tNow + SCHED_MS_TICK;
}
void sched_printStats( void ) {
    printf("sched_ms overrun cnts and max durations (us)- "
           "'core 0': %d %d, slots %d %d %d %d, 'core 1': %d %d, slots %d \n",
        sched_cores[0]->schd_ovrrun,
        sched_cores[0]->schd_maxdur,
           sched_cores[0]->schd_slots[0].schdSlot_maxDur,
           sched_cores[0]->schd_slots[1].schdSlot_maxDur,
           sched_cores[0]->schd_slots[2].schdSlot_maxDur,
           sched_cores[0]->schd_slots[3].schdSlot_maxDur,
        sched_cores[1]->schd_ovrrun,
        sched_cores[1]->schd_maxdur,
           sched_cores[1]->schd_slots[0].schdSlot_maxDur);
}

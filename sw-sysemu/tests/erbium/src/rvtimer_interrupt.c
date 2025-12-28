/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: RVTimer interrupt fires when MTIME >= MTIMECMP
* Expected: PASS (machine timer interrupt fires, trap handler catches it)
*/

#include "test.h"
#include "trap.h"
#include <stdint.h>

#define ESR_MTIME             0x80F40200ull
#define ESR_MTIMECMP          0x80F40208ull
#define ESR_MTIME_LOCAL_TARGET 0x80F40218ull

#define MIE_MTIE    (1UL << 7)   /* Machine Timer Interrupt Enable */

#define MSTATUS_MIE (1UL << 3)   /* Machine Interrupt Enable */

int main() {
    volatile uint64_t *mtime = (volatile uint64_t *)ESR_MTIME;
    volatile uint64_t *mtimecmp = (volatile uint64_t *)ESR_MTIMECMP;
    volatile uint64_t *mtime_target = (volatile uint64_t *)ESR_MTIME_LOCAL_TARGET;

    /* Enable timer interrupt for minion 0 only */
    *mtime_target = 0x1;

    *mtime = 0;

    /* Set MTIMECMP to fire after 100 timer ticks
     * Note: write_mtimecmp sets mtimecmp = mtime + value */
    *mtimecmp = 100;

    expect_exception(CAUSE_MACHINE_TIMER_INTERRUPT);

    /* Enable machine timer interrupt in MIE */
    asm volatile("csrs mie, %0" :: "r"(MIE_MTIE));

    /* Enable global machine interrupts in MSTATUS */
    asm volatile("csrs mstatus, %0" :: "r"(MSTATUS_MIE));

    /* Wait for interrupt - should fire within a few thousand cycles */
    for (volatile int i = 0; i < 100000; i++) {
        asm volatile("nop");
    }

    TEST_FAIL;
    return 0;
}

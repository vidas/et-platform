/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: U-mode access to S-mode MRAM region triggers fault
* Expected: Load access fault (cause=5)
*/

#include "test.h"
#include "trap.h"
#include "priv.h"

#define SMODE_REGION_ADDR  (MRAM_BASE + 0x1000)  /* 4KB offset */

/* Test callback - runs in U-mode */
void umode_test(void) {
    /* Try to read from S-mode only region - should trap */
    volatile uint64_t *smode_region = (volatile uint64_t *)SMODE_REGION_ADDR;
    (void)*smode_region;

    TEST_FAIL;
}

int main() {
    /* Enable MPROT: 4KB M-mode (size=0), 8KB S-mode (size=1) */
    mprot_write(MPROT_EN | MPROT_MMODE_SIZE(0) | MPROT_SMODE_SIZE(1));

    expect_exception(CAUSE_LOAD_ACCESS_FAULT);

    run_in_umode(umode_test);

    return 0;
}

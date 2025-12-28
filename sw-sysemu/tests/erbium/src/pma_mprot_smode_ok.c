/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: S-mode can access S-mode MRAM region (no fault)
*/

#include "test.h"
#include "trap.h"
#include "priv.h"

#define SMODE_REGION_ADDR  (MRAM_BASE + 0x1000)  /* 4KB offset */

/* Test callback - runs in S-mode */
void smode_test(void) {
    /* Read from S-mode accessible region - should succeed */
    volatile uint64_t *smode_region = (volatile uint64_t *)SMODE_REGION_ADDR;
    (void)*smode_region;

    TEST_PASS;
}

int main() {
    /* Enable MPROT: 4KB M-mode (size=0), 8KB S-mode (size=1) */
    mprot_write(MPROT_EN | MPROT_MMODE_SIZE(0) | MPROT_SMODE_SIZE(1));

    expect_no_exception();

    run_in_smode(smode_test);

    return 0;
}

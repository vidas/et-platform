/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: S-mode access to M-mode-only MRAM region triggers fault
* Expected: Load access fault (cause=5)
*/

#include "test.h"
#include "trap.h"
#include "priv.h"

/* Test callback - runs in S-mode */
void smode_test(void) {
    /* Try to read from M-mode only region - should trap */
    volatile uint64_t *mram = (volatile uint64_t *)MRAM_BASE;
    (void)*mram;

    TEST_FAIL;
}

int main() {
    /* Enable MPROT: 4KB M-mode region (MMODE_SIZE=0) */
    mprot_write(MPROT_EN | MPROT_MMODE_SIZE(0) | MPROT_SMODE_SIZE(0));

    expect_exception(CAUSE_LOAD_ACCESS_FAULT);

    run_in_smode(smode_test);

    return 0;
}

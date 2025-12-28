/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: With MPROT disabled, U-mode can access all of MRAM
*/

#include "test.h"
#include "trap.h"
#include "priv.h"

/* Test callback - runs in U-mode */
void umode_test(void) {
    /* Read from MRAM base - should succeed when MPROT disabled */
    volatile uint64_t *mram = (volatile uint64_t *)MRAM_BASE;
    (void)*mram;

    TEST_PASS;
}

int main() {
    /* Ensure MPROT is disabled (default state, but be explicit) */
    mprot_write(0);

    expect_no_exception();

    run_in_umode(umode_test);

    return 0;
}

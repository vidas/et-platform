/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: U-mode cannot execute code in M-mode-only MRAM region
* Expected: Instruction access fault (cause=1)
*/

#include "test.h"
#include "trap.h"
#include "priv.h"

/* Target function placed in MRAM (M-mode only when MPROT enabled) */
__attribute__((section(".mram_text"), noinline))
void mram_func(void) {
    /* Should never execute - U-mode can't fetch from here */
    TEST_FAIL;
}

/* Test callback - runs in U-mode */
void umode_test(void) {
    /* Function pointer on stack (SRAM) - U-mode can read this */
    void (*volatile mram_func_ptr)(void) = mram_func;

    /* Try to call function in M-mode only region - should fault on fetch */
    mram_func_ptr();

    TEST_FAIL;
}

int main() {
    /* Enable MPROT: 4KB M-mode region (MMODE_SIZE=0) */
    mprot_write(MPROT_EN | MPROT_MMODE_SIZE(0) | MPROT_SMODE_SIZE(0));

    expect_exception(CAUSE_INSTRUCTION_ACCESS_FAULT);

    run_in_umode(umode_test);

    return 0;
}

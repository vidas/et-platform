/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: ESR unaligned access triggers access fault
* Expected: Load access fault
*
* ESR region requires 64-bit aligned access only.
*/

#include "test.h"
#include "trap.h"

#define ESR_BASE 0x8000'0000ull

int main() {
    expect_exception(CAUSE_LOAD_ACCESS_FAULT);

    /* Unaligned 64-bit read from ESR (offset +4) - should trap */
    volatile uint64_t *esr = (volatile uint64_t *)(ESR_BASE + 4);
    (void)*esr;

    TEST_FAIL;
    return 0;
}

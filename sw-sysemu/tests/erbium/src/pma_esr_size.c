/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: ESR access with wrong size (32-bit) triggers access fault
* Expected: Load access fault (cause=5)
*
* ESR region requires 64-bit size access only.
*/

#include "test.h"
#include "trap.h"

#define ESR_BASE 0x8000'0000ull

int main() {
    expect_exception(CAUSE_LOAD_ACCESS_FAULT);

    /* 32-bit read from ESR - should trap (requires 64-bit) */
    volatile uint32_t *esr = (volatile uint32_t *)ESR_BASE;
    (void)*esr;

    TEST_FAIL;
    return 0;
}

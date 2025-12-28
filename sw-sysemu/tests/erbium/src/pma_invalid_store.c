/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: Store to invalid memory region triggers store access fault
* Expected: Store access fault (cause=7), test PASSes via trap handler
*/

#include "test.h"
#include "trap.h"

#define INVALID_ADDR 0x1000'0000ull

int main() {
    expect_exception(CAUSE_STORE_ACCESS_FAULT);

    volatile uint64_t *invalid = (volatile uint64_t *)INVALID_ADDR;
    *invalid = 0xDEAD'BEEF;

    TEST_FAIL;
    return 0;
}

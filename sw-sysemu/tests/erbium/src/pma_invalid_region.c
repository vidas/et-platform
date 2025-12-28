/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: Access to invalid memory region triggers exception
* Expected: Load access fault (cause=5), test PASSes via trap handler
*/


#include "test.h"
#include "trap.h"

#define INVALID_ADDR 0x1000'0000ull

int main() {
    expect_exception(CAUSE_LOAD_ACCESS_FAULT);

    /* Access invalid region - should trap */
    volatile uint64_t *invalid = (volatile uint64_t *)INVALID_ADDR;
    (void)*invalid;

    TEST_FAIL;
    return 0;
}

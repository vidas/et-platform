/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: Write to bootrom triggers store access fault (read-only region)
* Expected: Store access fault (cause=7), test PASSes via trap handler
*/

#include "test.h"
#include "trap.h"

/* Bootrom region: 0x0200A000 - 0x0200BFFF (8KB, read-only) */
#define BOOTROM_BASE 0x0200'A000ull

int main() {
    expect_exception(CAUSE_STORE_ACCESS_FAULT);

    /* Attempt to write to read-only bootrom - should trap */
    volatile uint32_t *bootrom = (volatile uint32_t *)BOOTROM_BASE;
    *bootrom = 0xDEADBEEF;

    TEST_FAIL;
    return 0;
}

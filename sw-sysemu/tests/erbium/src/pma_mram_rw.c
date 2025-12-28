/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: Basic MRAM read/write access
*/

#include "test.h"
#include <stdint.h>

#define MRAM_BASE 0x4000'0000ull
#define TEST_PATTERN 0xDEAD'BEEF'CAFE'FEEDull

int main() {
    volatile uint64_t *mram = (volatile uint64_t *)MRAM_BASE;

    *mram = TEST_PATTERN;

    if (*mram == TEST_PATTERN) {
        TEST_PASS;
    }

    TEST_FAIL;
    return 0;
}

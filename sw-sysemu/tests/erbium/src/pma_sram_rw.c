/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: Basic SRAM read/write access
*/

#include "test.h"
#include <stdint.h>

/* SRAM region: 0x0200E000 - 0x0200E7FF (2KB) */
#define SRAM_BASE 0x0200'E000ull
#define TEST_PATTERN 0xCAFE'FEED'DEAD'BEEFull

int main() {
    volatile uint64_t *sram = (volatile uint64_t *)SRAM_BASE;

    /* Write test pattern */
    *sram = TEST_PATTERN;

    /* Read back and verify */
    if (*sram == TEST_PATTERN) {
        TEST_PASS;
    }

    TEST_FAIL;
    return 0;
}

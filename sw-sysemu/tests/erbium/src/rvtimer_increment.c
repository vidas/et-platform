/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: RVTimer MTIME increments over time
* Expected: PASS (MTIME value increases after delay)
*/

#include "test.h"
#include <stdint.h>

/* ESR addresses for RVTimer */
#define ESR_MTIME    0x80F40200ull
#define ESR_MTIMECMP 0x80F40208ull

int main() {
    volatile uint64_t *mtime = (volatile uint64_t *)ESR_MTIME;

    *mtime = 0;

    uint64_t time1 = *mtime;

    /* Delay loop - timer should tick during this */
    for (volatile int i = 0; i < 1000; i++) {
        asm volatile("nop");
    }

    uint64_t time2 = *mtime;

    if (time2 > time1) {
        TEST_PASS;
    }

    TEST_FAIL;
    return 0;
}

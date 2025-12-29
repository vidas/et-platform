/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#ifndef ERBIUM_TEST_H
#define ERBIUM_TEST_H

#include <stdint.h>

// Read hart ID
static inline uint64_t get_hart_id(void) {
    uint64_t val;
    asm volatile("csrr %0, mhartid" : "=r"(val));
    return val;
}

// Test result signaling via validation0 CSR
// These magic values are recognized by the emulator:
//   0x1FEED000 - Signal test PASS, hart becomes unavailable
//   0x50BAD000 - Signal test FAIL, emulator stops with failure

#define TEST_PASS do { \
    asm volatile("li a7, 0x1feed000; csrw validation0, a7" ::: "a7"); \
} while(0)

#define TEST_FAIL do { \
    asm volatile("li a7, 0x50bad000; csrw validation0, a7" ::: "a7"); \
} while(0)

#endif // ERBIUM_TEST_H

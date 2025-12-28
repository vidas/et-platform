/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#ifndef ERBIUM_PRIV_H
#define ERBIUM_PRIV_H

#include <stdint.h>

#define ESR_MPROT_ADDR  0x80D00020UL

#define MPROT_EN            (1 << 8)    /* Enable protection */
#define MPROT_MMODE_SIZE(n) ((n) << 4)  /* M-mode region: 4KB * 2^n */
#define MPROT_SMODE_SIZE(n) ((n) << 0)  /* S-mode region: 4KB * 2^n */

#define MRAM_BASE  0x40000000UL

static inline void mprot_write(uint64_t value) {
    volatile uint64_t *mprot = (volatile uint64_t *)ESR_MPROT_ADDR;
    *mprot = value;
}

__attribute__((unused))
static inline uint64_t mprot_read(void) {
    volatile uint64_t *mprot = (volatile uint64_t *)ESR_MPROT_ADDR;
    return *mprot;
}

/*
 * Run callback in S-mode.
 * The callback must not return - it should call TEST_PASS/TEST_FAIL
 * or trigger a trap that ends the test.
 */
__attribute__((noinline, noreturn, unused))
static void run_in_smode(void (*func)(void)) {
    asm volatile(
        "csrw mepc, %0\n"
        "li t0, (3 << 11)\n"    /* MPP mask */
        "csrc mstatus, t0\n"    /* clear MPP */
        "li t0, (1 << 11)\n"    /* MPP = S-mode */
        "csrs mstatus, t0\n"
        "mret\n"
        :
        : "r"(func)
        : "t0"
    );
    __builtin_unreachable();
}

/*
 * Run callback in U-mode.
 * The callback must not return - it should call TEST_PASS/TEST_FAIL
 * or trigger a trap that ends the test.
 */
__attribute__((noinline, noreturn, unused))
static void run_in_umode(void (*func)(void)) {
    asm volatile(
        "csrw mepc, %0\n"
        "li t0, (3 << 11)\n"    /* MPP mask */
        "csrc mstatus, t0\n"    /* clear MPP (00 = U-mode) */
        "mret\n"
        :
        : "r"(func)
        : "t0"
    );
    __builtin_unreachable();
}

#endif /* ERBIUM_PRIV_H */

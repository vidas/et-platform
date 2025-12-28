/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#ifndef ERBIUM_TRAP_H
#define ERBIUM_TRAP_H

#include <stdint.h>

/* Exception cause codes (from RISC-V spec) */
#define CAUSE_INSTRUCTION_ACCESS_FAULT  1
#define CAUSE_LOAD_ACCESS_FAULT         5
#define CAUSE_STORE_ACCESS_FAULT        7

/* Interrupt cause codes (bit 63 set + interrupt code) */
#define CAUSE_MACHINE_TIMER_INTERRUPT   0x8000000000000007ULL

/* Set expected exception cause in mscratch.
 * When trap occurs with matching cause -> PASS
 * When trap occurs with different cause -> FAIL
 * When no trap occurs (test continues) -> test should call TEST_FAIL */
static inline void expect_exception(uint64_t cause) {
    asm volatile("csrw mscratch, %0" :: "r"(cause));
}

/* Clear expected exception (no trap expected) */
static inline void expect_no_exception(void) {
    asm volatile("csrw mscratch, zero");
}

#endif /* ERBIUM_TRAP_H */

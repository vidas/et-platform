/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
* Test: TensorReduce with invalid tree_depth (height)
* Expected: tensor_error bit 9 is set
*
* On Erbium, valid tree_depth is 0..2 (max 2^(2+1) = 8 minions).
* Writing a height > 2 should set tensor_error[9].
*/

#include "test.h"

#define CSR_TENSOR_REDUCE  0x800
#define CSR_TENSOR_ERROR   0x808

/* TensorReduce command encoding:
 * bits [1:0]  = command (0=send, 1=receive, 2=broadcast, 3=reduce)
 * bits [6:3]  = height/tree_depth (for broadcast/reduce)
 */
#define TENSOR_CMD_BROADCAST 2

/* Build tensor_reduce value for broadcast/reduce with given height */
#define TENSOR_REDUCE_BROADCAST(height) (((height) << 3) | TENSOR_CMD_BROADCAST)

/* tensor_error bit 9 indicates invalid tree_depth */
#define TENSOR_ERROR_INVALID_HEIGHT (1 << 9)

static inline void write_csr_tensor_reduce(uint64_t val) {
    asm volatile("csrw %0, %1" :: "i"(CSR_TENSOR_REDUCE), "r"(val));
}

static inline void write_csr_tensor_error(uint64_t val) {
    asm volatile("csrw %0, %1" :: "i"(CSR_TENSOR_ERROR), "r"(val));
}

static inline uint64_t read_csr_tensor_error(void) {
    uint64_t val;
    asm volatile("csrr %0, %1" : "=r"(val) : "i"(CSR_TENSOR_ERROR));
    return val;
}

int main() {
    uint64_t error;

    /* tensor_reduce CSR is only accessible from thread 0.
     * Other threads should exit immediately. */
    if (get_hart_id() % 2 != 0) {
        TEST_PASS;
        return 0;
    }

    /* Clear tensor_error */
    write_csr_tensor_error(0);

    /* Verify tensor_error is cleared */
    error = read_csr_tensor_error();
    if (error != 0) {
        TEST_FAIL;
    }

    /* Write tensor_reduce broadcast with invalid height (3) */
    write_csr_tensor_reduce(TENSOR_REDUCE_BROADCAST(3));

    /* Read tensor_error and check bit 9 */
    error = read_csr_tensor_error();
    if (error & TENSOR_ERROR_INVALID_HEIGHT) {
        TEST_PASS;
    }

    TEST_FAIL;
    return 0;
}

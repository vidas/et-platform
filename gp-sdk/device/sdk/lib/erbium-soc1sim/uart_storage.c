/*-------------------------------------------------------------------------
 * Copyright (c) 2026 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-----------------------------------------------------------------------*/

/* Storage for the erbium-soc1sim fake-UART region. The driver header
 * <erbium/drivers/uart.h> is header-only; the actual ring memory has
 * to live in some kernel-side TU. Adding this file to every kernel
 * via the erbium-soc1sim runtime sources costs nothing for kernels
 * that don't use the UART: --gc-sections drops the unreferenced
 * symbol. */

#include "erbium/drivers/uart.h"

__attribute__((section(".uart_ring"), aligned(64)))
struct erbium_uart_region __erbium_uart_region;

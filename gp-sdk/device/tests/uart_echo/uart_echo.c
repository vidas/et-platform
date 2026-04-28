/* Copyright (c) 2026 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Simple echo loop over the UART. Hart 0 publishes the region header
 * (uart_init), then reads bytes from the host one at a time and
 * echoes each one back. Stops when it sees ASCII EOT (0x04) — the
 * host bridge sends EOT on stdin EOF or Ctrl-D so the kernel cleanly
 * returns. All other harts return immediately so the post-launch FCC
 * barrier still completes (the soc1sim kernel runtime requires every
 * hart in the shire to reach the ecall). */

#include <stdint.h>
#include <stdbool.h>

#include "erbium/isa/hart.h"
#include "erbium/drivers/uart.h"

#define UART_EOT  0x04u  /* ASCII EOT — "end of transmission" */

int main(void)
{
    const unsigned hart_id = get_hart_id();

    if (hart_id != 0u) {
        return 0;
    }

    uart_init();

    for (;;) {
        uint8_t c = uart_rx_byte();
        if (c == UART_EOT) {
            break;
        }
        uart_tx_byte(c);
    }

    uart_exit();
    return 0;
}

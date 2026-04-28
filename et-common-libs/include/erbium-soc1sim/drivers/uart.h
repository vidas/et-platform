/*-------------------------------------------------------------------------
 * Copyright (c) 2026 Ainekko, Co.
 * SPDX-License-Identifier: Apache-2.0
 *-----------------------------------------------------------------------*/

/* Fake UART for erbium-soc1sim — exposes the same uart_*() U-mode API
 * shape as <erbium/drivers/uart.h>, but backs it with a pair of byte
 * ring buffers in device memory polled by the host launcher. No
 * interrupts. Reentrant across harts via fetch-add reservation +
 * head/tail rendezvous on commit.
 *
 * Discovery: the launcher scans device DRAM for the magic 0xFA4EFA4E
 * in the region header. The header carries self-relative offsets to
 * the TX/RX rings, so its own placement is determined entirely by the
 * linker (see share/erbium-soc1sim/erbium.ld).
 *
 * Cache discipline. erbium-soc1sim's public evict() is hard-pinned to
 * dst=L2, which the host cannot observe — host DMAs come from DRAM.
 * The driver therefore programs the cache-op CSRs directly with
 * dst=MEM (0x3) so producer writes propagate all the way through L1
 * and L2 to DRAM. Consumer sides do the same to drop L1 lines before
 * re-reading state the host has updated. */

#ifndef _ERBIUM_DRIVERS_UART_H_
#define _ERBIUM_DRIVERS_UART_H_

#include <stdbool.h>
#include <stdint.h>

#include "erbium/isa/atomic.h"
#include "erbium/isa/utils.h"  /* FENCE, WAIT_CACHEOPS */

#ifdef __cplusplus
extern "C" {
#endif

/* On-wire ABI for the host launcher. Bumping requires updating the
 * matching parser in soc1sim/src/uart_bridge.cpp. */
#define ERBIUM_UART_MAGIC      0xFA4EFA4Eu
#define ERBIUM_UART_VERSION    1u

#define ERBIUM_UART_TX_CAP     8192u
#define ERBIUM_UART_RX_CAP     8192u

#if (ERBIUM_UART_TX_CAP & (ERBIUM_UART_TX_CAP - 1)) != 0
#  error "ERBIUM_UART_TX_CAP must be a power of two"
#endif
#if (ERBIUM_UART_RX_CAP & (ERBIUM_UART_RX_CAP - 1)) != 0
#  error "ERBIUM_UART_RX_CAP must be a power of two"
#endif

/* Two cursor pairs per ring: a "reservation" cursor that producers
 * fetch-add to claim a slot, and a "visible" cursor that they store
 * once their byte is committed. Consumers only ever look at the
 * visible cursor. The split is what makes multiple producers safe
 * without a lock. */
struct __attribute__((aligned(64))) erbium_uart_hdr {
    uint32_t magic;             /* ERBIUM_UART_MAGIC */
    uint32_t version;           /* ERBIUM_UART_VERSION */
    uint32_t tx_capacity;       /* ERBIUM_UART_TX_CAP */
    uint32_t rx_capacity;       /* ERBIUM_UART_RX_CAP */
    uint32_t tx_offset;         /* offset of tx_ring[0] from &hdr */
    uint32_t rx_offset;         /* offset of rx_ring[0] from &hdr */
    /* TX = device→host. */
    volatile uint32_t tx_head_resv;   /* device producers fetch-add */
    volatile uint32_t tx_head;        /* device commits; host reads */
    volatile uint32_t tx_tail;        /* host writes; device reads (full check) */
    /* RX = host→device. */
    volatile uint32_t rx_head;        /* host writes; device reads (avail check) */
    volatile uint32_t rx_tail_resv;   /* device consumers fetch-add */
    volatile uint32_t rx_tail;        /* device commits; host reads (drain check) */
    volatile uint32_t exit_flag;      /* device sets to 1 to ask host to wind down */
    uint32_t _pad;                    /* keep header at exactly 64 bytes */
};

struct __attribute__((aligned(64))) erbium_uart_region {
    struct erbium_uart_hdr hdr;
    uint8_t  tx_ring[ERBIUM_UART_TX_CAP];
    uint8_t  rx_ring[ERBIUM_UART_RX_CAP];
};

/* Linker-provided storage. The actual placement is determined by the
 * .uart_ring section in erbium.ld; the driver references it
 * symbolically and never assumes a specific address. */
extern struct erbium_uart_region __erbium_uart_region;

/* ------------------------------------------------------------------
 * Direct CSR-level cache ops with dst=MEM. The public
 * <erbium/isa/cacheops-umode.h> evict_va pins dst=L2, which is
 * invisible to the host's DMA path. We need DRAM visibility, so we
 * issue the same CSR with dst=CACHEOP_DST_MEM (0x3) instead.
 *
 * CSR encoding (matches cacheops-umode.h evict_va):
 *   bits[63]    use_tmask = 0
 *   bits[59:58] dst       = CACHEOP_DST_MEM (0x3)
 *   bits[51:6]  base addr (cache-line aligned)
 *   bits[3:0]   num_lines (1..15; 0 means 16)
 * x31 carries stride[51:6] | id[0]; for byte-granularity ops we use
 * stride=64, id=0. CSR address: 0x89f for evict, 0x8bf for flush. */

#define _ERBIUM_UART_CACHEOP_DST_MEM  0x3ULL

static inline __attribute__((always_inline))
void _erbium_uart_evict_to_mem_one(uint64_t base, uint64_t num_lines)
{
    uint64_t csr_enc = (_ERBIUM_UART_CACHEOP_DST_MEM << 58) |
                       (base & 0xFFFFFFFFFFC0ULL) |
                       (num_lines & 0xFULL);
    register uint64_t x31 asm("x31") = (64ULL & 0xFFFFFFFFFFC0ULL);
    __asm__ __volatile__("csrw 0x89f, %[csr_enc]\n"
                         :
                         : [csr_enc] "r"(csr_enc),
                           "r"(x31));
}

static inline __attribute__((always_inline))
void _erbium_uart_evict_to_mem(volatile const void *addr, uint64_t size)
{
    uint64_t a    = (uint64_t)addr;
    uint64_t base = a & ~0x3FULL;
    uint64_t n    = (((a & 0x3FULL) + size + 63ULL) >> 6);
    while (n > 15) {
        _erbium_uart_evict_to_mem_one(base, 15);
        base += 15ULL * 64ULL;
        n    -= 15ULL;
    }
    if (n) _erbium_uart_evict_to_mem_one(base, n);
    FENCE;
    WAIT_CACHEOPS;
}

/* ------------------------------------------------------------------
 * Init. Hart 0 must call this once before any other hart hits the
 * driver. Zeros the cursors, sets magic + version + offsets, evicts
 * the whole region to MEM so the host scan finds it. */

static inline __attribute__((always_inline))
void uart_init(void)
{
    struct erbium_uart_region *r = &__erbium_uart_region;
    r->hdr.magic         = 0u;  /* publish magic LAST */
    r->hdr.version       = ERBIUM_UART_VERSION;
    r->hdr.tx_capacity   = ERBIUM_UART_TX_CAP;
    r->hdr.rx_capacity   = ERBIUM_UART_RX_CAP;
    r->hdr.tx_offset     = (uint32_t)((uint64_t)&r->tx_ring[0] - (uint64_t)&r->hdr);
    r->hdr.rx_offset     = (uint32_t)((uint64_t)&r->rx_ring[0] - (uint64_t)&r->hdr);
    r->hdr.tx_head_resv  = 0u;
    r->hdr.tx_head       = 0u;
    r->hdr.tx_tail       = 0u;
    r->hdr.rx_head       = 0u;
    r->hdr.rx_tail_resv  = 0u;
    r->hdr.rx_tail       = 0u;
    r->hdr.exit_flag     = 0u;
    FENCE;
    /* Publish magic last so a racing host scan can never see a
     * partially-initialized header. */
    r->hdr.magic         = ERBIUM_UART_MAGIC;
    _erbium_uart_evict_to_mem(&r->hdr, sizeof(r->hdr));
}

/* ------------------------------------------------------------------
 * TX (device → host).
 *
 *   reserve  : slot = atomic_fetch_add(&tx_head_resv, 1)
 *   wait     : while (slot - tx_tail >= TX_CAP) — buffer full
 *   write    : tx_ring[slot & MASK] = byte; evict to MEM
 *   commit   : while (tx_head != slot); tx_head = slot+1; evict to MEM
 *
 * The reservation cursor is monotonic so the order in which producers
 * commit matches the order in which they reserved — `tx_head` only
 * advances by exactly one per commit and the host sees a continuous
 * stream of bytes.
 */

#define _ERBIUM_UART_TX_MASK ((uint32_t)(ERBIUM_UART_TX_CAP - 1u))

static inline __attribute__((always_inline))
bool uart_tx_ready(void)
{
    struct erbium_uart_hdr *h = &__erbium_uart_region.hdr;
    /* Host writes tx_tail. Drop our cached copy before peeking. */
    _erbium_uart_evict_to_mem(&h->tx_tail, 4);
    uint32_t head = atomic_load_global_32(&h->tx_head_resv);
    uint32_t tail = atomic_load_global_32(&h->tx_tail);
    return (head - tail) < ERBIUM_UART_TX_CAP;
}

static inline __attribute__((always_inline))
void uart_tx_byte(uint8_t c)
{
    struct erbium_uart_region *r = &__erbium_uart_region;
    struct erbium_uart_hdr *h = &r->hdr;

    /* Reserve our slot. */
    uint32_t slot = atomic_add_global_32(&h->tx_head_resv, 1u);

    /* Spin while the ring is full from our perspective. */
    for (;;) {
        _erbium_uart_evict_to_mem(&h->tx_tail, 4);
        uint32_t tail = atomic_load_global_32(&h->tx_tail);
        if ((slot - tail) < ERBIUM_UART_TX_CAP) break;
    }

    /* Write the byte and push it to DRAM. */
    r->tx_ring[slot & _ERBIUM_UART_TX_MASK] = c;
    _erbium_uart_evict_to_mem(&r->tx_ring[slot & _ERBIUM_UART_TX_MASK], 1);

    /* Commit in slot order. */
    for (;;) {
        uint32_t head = atomic_load_global_32(&h->tx_head);
        if (head == slot) break;
    }
    atomic_store_global_32(&h->tx_head, slot + 1u);
    _erbium_uart_evict_to_mem(&h->tx_head, 4);
}

/* ------------------------------------------------------------------
 * RX (host → device). Mirror of TX: device is the consumer, host is
 * the producer. The reservation cursor is the consumer-side counter
 * so multi-hart consumers each get a unique byte; commit serializes
 * tx-side updates back to the host (drain notification).
 */

#define _ERBIUM_UART_RX_MASK ((uint32_t)(ERBIUM_UART_RX_CAP - 1u))

static inline __attribute__((always_inline))
bool uart_rx_ready(void)
{
    struct erbium_uart_hdr *h = &__erbium_uart_region.hdr;
    _erbium_uart_evict_to_mem(&h->rx_head, 4);
    uint32_t head = atomic_load_global_32(&h->rx_head);
    uint32_t tail = atomic_load_global_32(&h->rx_tail_resv);
    return head != tail;
}

static inline __attribute__((always_inline))
uint8_t uart_rx_byte(void)
{
    struct erbium_uart_region *r = &__erbium_uart_region;
    struct erbium_uart_hdr *h = &r->hdr;

    uint32_t slot = atomic_add_global_32(&h->rx_tail_resv, 1u);

    /* Wait until the host has produced our slot. */
    for (;;) {
        _erbium_uart_evict_to_mem(&h->rx_head, 4);
        uint32_t head = atomic_load_global_32(&h->rx_head);
        if ((int32_t)(head - slot) > 0) break;
    }

    /* Drop any cached copy of the data byte before reading it. */
    _erbium_uart_evict_to_mem(&r->rx_ring[slot & _ERBIUM_UART_RX_MASK], 1);
    uint8_t c = r->rx_ring[slot & _ERBIUM_UART_RX_MASK];

    /* Commit in slot order so the host sees a monotonic drain
     * cursor and can safely refill the slots we just consumed. */
    for (;;) {
        uint32_t tail = atomic_load_global_32(&h->rx_tail);
        if (tail == slot) break;
    }
    atomic_store_global_32(&h->rx_tail, slot + 1u);
    _erbium_uart_evict_to_mem(&h->rx_tail, 4);

    return c;
}

/* ------------------------------------------------------------------
 * Shutdown. Asks the host bridge to wind down; the bridge then exits
 * its poll loop after draining any in-flight TX bytes. Idempotent. */

static inline __attribute__((always_inline))
void uart_exit(void)
{
    struct erbium_uart_hdr *h = &__erbium_uart_region.hdr;
    atomic_store_global_32(&h->exit_flag, 1u);
    _erbium_uart_evict_to_mem(&h->exit_flag, 4);
}

/* ------------------------------------------------------------------
 * Configuration shims. erbium-soc1sim has no real UART hardware, so
 * the pinmux + baud surfaces from <erbium/drivers/uart.h> are
 * reported as unsupported. Kernels that branch on these probes get a
 * deterministic answer; kernels that call them anyway get no-ops.
 * Matches the etsoc/drivers/uart.h convention. */

static inline __attribute__((always_inline))
bool uart_supports_pinmux_gate(void) { return false; }

static inline __attribute__((always_inline))
void uart_enable_pinmux(void) {}

static inline __attribute__((always_inline))
bool uart_supports_baud_roundtrip(void) { return false; }

static inline __attribute__((always_inline))
uint32_t uart_baud_get(void) { return 0u; }

static inline __attribute__((always_inline))
void uart_baud_set(uint32_t val) { (void)val; }

static inline __attribute__((always_inline))
bool uart_tx_empty(void)
{
    struct erbium_uart_hdr *h = &__erbium_uart_region.hdr;
    _erbium_uart_evict_to_mem(&h->tx_tail, 4);
    uint32_t head = atomic_load_global_32(&h->tx_head);
    uint32_t tail = atomic_load_global_32(&h->tx_tail);
    return head == tail;
}

#ifdef __cplusplus
}
#endif

#endif /* _ERBIUM_DRIVERS_UART_H_ */

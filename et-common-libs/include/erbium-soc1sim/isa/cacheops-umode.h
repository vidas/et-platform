/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------
*/

/*! \file cacheops-umode.h
    \brief U-mode entry points for cache-maintenance ops that require
    M-mode privilege on ET-SoC-1. Routes each call through an ecall
    handled by the WorkerMinion firmware's syscall dispatcher.

    Companion to <erbium/isa/cacheops.h>, which holds the ops that
    are directly issuable from U-mode (VA-based evict/flush/prefetch
    and the U-mode control CSRs). The ops here write CSRs or invoke
    logic that is privileged on ET-SoC-1 and therefore wrapped.

    Scope: only syscalls whose semantics are meaningful on **real
    Erbium** (one shire / one neighborhood of L1 feeding directly
    into MRAM) are exposed. Cross-shire / L2 / L3 / PMC syscalls
    that WorkerMinion dispatches on ET-SoC-1 are deliberately NOT
    surfaced here — using them would bake ET-SoC-1-specific
    topology into kernels that are supposed to run on Erbium too.
    Once Erbium grows a tiny M-mode shim that implements the same
    syscall numbers, kernels using this header will port unchanged.

    Destination level: hidden from every signature — soc1sim pins
    it to `CACHEOP_DST_L2` internally, mirroring the policy in
    cacheops.h (see that file for the reasoning). On Erbium the
    same numbers resolve to "memory" when the tiny M-mode shim
    lands.
*/

#ifndef _ERBIUM_ISA_CACHEOPS_UMODE_H_
#define _ERBIUM_ISA_CACHEOPS_UMODE_H_

#if defined(__cplusplus) && (__cplusplus >= 201103L)
#include <cinttypes>
#else
#include <inttypes.h>
#endif

#include "erbium/isa/cacheops.h"  /* CACHEOP_DST_L2 */
#include "erbium/isa/syscall.h"   /* SYSCALL_* numbers + syscall() */

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------- */
/* Set/way ops (M-mode CSRs 0x7f9 / 0x7fb / 0x7fd / 0x7ff)          */
/* --------------------------------------------------------------- */

/*! \brief Evict a specific set/way from this hart's L1. num_lines
 *         is 0..15 (inclusive) = 1..16 lines. */
static inline __attribute__((always_inline)) int64_t
evict_sw(uint64_t use_tmask, uint64_t way, uint64_t set, uint64_t num_lines)
{
    uint64_t csr_enc = ((use_tmask      & 1ULL)  << 63) |
                       ((CACHEOP_DST_L2 & 0x3ULL) << 58) |
                       ((set            & 0xFULL) << 14) |
                       ((way            & 0x3ULL) << 6)  |
                       (num_lines       & 0xFULL);
    return syscall(SYSCALL_CACHE_OPS_EVICT_SW, csr_enc, 0, 0);
}

/*! \brief Writeback a specific set/way if dirty; line stays in L1. */
static inline __attribute__((always_inline)) int64_t
flush_sw(uint64_t use_tmask, uint64_t way, uint64_t set, uint64_t num_lines)
{
    uint64_t csr_enc = ((use_tmask      & 1ULL)  << 63) |
                       ((CACHEOP_DST_L2 & 0x3ULL) << 58) |
                       ((set            & 0xFULL) << 14) |
                       ((way            & 0x3ULL) << 6)  |
                       (num_lines       & 0xFULL);
    return syscall(SYSCALL_CACHE_OPS_FLUSH_SW, csr_enc, 0, 0);
}

/*! \brief Hard-lock a physical address into a specific L1 way. */
static inline __attribute__((always_inline)) int64_t
lock_sw(uint64_t way, uint64_t paddr)
{
    uint64_t csr_enc = ((way & 0x3ULL) << 55) |
                       (paddr & 0xFFFFFFFFC0ULL);
    return syscall(SYSCALL_CACHE_OPS_LOCK_SW, csr_enc, 0, 0);
}

/*! \brief Hard-unlock a specific set/way. */
static inline __attribute__((always_inline)) int64_t
unlock_sw(uint64_t way, uint64_t set)
{
    uint64_t csr_enc = ((way & 0xFFULL) << 55) |
                       ((set & 0xFULL)  << 6);
    return syscall(SYSCALL_CACHE_OPS_UNLOCK_SW, csr_enc, 0, 0);
}

/* --------------------------------------------------------------- */
/* Invalidate / control (M-mode CSRs 0x7d0, 0x7e0)                  */
/* --------------------------------------------------------------- */

/*! \brief Invalidate L1 I-cache and/or TLBs + PTW. Either flag may
 *         be 0 to leave the corresponding structure alone. */
static inline __attribute__((always_inline)) int64_t
cache_invalidate(uint64_t inval_instr_cache, uint64_t inval_TLBs_and_PTW)
{
    uint64_t csr_enc = (inval_TLBs_and_PTW & 1ULL) |
                       ((inval_instr_cache & 1ULL) << 1);
    return syscall(SYSCALL_CACHE_OPS_INVALIDATE, csr_enc, 0, 0);
}

/*! \brief Re-configure this minion's L1 data cache: `d1_split`
 *         selects shared (0) vs split (1) mode (in split mode thread
 *         0 and thread 1 of the same minion don't alias each other);
 *         `scp_en` enables the scratchpad portion (requires split).
 *
 *  On ET-SoC-1 this requires the firmware-private CSR 0x7e0 and
 *  coordinated L1 drain/reconfigure, so it lands as a syscall (301)
 *  rather than a direct CSR write. Only arguments that make sense
 *  on Erbium are exposed; the etsoc mcache_control's
 *  `cacheop_rate` / `cacheop_max` fields are fixed by the firmware. */
static inline __attribute__((always_inline)) int64_t
set_l1_cache_control(uint64_t d1_split, uint64_t scp_en)
{
    return syscall(SYSCALL_CACHE_CONTROL, d1_split & 1ULL, scp_en & 1ULL, 0);
}

/* --------------------------------------------------------------- */
/* Whole-L1 evict                                                   */
/* --------------------------------------------------------------- */

/*! \brief Evict every currently-active cache line from this hart's
 *         L1. The firmware-side implementation reads the current L1
 *         mode (shared / split / SCP) and loops set/way evictions
 *         over the right set ranges; it wraps the sequence in
 *         `excl_mode(1/0)` so interrupts can't interleave the
 *         multi-CSR sequence.
 *
 *  We don't expose an in-library reconstruction of this composite
 *  because (a) excl_mode isn't reachable from U-mode, and (b)
 *  doing it from C would take ~64 syscall round-trips per invocation
 *  versus 1 for the firmware-dispatched version.
 *
 *  `use_tmask` gates each potential line eviction by the TensorMask
 *  CSR when set. */
static inline __attribute__((always_inline)) int64_t
evict_l1(uint64_t use_tmask)
{
    return syscall(SYSCALL_CACHE_OPS_EVICT_L1,
                   use_tmask & 1ULL,
                   CACHEOP_DST_L2, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* _ERBIUM_ISA_CACHEOPS_UMODE_H_ */

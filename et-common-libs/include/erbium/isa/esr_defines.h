/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
 * Erbium ESR (Esperanto Special Register) address layout.
 *
 * Address bit layout:
 *     [31]     1    ESR space marker (set by region base below)
 *     [30:24]  shire id  (always 0 on erbium; exactly one shire)
 *     [23:22]  PP        (privilege: U=0, S=1, D=2, M=3)
 *     [21:0]   sub-region base | byte offset within block
 *
 * Sub-region constants (ESR_SR_*) and the per-register byte offsets
 * come from the HAL hwinc headers; this file just composes them via
 * esr_addr() / esr_read_u64() / esr_write_u64() helpers.
 */

#ifndef _ERBIUM_ISA_ESR_DEFINES_H_
#define _ERBIUM_ISA_ESR_DEFINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__
#include <inttypes.h>
#endif

#include "hwinc/top.h"   /* ERBIUM_TOP_CPU_REGISTERS_BASE */
#include "hwinc/esr.h"   /* per-register *_BYTE_OFFSET symbols */

/* ---- privilege (PP) constants -------------------------------- */

#ifndef PRV_U
#ifdef __ASSEMBLER__
#define PRV_U 0
#else
#define PRV_U 0ull
#endif
#endif

#ifndef PRV_S
#ifdef __ASSEMBLER__
#define PRV_S 1
#else
#define PRV_S 1ull
#endif
#endif

#ifndef PRV_D
#ifdef __ASSEMBLER__
#define PRV_D 2
#else
#define PRV_D 2ull
#endif
#endif

#ifndef PRV_M
#ifdef __ASSEMBLER__
#define PRV_M 3
#else
#define PRV_M 3ull
#endif
#endif

/* ---- erbium ESR address composition -------------------------- */

#define ESR_REGION             ERBIUM_TOP_CPU_REGISTERS_BASE   /* ESR space marker bit [31] */

#define ESR_REGION_PROT_SHIFT  22              /* PP        [23:22] */
#define ESR_REGION_SHIRE_SHIFT 24              /* shire id  [30:24] */

/* Sub-region bases within the 22-bit offset field [21:0]. Pass one
 * of these as the `subregion` argument to esr_addr/read/write. */
#define ESR_SR_USER_CPU        0x340000ULL     /* User_cpu block (FCC, FLB, ...) */
#define ESR_SR_MACHINE_CPU     0xF40000ULL     /* Machine_cpu block (IPI, ...) */

/* Build an ESR address from (pp, shire, subregion, byte_offset).
 * Macro form so it's usable from assembler as well. */
#define ESR_ADDR(pp, shire, subregion, byte_offset)             \
    ((ESR_REGION) |                                             \
     ((uint64_t)((pp)    & 0x3ULL)  << ESR_REGION_PROT_SHIFT) | \
     ((uint64_t)((shire) & 0x7FULL) << ESR_REGION_SHIRE_SHIFT) |\
     ((uint64_t)((subregion) & 0x3FFFFFULL)) |                  \
     ((uint64_t)((byte_offset) & 0xFFFFULL)))

#ifndef __ASSEMBLER__

static inline __attribute__((always_inline))
uint64_t esr_addr(uint32_t pp, uint32_t shire, uint32_t subregion, uint32_t offset)
{
    return ESR_ADDR(pp, shire, subregion, offset);
}

static inline __attribute__((always_inline))
uint64_t esr_read_u64(uint32_t pp, uint32_t shire, uint32_t subregion, uint32_t offset)
{
    return *(volatile uint64_t *)esr_addr(pp, shire, subregion, offset);
}

static inline __attribute__((always_inline))
void esr_write_u64(uint32_t pp, uint32_t shire, uint32_t subregion, uint32_t offset,
                   uint64_t val)
{
    *(volatile uint64_t *)esr_addr(pp, shire, subregion, offset) = val;
}

#endif /* !__ASSEMBLER__ */

/* ---- thread identifiers ---------------------------- */
/* (THIS_SHIRE already lives in <erbium/isa/hart.h> == 0.) */

#define THREAD_0 0
#define THREAD_1 1

#ifdef __cplusplus
}
#endif

#endif /* _ERBIUM_ISA_ESR_DEFINES_H_ */

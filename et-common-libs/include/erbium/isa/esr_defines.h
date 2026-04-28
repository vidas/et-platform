/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
 * Erbium ESR (Esperanto Special Register) address layout.
 *
 * TODO: THIS FILE IS TEMPORARY. Once the erbium HAL
 *       (hal/platform/erbium/hwinc/esr.h + esr_platform.h) stabilizes
 *       these hand-picked constants should be replaced by (thin
 *       wrappers around) the HAL definitions. Do not grow this file
 *       beyond what is strictly needed for fcc/flb/barriers; add new
 *       ESRs to the HAL instead and pull them from there.
 *
 * The registers we care about right now (FCC credit counters and
 * Fast Local Barriers) were transplanted in the RTL from the etsoc
 * shire-level ESR region into the erbium neighborhood's "User_cpu"
 * block, so their byte offsets within the block are identical but
 * the absolute address bit layout changed.
 *
 * Erbium address bit layout:
 *     [31]     1    ESR space marker (set by region base below)
 *     [30:24]  shire id  (always 0 on erbium; exactly one shire)
 *     [23:22]  PP        (privilege: U=0, S=1, D=2, M=3)
 *     [21:0]   byte offset within the block (matches the sub-region
 *              byte offsets from the shire region in etsoc)
 *
 * This header intentionally covers only FCC CREDINC and FLB. Other
 * ESR sub-regions will be added as needed. The hal/platform/erbium
 * tree has the full auto-generated definitions; we hand-pick to
 * avoid pulling in those (WIP) headers.
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
#include "hwinc/esr.h"   /* USER_CPU_CREDINC*_BYTE_OFFSET */

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

/* Sub-region bases within the 22-bit offset field [21:0].
 * These select which ESR block the register lives in. */
#define ESR_SUBREGION_SHIRE    0x340000ULL     /* User_cpu / shire-level */

/* Build an ESR address from (pp, shire, subregion, byte_offset).
 * `subregion` is the block base (e.g. ESR_SUBREGION_SHIRE for
 * FCC/FLB). `byte_offset` is the register offset within the block. */
#define ESR_ADDR(pp, shire, subregion, byte_offset)             \
    ((ESR_REGION) |                                             \
     ((uint64_t)((pp)    & 0x3ULL)  << ESR_REGION_PROT_SHIFT) | \
     ((uint64_t)((shire) & 0x7FULL) << ESR_REGION_SHIRE_SHIFT) |\
     ((uint64_t)((subregion) & 0x3FFFFFULL)) |                  \
     ((uint64_t)((byte_offset) & 0xFFFFULL)))

/* Drop-in replacement for etsoc's ESR_SHIRE(shire, NAME). Names
 * match their etsoc counterparts (FCC_CREDINC_*, FAST_LOCAL_BARRIER*)
 * so existing sync/FCC/barrier code ports with only a header swap. */
#define ESR_SHIRE(shire, name)                                  \
    ESR_ADDR(ESR_SHIRE_##name##_PROT, (shire),                  \
             ESR_SUBREGION_SHIRE,                                \
             ESR_SHIRE_##name##_BYTE_OFFSET)

/* ---- FCC credit-counter registers ---------------------------- */
/* Four CREDINC registers, indexed (thread * 2 + fcc):
 *     CREDINC_0 -> thread 0, fcc 0
 *     CREDINC_1 -> thread 0, fcc 1
 *     CREDINC_2 -> thread 1, fcc 0
 *     CREDINC_3 -> thread 1, fcc 1
 * Same layout etsoc used — SEND_FCC's pointer arithmetic in fcc.h
 * works unchanged. */

#define ESR_SHIRE_FCC_CREDINC_0_BYTE_OFFSET USER_CPU_CREDINC0_BYTE_OFFSET
#define ESR_SHIRE_FCC_CREDINC_0_PROT        PRV_U

#define ESR_SHIRE_FCC_CREDINC_1_BYTE_OFFSET USER_CPU_CREDINC1_BYTE_OFFSET
#define ESR_SHIRE_FCC_CREDINC_1_PROT        PRV_U

#define ESR_SHIRE_FCC_CREDINC_2_BYTE_OFFSET USER_CPU_CREDINC2_BYTE_OFFSET
#define ESR_SHIRE_FCC_CREDINC_2_PROT        PRV_U

#define ESR_SHIRE_FCC_CREDINC_3_BYTE_OFFSET USER_CPU_CREDINC3_BYTE_OFFSET
#define ESR_SHIRE_FCC_CREDINC_3_PROT        PRV_U

/* ---- Fast Local Barriers (FLBs) ------------------------------ */
/* 32 FLBs, each one 8 bytes wide, contiguous from 0x100. */

#define ESR_SHIRE_FAST_LOCAL_BARRIER0_BYTE_OFFSET  0x100ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER0_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER1_BYTE_OFFSET  0x108ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER1_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER2_BYTE_OFFSET  0x110ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER2_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER3_BYTE_OFFSET  0x118ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER3_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER4_BYTE_OFFSET  0x120ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER4_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER5_BYTE_OFFSET  0x128ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER5_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER6_BYTE_OFFSET  0x130ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER6_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER7_BYTE_OFFSET  0x138ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER7_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER8_BYTE_OFFSET  0x140ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER8_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER9_BYTE_OFFSET  0x148ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER9_PROT         PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER10_BYTE_OFFSET 0x150ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER10_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER11_BYTE_OFFSET 0x158ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER11_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER12_BYTE_OFFSET 0x160ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER12_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER13_BYTE_OFFSET 0x168ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER13_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER14_BYTE_OFFSET 0x170ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER14_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER15_BYTE_OFFSET 0x178ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER15_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER16_BYTE_OFFSET 0x180ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER16_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER17_BYTE_OFFSET 0x188ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER17_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER18_BYTE_OFFSET 0x190ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER18_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER19_BYTE_OFFSET 0x198ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER19_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER20_BYTE_OFFSET 0x1A0ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER20_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER21_BYTE_OFFSET 0x1A8ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER21_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER22_BYTE_OFFSET 0x1B0ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER22_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER23_BYTE_OFFSET 0x1B8ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER23_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER24_BYTE_OFFSET 0x1C0ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER24_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER25_BYTE_OFFSET 0x1C8ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER25_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER26_BYTE_OFFSET 0x1D0ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER26_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER27_BYTE_OFFSET 0x1D8ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER27_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER28_BYTE_OFFSET 0x1E0ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER28_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER29_BYTE_OFFSET 0x1E8ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER29_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER30_BYTE_OFFSET 0x1F0ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER30_PROT        PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER31_BYTE_OFFSET 0x1F8ULL
#define ESR_SHIRE_FAST_LOCAL_BARRIER31_PROT        PRV_U

/* ---- thread and shire identifiers ---------------------------- */
/* (THIS_SHIRE already lives in <erbium/isa/hart.h> == 0.) */

#define THREAD_0 0
#define THREAD_1 1

#ifdef __cplusplus
}
#endif

#endif /* _ERBIUM_ISA_ESR_DEFINES_H_ */

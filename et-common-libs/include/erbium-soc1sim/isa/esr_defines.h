/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/*
 * Erbium-on-etsoc1 (soc1sim) ESR address layout.
 *
 * TODO: THIS FILE IS TEMPORARY. Once the erbium HAL exposes clean
 *       U-mode ESR access primitives we should drop these hand-picked
 *       defines in favor of pulling from there. Keep additions
 *       strictly scoped to the registers the simulator layer needs.
 *
 * The soc1sim backend runs on actual etsoc1 hardware, so the ESR
 * addressing follows the etsoc scheme (not the erbium one):
 *
 *     [39:32]  region marker                (ESR_REGION == bit 32)
 *     [31:30]  PP (privilege)
 *     [29:22]  shire id
 *     [21:20]  sub-region (00=HART, 01=NEIGH, 11=CACHE/RBOX/SHIRE/...)
 *     [19:3]   sub-region-specific (hart, neigh, regno<<3, ...)
 *
 * The erbium abstraction presents a single shire with id 0, but the
 * etsoc RTL has an "own shire" marker (0xFF) that routes any ESR
 * access back to whichever shire the writer is on — which is what
 * we want: a payload kernel running on shire N hits its own shire's
 * ESRs whenever it talks to them. So ESR_SHIRE(shire, NAME) here
 * deliberately IGNORES its `shire` argument and always encodes
 * 0xFF. The payload keeps writing `ESR_SHIRE(THIS_SHIRE, NAME)`
 * (with THIS_SHIRE == 0 from <erbium/isa/hart.h>), and the RTL
 * still sees "local shire".
 */

#ifndef _ERBIUM_SOC1SIM_ISA_ESR_DEFINES_H_
#define _ERBIUM_SOC1SIM_ISA_ESR_DEFINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLER__
#include <inttypes.h>
#endif

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

/* ---- etsoc ESR address composition --------------------------- */

#define ESR_REGION             0x0100000000ULL
#define ESR_SHIRE_REGION       0x0100340000ULL

#define ESR_REGION_PROT_SHIFT  30
#define ESR_REGION_SHIRE_SHIFT 22

/* etsoc's "this shire" marker — the RTL routes any ESR whose shire
 * field equals 0xFF back to the writer's own shire. */
#define SHIRE_OWN              0xFFULL

/* ESR_SHIRE(shire, NAME) is the drop-in API used by existing FCC /
 * FLB / barrier code. On soc1sim we deliberately drop the `shire`
 * argument and hard-code SHIRE_OWN so the abstraction-level
 * `THIS_SHIRE == 0` stays consistent with hart.h while the RTL sees
 * a local-shire access. */
#define ESR_SHIRE(shire, name)                                            \
    ((ESR_SHIRE_REGION) |                                                 \
     ((uint64_t)(ESR_SHIRE_##name##_PROT) << ESR_REGION_PROT_SHIFT) |     \
     (SHIRE_OWN << ESR_REGION_SHIRE_SHIFT) |                              \
     ((uint64_t)(ESR_SHIRE_##name##_REGNO) << 3))

/* ---- FCC credit-counter registers ---------------------------- */
/* Regno << 3 == byte offset, identical to erbium: 0xC0..0xD8. */

#define ESR_SHIRE_FCC_CREDINC_0_REGNO 0x18
#define ESR_SHIRE_FCC_CREDINC_0_PROT  PRV_U

#define ESR_SHIRE_FCC_CREDINC_1_REGNO 0x19
#define ESR_SHIRE_FCC_CREDINC_1_PROT  PRV_U

#define ESR_SHIRE_FCC_CREDINC_2_REGNO 0x1a
#define ESR_SHIRE_FCC_CREDINC_2_PROT  PRV_U

#define ESR_SHIRE_FCC_CREDINC_3_REGNO 0x1b
#define ESR_SHIRE_FCC_CREDINC_3_PROT  PRV_U

/* ---- Fast Local Barriers (FLBs) ------------------------------ */
/* Regnos 0x20..0x3f -> byte offsets 0x100..0x1F8. */

#define ESR_SHIRE_FAST_LOCAL_BARRIER0_REGNO  0x20
#define ESR_SHIRE_FAST_LOCAL_BARRIER0_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER1_REGNO  0x21
#define ESR_SHIRE_FAST_LOCAL_BARRIER1_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER2_REGNO  0x22
#define ESR_SHIRE_FAST_LOCAL_BARRIER2_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER3_REGNO  0x23
#define ESR_SHIRE_FAST_LOCAL_BARRIER3_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER4_REGNO  0x24
#define ESR_SHIRE_FAST_LOCAL_BARRIER4_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER5_REGNO  0x25
#define ESR_SHIRE_FAST_LOCAL_BARRIER5_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER6_REGNO  0x26
#define ESR_SHIRE_FAST_LOCAL_BARRIER6_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER7_REGNO  0x27
#define ESR_SHIRE_FAST_LOCAL_BARRIER7_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER8_REGNO  0x28
#define ESR_SHIRE_FAST_LOCAL_BARRIER8_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER9_REGNO  0x29
#define ESR_SHIRE_FAST_LOCAL_BARRIER9_PROT   PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER10_REGNO 0x2a
#define ESR_SHIRE_FAST_LOCAL_BARRIER10_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER11_REGNO 0x2b
#define ESR_SHIRE_FAST_LOCAL_BARRIER11_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER12_REGNO 0x2c
#define ESR_SHIRE_FAST_LOCAL_BARRIER12_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER13_REGNO 0x2d
#define ESR_SHIRE_FAST_LOCAL_BARRIER13_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER14_REGNO 0x2e
#define ESR_SHIRE_FAST_LOCAL_BARRIER14_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER15_REGNO 0x2f
#define ESR_SHIRE_FAST_LOCAL_BARRIER15_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER16_REGNO 0x30
#define ESR_SHIRE_FAST_LOCAL_BARRIER16_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER17_REGNO 0x31
#define ESR_SHIRE_FAST_LOCAL_BARRIER17_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER18_REGNO 0x32
#define ESR_SHIRE_FAST_LOCAL_BARRIER18_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER19_REGNO 0x33
#define ESR_SHIRE_FAST_LOCAL_BARRIER19_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER20_REGNO 0x34
#define ESR_SHIRE_FAST_LOCAL_BARRIER20_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER21_REGNO 0x35
#define ESR_SHIRE_FAST_LOCAL_BARRIER21_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER22_REGNO 0x36
#define ESR_SHIRE_FAST_LOCAL_BARRIER22_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER23_REGNO 0x37
#define ESR_SHIRE_FAST_LOCAL_BARRIER23_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER24_REGNO 0x38
#define ESR_SHIRE_FAST_LOCAL_BARRIER24_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER25_REGNO 0x39
#define ESR_SHIRE_FAST_LOCAL_BARRIER25_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER26_REGNO 0x3a
#define ESR_SHIRE_FAST_LOCAL_BARRIER26_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER27_REGNO 0x3b
#define ESR_SHIRE_FAST_LOCAL_BARRIER27_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER28_REGNO 0x3c
#define ESR_SHIRE_FAST_LOCAL_BARRIER28_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER29_REGNO 0x3d
#define ESR_SHIRE_FAST_LOCAL_BARRIER29_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER30_REGNO 0x3e
#define ESR_SHIRE_FAST_LOCAL_BARRIER30_PROT  PRV_U
#define ESR_SHIRE_FAST_LOCAL_BARRIER31_REGNO 0x3f
#define ESR_SHIRE_FAST_LOCAL_BARRIER31_PROT  PRV_U

/* ---- thread identifiers -------------------------------------- */
/* (THIS_SHIRE already lives in <erbium/isa/hart.h> == 0.) */

#define THREAD_0 0
#define THREAD_1 1

#ifdef __cplusplus
}
#endif

#endif /* _ERBIUM_SOC1SIM_ISA_ESR_DEFINES_H_ */

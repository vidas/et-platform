/***********************************************************************/
/*! \copyright
  Copyright (c) 2026 Ainekko, Co.
  SPDX-License-Identifier: Apache-2.0
*/
/***********************************************************************/
/*! \file layout.h
    \brief Erbium memory-layout description for user-mode kernels.

    Exposes a NULL-terminated list of heap regions available to the
    kernel (anything past the stack, up to the end of MRAM). A
    user-supplied allocator can walk `__heap_regions` to discover
    memory it is allowed to use; the backend (erbium vs
    erbium-soc1sim) decides how the list is populated.

*/
/***********************************************************************/

#ifndef _ERBIUM_SOC1SIM_ISA_LAYOUT_H_
#define _ERBIUM_SOC1SIM_ISA_LAYOUT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *start;
    void *end;
} heap_region_t;

/* Byte size of a region. Defined as a macro so a NULL-sentinel entry
 * evaluates to 0 without a division or other UB. */
#define heap_region_size(r)  ((size_t)((const uint8_t *)(r)->end - \
                                       (const uint8_t *)(r)->start))

/* NULL-terminated list of heap regions available to user code.
   The final entry { NULL, NULL } is the sentinel. */
extern const heap_region_t __heap_regions[];

#ifdef __cplusplus
}
#endif

#endif /* _ERBIUM_SOC1SIM_ISA_LAYOUT_H_ */

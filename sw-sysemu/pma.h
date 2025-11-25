/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#ifndef BEMU_PMA_H
#define BEMU_PMA_H

#include "emu_defines.h"
#include "processor.h"

namespace bemu {

uint64_t pma_check_data_access(const Hart& cpu, uint64_t vaddr,
                                      uint64_t addr, size_t size,
                                      mem_access_type macc,
                                      mreg_t mask = mreg_t(-1),
                                      cacheop_type cop = CacheOp_None);

uint64_t pma_check_fetch_access(const Hart& cpu, uint64_t vaddr,
                                       uint64_t addr, size_t size);

uint64_t pma_check_ptw_access(const Hart& cpu, uint64_t vaddr,
                                     uint64_t addr, mem_access_type macc);

} // namespace bemu

#endif // BEMU_PMA_H

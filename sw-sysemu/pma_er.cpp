/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#include "processor.h"

namespace bemu {

uint64_t pma_check_data_access(const Hart& cpu, uint64_t vaddr,
                                      uint64_t addr, size_t size,
                                      mem_access_type macc,
                                      mreg_t mask = mreg_t(-1),
                                      cacheop_type cop = CacheOp_None)
{
  (void)cpu; (void)vaddr; (void)size; (void)macc; (void)mask; (void)cop;
  // TODO
  return addr;
}


uint64_t pma_check_fetch_access(const Hart& cpu, uint64_t vaddr,
                                       uint64_t addr, size_t size)
{  
  (void)cpu; (void)vaddr; (void)size;
  // TODO
  return addr;
}

uint64_t pma_check_ptw_access(const Hart& cpu, uint64_t vaddr,
                                     uint64_t addr, mem_access_type macc)
{
  (void)cpu; (void)vaddr; (void)macc;
  // TODO
  return addr;
}

} // namespace bemu

/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#include "emu_defines.h"
#include "devices/sysregs_er.h"
#include "system.h"
#include "memory/sysreg_region.h"
#include "memory/dense_region.h"

namespace bemu {

void MainMemory::reset()
{
    size_t pos = 0;

    regions[pos++].reset(new SysregsEr<erbreg_base>());
    regions[pos++].reset(new DenseRegion<bootrom_base, 8_KiB, false>());
    regions[pos++].reset(new DenseRegion<sram_base, 2_KiB>());
    regions[pos++].reset(new DenseRegion<dram_base, 16_MiB>());
    // TODO:
    // regions[pos++].reset(new SysregRegion<sysreg_base, 16_MiB>());
}

void MainMemory::wdt_clock_tick(const Agent& agent, uint64_t cycle)
{
    auto ptr = dynamic_cast<SysregsEr<erbreg_base>*>(regions[0].get());
    ptr->wdt_clock_tick(agent, cycle);
}

} // namespace bemu

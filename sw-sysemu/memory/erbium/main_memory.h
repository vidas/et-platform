/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#ifndef BEMU_MAIN_MEMORY_H
#define BEMU_MAIN_MEMORY_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "agent.h"
#include "literals.h"
#include "memory/memory_error.h"
#include "memory/memory_region.h"

namespace bemu {


// Erbium Memory Map
//
// +---------------------------------+----------+-------------------+
// |      Address range (hex)        |          |                   |
// |      From      |      To        |   Size   | Maps to           |
// +----------------+----------------+----------+-------------------+
// | 0x00_0200_0000 | 0x00_0200_0FFF |  4KiB    | SystemRegisters   |
// | 0x00_0200_A000 | 0x00_0200_BFFF |  8KiB    | Boot ROM          |
// | 0x00_0200_E000 | 0x00_0200_E7FF |  2KiB    | Scratch SRAM      |
// | 0x00_4000_0000 | 0x00_40FF_FFFF | 16MiB    | MRAM              |
// | 0x00_8000_0000 | 0x00_80FF_FFFF | 16MiB    | ESR Registers     |
// | 0x00_8100_0000 | 0x00_81FF_FFFF | 16MiB    | PLIC              | <- to be changed
// +----------------+----------------+----------+-------------------+
//

struct MainMemory {
    using addr_type     = typename MemoryRegion::addr_type;
    using size_type     = typename MemoryRegion::size_type;
    using value_type    = typename MemoryRegion::value_type;
    using pointer       = typename MemoryRegion::pointer;
    using const_pointer = typename MemoryRegion::const_pointer;

    // ----- Types -----

    enum : unsigned long long {
        // base addresses for the various regions of the address space
        erbreg_base  = 0x0002000000ULL,
        bootrom_base = 0x000200A000ULL,
        sram_base    = 0x000200E000ULL,
        dram_base    = 0x0040000000ULL, /* Actually MRAM */
        sysreg_base  = 0x0080000000ULL,
    };

    // ----- Public methods -----

    void reset();

    void read(const Agent& agent, addr_type addr, size_type n, void* result) {
        const auto elem = search(addr, n);
        elem->read(agent, addr - elem->first(), n, reinterpret_cast<pointer>(result));
    }

    void write(const Agent& agent, addr_type addr, size_type n, const void* source) {
        auto elem = search(addr, n);
        elem->write(agent, addr - elem->first(), n, reinterpret_cast<const_pointer>(source));
    }

    void init(const Agent& agent, addr_type addr, size_type n, const void* source) {
        auto elem = search(addr, n);
        elem->init(agent, addr - elem->first(), n, reinterpret_cast<const_pointer>(source));
    }

    addr_type first() const { return regions.front()->first(); }
    addr_type last() const { return regions.back()->last(); }

    void dump_data(const Agent& agent, std::ostream& os, addr_type addr, size_type n) const {
        auto lo = std::lower_bound(regions.cbegin(), regions.cend(), addr, above);
        if ((lo == regions.cend()) || ((*lo)->first() > addr))
            throw std::out_of_range("bemu::MainMemory::dump_data()");
        auto hi = std::lower_bound(regions.cbegin(), regions.cend(), addr+n-1, above);
        if (hi == regions.cend())
            throw std::out_of_range("bemu::MainMemory::dump_data()");
        size_type pos = addr - (*lo)->first();
        while (lo != hi) {
            (*lo)->dump_data(agent, os, pos, (*lo)->last() - (*lo)->first() - pos + 1);
            ++lo;
            pos = 0;
        }
        (*lo)->dump_data(agent, os, pos, addr + n - (*lo)->first() - pos);
    }

    void wdt_clock_tick(const Agent& agent, uint64_t cycle);

protected:
    static inline bool above(const std::unique_ptr<MemoryRegion>& lhs, addr_type rhs) {
        return lhs->last() < rhs;
    }

    MemoryRegion* search(addr_type addr, size_type n) const {
        auto lo = std::lower_bound(regions.cbegin(), regions.cend(), addr, above);
        if ((lo == regions.cend()) || ((*lo)->first() > addr))
            throw memory_error(addr);
        if (addr+n-1 > (*lo)->last())
            throw std::out_of_range("bemu::MainMemory::search()");
        return lo->get();
    }

    // This array must be sorted by region base address
    std::array<std::unique_ptr<MemoryRegion>, 4> regions{};
};


} // namespace bemu

#endif // BEMU_MAIN_MEMORY_H

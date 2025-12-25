/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#include <cinttypes>

#include "pma.h"
#include "mmu.h"
#include "esrs.h"
#include "system.h"

namespace bemu {

//------------------------------------------------------------------------------
// Erbium memory region address helpers
//
//   0x0200_0000 - 0x0200_0FFF: System registers (4K)
//   0x0200_1000 - 0x0200_1FFF: MRAM registers (4K)
//   0x0200_2000 - 0x0200_2FFF: Periph registers (4K)
//   0x0200_3000 - 0x0200_3FFF: Hyperbus registers (4K)
//   0x0200_A000 - 0x0200_BFFF: Bootrom (8K)
//   0x0200_E000 - 0x0200_E7FF: Scratch SRAM (2K)
//   0x4000_0000 - 0x7FFF_FFFF: MRAM (1G, but only 16MB installed)
//   0x8000_0000 - 0xBFFF_FFFF: ESR/CPU registers (1G)
//   0xC000_0000 - 0xC3FF_FFFF: PLIC (64M)

static inline bool paddr_is_sysreg(uint64_t addr)
{ return (addr >= 0x0200'0000ull) && (addr < 0x0200'1000ull); }

static inline bool paddr_is_mramreg(uint64_t addr)
{ return (addr >= 0x0200'1000ull) && (addr < 0x0200'2000ull); }

static inline bool paddr_is_periph(uint64_t addr)
{ return (addr >= 0x0200'2000ull) && (addr < 0x0200'3000ull); }

static inline bool paddr_is_hyperbus(uint64_t addr)
{ return (addr >= 0x0200'3000ull) && (addr < 0x0200'4000ull); }

static inline bool paddr_is_bootrom(uint64_t addr)
{ return (addr >= 0x0200'A000ull) && (addr < 0x0200'C000ull); }

static inline bool paddr_is_sram(uint64_t addr)
{ return (addr >= 0x0200'E000ull) && (addr < 0x0200'E800ull); }

static inline bool paddr_is_mram(uint64_t addr)
{ return (addr >= 0x4000'0000ull) && (addr < 0x8000'0000ull); }

static inline bool paddr_is_esr(uint64_t addr)
{ return (addr >= 0x8000'0000ull) && (addr < 0xC000'0000ull); }

static inline bool paddr_is_plic(uint64_t addr)
{ return (addr >= 0xC000'0000ull) && (addr < 0xC400'0000ull); }


static bool data_access_is_write(mem_access_type macc)
{
    switch (macc)
    {
    case Mem_Access_Load:
    case Mem_Access_LoadL:
    case Mem_Access_LoadG:
    case Mem_Access_Fetch:
    case Mem_Access_TxLoad:
    case Mem_Access_TxLoadL2Scp:
    case Mem_Access_Prefetch:
        return false;
    case Mem_Access_Store:
    case Mem_Access_StoreL:
    case Mem_Access_StoreG:
    case Mem_Access_PTW:
    case Mem_Access_AtomicL:
    case Mem_Access_AtomicG:
    case Mem_Access_TxStore:
    case Mem_Access_CacheOp:
        return true;
    }
    throw std::invalid_argument("data_access_is_write()");
}


//------------------------------------------------------------------------------
// PP (privilege protection) bits extraction from ESR address
// Bits [23:22] encode minimum privilege level required

#define PP(x) (int(((x) & ESR_REGION_PROT_MASK) >> ESR_REGION_PROT_SHIFT))


//------------------------------------------------------------------------------
// PMA check functions

uint64_t pma_check_data_access(const Hart& cpu, uint64_t vaddr,
                               uint64_t addr, size_t size,
                               mem_access_type macc,
                               mreg_t mask,
                               cacheop_type cop)
{
    (void)cpu; (void)size; (void)mask; (void)cop;

    if (paddr_is_mram(addr))
        return addr;

    if (paddr_is_esr(addr))
        return addr;

    if (paddr_is_plic(addr))
        return addr;

    if (paddr_is_bootrom(addr))
        return addr;

    if (paddr_is_sram(addr))
        return addr;

    if (paddr_is_sysreg(addr))
        return addr;

    if (paddr_is_mramreg(addr))
        return addr;

    if (paddr_is_periph(addr))
        return addr;

    if (paddr_is_hyperbus(addr))
        return addr;

    // Unknown/reserved region - access fault
    throw_access_fault(vaddr, macc);
}


uint64_t pma_check_fetch_access(const Hart& cpu, uint64_t vaddr,
                                uint64_t addr, size_t size)
{
    (void)cpu; (void)vaddr; (void)size;
    return addr;
}


uint64_t pma_check_ptw_access(const Hart& cpu, uint64_t vaddr,
                              uint64_t addr, mem_access_type macc)
{
    (void)cpu; (void)vaddr; (void)macc;
    return addr;
}

} // namespace bemu

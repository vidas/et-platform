/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#include <cinttypes>

#include "pma.h"
#include "mmu.h"
#include "memmap.h"
#include "emu_gio.h"
#include "system.h"
#include "utility.h"
#include "insn_util.h"
#ifdef SYS_EMU
#include "sys_emu.h"
#include "checkers/mem_checker.h"
#endif

#ifdef SYS_EMU
#define SYS_EMU_PTR cpu.chip->emu()
#endif

namespace bemu {


//------------------------------------------------------------------------------
// PMA checks

#define MPROT_IO_ACCESS_MODE(x)    ((x) & 0x3)
#define MPROT_DISABLE_PCIE_ACCESS  0x04
#define MPROT_DISABLE_OSBOX_ACCESS 0x08
#define MPROT_DRAM_SIZE_8G         0x00
#define MPROT_DRAM_SIZE_16G        0x10
#define MPROT_DRAM_SIZE_24G        0x20
#define MPROT_DRAM_SIZE_32G        0x30
#define MPROT_DRAM_SIZE(x)         ((x) & 0x30)
#define MPROT_ENABLE_SECURE_MEMORY 0x40

#define PP(x)   (int(((x) & ESR_REGION_PROT_MASK) >> ESR_REGION_PROT_SHIFT))


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

static inline bool paddr_is_sp_cacheable(uint64_t addr)
{ return paddr_is_sp_rom(addr) || paddr_is_sp_sram(addr); }


static inline uint64_t truncated_dram_addr(const Hart& cpu, uint64_t addr)
{
    const uint64_t dram_size = cpu.chip->dram_size;
    const uint64_t naddr = 0x8000000000ULL + ((addr - 0x8000000000ULL) % dram_size);
    if (naddr != addr) {
        WARN_HART(memory, cpu, "Truncating DRAM address: %010lx => %010lx", addr, naddr);
    }
    return naddr;
}


// Convert between the two Minion L2SCP views to the NoC view
static inline uint64_t normalize_scratchpad_address(const Hart& cpu, uint64_t addr)
{
#if EMU_HAS_L2
    // Convert between format 0 and format 1 to internal format
    addr -= L2_SCP_BASE;
    if (addr >= 1_GiB) {
        addr = ( (addr         & ~0x4fffffc0ull) |
                 ((addr <<  1) &  0x40000000ull) |
                 ((addr << 17) &  0x0f800000ull) |
                 ((addr >>  5) &  0x007fffc0ull) );
    }
    addr |= ((addr << 1) & 0x40000000ull);
    addr += L2_SCP_BASE;
    // Replace local shire with proper shire number
    if ((addr & (255ull << 23)) == (255ull << 23)) {
        uint64_t shire = cpu.shireid();
        if (shireid_is_ioshire(shire)) {
            throw memory_error(addr);
        }
        addr = (addr & ~(255ull << 23)) | (shire << 23);
    }
    return addr;
#else
    (void)cpu;
    return addr;
#endif
}

static inline uint64_t pma_dram_limit(bool spio, uint8_t mprot)
{
    if (spio) {
        return 0x8000000000ULL + 32_GiB;
    }
    switch (MPROT_DRAM_SIZE(mprot)) {
    case MPROT_DRAM_SIZE_8G : return 0x8000000000ULL +  8_GiB;
    case MPROT_DRAM_SIZE_16G: return 0x8000000000ULL + 16_GiB;
    case MPROT_DRAM_SIZE_24G: return 0x8000000000ULL + 24_GiB;
    case MPROT_DRAM_SIZE_32G: return 0x8000000000ULL + 32_GiB;
    }
    throw std::runtime_error("Illegal mprot.dram_size value");
}


uint64_t pma_check_data_access(const Hart& cpu, uint64_t vaddr,
                                      uint64_t addr, size_t size,
                                      mem_access_type macc,
                                      mreg_t mask,
                                      cacheop_type cop)
{
#ifndef SYS_EMU
    (void) cop;
    (void) mask;
#endif
    bool spio     = hartid_is_svcproc(cpu.mhartid);
    bool amo      = (macc == Mem_Access_AtomicL) || (macc == Mem_Access_AtomicG);
    bool amo_l    = (macc == Mem_Access_AtomicL);
    bool ts_tl_co = (macc >= Mem_Access_TxLoad) && (macc <= Mem_Access_CacheOp);

    if (paddr_is_dram(addr)) {

        if (paddr_is_dram_uncacheable(addr)) {
            if (!spio || !addr_is_size_aligned(addr, size)) {
                throw_access_fault(vaddr, macc);
            }
            if (amo) {
                // NB: This is because we go directly to the memory controller
                // which does not support atomics; but the PMA does not catch
                // that, so the SP will get a bus error response eventually.
                // Since we do not model the NoC to all its detail we need to
                // put this check here.
                throw memory_error(addr);
            }
            if (ts_tl_co) {
                if (cpu.chip->stepping == System::Stepping::A0) {
                    // NB: On ET-SoC-1 A0 the PMA does not catch this case
                    // which leads to undefined behavior.
                    WARN_HART(cacheops, cpu, "CacheOp to uncacheable addr 0x%016"
                             PRIx64 " is UNDEFINED behavior", vaddr);
                }
                throw_access_fault(vaddr, macc);
            }
            // NB: The high-to-low conversion of the DRAM address happens in
            // the ETL2AXI bridge but we do not model this device, so we do
            // the conversion here.
            addr &= ~0x4000000000ULL;
        }

        if (!spio && !addr_is_size_aligned(addr, size)) {
            // when data cache is in bypass mode all accesses should be aligned
            uint8_t ctrl = cpu.chip->neigh_esrs[neigh_index(cpu)].neigh_chicken;
            if (ctrl & 0x2)
                throw_access_fault(vaddr, macc);
        }

        uint8_t mprot = cpu.chip->neigh_esrs[neigh_index(cpu)].mprot;

        if (mprot & MPROT_ENABLE_SECURE_MEMORY) {
            if (paddr_is_dram_mcode(addr)) {
                if (!spio && (data_access_is_write(macc)
                              || (effective_execution_mode(cpu, macc) != Privilege::M)))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_mdata(addr)) {
                if (!spio && (effective_execution_mode(cpu, macc) != Privilege::M))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_scode(addr)) {
                if (!spio && (data_access_is_write(macc)
                              ? (effective_execution_mode(cpu, macc) != Privilege::M)
                              : (effective_execution_mode(cpu, macc) == Privilege::U)))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_sdata(addr)) {
                if (!spio && (effective_execution_mode(cpu, macc) == Privilege::U))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_osbox(addr)) {
                if (!spio && (mprot & MPROT_DISABLE_OSBOX_ACCESS))
                    throw_access_fault(vaddr, macc);
            }
            else if (addr >= pma_dram_limit(spio, mprot)) {
                throw_access_fault(vaddr, macc);
            }
        } else {
            if (paddr_is_dram_mbox(addr)) {
                if (!spio && (effective_execution_mode(cpu, macc) != Privilege::M))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_sbox(addr)) {
                if (!spio && (mprot & MPROT_DISABLE_OSBOX_ACCESS))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_osbox(addr)) {
                if (!spio && (mprot & MPROT_DISABLE_OSBOX_ACCESS))
                    throw_access_fault(vaddr, macc);
            }
            else if (addr >= pma_dram_limit(spio, mprot)) {
                throw_access_fault(vaddr, macc);
            }
        }
#ifdef SYS_EMU
        if (SYS_EMU_PTR->get_mem_check()) {
            SYS_EMU_PTR->get_mem_checker().access(cpu.pc, addr, macc, cop, hart_index(cpu), size, mask);
        }
#endif
#ifdef SMB_SIZE
        if (((addr + size) > uint64_t(SMB_ADDR)) && (addr < (uint64_t(SMB_ADDR) + uint64_t(SMB_SIZE)))) {
            WARN_HART(memory, cpu, "%s SMB-reserved addr 0x%" PRIx64,
                     data_access_is_write(macc) ? "Writing to" : "Reading from",
                     std::max(addr, uint64_t(SMB_ADDR)));
        }
#endif
        // NB: The memory controller truncates addresses, but since we do not
        // model it we need to do the truncation here.
        return truncated_dram_addr(cpu, addr);
    }

    if (paddr_is_scratchpad(addr)) {
        if (amo_l) {
            throw_access_fault(vaddr, macc);
        }
        // NB: This address transformation occurs in the Minion neighborhood,
        // but since we do not model the neighborhood as a separate device we
        // do the transformation here.
        if (!spio) {
            addr = normalize_scratchpad_address(cpu, addr);
        }
#ifdef SYS_EMU
        if (SYS_EMU_PTR->get_mem_check()) {
            SYS_EMU_PTR->get_mem_checker().access(cpu.pc, addr, macc, cop, hart_index(cpu), size, mask);
        }
        if (SYS_EMU_PTR->get_l2_scp_check()) {
            SYS_EMU_PTR->get_l2_scp_checker().l2_scp_read(hart_index(cpu), addr);
        }
#endif
        return addr;
    }

    if (paddr_is_esr_space(addr)) {
        if (amo
            || ts_tl_co
            || (size != 8)
            || !addr_is_size_aligned(addr, size)
            || (PP(addr) > static_cast<int>(effective_execution_mode(cpu, macc)))
            || (PP(addr) == 2 && !spio))
            throw_access_fault(vaddr, macc);
        return addr;
    }

    if (paddr_is_sp_space(addr)) {
        Privilege mode = effective_execution_mode(cpu, macc);
        if (!spio
            || amo
            || (ts_tl_co && !paddr_is_sp_cacheable(addr))
            || (paddr_is_sp_sram_code(addr) && data_access_is_write(macc) && (mode != Privilege::M))
            || (paddr_is_sp_sram_data(addr) && (mode == Privilege::U))
            || (paddr_is_sp_misc(addr) && (mode != Privilege::M))
            || (!paddr_is_sp_cacheable(addr) && !addr_is_size_aligned(addr, size)))
            throw_access_fault(vaddr, macc);
        return addr;
    }

    if (paddr_is_io_space(addr)) {
        int io_mode = MPROT_IO_ACCESS_MODE(cpu.chip->neigh_esrs[neigh_index(cpu)].mprot);
        if (amo
            || ts_tl_co
            || !addr_is_size_aligned(addr, size)
            || (!spio && ((io_mode == 0x2)
                          || (static_cast<int>(effective_execution_mode(cpu, macc)) < io_mode))))
            throw_access_fault(vaddr, macc);
        return addr;
    }

    if (paddr_is_pcie_space(addr)) {
        int pcie_no_access = cpu.chip->neigh_esrs[neigh_index(cpu)].mprot & MPROT_DISABLE_PCIE_ACCESS;
        if (amo
            || ts_tl_co
            || !addr_is_size_aligned(addr, size)
            || (!spio && pcie_no_access))
            throw_access_fault(vaddr, macc);
        return addr;
    }

    throw_access_fault(vaddr, macc);
}


uint64_t pma_check_fetch_access(const Hart& cpu, uint64_t vaddr,
                                       uint64_t addr, size_t size)
{
#ifndef SMB_SIZE
    (void) size;
#endif
    bool spio = hartid_is_svcproc(cpu.mhartid);

    if (paddr_is_dram(addr)) {
        if (spio || paddr_is_dram_uncacheable(addr))
            throw_access_fault(vaddr, Mem_Access_Fetch);

        uint8_t mprot = cpu.chip->neigh_esrs[neigh_index(cpu)].mprot;

        if (mprot & MPROT_ENABLE_SECURE_MEMORY) {
            if (paddr_is_dram_mcode(addr)) {
                if (effective_execution_mode(cpu, Mem_Access_Fetch) != Privilege::M)
                    throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (paddr_is_dram_mdata(addr)) {
                throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (paddr_is_dram_scode(addr)) {
                if (effective_execution_mode(cpu, Mem_Access_Fetch) != Privilege::S)
                    throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (paddr_is_dram_sdata(addr)) {
                throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (paddr_is_dram_osbox(addr)) {
                if ((mprot & MPROT_DISABLE_OSBOX_ACCESS) ||
                    (effective_execution_mode(cpu, Mem_Access_Fetch) != Privilege::U))
                    throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if ((addr >= pma_dram_limit(spio, mprot)) ||
                     (effective_execution_mode(cpu, Mem_Access_Fetch) != Privilege::U)) {
                throw_access_fault(vaddr, Mem_Access_Fetch);
            }
        } else {
            if (paddr_is_dram_mbox(addr)) {
                if (effective_execution_mode(cpu, Mem_Access_Fetch) != Privilege::M)
                    throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (paddr_is_dram_sbox(addr)) {
                if (mprot & MPROT_DISABLE_OSBOX_ACCESS)
                    throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (paddr_is_dram_osbox(addr)) {
                if (mprot & MPROT_DISABLE_OSBOX_ACCESS)
                    throw_access_fault(vaddr, Mem_Access_Fetch);
            }
            else if (addr >= pma_dram_limit(spio, mprot)) {
                throw_access_fault(vaddr, Mem_Access_Fetch);
            }
        }
#ifdef SYS_EMU
        if (SYS_EMU_PTR->get_mem_check()) {
            SYS_EMU_PTR->get_mem_checker().access(cpu.pc, addr, Mem_Access_Fetch, CacheOp_None, hart_index(cpu), 64, mreg_t(-1));
        }
#endif
#ifdef SMB_SIZE
        if (((addr + size) > uint64_t(SMB_ADDR)) && (addr < (uint64_t(SMB_ADDR) + uint64_t(SMB_SIZE)))) {
            WARN_HART(memory, cpu, "Fetching from SMB-reserved addr 0x%" PRIx64,
                     std::max(addr, uint64_t(SMB_ADDR)));
        }
#endif
        // NB: The memory controller truncates addresses, but since we do not
        // model it we need to do the truncation here.
        return truncated_dram_addr(cpu, addr);
    }

    if (paddr_is_sp_rom(addr)) {
        if (!spio)
            throw_access_fault(vaddr, Mem_Access_Fetch);
        return addr;
    }

    if (paddr_is_sp_sram_code(addr)) {
        if (!spio || (effective_execution_mode(cpu, Mem_Access_Fetch) == Privilege::U))
            throw_access_fault(vaddr, Mem_Access_Fetch);
        return addr;
    }

    if (paddr_is_sp_sram_data(addr)) {
        if (!spio || (effective_execution_mode(cpu, Mem_Access_Fetch) != Privilege::M))
            throw_access_fault(vaddr, Mem_Access_Fetch);
        return addr;
    }

    throw_access_fault(vaddr, Mem_Access_Fetch);
}


uint64_t pma_check_ptw_access(const Hart& cpu, uint64_t vaddr,
                                     uint64_t addr, mem_access_type macc)
{
    bool spio = hartid_is_svcproc(cpu.mhartid);

    if (paddr_is_dram(addr)) {
        uint8_t mprot = cpu.chip->neigh_esrs[neigh_index(cpu)].mprot;

        if (spio)
            addr &= ~0x4000000000ULL;

        if (mprot & MPROT_ENABLE_SECURE_MEMORY) {
            if (paddr_is_dram_mcode(addr)) {
                if (!spio)
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_mdata(addr)) {
                if (!spio && (effective_execution_mode(cpu, macc) != Privilege::M))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_scode(addr)) {
                if (!spio && (effective_execution_mode(cpu, macc) != Privilege::M))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_osbox(addr)) {
                if (!spio && (mprot & MPROT_DISABLE_OSBOX_ACCESS))
                    throw_access_fault(vaddr, macc);
            }
            else if (addr >= pma_dram_limit(spio, mprot)) {
                throw_access_fault(vaddr, macc);
            }
        } else {
            if (paddr_is_dram_mbox(addr)) {
                if (!spio && (effective_execution_mode(cpu, macc) != Privilege::M))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_sbox(addr)) {
                if (!spio && (mprot & MPROT_DISABLE_OSBOX_ACCESS))
                    throw_access_fault(vaddr, macc);
            }
            else if (paddr_is_dram_osbox(addr)) {
                if (!spio && (mprot & MPROT_DISABLE_OSBOX_ACCESS))
                    throw_access_fault(vaddr, macc);
            }
            else if (addr >= pma_dram_limit(spio, mprot)) {
                throw_access_fault(vaddr, macc);
            }
        }
        return truncated_dram_addr(cpu, addr);
    }

    if (paddr_is_sp_rom(addr) || paddr_is_sp_sram(addr)) {
        if (!spio || paddr_is_sp_sram_code(addr))
            throw_access_fault(vaddr, macc);
        return addr;
    }

    throw_access_fault(vaddr, macc);
}

} // namespace bemu

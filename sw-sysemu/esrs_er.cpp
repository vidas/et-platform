/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

#include "emu_defines.h"

#include <array>
#include <cassert>
#include <stdexcept>

#include "emu_gio.h"
#include "esrs.h"
#include "sysreg_error.h"
#include "system.h"
#include "memory/memory_error.h"
#ifdef SYS_EMU
#include "checkers/mem_checker.h"
#include "sys_emu.h"

#define SYS_EMU_PTR agent.chip->emu()
#endif

namespace bemu {

// ESR region 'hart' field in bits [19:12] (when 'subregion' is 2'b00)
#define ESR_REGION_HART_MASK    0x00'000F'F000ull
#define ESR_REGION_HART_SHIFT   12

// ESR region 'neighborhood' field in bits [19:16] (when 'subregion' is 2'b01)
// The broadcast neighborhood has bits [19:16] == 4'b1111
#define ESR_REGION_NEIGH_MASK   0x00'000F'0000ull
#define ESR_REGION_NEIGH_SHIFT  16

// ESR region 'shireid' field in bits [30:24]
#define ESR_REGION_SHIRE_MASK   0x00'7F00'0000ull
#define ESR_REGION_SHIRE_SHIFT  24

// ESR region 'ESR' field masks
#define ESR_HART_ESR_MASK       0xFF'80F0'0FFFull
#define ESR_NEIGH_ESR_MASK      0xFF'80F0'FFFFull
#define ESR_SHIRE_ESR_MASK      0xFF'80FF'FFFFull

// Subregion masks
#define ESR_SREGION_MASK        0x00'8030'0000ull
#define ESR_SREGION_EXT_MASK    0x00'803E'0000ull

// Base addresses for ESR subregions
#define ESR_HART_REGION        0x00'8000'0000ull
#define ESR_NEIGH_REGION       0x00'8010'0000ull
#define ESR_SHIRE_REGION       0x00'8034'0000ull

// Debug mode (PP=2) data registers
#define ESR_NXDATA0             0x00'8080'0780ull
#define ESR_NXDATA1             0x00'8080'0788ull
#define ESR_AXDATA0             0x00'8080'0790ull
#define ESR_AXDATA1             0x00'8080'0798ull

// Neighborhood ESR addresses
#define ESR_DUMMY0                  0x00'80D0'0000ull
#define ESR_DUMMY1                  0x00'80D0'0008ull
#define ESR_MINION_BOOT             0x00'80D0'0018ull
#define ESR_MPROT                   0x00'80D0'0020ull
#define ESR_DUMMY2                  0x00'80D0'0028ull
#define ESR_DUMMY3                  0x00'80D0'0030ull
#define ESR_VMSPAGESIZE             0x00'80D0'0038ull
#define ESR_IPI_REDIRECT_PC         0x00'8010'0040ull
#define ESR_PMU_CTRL                0x00'80D0'0068ull
#define ESR_NEIGH_CHICKEN           0x00'80D0'0070ull
#define ESR_ICACHE_ERR_LOG_CTL      0x00'80D0'0078ull
#define ESR_ICACHE_ERR_LOG_INFO     0x00'80D0'0080ull
#define ESR_ICACHE_ERR_LOG_ADDRESS  0x00'80D0'0088ull
#define ESR_ICACHE_SBE_DBE_COUNTS   0x00'80D0'0090ull
#define ESR_HACTRL                  0x00'8090'FF80ull
#define ESR_HASTATUS0               0x00'8090'FF88ull
#define ESR_HASTATUS1               0x00'8090'FF90ull
#define ESR_AND_OR_TREE_L0          0x00'8090'FF98ull

// shire_other ESR addresses
#define ESR_MINION_FEATURE              0x00'80F4'0000ull
#define ESR_SHIRE_CONFIG                0x00'80F4'0008ull
#define ESR_THREAD1_DISABLE             0x00'80F4'0010ull
#define ESR_SHIRE_CACHE_BUILD_CONFIG    0x00'80F4'0018ull
#define ESR_SHIRE_CACHE_REVISION_ID     0x00'80F4'0020ull
#define ESR_IPI_REDIRECT_TRIGGER        0x00'8034'0080ull
#define ESR_IPI_REDIRECT_FILTER         0x00'80F4'0088ull
#define ESR_IPI_TRIGGER                 0x00'80F4'0090ull
#define ESR_IPI_TRIGGER_CLEAR           0x00'80F4'0098ull
#define ESR_FCC_CREDINC_0               0x00'8034'00C0ull
#define ESR_FCC_CREDINC_1               0x00'8034'00C8ull
#define ESR_FCC_CREDINC_2               0x00'8034'00D0ull
#define ESR_FCC_CREDINC_3               0x00'8034'00D8ull
#define ESR_FAST_LOCAL_BARRIER0         0x00'8034'0100ull
#define ESR_FAST_LOCAL_BARRIER1         0x00'8034'0108ull
#define ESR_FAST_LOCAL_BARRIER2         0x00'8034'0110ull
#define ESR_FAST_LOCAL_BARRIER3         0x00'8034'0118ull
#define ESR_FAST_LOCAL_BARRIER4         0x00'8034'0120ull
#define ESR_FAST_LOCAL_BARRIER5         0x00'8034'0128ull
#define ESR_FAST_LOCAL_BARRIER6         0x00'8034'0130ull
#define ESR_FAST_LOCAL_BARRIER7         0x00'8034'0138ull
#define ESR_FAST_LOCAL_BARRIER8         0x00'8034'0140ull
#define ESR_FAST_LOCAL_BARRIER9         0x00'8034'0148ull
#define ESR_FAST_LOCAL_BARRIER10        0x00'8034'0150ull
#define ESR_FAST_LOCAL_BARRIER11        0x00'8034'0158ull
#define ESR_FAST_LOCAL_BARRIER12        0x00'8034'0160ull
#define ESR_FAST_LOCAL_BARRIER13        0x00'8034'0168ull
#define ESR_FAST_LOCAL_BARRIER14        0x00'8034'0170ull
#define ESR_FAST_LOCAL_BARRIER15        0x00'8034'0178ull
#define ESR_FAST_LOCAL_BARRIER16        0x00'8034'0180ull
#define ESR_FAST_LOCAL_BARRIER17        0x00'8034'0188ull
#define ESR_FAST_LOCAL_BARRIER18        0x00'8034'0190ull
#define ESR_FAST_LOCAL_BARRIER19        0x00'8034'0198ull
#define ESR_FAST_LOCAL_BARRIER20        0x00'8034'01A0ull
#define ESR_FAST_LOCAL_BARRIER21        0x00'8034'01A8ull
#define ESR_FAST_LOCAL_BARRIER22        0x00'8034'01B0ull
#define ESR_FAST_LOCAL_BARRIER23        0x00'8034'01B8ull
#define ESR_FAST_LOCAL_BARRIER24        0x00'8034'01C0ull
#define ESR_FAST_LOCAL_BARRIER25        0x00'8034'01C8ull
#define ESR_FAST_LOCAL_BARRIER26        0x00'8034'01D0ull
#define ESR_FAST_LOCAL_BARRIER27        0x00'8034'01D8ull
#define ESR_FAST_LOCAL_BARRIER28        0x00'8034'01E0ull
#define ESR_FAST_LOCAL_BARRIER29        0x00'8034'01E8ull
#define ESR_FAST_LOCAL_BARRIER30        0x00'8034'01F0ull
#define ESR_FAST_LOCAL_BARRIER31        0x00'8034'01F8ull
#define ESR_MTIME                       0x00'80F4'0200ull
#define ESR_MTIMECMP                    0x00'80F4'0208ull
#define ESR_TIME_CONFIG                 0x00'80F4'0210ull
#define ESR_MTIME_LOCAL_TARGET          0x00'80F4'0218ull
#define ESR_THREAD0_DISABLE             0x00'80F4'0240ull
#define ESR_SHIRE_COOP_MODE             0x00'8074'0290ull
#define ESR_ICACHE_UPREFETCH            0x00'8034'02F8ull
#define ESR_ICACHE_SPREFETCH            0x00'8074'0300ull
#define ESR_ICACHE_MPREFETCH            0x00'80F4'0308ull
#define ESR_CLK_GATE_CTRL               0x00'80F4'0310ull
#define ESR_DEBUG_CLK_GATE_CTRL         0x00'80B5'FFA0ull
#define ESR_DMCTRL                      0x00'80B5'FF88ull
#define ESR_SM_CONFIG                   0x00'80B5'FF90ull
#define ESR_SM_TRIGGER                  0x00'80B5'FF98ull
#define ESR_SM_MATCH                    0x00'80B5'FFA8ull
#define ESR_SM_FILTER0                  0x00'80B5'FFB0ull
#define ESR_SM_FILTER1                  0x00'80B5'FFB8ull
#define ESR_SM_FILTER2                  0x00'80B5'FFC0ull
#define ESR_SM_DATA0                    0x00'80B5'FFC8ull
#define ESR_SM_DATA1                    0x00'80B5'FFD0ull


#define NEIGHID(pos)    ((pos) % EMU_NEIGH_PER_SHIRE)
#define MINION(hart)    ((hart) / EMU_THREADS_PER_MINION)
#define THREAD(hart)    ((hart) % EMU_THREADS_PER_MINION)


void neigh_esrs_t::debug_reset()
{
    hactrl = 0;
    hastatus0 = 0;
    hastatus1 = 0;
}


void neigh_esrs_t::warm_reset()
{
    ipi_redirect_pc = 0;
    pmu_ctrl = false;
}


void neigh_esrs_t::cold_reset()
{
    minion_boot = 0x0200'A000; // boot rom
    mprot = 0;
    dummy0 = 0;
    dummy2 = false;
    neigh_chicken = 0;
    icache_err_log_ctl = 0;
    icache_err_log_info = 0;
    icache_sbe_dbe_counts = 0;
}


void shire_cache_esrs_t::cold_reset()
{
    // No shire cache registers for Erbium
}


void shire_other_esrs_t::warm_reset()
{
    for (int i = 0; i < 32; ++i) {
        fast_local_barrier[i] = 0;
    }
    ipi_redirect_filter = 0;
    ipi_trigger = 0;
    shire_coop_mode = false;
    icache_prefetch_active = false;
}


void shire_other_esrs_t::cold_reset(unsigned shireid)
{
    (void) shireid;
    minion_feature = 0x01;
    thread0_disable = 0xFE; // Only start minion 0 hart 0.
    thread1_disable = 0xFF;
    mtime_local_target = 0;
    clk_gate_ctrl = 0;
    debug_clk_gate_ctrl = 0;
    // time_config = 0x28;
    // sm_config = 0;
}


void mem_shire_esrs_t::cold_reset()
{
    // No mem shire registers for Erbium
}


uint64_t System::esr_read(const Agent& agent, uint64_t addr)
{
    unsigned shire = shireindex((addr & ESR_REGION_SHIRE_MASK) >> ESR_REGION_SHIRE_SHIFT);
    if (shire >= EMU_NUM_SHIRES) {
            WARN_AGENT(esrs, agent, "Read ESR for illegal shire S%u:0x%" PRIx64,
                       shire, addr);
            throw memory_error(addr);
    }
    
    uint64_t sregion = addr & ESR_SREGION_MASK;

    if (sregion == ESR_HART_REGION) {
        uint64_t esr = addr & ESR_HART_ESR_MASK;
        unsigned hart = (addr & ESR_REGION_HART_MASK) >> ESR_REGION_HART_SHIFT;
        switch (esr) {
        case ESR_ABSCMD:
        case ESR_NXPROGBUF0:
        case ESR_NXPROGBUF1:
        case ESR_AXPROGBUF0:
        case ESR_AXPROGBUF1:
            return 0;
        case ESR_NXDATA0:
        case ESR_AXDATA0:
            return cpu[hart].ddata0 & 0xFFFF'FFFF;
        case ESR_NXDATA1:
        case ESR_AXDATA1:
            return cpu[hart].ddata0 >> 32;
        default:
            WARN_AGENT(esrs, agent, "Read unknown hart ESR S%u:M%u:T%u:0x%" PRIx64,
                       shireid(shire), MINION(hart), THREAD(hart), esr);
            throw memory_error(addr);
        }
    }

    if (sregion == ESR_NEIGH_REGION) {
        unsigned neigh = (addr & ESR_REGION_NEIGH_MASK) >> ESR_REGION_NEIGH_SHIFT;
        uint64_t esr = addr & ESR_NEIGH_ESR_MASK;
        if (neigh >= EMU_NEIGH_PER_SHIRE) {
            WARN_AGENT(esrs, agent, "Read illegal neigh ESR S%u:N%u:0x%" PRIx64, shireid(shire), neigh, esr);
            throw memory_error(addr);
        }
        unsigned pos = neigh + EMU_NEIGH_PER_SHIRE * shire;
        switch (esr) {
        case ESR_DUMMY0:
            return neigh_esrs[pos].dummy0;
        case ESR_DUMMY1:
            return 0;
        case ESR_MINION_BOOT:
            return neigh_esrs[pos].minion_boot;
        case ESR_MPROT:
            return neigh_esrs[pos].mprot;
        case ESR_DUMMY2:
            return neigh_esrs[pos].dummy2;
        case ESR_DUMMY3:
            return 0;
        case ESR_VMSPAGESIZE:
            return 0; // Not supported, return 0 anyways.
        case ESR_IPI_REDIRECT_PC:
            return neigh_esrs[pos].ipi_redirect_pc;
        case ESR_PMU_CTRL:
            return neigh_esrs[pos].pmu_ctrl;
        case ESR_NEIGH_CHICKEN:
            return neigh_esrs[pos].neigh_chicken;
        case ESR_ICACHE_ERR_LOG_CTL:
            return neigh_esrs[pos].icache_err_log_ctl;
        case ESR_ICACHE_ERR_LOG_INFO:
            return neigh_esrs[pos].icache_err_log_info;
        case ESR_ICACHE_ERR_LOG_ADDRESS:
            return 0;
        case ESR_ICACHE_SBE_DBE_COUNTS:
            return neigh_esrs[pos].icache_sbe_dbe_counts;
        case ESR_HACTRL:
            return neigh_esrs[pos].hactrl;
        case ESR_HASTATUS0:
            return neigh_esrs[pos].hastatus0;
        case ESR_HASTATUS1:
            return neigh_esrs[pos].hastatus1;
        case ESR_AND_OR_TREE_L0:
            return calculate_andortree0(pos);
        }
        WARN_AGENT(esrs, agent, "Read unknown neigh ESR S%u:N%u:0x%" PRIx64, shireid(shire), neigh, esr);
        throw memory_error(addr);
    }

    uint64_t sregion_extra = addr & ESR_SREGION_EXT_MASK;

    if (sregion_extra == ESR_SHIRE_REGION) {
        uint64_t esr = addr & ESR_SHIRE_ESR_MASK;
        switch (esr) {
        case ESR_MINION_FEATURE:
            return shire_other_esrs[shire].minion_feature;
        case ESR_SHIRE_CONFIG:
            return 0; // Unsupported, return 0 anyways.
        case ESR_THREAD1_DISABLE:
            return shire_other_esrs[shire].thread1_disable;
        case ESR_SHIRE_CACHE_BUILD_CONFIG:
        case ESR_SHIRE_CACHE_REVISION_ID:
            return 0; // Unsupported, return 0 anyways.
        case ESR_IPI_REDIRECT_TRIGGER:
            return 0;
        case ESR_IPI_REDIRECT_FILTER:
            return shire_other_esrs[shire].ipi_redirect_filter;
        case ESR_IPI_TRIGGER:
            return shire_other_esrs[shire].ipi_trigger;
        case ESR_IPI_TRIGGER_CLEAR:
        case ESR_FCC_CREDINC_0:
        case ESR_FCC_CREDINC_1:
        case ESR_FCC_CREDINC_2:
        case ESR_FCC_CREDINC_3:
            return 0;
        case ESR_FAST_LOCAL_BARRIER0:
        case ESR_FAST_LOCAL_BARRIER1:
        case ESR_FAST_LOCAL_BARRIER2:
        case ESR_FAST_LOCAL_BARRIER3:
        case ESR_FAST_LOCAL_BARRIER4:
        case ESR_FAST_LOCAL_BARRIER5:
        case ESR_FAST_LOCAL_BARRIER6:
        case ESR_FAST_LOCAL_BARRIER7:
        case ESR_FAST_LOCAL_BARRIER8:
        case ESR_FAST_LOCAL_BARRIER9:
        case ESR_FAST_LOCAL_BARRIER10:
        case ESR_FAST_LOCAL_BARRIER11:
        case ESR_FAST_LOCAL_BARRIER12:
        case ESR_FAST_LOCAL_BARRIER13:
        case ESR_FAST_LOCAL_BARRIER14:
        case ESR_FAST_LOCAL_BARRIER15:
        case ESR_FAST_LOCAL_BARRIER16:
        case ESR_FAST_LOCAL_BARRIER17:
        case ESR_FAST_LOCAL_BARRIER18:
        case ESR_FAST_LOCAL_BARRIER19:
        case ESR_FAST_LOCAL_BARRIER20:
        case ESR_FAST_LOCAL_BARRIER21:
        case ESR_FAST_LOCAL_BARRIER22:
        case ESR_FAST_LOCAL_BARRIER23:
        case ESR_FAST_LOCAL_BARRIER24:
        case ESR_FAST_LOCAL_BARRIER25:
        case ESR_FAST_LOCAL_BARRIER26:
        case ESR_FAST_LOCAL_BARRIER27:
        case ESR_FAST_LOCAL_BARRIER28:
        case ESR_FAST_LOCAL_BARRIER29:
        case ESR_FAST_LOCAL_BARRIER30:
        case ESR_FAST_LOCAL_BARRIER31:
            return shire_other_esrs[shire].fast_local_barrier[(esr - ESR_FAST_LOCAL_BARRIER0)>>3];
        case ESR_MTIME:
            return 0;
            // TODO: implement actual timer (restore PU)
            //return memory.timer_read_mtime();
        case ESR_MTIMECMP:
            return 0;
            // TODO: implement actual timer (restore PU)
            // return memory.timer_read_mtimecmp();
        case ESR_TIME_CONFIG:
            return 0;
            // TODO: implement actual timer (restore PU)
            // return memory.timer_read_mtimecmp();
        case ESR_MTIME_LOCAL_TARGET:
            return shire_other_esrs[shire].mtime_local_target;
        case ESR_THREAD0_DISABLE:
            return shire_other_esrs[shire].thread0_disable;
        case ESR_SHIRE_COOP_MODE:
            return shire_other_esrs[shire].shire_coop_mode;
        case ESR_ICACHE_UPREFETCH:
            return read_icache_prefetch(Privilege::U, shire);
        case ESR_ICACHE_SPREFETCH:
            return read_icache_prefetch(Privilege::S, shire);
        case ESR_ICACHE_MPREFETCH:
            return read_icache_prefetch(Privilege::M, shire);
        case ESR_CLK_GATE_CTRL:
            return shire_other_esrs[shire].clk_gate_ctrl;
        case ESR_DEBUG_CLK_GATE_CTRL:
            return shire_other_esrs[shire].debug_clk_gate_ctrl;
        case ESR_DMCTRL:
            return agent.chip->read_dmctrl();
        case ESR_SM_CONFIG:
            // TODO: implement Status Monitor in debug module
            return 0;
        case ESR_SM_TRIGGER:
            return 0; // WARL
        case ESR_SM_MATCH:
        case ESR_SM_FILTER0:
        case ESR_SM_FILTER1:
        case ESR_SM_FILTER2:
        case ESR_SM_DATA0:
        case ESR_SM_DATA1:
            // TODO: implement Status Monitor in debug module
            return 0;
        }
        WARN_AGENT(esrs, agent, "Read unknown shire_other ESR S%u:0x%" PRIx64, shireid(shire), esr);
        throw memory_error(addr);
    }

    WARN_AGENT(esrs, agent, "Read illegal ESR S%u:0x%" PRIx64, shireid(shire), addr);
    throw memory_error(addr);
}


void System::esr_write(const Agent& agent, uint64_t addr, uint64_t value)
{
    unsigned shire = shireindex((addr & ESR_REGION_SHIRE_MASK) >> ESR_REGION_SHIRE_SHIFT);
    if (shire >= EMU_NUM_SHIRES) {
            WARN_AGENT(esrs, agent, "Write ESR for illegal shire S%u:0x%" PRIx64,
                       shire, addr);
            throw memory_error(addr);
    }
    
    uint64_t sregion = addr & ESR_SREGION_MASK;

    if (sregion == ESR_HART_REGION) {
        uint64_t esr = addr & ESR_HART_ESR_MASK;
        unsigned hart = (addr & ESR_REGION_HART_MASK) >> ESR_REGION_HART_SHIFT;
        switch (esr) {
        case ESR_ABSCMD:
        case ESR_NXPROGBUF0:
        case ESR_NXPROGBUF1:
        case ESR_AXPROGBUF0:
        case ESR_AXPROGBUF1: {
            unsigned hartid = hart;
            if (cpu[hartid].in_progbuf()) {
                cpu[hartid].exit_progbuf(Hart::Progbuf::error);
            } else if (cpu[hartid].is_halted()) {
                cpu[hartid].write_progbuf(esr, value);
                if (esr < ESR_NXPROGBUF0 || esr > ESR_NXPROGBUF1) {
                    cpu[hartid].enter_progbuf();
                }
            }
            break;
        }
        case ESR_NXDATA0:
        case ESR_AXDATA0: {
            unsigned hartid = hart;
            cpu[hartid].ddata0 &= 0xFFFF'FFFF'0000'0000ull;
            cpu[hartid].ddata0 |= value & 0xFFFF'FFFFull;
            if (esr == ESR_AXDATA0) cpu[hartid].enter_progbuf();
            LOG_AGENT(DEBUG, agent, "S%u:H%u:data0 = 0x%" PRIx32, shireid(shire), hart, uint32_t(value));
            break;
        }
        case ESR_NXDATA1:
        case ESR_AXDATA1: {
            unsigned hartid = hart + shire * EMU_THREADS_PER_SHIRE;
            cpu[hartid].ddata0 &= 0xFFFF'FFFFull;
            cpu[hartid].ddata0 |= value << 32;
            LOG_AGENT(DEBUG, agent, "S%u:H%u:data1 = 0x%" PRIx32, shireid(shire), hart, uint32_t(value));
            if (esr == ESR_AXDATA1) cpu[hartid].enter_progbuf();
            break;
        }
        default:
            WARN_AGENT(esrs, agent, "Write unknown hart ESR S%u:M%u:T%u:0x%" PRIx64,
                       shireid(shire), MINION(hart), THREAD(hart), esr);
            throw memory_error(addr);
        }
        return;
    }

    if (sregion == ESR_NEIGH_REGION) {
        unsigned neigh = (addr & ESR_REGION_NEIGH_MASK) >> ESR_REGION_NEIGH_SHIFT;
        uint64_t esr = addr & ESR_NEIGH_ESR_MASK;
        if (neigh >= EMU_NEIGH_PER_SHIRE) {
            WARN_AGENT(esrs, agent, "Write illegal neigh ESR S%u:N%u:0x%" PRIx64, shireid(shire), neigh, esr);
            throw memory_error(addr);
        }
        unsigned pos = neigh + EMU_NEIGH_PER_SHIRE * shire;
        switch (esr) {
        case ESR_DUMMY0:
            neigh_esrs[pos].dummy0 = uint32_t(value & 0xFFFF'FFFF);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:dummy0 = 0x%" PRIx32,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].dummy0);
            break;
        case ESR_MINION_BOOT:
            neigh_esrs[pos].minion_boot = value & 0xFFFF'FFFF'FFFFull;
            LOG_AGENT(DEBUG, agent, "S%u:N%u:minion_boot = 0x%" PRIx64,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].minion_boot);
            break;
        case ESR_MPROT:
            neigh_esrs[pos].mprot = uint16_t(value & 0x1FF);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:mprot = 0x%" PRIx16,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].mprot);
            break;
        case ESR_DUMMY2:
            neigh_esrs[pos].dummy2 = bool(value & 1);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:dummy2 = 0x%x",
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].dummy2 ? 1 : 0);
            break;
        case ESR_IPI_REDIRECT_PC:
            neigh_esrs[pos].ipi_redirect_pc = value & 0xffff'ffff'ffffull;
            LOG_AGENT(DEBUG, agent, "S%u:N%u:ipi_redirect_pc = 0x%" PRIx64,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].ipi_redirect_pc);
            break;
        case ESR_PMU_CTRL:
            neigh_esrs[pos].pmu_ctrl = bool(value & 1);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:pmu_ctrl = 0x%x",
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].pmu_ctrl ? 1 : 0);
            break;
        case ESR_NEIGH_CHICKEN:
            neigh_esrs[pos].neigh_chicken = uint8_t(value & 0x7F);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:neigh_chicken = 0x%" PRIx8,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].neigh_chicken);
            break;
        case ESR_ICACHE_ERR_LOG_CTL:
            neigh_esrs[pos].icache_err_log_ctl = uint8_t(value & 0x7);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_err_log_ctl = 0x%" PRIx8,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_err_log_ctl);
            break;
        case ESR_ICACHE_ERR_LOG_INFO:
            neigh_esrs[pos].icache_err_log_info = value & ~0x8ull;
            LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_err_log_info = 0x%" PRIx64,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_err_log_info);
            break;
        case ESR_ICACHE_ERR_LOG_ADDRESS:
            // TODO: implement
            // neigh_esrs[pos].icache_err_log_address = value & 0x3'FFFF'FFFFull;
            // LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_err_log_address = 0x%" PRIx64,
            //           shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_err_log_address);
            break;
        case ESR_ICACHE_SBE_DBE_COUNTS:
            neigh_esrs[pos].icache_sbe_dbe_counts = uint16_t(value & 0x7FF);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_sbe_dbe_counts = 0x%" PRIx16,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_sbe_dbe_counts);
            break;
        case ESR_HACTRL:
            neigh_esrs[pos].hactrl = uint32_t(value & 0xffff'ffff);
            LOG_AGENT(DEBUG, agent, "S%u:N%u:hactrl = 0x%" PRIx32,
                      shireid(shire), NEIGHID(pos), uint32_t(neigh_esrs[pos].hactrl));
            break;
        case ESR_HASTATUS1:
            neigh_esrs[pos].hastatus1 = value & 0xffff'ffff'0000ull;
            LOG_AGENT(DEBUG, agent, "S%u:N%u:hastatus1 = 0x%" PRIx64,
                      shireid(shire), NEIGHID(pos), neigh_esrs[pos].hastatus1);
            break;
        default:
            WARN_AGENT(esrs, agent, "Write unknown neigh ESR S%u:N%u:0x%" PRIx64, shireid(shire), neigh, esr);
            throw memory_error(addr);
        }
        return;
    }

    uint64_t sregion_extra = addr & ESR_SREGION_EXT_MASK;

    if (sregion_extra == ESR_SHIRE_REGION) {
        uint64_t esr = addr & ESR_SHIRE_ESR_MASK;
        switch (esr) {
        case ESR_MINION_FEATURE:
            write_minion_feature(shire, uint8_t(value & 0x3F));
            LOG_AGENT(DEBUG, agent, "S%u:minion_feature = 0x%" PRIx8,
                      shireid(shire), shire_other_esrs[shire].minion_feature);
            return;
        case ESR_THREAD1_DISABLE:
            write_thread1_disable(shire, uint32_t(value & 0xFF));
            LOG_AGENT(DEBUG, agent, "S%u:thread1_disable = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].thread1_disable);
            return;
        case ESR_IPI_REDIRECT_TRIGGER:
            LOG_AGENT(DEBUG, agent, "S%u:ipi_redirect_trigger = 0x%" PRIx64, shireid(shire), value);
            send_ipi_redirect(shire, value);
            return;
        case ESR_IPI_REDIRECT_FILTER:
            shire_other_esrs[shire].ipi_redirect_filter = value & 0xFFFF;
            LOG_AGENT(DEBUG, agent, "S%u:ipi_redirect_filter = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].ipi_redirect_filter);
            return;
        case ESR_IPI_TRIGGER:
            shire_other_esrs[shire].ipi_trigger = value & 0xFFFF;
            LOG_AGENT(DEBUG, agent, "S%u:ipi_trigger = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].ipi_trigger);
            raise_machine_software_interrupt(shire, value & 0xFFFF);
            return;
        case ESR_IPI_TRIGGER_CLEAR:
            LOG_AGENT(DEBUG, agent, "S%u:ipi_trigger_clear = 0x%" PRIx64, shireid(shire), value & 0xffff);
            clear_machine_software_interrupt(shire, value & 0xFFFF);
            return;
        case ESR_FCC_CREDINC_0:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_0 = 0x%" PRIx64, shireid(shire), value & 0xff);
            write_fcc_credinc(0, shire, value & 0xFF);
            return;
        case ESR_FCC_CREDINC_1:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_1 = 0x%" PRIx64, shireid(shire), value & 0xff);
            write_fcc_credinc(1, shire, value & 0xFF);
            return;
        case ESR_FCC_CREDINC_2:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_2 = 0x%" PRIx64, shireid(shire), value & 0xff);
            write_fcc_credinc(2, shire, value & 0xFF);
            return;
        case ESR_FCC_CREDINC_3:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_3 = 0x%" PRIx64, shireid(shire), value & 0xff);
            write_fcc_credinc(3, shire, value & 0xFF);
            return;
        case ESR_FAST_LOCAL_BARRIER0:
        case ESR_FAST_LOCAL_BARRIER1:
        case ESR_FAST_LOCAL_BARRIER2:
        case ESR_FAST_LOCAL_BARRIER3:
        case ESR_FAST_LOCAL_BARRIER4:
        case ESR_FAST_LOCAL_BARRIER5:
        case ESR_FAST_LOCAL_BARRIER6:
        case ESR_FAST_LOCAL_BARRIER7:
        case ESR_FAST_LOCAL_BARRIER8:
        case ESR_FAST_LOCAL_BARRIER9:
        case ESR_FAST_LOCAL_BARRIER10:
        case ESR_FAST_LOCAL_BARRIER11:
        case ESR_FAST_LOCAL_BARRIER12:
        case ESR_FAST_LOCAL_BARRIER13:
        case ESR_FAST_LOCAL_BARRIER14:
        case ESR_FAST_LOCAL_BARRIER15:
        case ESR_FAST_LOCAL_BARRIER16:
        case ESR_FAST_LOCAL_BARRIER17:
        case ESR_FAST_LOCAL_BARRIER18:
        case ESR_FAST_LOCAL_BARRIER19:
        case ESR_FAST_LOCAL_BARRIER20:
        case ESR_FAST_LOCAL_BARRIER21:
        case ESR_FAST_LOCAL_BARRIER22:
        case ESR_FAST_LOCAL_BARRIER23:
        case ESR_FAST_LOCAL_BARRIER24:
        case ESR_FAST_LOCAL_BARRIER25:
        case ESR_FAST_LOCAL_BARRIER26:
        case ESR_FAST_LOCAL_BARRIER27:
        case ESR_FAST_LOCAL_BARRIER28:
        case ESR_FAST_LOCAL_BARRIER29:
        case ESR_FAST_LOCAL_BARRIER30:
        case ESR_FAST_LOCAL_BARRIER31:
            shire_other_esrs[shire].fast_local_barrier[(esr - ESR_FAST_LOCAL_BARRIER0)>>3] = uint8_t(value);
            LOG_AGENT(DEBUG, agent, "S%u:fast_local_barrier%llu = 0x%" PRIx8,
                      shireid(shire), (esr - ESR_FAST_LOCAL_BARRIER0)>>3,
                      shire_other_esrs[shire].fast_local_barrier[(esr - ESR_FAST_LOCAL_BARRIER0)>>3]);
            return;
        case ESR_MTIME:
            // TODO: implement actual timer (restore PU)
            return;
        case ESR_MTIMECMP:
            // TODO: implement actual timer (restore PU)
            return;
        case ESR_TIME_CONFIG:
            // TODO: implement actual timer
            // shire_other_esrs[shire].time_config = uint16_t(value & 0x3ff);
            // LOG_AGENT(DEBUG, agent, "S%u:time_config = 0x%" PRIx16,
            //           shireid(shire), shire_other_esrs[shire].time_config);
            return;
        case ESR_MTIME_LOCAL_TARGET:
            shire_other_esrs[shire].mtime_local_target = uint16_t(value & 0xFFFF);
            LOG_AGENT(DEBUG, agent, "S%u:mtime_local_target = 0x%" PRIx16,
                      shireid(shire), shire_other_esrs[shire].mtime_local_target);
            return;
        case ESR_THREAD0_DISABLE:
            write_thread0_disable(shire, uint8_t(value & 0xFF));
            LOG_AGENT(DEBUG, agent, "S%u:thread0_disable = 0x%" PRIx8,
                      shireid(shire), uint8_t(shire_other_esrs[shire].thread0_disable));
            return;
        case ESR_SHIRE_COOP_MODE:
            LOG_AGENT(DEBUG, agent, "S%u:shire_coop_mode = 0x%x", shireid(shire), unsigned(value & 0x1));
            write_shire_coop_mode(shire, value);
            return;
        case ESR_ICACHE_UPREFETCH:
            LOG_AGENT(DEBUG, agent, "S%u:icache_uprefetch = 0x%" PRIx64, shireid(shire), uint64_t(value & 0xFFFF'FFFF'FFFFull));
            write_icache_prefetch(Privilege::U, shire, value & 0xffff'ffff'ffffull);
            return;
        case ESR_ICACHE_SPREFETCH:
            LOG_AGENT(DEBUG, agent, "S%u:icache_sprefetch = 0x%" PRIx64, shireid(shire), uint64_t(value & 0xFFFF'FFFF'FFFFull));
            write_icache_prefetch(Privilege::S, shire, value & 0xffff'ffff'ffffull);
            return;
        case ESR_ICACHE_MPREFETCH:
            LOG_AGENT(DEBUG, agent, "S%u:icache_mprefetch = 0x%" PRIx64, shireid(shire), uint64_t(value & 0xFFFF'FFFF'FFFFull));
            write_icache_prefetch(Privilege::M, shire, value & 0xffff'ffff'ffffull);
            return;
        case ESR_CLK_GATE_CTRL:
            shire_other_esrs[shire].clk_gate_ctrl = uint8_t(value & 0xDF);
            LOG_AGENT(DEBUG, agent, "S%u:clk_gate_ctrl = 0x%" PRIx16,
                      shireid(shire), shire_other_esrs[shire].clk_gate_ctrl);
            return;
        case ESR_DEBUG_CLK_GATE_CTRL:
            shire_other_esrs[shire].debug_clk_gate_ctrl = uint8_t(value & 0x1);
            LOG_AGENT(DEBUG, agent, "S%u:debug_clk_gate_ctrl = 0x%" PRIx8,
                      shireid(shire), shire_other_esrs[shire].debug_clk_gate_ctrl);
            return;
        case ESR_DMCTRL:
            agent.chip->write_dmctrl(uint32_t(value & 0xF400'000F));
            LOG_AGENT(DEBUG, agent, "S%u:dmctrl = 0x%" PRIx32,
                      shireid(shire), uint32_t(value & 0xF400'000F));
            return;
        case ESR_SM_CONFIG:
            // TODO: implement Status Monitor in debug module
            // shire_other_esrs[shire].sm_config = uint16_t(value & 0xFFF);
            // LOG_AGENT(DEBUG, agent, "S%u:sm_config = 0x%" PRIx16,
            //           shireid(shire), shire_other_esrs[shire].sm_config);
            return;
        case ESR_SM_TRIGGER:
            // TODO: implement Status Monitor in debug module
            // shire_other_esrs[shire].sm_trigger = uint8_t(value & 0x1);
            // LOG_AGENT(DEBUG, agent, "S%u:sm_trigger = 0x%" PRIx8,
            //           shireid(shire), shire_other_esrs[shire].sm_trigger);
            return;
        }
        WARN_AGENT(esrs, agent, "Write unknown shire_other ESR S%u:0x%" PRIx64, shireid(shire), esr);
        throw memory_error(addr);
    }

    WARN_AGENT(esrs, agent, "Write illegal ESR S%u:0x%" PRIx64, shireid(shire), addr);
    throw memory_error(addr);
}


void System::write_shire_coop_mode(unsigned shire, uint64_t value)
{
    assert(shire < EMU_NUM_SHIRES);
    value &= 1;
    shire_other_esrs[shire].shire_coop_mode = value;
    if (!value)
        finish_icache_prefetch(shire);
}


void System::write_thread0_disable(unsigned shire, uint32_t value)
{
    shire_other_esrs[shire].thread0_disable = value;
    recalculate_thread0_enable(shire);
}


void System::write_thread1_disable(unsigned shire, uint32_t value)
{
    shire_other_esrs[shire].thread1_disable = value;
    recalculate_thread1_enable(shire);
}


void System::write_minion_feature(unsigned shire, uint8_t value)
{
    shire_other_esrs[shire].minion_feature = value;
    recalculate_thread1_enable(shire);
}


void System::write_icache_prefetch(Privilege /*privilege*/, unsigned shire, uint64_t value)
{
    assert(shire <= EMU_NUM_COMPUTE_SHIRES);
#ifdef SYS_EMU
    (void)(shire);
    (void)(value);
#else
    if (!shire_other_esrs[shire].icache_prefetch_active) {
        bool active = shire_other_esrs[shire].shire_coop_mode;
        shire_other_esrs[shire].icache_prefetch_active = active;
    }
#endif
}


uint64_t System::read_icache_prefetch(Privilege /*privilege*/, unsigned shire) const
{
    assert(shire <= EMU_NUM_COMPUTE_SHIRES);
#ifdef SYS_EMU
    (void) shire;
    // NB: Prefetches finish instantaneously in sys_emu
    return 1;
#else
    return shire_other_esrs[shire].icache_prefetch_active;
#endif
}


void System::finish_icache_prefetch(unsigned shire)
{
    assert(shire <= EMU_NUM_COMPUTE_SHIRES);
#ifndef SYS_EMU
    shire_other_esrs[shire].icache_prefetch_active = 0;
#else
    (void) shire;
#endif
}


} // namespace bemu

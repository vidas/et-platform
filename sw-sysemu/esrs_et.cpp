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


// Message ports
unsigned get_msg_port_write_width(const Hart&, unsigned);


#define ESR_NEIGH_MINION_BOOT_RESET_VAL   0x8000001000
#define ESR_ICACHE_ERR_LOG_CTL_RESET_VAL  0x6
#define ESR_TEXTURE_CONTROL_RESET_VAL     0x5
#define ESR_MPROT_RESET_VAL               0x13

#define ESR_SC_L3_SHIRE_SWIZZLE_CTL_RESET_VAL   0x0000987765543210ULL
#define ESR_SC_REQQ_CTL_RESET_VAL               0x00038A80
#define ESR_SC_PIPE_CTL_RESET_VAL               0x0000005CFFFFFFFFULL
#define ESR_SC_L2_CACHE_CTL_RESET_VAL           0x02800080007F007FULL
#define ESR_SC_L3_CACHE_CTL_RESET_VAL           0x0300010000FF00FFULL
#define ESR_SC_SCP_CACHE_CTL_RESET_VAL          0x0000028003FF01FFULL
#define ESR_SC_ERR_LOG_CTL_RESET_VAL            0x1FE

#define ESR_FILTER_IPI_RESET_VAL                0xFFFFFFFFFFFFFFFFULL
#define ESR_SHIRE_CONFIG_CONST_RESET_VAL        0x392A000
#define ESR_SHIRE_CACHE_RAM_CFG1_RESET_VAL      0xE800340
#define ESR_SHIRE_CACHE_RAM_CFG2_RESET_VAL      0x03A0
#define ESR_SHIRE_CACHE_RAM_CFG3_RESET_VAL      0x0C8C0323
#define ESR_SHIRE_CACHE_RAM_CFG4_RESET_VAL      0x34000C8C03A0
#define ESR_SHIRE_CLK_GATE_CTRL_RESET_VAL       0x0


// Broadcast ESR fields
// - ESR privilege (bits [31:30]) is in bits [60:59] in broadcast_data.
// - ESR region (bits [21:17]) is in bits [58:45] in {usm}broadcast
// - ESR address (bits [17:3]) is in bits [54:40] in {usm}broadcast
// - Broadcast destination shires' bitmask is in bits [39:0]
#define ESR_BROADCAST_PROT_MASK              0x1800000000000000ULL
#define ESR_BROADCAST_PROT_SHIFT             59
#define ESR_BROADCAST_ESR_SREGION_MASK       0x07C0000000000000ULL
#define ESR_BROADCAST_ESR_SREGION_MASK_SHIFT 54
#define ESR_BROADCAST_ESR_ADDR_MASK          0x003FFF0000000000ULL
#define ESR_BROADCAST_ESR_ADDR_SHIFT         40
#define ESR_BROADCAST_ESR_SHIRE_MASK         0x000000FFFFFFFFFFULL
#define ESR_BROADCAST_ESR_MAX_SHIRES         ESR_BROADCAST_ESR_ADDR_SHIFT


#define PP(val) \
    (((val) & ESR_REGION_PROT_MASK) >> ESR_REGION_PROT_SHIFT)

#define NEIGHID(pos)    ((pos) % EMU_NEIGH_PER_SHIRE)
#define MINION(hart)    ((hart) / EMU_THREADS_PER_MINION)
#define THREAD(hart)    ((hart) % EMU_THREADS_PER_MINION)


// ESR region 'shireid' field in bits [29:22]
// The local shire has bits [29:22] = 8'b11111111
#define ESR_REGION_SHIRE_MASK   0x003FC00000ULL
#define ESR_REGION_LOCAL_SHIRE  0x003FC00000ULL
#define ESR_REGION_SHIRE_SHIFT  22

// ESR region 'hart' field in bits [19:12] (when 'subregion' is 2'b00)
#define ESR_REGION_HART_MASK    0x00000FF000ULL
#define ESR_REGION_HART_SHIFT   12

// ESR region 'neighborhood' field in bits [19:16] (when 'subregion' is 2'b01)
// The broadcast neighborhood has bits [19:16] == 4'b1111
#define ESR_REGION_NEIGH_MASK       0x00000F0000ULL
#define ESR_REGION_NEIGH_SHIFT      16
#define ESR_REGION_NEIGH_BROADCAST  0xF

// ESR region 'bank' field in bits [16:13] (when 'subregion' is 2'b11)
#define ESR_REGION_BANK_MASK    0x000001E000ULL
#define ESR_REGION_BANK_SHIFT   13

// ESR region 'ESR' field masks
#define ESR_HART_ESR_MASK       0xFFC0300FFFULL
#define ESR_NEIGH_ESR_MASK      0xFFC030FFFFULL
#define ESR_CACHE_ESR_MASK      0xFFC03E1FFFULL
#define ESR_SHIRE_ESR_MASK      0xFFC03FFFFFULL
#define ESR_RBOX_ESR_MASK       0xFFC03FFFFFULL
#define ESR_IOSHIRE_ESR_MASK    0xFFC0200FFFULL

// Subregion masks
#define ESR_SREGION_MASK        0x0100300000ULL
#define ESR_SREGION_EXT_MASK    0x01003E0000ULL
#define ESR_SREGION_EXT_SHIFT   17
#define ESR_MSREGION_MASK       0x0100000200ULL

// Base addresses for ESR subregions
#define ESR_HART_REGION        0x0100000000ULL
#define ESR_NEIGH_REGION       0x0100100000ULL
#define ESR_RSRVD_REGION       0x0100200000ULL
#define ESR_CACHE_REGION       0x0100300000ULL
#define ESR_RBOX_REGION        0x0100320000ULL
#define ESR_SHIRE_REGION       0x0100340000ULL
#define ESR_MEMSHIRE_REGION    0x0100000000ULL
#define ESR_DDRC_REGION        0x0100000200ULL

// Message port subregion
#define ESR_HART_PORT_ADDR_VALID(x) (((x) & 0xF38) == 0x800)
#define ESR_HART_PORT_NUM_MASK      0xC0ULL
#define ESR_HART_PORT_NUM_SHIFT     6

// Helper macros to construct ESR addresses
#define ESR_HART(shire, hart, name) \
    ((uint64_t(shire) << ESR_REGION_SHIRE_SHIFT) + \
     (uint64_t(hart) << ESR_REGION_HART_SHIFT) + \
      uint64_t(ESR_ ## name))

#define ESR_NEIGH(shire, neigh, name) \
    ((uint64_t(shire) << ESR_REGION_SHIRE_SHIFT) + \
     (uint64_t(neigh) << ESR_REGION_NEIGH_SHIFT) + \
      uint64_t(ESR_ ## name))

#define ESR_CACHE(shire, bank, name) \
    ((uint64_t(shire) << ESR_REGION_SHIRE_SHIFT) + \
     (uint64_t(bank) << ESR_REGION_BANK_SHIFT) + \
      uint64_t(ESR_ ## name))

#define ESR_RBOX(shire, name) \
    ((uint64_t(shire) << ESR_REGION_SHIRE_SHIFT) + \
      uint64_t(ESR_ ## name))

#define ESR_SHIRE(shire, name) \
    ((uint64_t(shire) << ESR_REGION_SHIRE_SHIFT) + \
      uint64_t(ESR_ ## name))

// Hart ESR addresses
#define ESR_HART_U0             0x0100000000ULL
#define ESR_HART_S0             0x0140000000ULL
#define ESR_HART_D0             0x0180000000ULL
#define ESR_HART_M0             0x01C0000000ULL
#define ESR_NXDATA0             0x0180000780ULL
#define ESR_NXDATA1             0x0180000788ULL
#define ESR_AXDATA0             0x0180000790ULL
#define ESR_AXDATA1             0x0180000798ULL

// Message Port addresses
#define ESR_PORT0               0x0100000800ULL
#define ESR_PORT1               0x0100000840ULL
#define ESR_PORT2               0x0100000880ULL
#define ESR_PORT3               0x01000008c0ULL

// Neighborhood ESR addresses
#define ESR_NEIGH_U0                0x0100100000ULL
#define ESR_NEIGH_S0                0x0140100000ULL
#define ESR_NEIGH_D0                0x0180100000ULL
#define ESR_NEIGH_M0                0x01C0100000ULL
#define ESR_DUMMY0                  0x0100100000ULL
#define ESR_DUMMY1                  0x0100100008ULL
#define ESR_MINION_BOOT             0x01C0100018ULL
#define ESR_MPROT                   0x01C0100020ULL
#define ESR_DUMMY2                  0x01C0100028ULL
#define ESR_DUMMY3                  0x01C0100030ULL
#define ESR_VMSPAGESIZE             0x01C0100038ULL
#define ESR_IPI_REDIRECT_PC         0x0100100040ULL
#define ESR_PMU_CTRL                0x01C0100068ULL
#define ESR_NEIGH_CHICKEN           0x01C0100070ULL
#define ESR_ICACHE_ERR_LOG_CTL      0x01C0100078ULL
#define ESR_ICACHE_ERR_LOG_INFO     0x01C0100080ULL
#define ESR_ICACHE_ERR_LOG_ADDRESS  0x01C0100088ULL
#define ESR_ICACHE_SBE_DBE_COUNTS   0x01C0100090ULL
#define ESR_TEXTURE_CONTROL         0x0100108000ULL
#define ESR_TEXTURE_STATUS          0x0100108008ULL
#define ESR_TEXTURE_IMAGE_TABLE_PTR 0x0100108010ULL
#define ESR_HACTRL                  0x018010FF80ULL
#define ESR_HASTATUS0               0x018010FF88ULL
#define ESR_HASTATUS1               0x018010FF90ULL
#define ESR_AND_OR_TREE_L0          0x018010FF98ULL

// shire_cache ESR addresses
#define ESR_CACHE_U0                      0x0100300000ULL
#define ESR_CACHE_S0                      0x0140300000ULL
#define ESR_CACHE_D0                      0x0180300000ULL
#define ESR_CACHE_M0                      0x01C0300000ULL
#define ESR_SC_L3_SHIRE_SWIZZLE_CTL       0x01C0300000ULL
#define ESR_SC_REQQ_CTL                   0x01C0300008ULL
#define ESR_SC_PIPE_CTL                   0x01C0300010ULL
#define ESR_SC_L2_CACHE_CTL               0x01C0300018ULL
#define ESR_SC_L3_CACHE_CTL               0x01C0300020ULL
#define ESR_SC_SCP_CACHE_CTL              0x01C0300028ULL
#define ESR_SC_IDX_COP_SM_CTL             0x01C0300030ULL
#define ESR_SC_IDX_COP_SM_PHYSICAL_INDEX  0x01C0300038ULL
#define ESR_SC_IDX_COP_SM_DATA0           0x01C0300040ULL
#define ESR_SC_IDX_COP_SM_DATA1           0x01C0300048ULL
#define ESR_SC_IDX_COP_SM_ECC             0x01C0300050ULL
#define ESR_SC_ERR_LOG_CTL                0x01C0300058ULL
#define ESR_SC_ERR_LOG_INFO               0x01C0300060ULL
#define ESR_SC_ERR_LOG_ADDRESS            0x01C0300068ULL
#define ESR_SC_SBE_DBE_COUNTS             0x01C0300070ULL
#define ESR_SC_REQQ_DEBUG_CTL             0x01C0300078ULL
#define ESR_SC_REQQ_DEBUG0                0x01C0300080ULL
#define ESR_SC_REQQ_DEBUG1                0x01C0300088ULL
#define ESR_SC_REQQ_DEBUG2                0x01C0300090ULL
#define ESR_SC_REQQ_DEBUG3                0x01C0300098ULL
#define ESR_SC_ECO_CTL                    0x01C03000A0ULL
#define ESR_SC_PERFMON_CTL_STATUS         0x01C03000B8ULL
#define ESR_SC_PERFMON_CYC_CNTR           0x01C03000C0ULL
#define ESR_SC_PERFMON_P0_CNTR            0x01C03000C8ULL
#define ESR_SC_PERFMON_P1_CNTR            0x01C03000D0ULL
#define ESR_SC_PERFMON_P0_QUAL            0x01C03000D8ULL
#define ESR_SC_PERFMON_P1_QUAL            0x01C03000E0ULL
#define ESR_SC_IDX_COP_SM_CTL_USER        0x0100300100ULL

// RBOX ESR addresses
#define ESR_RBOX_U0             0x0100320000ULL
#define ESR_RBOX_S0             0x0140320000ULL
#define ESR_RBOX_M0             0x01C0320000ULL
#define ESR_RBOX_CONFIG         0x0100320000ULL
#define ESR_RBOX_IN_BUF_PG      0x0100320008ULL
#define ESR_RBOX_IN_BUF_CFG     0x0100320010ULL
#define ESR_RBOX_OUT_BUF_PG     0x0100320018ULL
#define ESR_RBOX_OUT_BUF_CFG    0x0100320020ULL
#define ESR_RBOX_STATUS         0x0100320028ULL
#define ESR_RBOX_START          0x0100320030ULL
#define ESR_RBOX_CONSUME        0x0100320038ULL

// shire_other ESR addresses
#define ESR_SHIRE_U0                    0x0100340000ULL
#define ESR_SHIRE_S0                    0x0140340000ULL
#define ESR_SHIRE_D0                    0x0180340000ULL
#define ESR_SHIRE_M0                    0x01C0340000ULL
#define ESR_MINION_FEATURE              0x01C0340000ULL
#define ESR_SHIRE_CONFIG                0x01C0340008ULL
#define ESR_THREAD1_DISABLE             0x01C0340010ULL
#define ESR_SHIRE_CACHE_BUILD_CONFIG    0x01C0340018ULL
#define ESR_SHIRE_CACHE_REVISION_ID     0x01C0340020ULL
#define ESR_IPI_REDIRECT_TRIGGER        0x0100340080ULL
#define ESR_IPI_REDIRECT_FILTER         0x01C0340088ULL
#define ESR_IPI_TRIGGER                 0x01C0340090ULL
#define ESR_IPI_TRIGGER_CLEAR           0x01C0340098ULL
#define ESR_FCC_CREDINC_0               0x01003400C0ULL
#define ESR_FCC_CREDINC_1               0x01003400C8ULL
#define ESR_FCC_CREDINC_2               0x01003400D0ULL
#define ESR_FCC_CREDINC_3               0x01003400D8ULL
#define ESR_FAST_LOCAL_BARRIER0         0x0100340100ULL
#define ESR_FAST_LOCAL_BARRIER1         0x0100340108ULL
#define ESR_FAST_LOCAL_BARRIER2         0x0100340110ULL
#define ESR_FAST_LOCAL_BARRIER3         0x0100340118ULL
#define ESR_FAST_LOCAL_BARRIER4         0x0100340120ULL
#define ESR_FAST_LOCAL_BARRIER5         0x0100340128ULL
#define ESR_FAST_LOCAL_BARRIER6         0x0100340130ULL
#define ESR_FAST_LOCAL_BARRIER7         0x0100340138ULL
#define ESR_FAST_LOCAL_BARRIER8         0x0100340140ULL
#define ESR_FAST_LOCAL_BARRIER9         0x0100340148ULL
#define ESR_FAST_LOCAL_BARRIER10        0x0100340150ULL
#define ESR_FAST_LOCAL_BARRIER11        0x0100340158ULL
#define ESR_FAST_LOCAL_BARRIER12        0x0100340160ULL
#define ESR_FAST_LOCAL_BARRIER13        0x0100340168ULL
#define ESR_FAST_LOCAL_BARRIER14        0x0100340170ULL
#define ESR_FAST_LOCAL_BARRIER15        0x0100340178ULL
#define ESR_FAST_LOCAL_BARRIER16        0x0100340180ULL
#define ESR_FAST_LOCAL_BARRIER17        0x0100340188ULL
#define ESR_FAST_LOCAL_BARRIER18        0x0100340190ULL
#define ESR_FAST_LOCAL_BARRIER19        0x0100340198ULL
#define ESR_FAST_LOCAL_BARRIER20        0x01003401A0ULL
#define ESR_FAST_LOCAL_BARRIER21        0x01003401A8ULL
#define ESR_FAST_LOCAL_BARRIER22        0x01003401B0ULL
#define ESR_FAST_LOCAL_BARRIER23        0x01003401B8ULL
#define ESR_FAST_LOCAL_BARRIER24        0x01003401C0ULL
#define ESR_FAST_LOCAL_BARRIER25        0x01003401C8ULL
#define ESR_FAST_LOCAL_BARRIER26        0x01003401D0ULL
#define ESR_FAST_LOCAL_BARRIER27        0x01003401D8ULL
#define ESR_FAST_LOCAL_BARRIER28        0x01003401E0ULL
#define ESR_FAST_LOCAL_BARRIER29        0x01003401E8ULL
#define ESR_FAST_LOCAL_BARRIER30        0x01003401F0ULL
#define ESR_FAST_LOCAL_BARRIER31        0x01003401F8ULL
#define ESR_MTIME_LOCAL_TARGET          0x01C0340218ULL
#define ESR_SHIRE_POWER_CTRL            0x01C0340220ULL
#define ESR_POWER_CTRL_NEIGH_NSLEEPIN   0x01C0340228ULL
#define ESR_POWER_CTRL_NEIGH_ISOLATION  0x01C0340230ULL
#define ESR_POWER_CTRL_NEIGH_NSLEEPOUT  0x01C0340238ULL
#define ESR_THREAD0_DISABLE             0x01C0340240ULL
#define ESR_SHIRE_ERROR_LOG             0x01C0340248ULL
#define ESR_SHIRE_PLL_AUTO_CONFIG       0x01C0340250ULL
#define ESR_SHIRE_PLL_CONFIG_DATA_0     0x01C0340258ULL
#define ESR_SHIRE_PLL_CONFIG_DATA_1     0x01C0340260ULL
#define ESR_SHIRE_PLL_CONFIG_DATA_2     0x01C0340268ULL
#define ESR_SHIRE_PLL_CONFIG_DATA_3     0x01C0340270ULL
#define ESR_SHIRE_PLL_READ_DATA         0x01C0340288ULL
#define ESR_SHIRE_COOP_MODE             0x0140340290ULL
#define ESR_SHIRE_CTRL_CLOCKMUX         0x01C0340298ULL
#define ESR_SHIRE_CACHE_RAM_CFG1        0x01C03402A0ULL
#define ESR_SHIRE_CACHE_RAM_CFG2        0x01C03402A8ULL
#define ESR_SHIRE_CACHE_RAM_CFG3        0x01C03402B0ULL
#define ESR_SHIRE_CACHE_RAM_CFG4        0x01C03402B8ULL
#define ESR_SHIRE_NOC_INTERRUPT_STATUS  0x01C03402C0ULL
#define ESR_SHIRE_DLL_AUTO_CONFIG       0x01C03402C8ULL
#define ESR_SHIRE_DLL_CONFIG_DATA_0     0x01C03402D0ULL
#define ESR_SHIRE_DLL_CONFIG_DATA_1     0x01C03402D8ULL
#define ESR_SHIRE_DLL_READ_DATA         0x01C03402E0ULL
#define ESR_UC_CONFIG                   0x01403402E8ULL
#define ESR_ICACHE_UPREFETCH            0x01003402F8ULL
#define ESR_ICACHE_SPREFETCH            0x0140340300ULL
#define ESR_ICACHE_MPREFETCH            0x01C0340308ULL
#define ESR_CLK_GATE_CTRL               0x01C0340310ULL
#define ESR_SHIRE_CHANNEL_ECO_CTL       0x01C0340340ULL
#define ESR_AND_OR_TREE_L1              0x018035FF80ULL

// IOshire ESR addresses
#define ESR_PU_RVTIM_MTIME      0x01C0000000ULL
#define ESR_PU_RVTIM_MTIMECMP   0x01C0000008ULL

// Memshire ESR addresses
#define ESR_MS_MEM_CTL               0x0180000000ULL
#define ESR_MS_ATOMIC_SM_CTL         0x0180000008ULL
#define ESR_MS_MEM_REVISION_ID       0x0180000010ULL
#define ESR_MS_CLK_GATE_CTL          0x0180000018ULL
#define ESR_MS_MEM_STATUS            0x0180000020ULL

// DDR controller ESR addresses
#define ESR_DDRC_RESET_CTL           0x0180000200ULL
#define ESR_DDRC_CLOCK_CTL           0x0180000208ULL
#define ESR_DDRC_MAIN_CTL            0x0180000210ULL
#define ESR_DDRC_SCRUB1              0x0180000218ULL
#define ESR_DDRC_SCRUB               0x0180000220ULL
#define ESR_DDRC_U0_MRR_DATA         0x0180000228ULL
#define ESR_DDRC_U1_MRR_DATA         0x0180000230ULL
#define ESR_DDRC_MRR_STATUS          0x0180000238ULL
#define ESR_DDRC_INT_STATUS          0x0180000240ULL
#define ESR_DDRC_CRTIT_INT_EN        0x0180000248ULL
#define ESR_DDRC_INT_EN              0x0180000250ULL
#define ESR_DDRC_ERR_INT_LOG         0x0180000258ULL
#define ESR_DDRC_DEBUG_SIGS_MASK0    0x0180000260ULL
#define ESR_DDRC_DEBUG_SIGS_MASK1    0x0180000268ULL
#define ESR_DDRC_SCRATCH             0x01c0000270ULL
#define ESR_DDRC_TRACE_CTL           0x0180000278ULL
#define ESR_DDRC_CRTIT2_INT_EN       0x01fa000280ULL
#define ESR_DDRC_PERFMON_CTRL_STATUS 0x01C0000280ULL
#define ESR_DDRC_PERFMON_CYC_CNTR    0x01C0000288ULL
#define ESR_DDRC_PERFMON_P0_CNTR     0x01C0000290ULL
#define ESR_DDRC_PERFMON_P1_CNTR     0x01C0000298ULL
#define ESR_DDRC_PERFMON_P0_QUAL     0x01C00002A0ULL
#define ESR_DDRC_PERFMON_P1_QUAL     0x01C00002A8ULL
#define ESR_DDRC_PERFMON_P0_QUAL2    0x01C00002B0ULL
#define ESR_DDRC_PERFMON_P1_QUAL2    0x01C00002B8ULL

// Broadcast ESR addresses
#define ESR_BROADCAST_DATA      0x013FF5FFF0ULL
#define ESR_UBROADCAST          0x013FF5FFF8ULL
#define ESR_SBROADCAST          0x017FF5FFF8ULL
#define ESR_MBROADCAST          0x01FFF5FFF8ULL

#define ESR_SHIRE_RESET_MASK   0x1E00


static uint64_t legalize_esr_address(const Agent& agent, uint64_t addr)
{
    uint64_t shire = addr & ESR_REGION_SHIRE_MASK;
    if (shire == ESR_REGION_SHIRE_MASK) {
        try {
            const Hart& cpu = dynamic_cast<const Hart&>(agent);
            shire = uint64_t(cpu.shireid()) << ESR_REGION_SHIRE_SHIFT;
        }
        catch (const std::bad_cast&) {
            throw memory_error(addr);
        }
        if (shireid_is_ioshire(shire >> ESR_REGION_SHIRE_SHIFT)) {
            throw memory_error(addr);
        }
        return (addr & ~ESR_REGION_SHIRE_MASK) | shire;
    }
    return addr;
}


static uint64_t decode_broadcast_esr_value(uint64_t pp, uint64_t value)
{
    uint64_t x = (value & ESR_BROADCAST_ESR_SREGION_MASK) >> ESR_BROADCAST_ESR_SREGION_MASK_SHIFT;
    uint64_t r = (value & ESR_BROADCAST_ESR_ADDR_MASK) >> ESR_BROADCAST_ESR_ADDR_SHIFT;
    return ESR_REGION_BASE
            | (pp << ESR_REGION_PROT_SHIFT)
            | (x << ESR_SREGION_EXT_SHIFT)
            | (r << 3);
}


void neigh_esrs_t::debug_reset()
{
    hactrl = 0;
    hastatus0 = 0;
    hastatus1 = 0;
}


void neigh_esrs_t::warm_reset()
{
    ipi_redirect_pc = 0;
    texture_image_table_ptr = 0;
    texture_control = ESR_TEXTURE_CONTROL_RESET_VAL;
    texture_status = 0;
    vmspagesize = 0;
    pmu_ctrl = false;
}


void neigh_esrs_t::cold_reset()
{
    minion_boot = ESR_NEIGH_MINION_BOOT_RESET_VAL;
    icache_err_log_ctl = ESR_ICACHE_ERR_LOG_CTL_RESET_VAL;
    mprot = ESR_MPROT_RESET_VAL;
    neigh_chicken = 0;
}


void shire_cache_esrs_t::cold_reset()
{
    for (int i = 0; i < 4; ++i) {
        bank[i].sc_l2_cache_ctl = ESR_SC_L2_CACHE_CTL_RESET_VAL;
        bank[i].sc_l3_cache_ctl = ESR_SC_L3_CACHE_CTL_RESET_VAL;
        bank[i].sc_l3_shire_swizzle_ctl = ESR_SC_L3_SHIRE_SWIZZLE_CTL_RESET_VAL;
        bank[i].sc_pipe_ctl = ESR_SC_PIPE_CTL_RESET_VAL;
        bank[i].sc_scp_cache_ctl = ESR_SC_SCP_CACHE_CTL_RESET_VAL;
        bank[i].sc_reqq_ctl = ESR_SC_REQQ_CTL_RESET_VAL;
        bank[i].sc_err_log_ctl = ESR_SC_ERR_LOG_CTL_RESET_VAL;
        bank[i].sc_eco_ctl = 0;
    }
}


void shire_other_esrs_t::warm_reset()
{
    for (int i = 0; i < 32; ++i) {
        fast_local_barrier[i] = 0;
    }
    ipi_redirect_filter = ESR_FILTER_IPI_RESET_VAL;
    ipi_trigger = 0;
    shire_coop_mode = false;
    icache_prefetch_active = false;
}


void shire_other_esrs_t::cold_reset(unsigned shire)
{
    shire_cache_ram_cfg1 = ESR_SHIRE_CACHE_RAM_CFG1_RESET_VAL;
    shire_cache_ram_cfg3 = ESR_SHIRE_CACHE_RAM_CFG3_RESET_VAL;
    shire_cache_ram_cfg4 = ESR_SHIRE_CACHE_RAM_CFG4_RESET_VAL;
    shire_cache_ram_cfg2 = ESR_SHIRE_CACHE_RAM_CFG2_RESET_VAL;
    shire_config = ESR_SHIRE_CONFIG_CONST_RESET_VAL | shireid(shire);
    thread0_disable = 0xffffffff;
    thread1_disable = 0xffffffff;
    mtime_local_target = 0;
    power_ctrl_neigh_nsleepin = 0;
    power_ctrl_neigh_isolation = 0;
    shire_pll_auto_config = 0;
    shire_dll_auto_config = 0;
    shire_power_ctrl = 0;
    clk_gate_ctrl = ESR_SHIRE_CLK_GATE_CTRL_RESET_VAL;
    // XXX: This should be either ID or IDX?
    if (shireid_is_ioshire(shire) || shireindex_is_ioshire(shire)) {
        minion_feature = 0x3b;
    } else {
        minion_feature = 0x01;
    }
    shire_ctrl_clockmux = 0;
    shire_channel_eco_ctl = 0;
    uc_config = false;
}

#if EMU_HAS_MEMSHIRE
void mem_shire_esrs_t::cold_reset()
{
    status = 0x1;
    int_en = 0;
    perf_ctrl_status = 0;
}
#endif


uint64_t System::esr_read(const Agent& agent, uint64_t addr)
{
    // Redirect local shire requests to the corresponding shire
    uint64_t addr2 = legalize_esr_address(agent, addr);
    unsigned shire = shireindex((addr2 & ESR_REGION_SHIRE_MASK) >> ESR_REGION_SHIRE_SHIFT);

    // Broadcast is special...
    switch (addr) {
    case ESR_BROADCAST_DATA:
    case ESR_UBROADCAST:
    case ESR_SBROADCAST:
    case ESR_MBROADCAST:
        return 0;
    default:
        break;
    }

#if EMU_HAS_PU
    // addr[21] == 0 means accessing the R_PU_RVTim ESRs
    if (shireindex_is_ioshire(shire) && (~addr2 & 0x200000ULL)) {
        uint64_t esr = addr2 & ESR_IOSHIRE_ESR_MASK;
#ifdef SYS_EMU
        switch (esr) {
        case ESR_PU_RVTIM_MTIME:
            return memory.pu_rvtimer_read_mtime();
        case ESR_PU_RVTIM_MTIMECMP:
            return memory.pu_rvtimer_read_mtimecmp();
        }
#else
        switch (esr) {
        case ESR_PU_RVTIM_MTIME:
        case ESR_PU_RVTIM_MTIMECMP:
            throw sysreg_error(esr);
        }
#endif
        WARN_AGENT(esrs, agent, "Read unknown R_PU_RVTim ESR 0x%" PRIx64, esr);
        throw memory_error(addr);
    }
#endif

#if EMU_HAS_MEMSHIRE
    if ((shire >= EMU_NUM_SHIRES) && !((shire >= EMU_MEM_SHIRE_BASE_ID) && (shire < (EMU_MEM_SHIRE_BASE_ID + NUM_MEM_SHIRES)))) {
        WARN_AGENT(esrs, agent, "Read illegal ESR S%u:0x%llx", shireid(shire), addr2 & ~ESR_REGION_SHIRE_MASK);
        throw memory_error(addr);
    }
    uint64_t msregion = addr2 & ESR_MSREGION_MASK;

    if ((shire >= EMU_MEM_SHIRE_BASE_ID) && (shire < (EMU_MEM_SHIRE_BASE_ID + NUM_MEM_SHIRES))) {
        uint64_t esr = addr2 & ESR_SHIRE_ESR_MASK;
        if (msregion == ESR_DDRC_REGION) {
            switch (esr) {
                case ESR_DDRC_RESET_CTL:
                case ESR_DDRC_CLOCK_CTL:
                case ESR_DDRC_MAIN_CTL:
                case ESR_DDRC_SCRUB1:
                case ESR_DDRC_SCRUB:
                case ESR_DDRC_U0_MRR_DATA:
                case ESR_DDRC_U1_MRR_DATA:
                case ESR_DDRC_ERR_INT_LOG:
                case ESR_DDRC_DEBUG_SIGS_MASK0:
                case ESR_DDRC_DEBUG_SIGS_MASK1:
                case ESR_DDRC_SCRATCH:
                case ESR_DDRC_TRACE_CTL:
                case ESR_DDRC_PERFMON_CYC_CNTR:
                case ESR_DDRC_PERFMON_P0_CNTR:
                case ESR_DDRC_PERFMON_P1_CNTR:
                case ESR_DDRC_PERFMON_P0_QUAL:
                case ESR_DDRC_PERFMON_P1_QUAL:
                case ESR_DDRC_PERFMON_P0_QUAL2:
                case ESR_DDRC_PERFMON_P1_QUAL2:
                    return 0;
                case ESR_DDRC_PERFMON_CTRL_STATUS:
                    return mem_shire_esrs.perf_ctrl_status;
                case ESR_DDRC_MRR_STATUS:
                    return mem_shire_esrs.status;
                case ESR_DDRC_INT_EN:
                case ESR_DDRC_CRTIT_INT_EN:
                case ESR_DDRC_CRTIT2_INT_EN:
                    return mem_shire_esrs.int_en;
                default:
                    WARN_AGENT(esrs, agent, "Read unknown DDRC ESR S%u:0x%" PRIx64, shireid(shire), esr);
                    throw memory_error(addr);
            }
        } else if (msregion == ESR_MEMSHIRE_REGION) {
            switch (esr) {
                case ESR_MS_MEM_CTL:
                case ESR_MS_ATOMIC_SM_CTL:
                case ESR_MS_MEM_REVISION_ID:
                case ESR_MS_CLK_GATE_CTL:
                case ESR_MS_MEM_STATUS:
                    return 0;
                default:
                    WARN_AGENT(esrs, agent, "Read unknown MEM Shire ESR S%u:0x%" PRIx64, shireid(shire), esr);
                    throw memory_error(addr);
            }
        } else {
            WARN_AGENT(esrs, agent, "Read unknown MEM Shire region S%u:0x%" PRIx64, shireid(shire), esr);
            throw memory_error(addr);
        }
    }
#endif // EMU_HAS_MEMSHIRE

    uint64_t sregion = addr2 & ESR_SREGION_MASK;

    if (sregion == ESR_HART_REGION) {
        uint64_t esr = addr2 & ESR_HART_ESR_MASK;
        unsigned hart = (addr2 & ESR_REGION_HART_MASK) >> ESR_REGION_HART_SHIFT;
        switch (esr) {
        case ESR_ABSCMD:
        case ESR_NXPROGBUF0:
        case ESR_NXPROGBUF1:
        case ESR_AXPROGBUF0:
        case ESR_AXPROGBUF1:
            return 0;
        case ESR_NXDATA0:
        case ESR_AXDATA0:
            return cpu[hart + shire * EMU_THREADS_PER_SHIRE].ddata0 & 0xFFFFFFFF;
        case ESR_NXDATA1:
        case ESR_AXDATA1:
            return cpu[hart + shire * EMU_THREADS_PER_SHIRE].ddata0 >> 32;
        default:
            WARN_AGENT(esrs, agent, "Read unknown hart ESR S%u:M%u:T%u:0x%" PRIx64,
                      shireid(shire), MINION(hart), THREAD(hart), esr);
            throw memory_error(addr);
        }
    }

    if (sregion == ESR_NEIGH_REGION) {
        unsigned neigh = (addr2 & ESR_REGION_NEIGH_MASK) >> ESR_REGION_NEIGH_SHIFT;
        uint64_t esr = addr2 & ESR_NEIGH_ESR_MASK;
        if (shireindex_is_ioshire(shire) || (neigh >= EMU_NEIGH_PER_SHIRE)) {
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
            return neigh_esrs[pos].vmspagesize;
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
        case ESR_TEXTURE_CONTROL:
            return neigh_esrs[pos].texture_control;
        case ESR_TEXTURE_STATUS:
            return neigh_esrs[pos].texture_status;
        case ESR_TEXTURE_IMAGE_TABLE_PTR:
            return neigh_esrs[pos].texture_image_table_ptr;
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

    uint64_t sregion_extra = addr2 & ESR_SREGION_EXT_MASK;

    if (sregion_extra == ESR_CACHE_REGION) {
        uint64_t esr = addr2 & ESR_CACHE_ESR_MASK;
        unsigned bnk = (addr2 & ESR_REGION_BANK_MASK) >> ESR_REGION_BANK_SHIFT;
        if (bnk >= 4) {
            WARN_AGENT(esrs, agent, "Read illegal shire_cache ESR S%u:B%u:0x%" PRIx64, shireid(shire), bnk, esr);
            throw memory_error(addr);
        }
        switch (esr) {
        case ESR_SC_L3_SHIRE_SWIZZLE_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_l3_shire_swizzle_ctl;
        case ESR_SC_REQQ_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_reqq_ctl;
        case ESR_SC_PIPE_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_pipe_ctl;
        case ESR_SC_L2_CACHE_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_l2_cache_ctl;
        case ESR_SC_L3_CACHE_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_l3_cache_ctl;
        case ESR_SC_SCP_CACHE_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_scp_cache_ctl;
        case ESR_SC_IDX_COP_SM_CTL:
            return 4ull << 24; // idx_cop_sm_state = IDLE
        case ESR_SC_IDX_COP_SM_PHYSICAL_INDEX:
            return shire_cache_esrs[shire].bank[bnk].sc_idx_cop_sm_physical_index;
        case ESR_SC_IDX_COP_SM_DATA0:
            return shire_cache_esrs[shire].bank[bnk].sc_idx_cop_sm_data0;
        case ESR_SC_IDX_COP_SM_DATA1:
            return shire_cache_esrs[shire].bank[bnk].sc_idx_cop_sm_data1;
        case ESR_SC_IDX_COP_SM_ECC:
            return shire_cache_esrs[shire].bank[bnk].sc_idx_cop_sm_ecc;
        case ESR_SC_ERR_LOG_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_err_log_ctl;
        case ESR_SC_ERR_LOG_INFO:
            return shire_cache_esrs[shire].bank[bnk].sc_err_log_info;
        case ESR_SC_ERR_LOG_ADDRESS:
            return 0;
        case ESR_SC_SBE_DBE_COUNTS:
            return shire_cache_esrs[shire].bank[bnk].sc_sbe_dbe_counts;
        case ESR_SC_REQQ_DEBUG_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_reqq_debug_ctl;
        case ESR_SC_REQQ_DEBUG0:
        case ESR_SC_REQQ_DEBUG1:
        case ESR_SC_REQQ_DEBUG2:
        case ESR_SC_REQQ_DEBUG3:
            return 0;
        case ESR_SC_ECO_CTL:
            return shire_cache_esrs[shire].bank[bnk].sc_eco_ctl;
        case ESR_SC_PERFMON_CTL_STATUS:
            return shire_cache_esrs[shire].bank[bnk].sc_perfmon_ctl_status;
        case ESR_SC_PERFMON_CYC_CNTR:
            return shire_cache_esrs[shire].bank[bnk].sc_perfmon_cyc_cntr;
        case ESR_SC_PERFMON_P0_CNTR:
            return shire_cache_esrs[shire].bank[bnk].sc_perfmon_p0_cntr;
        case ESR_SC_PERFMON_P1_CNTR:
            return shire_cache_esrs[shire].bank[bnk].sc_perfmon_p1_cntr;
        case ESR_SC_PERFMON_P0_QUAL:
            return shire_cache_esrs[shire].bank[bnk].sc_perfmon_p0_qual;
        case ESR_SC_PERFMON_P1_QUAL:
            return shire_cache_esrs[shire].bank[bnk].sc_perfmon_p1_qual;
        case ESR_SC_IDX_COP_SM_CTL_USER:
            return 4ull << 24; // idx_cop_sm_state = IDLE
        }
        WARN_AGENT(esrs, agent, "Read unknown shire_cache ESR S%u:B%u:0x%" PRIx64, shireid(shire), bnk, esr);
        throw memory_error(addr);
    }

    if (sregion_extra == ESR_RBOX_REGION) {
        uint64_t esr = addr2 & ESR_RBOX_ESR_MASK;
        if (shire >= EMU_NUM_COMPUTE_SHIRES) {
            WARN_AGENT(esrs, agent, "Read illegal rbox ESR S%u:0x%" PRIx64, shireid(shire), esr);
            throw memory_error(addr);
        }
        switch (esr) {
        case ESR_RBOX_CONFIG:
        case ESR_RBOX_IN_BUF_PG:
        case ESR_RBOX_IN_BUF_CFG:
        case ESR_RBOX_OUT_BUF_PG:
        case ESR_RBOX_OUT_BUF_CFG:
        case ESR_RBOX_START:
        case ESR_RBOX_CONSUME:
        case ESR_RBOX_STATUS:
            return 0; //GET_RBOX(shire, 0).read_esr((esr >> 3) & 0x3FFF);
        }
        WARN_AGENT(esrs, agent, "Read unknown rbox ESR S%u:0x%" PRIx64, shireid(shire), esr);
        throw memory_error(addr);
    }

    if (sregion_extra == ESR_SHIRE_REGION) {
        uint64_t esr = addr2 & ESR_SHIRE_ESR_MASK;
        switch (esr) {
        case ESR_MINION_FEATURE:
            return shire_other_esrs[shire].minion_feature;
        case ESR_SHIRE_CONFIG:
            return shire_other_esrs[shire].shire_config;
        case ESR_THREAD1_DISABLE:
            return shire_other_esrs[shire].thread1_disable;
        case ESR_SHIRE_CACHE_BUILD_CONFIG:
            return 0x2040040404040400ull;
        case ESR_SHIRE_CACHE_REVISION_ID:
            return 0x0000000900000001ull;
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
        case ESR_MTIME_LOCAL_TARGET:
            return shire_other_esrs[shire].mtime_local_target;
        case ESR_SHIRE_POWER_CTRL:
            return shire_other_esrs[shire].shire_power_ctrl;
        case ESR_POWER_CTRL_NEIGH_NSLEEPIN:
            return shire_other_esrs[shire].power_ctrl_neigh_nsleepin;
        case ESR_POWER_CTRL_NEIGH_ISOLATION:
            return shire_other_esrs[shire].power_ctrl_neigh_isolation;
        case ESR_POWER_CTRL_NEIGH_NSLEEPOUT:
            return 0;
        case ESR_THREAD0_DISABLE:
            return shire_other_esrs[shire].thread0_disable;
        case ESR_SHIRE_ERROR_LOG:
            return 0;
        case ESR_SHIRE_PLL_AUTO_CONFIG:
            return shire_other_esrs[shire].shire_pll_auto_config;
        case ESR_SHIRE_PLL_CONFIG_DATA_0:
        case ESR_SHIRE_PLL_CONFIG_DATA_1:
        case ESR_SHIRE_PLL_CONFIG_DATA_2:
        case ESR_SHIRE_PLL_CONFIG_DATA_3:
            return shire_other_esrs[shire].shire_pll_config_data[(esr - ESR_SHIRE_PLL_CONFIG_DATA_0)>>3];
        case ESR_SHIRE_PLL_READ_DATA:
            return 0x20000; /* PLL is locked */
        case ESR_SHIRE_COOP_MODE:
            return shire_other_esrs[shire].shire_coop_mode;
        case ESR_SHIRE_CTRL_CLOCKMUX:
            return shire_other_esrs[shire].shire_ctrl_clockmux;
        case ESR_SHIRE_CACHE_RAM_CFG1:
            return shire_other_esrs[shire].shire_cache_ram_cfg1;
        case ESR_SHIRE_CACHE_RAM_CFG2:
            return shire_other_esrs[shire].shire_cache_ram_cfg2;
        case ESR_SHIRE_CACHE_RAM_CFG3:
            return shire_other_esrs[shire].shire_cache_ram_cfg3;
        case ESR_SHIRE_CACHE_RAM_CFG4:
            return shire_other_esrs[shire].shire_cache_ram_cfg4;
        case ESR_SHIRE_NOC_INTERRUPT_STATUS:
            return 0;
        case ESR_SHIRE_DLL_AUTO_CONFIG:
            return shire_other_esrs[shire].shire_dll_auto_config;
        case ESR_SHIRE_DLL_CONFIG_DATA_0:
            return shire_other_esrs[shire].shire_dll_config_data_0;
        case ESR_SHIRE_DLL_CONFIG_DATA_1:
            return shire_other_esrs[shire].shire_dll_config_data_1;
        case ESR_SHIRE_DLL_READ_DATA:
            return 0x20000; /* DLL is locked */
        case ESR_UC_CONFIG:
            return shire_other_esrs[shire].uc_config;
        case ESR_ICACHE_UPREFETCH:
            return read_icache_prefetch(Privilege::U, shire);
        case ESR_ICACHE_SPREFETCH:
            return read_icache_prefetch(Privilege::S, shire);
        case ESR_ICACHE_MPREFETCH:
            return read_icache_prefetch(Privilege::M, shire);
        case ESR_CLK_GATE_CTRL:
            return shire_other_esrs[shire].clk_gate_ctrl;
        case ESR_SHIRE_CHANNEL_ECO_CTL:
            return shire_other_esrs[shire].shire_channel_eco_ctl;
#if EMU_ETSOC1
        case ESR_AND_OR_TREE_L1:
            return calculate_andortree1(shire);
#endif
        }
        WARN_AGENT(esrs, agent, "Read unknown shire_other ESR S%u:0x%" PRIx64, shireid(shire), esr);
        throw memory_error(addr);
    }

    WARN_AGENT(esrs, agent, "Read illegal ESR 0x%" PRIx64, addr);
    throw memory_error(addr);
}


void System::esr_write(const Agent& agent, uint64_t addr, uint64_t value)
{
    // Redirect local shire requests to the corresponding shire
    uint64_t addr2 = legalize_esr_address(agent, addr);
    unsigned shire = shireindex((addr2 & ESR_REGION_SHIRE_MASK) >> ESR_REGION_SHIRE_SHIFT);

    // Broadcast is special...
    switch (addr) {
    case ESR_BROADCAST_DATA:
        broadcast_esrs[shire].data = value;
        LOG_AGENT(DEBUG, agent, "broadcast_data = 0x%" PRIx64, value);
        return;
    case ESR_UBROADCAST:
    case ESR_SBROADCAST:
    case ESR_MBROADCAST:
        {
            uint64_t emask;
            uint64_t eaddr;
            eaddr = decode_broadcast_esr_value(PP(addr), value);
            LOG_AGENT(DEBUG, agent, "%cbroadcast = 0x%" PRIx64, "uhsm"[PP(addr)], value);
            emask = value & ESR_BROADCAST_ESR_SHIRE_MASK;
            while (emask) {
                if (emask & 1) {
                    esr_write(agent, eaddr, broadcast_esrs[shire].data);
                }
                eaddr += 1ull << ESR_REGION_SHIRE_SHIFT;
                emask >>= 1;
            }
            return;
        }
    }

#if EMU_HAS_PU
    // addr[21] == 0 means accessing the R_PU_RVTim ESRs
    if (shireindex_is_ioshire(shire) && (~addr2 & 0x200000ULL)) {
        uint64_t esr = addr2 & ESR_IOSHIRE_ESR_MASK;
#ifdef SYS_EMU
        switch (esr) {
        case ESR_PU_RVTIM_MTIME:
            memory.pu_rvtimer_write_mtime(agent, value);
            return;
        case ESR_PU_RVTIM_MTIMECMP:
            memory.pu_rvtimer_write_mtimecmp(agent, value);
            return;
        }
#else
        switch (esr) {
        case ESR_PU_RVTIM_MTIME:
        case ESR_PU_RVTIM_MTIMECMP:
            throw sysreg_error(esr);
        }
#endif
        WARN_AGENT(esrs, agent, "Write unknown R_PU_RVTim ESR 0x%" PRIx64, esr);
        throw memory_error(addr);
    }
#endif // EMU_HAS_PU

#if EMU_HAS_MEMSHIRE
    if ((shire >= EMU_NUM_SHIRES) && !((shire >= EMU_MEM_SHIRE_BASE_ID) && (shire < (EMU_MEM_SHIRE_BASE_ID + NUM_MEM_SHIRES)))) {
        WARN_AGENT(esrs, agent, "Write illegal ESR S%u:0x%llx", shireid(shire), addr2 & ~ESR_REGION_SHIRE_MASK);
        throw memory_error(addr);
    }

    uint64_t msregion = addr2 & ESR_MSREGION_MASK;

    if ((shire >= EMU_MEM_SHIRE_BASE_ID) && (shire < (EMU_MEM_SHIRE_BASE_ID + NUM_MEM_SHIRES))) {
        uint64_t esr = addr2 & ESR_SHIRE_ESR_MASK;
        if (msregion == ESR_DDRC_REGION) {
            switch (esr) {
                case ESR_DDRC_RESET_CTL:
                case ESR_DDRC_CLOCK_CTL:
                case ESR_DDRC_MAIN_CTL:
                case ESR_DDRC_SCRUB1:
                case ESR_DDRC_SCRUB:
                case ESR_DDRC_U0_MRR_DATA:
                case ESR_DDRC_U1_MRR_DATA:
                case ESR_DDRC_ERR_INT_LOG:
                case ESR_DDRC_DEBUG_SIGS_MASK0:
                case ESR_DDRC_DEBUG_SIGS_MASK1:
                case ESR_DDRC_SCRATCH:
                case ESR_DDRC_TRACE_CTL:
                case ESR_DDRC_PERFMON_CYC_CNTR:
                case ESR_DDRC_PERFMON_P0_CNTR:
                case ESR_DDRC_PERFMON_P1_CNTR:
                case ESR_DDRC_PERFMON_P0_QUAL:
                case ESR_DDRC_PERFMON_P1_QUAL:
                case ESR_DDRC_PERFMON_P0_QUAL2:
                case ESR_DDRC_PERFMON_P1_QUAL2:
                    // Dummy access, do nothing
                    break;
                case ESR_DDRC_PERFMON_CTRL_STATUS:
                    mem_shire_esrs.perf_ctrl_status = value;
                    LOG_AGENT(DEBUG, agent, "S%u:perf_ctrl_status = 0x%" PRIx64,
                            shireid(shire), mem_shire_esrs.perf_ctrl_status);
                    break;
                case ESR_DDRC_INT_EN:
                case ESR_DDRC_CRTIT_INT_EN:
                case ESR_DDRC_CRTIT2_INT_EN:
                    mem_shire_esrs.int_en = value;
                    LOG_AGENT(DEBUG, agent, "S%u:int_en = 0x%" PRIx64,
                            shireid(shire), mem_shire_esrs.int_en);
                    break;
                default:
                    WARN_AGENT(esrs, agent, "Write unknown DDRC ESR S%u:0x%" PRIx64, shireid(shire), esr);
                    throw memory_error(addr);
            }
        } else if (msregion == ESR_MEMSHIRE_REGION) {
            switch (esr) {
                case ESR_MS_MEM_CTL:
                case ESR_MS_ATOMIC_SM_CTL:
                case ESR_MS_MEM_REVISION_ID:
                case ESR_MS_CLK_GATE_CTL:
                case ESR_MS_MEM_STATUS:
                    // Dummy access, do nothing
                    break;
                default:
                    LOG_AGENT(WARN, agent, "Write unknown MEM Shire ESR S%u:0x%" PRIx64, shireid(shire), esr);
                    WARN_AGENT(esrs, agent, "Write unknown MEM Shire ESR S%u:0x%" PRIx64, shireid(shire), esr);
                    throw memory_error(addr);
            }
        } else {
            WARN_AGENT(esrs, agent, "Write unknown MEM Shire region S%u:0x%" PRIx64, shireid(shire), esr);
            throw memory_error(addr);
        }
        return;
    }
#endif // EMU_HAS_MEMSHIRE

    uint64_t sregion = addr2 & ESR_SREGION_MASK;

    if (sregion == ESR_HART_REGION) {
        uint64_t esr = addr2 & ESR_HART_ESR_MASK;
        unsigned hart = (addr2 & ESR_REGION_HART_MASK) >> ESR_REGION_HART_SHIFT;
        if (!shireindex_is_ioshire(shire) && (esr >= ESR_PORT0) && (esr <= ESR_PORT3)) {
            unsigned hartid = hart + shire * EMU_THREADS_PER_SHIRE;
            unsigned port = (esr - ESR_PORT0) >> 6;
            if (get_msg_port_write_width(cpu[hartid], port) > 8)
                throw std::runtime_error("Write to port with incompatible size");
            uint64_t tmp = value;
            try {
                const Hart& cpu = dynamic_cast<const Hart&>(agent);
                write_msg_port_data(hartid, port, hart_index(cpu), (uint32_t*)&tmp);
            }
            catch (const std::bad_cast&) {
                throw memory_error(addr);
            }
            return;
        }
        switch (esr) {
        case ESR_ABSCMD:
        case ESR_NXPROGBUF0:
        case ESR_NXPROGBUF1:
        case ESR_AXPROGBUF0:
        case ESR_AXPROGBUF1: {
            unsigned hartid = hart + shire * EMU_THREADS_PER_SHIRE;
            if (cpu[hartid].in_progbuf()) {
                cpu[hartid].exit_progbuf(Hart::Progbuf::error);
            } else if (cpu[hartid].is_halted()) {
                cpu[hartid].write_progbuf(esr, value);
                if (esr < ESR_NXPROGBUF0 || esr > ESR_NXPROGBUF1) {
                    cpu[hartid].enter_progbuf();
                }
            }
            // FIXME(cabul): What happens if the cpu is not halted?
            break;
        }
        case ESR_NXDATA0:
        case ESR_AXDATA0: {
            unsigned hartid = hart + shire * EMU_THREADS_PER_SHIRE;
            cpu[hartid].ddata0 &= 0xFFFFFFFF00000000ull;
            cpu[hartid].ddata0 |= value & 0xFFFFFFFFull;
            if (esr == ESR_AXDATA0) cpu[hartid].enter_progbuf();
            LOG_AGENT(DEBUG, agent, "S%u:H%u:data0 = 0x%" PRIx64, shireid(shire), hart, value);
            break;
        }
        case ESR_NXDATA1:
        case ESR_AXDATA1: {
            unsigned hartid = hart + shire * EMU_THREADS_PER_SHIRE;
            cpu[hartid].ddata0 &= 0xFFFFFFFFull;
            cpu[hartid].ddata0 |= value << 32;
            LOG_AGENT(DEBUG, agent, "S%u:H%u:data1 = 0x%" PRIx64, shireid(shire), hart, value);
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
        unsigned neigh = (addr2 & ESR_REGION_NEIGH_MASK) >> ESR_REGION_NEIGH_SHIFT;
        uint64_t esr = addr2 & ESR_NEIGH_ESR_MASK;
        unsigned frst = neigh + EMU_NEIGH_PER_SHIRE * shire;
        unsigned last = frst + 1;
        if (shireindex_is_ioshire(shire)) {
            WARN_AGENT(esrs, agent, "Write illegal neigh ESR S%u:N%u:0x%" PRIx64, shireid(shire), neigh, esr);
            throw memory_error(addr);
        }
        if (neigh == ESR_REGION_NEIGH_BROADCAST) {
            frst = EMU_NEIGH_PER_SHIRE * shire;
            last = frst + EMU_NEIGH_PER_SHIRE;
        } else if (neigh >= EMU_NEIGH_PER_SHIRE) {
            WARN_AGENT(esrs, agent, "Write illegal neigh ESR S%u:N%u:0x%" PRIx64, shireid(shire), neigh, esr);
            throw memory_error(addr);
        }
        for (unsigned pos = frst; pos < last; ++pos) {
            switch (esr) {
            case ESR_DUMMY0:
                neigh_esrs[pos].dummy0 = uint32_t(value & 0xffffffff);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:dummy0 = 0x%" PRIx32,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].dummy0);
                break;
            case ESR_MINION_BOOT:
                neigh_esrs[pos].minion_boot = value & VA_M;
                LOG_AGENT(DEBUG, agent, "S%u:N%u:minion_boot = 0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].minion_boot);
                break;
            case ESR_MPROT:
                neigh_esrs[pos].mprot = uint8_t(value & 0x7f);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:mprot = 0x%" PRIx8,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].mprot);
                break;
            case ESR_DUMMY2:
                neigh_esrs[pos].dummy2 = bool(value & 1);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:dummy2 = 0x%x",
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].dummy2 ? 1 : 0);
                break;
            case ESR_VMSPAGESIZE:
                neigh_esrs[pos].vmspagesize = value & 0x3;
                LOG_AGENT(DEBUG, agent, "S%u:N%u:vmspagesize = 0x%" PRIx8,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].vmspagesize);
                break;
            case ESR_IPI_REDIRECT_PC:
                neigh_esrs[pos].ipi_redirect_pc = value & VA_M;
                LOG_AGENT(DEBUG, agent, "S%u:N%u:ipi_redirect_pc = 0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].ipi_redirect_pc);
                break;
            case ESR_PMU_CTRL:
                neigh_esrs[pos].pmu_ctrl = bool(value & 1);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:pmu_ctrl = 0x%x",
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].pmu_ctrl ? 1 : 0);
                break;
            case ESR_NEIGH_CHICKEN:
                neigh_esrs[pos].neigh_chicken = uint8_t(value & 0xff);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:neigh_chicken = 0x%" PRIx8,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].neigh_chicken);
                break;
            case ESR_ICACHE_ERR_LOG_CTL:
                neigh_esrs[pos].icache_err_log_ctl = uint8_t(value & 0xf);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_err_log_ctl = 0x%" PRIx8,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_err_log_ctl);
                break;
            case ESR_ICACHE_ERR_LOG_INFO:
                neigh_esrs[pos].icache_err_log_info = value & 0x0010ff000003ffffull;
                LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_err_log_info = 0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_err_log_info);
                break;
            case ESR_ICACHE_SBE_DBE_COUNTS:
                neigh_esrs[pos].icache_sbe_dbe_counts = uint16_t(value & 0x7ff);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:icache_sbe_dbe_counts = 0x%" PRIx16,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].icache_sbe_dbe_counts);
                break;
            case ESR_TEXTURE_CONTROL:
                neigh_esrs[pos].texture_control = uint16_t(value & 0xff80);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:texture_control = 0x%" PRIx16,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].texture_control);
                break;
            case ESR_TEXTURE_IMAGE_TABLE_PTR:
                neigh_esrs[pos].texture_image_table_ptr = value & VA_M;
                LOG_AGENT(DEBUG, agent, "S%u:N%u:texture_image_table_ptr = 0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].texture_image_table_ptr);
                // try {
                //     const Hart& cpu = dynamic_cast<const Hart&>(agent);
                //     unsigned tbox_id = tbox_id_from_thread(hart_index(cpu));
                //     GET_TBOX(shire, tbox_id).set_image_table_address(value);
                // }
                // catch (const std::bad_cast&) {
                //     throw memory_error(addr);
                // }
                break;
            case ESR_HACTRL:
                neigh_esrs[pos].hactrl = uint64_t(value & 0x0000ffffffffffffull);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:hactrl = 0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].hactrl);
                break;
            case ESR_HASTATUS1:
                neigh_esrs[pos].hastatus1 = uint64_t(value & 0x0000ffffffff0000ull);
                LOG_AGENT(DEBUG, agent, "S%u:N%u:hastatus1 = 0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), neigh_esrs[pos].hastatus1);
                break;
            default:
                WARN_AGENT(esrs, agent, "Write unknown neigh ESR S%u:N%u:0x%" PRIx64,
                          shireid(shire), NEIGHID(pos), esr);
                throw memory_error(addr);
            }
        }
        return;
    }

    uint64_t sregion_extra = addr2 & ESR_SREGION_EXT_MASK;

    if (sregion_extra == ESR_CACHE_REGION) {
        uint64_t esr = addr2 & ESR_CACHE_ESR_MASK;
        unsigned bnk = (addr2 & ESR_REGION_BANK_MASK) >> ESR_REGION_BANK_SHIFT;
        unsigned frst = bnk;
        unsigned last = frst + 1;
        if (bnk == (ESR_REGION_BANK_MASK >> ESR_REGION_BANK_SHIFT)) {
            frst = 0;
            last = frst + 4;
        } else if (bnk >= 4) {
            WARN_AGENT(esrs, agent, "Write illegal shire_cache ESR S%u:B%u:0x%" PRIx64, shireid(shire), bnk, esr);
            throw memory_error(addr);
        }
        for (unsigned b = frst; b < last; ++b) {
            switch (esr) {
            case ESR_SC_L3_SHIRE_SWIZZLE_CTL:
                shire_cache_esrs[shire].bank[b].sc_l3_shire_swizzle_ctl = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_l3_shire_swizzle_ctl = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_l3_shire_swizzle_ctl);
                break;
            case ESR_SC_REQQ_CTL:
                shire_cache_esrs[shire].bank[b].sc_reqq_ctl = uint32_t(value);
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_reqq_ctl = 0x%" PRIx32,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_reqq_ctl);
                break;
            case ESR_SC_PIPE_CTL:
                shire_cache_esrs[shire].bank[b].sc_pipe_ctl = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_pipe_ctl = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_pipe_ctl);
                break;
            case ESR_SC_L2_CACHE_CTL:
                shire_cache_esrs[shire].bank[b].sc_l2_cache_ctl = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_l2_cache_ctl = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_l2_cache_ctl);
                break;
            case ESR_SC_L3_CACHE_CTL:
                shire_cache_esrs[shire].bank[b].sc_l3_cache_ctl = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_l3_cache_ctl = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_l3_cache_ctl);
                break;
            case ESR_SC_SCP_CACHE_CTL:
                shire_cache_esrs[shire].bank[b].sc_scp_cache_ctl = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_scp_cache_ctl = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_scp_cache_ctl);
                break;
            case ESR_SC_IDX_COP_SM_CTL:
#ifdef SYS_EMU
                if (SYS_EMU_PTR->get_mem_check()) {
                    // Doing a CB drain
                    if ((value & 1) && (((value >> 8) & 0xF) == 10)) {
                        SYS_EMU_PTR->get_mem_checker().cb_drain(shire, b);
                    }
                    // Doing an L2 flush
                    else if ((value & 1) && (((value >> 8) & 0xF) == 2)) {
                        SYS_EMU_PTR->get_mem_checker().l2_flush(shire, b);
                    }
                    // Doing an L2 evict
                    else if ((value & 1) && (((value >> 8) & 0xF) == 3)) {
                        SYS_EMU_PTR->get_mem_checker().l2_evict(shire, b);
                    }
                }
#endif
                // shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_ctl = value;
                break;
            case ESR_SC_IDX_COP_SM_PHYSICAL_INDEX:
                shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_physical_index = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_idx_cop_sm_physical_index = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_physical_index);
                break;
            case ESR_SC_IDX_COP_SM_DATA0:
                shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_data0 = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_idx_cop_sm_data0 = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_data0);
                break;
            case ESR_SC_IDX_COP_SM_DATA1:
                shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_data1 = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_idx_cop_sm_data1 = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_data1);
                break;
            case ESR_SC_IDX_COP_SM_ECC:
                shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_ecc = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_idx_cop_sm_ecc = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_ecc);
                break;
            case ESR_SC_ERR_LOG_CTL:
                shire_cache_esrs[shire].bank[b].sc_err_log_ctl = uint16_t(value);
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_err_log_ctl = 0x%" PRIx16,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_err_log_ctl);
                break;
            case ESR_SC_ERR_LOG_INFO:
                shire_cache_esrs[shire].bank[b].sc_err_log_info = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_err_log_info = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_err_log_info);
                break;
            case ESR_SC_SBE_DBE_COUNTS:
                shire_cache_esrs[shire].bank[b].sc_sbe_dbe_counts = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_sbe_dbe_counts = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_sbe_dbe_counts);
                break;
            case ESR_SC_REQQ_DEBUG_CTL:
                shire_cache_esrs[shire].bank[b].sc_reqq_debug_ctl = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_reqq_debug_ctl = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_reqq_debug_ctl);
                break;
            case ESR_SC_ECO_CTL:
                shire_cache_esrs[shire].bank[b].sc_eco_ctl = uint8_t(value);
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_eco_ctl = 0x%" PRIx8,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_eco_ctl);
                break;
            case ESR_SC_PERFMON_CTL_STATUS:
                shire_cache_esrs[shire].bank[b].sc_perfmon_ctl_status = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_perfmon_ctl_status = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_perfmon_ctl_status);
                break;
            case ESR_SC_PERFMON_CYC_CNTR:
                shire_cache_esrs[shire].bank[b].sc_perfmon_cyc_cntr = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_perfmon_cyc_cntr = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_perfmon_cyc_cntr);
                break;
            case ESR_SC_PERFMON_P0_CNTR:
                shire_cache_esrs[shire].bank[b].sc_perfmon_p0_cntr = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_perfmon_p0_cntr = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_perfmon_p0_cntr);
                break;
            case ESR_SC_PERFMON_P1_CNTR:
                shire_cache_esrs[shire].bank[b].sc_perfmon_p1_cntr = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_perfmon_p1_cntr = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_perfmon_p1_cntr);
                break;
            case ESR_SC_PERFMON_P0_QUAL:
                shire_cache_esrs[shire].bank[b].sc_perfmon_p0_qual = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_perfmon_p0_qual = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_perfmon_p0_qual);
                break;
            case ESR_SC_PERFMON_P1_QUAL:
                shire_cache_esrs[shire].bank[b].sc_perfmon_p1_qual = value;
                LOG_AGENT(DEBUG, agent, "S%u:B%u:sc_perfmon_p1_qual = 0x%" PRIx64,
                          shireid(shire), b, shire_cache_esrs[shire].bank[b].sc_perfmon_p1_qual);
                break;
            case ESR_SC_IDX_COP_SM_CTL_USER:
#ifdef SYS_EMU
                if (SYS_EMU_PTR->get_mem_check()) {
                    // Doing a CB drain
                    if ((value & 1) && (((value >> 8) & 0xF) == 10)) {
                        SYS_EMU_PTR->get_mem_checker().cb_drain(shire, b);
                    }
                }
#endif
                // shire_cache_esrs[shire].bank[b].sc_idx_cop_sm_ctl = value;
                break;
             default:
                WARN_AGENT(esrs, agent, "Write unknown shire_cache ESR S%u:B%u:0x%" PRIx64, shireid(shire), bnk, esr);
                throw memory_error(addr);
            }
        }
        return;
    }

    if (sregion_extra == ESR_RBOX_REGION) {
        uint64_t esr = addr2 & ESR_RBOX_ESR_MASK;
        if (shire >= EMU_NUM_COMPUTE_SHIRES) {
            WARN_AGENT(esrs, agent, "Write illegal rbox ESR S%u:0x%" PRIx64, shireid(shire), esr);
            throw memory_error(addr);
        }
        switch (esr) {
        case ESR_RBOX_CONFIG:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_config = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        case ESR_RBOX_IN_BUF_PG:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_in_buf_pg = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        case ESR_RBOX_IN_BUF_CFG:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_in_buf_cfg = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        case ESR_RBOX_OUT_BUF_PG:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_out_buf_pg = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        case ESR_RBOX_OUT_BUF_CFG:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_out_buf_cfg = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        case ESR_RBOX_START:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_start = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        case ESR_RBOX_CONSUME:
            LOG_AGENT(DEBUG, agent, "S%u:rbox_consume = 0x%" PRIx64, shireid(shire), value);
            //GET_RBOX(shire, 0).write_esr((esr >> 3) & 0x3FFF, value);
            return;
        }
        WARN_AGENT(esrs, agent, "Write unknown rbox ESR S%u:0x%" PRIx64, shireid(shire), esr);
        throw memory_error(addr);
    }

    if (sregion_extra == ESR_SHIRE_REGION) {
        uint64_t esr = addr2 & ESR_SHIRE_ESR_MASK;
        switch (esr) {
        case ESR_MINION_FEATURE:
            write_minion_feature(shire, uint8_t(value & 0x3f));
            LOG_AGENT(DEBUG, agent, "S%u:minion_feature = 0x%" PRIx8,
                      shireid(shire), shire_other_esrs[shire].minion_feature);
            return;
        case ESR_SHIRE_CONFIG:
            shire_other_esrs[shire].shire_config = uint32_t(value & 0x3ffffff);
            LOG_AGENT(DEBUG, agent, "S%u:shire_config = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].shire_config);
            return;
        case ESR_THREAD1_DISABLE:
            write_thread1_disable(shire, uint32_t(value));
            LOG_AGENT(DEBUG, agent, "S%u:thread1_disable = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].thread1_disable);
            return;
        case ESR_IPI_REDIRECT_TRIGGER:
            LOG_AGENT(DEBUG, agent, "S%u:ipi_redirect_trigger = 0x%" PRIx64, shireid(shire), value);
            if (!shireindex_is_ioshire(shire)) {
                send_ipi_redirect(shire, value);
            }
            return;
        case ESR_IPI_REDIRECT_FILTER:
            shire_other_esrs[shire].ipi_redirect_filter = value;
            LOG_AGENT(DEBUG, agent, "S%u:ipi_redirect_filter = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].ipi_redirect_filter);
            return;
        case ESR_IPI_TRIGGER:
            shire_other_esrs[shire].ipi_trigger = value;
            LOG_AGENT(DEBUG, agent, "S%u:ipi_trigger = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].ipi_trigger);
            if (!shireindex_is_ioshire(shire)) {
                raise_machine_software_interrupt(shire, value);
            }
            return;
        case ESR_IPI_TRIGGER_CLEAR:
            LOG_AGENT(DEBUG, agent, "S%u:ipi_trigger_clear = 0x%" PRIx64, shireid(shire), value);
            clear_machine_software_interrupt(shire, value);
            return;
        case ESR_FCC_CREDINC_0:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_0 = 0x%" PRIx64, shireid(shire), value);
            if (!shireindex_is_ioshire(shire)) {
                write_fcc_credinc(0, shire, value);
            }
            return;
        case ESR_FCC_CREDINC_1:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_1 = 0x%" PRIx64, shireid(shire), value);
            if (!shireindex_is_ioshire(shire)) {
                write_fcc_credinc(1, shire, value);
            }
            return;
        case ESR_FCC_CREDINC_2:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_2 = 0x%" PRIx64, shireid(shire), value);
            if (!shireindex_is_ioshire(shire)) {
                write_fcc_credinc(2, shire, value);
            }
            return;
        case ESR_FCC_CREDINC_3:
            LOG_AGENT(DEBUG, agent, "S%u:fcc_credinc_3 = 0x%" PRIx64, shireid(shire), value);
            if (!shireindex_is_ioshire(shire)) {
                write_fcc_credinc(3, shire, value);
            }
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
        case ESR_MTIME_LOCAL_TARGET:
            shire_other_esrs[shire].mtime_local_target = uint32_t(value);
            LOG_AGENT(DEBUG, agent, "S%u:mtime_local_target = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].mtime_local_target);
            return;
        case ESR_SHIRE_POWER_CTRL:
            shire_other_esrs[shire].shire_power_ctrl = uint16_t(value & 0xfff);
            LOG_AGENT(DEBUG, agent, "S%u:shire_power_ctrl = 0x%" PRIx16,
                      shireid(shire), shire_other_esrs[shire].shire_power_ctrl);
            return;
        case ESR_POWER_CTRL_NEIGH_NSLEEPIN:
            shire_other_esrs[shire].power_ctrl_neigh_nsleepin = uint32_t(value);
            LOG_AGENT(DEBUG, agent, "S%u:power_ctrl_neigh_nsleepin = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].power_ctrl_neigh_nsleepin);
            return;
        case ESR_POWER_CTRL_NEIGH_ISOLATION:
            shire_other_esrs[shire].power_ctrl_neigh_isolation = uint32_t(value);
            LOG_AGENT(DEBUG, agent, "S%u:power_ctrl_neigh_isolation = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].power_ctrl_neigh_isolation);
            return;
        case ESR_THREAD0_DISABLE:
            write_thread0_disable(shire, uint32_t(value));
            LOG_AGENT(DEBUG, agent, "S%u:thread0_disable = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].thread0_disable);
            return;
        case ESR_SHIRE_PLL_AUTO_CONFIG:
            shire_other_esrs[shire].shire_pll_auto_config = uint32_t(value & 0x1ffff);
            LOG_AGENT(DEBUG, agent, "S%u:shire_pll_auto_config = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].shire_pll_auto_config);
            return;
        case ESR_SHIRE_PLL_CONFIG_DATA_0:
        case ESR_SHIRE_PLL_CONFIG_DATA_1:
        case ESR_SHIRE_PLL_CONFIG_DATA_2:
        case ESR_SHIRE_PLL_CONFIG_DATA_3:
            shire_other_esrs[shire].shire_pll_config_data[(esr - ESR_SHIRE_PLL_CONFIG_DATA_0)>>3] = value;
            LOG_AGENT(DEBUG, agent, "S%u:shire_pll_config_data_%llu = 0x%" PRIx64,
                      shireid(shire), (esr - ESR_SHIRE_PLL_CONFIG_DATA_0)>>3,
                      shire_other_esrs[shire].shire_pll_config_data[(esr - ESR_SHIRE_PLL_CONFIG_DATA_0)>>3]);
            return;
        case ESR_SHIRE_COOP_MODE:
            LOG_AGENT(DEBUG, agent, "S%u:shire_coop_mode = 0x%x", shireid(shire), unsigned(value & 0x1));
            write_shire_coop_mode(shire, value);
            return;
        case ESR_SHIRE_CTRL_CLOCKMUX:
            shire_other_esrs[shire].shire_ctrl_clockmux = uint8_t(value & 0x4f);
            LOG_AGENT(DEBUG, agent, "S%u:shire_ctrl_clockmux = 0x%" PRIx8,
                      shireid(shire), shire_other_esrs[shire].shire_ctrl_clockmux);
            return;
        case ESR_SHIRE_CACHE_RAM_CFG1:
            shire_other_esrs[shire].shire_cache_ram_cfg1 = value & 0xfffffffffull;
            LOG_AGENT(DEBUG, agent, "S%u:shire_cache_ram_cfg1 = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].shire_cache_ram_cfg1);
            return;
        case ESR_SHIRE_CACHE_RAM_CFG2:
            shire_other_esrs[shire].shire_cache_ram_cfg2 = uint32_t(value & 0x3ffff);
            LOG_AGENT(DEBUG, agent, "S%u:shire_cache_ram_cfg2 = 0x%" PRIx32,
                      shireid(shire), shire_other_esrs[shire].shire_cache_ram_cfg2);
            return;
        case ESR_SHIRE_CACHE_RAM_CFG3:
            shire_other_esrs[shire].shire_cache_ram_cfg3 = value & 0xfffffffffull;
            LOG_AGENT(DEBUG, agent, "S%u:shire_cache_ram_cfg3 = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].shire_cache_ram_cfg3);
            return;
        case ESR_SHIRE_CACHE_RAM_CFG4:
            shire_other_esrs[shire].shire_cache_ram_cfg4 = value & 0xfffffffffull;
            LOG_AGENT(DEBUG, agent, "S%u:shire_cache_ram_cfg4 = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].shire_cache_ram_cfg4);
            return;
        case ESR_SHIRE_DLL_AUTO_CONFIG:
            shire_other_esrs[shire].shire_dll_auto_config = uint16_t(value & 0x3fff);
            LOG_AGENT(DEBUG, agent, "S%u:shire_dll_auto_config = 0x%" PRIx16,
                      shireid(shire), shire_other_esrs[shire].shire_dll_auto_config);
            return;
        case ESR_SHIRE_DLL_CONFIG_DATA_0:
            shire_other_esrs[shire].shire_dll_config_data_0 = value;
            LOG_AGENT(DEBUG, agent, "S%u:shire_dll_config_data_0 = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].shire_dll_config_data_0);
            return;
        case ESR_SHIRE_DLL_CONFIG_DATA_1:
            shire_other_esrs[shire].shire_dll_config_data_1 = value;
            LOG_AGENT(DEBUG, agent, "S%u:shire_dll_config_data_1 = 0x%" PRIx64,
                      shireid(shire), shire_other_esrs[shire].shire_dll_config_data_1);
            return;
        case ESR_UC_CONFIG:
            shire_other_esrs[shire].uc_config = value & 1;
            LOG_AGENT(DEBUG, agent, "S%u:uc_config = %d",
                      shireid(shire), int(shire_other_esrs[shire].uc_config));
            return;
        case ESR_ICACHE_UPREFETCH:
            LOG_AGENT(DEBUG, agent, "S%u:icache_uprefetch = 0x%" PRIx64, shireid(shire), value);
            write_icache_prefetch(Privilege::U, shire, value);
            return;
        case ESR_ICACHE_SPREFETCH:
            LOG_AGENT(DEBUG, agent, "S%u:icache_sprefetch = 0x%" PRIx64, shireid(shire), value);
            write_icache_prefetch(Privilege::S, shire, value);
            return;
        case ESR_ICACHE_MPREFETCH:
            LOG_AGENT(DEBUG, agent, "S%u:icache_mprefetch = 0x%" PRIx64, shireid(shire), value);
            write_icache_prefetch(Privilege::M, shire, value);
            return;
        case ESR_CLK_GATE_CTRL:
            shire_other_esrs[shire].clk_gate_ctrl = uint16_t(value & 0x7ff);
            LOG_AGENT(DEBUG, agent, "S%u:clk_gate_ctrl = 0x%" PRIx16,
                      shireid(shire), shire_other_esrs[shire].clk_gate_ctrl);
            return;
        case ESR_SHIRE_CHANNEL_ECO_CTL:
            shire_other_esrs[shire].shire_channel_eco_ctl = uint8_t(value);
            LOG_AGENT(DEBUG, agent, "S%u:shire_channel_eco_ctl = 0x%" PRIx8,
                      shireid(shire), shire_other_esrs[shire].shire_channel_eco_ctl);
            return;
        }
        WARN_AGENT(esrs, agent, "Write unknown shire_other ESR S%u:0x%" PRIx64, shireid(shire), esr);
        throw memory_error(addr);
    }

    WARN_AGENT(esrs, agent, "Write illegal ESR 0x%" PRIx64, addr);
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
        bool active = ((value >> 48) & 0xF) && shire_other_esrs[shire].shire_coop_mode;
        shire_other_esrs[shire].icache_prefetch_active = active;
    }
#endif
}


uint64_t System::read_icache_prefetch(Privilege /*privilege*/, unsigned shire) const
{
    (void) shire;
    assert(shire <= EMU_NUM_COMPUTE_SHIRES);
#ifdef SYS_EMU
    // NB: Prefetches finish instantaneously in sys_emu
    return 1;
#else
    return shire_other_esrs[shire].icache_prefetch_active;
#endif
}


void System::finish_icache_prefetch(unsigned shire)
{
    (void) shire;
    assert(shire <= EMU_NUM_COMPUTE_SHIRES);
#ifndef SYS_EMU
    shire_other_esrs[shire].icache_prefetch_active = 0;
#endif
}


} // namespace bemu

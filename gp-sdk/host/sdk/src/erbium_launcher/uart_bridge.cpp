//******************************************************************************
// Copyright (c) 2026 Ainekko, Co.
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------

#include "uart_bridge.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace erbium_launcher {

namespace {

// Minimal ELF64 layout — just enough fields to find the .uart_ring
// section's VA and the kernel's link-time base.
//
// We avoid pulling in elf.h / ELFIO so the bridge stays a single
// translation unit; the full ELF spec is overkill for one section
// lookup.

struct Elf64Ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};
static_assert(sizeof(Elf64Ehdr) == 64);

struct Elf64Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};
static_assert(sizeof(Elf64Phdr) == 56);

struct Elf64Shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
};
static_assert(sizeof(Elf64Shdr) == 64);

constexpr uint32_t PT_LOAD = 1;

// View a region of the ELF byte buffer as a contiguous T*. Returns
// nullptr if the buffer doesn't fully contain the requested range.
template <typename T>
const T* elfPeek(const std::vector<std::byte>& elf, uint64_t off, size_t count = 1) {
    if (off + sizeof(T) * count > elf.size())
        return nullptr;
    return reinterpret_cast<const T*>(elf.data() + off);
}

const char* elfPeekString(const std::vector<std::byte>& elf, uint64_t off) {
    if (off >= elf.size())
        return nullptr;
    // Bounded strnlen-style scan to keep us from wandering off the buffer.
    auto* p = reinterpret_cast<const char*>(elf.data() + off);
    auto remaining = elf.size() - off;
    if (memchr(p, 0, remaining) == nullptr)
        return nullptr;
    return p;
}

// "Link base" = the VA that the relocator considers the buffer's
// origin, i.e. the VA that maps to deviceBuf + 0. For each LOAD
// segment, the VA of buffer offset 0 is `p_vaddr - p_offset`. We
// take the minimum across all LOAD segments.
//
// Worked example for the soc1sim uart_echo ELF:
//   region0_base       = 0x8005801000  (linker script default)
//   MMODE_RESERVED_SIZE= 0x1000
//   MRAM ORIGIN        = 0x8005802000
//   LOAD: p_vaddr=0x8005802000, p_offset=0x1000
//   link_base          = 0x8005802000 - 0x1000 = 0x8005801000  ✓
std::optional<uint64_t> findLinkBase(const std::vector<std::byte>& elf,
                                     const Elf64Ehdr& eh) {
    std::optional<uint64_t> low;
    for (uint16_t i = 0; i < eh.e_phnum; i++) {
        auto* ph = elfPeek<Elf64Phdr>(elf, eh.e_phoff + i * eh.e_phentsize);
        if (!ph) return std::nullopt;
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_vaddr < ph->p_offset) continue;  // unusual; ignore
        uint64_t base = ph->p_vaddr - ph->p_offset;
        if (!low || base < *low)
            low = base;
    }
    return low;
}

}  // namespace

std::optional<uint64_t>
findUartRingSectionInElf(const std::vector<std::byte>& elf,
                         uint64_t kernel_load_addr) {
    if (elf.size() < sizeof(Elf64Ehdr)) return std::nullopt;
    auto* eh = elfPeek<Elf64Ehdr>(elf, 0);
    if (!eh) return std::nullopt;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F')
        return std::nullopt;
    if (eh->e_ident[4] != 2 /* ELFCLASS64 */) return std::nullopt;
    if (eh->e_shentsize != sizeof(Elf64Shdr)) return std::nullopt;

    // .shstrtab — the section name string table. Indexed by sh_name.
    if (eh->e_shstrndx >= eh->e_shnum) return std::nullopt;
    auto* shstrhdr = elfPeek<Elf64Shdr>(
        elf, eh->e_shoff + eh->e_shstrndx * eh->e_shentsize);
    if (!shstrhdr) return std::nullopt;
    auto shstrtab_off  = shstrhdr->sh_offset;
    auto shstrtab_size = shstrhdr->sh_size;
    if (shstrtab_off + shstrtab_size > elf.size()) return std::nullopt;

    auto link_base = findLinkBase(elf, *eh);
    if (!link_base) return std::nullopt;

    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        auto* sh = elfPeek<Elf64Shdr>(
            elf, eh->e_shoff + i * eh->e_shentsize);
        if (!sh) return std::nullopt;
        if (sh->sh_name >= shstrtab_size) continue;
        auto* name = elfPeekString(elf, shstrtab_off + sh->sh_name);
        if (!name) continue;
        if (std::string_view(name) != ".uart_ring") continue;
        if (sh->sh_size == 0) return std::nullopt;
        if (sh->sh_addr < *link_base) return std::nullopt;

        uint64_t offset_in_buffer = sh->sh_addr - *link_base;
        // kernel_load_addr is currently informational — the launcher
        // places the kernel at its link-time base, so the offset we
        // computed against link_base IS the device-buffer offset.
        (void)kernel_load_addr;
        return offset_in_buffer;
    }
    return std::nullopt;
}

std::optional<uint64_t>
findUartHeaderByMagic(rt::IRuntime& rt,
                      rt::StreamId stream,
                      std::byte* device_buffer,
                      uint64_t device_buffer_size) {
    // Pull the whole buffer up to the host once and scan. Slow but
    // simple; only used as a fallback when ELF section parsing
    // failed. The header is 64 B aligned so we step by 64.
    std::vector<std::byte> snapshot(device_buffer_size);
    auto evt = rt.memcpyDeviceToHost(stream, device_buffer,
                                      snapshot.data(), snapshot.size());
    if (!rt.waitForEvent(evt))
        return std::nullopt;

    for (uint64_t off = 0; off + sizeof(uint32_t) <= snapshot.size();
         off += 64) {
        uint32_t word;
        std::memcpy(&word, snapshot.data() + off, sizeof(word));
        if (word == kUartMagic)
            return off;
    }
    return std::nullopt;
}

std::optional<UartLocation>
readAndValidateHeader(rt::IRuntime& rt,
                      rt::StreamId stream,
                      std::byte* device_buffer,
                      uint64_t device_buffer_size,
                      uint64_t hdr_offset) {
    if (hdr_offset + sizeof(UartHdr) > device_buffer_size) {
        std::cerr << "uart_bridge: hdr_offset 0x" << std::hex << hdr_offset
                  << std::dec << " out of buffer\n";
        return std::nullopt;
    }
    UartHdr hdr;
    auto evt = rt.memcpyDeviceToHost(stream, device_buffer + hdr_offset,
                                      reinterpret_cast<std::byte*>(&hdr),
                                      sizeof(hdr));
    if (!rt.waitForEvent(evt)) {
        std::cerr << "uart_bridge: timed out reading header\n";
        return std::nullopt;
    }
    if (hdr.magic != kUartMagic) {
        // Magic absent -- the kernel hasn't run uart_init() yet.
        // The launcher polls this routine, so this is an expected
        // transient state, not an error.  Stay quiet to avoid log
        // spam during the poll.
        return std::nullopt;
    }
    if (hdr.version != kUartVersion) {
        std::cerr << "uart_bridge: version mismatch (device=" << hdr.version
                  << ", host=" << kUartVersion << ")\n";
        return std::nullopt;
    }

    UartLocation loc;
    loc.hdr_offset     = hdr_offset;
    loc.tx_ring_offset = hdr_offset + hdr.tx_offset;
    loc.rx_ring_offset = hdr_offset + hdr.rx_offset;
    loc.tx_capacity    = hdr.tx_capacity;
    loc.rx_capacity    = hdr.rx_capacity;

    // Sanity-check ring placement.
    if (loc.tx_ring_offset + loc.tx_capacity > device_buffer_size ||
        loc.rx_ring_offset + loc.rx_capacity > device_buffer_size) {
        std::cerr << "uart_bridge: ring buffers extend past device buffer\n";
        return std::nullopt;
    }
    return loc;
}

UartBridge::UartBridge(rt::IRuntime& rt,
                       rt::StreamId stream,
                       std::byte* device_buffer,
                       UartLocation loc,
                       int input_fd,
                       int output_fd)
    : rt_(rt), stream_(stream), device_buffer_(device_buffer),
      loc_(loc), input_fd_(input_fd), output_fd_(output_fd) {
    // The kernel may have started before we get here, so don't
    // assume tx_tail_local_ / rx_head_local_ are at zero — pull the
    // current device cursors so we don't double-process or skip.
    UartHdr hdr;
    if (readHeader(hdr)) {
        tx_tail_local_ = hdr.tx_head;  // any TX produced before now is "already drained"
        rx_head_local_ = hdr.rx_head;  // anything the device already saw is "already filled"
    }
}

UartBridge::~UartBridge() = default;

bool UartBridge::readHeader(UartHdr& hdr) {
    auto evt = rt_.memcpyDeviceToHost(stream_,
                                       device_buffer_ + loc_.hdr_offset,
                                       reinterpret_cast<std::byte*>(&hdr),
                                       sizeof(hdr));
    return rt_.waitForEvent(evt);
}

void UartBridge::writeHeaderField(uint64_t field_offset_in_hdr, uint32_t value) {
    auto evt = rt_.memcpyHostToDevice(
        stream_,
        reinterpret_cast<const std::byte*>(&value),
        device_buffer_ + loc_.hdr_offset + field_offset_in_hdr,
        sizeof(value));
    rt_.waitForEvent(evt);
}

void UartBridge::drainTx(const UartHdr& hdr) {
    uint32_t avail = hdr.tx_head - tx_tail_local_;
    if (avail == 0) return;
    if (avail > loc_.tx_capacity) {
        // Should be impossible if device respects ring capacity.
        std::cerr << "uart_bridge: TX overflow (avail=" << avail
                  << " > cap=" << loc_.tx_capacity << "), clamping\n";
        avail = loc_.tx_capacity;
    }

    std::vector<std::byte> buf(avail);
    uint32_t mask  = loc_.tx_capacity - 1u;
    uint32_t start = tx_tail_local_ & mask;
    uint32_t first = std::min<uint32_t>(avail, loc_.tx_capacity - start);

    auto e1 = rt_.memcpyDeviceToHost(
        stream_, device_buffer_ + loc_.tx_ring_offset + start,
        buf.data(), first);
    rt_.waitForEvent(e1);
    if (avail > first) {
        auto e2 = rt_.memcpyDeviceToHost(
            stream_, device_buffer_ + loc_.tx_ring_offset,
            buf.data() + first, avail - first);
        rt_.waitForEvent(e2);
    }

    ssize_t off = 0;
    while (off < static_cast<ssize_t>(buf.size())) {
        ssize_t n = ::write(output_fd_,
                            reinterpret_cast<const char*>(buf.data()) + off,
                            buf.size() - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "uart_bridge: write to output_fd failed: "
                      << std::strerror(errno) << "\n";
            break;
        }
        off += n;
    }

    tx_tail_local_ += avail;
    writeHeaderField(offsetof(UartHdr, tx_tail), tx_tail_local_);
}

void UartBridge::fillRx(const UartHdr& hdr) {
    if (input_eof_) return;

    uint32_t in_flight = rx_head_local_ - hdr.rx_tail;
    if (in_flight > loc_.rx_capacity) in_flight = loc_.rx_capacity;
    uint32_t free_slots = loc_.rx_capacity - in_flight;
    if (free_slots == 0) return;

    // Best-effort non-blocking read. The fd was set to O_NONBLOCK by
    // the launcher; if there's nothing to read right now we just
    // come back next tick.
    std::vector<std::byte> buf(free_slots);
    ssize_t n = ::read(input_fd_, buf.data(), buf.size());
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        std::cerr << "uart_bridge: read from input_fd failed: "
                  << std::strerror(errno) << "\n";
        input_eof_ = true;
        return;
    }
    if (n == 0) {
        input_eof_ = true;
        return;
    }

    uint32_t mask  = loc_.rx_capacity - 1u;
    uint32_t start = rx_head_local_ & mask;
    uint32_t first = std::min<uint32_t>(static_cast<uint32_t>(n),
                                         loc_.rx_capacity - start);

    auto e1 = rt_.memcpyHostToDevice(
        stream_, buf.data(),
        device_buffer_ + loc_.rx_ring_offset + start, first);
    rt_.waitForEvent(e1);
    if (static_cast<uint32_t>(n) > first) {
        auto e2 = rt_.memcpyHostToDevice(
            stream_, buf.data() + first,
            device_buffer_ + loc_.rx_ring_offset,
            static_cast<uint32_t>(n) - first);
        rt_.waitForEvent(e2);
    }

    rx_head_local_ += static_cast<uint32_t>(n);
    writeHeaderField(offsetof(UartHdr, rx_head), rx_head_local_);
}

bool UartBridge::tick() {
    UartHdr hdr;
    if (!readHeader(hdr)) return false;
    drainTx(hdr);
    fillRx(hdr);
    return hdr.exit_flag != 0;
}

void UartBridge::drainFinal() {
    UartHdr hdr;
    if (!readHeader(hdr)) return;
    drainTx(hdr);
}

}  // namespace erbium_launcher

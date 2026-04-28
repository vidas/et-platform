//******************************************************************************
// Copyright (c) 2026 Ainekko, Co.
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
//
// UartBridge — host side of the erbium-soc1sim fake UART.
//
// The on-device driver (et-common-libs/include/erbium-soc1sim/drivers/uart.h)
// exposes a header + two byte ring buffers in device DRAM, identified
// by the magic word 0xFA4EFA4E. This class:
//
//   1. Locates the ring header in the loaded ELF by reading the
//      .uart_ring section's VA, fall back to a magic scan if absent.
//   2. Polls the device header via memcpyDeviceToHost during kernel
//      execution, draining new TX bytes to an output fd and pumping
//      input fd bytes into the RX ring via memcpyHostToDevice.
//   3. Stops on the kernel's launch EventId completing OR the device
//      setting hdr.exit_flag.
//
// Single-threaded by design: the caller owns the polling cadence
// (typically interleaved with non-blocking waitForEvent on the
// kernel event).
//
//------------------------------------------------------------------------------

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <runtime/IRuntime.h>
#include <runtime/Types.h>

namespace erbium_launcher {

// On-device ring header. Mirrors `struct erbium_uart_hdr` in
// <erbium-soc1sim/drivers/uart.h>. Layout must stay byte-for-byte
// identical; the struct lives on the device side and we read it via
// raw DMA.
struct UartHdr {
    uint32_t magic;
    uint32_t version;
    uint32_t tx_capacity;
    uint32_t rx_capacity;
    uint32_t tx_offset;
    uint32_t rx_offset;
    uint32_t tx_head_resv;
    uint32_t tx_head;
    uint32_t tx_tail;
    uint32_t rx_head;
    uint32_t rx_tail_resv;
    uint32_t rx_tail;
    uint32_t exit_flag;
    uint32_t _pad;
};
static_assert(sizeof(UartHdr) == 56, "UartHdr layout mismatch");

constexpr uint32_t kUartMagic   = 0xFA4EFA4Eu;
constexpr uint32_t kUartVersion = 1u;

// Discovery result: byte offsets of the header and ring bodies
// within the device buffer the launcher allocated for the kernel.
struct UartLocation {
    uint64_t hdr_offset;
    uint64_t tx_ring_offset;
    uint64_t rx_ring_offset;
    uint32_t tx_capacity;
    uint32_t rx_capacity;
};

// Read the section table out of an ELF64 image and return the
// device-buffer offset of `.uart_ring`. Returns nullopt if the
// section is absent or zero-sized; callers can then fall back to a
// magic scan.
std::optional<uint64_t>
findUartRingSectionInElf(const std::vector<std::byte>& elf,
                         uint64_t kernel_load_addr);

// DMA-scan the device buffer for kUartMagic. Slow on first call but
// correct for any kernel that runs uart_init() — the magic is
// published only after init completes. Returns nullopt if not found
// within `device_buffer_size`.
std::optional<uint64_t>
findUartHeaderByMagic(rt::IRuntime& rt,
                      rt::StreamId stream,
                      std::byte* device_buffer,
                      uint64_t device_buffer_size);

// Read the header at `hdr_offset` and validate it. On success
// returns a fully-populated UartLocation; on validation failure
// (wrong magic / wrong version / impossible ring offsets) returns
// nullopt with a diagnostic on stderr.
std::optional<UartLocation>
readAndValidateHeader(rt::IRuntime& rt,
                      rt::StreamId stream,
                      std::byte* device_buffer,
                      uint64_t device_buffer_size,
                      uint64_t hdr_offset);

// The bridge itself. Construct after the kernel has been launched
// and the header has been located + validated. Then call tick()
// repeatedly until the kernel event fires; tick() is responsible
// for one full pump cycle (drain TX, fill RX). When the kernel is
// done call drainFinal() once to pull any in-flight TX bytes that
// the device pushed between the last tick and the kernel's exit.
class UartBridge {
public:
    UartBridge(rt::IRuntime& rt,
               rt::StreamId stream,
               std::byte* device_buffer,
               UartLocation loc,
               int input_fd,
               int output_fd);
    ~UartBridge();

    UartBridge(const UartBridge&) = delete;
    UartBridge& operator=(const UartBridge&) = delete;

    // One pump cycle. Returns true if the device has set exit_flag.
    bool tick();

    // Pull any remaining TX bytes after the kernel event has fired.
    // Idempotent.
    void drainFinal();

private:
    bool readHeader(UartHdr& hdr);
    void writeHeaderField(uint64_t field_offset_in_hdr,
                          uint32_t value);
    void drainTx(const UartHdr& hdr);
    void fillRx(const UartHdr& hdr);

    rt::IRuntime& rt_;
    rt::StreamId stream_;
    std::byte* device_buffer_;
    UartLocation loc_;
    int input_fd_;
    int output_fd_;

    // Cursors the host owns. The device reads them to gauge ring
    // fullness; we never read them back from the device.
    uint32_t tx_tail_local_ = 0;
    uint32_t rx_head_local_ = 0;

    bool input_eof_ = false;
};

}  // namespace erbium_launcher

/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/* uart_echo M-mode boot. Native erbium gates UART and system-register
 * MMIO behind M/S privilege (sw-sysemu/pma_er.cpp:308-321), so the
 * kernel cannot use the UART driver from U-mode without a syscall
 * bridge. Until that bridge exists, this kernel just stays in M-mode
 * and calls main() directly — no mret, no _start, no crt.
 *
 * Composed from <erbium/boot.h> macros for the steps the runtime's
 * default _boot also uses; the rest (UART pinmux open, call main,
 * test-PASS handshake) is inline since this is the only kernel that
 * needs that exact tail today. Once we have an M-mode UART-init
 * helper we can drop the inline pinmux block.
 *
 * Overrides the runtime's weak _boot. erbium-soc1sim has no M-mode
 * at all, so its kernels go through the standard runtime path
 * (which lands directly in U-mode); this override is build-conditional
 * via the kernel CMakeLists (boot.S is only fed to the .erbium.elf
 * target).
 */

#include "erbium/boot.h"

  .section .mboot, "ax", @progbits
  .global _boot
_boot:
  et_boot_init_mmode_csrs
  et_boot_set_per_hart_stack

  /* Open the UART pinmux gate — SystemConfig is M-mode-only, so we
   * do this here once and main() never has to. */
  li    t0, 0x0002000000          /* ERBIUM_TOP_SYSTEM_REGISTERS_BASE */
  li    t1, 0x08                  /* SYSTEM_SYSTEMCONFIG_ADDRESS (offset) */
  add   t0, t0, t1
  lw    t1, 0(t0)
  li    t2, 0x40                  /* UART_ENABLE bit (1 << 6) */
  or    t1, t1, t2
  sw    t1, 0(t0)

  /* Call main() in M-mode. Single thread/hart on this demo path. */
  call  main

  /* Signal test completion to the emulator: write 0x1FEED000 to
   * validation0 (CSR 0x8D0) — once every hart has done so,
   * erbium_emu stops cleanly. Mirrors the runtime's crt.S exit. On
   * real hardware this is a harmless diagnostic CSR write. */
  li    t0, 0x1FEED000
  csrw  validation0, t0

  /* Spin in case we're on real hardware (no emulator to halt us). */
1:
  j     1b

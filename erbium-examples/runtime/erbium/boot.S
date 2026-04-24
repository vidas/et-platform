/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/* Erbium M-mode first-stage.
 *
 * Lands in the .mboot section, placed by the linker at
 *   MRAM_BASE + MBOOT_OFFSET
 * (defaults: 0x40000040). The first MBOOT_OFFSET bytes of MRAM are
 * reserved for a runtime-supplied config payload and stay empty in
 * the ELF.
 *
 * Responsibilities:
 *   1. Initialize M-mode CSRs (satp=0, enable FPU, clear mip/fcsr/
 *      tensor_mask).
 *   2. Set the per-hart stack pointer (carries through mret into
 *      U-mode).
 *   3. Point mepc at _start and clear mstatus.MPP so mret returns
 *      into U-mode.
 *   4. Zero the integer, FP, and matrix register files (skipping
 *      x0 and x2=sp).
 *   5. mret -> user's _start in .text.
 *
 * A user who wants their own boot sequence just defines their own
 * _boot (in any source file); the linker resolves that local symbol
 * before scanning the archive and leaves this one out of the ELF.
 */

  .section .mboot, "ax", @progbits
  .global _boot
_boot:
  /* Signal start of boot via validation0 CSR (emulator breadcrumb). */
  li   a7, 0xDEAD0001
  csrw validation0, a7

  /* M-mode CSR init */
  csrwi satp, 0
  csrr  t0, mstatus
  li    t1, 0x6000
  or    t0, t0, t1
  csrw  mstatus, t0              /* fs = dirty (11) */
  csrrw x0, fcsr, x0
  csrrw x0, mip, x0
  csrwi tensor_mask, 0

  /* Per-hart stack pointer; carries through mret into U-mode.
   * STACK_SIZE is a linker symbol (default in erbium.ld, overridable
   * per-kernel via -Wl,--defsym=STACK_SIZE=<bytes>). */
  la    sp, __stack_base
  csrr  a1, mhartid
  lui   t1, %hi(STACK_SIZE)
  addi  t1, t1, %lo(STACK_SIZE)
  mul   a1, a1, t1
  sub   sp, sp, a1

  /* Point mret at U-mode _start (in .text). */
  la    t0, _start
  csrw  mepc, t0

  /* Allow U-mode access to the hartid shadow CSR (0xCD0).
   * Without this, csrr hartid traps in U-mode on erbium_emu. */
  csrwi menable_shadows, 1

  /* mstatus.MPP (bits [12:11]) = 00 -> mret returns to U-mode. */
  li    t1, 0x1800
  csrc  mstatus, t1

  /* Signal end of boot via validation0 CSR (emulator breadcrumb). */
  li   a7, 0xDEAD0002
  csrw validation0, a7

  /* Zero integer registers (skip x0=zero and x2=sp). */
  addi x1,  x0, 0
  addi x3,  x0, 0
  addi x4,  x0, 0
  addi x5,  x0, 0
  addi x6,  x0, 0
  addi x7,  x0, 0
  addi x8,  x0, 0
  addi x9,  x0, 0
  addi x10, x0, 0
  addi x11, x0, 0
  addi x12, x0, 0
  addi x13, x0, 0
  addi x14, x0, 0
  addi x15, x0, 0
  addi x16, x0, 0
  addi x17, x0, 0
  addi x18, x0, 0
  addi x19, x0, 0
  addi x20, x0, 0
  addi x21, x0, 0
  addi x22, x0, 0
  addi x23, x0, 0
  addi x24, x0, 0
  addi x25, x0, 0
  addi x26, x0, 0
  addi x27, x0, 0
  addi x28, x0, 0
  addi x29, x0, 0
  addi x30, x0, 0
  addi x31, x0, 0

  /* Zero floating-point registers */
  fcvt.s.w f0,  x0
  fcvt.s.w f1,  x0
  fcvt.s.w f2,  x0
  fcvt.s.w f3,  x0
  fcvt.s.w f4,  x0
  fcvt.s.w f5,  x0
  fcvt.s.w f6,  x0
  fcvt.s.w f7,  x0
  fcvt.s.w f8,  x0
  fcvt.s.w f9,  x0
  fcvt.s.w f10, x0
  fcvt.s.w f11, x0
  fcvt.s.w f12, x0
  fcvt.s.w f13, x0
  fcvt.s.w f14, x0
  fcvt.s.w f15, x0
  fcvt.s.w f16, x0
  fcvt.s.w f17, x0
  fcvt.s.w f18, x0
  fcvt.s.w f19, x0
  fcvt.s.w f20, x0
  fcvt.s.w f21, x0
  fcvt.s.w f22, x0
  fcvt.s.w f23, x0
  fcvt.s.w f24, x0
  fcvt.s.w f25, x0
  fcvt.s.w f26, x0
  fcvt.s.w f27, x0
  fcvt.s.w f28, x0
  fcvt.s.w f29, x0
  fcvt.s.w f30, x0
  fcvt.s.w f31, x0

  /* Zero matrix registers */
  li zero, 0xf
  mova.m.x zero

  /* Drop into U-mode at _start. */
  mret

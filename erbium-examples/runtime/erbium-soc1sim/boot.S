/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/* Erbium-on-etsoc1 first-stage entry.
 *
 * The etsoc1 firmware hands us control in U-mode with:
 *   - sp      - points into a firmware-owned stack (pre-built frame);
 *   - a0..a3  - firmware-supplied kernel-launch args (dismissed in crt.S);
 *   - gp      - zeroed (crt.S re-establishes it);
 *   - GPR/FP/matrix regs cleared, FPU enabled, fcsr cleared.
 *
 * We replace the firmware-supplied sp with our own linker-managed
 * per-hart stack so the host side doesn't have to know, communicate,
 * or carve anything about kernel stack layout:
 *
 *   sp = __stack_base - (hartid << STACK_SIZE_LOG2)
 *
 */

  .section .text.init, "ax", @progbits
  .global _boot
_boot:
  /* sp = __stack_base - (local_hartid * STACK_SIZE).
   * STACK_SIZE is a linker symbol (default in erbium.ld, overridable
   * per-kernel via -Wl,--defsym=STACK_SIZE=<bytes>). */
  la    sp, __stack_base
  csrr  t0, hartid
  andi  t0, t0, 0x3F         /* local hart index within shire (64 harts/shire) */
  lui   t1, %hi(STACK_SIZE)
  addi  t1, t1, %lo(STACK_SIZE)
  mul   t0, t0, t1
  sub   sp, sp, t0

  j _start

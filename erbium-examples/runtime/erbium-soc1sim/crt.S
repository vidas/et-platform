/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/* Erbium-on-etsoc1 U-mode entry.
 *
 * On entry:
 *   sp      - set up by boot.S (per-hart descending stack)
 *   a0..a3  - firmware-supplied kernel args (dismissed here)
 *   gp      - wiped; re-establish below
 *   All other integer / FP / matrix registers cleared.
 *
 * Only the first NUM_HARTS harts run main(); every other hart in
 * the shire skips straight to the firmware exit path so the
 * post-launch FCC barrier in device-minion-runtime's
 * kernel_launch_post_cleanup() still completes — the firmware
 * always launches every thread in the shire (64 on non-master,
 * 32 on master) and waits for them all to return, regardless of
 * what our kernel wants. Just spinning would deadlock the shire.
 *
 */

#include "erbium/isa/syscall.h"

  .text
  .global _start
_start:
  # Re-establish gp. Don't use relaxed relocation or the linker turns
  # this into "mv gp, gp" once it sees gp is the destination.
  .option push
  .option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
  .option pop

  # Dismiss firmware-supplied arguments so main() sees a clean state.
  mv    a0, zero
  mv    a1, zero
  mv    a2, zero
  mv    a3, zero

  # Only harts [0, NUM_HARTS) run main(); everyone else falls through
  # to the ecall exit so the firmware's post-launch FCC barrier
  # completes.
  csrr  t0, hartid
  andi  t0, t0, 0x3F         /* local hart index within shire (64 harts/shire) */
  lui   t1, %hi(NUM_HARTS)
  addi  t1, t1, %lo(NUM_HARTS)
  bgeu  t0, t1, .Lkernel_exit

  call  main

.Lkernel_exit:
  # Clean return to firmware:
  #   a0 = SYSCALL_RETURN_FROM_KERNEL
  #   a1 = main()'s return value (skipping harts carry 0 from the
  #        mv a0, zero above)
  #   a2 = KERNEL_RETURN_SUCCESS
  mv    a1, a0
  li    a2, KERNEL_RETURN_SUCCESS
  li    a0, SYSCALL_RETURN_FROM_KERNEL
  ecall

1:j 1b

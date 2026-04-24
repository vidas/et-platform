/*-------------------------------------------------------------------------
* Copyright (c) 2026 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

/* Erbium U-mode entry.
 *
 * _start is the first instruction reached in U-mode after the
 * M-mode first-stage (boot.S) issues mret. sp was already set by
 * boot.S and carries through the mode change; gp was wiped with
 * the rest of the GPR file, so re-establish it here. */

  .text
  .global _start
_start:
  # Don't use relaxed relocation for the gp init or the linker will
  # turn it into "mv gp, gp" once it sees gp is the destination.
  .option push
  .option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
  .option pop
  call  main
  # Signal test completion to the emulator. Writing 0x1FEED000 to
  # validation0 (CSR 0x8D0) marks this hart as "done — PASS";
  # once every hart has done so, erbium_emu stops cleanly.
  # On real hardware this is a harmless write to a diagnostic CSR.
  li    t0, 0x1FEED000
  csrw  validation0, t0
  # Spin in case we're on real hardware (no emulator to stop us).
1:j 1b

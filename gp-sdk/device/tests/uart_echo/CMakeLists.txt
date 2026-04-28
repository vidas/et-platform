# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0

# Same source, two backends.  ELFs are install-suffixed by backend so
# they don't collide in /opt/et/kernels/.  test_uart_echo.py picks the
# right one per --device.
#
# Native erbium ships an extra boot.S that overrides the runtime's
# weak _boot to keep the kernel in M-mode (UART/SysReg MMIO is
# M/S-only).  erbium-soc1sim is U-mode by construction and uses the
# standard runtime boot path.
add_erbium_test_kernel(uart_echo.erbium-soc1sim.elf
  SOURCES uart_echo.c BACKEND erbium-soc1sim)
add_erbium_test_kernel(uart_echo.erbium.elf
  SOURCES uart_echo.c boot.S BACKEND erbium)

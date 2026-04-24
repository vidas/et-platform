# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0

add_erbium_test_kernel(hart_id_soc1sim.elf
  SOURCES hart_id.c
  BACKEND erbium-soc1sim)

add_erbium_test_kernel(hart_id_erbium.elf
  SOURCES hart_id.c
  BACKEND erbium)

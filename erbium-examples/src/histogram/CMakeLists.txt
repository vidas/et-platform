# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0

set(HIST_ITERATIONS 100 CACHE STRING "Number of histogram iterations (for benchmarking)")

erbium_kernel(
  NAME histogram
  SOURCES histogram.c
)

target_compile_definitions(histogram.elf PRIVATE HIST_ITERATIONS=${HIST_ITERATIONS})

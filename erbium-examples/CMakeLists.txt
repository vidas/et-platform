# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.15)

# Single knob for third-party consumers: point at an installed
# et-platform tree and everything else (toolchain, cmake-modules,
# find_package search prefix) is derived from it.
#   cmake -DET_PLATFORM=/path/to/et-install ..
# or set ET_PLATFORM in the environment; defaults to /opt/et.
if (NOT DEFINED ET_PLATFORM)
    if (DEFINED ENV{ET_PLATFORM})
        set(ET_PLATFORM "$ENV{ET_PLATFORM}")
    else()
        set(ET_PLATFORM "/opt/et")
    endif()
endif()
set(ET_PLATFORM "${ET_PLATFORM}" CACHE PATH "Path to installed et-platform tree")

if (NOT EXISTS "${ET_PLATFORM}")
    message(FATAL_ERROR
        "ET_PLATFORM='${ET_PLATFORM}' does not exist. "
        "Install et-platform or pass -DET_PLATFORM=<prefix>.")
endif()

# Use the riscv64 toolchain shipped with et-platform unless the user
# explicitly supplied their own via -DCMAKE_TOOLCHAIN_FILE=...
# Must be set before project() so CMake picks it up on first configure.
if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${ET_PLATFORM}/lib/cmake/riscv64-ec-toolchain.cmake"
        CACHE FILEPATH "RISC-V toolchain file from et-platform")
endif()

# Make find_package(et-common-libs ...) discover the installed tree.
list(APPEND CMAKE_PREFIX_PATH "${ET_PLATFORM}")

project(ErbiumExamples VERSION 0.1.0 LANGUAGES C ASM)

# Project-local modules first, then et-platform's shared modules
# (for DeviceUtils etc).
list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${ET_PLATFORM}/lib/cmake/cmake-modules
)

# Erbium target flags (match sw-sysemu/tests/erbium Makefile):
#   rv64imfc / lp64f ABI, medany code model, freestanding.
set(ERBIUM_ARCH_FLAGS "-march=rv64imfc -mabi=lp64f -mcmodel=medany")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ERBIUM_ARCH_FLAGS} -nostdlib -fno-zero-initialized-in-bss -ffunction-sections -fdata-sections ")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${ERBIUM_ARCH_FLAGS} ")

# MRAM is a single RWX region in the linker script; suppress the W^X linker warning.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-warn-rwx-segments")

if (NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()

include(ExternalProject)
include(GNUInstallDirs)
include(DeviceUtils)
include(ErbiumExamplesUtils)

cmake_policy(SET CMP0076 NEW)

# Backend selection: native erbium vs erbium-on-etsoc1 (soc1sim).
# Both backends publish `#include <erbium/isa/...>`; we just link the
# chosen target and the right header file resolves via include-dir order.
option(ERBIUM_EXAMPLES_USE_SOC1SIM
       "Target erbium-soc1sim (erbium API on etsoc1 hardware) instead of native erbium"
       OFF)

# Dependencies + per-backend boot/crt/layout glue. The et-common-libs
# targets are header-only INTERFACE libraries; we own the runtime
# sources here so kernels can override _boot/_start by simply defining
# their own.
if (ERBIUM_EXAMPLES_USE_SOC1SIM)
    find_package(et-common-libs REQUIRED COMPONENTS erbium-soc1sim)
    set(ERBIUM_UMODE_TARGET et-common-libs::erbium-soc1sim)
    set(ERBIUM_RUNTIME_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/erbium-soc1sim/boot.S
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/erbium-soc1sim/crt.S
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/erbium-soc1sim/layout.c
    )
else()
    find_package(et-common-libs REQUIRED COMPONENTS erbium-umode)
    set(ERBIUM_UMODE_TARGET et-common-libs::erbium-umode)
    set(ERBIUM_RUNTIME_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/erbium/boot.S
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/erbium/crt.S
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/erbium/layout.c
    )
endif()

# The selected backend publishes its default linker script as a
# target property; pick it up uniformly rather than hardcoding paths.
get_target_property(ERBIUM_DEFAULT_LINKER_SCRIPT
    ${ERBIUM_UMODE_TARGET} ERBIUM_LINKER_SCRIPT)

add_subdirectory(src)

# Copyright (c) 2026 Ainekko, Co.
# SPDX-License-Identifier: Apache-2.0

# Helper macro for building an Erbium example kernel ELF.
# Arguments:
#   NAME       : Name of the kernel (drives target and artifact names)
#   SOURCES    : Input compile sources
#   INCLUDES   : Optional per-kernel include directories
#   STACK_SIZE : Optional per-hart stack size in bytes (overrides linker default)
macro(erbium_kernel)
    set(options)
    set(oneValueArgs NAME STACK_SIZE)
    set(multiValueArgs SOURCES INCLUDES)
    cmake_parse_arguments(ERBIUM_KERNEL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ERBIUM_KERNEL_NAME)
        message(SEND_ERROR "Error: erbium_kernel() called without NAME argument!")
    endif()

    if (NOT ERBIUM_KERNEL_SOURCES)
        message(SEND_ERROR "Error: erbium_kernel() called without SOURCES argument!")
    endif()

    set(TARGET_NAME ${ERBIUM_KERNEL_NAME})
    # Default to the linker script shipped by the selected et-common-libs
    # backend; any caller that wants its own layout just sets LINKER_SCRIPT
    # before calling erbium_kernel().
    if (NOT LINKER_SCRIPT)
        set(LINKER_SCRIPT ${ERBIUM_DEFAULT_LINKER_SCRIPT})
    endif()

    add_riscv_executable(${TARGET_NAME})
    # ERBIUM_RUNTIME_SOURCES (set in the top-level CMakeLists by the
    # backend-selection block) carries the per-backend boot.S/crt.S/
    # layout.c. A kernel that wants its own _boot or _start just
    # defines them in ERBIUM_KERNEL_SOURCES; ours stay GC'd by
    # --gc-sections.
    target_sources(${TARGET_NAME}.elf PRIVATE
        ${ERBIUM_KERNEL_SOURCES}
        ${ERBIUM_RUNTIME_SOURCES})
    target_include_directories(${TARGET_NAME}.elf PRIVATE ${ERBIUM_KERNEL_INCLUDES})
    target_link_libraries(${TARGET_NAME}.elf
        PRIVATE
            ${ERBIUM_UMODE_TARGET}
    )
    set_target_properties(${TARGET_NAME}.elf
        PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE  # fPIC
    )

    if (ERBIUM_KERNEL_STACK_SIZE)
        target_link_options(${TARGET_NAME}.elf PRIVATE
            "LINKER:--defsym=STACK_SIZE=${ERBIUM_KERNEL_STACK_SIZE}")
    endif()

endmacro()

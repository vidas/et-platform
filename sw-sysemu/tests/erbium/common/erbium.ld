/*-------------------------------------------------------------------------
* Copyright (c) 2025 Ainekko, Co.
* SPDX-License-Identifier: Apache-2.0
*-------------------------------------------------------------------------*/

OUTPUT_ARCH("riscv")
ENTRY(_boot)

/* Erbium memory layout */
BOOTROM_BASE = 0x000200A000;
BOOTROM_SIZE = 8K;
SRAM_BASE    = 0x000200E000;
SRAM_SIZE    = 2K;
MRAM_BASE    = 0x0040000000;
MRAM_SIZE    = 16M;

/* Stack configuration */
MAX_THREADS     = 16;
STACK_SIZE      = 64;
STACK_SIZE_LOG2 = 6;

MEMORY {
    BOOTROM  (rx)  : org = BOOTROM_BASE, len = BOOTROM_SIZE
    SRAM     (rwx) : org = SRAM_BASE,    len = SRAM_SIZE
    MRAM     (rwx) : org = MRAM_BASE,    len = MRAM_SIZE
}

SECTIONS {
    .bootrom : {
        *(bootrom bootrom.*)
        /* Test code goes here by default (accessible from all privilege modes) */
        . = ALIGN(8);
        PROVIDE(__code_start = .);
        *(.text.init)
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        PROVIDE(__code_end = .);
        __bootrom_end = .;
    } > BOOTROM

    /* Trap handler at second 4K page of bootrom (0x0200B000) */
    .traprom 0x0200B000 : {
        *(traprom traprom.*)
    } > BOOTROM

    ASSERT(__bootrom_end <= 0x0200B000, ".bootrom overflows into trap handler!")

    /* Code that must be in MRAM (use __attribute__((section(".mram_text"))) */
    .mram_text : {
        . = ALIGN(8);
        *(.mram_text .mram_text.*)
    } > MRAM

    .data : {
        . = ALIGN(8);
        PROVIDE(__data_start = .);
        *(.data .data.*)
    } > MRAM

    .sdata : {
        __global_pointer$ = . + 0x800;
        . = ALIGN(8);
        *(.sdata .sdata.*)
        PROVIDE(__data_end = .);
    } > MRAM

    .bss (NOLOAD) : {
        . = ALIGN(8);
        PROVIDE(__bss_start = .);
        *(.bss .bss.*)
    } > MRAM

    .sbss (NOLOAD) : {
        *(.sbss .sbss.*)
        . = ALIGN(8);
        PROVIDE(__bss_end = .);
    } > MRAM

    .stack (NOLOAD) : {
        . = ALIGN(16);
        PROVIDE(__stack_start = .);
        . = . + (MAX_THREADS * STACK_SIZE);
        PROVIDE(__stack_base = .);
    } > SRAM
}

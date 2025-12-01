/*
 * $QNXLicenseC:
 * Copyright 2017, QNX Software Systems.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

     .text
    .align  2

    .globl aarch64_dcache_flush_va
    .globl aarch64_enable_mmu
    .globl aarch64_enable_mmu_4k
    .globl aarch64_enable_mmu_64k
    .globl aarch64_disable_mmu
    .globl aarch64_alignment_check_enable
    .globl aarch64_alignment_check_disable
    .globl aarch64_dcache_enable
    .globl aarch64_dcache_disable
    .globl aarch64_icache_enable
    .globl aarch64_icache_disable

/*
 * Flush cached data to memory and invalidate.
 *  x0 - start vaddr
 *  x1 - length to flush/invalidate
 */
aarch64_dcache_flush_va:
    mrs     x3, ctr_el0
    lsr     x3, x3, #16
    and     x3, x3, #0xf // DminLine
    mov     x2, #4
    lsl     x2, x2, x3   // 1<<(DminLine+2), min data cache line size in bytes
1:  dc      civac, x0
    add     x0, x0, x2
    subs    x1, x1, x2
    bgt     1b
    dsb     sy

    ret


aarch64_alignment_check_enable:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 1)
    msr     sctlr_el1, x0
    isb

    b       done

aarch64_alignment_check_disable:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    mrs     x0, sctlr_el1
    bic     x0, x0, #(1 << 1)
    msr     sctlr_el1, x0
    isb

    b       done

aarch64_dcache_enable:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    // Cache Flush
    bl      aarch64_cache_flush

    // Invalidate TLB All
    ic      iallu
    tlbi    vmalle1
    dsb     sy
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 2)
    msr     sctlr_el1, x0
    isb

    b       done

aarch64_dcache_disable:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    mrs     x0, sctlr_el1
    bic     x0, x0, #(1 << 2)
    msr     sctlr_el1, x0
    isb

    // Cache Flush
    bl      aarch64_cache_flush

    // Invalidate TLB All
    ic      iallu
    tlbi    vmalle1
    dsb     sy
    isb

    b       done

aarch64_icache_enable:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    // Cache Flush
    bl      aarch64_cache_flush

    // Invalidate TLB All
    ic      iallu
    tlbi    vmalle1
    dsb     sy
    isb

    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 12)
    msr     sctlr_el1, x0
    isb

    b       done

aarch64_icache_disable:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    mrs     x0, sctlr_el1
    bic     x0, x0, #(1 << 12)
    msr     sctlr_el1, x0
    isb

    // Cache Flush
    bl      aarch64_cache_flush

    // Invalidate TLB All
    ic      iallu
    tlbi    vmalle1
    dsb     sy
    isb

    b       done

aarch64_enable_mmu:
aarch64_enable_mmu_64k:
    // 64k pages TCR_EL1 Configuration
    mov     x1, #22             // 42 bits virtual address
    orr     x1, x1, #1 << 14    // AARCH64_TCR_EL1_TG0_64K
    b       1f

aarch64_enable_mmu_4k:
    // 4k pages TCR_EL1 Configuration
    mov     x1, #25             // 39 bits virtual address
//  orr     x1, x1, #0 << 14    // AARCH64_TCR_EL1_TG0_4K

1:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    // Check if MMU is already enabled
    mrs     x2, sctlr_el1
    tbnz    x2, #0, done

    // Set Translate Table Base
    msr     ttbr0_el1, x0

    // Common TCR_EL1 Configuration
    mrs     x2, id_aa64mmfr0_el1
    and     x2, x2, #7          // MMFR0_PARANGE
    orr     x1, x1, x2, lsl #32 // max physical address size for IPS
    orr     x1, x1, #3 << 12    // AARCH64_TCR_EL1_SH0_ISH
    orr     x1, x1, #1 << 10    // AARCH64_TCR_EL1_ORGN0_WBWA
    orr     x1, x1, #1 << 8     // AARCH64_TCR_EL1_IRGN0_WBWA
    orr     x1, x1, #1 << 23    // AARCH64_TCR_EL1_EPD1
    msr     tcr_el1, x1

    /*
     * Default MAIR value.
     * Index  MAIR  ARMv8 meaning
     * -----  ----  ------------- -------------
     *     0  0x00  device-nGnRnE strongly-ordered
     *     1  0x04  device-nGnRE  shared-device
     *     2  0x0C  device-GRE    shared-device
     *     3  0x44  normal-NC     normal-NC
     *     4  0xff  normal-NT     normal-NT
     *     5  0x00  reserved      reserved
     *     6  0x00  normal        normal
     *     7  0x00  normal        normal
     */
    ldr     x1, =0x000000ff440C0400
    msr     mair_el1, x1
    isb

    mrs     x1, sctlr_el1
    orr     x1, x1, #(1 << 0)   // MMU
    msr     sctlr_el1, x1
    isb

    b       done

aarch64_disable_mmu:
    sub     sp, sp, #16
    stp     x19, x30, [sp]

    // Check if MMU is disabled
    mrs     x1, sctlr_el1
    tbz     x1, #0, done

    mrs     x1, sctlr_el1
    bic     x1, x1, #(1 << 0)   // MMU
    msr     sctlr_el1, x1
    isb

    // Cache Flush
    bl      aarch64_cache_flush

    // Invalidate TLB All
    ic      iallu
    tlbi    vmalle1
    dsb     sy
    isb

done:
    ldp     x19, x30, [sp]
    add     sp, sp, #16
    ret

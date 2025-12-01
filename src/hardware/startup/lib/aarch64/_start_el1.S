#
# Copyright 2015, QNX Software Systems.
#
# Licensed under the Apache License, Version 2.0 (the "License"). You
# may not reproduce, modify or distribute this software except in
# compliance with the License. You may obtain a copy of the License
# at: http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
#
# This file may contain contributions from others, either as
# contributors under the License or as licensors under other terms.
# Please review this entire file for other proprietary rights or license
# notices, as well as the QNX Development Suite License Guide at
# http://licensing.qnx.com/license-guide/ for other information.
#

/*
 * Helper functions for cstart and smp_start to switch from EL3 to EL2 if
 * possible and EL1 if necessary.
 *
 * This code can be overridden by a board specific startup if necessary.
 *
 * NOTE: The global routines _start_el2_or_el1 and _start_el1 below are
 * designed to be called during the first few instructions of Startup and have
 * some special characteristics:
 * 1. they do not assume the availability of a stack
 * 2. they are not required to preserve "Callee-saved registers (X19-X29)" as
 *    normally expected by the aarch64 C Calling Convention.
 *
 * Below we manually ensure nested function calls dont conflict. For example,
 * we use both X20 and X21 to save X30/LR values across calls.
 */

#include "asmoff.def"

    .text
    .align 2

//-----------------------------------------------------------------------------
/**
 * Switch from EL3 to EL2 if we can (otherwise EL1) with interrupts disabled
 * This allows hypervisor support to be setup later if needed and if it's not
 * needed then drop_to_el1 may be called.
 *
 * IMPORTANT: see file header comment. This routine does not fully adhere to the
 * C calling convention, in particular it doesnt preserve callee-saved registers
 */
    .global _start_el2_or_el1
_start_el2_or_el1:
    // Save the return address because we will be doing BL calls later
    mov     x20, x30

    mrs     x0, CurrentEL
    lsr     x0, x0, #2              // Shift EL field
    cmp     x0, #0x3                // check for EL3
    beq     at_el3

    msr     SPSel, #1               // switch to system stack
    msr     daifset, #0xf           // disable exceptions

    cmp     x0, #0x2                // check for EL2
    beq     at_el2
    ret


at_el3:
    /*
     * Set up minimal EL3 state required for EL1 execution
     */

    msr     cptr_el3, xzr           // reset CPTR_EL3

    /*
     * Enable system register access to GIC if supported on aarch64
     */
    mrs     x1, id_aa64pfr0_el1
    tst     x1, 0xf << 24
    beq     1f
    mov     x1, 0xf
    msr     S3_6_C12_C12_5,x1       // ICC_SRE_EL3
1:
    /*
     * Setup SCR_EL3
     *
     * SCR_EL3.RW[10] - EL2 is aarch64 (if no EL2, then EL1 is aarch64)
     * SCR_EL3.HCE[8] - HVC instruction enable
     * SCR_EL3.NS[0]  - EL2, EL1 are in non-secure state
     * SCR_EL3.{API[17],APK[16]} - if present, for pauth (pointer authentication)
     */
    bl      is_pauth_supported
    mov     x1, #((1<<10)|(1<<8)|(1<<0))
    cbz     x0, 1f
    orr     x1, x1, #(3 << 16)
1:
    msr     scr_el3, x1
    isb

    /* If aarch64 EL2 is supported (anything in ID_AA64PFR0_EL1 bits 8-11), then
     * we can return to the caller at EL2 otherwise we have to return at EL1
     */
    mrs     x1, id_aa64pfr0_el1
    tst     x1, #(0xf << 8)
    bne     el2_is_supported

    // Return to caller, executing at EL1h with interrupts disabled
    msr     elr_el3, x20            // The orig LR is saved in X20
    mov     x0, #0x3c5              // DAIF + EL1h
    msr     spsr_el3, x0
    eret

el2_is_supported:
    // Drop to EL2h with interrupts disabled and continue at "at_el2"
    adr     x0, at_el2
    msr     elr_el3, x0
    mov     x0, #0x3c9              // DAIF + EL2h
    msr     spsr_el3, x0
    eret

/**
 * Setup minimal EL2 state (from EL2h with exceptions disabled)
 */
at_el2:
    /*
     * If MMU, data or instruction cache are on, turn them off
     * SCTLR_EL2.M [0]  - MMU enable
     * SCTLR_EL2.C [2]  - Data cache enable
     * SCTLR_EL2.I [12] - Instruction cache enable
     */
    mrs     x0, sctlr_el2
    mov     x1, #((1<<0)|(1<<2)|(1<<12))
    tst     x0, x1
    beq     1f
    bic     x0, x0, x1
    msr     sctlr_el2, x0
    isb
    bl      aarch64_cache_flush
1:

    /*
     * Assume that HCR_EL2 is random, so set it to a known good value
     * HCR_EL2.{API, APK} [41,40] - (3) = if pauth (pointer authentication) is
     *                              supported, don't trap pauth instructions
     * HCR_EL2.RW  [31] - execute EL1 in AArch64
     * HCR_EL2.HCD [29] - disable HVC
     * HCR_EL2.TSC [19] - 0 = dont trap SMC instructions to ensure PSCI power
     *                    mgmt calls go straight through to board firmware
     */
    bl      is_pauth_supported
    mov     x1, #((1 << 31) | (1 << 29))
    cbz     x0, 1f
    orr     x1, x1, #(3 << 40)
1:
    msr     hcr_el2, x1

    /*
     * Dont trap to EL2 for accesses to CPACR, CPACR_EL1, floating point, etc
     */
    msr     cptr_el2,xzr

    /*
     * Dont trap to EL2 for accesses to physical timer by EL0/1
     * CNTP_CTL_EL0, CNTP_CVAL_EL0, and CNTP_TVAL_EL0
     */
    mov     x0, #0x3
    msr     cnthctl_el2, x0

    /*
     * When EL2 is present, some EL1 register accesses are redirected by the
     * architecture to an EL2 equivalent. For now, we simply ensure both EL1
     * and EL2 will see the same values.
     */
    mrs     x0, mpidr_el1
    msr     vmpidr_el2, x0
    mrs     x0, midr_el1
    msr     vpidr_el2, x0

    msr     cntvoff_el2, xzr        // reset CNTVOFF_EL2
    msr     vttbr_el2, xzr          // clear VMID

    /*
     * Enable system register access to GIC if supported on aarch64
     */
    mrs     x1,id_aa64pfr0_el1
    tst     x1,0xf << 24
    beq     1f
    mov     x1,0xf
    msr     S3_4_C12_C9_5,x1        // ICC_SRE_EL2
1:

    /*
     * Restore the saved return addr and return to caller at EL2h with intrs disabled
     */
    mov     x30, x20
    ret

//-----------------------------------------------------------------------------
/**
 * A caller at EL2 may call this to drop to EL1h. It returns to caller at EL1h
 * with interrupts disabled and brings the current EL2 stack pointer to EL1 too.
 *
 * This routine is safe to call from C
 */
    .global    drop_to_el1
drop_to_el1:
    mov     x0, sp
    msr     sp_el1, x0
    msr     elr_el2, x30            // Setup for eret back to the caller (not ret)
    mov     x0, #0x3c5              // DAIF + EL1h
    msr     spsr_el2, x0
    eret

//-----------------------------------------------------------------------------
/**
 * Switch to EL1h from any of EL3, EL2 or EL1 and disable interrupts
 * Use this function only if hypervisor is not required
 *
 * IMPORTANT: see file header comment. This routine does not fully adhere to the
 * C calling convention, in particular it doesnt preserve callee-saved registers
*/
    .global _start_el1
_start_el1:
    mov     x21, x30                // Preserve return addr across next BL call
    bl      _start_el2_or_el1
    mov     x30, x21
    mrs     x0, CurrentEL
    lsr     x0, x0, #2              // Shift EL field right
    cmp     x0, #0x1                // check for EL1
    bne     drop_to_el1             // returns to our caller at EL1
    ret

//-----------------------------------------------------------------------------
/**
 * Check if FEAT_PAuth is supported. If the value of one or more of the
 * the following fields is non-zero, then pauth is supported:
 *     ID_AA64ISAR1_EL1.GPI  [31:28]
 *     ID_AA64ISAR1_EL1.GPA  [27:24]
 *     ID_AA64ISAR1_EL1.API  [11:8]
 *     ID_AA64ISAR1_EL1.APA  [7:4]
 *     ID_AA64ISAR2_EL1.APA3 [15:12]
 *     ID_AA64ISAR2_EL1.GPA3 [11:8]
 *
 * @returns x0 = 1 if supported, otherwise 0
 */
is_pauth_supported:
    mrs     x9, id_aa64isar1_el1
    tst     x9, (0xff << 4)         // ID_AA64ISAR1_EL1.{API, APA}
    bne     1f
    tst     x9, (0xff << 24)        // ID_AA64ISAR1_EL1.{GPI, GPA}
    bne     1f
    mrs     x9, id_aa64isar2_el1
    tst     x9, (0xff << 8)         // ID_AA64ISAR2_EL1.{APA3, GPA3}
    bne     1f
    mov     x0, #0
    ret
1:
    mov     x0, #1
    ret

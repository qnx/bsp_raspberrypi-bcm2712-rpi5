/*
 * Notice on Software Maturity and Quality
 *
 * The software included in this repository is classified under our Software Maturity Standard as Experimental Software - Software Quality and Maturity Level (SQML) 1.
 *
 * As defined in the QNX Development License Agreement, Experimental Software represents early-stage deliverables intended for evaluation or proof-of-concept purposes.
 *
 * SQML 1 indicates that this software is provided without one or more of the following:
 *     - Formal requirements
 *     - Formal design or architecture
 *     - Formal testing
 *     - Formal support
 *     - Formal documentation
 *     - Certifications of any type
 *     - End-of-Life or End-of-Support policy
 *
 * Additionally, this software is not monitored or scanned under our Cybersecurity Management Standard.
 *
 * No warranties, guarantees, or claims are offered at this SQML level.
 */


# Copyright 2014, 2021, 2025 QNX Software Systems.
# Copyright 2016, Freescale Semiconductor, Inc.
# Copyright 2017 NXP
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
 * GICv3 interrupt controller callouts.
 *
 * See callout.ah for description of special registers for interrupt callouts.
 */

#include <aarch64/callout.ah>
#include "aarch64/gic_v3.h"


#define	ARM_GITS_CWRITER	0x88

/*
 * Get access to the MMIO GICC registers
 *
 * Patcher routines take the following arguments:
 *
 * x0 - syspage paddr
 * x1 - vaddr of callout
 * x2 - offset from start of syspage to start of callout routine
 * x3 - offset from start of syspage to rw storage
 * x4 - patch data
 * x5 - callout_rtn
 */
gicc_patch:

    /* x2 points to the address of callout routine */
    add     x2, x0, x2

    /*
     * patch the base vaddr for the GIC CPU registers (GICC)
     * x4 points to the first entry in the patch data structure which contains this information
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x7, #1234)
    */
    ldr    x0, [x4]
    CALLOUT_PATCH   x2, w5, w6

    ret

/*
 * -----------------------------------------------------------------------
 * Identify interrupt source
 *
 * INTID returned in X19
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_id_gic_v3_ppi_sr, 0, 0)

    /* Get interrupt ID and handle special cases */
    mrs     x24, ARM_GICC_IAR1_EL1
    cmp     x24, #31
    mov     x0, #-1
    /* check for non-PPI/SGI value and return INTID if <= 31, -1 otherwise */
    csel    x19, x0, x24, hi

CALLOUT_END(interrupt_id_gic_v3_ppi_sr)

CALLOUT_START(interrupt_id_gic_v3_ppi_mm, 0, gicc_patch)

    /* x7 will be patched with the vaddr for the base of the GIC CPU registers (GICC) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* Get interrupt ID and handle special cases */
    ldr     w24, [x7, ARM_GICC_IAR]
    cmp     w24, #31
    mov     x0, #-1
    /* check for non-PPI/SGI value and return INTID if <= 31, -1 otherwise */
    csel    x19, x0, x24, hi

CALLOUT_END(interrupt_id_gic_v3_ppi_mm)

/*
 * -----------------------------------------------------------------------
 * Acknowledge specified interrupt
 *
 * x19 - contains interrupt id
 * x24 - contains original IAR read value from interrupt_id_gic_v3_ppi_sr
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_eoi_gic_v3_ppi_sr, 0, 0)

    /* Send EOI */
    msr     ARM_GICC_EOIR1_EL1, x24
    isb

CALLOUT_END(interrupt_eoi_gic_v3_ppi_sr)

CALLOUT_START(interrupt_eoi_gic_v3_ppi_mm, 0, gicc_patch)

    /* x7 will be patched with the vaddr for the base of the GIC CPU registers (GICC) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    str     w24, [x7, ARM_GICC_EOIR]

CALLOUT_END(interrupt_eoi_gic_v3_ppi_mm)


/*
 * Patch PPI/SGI interrupt callouts to access GICR regs
 *
 * Patcher routines take the following arguments:
 *
 * x0 - syspage paddr
 * x1 - vaddr of callout
 * x2 - offset from start of syspage to start of callout routine
 * x3 - offset from start of syspage to rw storage
 * x4 - patch data
 * x5 - callout_rtn
*/
ppi_patch:

    /* x2 points to the address of callout routine */
    add     x2, x0, x2

    /*
     * patch the base vaddr for the redistributor registers
     * x4 points to the first entry in the patch data structure which contains
     * the GICC vaddr so step past it with a pre-indexed load
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x7, #1234)
    */
    ldr    x0, [x4, #8]!
    /* All the registers we want to access are in the SGI region */
    add     x0, x0, #ARM_GICR_SGI_BASE_OFFSET
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the GICR shift value
     * x4 is (still) pointing to the second patch data field so adjust to point
     * to the GICR shift value with a pre-indexed load
     * w0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x8, #1234)
    */
    ldr    w0, [x4, #8]!
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    ret

/*
 * -----------------------------------------------------------------------
 * Mask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v3_ppi, 0, ppi_patch)

    /* x7 will be patched with the vaddr for the base of the redistributor registers (GICR) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* x8 will be patched with the GICR spacing shift value */
    mov     x8, #1234

    /* adjust GICR to proper CPU */
    mrs     x9, tpidr_el1
    /* top bit indicate whether GICR location is in the top 32-bits */
    and     x2, x9, #(1ul << 63)
    cbz     x2, 0f
    lsr     x9, x9, #32
    /* x7 has the right GICR region, only bits 32-47 of TPIDR matter */
    and     x9, x9, #0xffff
0:
    lsl     x9, x9, x8
    add     x7, x7, x9

    /*
     * Mask interrupt
     * Bit to set is 1 << INTID (x1)
     */
    mov     w2, #1
    lsl     w1, w2, w1
    str     w1, [x7, #ARM_GICR_ICENABLER0]

    /* wait for RWP to be clear */
    /* x7 is in the SGI_base frame, but GICR_CTLR is in the RD_base frame */
    sub     x10, x7, #ARM_GICR_SGI_BASE_OFFSET
1:
    ldr     w2, [x10, #ARM_GICR_CTLR]
    tbnz    w2, #3, 1b

    mov     x0, #0
    ret

CALLOUT_END(interrupt_mask_gic_v3_ppi)

/*
 * -----------------------------------------------------------------------
 * Unmask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v3_ppi, 0, ppi_patch)

    /* x7 will be patched with the vaddr for the base of the redistributor registers (GICR) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* x8 will be patched with the GICR spacing shift value */
    mov     x8, #1234

    /* adjust GICR to proper CPU */
    mrs     x9, tpidr_el1
    /* top bit indicate whether GICR location is in the top 32-bits */
    and     x2, x9, #(1ul << 63)
    cbz     x2, 0f
    lsr     x9, x9, #32
    /* x7 has the right GICR region, only bits 32-47 of TPIDR matter */
    and     x9, x9, #0xffff
0:
    lsl     x9, x9, x8
    add     x7, x7, x9

    /*
     * Unmask interrupt
     * Bit to set is 1 << INTID (x1)
     */
    mov     w2, #1
    lsl     w1, w2, w1
    str     w1, [x7, #ARM_GICR_ISENABLER0]

    mov     x0, #0
    ret

CALLOUT_END(interrupt_unmask_gic_v3_ppi)

/*
 * -----------------------------------------------------------------------
 * Configure interrupt flags for a specified interrupt vector
 *
 * x0 - syspage pointer
 * x1 - intrinfo_entry pointer
 * x2 - vector number (relative to controller)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_config_gic_v3_ppi, 0, 0)
    /*
     * Use ID0 for IPI
     */
    cmp     x2, #0
    mov     w3, #INTR_CONFIG_FLAG_IPI
    csel    w0, w3, wzr, eq
    ret

CALLOUT_END(interrupt_config_gic_v3_ppi)

/*
 * -----------------------------------------------------------------------
 * Identify interrupt source
 *
 * X24 - read of IAR from interrupt_id_gic_v3_ppi_[sr,mr]
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_id_gic_v3_spi, 0, 0)
    /*
     * Get interrupt ID and handle special cases:
     * ID1022 - spurious interrupt
     * ID1023 - spurious interrupt
     */
    /* make return value relative to SPIs */
    sub     x0, x24, #32
    cmp     x24, #1021
    mov     x1, #-1
    /* Return INTID if <= 1021, -1 otherwise */
    csel    x19, x1, x0, hi

CALLOUT_END(interrupt_id_gic_v3_spi)

/*
 * -----------------------------------------------------------------------
 * Acknowledge specified SPI interrupt
 *
 * x19 - contains interrupt id
 * x24 - contains original IAR read value from interrupt_id_gic_v3_ppi_sr
 *
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_eoi_gic_v3_spi_sr, 0, 0)

    /* Send EOI */
    msr     ARM_GICC_EOIR1_EL1, x24
    isb

CALLOUT_END(interrupt_eoi_gic_v3_spi_sr)

CALLOUT_START(interrupt_eoi_gic_v3_spi_mm, 0, gicc_patch)

    /* x7 will be patched with the vaddr for the base of the GIC CPU registers (GICC) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    str     w24, [x7, ARM_GICC_EOIR]

CALLOUT_END(interrupt_eoi_gic_v3_spi_mm)


/*
 * Patch SPI/ESPI interrupt callouts to access GICD regs

 * Patcher routines take the following arguments:
 *  x0 - syspage paddr
 *  x1 - vaddr of callout
 *  x2 - offset from start of syspage to start of callout routine
 *  x3 - offset from start of syspage to rw storage
 *  x4 - patch data
 *  x5 - callout_rtn
 */
spi_patch:

    /* x2 points to the address of callout routine */
    add     x2, x0, x2

    /*
     * patch the base vaddr for the distributor registers
     * x4 points to the first entry in the patch data structure which contains
     * the GICC vaddr so step past it with a pre-indexed load
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x15, #1234)
    */
    ldr    x0, [x4, #8]!
    CALLOUT_PATCH   x2, w5, w6

/*
     * Now patch the SPI vector adjustment value
     * x4 is (still) pointing to the second patch data field so adjust to point
     * to the vector offset with a pre-indexed load
     * w0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x8, #1234)
    */
    ldr    w0, [x4, #8]!
    CALLOUT_PATCH_RW   x2, w0, w5, w6
    ret

/*
 * -----------------------------------------------------------------------
 * Mask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to SPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v3_spi, 0, spi_patch)

    /* x7 will be patched with the vaddr for the base of the distributor registers (GICD) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* x8 will be patched with the SPI offset adjustment for this SPI block
    /* could have used an add x1, x1, #1234 but then couldn't use CALLOUT_PATCH_RW macro */
    mov     x8, #1234
    add     x1, x1, x8

    /*
     * Mask interrupt
     * Bit to set is 1 << (INTID % 32)
     * Register Index is INTID / 32
     * ICENABLERn[Register Index] = bit
     */
    add     x5, x7, #ARM_GICD_ICENABLERn + 4
    and     w0, w1, #0x1f
    mov     w2, #1
    lsl     w2, w2, w0
    lsr     w0, w1, #5
    str     w2, [x5, x0, lsl #2]

    /* wait for RWP to be clear */
1:
    ldr     w2, [x7, #ARM_GICD_CTLR]
    tbnz    w2, #31, 1b

    mov     x0, #0
    ret

CALLOUT_END(interrupt_mask_gic_v3_spi)

/*
 * -----------------------------------------------------------------------
 * Unmask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to SPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v3_spi, 0, spi_patch)

    /* x7 will be patched with the vaddr for the base of the distributor registers (GICD) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* x8 will be patched with the SPI offset adjustment for this SPI block
    /* could have used an add x1, x1, #1234 but then couldn't use CALLOUT_PATCH_RW macro */
    mov     x8, #1234
    add     x1, x1, x8

    /*
     * Unmask interrupt
     * Bit to set is 1 << (INTID % 32)
     * Register Index is INTID / 32
     * ISENABLERn[Register Index] = bit
     */
    add     x5, x7, #ARM_GICD_ISENABLERn + 4
    and     w0, w1, #0x1f
    mov     w2, #1
    lsl     w2, w2, w0
    lsr     w0, w1, #5
    str     w2, [x5, x0, lsl #2]

    mov     x0, #0
    ret
CALLOUT_END(interrupt_unmask_gic_v3_spi)

/*
 * -----------------------------------------------------------------------
 * Identify ESPI interrupt source
 *
 * X24 - read of IAR from interrupt_id_gic_v3_ppi_[sr,mr]
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_id_gic_v3_espi, 0, 0)
    /*
     * Get interrupt ID
     */
    /* make return value relative to ESPIs */
    sub     x0, x24, #4096
    cmp     x0, #1023
    mov     x1, #-1
    /* Return INTID if <= 4096+1023, -1 otherwise */
    csel    x19, x1, x0, hi

CALLOUT_END(interrupt_id_gic_v3_espi)

/*
 * -----------------------------------------------------------------------
 * Acknowledge specified ESPI interrupt
 *
 * x19 - contains interrupt id
 * x24 - contains original IAR read value from interrupt_id_gic_v3_ppi_sr
 *
 * -----------------------------------------------------------------------
 */
 /* same as SPI EOI so use that routine */

/*
 * -----------------------------------------------------------------------
 * Mask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to ESPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v3_espi, 0, spi_patch)

    /* x7 will be patched with the vaddr for the base of the distributor registers (GICD) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* x8 will be patched with the ESPI offset adjustment for this ESPI block
    /* could have used an add x1, x1, #1234 but then couldn't use CALLOUT_PATCH_RW macro */
    mov     x8, #1234
    add     x1, x1, x8

    /*
     * Mask interrupt
     * Bit to set is 1 << (INTID % 32)
     * Register Index is INTID / 32
     * ICENABLERnE[Register Index] = bit
     */
    mov     x5, #ARM_GICD_ICENABLERnE
    add     x5, x7, x5
    and     w0, w1, #0x1f
    mov     w2, #1
    lsl     w2, w2, w0
    lsr     w0, w1, #5
    str     w2, [x5, x0, lsl #2]

    /* wait for RWP to be clear */
1:
    ldr     w2, [x7, #ARM_GICD_CTLR]
    tbnz    w2, #31, 1b

    mov     x0, #0
    ret

CALLOUT_END(interrupt_mask_gic_v3_espi)

/*
 * -----------------------------------------------------------------------
 * Unmask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to ESPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v3_espi, 0, spi_patch)

    /* x7 will be patched with the vaddr for the base of the distributor registers (GICD) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    /* x8 will be patched with the ESPI offset adjustment for this ESPI block
    /* could have used an add x1, x1, #1234 but then couldn't use CALLOUT_PATCH_RW macro */
    mov     x8, #1234
    add     x1, x1, x8

    /*
     * Unmask interrupt
     * Bit to set is 1 << (INTID % 32)
     * Register Index is INTID / 32
     * ISENABLERnE[Register Index] = bit
     */
    mov     x5, #ARM_GICD_ISENABLERnE
    add     x5, x7, x5
    and     w0, w1, #0x1f
    mov     w2, #1
    lsl     w2, w2, w0
    lsr     w0, w1, #5
    str     w2, [x5, x0, lsl #2]

    mov     x0, #0
    ret
CALLOUT_END(interrupt_unmask_gic_v3_espi)


/*
 * -----------------------------------------------------------------------
 * Identify interrupt source (LPI's)
 *
 * X24 - read of IAR from interrupt_id_gic_v3_ppi_[sr,mr]
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_id_gic_v3_lpi, 0, 0)

    /* make return value relative to LPIs */
	sub		x0, x24, #8192
    cmp     x24, #8192
    mov     x1, #-1
    /* Return INTID if >= 8192, -1 otherwise */
    csel    x19, x1, x0, lo

CALLOUT_END(interrupt_id_gic_v3_lpi)

/*
 * -----------------------------------------------------------------------
 * Acknowledge specified interrupt
 *
 * x19 - contains interrupt id
 * x24 - contains original IAR read value from interrupt_id_gic_v3_ppi_sr
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_eoi_gic_v3_lpi_sr, 0, 0)

    /* Send EOI */
    msr     ARM_GICC_EOIR1_EL1, x24
    isb

CALLOUT_END(interrupt_eoi_gic_v3_lpi_sr)

CALLOUT_START(interrupt_eoi_gic_v3_lpi_mm, 0, gicc_patch)

    /* x7 will be patched with the vaddr for the base of the GIC CPU registers (GICC) */
    mov     x7, #1234
    movk    x7, #1234, lsl #16
    movk    x7, #1234, lsl #32
    movk    x7, #1234, lsl #48

    str     w24, [x7, ARM_GICC_EOIR]

CALLOUT_END(interrupt_eoi_gic_v3_lpi_mm)


/*
 * Patch LPI direct interrupt callouts to access LPI configuration table
 *
 * Patcher routines take the following arguments:
 *
 * x0 - syspage paddr
 * x1 - vaddr of callout
 * x2 - offset from start of syspage to start of callout routine
 * x3 - offset from start of syspage to rw storage
 * x4 - patch data
 * x5 - callout_rtn
 */
lpi_direct_patch:

    /* x2 points to the address of callout routine */
    add     x2, x0, x2

    /*
     * patch the base vaddr for the redistributor registers
     * x4 points to the first entry in the patch data structure which contains
     * the GICC vaddr so step past it with a pre-indexed load
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x15, #1234)
    */
    ldr    x0, [x4, #8]!
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the vaddr for the LPI config table
     * x4 has been adjusted to point to the next patch data variable which contains this information
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x14, #1234)
    */
    add    x4, x4, #8
    ldr    x0, [x4], #8
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the redistributor shift value
     * x4 has been adjusted to point to the patch data variable which contains this information
     * w0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x13, #1234)
    */
    ldr    w0, [x4], #4
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    /*
     * Now patch the LPI vector offset for this LPI callout block
     * x4 has been adjusted to point to the patch data variable which contains this information
     * w0 contains the patch data
     * x2 needs to be advanced to the next instruction to patch (ie. mov x12, #1234)
    */
    ldr    w0, [x4]
    add    x2, x2, #4
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    ret

/*
 * -----------------------------------------------------------------------
 * Mask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to LPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v3_lpi_direct, 0, lpi_direct_patch)

    /* x15 will be patched with the vaddr for the base of the redistributor registers */
    mov     x15, #1234
    movk    x15, #1234, lsl #16
    movk    x15, #1234, lsl #32
    movk    x15, #1234, lsl #48

    /* x14 will be patched with the vaddr for the LPI config table */
    mov     x14, #1234
    movk    x14, #1234, lsl #16
    movk    x14, #1234, lsl #32
    movk    x14, #1234, lsl #48

    /* x13 will be patched with the GICR spacing shift value */
    mov     x13, #1234
    /*
     * x12 will be patched with the LPI vector offset for this LPI callout block
    */
    mov     x12, #1234
    add     x12, x1, x12

    /*
     * At this point the following register assignments exist
     *
     *    x15 - pointer to the base of the redistributor registers
     *    x14 - pointer to the base of the LPI config table
     *    x13 - contains the GICR spacing shift value
     *    x12 - offset of LPI vector entry to mask/disable
     *    x1 - x11 - scratch
    */

    /* mask (disable) interrupt in the LPI config table */
    ldrb    w2, [x14, x12]
    and     w3, w2, #0xfe
    strb    w3, [x14, x12]

    /* loop invalidating redistributor cache entries for the INTID (x1) */
    mov     x0, #1
    lsl     x0, x0, x13
1:
    str     w1, [x15, ARM_GICR_INVLPIR]
    ldr     x2, [x15, ARM_GICR_TYPER]
    add     x15, x15, x0
    /* break loop on last entry */
    tst     x2, GICR_TYPER_LAST
    beq     1b

    mov     x0, #0
    ret

CALLOUT_END(interrupt_mask_gic_v3_lpi_direct)

/*
 * -----------------------------------------------------------------------
 * Unmask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to LPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v3_lpi_direct, 0, lpi_direct_patch)

    /* x15 will be patched with the vaddr for the base of the redistributor registers */
    mov     x15, #1234
    movk    x15, #1234, lsl #16
    movk    x15, #1234, lsl #32
    movk    x15, #1234, lsl #48

    /* x14 will be patched with the vaddr for the LPI config table */
    mov     x14, #1234
    movk    x14, #1234, lsl #16
    movk    x14, #1234, lsl #32
    movk    x14, #1234, lsl #48

    /* x13 will be patched with the GICR spacing shift value */
    mov     x13, #1234
    /*
     * x12 will be patched with the LPI vector offset for this LPI callout block
    */
    mov     x12, #1234
    add     x12, x1, x12

    /*
     * At this point the following register assignments exist
     *
     *    x15 - pointer to the base of the redistributor registers
     *    x14 - pointer to the base of the LPI config table
     *    x13 - contains the GICR spacing shift value
     *    x12 - offset of LPI vector entry to unmask/enable
     *    x1 - x11 - scratch
    */

    /* unmask (enable) interrupt in the LPI config table */
    ldrb    w2, [x14, x12]
    orr     w3, w2, #1
    strb    w3, [x14, x12]

    /* loop invalidating redistributor cache entries for the INTID (x1) */
    mov     x0, #1
    lsl     x0, x0, x13
1:
    str     w1, [x15, ARM_GICR_INVLPIR]
    ldr     x2, [x15, ARM_GICR_TYPER]
    add     x15, x15, x0
    /* break loop on last entry */
    tst     x2, GICR_TYPER_LAST
    beq     1b

    mov     x0, #0
    ret

CALLOUT_END(interrupt_unmask_gic_v3_lpi_direct)


/*
 * Patch LPI ITS interrupt callouts to access LPI configuration table and
 * to be able to issue commands to the ITS
 *
 * Patcher routines take the following arguments:
 *
 * x0 - syspage paddr
 * x1 - vaddr of callout
 * x2 - offset from start of syspage to start of callout routine
 * x3 - offset from start of syspage to rw storage
 * x4 - patch data
 * x5 - callout_rtn
 */
lpi_its_patch:

    /* x2 points to the address of callout routine */
    add     x2, x0, x2

    /*
     * patch the base vaddr for the ITS registers
     * x4 points to the first entry in the patch data structure which contains
     * the GICC vaddr so step past it with a pre-indexed load
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x15, #1234)
    */
    ldr    x0, [x4, #8]!
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the vaddr for the LPI config table
     * x4 has been adjusted to point to the next patch data variable which contains this information
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x14, #1234)
    */
    add    x4, x4, #8
    ldr    x0, [x4], #8
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the vaddr for the ITS command queue
     * x4 has been adjusted to point to the next patch data variable which contains this information
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x13, #1234)
    */
    ldr    x0, [x4], #8
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the vaddr for the ITS command queue lock
     * x4 has been adjusted to point to the next patch data variable which contains this information
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x12, #1234)
    */
    ldr    x0, [x4], #8
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the RDbase value
     * x4 has been adjusted to point to the next patch data variable which contains this information
     * x0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x11, #1234)
    */
    ldr    x0, [x4], #8
    CALLOUT_PATCH   x2, w5, w6

    /*
     * Now patch the redistributor shift value
     * x4 has been adjusted to point to the patch data variable which contains this information
     * w0 contains the patch data
     * x2 is pointing to the instruction to patch (ie. mov x10, #1234)
    */
    ldr    w0, [x4], #4
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    /*
     * Now patch the ITS last entry offset
     * x4 has been adjusted to point to the patch data variable which contains this information
     * w0 contains the patch data
     * x2 needs to be advanced to the next instruction to patch (ie. mov x9, #1234)
    */
    ldr    w0, [x4], #4
	add    x2, x2, #4
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    /*
     * Now patch the number of collections to loop over
     * x4 has been adjusted to point to the patch data variable which contains this information
     * w0 contains the patch data
     * x2 needs to be advanced to the next instruction to patch (ie. mov x8, #1234)
    */
    ldr    w0, [x4], #4
	add    x2, x2, #4
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    /*
     * Now patch the LPI vector offset for this LPI callout block
     * x4 has been adjusted to point to the patch data variable which contains this information
     * w0 contains the patch data
     * x2 needs to be advanced to the next instruction to patch (ie. mov x7, #1234)
    */
    ldr    w0, [x4]
    add    x2, x2, #4
    CALLOUT_PATCH_RW   x2, w0, w5, w6

    ret

/*
 * -----------------------------------------------------------------------
 * Mask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to LPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v3_lpi_its, 0, lpi_its_patch)

    /* x15 will be patched with the vaddr for the GIC ITS registers */
    mov     x15, #1234
    movk    x15, #1234, lsl #16
    movk    x15, #1234, lsl #32
    movk    x15, #1234, lsl #48

    /* x14 will be patched with the vaddr for the LPI config table */
    mov     x14, #1234
    movk    x14, #1234, lsl #16
    movk    x14, #1234, lsl #32
    movk    x14, #1234, lsl #48

    /* x13 will be patched with the vaddr for ITS command queue */
    mov     x13, #1234
    movk    x13, #1234, lsl #16
    movk    x13, #1234, lsl #32
    movk    x13, #1234, lsl #48

    /* x12 will be patched with the vaddr for the lock which protects access to the command queue */
    mov     x12, #1234
    movk    x12, #1234, lsl #16
    movk    x12, #1234, lsl #32
    movk    x12, #1234, lsl #48
    /* x11 will be patched with the RDbase value for the redistributor SYNC command */
    mov     x11, #1234
    movk    x11, #1234, lsl #16
    movk    x11, #1234, lsl #32
    movk    x11, #1234, lsl #48

    /* x10 will be patched with the spacing shift value to the next redistributor based on the format of RDbase */
    mov     x10, #1234
    /* x9 will be patched with the last ITS command queue offset before wrap is required */
    mov     x9, #1234
    /* x8 will be patched with the number of collections we need to issue ITS commands to */
    mov     x8, #1234
    /*
     * x7 will be patched with the LPI vector offset for this LPI callout block
     * This is required in order to get the correct offset into the LPI config
     * table to manipulate the 'enable' bit
    */
    mov     x7, #1234
    add     x7, x1, x7

    /*
     * At this point the following register assignments exist
     *
     *    x15 - pointer to frame 0 of the GITS registers
     *    x14 - pointer to the base of the LPI config table
     *    x13 - pointer to the base of the ITS command queue
     *    x12 - pointer to the lock protecting the ITS command queue
     *    x11 - RDbase value required for the SYNC command
     *    x10 - RDbase increment value (based on RDbase format)
     *    x9  - last ITS command queue offset before wrap is required
     *    x8  - number of collections to which the INVALL and SYNC commands must be issued
     *    x7  - offset of LPI vector entry to mask/disable
     *    x1 - x6 - scratch
    */
    /* Obtain spinlock - InterruptLock() */
    mov     w4, #1
mask_lock:
    ldaxr   w1, [x12]
    cbnz    w1, 0f
    stxr    w1, w4, [x12]
    cbnz    w1, mask_lock
    b       1f
0:
    wfe
    b       mask_lock

1:
    /* mask (disable) interrupt in the LPI config table */
    ldrb    w2, [x14, x7]
    and     w3, w2, #0xfe
    strb    w3, [x14, x7]

    /*
     * Issue INVALL and SYNC commands to the ITS command queue
     *
     *    x2 - will contain the command to write
     *    x3 - will hold the current ITS command queue offset
     *    x4 - will hold the current ITS command queue write address
     *    x5 - will hold the current collection number (ie. CPU/PE count)
    */
    ldr     x3, [x15, ARM_GITS_CWRITER]
    mov     x5, x8

mask_loop:
    /* point to next free ITS command Q position */
    add     x4, x13, x3

    /* build INVALL command */
    mov     x2, #0xD
    str     x2, [x4], #8
    str     xzr, [x4], #8
    sub     x2, x8, x5			/* ICID */
    str     x2, [x4], #8
    str     xzr, [x4], #8
    add     x3, x3, #32
    /* check whether we're past the last entry and if so wrap to 0 */
    cmp		x3, x9
    bls     2f
    mov     x3, 0
    mov     x4, x13
2:
    /* build SYNC command */
    mov     x2, #0x5
    str     x2, [x4], #8
    str     xzr, [x4], #8
    /* write out and update RDbase */
    str     x11, [x4], #8
    add     x11, x11, x10
    str     xzr, [x4], #8
    add     x3, x3, #32
    /* check whether we're past the last entry and if so wrap to 0 */
    cmp		x3, x9
    bls     3f
    mov     x3, 0
3:
    /* update loop count */
    subs    x5, x5, #1
    cbnz    x5, mask_loop

    /* update CWRITER */
    str     x3, [x15, ARM_GITS_CWRITER]
    dsb sy

    /* release lock - InterruptUnlock() */
    stlr	wzr, [x12]
    dsb sy
    sev

    mov     x0, #0
    ret

CALLOUT_END(interrupt_mask_gic_v3_lpi_its)

/*
 * -----------------------------------------------------------------------
 * Unmask specified interrupt
 *
 * x0 - _syspage_ptr
 * x1 - vector number (relative to LPI's)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v3_lpi_its, 0, lpi_its_patch)

    /* x15 will be patched with the vaddr for the GIC ITS registers */
    mov     x15, #1234
    movk    x15, #1234, lsl #16
    movk    x15, #1234, lsl #32
    movk    x15, #1234, lsl #48

    /* x14 will be patched with the vaddr for the LPI config table */
    mov     x14, #1234
    movk    x14, #1234, lsl #16
    movk    x14, #1234, lsl #32
    movk    x14, #1234, lsl #48

    /* x13 will be patched with the vaddr for ITS command queue */
    mov     x13, #1234
    movk    x13, #1234, lsl #16
    movk    x13, #1234, lsl #32
    movk    x13, #1234, lsl #48

    /* x12 will be patched with the vaddr for the lock which protects access to the command queue */
    mov     x12, #1234
    movk    x12, #1234, lsl #16
    movk    x12, #1234, lsl #32
    movk    x12, #1234, lsl #48
    /* x11 will be patched with the RDbase value for the redistributor SYNC command */
    mov     x11, #1234
    movk    x11, #1234, lsl #16
    movk    x11, #1234, lsl #32
    movk    x11, #1234, lsl #48

    /* x10 will be patched with the spacing shift value to the next redistributor based on the format of RDbase */
    mov     x10, #1234
    /* x9 will be patched with the last ITS command queue offset before wrap is required */
    mov     x9, #1234
    /* x8 will be patched with the number of collections we need to issue ITS commands to */
    mov     x8, #1234
    /*
     * x7 will be patched with the LPI vector offset for this LPI callout block
     * This is required in order to get the correct offset into the LPI config
     * table to manipulate the 'enable' bit
    */
    mov     x7, #1234
    add     x7, x1, x7

    /*
     * At this point the following register assignments exist
     *
     *    x15 - pointer to frame 0 of the GITS registers
     *    x14 - pointer to the base of the LPI config table
     *    x13 - pointer to the base of the ITS command queue
     *    x12 - pointer to the lock protecting the ITS command queue
     *    x11 - RDbase value required for the SYNC command
     *    x10 - RDbase increment value (based on RDbase format)
     *    x9  - last ITS command queue offset before wrap is required
     *    x8  - number of collections to which the INVALL and SYNC commands must be issued
     *    x7  - offset of LPI vector entry to unmask/enable
     *    x1 - x6 - scratch
    */
    /* Obtain spinlock - InterruptLock() */
    mov     w4, #1
unmask_lock:
    ldaxr   w1, [x12]
    cbnz    w1, 0f
    stxr    w1, w4, [x12]
    cbnz    w1, unmask_lock
    b       1f
0:
    wfe
    b       unmask_lock

1:
    /* unmask (enable) interrupt in the LPI config table */
    ldrb    w2, [x14, x7]
    orr     w3, w2, #1
    strb    w3, [x14, x7]

    /*
     * Issue INVALL and SYNC commands to the ITS command queue
     *
     *    x2 - will contain the command to write
     *    x3 - will hold the current ITS command queue offset
     *    x4 - will hold the current ITS command queue write address
     *    x5 - will hold the current collection number (ie. CPU/PE count)
    */
    ldr     x3, [x15, ARM_GITS_CWRITER]
    mov     x5, x8

unmask_loop:
    /* point to next free ITS command Q position */
    add     x4, x13, x3

    /* build INVALL command */
    mov     x2, #0xD
    str     x2, [x4], #8
    str     xzr, [x4], #8
    sub     x2, x8, x5			/* ICID */
    str     x2, [x4], #8
    str     xzr, [x4], #8
    add     x3, x3, #32
    /* check whether we're past the last entry and if so wrap to 0 */
    cmp     x3, x9
    bls     2f
    mov     x3, 0
    mov     x4, x13
2:
    /* build SYNC command */
    mov     x2, #0x5
    str     x2, [x4], #8
    str     xzr, [x4], #8
    /* write out and update RDbase */
    str     x11, [x4], #8
    add     x11, x11, x10
    str     xzr, [x4], #8
    add     x3, x3, #32
    /* check whether we're past the last entry and if so wrap to 0 */
    cmp		x3, x9
    bls     3f
    mov     x3, 0
3:
    /* update loop count */
    subs    x5, x5, #1
    cbnz    x5, unmask_loop

    /* update CWRITER */
    str     x3, [x15, ARM_GITS_CWRITER]
	dsb sy

    /* release lock - InterruptUnlock() */
    stlr	wzr, [x12]
    dsb sy
    sev

    mov     x0, #0
    ret

CALLOUT_END(interrupt_unmask_gic_v3_lpi_its)

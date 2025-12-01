#
# Copyright 2014, QNX Software Systems.
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
 * GICv2 interrupt controller callouts.
 *
 * See callout.ah for description of special registers for interrupt callouts.
 */

#include <aarch64/callout.ah>
#include "aarch64/gic_v2.h"

/*
 * Patch interrupt callouts to access GICC/GICD registers
 *
 * Patcher routine takes the following arguments:
 *	x0 - syspage paddr
 *	x1 - vaddr of callout
 *	x2 - offset from start of syspage to start of callout routine
 *	x3 - offset from start of syspage to rw storage
 *	x4 - patch data
 *	x5 - callout_rtn
 */

gicc_patch:
    add     x2, x0, x2                  // x2 = address of callout routine
	ldr		x0, gic_gicc_vaddr

    CALLOUT_PATCH   x2, w4, w5

    ret

gicd_patch:
    add     x2, x0, x2                  // x2 = address of callout routine
	ldr		x0, gic_gicd_vaddr

    CALLOUT_PATCH   x2, w4, w5

    ret

/*
 * -----------------------------------------------------------------------
 * Identify PPI interrupt source
 *
 *	x20 - syspage pointer
 *
 * Controller-relative Interrupt ID returned in x19
 * Read from IAR returned in x24
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_id_gic_v2_ppi, 0, gicc_patch)
	mov		x6, #1234				// vaddr for GICC (patched)
	movk	x6, #1234, lsl #16		// ...
	movk	x6, #1234, lsl #32		// ...
	movk	x6, #1234, lsl #48		// ...

	/*
	 * Get interrupt ID and handle special cases:
	 * ID0    - used for IPIs
	 */
	ldr		w0, [x6, #ARM_GICC_IAR]
	and		w19, w0, #ARM_GICC_IAR_IDMASK
	cmp		w19, #31
	mov		x1, #-1
	mov		w24,w19 // save for id_gic_v2_spi
	// Return intid if it's <= 31, -1 otherwise
	csel	x19, x1, x19, hi
	cbnz	x19, 2f

	/*
	 * IPI interrupt (ID0) - acknowledge using full SRCID and exit
	 */
	str		w0, [x6, #ARM_GICC_EOIR]

	/*
	 * Done - interrupt id is in x19
	 */
2:
CALLOUT_END(interrupt_id_gic_v2_ppi)


/*
 * -----------------------------------------------------------------------
 * Identify SPI interrupt source
 *
 * X24 - read of IAR from interrupt_id_gic_v2_ppi
 *
 * Controller-relative Interrupt ID returned in x19
 * Read from IAR in x24 is untouched.
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_id_gic_v2_spi, 0, 0)
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

CALLOUT_END(interrupt_id_gic_v2_spi)


/*
 * -----------------------------------------------------------------------
 * Acknowledge specified interrupt (PPI and SPIs)
 *
 *	x19 - contains interrupt id (relative to controller)
 *	x20 - contains syspage pointer (INTR_GENFLAG_LOAD_SYSPAGE was used)
 *	x22 - contains intr mask count (INTR_GENFLAG_LOAD_INTRMASK was used)
 *  X24 - read of IAR from interrupt_id_gic_v2_ppi
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_eoi_gic_v2, 0, gicc_patch)
	mov		x6, #1234				// vaddr for GICC (patched)
	movk	x6, #1234, lsl #16		// ...
	movk	x6, #1234, lsl #32		// ...
	movk	x6, #1234, lsl #48		// ...

	/*
	 * Skip ID0 because we EOI in the id callout
	 */
	cbz		w24, 0f

	/*
	 * Send EOI
	 */
	str		w24, [x6, #ARM_GICC_EOIR]
0:
CALLOUT_END(interrupt_eoi_gic_v2)


/*
 * -----------------------------------------------------------------------
 * Mask specified PPI interrupt
 *
 *	x0 - _syspage_ptr
 *	x1 - vector number (relative to controller)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v2_ppi, 0, gicd_patch)
	mov		x5, #1234				// vaddr for GICD (patched)
	movk	x5, #1234, lsl #16		// ...
	movk	x5, #1234, lsl #32		// ...
	movk	x5, #1234, lsl #48		// ...

	/*
	 * Mask interrupt
	 */
	add		x5, x5, #ARM_GICD_ICENABLERn
	and		w0, w1, #0x1f
	mov		w2, #1
	lsl		w2, w2, w0						// bit to set = 1 << (id % 32)
	lsr		w0, w1, #5						// index = id / 32
	str		w2, [x5, x0, lsl #2]			// ICENABLERn[index] = bit

	mov		x0, #0
	ret
CALLOUT_END(interrupt_mask_gic_v2_ppi)


/*
 * -----------------------------------------------------------------------
 * Unmask specified PPI interrupt
 *
 *	x0 - _syspage_ptr
 *	x1 - vector number (relative to controller)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v2_ppi, 0, gicd_patch)
	mov		x5, #1234				// vaddr for GICD (patched)
	movk	x5, #1234, lsl #16		// ...
	movk	x5, #1234, lsl #32		// ...
	movk	x5, #1234, lsl #48		// ...

	/*
	 * Unmask interrupt
	 */
	add		x5, x5, #ARM_GICD_ISENABLERn
	and		w0, w1, #0x1f
	mov		w2, #1
	lsl		w2, w2, w0						// bit to set = 1 << (id % 32)
	lsr		w0, w1, #5						// index = id / 32
	str		w2, [x5, x0, lsl #2]			// ISENABLERn[index] = bit

	mov		x0, #0
	ret
CALLOUT_END(interrupt_unmask_gic_v2_ppi)

/*
 * -----------------------------------------------------------------------
 * Mask specified SPI interrupt
 *
 *	x0 - _syspage_ptr
 *	x1 - vector number (relative to controller)
 *
 * Returns 0.
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_mask_gic_v2_spi, 0, gicd_patch)
	mov		x5, #1234				// vaddr for GICD (patched)
	movk	x5, #1234, lsl #16		// ...
	movk	x5, #1234, lsl #32		// ...
	movk	x5, #1234, lsl #48		// ...

	/*
	 * Mask interrupt
	 */
	add		x5, x5, #ARM_GICD_ICENABLERn
	add		w1, w1, #32						// vector passed is relative to SPI base,
											// but write in ICENABLERn is relative to 0
	and		w0, w1, #0x1f
	mov		w2, #1
	lsl		w2, w2, w0						// bit to set = 1 << (id % 32)
	lsr		w0, w1, #5						// index = id / 32
	str		w2, [x5, x0, lsl #2]			// ICENABLERn[index] = bit

	mov		x0, #0
	ret
CALLOUT_END(interrupt_mask_gic_v2_spi)


/*
 * -----------------------------------------------------------------------
 * Unmask specified SPI interrupt
 *
 *	x0 - _syspage_ptr
 *	x1 - vector number (relative to controller)
 *
 * Returns 0.
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_unmask_gic_v2_spi, 0, gicd_patch)
	mov		x5, #1234				// vaddr for GICD (patched)
	movk	x5, #1234, lsl #16		// ...
	movk	x5, #1234, lsl #32		// ...
	movk	x5, #1234, lsl #48		// ...

	/*
	 * Unmask interrupt
	 */
	add		x5, x5, #ARM_GICD_ISENABLERn
	add		w1, w1, #32						// vector passed is relative to SPI base,
											// but write in ISENABLERn is relative to 0
	and		w0, w1, #0x1f
	mov		w2, #1
	lsl		w2, w2, w0						// bit to set = 1 << (id % 32)
	lsr		w0, w1, #5						// index = id / 32
	str		w2, [x5, x0, lsl #2]			// ISENABLERn[index] = bit

	mov		x0, #0
	ret
CALLOUT_END(interrupt_unmask_gic_v2_spi)

/*
 * -----------------------------------------------------------------------
 * Configure interrupt flags for a specified PPI interrupt vector
 *
 * Used to flag INTID0 as the IPI vector.
 *
 *	x0 - syspage pointer
 *	x1 - intrinfo_entry pointer
 *	x2 - vector number (relative to controller)
 * -----------------------------------------------------------------------
 */
CALLOUT_START(interrupt_config_gic_v2_ppi, 0, 0)
	/*
	 * Use ID0 for IPI
	 */
	cmp		x2, #0
	mov		w0, #INTR_CONFIG_FLAG_IPI
	csel	w0, w0, wzr, eq
	ret
CALLOUT_END(interrupt_config_gic_v2_ppi)

/*
 * $QNXLicenseC:
 * Copyright 2018, QNX Software Systems.
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


#include <aarch64/callout.ah>


#define QorIQ_LSx_MSIR1 0x10
#define QorIQ_LSx_MSIR2 0x14
#define QorIQ_LSx_MSIR3 0x18
#define QorIQ_LSx_MSIR4 0x1c


/*
 * These callouts handle the QorIQ LSxxxx MSI interrupts
 *
 * This version (v2) of the QorIQ callouts support the Big Endian SCFG register
 * set which defines 5, 32 bit MSI registers per controller. Specifically, the 32
 * MSI interrupts per controller are identified within a single 32 bit MSIIR register.
 * There are 4 32 bit MSIR registers indicates which of the up to 8 interrupt sources
 * sharing a message register have pending interrupts.
 * This currently includes the revision 1.1 LS1043 SoC's.
 *
 * The MSI's for the LS1043A PCIe controllers are implemented within
 * the Supplementary Configuration Unit (SCFG). This unit is big endian and so
 * the register contents need to be swapped.
 * Additionally, there are 4 registers to read, QorIQ_LSx_MSIR[1-4] and these
 * registers are clear-on-read so we need to keep a copy of them and have the kernel
 * repeatedly call the ID routine until there are no more bits set in these
 * registers. This requires the intrinfo flags to contain INTR_GENFLAG_ID_LOOP
*/
/*
 * -----------------------------------------------------------------------
 * Patch interrupt callouts to access rw data.
 * The first call will also map the uart.
 *
 * Patcher routine takes the following arguments:
 *
 *	x0 - syspage paddr
 *	x1 - vaddr of callout
 *	x2 - offset from start of syspage to start of callout routine
 *	x3 - offset from start of syspage to rw storage
 *	x4 - patch data
 *	x5 - callout_rtn
 * -----------------------------------------------------------------------
 */
.text
.align 4

patch_intr:
	sub		sp, sp, #32
	stp		x19, x30, [sp]
	stp		x17, x18, [sp, #16]

	add		x19, x0, x2			/* x19 = address of callout routine */

	/*
	 * Calculate controller index. The first call establishes the base address
	 * of controller 0 and all other controllers are offset by 0x1000 from that
	 */
	adr		x6, Lpaddr_first	/* x6 = &Laddr_first */
	ldr		x1, [x6]			/* x1 = Lpaddr_first */
	ldr		x17, [x4]			/* x17 = patch data */
	cbnz	x1, 1f
	str		x17, [x6]			/* Lpaddr_first = patch data */
	mov		x1, x17				/* Lpaddr_first was unset so initialize x1 for calc */
1:
	subs	x17, x17, x1		/* controller page offset (x17 - x1) */
	add		x18, x3, x17, lsr #(12 - 2)	/* x18 = &MSIR[controller #] cache as an offset */

	/*
	 * Check if we have already mapped this controllers registers
	 */
	adr		x6, Lvaddr			/* x6 = &Lvaddr[0] */
	add		x6, x6, x17, lsr #(12 - 3)	/* x6 = &Lvaddr[controller #] */
	ldr		x0, [x6]			/* x0 = Lvaddr[controller #] */
	cbnz	x0, 0f

	/*
	 * Map the QorIQ_LSx SCFG Base address. Fortunately the MSIIR and MSIR
	 * registers are page aligned so our mapping will get us an entire page
	 * (4K) even though we are only specifying the size of the registers
	 */
	mov		w0, #8			/* 8 bytes for MSIIR/MSIR */
	ldr		x1, [x4]
	bl		callout_io_map
	/* x0 = mmap()'d address of controller registers */
	adr		x6, Lvaddr
	ldr		x1, [x6]					/* x1 = Lvaddr[0] */
	add		x6, x6, x17, lsr #(12 - 3)	/* x6 = &Lvaddr[controller #] */
	str		x0, [x6]
0:
	/*
	 * Patch the callout routine with the address of the MSIIR/MSIR registers
	 * - patch argument in x0
	 * - location to patch specified as first parameter
	 * - remaining 2 parameters are scratch registers
	 */
	CALLOUT_PATCH	x19, w6, w7

	/*
	 * Patch callout with RW offset in w18
	 * Address in x19 is the next patch location after CALLOUT_PATCH which is
	 * what we want
	 */
	CALLOUT_PATCH_RW	x19, w18, w6, w7

	ldp		x19, x30, [sp]
	ldp		x17, x18, [sp, #16]
	add		sp, sp, #32
	ret

/*
 * -----------------------------------------------------------------------
 * Identify interrupt source
 *
 * On entry
 * - syspage pointer in x20
 *
 * On exit
 * - interrupt id returned in x19
 * -----------------------------------------------------------------------
 */

CALLOUT_START(interrupt_id_qoriq_lsx_msi_v2, Lmsir_size, patch_intr)

	/* Get the MSIIR/MSIR base address (patched) */
	mov		x7, #0x0000
	movk	x7, #0x0000, lsl #16
	movk	x7, #0x0000, lsl #32
	movk	x7, #0x0000, lsl #48

	/* get the location of cached MSIR[controller #] (patched) */
	mov		w6, 0x0000				// offset to rw data (patched)
	add		x6, x6, x20				// address of rw data

	mov		x19, #-1

	/*
	 * Read MSIR[1-4] registers. Each of them has up to 8 interrupt sources.
	 * MSIR[1]: only bit 0-7 defined
	 * MSIR[2]: only bit 8-15 defined
	 * MSIR[3]: only bit 16-23 defined
	 * MSIR[4]: only bit 24-31 defined
	 * We do logic OR on the 4 registers values to combine them to be one
	 * 32-bit value. Then convert it to LE. Reading clears the register
	 * and deasserts the MSI interrupt line so if the current value is not zero,
	 * process it and cache the remaining unprocessed bits
	*/
	ldr		w0, [x7, #QorIQ_LSx_MSIR1]
	ldr		w1, [x7, #QorIQ_LSx_MSIR2]
	orr		w0, w0, w1
	ldr		w1, [x7, #QorIQ_LSx_MSIR3]
	orr		w0, w0, w1
	ldr		w1, [x7, #QorIQ_LSx_MSIR4]
	orr		w0, w0, w1

	rev		w0, w0
	/* OR the previous MSIR value with the current value */
	ldr		w1, [x6]
	orr		w1, w1, w0
	/* if no more bits set we're done */
	cbz		w1, 0f

	/*
	 * calculate the interrupt number to be returned. On this platform
	 * MSI vector 0 sets bit 31, vector 1 sets bit 30, etc and vector 31
	 * sets bit 0
	*/
	mov		x19, #0
	clz		w19, w1

	/* clear bit (31 - vector #) in w1 and save the new value */
	mov		w2, #1
	lsl		w2, w2, w19
	rbit	w2, w2
	bic		w1, w1, w2
	str		w1, [x6]
0:

CALLOUT_END(interrupt_id_qoriq_lsx_msi_v2)

/*
 * -----------------------------------------------------------------------
 * Acknowledge specified interrupt
 *
 * On entry
 * - x19 - contains interrupt id
 * - x20 - contains syspage pointer (INTR_GENFLAG_LOAD_SYSPAGE was used)
 * - x22 - contains the interrupt mask count (INTR_GENFLAG_LOAD_INTRMASK was used)
 * -----------------------------------------------------------------------
 */
#CALLOUT_START(interrupt_eoi_qoriq_lsx_msi_v2, Lmsir_size, patch_intr)
CALLOUT_START(interrupt_eoi_qoriq_lsx_msi_v2, 0, 0)

CALLOUT_END(interrupt_eoi_qoriq_lsx_msi_v2)

/*
 * -----------------------------------------------------------------------
 * Mask specified interrupt
 *
 * On entry:
 * - x0 - _syspage_ptr
 * - x1 - vector number (relative to controller)
 *
 * Returns:
 * - x0 - error status
 * -----------------------------------------------------------------------
 */
#CALLOUT_START(interrupt_mask_qoriq_lsx_msi_v2, Lmsir_size, patch_intr)
CALLOUT_START(interrupt_mask_qoriq_lsx_msi_v2, 0, 0)

	mov		x0, #0
	ret

CALLOUT_END(interrupt_mask_qoriq_lsx_msi_v2)

/*
 * -----------------------------------------------------------------------
 * Unmask specified interrupt
 *
 * On entry:
 * - x0 - _syspage_ptr
 * - x1 - vector number (relative to controller)
 *
 * Returns:
 * - x0 - error status
 * -----------------------------------------------------------------------
 */
#CALLOUT_START(interrupt_unmask_qoriq_lsx_msi_v2, Lmsir_size, patch_intr)
CALLOUT_START(interrupt_unmask_qoriq_lsx_msi_v2, 0, 0)

	mov		x0, #0
	ret

CALLOUT_END(interrupt_unmask_qoriq_lsx_msi_v2)

.data
.align 3
Lvaddr:	.word	0,0,0,0,0,0,0,0	/* 32 bytes of storage for a uintptr_t for 4 controllers */
Lpaddr_first: .word		0, 0	/* 8 bytes of storage for paddr_t of first patch_data */
Lmsir_size: .word	16			/* 16 bytes of storage for the MSIR for 4 controllers */

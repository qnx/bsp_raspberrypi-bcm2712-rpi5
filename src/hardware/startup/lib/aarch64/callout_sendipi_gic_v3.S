#
# Copyright 2014, QNX Software Systems.
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
#
# $QNXLicenseA$
#

#include <aarch64/callout.ah>
#include "aarch64/gic_v3.h"


/*
 * On entry:
 * x0 - _syspage_ptr
 * w1 - cpu target
 * w2 - cmd
 * x3 - &ipicmds[cpu]
 */
CALLOUT_START(sendipi_gic_v3_sr, 0, 0)
    /*
     * Disable interrupts (I bit) - keep old mask in x0 so we can restore on return.
     */
    mrs         x0, daif
    msr         daifset, #2

    /*
     * Set ipicmd[cpu] - keep old value in w7 for check below
     */
1:
    ldxr        w7, [x3]            // w7 = &ipicmds[cpu]
    orr         w6, w7, w2          // w6 = &ipicmds[cpu] | cmd
    stxr        w4, w6, [x3]
    cbnz        w4, 1b
    dsb         sy

    /*
     * Trigger ID0 to target CPU if old ipicmd[cpu] was 0
     */
    cbnz        w7, 2f
    msr         ARM_GICC_SGI1R_EL1, x1
    isb

2:
    /*
     * Restore interrupt state
     */
    msr         daif, x0
    ret
CALLOUT_END(sendipi_gic_v3_sr)



/*
 * Patch sendipi callout to access rw data.
 *
 * Patcher routine takes the following arguments:
 *	x0 - syspage paddr
 *	x1 - vaddr of callout
 *	x2 - offset from start of syspage to start of callout routine
 *	x3 - offset from start of syspage to rw storage
 *	x4 - patch data
 *	x5 - callout_rtn
 */

gicr_patch:
    add     x2, x0, x2                  // x2 = address of callout routine
	ldr		x0, gic_gicr_vaddr

    CALLOUT_PATCH   x2, w4, w5

    ret
/*
 * On entry:
 * x0 - _syspage_ptr
 * w1 - cpu target
 * w2 - cmd
 * x3 - &ipicmds[cpu]
 */
CALLOUT_START(sendipi_gic_v3_mm, 0, gicr_patch)
	mov		x5, #1234				// vaddr for GICR (patched)
	movk	x5, #1234, lsl #16		// ...
	movk	x5, #1234, lsl #32		// ...
	movk	x5, #1234, lsl #48		// ...
    /*
     * Disable interrupts (I bit) - keep old mask in x0 so we can restore on return.
     */
    mrs         x0, daif
    msr         daifset, #2

    /*
     * Set ipicmd[cpu] - keep old value in w7 for check below
     */
1:
    ldxr        w7, [x3]            // w7 = &ipicmds[cpu]
    orr         w6, w7, w2          // w6 = &ipicmds[cpu] | cmd
    stxr        w4, w6, [x3]
    cbnz        w4, 1b
    dsb         sy

    /*
     * Trigger ID0 to target CPU if old ipicmd[cpu] was 0
     */
    cbnz        w7, 2f
	mov			w2,1
	// X1 is the offset to the appropriate ARM_GICR_ISPENDING register
    str         w2,[x5,x1]

2:
    /*
     * Restore interrupt state
     */
    msr         daif, x0
    ret
CALLOUT_END(sendipi_gic_v3_mm)

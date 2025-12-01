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
 * Stub exception vector table.
 */

#include "asmoff.def"

	.text

.macro ventry
	.align	7
0:	b	0b
.endm

	.align	11

	/*
	 * A default exception vector table that loops on exception entry.
	 * This allows us to inspect the elr_el1 and esr_el1 registers from a
	 * debugger to see what exception occurred.
	 */
	.global	vbar_default
vbar_default:
	ventry		// 0x000
	ventry		// 0x080
	ventry		// 0x100
	ventry		// 0x180
	ventry		// 0x200
	ventry		// 0x280
	ventry		// 0x300
	ventry		// 0x380
	ventry		// 0x400
	ventry		// 0x480
	ventry		// 0x500
	ventry		// 0x580
	ventry		// 0x600
	ventry		// 0x680
	ventry		// 0x700
	ventry		// 0x780

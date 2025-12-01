#
# Copyright 2017, QNX Software Systems.
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
//
// Wait for spin_shim_id to match my MPIDR_EL1 register and then
// jump to the address in spin_shim_start
//
	.global spin_shim_rtn

spin_shim_rtn:
	mrs		x1, mpidr_el1
	ldr		x2, =0xFF00FFFFFF
	and		x1, x1, x2

	sevl
1:
	wfe
	ldr		x2, spin_shim_id
	cmp		x2, x1
	bne		1b
	ldr		x7,spin_shim_start
	br		x7

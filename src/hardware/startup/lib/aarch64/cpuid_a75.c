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

#include <startup.h>

static void
a75_setup(void)
{
	/*
	 * Handle A75 Errata fixes
	 */
#ifdef AUTO_ENABLE_MELTDOWN
	if(meltdown_active < 0) {
		meltdown_active = 1;
	}
#endif
}

struct aarch64_cpuid	cpuid_a75 = {
	.midr = 0x4100d0a0,
	.name = "Cortex-A75",
	.setup = a75_setup,
};

/*
 * $QNXLicenseC:
 * Copyright 2014, QNX Software Systems.
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

int		pauth_active;
int		meltdown_active = -1; // indicate not set yet.
int		spectre_v2_active = -1; // Spectre V2 mitigation is off by default.
int		ssbs_active = -1; // Speculative Store Bypassing Safe is off by default.
int		cache_snooped = 0; // Cache not snooped by default.
uint32_t	sve_requested = 0; // SVE support is off by default.

size_t __page_size = 0x1000;


#define	CPUID_MASK			0xff00fff0u

struct aarch64_cpuid *
aarch64_cpuid_find(void) {
	unsigned const cpuid = aa64_sr_rd32(midr_el1);

	struct aarch64_cpuid *id;
	struct aarch64_cpuid	**idp = aarch64_cpuid;
	for(;;) {
		id = *idp;
		if(id == NULL) break;
		if(id->midr == (cpuid & CPUID_MASK)) break;
		++idp;
	}
	return id;
}


/*
 * Perform CPU specific startup initialization.
 * This code is hardware independant and should not have to be changed
 * changed by end users.
 */
void
cpu_startup()
{
	/*
	 * Perform any errata fixes 
	 */
	struct aarch64_cpuid *const id = aarch64_cpuid_find();
	if((id != NULL) && (id->setup != NULL)) {
		id->setup();
	}
}

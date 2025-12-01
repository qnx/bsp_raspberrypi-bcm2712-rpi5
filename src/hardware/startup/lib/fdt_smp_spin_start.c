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

#include <startup.h>
#include <libfdt.h>

#if defined(__aarch64__)
	#define CACHE_FLUSH(addr) asm volatile("dc cvac,%0" :: "r"(addr))
	#define SIGNAL() asm volatile("sev")
#elif defined(__ARM__)
	#define CACHE_FLUSH(addr) asm volatile("mcr p15,0,%0,c7,c6,2" :: "r"(addr))
	#define SIGNAL() asm volatile("sev")
#elif defined(__X86__) || defined(__X86_64__)
	// nothing to do
	#define CACHE_FLUSH(addr)
	#define SIGNAL()
#else
	#error Unknown architecture
#endif


// Some firmware use the same spin-release-addr attribute value for
// multiple CPU's and release them all in one go. We only want
// one at a time released, so transfer them to a small piece of shim
// code that spins until spin_shim_id matches their CPU ID (MPIDR on ARM,
// LAPICID on X86). At that point, that particular CPU jumps to the
// real start address (contained in spin_shim_start).

void (*spin_shim_start)(void);
unsigned long spin_shim_id;

int
fdt_smp_spin_start(unsigned cpu, void (*const start)(void)) {
	fdt32_t *valp = NULL;

	if(fdt == NULL) {
		return 0;
	}
	spin_shim_start = start;
	CACHE_FLUSH(&spin_shim_start);
	int node = -1;
	for( ;; ) {
		node = fdt_node_offset_by_prop_value(fdt, node, "device_type", "cpu", sizeof "cpu");
		if(node < 0) {
			return 0;
		}
		if(cpu == 0) break;
		--cpu;
	}
	int plen;
	struct fdt_property *prop;
	prop = (struct fdt_property *)fdt_get_property(fdt, node,"cpu-release-addr", &plen);
	if(prop == NULL) {
		return 0;
	}
	valp = (fdt32_t *)prop->data;
	uint64_t spin_addr;
	switch(plen) {
	case 4:
		spin_addr = fdt32_to_cpu(valp[0]);
		break;
	case 8:
		spin_addr = ((uint64_t)fdt32_to_cpu(valp[0]) << 32) | fdt32_to_cpu(valp[1]);
		break;
	default:
		return 0;
	}

	*(uintptr_t volatile *)(uintptr_t)spin_addr = (uintptr_t)spin_shim_rtn;
	CACHE_FLUSH(spin_addr);
	__cpu_membarrier();
	SIGNAL();

	prop =(struct fdt_property *)fdt_get_property(fdt, node, "reg",	&plen);
	if(prop == NULL) {
		return 0;
	}

	unsigned long id;
	valp = (fdt32_t *)prop->data;
	switch(plen) {
	case 4:
		id = fdt32_to_cpu(valp[0]);
		break;
	case 8:
		id = (unsigned long)(((uint64_t)fdt32_to_cpu(valp[0]) << 32) | fdt32_to_cpu(valp[1]));
		break;
	default:
		crash("Unsupported %d bytes for reg node.\n", plen);
	}
	spin_shim_id = id;
	CACHE_FLUSH(&spin_shim_id);
	__cpu_membarrier();
	SIGNAL();
	return 1;
}

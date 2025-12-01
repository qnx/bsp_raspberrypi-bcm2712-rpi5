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

void init_mitigation_mem(void)
{
	const struct cpuinfo_entry	*const cpu = &lsp.cpuinfo.p[0];

	if (cpu->flags & AARCH64_CPU_SPECTRE_V2_MMU) {
		paddr_t addr = find_top_ram_aligned_limit(lsp.system_private.p->pagesize, lsp.system_private.p->pagesize,
											lsp.system_private.p->pagesize, 1ul<<39);
		if(addr == NULL_PADDR) {
			crash("Could not reserve 0x%x bytes of memory for spectre2 mmu mitigation.\n",lsp.system_private.p->pagesize);
		}

		alloc_ram(addr, lsp.system_private.p->pagesize, lsp.system_private.p->pagesize);

		as_add_containing(addr, (addr + lsp.system_private.p->pagesize - 1), AS_ATTR_RAM, "sp2mmu", "ram");
	}
}

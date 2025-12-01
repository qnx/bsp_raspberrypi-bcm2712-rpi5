/*
 * $QNXLicenseC:
 * Copyright 2015, QNX Software Systems.
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


void
gicv_asinfo(paddr_t gicv, paddr_t banked_gich, paddr_t direct_gich,
		unsigned direct_num, unsigned direct_spacing) {

	unsigned const off = as_find(AS_NULL_OFF, "memory", NULL);
	if(gicv != NULL_PADDR) {
		as_add64(gicv, gicv + 0xfffu, AS_ATTR_DEV, "gicv", off);
	}
	if(banked_gich != NULL_PADDR) {
		as_add64(banked_gich, banked_gich + 0xfffu, AS_ATTR_DEV, "gich", off);
	}
	if(direct_gich != NULL_PADDR) {
		char name[10];
		ksprintf(name, "gich.%u", direct_num);
		as_add64(direct_gich, direct_gich + direct_spacing-1, AS_ATTR_DEV, name, off);
	}
}

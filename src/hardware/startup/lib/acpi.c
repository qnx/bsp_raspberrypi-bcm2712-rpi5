/*
 * Copyright 2008,2022,2025, BlackBerry Limited.
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
#include <hw/acpi.h>

static int     num_tbls = -1;
static paddr_t *tbl_array = NULL;

paddr_t
acpi_find_table_next(unsigned table_name, unsigned *lenp, paddr_t start_search) {

	paddr_t				tbl_paddr;
	acpi_rsdp			*root;
	acpi_generic		*tbl;
	unsigned			i, num_xsdt_tbls = 0, tbl_index = 0;
	unsigned			sig;

	if(num_tbls == 0) {
		return NULL_PADDR;
	}
	if(tbl_array == NULL) {
		root = board_find_acpi_rsdp();
		if(root == NULL) {
			num_tbls = 0;
			return NULL_PADDR;
		}
		if((root->v1.Revision == ACPI_RSDP_REVISION_1_0) || (root->XsdtAddress == 0)) {
			if((root->v1.Revision != ACPI_RSDP_REVISION_1_0) && (debug_flag > 0)) {
				kprintf("ACPI rev %d, XsdtAddress not set, use RsdtAddress\n", root->v1.Revision);
			}
			tbl_paddr = root->v1.RsdtAddress;
			startup_memory_unmap(root);
			tbl = startup_memory_map(0x4096, tbl_paddr, PROT_READ);
			num_tbls = (tbl->hdr.Length - offsetof(acpi_rsdt, Entry)) / sizeof(tbl->rsdt.Entry[0]);
			tbl_array = ws_alloc(num_tbls * sizeof(*tbl_array));
			if(tbl_array == NULL) {
				return NULL_PADDR;
			}
			for(i = 0; i < num_tbls; ++i) {
				tbl_array[i] = tbl->rsdt.Entry[i];
			}
		} else {
			tbl_paddr = root->XsdtAddress;
			startup_memory_unmap(root);
			tbl = startup_memory_map(0x4096, tbl_paddr, PROT_READ);
			num_xsdt_tbls = (tbl->hdr.Length - offsetof(acpi_xsdt, Entry)) / sizeof(tbl->xsdt.Entry[0]);
			/* Alloc one extra entry for the DSDT table which linked to Fixed ACPI Description Table (FADT) */
			num_tbls = num_xsdt_tbls + 1;
			tbl_array = ws_alloc(num_tbls * sizeof(*tbl_array));
			if(tbl_array == NULL) {
				return NULL_PADDR;
			}
			for(i = 0; i < num_tbls; ++i) {
				tbl_array[i] = NULL_PADDR;
			}
			tbl_index = 0;
			for(i = 0; i < num_xsdt_tbls; ++i) {
				tbl_array[tbl_index++] = tbl->xsdt.Entry[i];
				acpi_generic *generic_tbl;
				generic_tbl = startup_memory_map(0x4096, tbl_array[i], PROT_READ);
				if (generic_tbl->hdr.Signature == ACPI_FADT_SIGNATURE) {
					/*
					 * DSDT table is linked to Fixed ACPI Description Table (FADT)
					 */
					if ((paddr_t)generic_tbl->fadt.X_DSDT != NULL_PADDR) {

						tbl_array[tbl_index++] = (paddr_t)generic_tbl->fadt.X_DSDT;
					}
					/*
					 * FACS table is linked to Fixed ACPI Description Table (FADT)
					 * -- not supported yet
					 */
				}
				startup_memory_unmap(generic_tbl);
			}
			if (tbl_index < num_tbls) {
				/* in case extra allocated table not found */
				num_tbls = tbl_index;
			}
		}
		startup_memory_unmap(tbl);
	}

	int searching = (start_search == NULL_PADDR);

	for(i = 0; i < num_tbls; ++i) {
		tbl_paddr = tbl_array[i];
		if(searching) {
			tbl = startup_memory_map(sizeof(*tbl), tbl_paddr, PROT_READ);
			sig = tbl->hdr.Signature;
			if(lenp != NULL) {
				*lenp = tbl->hdr.Length;
			}
			startup_memory_unmap(tbl);
			if(sig == table_name) {
				return tbl_paddr;
			}
		} else if(start_search == tbl_paddr) {
			searching = 1;
		}
	}

	return NULL_PADDR;
}

void add_acpi_table(void)
{
	unsigned     i, sig, len;
	paddr_t      tbl_paddr;
	acpi_generic *tbl;
	char         name[5];

	if(num_tbls == 0) {
		return ;
	}
	for(i = 0; i < num_tbls; ++i) {
		tbl_paddr = tbl_array[i];
		tbl = startup_memory_map(sizeof(*tbl), tbl_paddr, PROT_READ);
		sig = tbl->hdr.Signature;
		len = tbl->hdr.Length;
		memcpy(name, &tbl->hdr.Signature, 4);
		name[4] = 0;
		startup_memory_unmap(tbl);

		if (debug_flag > 2) {
			kprintf("ACPI table: %s sig=%x len=%x at %L\n", name, sig, len, tbl_paddr);
		}
		as_add((uintptr_t)tbl_paddr, (uintptr_t)tbl_paddr+len-1, AS_ATTR_RAM,
				name, as_find(AS_NULL_OFF, "memory", NULL));
	}
}

paddr_t
acpi_find_table(unsigned table_name, unsigned *lenp) {
	return acpi_find_table_next(table_name, lenp, NULL_PADDR);
}

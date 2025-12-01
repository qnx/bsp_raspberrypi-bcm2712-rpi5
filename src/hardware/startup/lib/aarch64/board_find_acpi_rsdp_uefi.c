/*
 * Copyright 2022, BlackBerry Limited.
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
#include <hw/uefi.h>

static const EFI_GUID acpi_guids[] = {EFI_ACPI_TABLE_GUID, ACPI_10_TABLE_GUID};

void *
board_find_acpi_rsdp_uefi(void) {
	acpi_rsdp	*root;
	unsigned	i;

	for(i = 0; i < NUM_ELTS(acpi_guids); ++i) {
		root = uefi_find_config_tbl(&acpi_guids[i]);
		if(root != NULL) {
			static int recorded_table = 0;
			if(!recorded_table) {
				recorded_table = 1;
				unsigned len = sizeof(root->v1);
				if(root->v1.Revision > ACPI_RSDP_REVISION_1_0) {
					len = root->Length;
				}
				as_add((uintptr_t)root, (uintptr_t)root+len-1, AS_ATTR_RAM,
					"acpi_rsdp", as_find(AS_NULL_OFF, "memory", NULL));
			}
			return root;
		}
	}
	return NULL;
}

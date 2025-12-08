/*
 * Notice on Software Maturity and Quality
 *
 * The software included in this repository is classified under our Software Maturity Standard as Experimental Software - Software Quality and Maturity Level (SQML) 1.
 *
 * As defined in the QNX Development License Agreement, Experimental Software represents early-stage deliverables intended for evaluation or proof-of-concept purposes.
 *
 * SQML 1 indicates that this software is provided without one or more of the following:
 *     - Formal requirements
 *     - Formal design or architecture
 *     - Formal testing
 *     - Formal support
 *     - Formal documentation
 *     - Certifications of any type
 *     - End-of-Life or End-of-Support policy
 *
 * Additionally, this software is not monitored or scanned under our Cybersecurity Management Standard.
 *
 * No warranties, guarantees, or claims are offered at this SQML level.
 */

/*
 * $QNXLicenseC:
 * Copyright 2008, 2020, 2024, BlackBerry Limited.
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
#include <hw/uefi.h>

#define UEFI_EXIT_RETRY_MAX		2

/*
 * EFI Image Handle (QNX startup image)
 */
EFI_HANDLE efi_image_handle = NULL;

/*
 * EFI Services Pointers
 */
EFI_SYSTEM_TABLE *efi_system_table = NULL;
EFI_BOOT_SERVICES *efi_boot_services = NULL;
EFI_RUNTIME_SERVICES *efi_runtime_services = NULL;


void *
uefi_find_config_tbl(const EFI_GUID *const uid) {
	EFI_CONFIGURATION_TABLE	*tbl;
	unsigned				i;

	tbl = efi_system_table->ConfigurationTable;
	for(i = 0; i < efi_system_table->NumberOfTableEntries; ++i) {
		if(memcmp(&tbl->VendorGuid, uid, sizeof(*uid)) == 0) {
			return tbl->VendorTable;
		}
		++tbl;
	}
	return NULL;
}

static unsigned uefi_init_cnt = 0;
extern void (*uefi_exit_boot_services_f)(void);

static void _uefi_exit_boot_services(void)
{
	ASSERT(uefi_init_cnt > 0);

	if (--uefi_init_cnt == 0)
	{
		const EFI_BOOT_SERVICES	*const bs = efi_system_table->BootServices;
		_Uintn map_key;
		_Uintn desc_size;
		uint32_t desc_version;
		_Uintn map_size = 0;
		EFI_MEMORY_DESCRIPTOR *desc = NULL;
		EFI_STATUS status = 0;
		unsigned uefi_exit_retry_cnt = 0;

		if (debug_flag > 0) kprintf("%s, exiting\n", __FUNCTION__);
		uefi_io_flush();

		do {
			/* Get the UEFI memory map size */
			status = bs->GetMemoryMap(&map_size, desc, &map_key, &desc_size, &desc_version);
			if (status != EFI_BUFFER_TOO_SMALL) {
				crash("Failed to query the UEFI memory map with error %l\n", status);
			}

			do {
				/* Allocating the memory creates at least one new descriptor */
				map_size += desc_size;
				status = bs->AllocatePool(EfiLoaderData, map_size, (void **)&desc);
				if ((desc == NULL) || (status != EFI_SUCCESS)) {
					crash("Fatal error while allocating the UEFI memory map buffer\n");
				}

				/* Get the UEFI memory map */
				status = bs->GetMemoryMap(&map_size, desc, &map_key, &desc_size, &desc_version);
				if (status != EFI_SUCCESS) {
					bs->FreePool(desc);
					desc = NULL;
				}
			} while (status == EFI_BUFFER_TOO_SMALL);

			if (status != EFI_SUCCESS) {
				crash("Failed to get the UEFI memory map with error %l\n", status);
			}

			status = bs->ExitBootServices(efi_image_handle, map_key);

			if ((status == EFI_INVALID_PARAMETER) && (uefi_exit_retry_cnt <= UEFI_EXIT_RETRY_MAX)) {
				uefi_exit_retry_cnt++;
				kprintf("retry uefi exit: %u\n", uefi_exit_retry_cnt);
				bs->FreePool(desc);
				desc = NULL;
				map_size=0;
				continue;
			}
			else if (status != EFI_SUCCESS) {
				kprintf("exit status: %u\n", status);
				crash("Could not exit boot services\n");
			}
			else {							/* EFI_SUCCESS */
				break;
			}
		} while (uefi_exit_retry_cnt <= UEFI_EXIT_RETRY_MAX);

		efi_system_table->ConOut = NULL;	/* no more output */
		efi_system_table->ConIn = NULL;		/* no more input */
		efi_boot_services = NULL;			/* no more EFI_BOOT_SERVICES */
		uefi_exit_boot_services_f = NULL;
	}
	else if (debug_flag > 0) kprintf("%s, deferred, cnt = %u\n", __FUNCTION__, uefi_init_cnt);

	/*
	 * QNX procnto expects QNX startup to disable the interrupts.
	 * Failing to disable the interrupts may result in a crash during
	 * the early initialization of the kernel.
	 */
	InterruptDisable();
}

void uefi_exit_init(void)
{
	uefi_exit_boot_services_f = _uefi_exit_boot_services;
	++uefi_init_cnt;
}

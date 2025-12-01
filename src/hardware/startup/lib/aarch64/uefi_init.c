/*
 * Copyright (c) 2022, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <startup.h>
#include <hw/uefi.h>

/**
 * @brief Configure the global UEFI service pointers
 *
 * The EFI_IMAGE_ENTRY_POINT provides the two arguments:
 *  1. EFI_HANDLE ImageHandle
 *  2. EFI_SYSTEM_TABLE *SystemTable
 * The SystemTable is a pointer to the image's system table which contains
 * the standard console input and output protocols and the services pointers
 * such as EFI_BOOT_SERVICES and EFI_RUNTIME_SERVICES tables.
 *
 * This routine is responsible to verify the validity of the service pointers,
 * and set the global UEFI services pointers that are used in the rest of the
 * startup code.
 * A failure in this routine should assert the boot since a system without
 * a valid EFI_SYSTEM_TABLE would not be compliant with the UEFI specification,
 * and crash later in the code.
 */
void uefi_init(void)
{
    static unsigned uefi_init_done = 0;

    /* Prevent reentry */
    ASSERT(uefi_init_done == 0);
    uefi_init_done = 1;

    /* Assign the standard arguments for an UEFI application */
    efi_image_handle = (void*)boot_regs[0];
    efi_system_table = (void*)boot_regs[1];

    /* validation of  UEFI service pointers and EFI_SYSTEM_TABLE signature has been done in is_uefi_boot() */

    /* Assign the UEFI services pointers */
    efi_boot_services = efi_system_table->BootServices;
    efi_runtime_services = efi_system_table->RuntimeServices;

    /* Configure the function pointer to exit the EFI Boot Services */
    uefi_exit_init();
}

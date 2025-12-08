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


/* return true (!0) or false (0) depending on whether this was a UEFI boot */
unsigned is_uefi_boot(void)
{
    static unsigned uefi = 0x55555555;    /* a value that a boolean will never be */

    /* Assign the standard arguments for an UEFI application */
    efi_image_handle = (void*)boot_regs[0];
    efi_system_table = (void*)boot_regs[1];

    if (uefi == 0x55555555)
    {
        uefi = 0;

        do {
            /* Check validity of the UEFI service pointers */
            if (efi_system_table == NULL) {
                break;
            }
            if (efi_system_table->BootServices == NULL) {
                break;
            }
            if (efi_system_table->RuntimeServices == NULL) {
                break;
            }

            /* Verify EFI_SYSTEM_TABLE signature */
            if (efi_system_table->Hdr.Signature != EFI_SYSTEM_TABLE_SIGNATURE) {
                break;
            }

            /* Verify EFI_BOOT_SERVICES signature */
            if (efi_system_table->BootServices->Hdr.Signature != EFI_BOOT_SERVICES_SIGNATURE) {
                break;
            }

            /* Verify EFI_RUNTIME_SERVICES signature */
            if (efi_system_table->RuntimeServices->Hdr.Signature != EFI_RUNTIME_SERVICES_SIGNATURE) {
                break;
            }
            uefi = 1;

        } while (0);

        if (uefi == 0) {
            efi_image_handle = NULL;
            efi_system_table = NULL;
        }
    }

    return uefi;
}

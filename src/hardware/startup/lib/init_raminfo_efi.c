/*
 * Copyright 2015,2022, BlackBerry Limited.
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

#ifdef RAMINFO_EFI_DEBUG
#define EFI_MEMORY_UC 0x0000000000000001ul
#define EFI_MEMORY_WC 0x0000000000000002ul
#define EFI_MEMORY_WT 0x0000000000000004ul
#define EFI_MEMORY_WB 0x0000000000000008ul
#define EFI_MEMORY_UCE 0x0000000000000010ul
#define EFI_MEMORY_WP 0x0000000000001000ul
#define EFI_MEMORY_RP 0x0000000000002000ul
#define EFI_MEMORY_XP 0x0000000000004000ul
#define EFI_MEMORY_NV 0x0000000000008000ul
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000ul
#define EFI_MEMORY_RO 0x0000000000020000ul
#define EFI_MEMORY_SP 0x0000000000040000ul
#define EFI_MEMORY_CPU_CRYPTO 0x0000000000080000ul
#define EFI_MEMORY_RUNTIME 0x8000000000000000ul
#define EFI_MEMORY_ISA_VALID 0x4000000000000000ul
#define EFI_MEMORY_ISA_MASK 0x0FFFF00000000000ul
#endif

void
init_raminfo_efi(void)
{
    EFI_MEMORY_DESCRIPTOR *desc;

#if RAMINFO_EFI_DEBUG
    const char *efi_types[] = {
        [EfiReservedMemoryType] =   "Reserved     ",
        [EfiLoaderCode] =           "Loader Code  ",
        [EfiLoaderData] =           "Loader Data  ",
        [EfiBootServicesCode] =     "Boot Code    ",
        [EfiBootServicesData] =     "Boot Data    ",
        [EfiRuntimeServicesCode] =  "Runtime Code ",
        [EfiRuntimeServicesData] =  "Runtime Data ",
        [EfiConventionalMemory] =   "Conventional ",
        [EfiUnusableMemory] =       "Unusable     ",
        [EfiACPIReclaimMemory] =    "ACPIReclaim  ",
        [EfiACPIMemoryNVS] =        "ACPIMemoryNVS",
        [EfiMemoryMappedIO] =       "MMIO         ",
        [EfiMemoryMappedIOPortSpace] ="Port Space   ",
    };
#endif

    for (int pass = 0; pass < 3; ++pass) {
        _Uint64t cur_start = ~0ul;
        _Uint64t cur_size = 0;

    for (desc = efi_walk_map(NULL); desc != NULL; desc = efi_walk_map(desc)) {
#if EFI_RAMINFO_DEBUG
        char attrs[] = "---------------";
        if (desc->Attribute & EFI_MEMORY_UC) {
            attrs[0] = 'U';
        }
        if (desc->Attribute & EFI_MEMORY_WC) {
            attrs[1] = 'C';
        }
        if (desc->Attribute & EFI_MEMORY_WT) {
            attrs[2] = 'T';
        }
        if (desc->Attribute & EFI_MEMORY_WB) {
            attrs[3] = 'B';
        }
        if (desc->Attribute & EFI_MEMORY_UCE) {
            attrs[4] = 'U';
        }
        if (desc->Attribute & EFI_MEMORY_WP) {
            attrs[5] = 'W';
        }
        if (desc->Attribute & EFI_MEMORY_RP) {
            attrs[6] = 'R';
        }
        if (desc->Attribute & EFI_MEMORY_XP) {
            attrs[7] = 'X';
        }
        if (desc->Attribute & EFI_MEMORY_NV) {
            attrs[8] = 'N';
        }
        if (desc->Attribute & EFI_MEMORY_MORE_RELIABLE) {
            attrs[9] = 'M';
        }
        if (desc->Attribute & EFI_MEMORY_RO) {
            attrs[10] = 'R';
        }
        if (desc->Attribute & EFI_MEMORY_SP) {
            attrs[11] = 'S';
        }
        if (desc->Attribute & EFI_MEMORY_CPU_CRYPTO) {
            attrs[12] = 'C';
        }
        if (desc->Attribute & EFI_MEMORY_RUNTIME) {
            attrs[13] = 'R';
        }
        if (desc->Attribute & EFI_MEMORY_ISA_VALID) {
            attrs[14] = 'I';
        }
        const char *type_str = "???";
        if (desc->Type < EfiMaxMemoryType) {
            type_str = efi_types[desc->Type];
        }
        kprintf("Type=%s PS=%l NOP=%l Attr=%s\r\n", type_str, desc->PhysicalStart, (_Uint64t)desc, attrs);
#endif

        _Uint64t const sz = 0x1000ul * desc->NumberOfPages;
        switch (desc->Type) {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiACPIReclaimMemory:
            if (pass == 0) {
                avoid_ram(desc->PhysicalStart, sz);
            }
            if (pass == 2) {
                alloc_ram(desc->PhysicalStart, sz, 0);
            }
            // FALLTHROUGH
        case EfiConventionalMemory:
            if (pass == 1) {
                if (desc->PhysicalStart == (cur_start + cur_size)) {
                    cur_size += sz;
                } else {
                    add_ram(cur_start, cur_size);
                    cur_start = desc->PhysicalStart;
                    cur_size = sz;
                }
            }
            break;

        default:
            if (pass == 0) {
                avoid_ram(desc->PhysicalStart, sz);
            }
            break;
        }
    }
        if ((pass == 1) && (cur_size != 0)) {
            add_ram(cur_start, cur_size);
        }
    }
}

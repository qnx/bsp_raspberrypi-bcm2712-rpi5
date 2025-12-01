/*
 * Copyright (c) 2023, BlackBerry Limited.
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

// static EFI_SYSTEM_TABLE *efi_system_table; // defined in uefi.h
static _Uintn           efi_map_desc_size;
static _Uintn           efi_map_size;
static void             *efi_map;
static _Char16t         *efi_command_line;
static size_t           efi_command_line_size;

#if 0
static void
efi_print_num(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL * const ConOut, _Uint64t num, unsigned const base)
{
    _Char16t numstr[32];
    _Char16t *p = &numstr[31];
    *p-- = 0;
    *p-- = '\n';
    *p = '\r';
    do {
        --p;
        *p = u"0123456789abcdef"[num % base];
        num /= base;
    } while (num);

    ConOut->OutputString(ConOut, p);
}
#endif

EFI_STATUS
efi_entry_point(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    SystemTable->ConOut->OutputString(SystemTable->ConOut, u"Entering startup...\r\n");

    efi_system_table = SystemTable;

    const EFI_BOOT_SERVICES * const bs = SystemTable->BootServices;

    EFI_GUID LoadedImageProto = LOADED_IMAGE_PROTO_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *plip;
    EFI_STATUS status = bs->HandleProtocol(ImageHandle, &LoadedImageProto, (void **)&plip);
    if (status == EFI_SUCCESS) {
        if (plip->LoadOptionsSize > 0) {
            efi_command_line_size = plip->LoadOptionsSize;
            efi_command_line = plip->LoadOptions;
        }
    }

    _Uintn map_key;
    _Uint32t desc_version;

    efi_map_size = 0;
    status = bs->GetMemoryMap(&efi_map_size, NULL, &map_key,
        &efi_map_desc_size, &desc_version);
    if (status == EFI_BUFFER_TOO_SMALL) {
        for (;;) {
            // Allocation is all but guaranteed to add an entry to the list
            efi_map_size += efi_map_desc_size;
            status = bs->AllocatePool(EfiLoaderData, efi_map_size, &efi_map);
            if (status != EFI_SUCCESS) {
                SystemTable->ConOut->OutputString(SystemTable->ConOut, u"AllocatePool\r\n");
                break;
            }
            status = bs->GetMemoryMap(&efi_map_size, efi_map, &map_key,
                &efi_map_desc_size, &desc_version);
            if (status != EFI_BUFFER_TOO_SMALL) {
                if (status != EFI_SUCCESS) {
                    SystemTable->ConOut->OutputString(SystemTable->ConOut, u"GetMemoryMap\r\n");
                }
                break;
            }
        }

        if (status == EFI_SUCCESS) {
            status = bs->ExitBootServices(ImageHandle, map_key);
            if (status == EFI_SUCCESS) {
                void cstart();
                cstart();
                // NOTREACHED
            }
            SystemTable->ConOut->OutputString(SystemTable->ConOut, u"ExitBootServices failed\r\n");
        }
    } else {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, u"Error getting memory map\r\n");
    }
    bs->Exit(ImageHandle, EFI_LOAD_ERROR, 0, NULL);
    // NOTREACHED
    return EFI_LOAD_ERROR;
}

static EFI_MEMORY_DESCRIPTOR *
efi_map_entry(_Uintn const idx)
{
    return (void *)((_Uint8t *)efi_map + (idx * efi_map_desc_size));
}

static void
heapify(_Uintn const count)
{
    for (_Uintn end = 1; end < count; ++end) {
        _Uintn child = end;
        while (child > 0) {
            _Uintn const parent = (child-1)/2;
            EFI_MEMORY_DESCRIPTOR * const pdesc = efi_map_entry(parent);
            EFI_MEMORY_DESCRIPTOR * const cdesc = efi_map_entry(child);

            if (pdesc->PhysicalStart >= cdesc->PhysicalStart) {
                break;
            }

            EFI_MEMORY_DESCRIPTOR desc = *pdesc;
            *pdesc = *cdesc;
            *cdesc = desc;
            child = parent;
        }
    }
}

static void
efi_sort_map(void)
{
    _Uintn const ndescs = efi_map_size / efi_map_desc_size;

    heapify(ndescs);
    for (_Uintn end = ndescs - 1; end > 0; --end) {
        EFI_MEMORY_DESCRIPTOR *pend = efi_map_entry(end);
        EFI_MEMORY_DESCRIPTOR *p0 = efi_map_entry(0);
        EFI_MEMORY_DESCRIPTOR desc = *pend;
        *pend = *p0;
        *p0 = desc;
        heapify(end);
    }
}

EFI_MEMORY_DESCRIPTOR *
efi_walk_map(EFI_MEMORY_DESCRIPTOR * const previous)
{
    if (previous == NULL) {
        static unsigned sorted = 0;
        if (!sorted) {
            efi_sort_map();
            sorted = 1;
        }
        return efi_map;
    }

    _Uintptrt const next = (_Uintptrt)(void *)previous + efi_map_desc_size;
    _Uintptrt const end = (_Uintptrt)(void *)efi_map + efi_map_size;
    if (next >= end) return NULL;

    return (EFI_MEMORY_DESCRIPTOR *)(void *)next;
}

void *
efi_get_table(const EFI_GUID * const guid)
{
    for (_Uintn i = 0; i < efi_system_table->NumberOfTableEntries; ++i) {
        if (memcmp(&efi_system_table->ConfigurationTable[i].VendorGuid,
                guid, sizeof(EFI_GUID)) == 0) {
            return efi_system_table->ConfigurationTable[i].VendorTable;
        }
    }
    return NULL;
}

char *
efi_convert_command_line(void)
{
    if (efi_command_line_size == 0) return NULL;

    char *ascii_str = (void *)efi_command_line;
    size_t i;
    for (i = 0; i < efi_command_line_size; ++i) {
        // Crude conversion
        const char c = (char)efi_command_line[i];
        *ascii_str++ = c;
        if (c == 0) break;
    }
    if (i == efi_command_line_size) {
        // Make sure to terminate it
        *ascii_str = 0;
    }

    return (void *)efi_command_line;
}

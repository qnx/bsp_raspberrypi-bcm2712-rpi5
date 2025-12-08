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
 * Copyright 2022, 2024, BlackBerry Limited.
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
#include <hw/uefi_smbios.h>
#include <hw/uefi.h>

static const EFI_GUID smbios_guids[] = {SMBIOS_3_TABLE_GUID, /* 64-bit smbios data */ SMBIOS_TABLE_GUID };
static paddr_t smbios_table = NULL_PADDR;
static _Uint8t use_smbios_3 = 0;         /* 64 bit smbios */


static void dump_hex(_Uint8t *data, _Uint8t len)
{
	_Uint8t i;
    for (i=0; i<len; i++) {
        if ((i % 16) == 0) {
            kprintf("0x%x: ", i);
        }
        kprintf("%b ", data[i]);
        if ((i % 16) == 15) {
            kprintf("\n");
        }
    }
    kprintf("\n");
}

static const void *get_guid_str(EFI_GUID *guid)
{
    static char guid_str[120];

    ksprintf(guid_str, "%x %w-%w %b-%b-%b-%b-%b-%b-%b-%b",
            guid->Data1, guid->Data2, guid->Data3,
            guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
            guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
    return guid_str;
}

static void dump_smbios_table(smbios_table_t *tbl)
{
    if (debug_flag > 4) {
        kprintf("SMBIOS Entry at %L:\n", (uint64_t)tbl);
        dump_hex((_Uint8t*)tbl, sizeof(*tbl));
        kprintf("  Anchor String:        %b%b%b%b\n",
                tbl->AnchorString[0], tbl->AnchorString[1],
                tbl->AnchorString[2], tbl->AnchorString[3]);
        kprintf("  EPS Checksum:         0x%x\n", tbl->EntryPointStructureChecksum);
        kprintf("  Entry Point Len:      0x%x\n", tbl->EntryPointLength);
        kprintf("  Version:              %u.%u\n", tbl->MajorVersion, tbl->MinorVersion);
        kprintf("  Max Struct size:      0x%x\n", tbl->MaxStructureSize);
        kprintf("  Entry Point revision: 0x%x\n", tbl->EntryPointRevision);
        kprintf("  Inter Anchor:         %b%b%b%b\n",
                tbl->IntermediateAnchorString[0], tbl->IntermediateAnchorString[1],
                tbl->IntermediateAnchorString[2], tbl->IntermediateAnchorString[3]);
        kprintf("  Inter Checksum:       0x%x\n", tbl->IntermediateChecksum);
        kprintf("  Table Length:         0x%x\n", tbl->TableLength);
        kprintf("  Table Address:        0x%x\n", tbl->TableAddress);
        kprintf("  Number of Structures: %u\n", tbl->NumberOfSmbiosStructures);
        kprintf("  BCD Revision:         %u\n", tbl->SmbiosBcdRevision);
    }
}

static void dump_smbios_3_table(smbios_table_3_t *tbl)
{
    if (debug_flag > 4) {
        kprintf("SMBIOS Entry at %L::\n", (uint64_t)tbl);
        dump_hex((_Uint8t*)tbl, sizeof(*tbl));
        kprintf("  Anchor String:        %b%b%b%b%b\n",
                tbl->AnchorString[0], tbl->AnchorString[1],
                tbl->AnchorString[2], tbl->AnchorString[3], tbl->AnchorString[4]);
        kprintf("  EPS Checksum:         0x%x\n", tbl->EntryPointStructureChecksum);
        kprintf("  Entry Point Len:      0x%x\n", tbl->EntryPointLength);
        kprintf("  Version:              %u.%u\n", tbl->MajorVersion, tbl->MinorVersion);
        kprintf("  Entry Point revision: 0x%x\n", tbl->EntryPointRevision);
        kprintf("  Table Length:         0x%x\n", tbl->TableMaximumSize);
        kprintf("  Table Address:        %L\n", tbl->TableAddress);
    }
}

static void dump_smbios_data(smbios_structure_t *smbios_data_p)
{
    _Uint8t *data;
    _Uint8t len = 0;

    if (debug_flag > 4) {
        kprintf("Hdr:   %u-%u-%u   Buf:   %L\n",
                smbios_data_p->Hdr->Type, smbios_data_p->Hdr->Length, smbios_data_p->Hdr->Handle, smbios_data_p->buf);

        /* get smbios structure string section length */
        data = smbios_data_p->buf + smbios_data_p->Hdr->Length;
        while (data[0] || data[1]) {
            data++;
            len++;
        }
        len += 2 + smbios_data_p->Hdr->Length;

        data = smbios_data_p->buf;
        dump_hex(data, len);
    }
}

static void dump_smbios_data_type_1(smbios_table_type_1_t *smbios_p)
{
    if (debug_flag > 4) {
        kprintf("System Information\n");
        kprintf("Manufacturer          %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->Manufacturer));
        kprintf("ProductName           %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->ProductName));
        kprintf("Version               %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->Version));
        kprintf("SerialNumber          %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->SerialNumber));
        kprintf("Guid:                 %s\n", get_guid_str(&smbios_p->Guid));
        kprintf("WakeUpType:           0x%b\n", smbios_p->WakeUpType);
        kprintf("SKUNumber             %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->SKUNumber));
        kprintf("Family                %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->Family));
    }
}

static void dump_smbios_data_type_4(smbios_table_type_4_t *smbios_p)
{
    if (debug_flag > 4) {
        kprintf("Processor Information\n");
        kprintf("Socket                %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->Socket));
        kprintf("ProcessorType         %u\n", smbios_p->ProcessorType);
        kprintf("ProcessorFamily       %u\n", smbios_p->ProcessorFamily);
        kprintf("ProcessorManufacturer %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->ProcessorManufacturer));
        kprintf("ProcessorVersion      %s\n", get_smbios_string(&smbios_p->Hdr, smbios_p->ProcessorVersion));
        kprintf("MaxSpeed              0x%w (%u Mhz)\n", smbios_p->MaxSpeed, smbios_p->MaxSpeed);
        kprintf("CurrentSpeed          0x%w (%u Mhz)\n", smbios_p->CurrentSpeed, smbios_p->CurrentSpeed);
        kprintf("CoreCount             %u\n", smbios_p->CoreCount);
        kprintf("EnabledCoreCount      %u\n", smbios_p->EnabledCoreCount);
    }
}

const char *get_smbios_string(smbios_header_t *hdr, smbios_string_idx_t idx)
{
    char *data = (char *)hdr;

    if ((hdr != NULL) && (idx > 0)) {
        data += hdr->Length;
        while (idx > 1) {
            data += strlen(data);
            data++;
            idx--;
        }
        return data;
    }
    return "";
}

paddr_t board_find_efi_smbios_table(void)
{
    unsigned i;
    void     *cfg_tbl;

    if (smbios_table == NULL_PADDR) {
        for (i = 0; i < NUM_ELTS(smbios_guids); ++i) {
            cfg_tbl = uefi_find_config_tbl(&smbios_guids[i]);
            if (cfg_tbl != NULL) {
                if (i==0) {
                    smbios_table_3_t *table = (smbios_table_3_t *) cfg_tbl;
                    use_smbios_3 = 1;
                    dump_smbios_3_table(table);
                    as_add((paddr_t)table, (paddr_t)table+(table->TableMaximumSize)-1, AS_ATTR_RAM,
                        "smbios3", as_find(AS_NULL_OFF, "memory", NULL));
                }
                else {
                    smbios_table_t *table = (smbios_table_t *) cfg_tbl;
                    dump_smbios_table(table);
                    as_add((paddr_t)table, (paddr_t)table+(table->TableLength)-1, AS_ATTR_RAM,
                        "smbios", as_find(AS_NULL_OFF, "memory", NULL));
                }
                smbios_table = (paddr_t)cfg_tbl;
                break;
            }
        }
    }
    return smbios_table;
}

paddr_t board_find_efi_smbios_by_type(_Uint8t type)
{
	paddr_t table;
    smbios_structure_t smbios_data;
    smbios_structure_t smbios_end_data;
    _Uint8t *data_p;

    table = board_find_efi_smbios_table();
    if (table == NULL_PADDR) {
        kprintf("Could not find smbios table\n");
        return NULL_PADDR;
    }

    if (use_smbios_3 == 0) {
        smbios_data.buf = (_Uint8t *)(_Uint64t)((smbios_table_t *)table)->TableAddress;
        smbios_end_data.buf = smbios_data.buf + ((smbios_table_t *)table)->TableLength;
    }
    else {
        smbios_data.buf = (_Uint8t *)(_Uint64t)((smbios_table_3_t *)table)->TableAddress;
        smbios_end_data.buf = smbios_data.buf + ((smbios_table_3_t *)table)->TableMaximumSize;
    }

    while (1) {

        if (smbios_data.Hdr->Type == type) {
            if(debug_flag > 2) {
                kprintf("Find smbios type %u data at %L\n", type, smbios_data.buf);
            }
            dump_smbios_data(&smbios_data);
            if (type == 1) {
                 dump_smbios_data_type_1((smbios_table_type_1_t *)smbios_data.buf);
            }
            if (type == 4) {
                 dump_smbios_data_type_4((smbios_table_type_4_t *)smbios_data.buf);
            }
            return (paddr_t)smbios_data.buf;
        }

        if (smbios_data.Hdr->Type == 127) {
            /* reach to the last type */
            if(debug_flag > 2) {
                kprintf("Find smbios end type data\n");
            }
            break;
        }
        /* point to the next smbios structure */
        data_p = smbios_data.buf + smbios_data.Hdr->Length;

        /* skip string section, smbios structure ends with two consecutive 0s */
        while (data_p[0] || data_p[1]) {
            data_p++;
        }
        data_p += 2;
        smbios_data.buf = data_p;
        if (smbios_data.buf >= smbios_end_data.buf)
            break;
    }
    if (debug_flag > 2) {
        kprintf("Could not find type %u smbios data\n", type);
    }
    return NULL_PADDR;
}

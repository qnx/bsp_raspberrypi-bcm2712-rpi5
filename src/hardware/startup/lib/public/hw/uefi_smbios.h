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

#ifndef _HW_EFI_SMBIOS_H_INCLUDED
#define _HW_EFI_SMBIOS_H_INCLUDED

#ifndef __PLATFORM_H_INCLUDED
#include <sys/platform.h>
#endif
#include <hw/uefi.h>

/* Argh, some of the ACPI structures aren't naturally aligned :-( */
#include _NTO_HDR_(_pack1.h)

typedef _Uint8t  smbios_type_t;
typedef _Uint16t smbios_handle_t;
typedef _Uint8t  smbios_string_idx_t;

typedef struct {
    _Uint8t     AnchorString[4];
    _Uint8t     EntryPointStructureChecksum;
    _Uint8t     EntryPointLength;
    _Uint8t     MajorVersion;
    _Uint8t     MinorVersion;
    _Uint16t    MaxStructureSize;
    _Uint8t     EntryPointRevision;
    _Uint8t     FormattedArea[5];
    _Uint8t     IntermediateAnchorString[5];
    _Uint8t     IntermediateChecksum;
    _Uint16t    TableLength;
    _Uint32t    TableAddress;
    _Uint16t    NumberOfSmbiosStructures;
    _Uint8t     SmbiosBcdRevision;
} smbios_table_t;

typedef struct {
    _Uint8t     AnchorString[5];
    _Uint8t     EntryPointStructureChecksum;
    _Uint8t     EntryPointLength;
    _Uint8t     MajorVersion;
    _Uint8t     MinorVersion;
    _Uint8t     DocRev;
    _Uint8t     EntryPointRevision;
    _Uint8t     Reserved;
    _Uint32t    TableMaximumSize;
    _Uint64t    TableAddress;
} smbios_table_3_t;

typedef struct {
    smbios_type_t      Type;
    _Uint8t            Length;
    smbios_handle_t    Handle;
} smbios_header_t;

typedef struct {
    _Uint32t        Signature;
    _Uint32t        FeatureFlags;
} processor_id_data_t;

/* BIOS Information */
typedef struct {
	smbios_header_t          Hdr;
	smbios_string_idx_t      Vendor;
	smbios_string_idx_t      BiosVersion;
	_Uint16t                 BiosSegment;
    smbios_string_idx_t      BiosReleaseDate;
    _Uint8t                  BiosSize;
    _Uint32t                 BiosCharacteristics;
    _Uint8t                  BIOSCharacteristicsExtBytes[2];
    _Uint8t                  SysBiosMajorRelease;
    _Uint8t                  SysBiosMinorRelease;
    _Uint8t                  EmbeddedCtrlFmMajorRelease;
    _Uint8t                  EmbeddedCtrlFmMinorRelease;
    // From smbios 3.1.0
    _Uint16t                 ExtendedBiosSize;
} smbios_table_type_0_t;

/* System Information */
typedef struct {
	smbios_header_t          Hdr;
	smbios_string_idx_t      Manufacturer;
	smbios_string_idx_t      ProductName;
	smbios_string_idx_t      Version;
	smbios_string_idx_t      SerialNumber;
	EFI_GUID                 Guid;
	_Uint8t                  WakeUpType;            ///< The enumeration value from MISC_SYSTEM_WAKEUP_TYPE.
	smbios_string_idx_t      SKUNumber;
	smbios_string_idx_t      Family;
} smbios_table_type_1_t;

/* Base Board (or Module) Information (Type 2) */
typedef struct {
	smbios_header_t          Hdr;
	smbios_string_idx_t      Manufacturer;
	smbios_string_idx_t      ProductName;
	smbios_string_idx_t      Version;
	smbios_string_idx_t      SerialNumber;
	smbios_string_idx_t      AssetTag;
	_Uint8t                  FeatureFlag;
	smbios_string_idx_t      LocationInChassis;
	_Uint16t                 ChassisHandle;
	_Uint8t                  BoardType;            ///< The enumeration value from BASE_BOARD_TYPE.
	_Uint8t                  NumberOfContainedObjectHandles;
	_Uint16t                 ContainedObjectHandles[1];
} smbios_table_type_2_t;

/* Processor Information */
typedef struct {
    smbios_header_t          Hdr;
    smbios_string_idx_t      Socket;
    _Uint8t                  ProcessorType;         ///< The enumeration value from PROCESSOR_TYPE_DATA.
    _Uint8t                  ProcessorFamily;       ///< The enumeration value from PROCESSOR_FAMILY_DATA.
    smbios_string_idx_t      ProcessorManufacturer;
    processor_id_data_t      ProcessorId;
    smbios_string_idx_t      ProcessorVersion;
    _Uint8t                  Voltage;
    _Uint16t                 ExternalClock;
    _Uint16t                 MaxSpeed;
    _Uint16t                 CurrentSpeed;
    _Uint8t                  Status;
    _Uint8t                  ProcessorUpgrade;     ///< The enumeration value from PROCESSOR_UPGRADE.
    _Uint16t                 L1CacheHandle;
    _Uint16t                 L2CacheHandle;
    _Uint16t                 L3CacheHandle;
    smbios_string_idx_t      SerialNumber;
    smbios_string_idx_t      AssetTag;
    smbios_string_idx_t      PartNumber;
    // From smbios 2.5
    _Uint8t                  CoreCount;
    _Uint8t                  EnabledCoreCount;
    _Uint8t                  ThreadCount;
    _Uint16t                 ProcessorCharacteristics;
    // From smbios 2.6
    _Uint16t                 ProcessorFamily2;
    // From smbios 3.0
    _Uint16t                 CoreCount2;
    _Uint16t                 EnabledCoreCount2;
    _Uint16t                 ThreadCount2;
} smbios_table_type_4_t;

typedef struct {
	smbios_header_t    Hdr;
} smbios_table_type_127_t;

typedef union {
	smbios_header_t         *Hdr;
    smbios_table_type_4_t   *Type4;
    smbios_table_type_127_t *Type127;
    _Uint8t                 *buf;
} smbios_structure_t;



#include _NTO_HDR_(_packpop.h)

const char *get_smbios_string(smbios_header_t *hdr, smbios_string_idx_t idx);
paddr_t board_find_efi_smbios_table(void);
paddr_t board_find_efi_smbios_by_type(_Uint8t type);

#endif // _HW_EFI_SMBIOS_H_INCLUDED

/* __SRCVERSION("acpi.h $Rev: 965444 $"); */

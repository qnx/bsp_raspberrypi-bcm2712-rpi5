/*
 * $QNXLicenseC:
 * Copyright 2019, QNX Software Systems.
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

/*
 * ARM GIC Interrupt Translation Service
 */

#ifndef GIC_ITS_H_
#define GIC_ITS_H_


/* From ITS_base */
#define GITS_CTLR		0x0000u
#define GITS_IIDR		0x0004u
#define GITS_TYPER		0x0008u
#define GITS_CBASER		0x0080u
#define GITS_CWRITER	0x0088u
#define GITS_CREADR		0x0090u
#define GITS_BASER(n)	(0x0100u + ((n)*8))
#define GITS_PIDR2		(0xFFE8)


/* From ITS_base+0x10000 */
#define GITS_TRANSLATER	0x0040u


/* ITS commands */
#if defined(__BIG_ENDIAN__)
	#error ITS command structure definitions not valid for big endian systems
#endif

#define GITS_CMD_CLEAR		0x04u
struct gits_clear {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	resv1;
	_Uint64t	resv2[2];
};

#define GITS_CMD_DISCARD	0x0fu
struct gits_discard {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	resv1;
	_Uint64t	resv2[2];
};

#define GITS_CMD_INT		0x03u
struct gits_int {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	resv1;
	_Uint64t	resv2[2];
};

#define GITS_CMD_INV		0x0cu
struct gits_inv {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	resv1;
	_Uint64t	resv2[2];
};

#define GITS_CMD_INVALL		0x0du
struct gits_invall {
	_Uint8t		cmd;
	_Uint8t		resv0[7];
	_Uint64t	resv1;
	_Uint16t	ICID;
	_Uint8t		resv2[6];
	_Uint64t	resv3;
};

#define GITS_CMD_MAPC		0x09u
struct gits_mapc {
	_Uint8t		cmd;
	_Uint8t		resv0[7];
	_Uint64t	resv1;
	_Uint64t	RDbase_and_ICID;
	_Uint64t	resv2;
};

#define GITS_CMD_MAPD		0x08u
struct gits_mapd {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint8t		size;
	_Uint8t		resv1[7];
	_Uint64t	ITT_addr;
	_Uint64t	resv2;
};

#define GITS_CMD_MAPI		0x0bu
struct gits_mapi {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	resv1;
	_Uint16t	ICID;
	_Uint8t		resv2[6];
	_Uint64t	resv3;
};

#define GITS_CMD_MAPTI		0x0au
struct gits_mapti {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	pINTID;
	_Uint16t	ICID;
	_Uint8t		resv2[6];
	_Uint64t	resv3;
};

#define GITS_CMD_MOVALL		0x0eu
struct gits_movall {
	_Uint8t		cmd;
	_Uint8t		resv0[7];
	_Uint64t	resv1;
	_Uint64t	RDbase1;
	_Uint64t	RDbase2;
};

#define GITS_CMD_MOVI		0x01u
struct gits_movi {
	_Uint8t		cmd;
	_Uint8t		resv0[3];
	_Uint32t	DeviceID;
	_Uint32t	EventID;
	_Uint32t	resv1;
	_Uint16t	ICID;
	_Uint8t		resv2[6];
	_Uint64t	resv3;
};

#define GITS_CMD_SYNC		0x05u
struct gits_sync {
	_Uint8t		cmd;
	_Uint8t		resv0[7];
	_Uint64t	resv1;
	_Uint64t	RDbase1;
	_Uint64t	resv2;
};

union gits_cmds {
	struct gits_clear 	gclear;
	struct gits_discard gdiscard;
	struct gits_int 	gint;
	struct gits_inv 	ginv;
	struct gits_invall 	ginvall;
	struct gits_mapc 	gmapc;
	struct gits_mapd 	gmapd;
	struct gits_mapi 	gmapi;
	struct gits_mapti 	gmapti;
	struct gits_movall	gmovall;
	struct gits_movi	gmovi;
	struct gits_sync	gsync;
};


/* ITS command error numbers */
#define GITS_ERR_INV_DEVICE_OOR			0x010C01u
#define GITS_ERR_INV_UNMAPPED_DEVICE	0x010C04u
#define GITS_ERR_INV_ID_OOR				0x010C05u
#define GITS_ERR_INV_UNMAPPED_INTERRUPT	0x010C07u
#define GITS_ERR_INV_ITE_INVALID		0x010C10u
#define GITS_ERR_INVALL_COLLECTION_OOR	0x010D03u
#define GITS_ERR_INT_ID_OOR				0x010305u
#define GITS_ERR_INT_UNMAPPED_INTERRUPT	0x010307u
#define GITS_ERR_INT_ITE_INVALID		0x010310u
#define GITS_ERR_CLEAR_DEVICE_OOR		0x010501u
#define GITS_ERR_CLEAR_ID_OOR			0x010505u
#define GITS_ERR_CLEAR_UNMAPPED_INTERRUPT 0x010507u
#define GITS_ERR_CLEAR_ITE_INVALID		0x010510u
#define GITS_ERR_VMAPP_VCPU_OOR			0x012911u
#define GITS_ERR_VMAPI_ID_OOR			0x012b05u
#define GITS_ERR_VMAPI_PHYSICALID_OOR	0x012b06u
#define GITS_ERR_VMAPTI_ID_OOR			0x012a05u
#define GITS_ERR_VMAPTI_VIRTUALID_OOR	0x012a13u
#define GITS_ERR_VSYNC_VCPU_OOR			0x012511u
#define GITS_ERR_VSYNC_VCPU_INVALID		0x012514u
#define GITS_ERR_VMOVI_ID_OOR			0x012105u
#define GITS_ERR_VMOVI_PHYSICALID_OOR	0x012106u
#define GITS_ERR_VMOVI_ID_IS_PHYSICAL	0x012115u
#define GITS_ERR_VMOVI_ITEVCPU_INVALID	0x012116u
#define GITS_ERR_VMOVI_CMDVCPU_INVALID	0x012117u

#endif    /* GIC_ITS_H_ */

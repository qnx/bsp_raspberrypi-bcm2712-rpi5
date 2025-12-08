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
 * Copyright 2007, 2008, QNX Software Systems.
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

#ifndef _SD_H_INCLUDED
#define _SD_H_INCLUDED

#define SD_CID_SIZE					16
#define SD_CSD_SIZE					16
#define SD_ERASE_TIMEOUT			250			// 250ms

#define	SD_GO_IDLE_STATE			0
#define	SD_ALL_SEND_CID				2
#define	SD_SEND_RELATIVE_ADDR				3
	#define SD_SEND_RELATIVE_ADDR_RETRIES	255

#define	SD_SET_DSR					4
#define	SD_SLEEP_AWAKE				5
	#define SD_SA_SLEEP				(1 << 15 )

#define	SD_SWITCH_FUNC				6
	#define SD_SF_MODE_SET			0x1
	#define SD_SF_MODE_CHECK		0x0

	#define SD_SF_GRP_DFLT			0x0
	#define SD_SF_GRP_BUS_SPD		0x0		// Bus Speed
	#define SD_SF_GRP_CMD_EXT		0x1		// Command System Extension
	#define SD_SF_GRP_DRV_STR		0x2		// Driver Strength
	#define SD_SF_GRP_CUR_LMT		0x3		// Current Limit

	#define SD_SF_CUR_FCN			0xf

	#define SD_SF_STATUS_SIZE		64

#define	SD_SEL_DES_CARD				7
#define	SD_SEND_IF_COND				8
	#define SD_SIC_VHS_27_36V		1
	#define SD_SIC_TEST_PATTERN		0xaa

#define	SD_SEND_EXT_CSD				8
#define	SD_SEND_CSD					9
#define	SD_SEND_CID					10
#define	SD_VOLTAGE_SWITCH			11
#define	SD_STOP_TRANSMISSION		12
#define	SD_SEND_STATUS				13
// Card/Device Status Response Bits
	#define	CDS_OUT_OF_RANGE			(1 << 31)
	#define	CDS_ADDRESS_ERROR			(1 << 30)
	#define	CDS_BLOCK_LEN_ERROR			(1 << 29)
	#define	CDS_ERASE_SEQ_ERROR			(1 << 28)
	#define	CDS_ERASE_PARAM				(1 << 27)
	#define	CDS_WP_VIOLATION			(1 << 26)
	#define	CDS_CARD_IS_LOCKED			(1 << 25)
	#define	CDS_LOCK_UNLOCK_FAILED		(1 << 24)
	#define	CDS_COM_CRC_ERROR			(1 << 23)
	#define	CDS_ILLEGAL_COMMAND			(1 << 22)
	#define	CDS_CARD_ECC_FAILED			(1 << 21)
	#define	CDS_CC_ERROR				(1 << 20)
	#define	CDS_ERROR					(1 << 19)
	#define	CDS_UNDERRUN				(1 << 18)
	#define	CDS_OVERRUN					(1 << 17)
	#define	CDS_CID_CSD_OVERWRITE		(1 << 16)
	#define	CDS_WP_ERASE_SKIP			(1 << 15)
	#define	CDS_CARD_ECC_DISABLED		(1 << 14)
	#define	CDS_ERASE_RESET				(1 << 13)

	#define CDS_CUR_STATE_MSK			( 0xf << 9 )
	#define	CDS_CUR_STATE_IDLE			(0 << 9)
	#define	CDS_CUR_STATE_READY			(1 << 9)
	#define	CDS_CUR_STATE_IDENT			(2 << 9)
	#define	CDS_CUR_STATE_STANDBY		(3 << 9)
	#define	CDS_CUR_STATE_TRAN			(4 << 9)
	#define	CDS_CUR_STATE_DATA			(5 << 9)
	#define	CDS_CUR_STATE_RCV			(6 << 9)
	#define	CDS_CUR_STATE_PRG			(7 << 9)
	#define	CDS_CUR_STATE_DIS			(8 << 9)

	#define	CDS_READY_FOR_DATA			(1 << 8)
	#define	CDS_SWITCH_ERROR			(1 << 7)
	#define	CDS_URGENT_BKOPS			(1 << 6)
	#define	CDS_APP_CMD_S				(1 << 5)

	#define CDS_ERROR_MSK				(	CDS_OUT_OF_RANGE		| \
											CDS_ADDRESS_ERROR		| \
											CDS_BLOCK_LEN_ERROR		| \
											CDS_ERASE_SEQ_ERROR		| \
											CDS_ERASE_PARAM			| \
											CDS_WP_VIOLATION		| \
											CDS_CARD_IS_LOCKED		| \
											CDS_LOCK_UNLOCK_FAILED	| \
											CDS_COM_CRC_ERROR		| \
											CDS_ILLEGAL_COMMAND		| \
											CDS_CARD_ECC_FAILED		| \
											CDS_CC_ERROR			| \
											CDS_ERROR				| \
											CDS_SWITCH_ERROR 		| \
	CDS_CID_CSD_OVERWRITE )

#define	SD_GO_INACTIVE_STATE			15
#define	SD_SET_BLOCKLEN					16
#define	SD_READ_SINGLE_BLOCK			17
#define	SD_READ_MULTIPLE_BLOCK			18
#define SD_SEND_TUNING_BLOCK			19
#define	SD_SPEED_CLASS_CONTROL			20
#define SD_SET_UPPER_ADDR				22
#define SD_SET_BLOCK_COUNT				23
#define	SD_WRITE_BLOCK					24
#define	SD_WRITE_MULTIPLE_BLOCK			25
#define	SD_PROGRAM_CSD					27
#define	SD_SET_WRITE_PROT				28
#define	SD_CLR_WRITE_PROT				29
#define	SD_SEND_WRITE_PROT				30
#define	SD_ERASE_WR_BLK_START			32
#define	SD_ERASE_WR_BLK_END				33
#define	SD_ERASE						38
	#define SD_ERASE_NORM				0x00000000
	#define SD_ERASE_TRIM				0x00000001
	#define SD_ERASE_GARBAGE_COLLECT	0x80008001
	#define SD_ERASE_SECURE_TRIM		0x80000001
	#define SD_ERASE_SECURE				0x80000000
#define	SD_LOCK_UNLOCK					42
	#define SD_LU_ERASE					0x08
	#define SD_LU_LOCK					0x04
	#define SD_LU_UNLOCK				0x00
	#define SD_LU_CLR_PWD				0x02
	#define SD_LU_SET_PWD				0x01
	#define SD_LU_PWD_SIZE				16		// max password size

#define SD_READ_EXTR_SINGLE				48
#define SD_WRITE_EXTR_SINGLE			49
#define SD_READ_EXTR_MULTI				58
#define SD_WRITE_EXTR_MULTI				59
	#define SD_EXTR_SIZE				512
	#define SD_EXTR_MIO( _t )			( (_t) & SD_EXTR_MIO_MSK )
	#define SD_EXTR_IO					( 1 << 31 )
	#define SD_EXTR_MEM					( 0 << 31 )
	#define SD_EXTR_MIO_MSK				( 1 << 31 )
	#define SD_EXTR_MW					( 1 << 26 )
	#define SD_EXTR_MWM( _m )			( (_m) & SD_EXTR_MWM_MSK )
	#define SD_EXTR_MWM_MSK				( 1 << 26 )
	#define SD_EXTR_FNO( _f )			( ( (_f) & 0xf ) << 27 )
	#define SD_EXTR_ADDR( _a )			( ( (_a) & 0x1ffff ) << 9 )
	#define SD_EXTR_LEN_MSK( _l, _m )	( (_m) ? ( (_l) & 0xff ) : ( ( (_l) - 1 ) & 0x1ff ) )

#define	SD_APP_CMD						55
#define	SD_GEN_CMD						56
#define	SD_READ_OCR						58
#define	SD_CRC_ON_OFF					59

// ACMDS - preceded with APP_CMD (CMD55)
#define	SD_AC_SET_BUS_WIDTH				6
	#define SD_BUS_WIDTH_1				0
	#define SD_BUS_WIDTH_4				2

#define SD_AC_SD_STATUS					13
	#define SD_STATUS_SIZE				64
#define SD_AC_SEND_NUM_WR_BLOCKS		22
#define SD_AC_SET_WR_BLK_ERASE_COUNT	23
#define	SD_AC_SEND_OP_COND				41
#define SD_AC_SET_CLR_CARD_DETECT		42
#define	SD_AC_SEND_SCR					51
	#define	SD_SCR_SIZE					8

// ASSD commands
#define SD_READ_SEC_CMD					34
#define SD_WRITE_SEC_CMD				35
	#define SD_SEC_CMD_SIZE				512
#define SD_SEND_PSI						36
	#define SD_PSI_ASSD_SR				0	// ASSD Status Register
	#define SD_PSI_ASSD_PR				4	// ASSD Properties Register
	#define SD_PSI_ASSD_RNR				6	// ASSD Random Number Register
	#define SD_PSI_SIZE					32
#define SD_CONTROL_ASSD_SYSTEM			37
#define SD_DIRECT_SECURE_READ			50
#define SD_DIRECT_SECURE_WRITE			57

typedef struct _sd_cid_t {
	_Uint8t		mid;		// Manufacture ID
	_Uint8t		oid[3];		// OEM/Application ID
	_Uint8t		pnm[6];		// Product name
	_Uint8t		prv;		// Product revision
	_Uint32t	psn;		// Product serial number
	_Uint16t	mdt;		// Manufacture date
} sd_cid_t;

typedef struct _sd_scr_t {
	_Uint8t		scr_structure;	// SCR structure

// SD version is a combination of sd_spec, sd_spec3, sd_spec4 and sd_specx. See specification.
#define SCR_SPEC_VER_0		0	// Version 1.0 - 1.01
#define SCR_SPEC_VER_1		1	// Version 1.10
#define SCR_SPEC_VER_2		2	// Version 2.0 or Version 3.0X

#define SCR_SPEC3_VER_3		1	// Version 3

#define SCR_SPEC4_VER_4		1	// Version 4

#define SCR_SPECX_VER_5		1	// Version 5
#define SCR_SPECX_VER_6		2	// Version 6
#define SCR_SPECX_VER_7		3	// Version 7
#define SCR_SPECX_VER_8		4	// Version 8
	_Uint8t		sd_spec;		// Physical layer specification
	_Uint8t		data_stat_after_erase;
	_Uint8t		sd_security;

#define SCR_BUS_WIDTH_1		(1 << 0)
#define SCR_BUS_WIDTH_4		(1 << 2)
	_Uint8t		sd_bus_widths;
	_Uint8t		sd_spec3;
	_Uint8t		sd_spec4;
	_Uint8t		sd_specx;
	_Uint8t		ex_security;
#define SCR_CMD58_59_SUP	(1 << 3)
#define SCR_CMD48_49_SUP	(1 << 2)
#define SCR_CMD23_SUP		(1 << 1)
#define SCR_CMD20_SUP		(1 << 0)
	_Uint8t		cmd_support;
} sd_scr_t;

typedef struct _sd_sds_t {		// SD Status
	_Uint32t	speed_class;
	_Uint32t	uhs_speed_grade;
	_Uint32t	au_size;
	_Uint32t	uhs_au_size;
	_Uint32t	erase_timeout;
	_Uint32t	erase_offset;
#define SD_PERF_CLASS_A2		2
	_Uint32t	perf_class;
	_Uint32t	perf_enh;
} sd_sds_t;

typedef struct _sd_switch_cap {
	_Uint32t		dtr_max_hs;

#define SD_BUS_MODE_LS			( 1 << 0 )
#define SD_BUS_MODE_SDR12		( 1 << 0 )
#define SD_BUS_MODE_HS			( 1 << 1 )
#define SD_BUS_MODE_SDR25		( 1 << 1 )
#define SD_BUS_MODE_SDR50		( 1 << 2 )
#define SD_BUS_MODE_SDR104		( 1 << 3 )
#define SD_BUS_MODE_DDR50		( 1 << 4 )
#define SD_BUS_MODE_UHS			( SD_BUS_MODE_SDR50 | SD_BUS_MODE_SDR104 | SD_BUS_MODE_DDR50 )
#define SD_BUS_MODE_MSK			0x1f
	_Uint32t		bus_mode;

#define SD_CMD_SYS_EC			( 1 << 1 )	// eCommerce
#define SD_CMD_SYS_OTP			( 1 << 3 )
#define SD_CMD_SYS_ASSD			( 1 << 4 )
	_Uint32t		cmd_sys;

#define SD_DRV_TYPE_B			0x01
#define SD_DRV_TYPE_A			0x02
#define SD_DRV_TYPE_C			0x04
#define SD_DRV_TYPE_D			0x08
#define SD_DRV_TYPE_MSK			0x0f
	_Uint32t		drv_type;

#define SD_CURR_LIMIT_200		( 1 << 0 )
#define SD_CURR_LIMIT_400		( 1 << 1 )
#define SD_CURR_LIMIT_600		( 1 << 2 )
#define SD_CURR_LIMIT_800		( 1 << 3 )
#define SD_CURR_LIMIT_MSK		0xf
	_Uint32t		curr_limit;
} sd_switch_cap_t;

#define SD_FCN_PG_SZ		512

typedef struct _sd_fcn_ext_hdr {
	uint8_t		structure_revsion[2];
	uint8_t		gen_info_length[2];
	uint8_t		number_extensions;
	uint8_t		rsvd[11];
} sd_fcn_ext_hdr;

typedef struct _sd_fcn_ext {
#define SFC_PWR_MGNT		0x01
#define SFC_PERF_ENH		0x02
	uint16_t		sfc;		// Standard Function Code
	uint16_t		fcc;		// Function Capability Code
	uint16_t		fmc;		// Function Manufacturer Code
	uint8_t			fmn[16];	// Function Manufacturer Name
	uint16_t		pfc;		// Particular Function Code
	uint8_t			fn[16];		// Function Name
	uint16_t		pne;		// Pointer to Next Extension
	uint8_t			nrs;		// Number of Register Sets
	uint8_t			rsvd;
#define SD_FCN_EXT_FNO( _rsa )			( ( (_rsa) >> 18 ) & 0xf )
#define SD_FCN_EXT_SADDR( _rsa )		( (_rsa) & 0x1ffff )
	uint32_t		rsa[1];		// Variable length Register Set Addresses
} sd_fcn_ext;

// Power Management Function Register definitions
#define PWR_MGNT_REV					0		// Ronly
	#define PWR_MGNT_REV_MSK			0xf
#define PWR_MGNT_STATUS					1		// Ronly
	#define PWR_MGNT_STATUS_PDMS		0x40	// Power down support
	#define PWR_MGNT_STATUS_PSUS		0x20	// Power sustenance support
	#define PWR_MGNT_STATUS_POFS		0x10	// Power off support
	#define PWR_MGNT_STATUS_PDMR		0x04	// Power down mode ready
	#define PWR_MGNT_STATUS_PSUR		0x02	// Power sustenance request
	#define PWR_MGNT_STATUS_POFR		0x01	// Power off ready
#define PWR_MGNT_SETTING				2		// R/W
	#define PWR_MGNT_SETTING_PDMN		0x04	// Power down mode notification
	#define PWR_MGNT_SETTING_PSUN		0x02	// Power sustenance notification
	#define PWR_MGNT_SETTING_POFN		0x01	// Power off notification

// Performance Enhancement Function Register definitions
#define PERF_ENH_REV					0		// Ronly
#define PERF_ENH_EVT_SUP				1		// Ronly
#define PERF_ENH_MAINT_SUP				2		// Ronly
#define PERF_ENH_MAINT_URGENCY			3		// Ronly
#define PERF_ENH_CACHE_SUP				4		// Ronly
	#define PERF_ENH_CACHE_SUPPORTED	0x1
#define PERF_ENH_CQ_DEPTH				6		// Ronly
#define PERF_ENH_TASK_STATUS			8		// 8 - 15 Ronly
#define PERF_ENH_EVT_FX					257		// R/W
	#define PERF_ENH_EVT_FX_EN			0x1
#define PERF_ENH_MAINT_TYP				258		// R/W
	#define PERF_ENH_MAINT_TYP_HOST_EN		0x2
	#define PERF_ENH_MAINT_CARD_EN		0x1
#define PERF_ENH_MAINT					259		// R/W
	#define PERF_ENH_MAINT_HOST_START	0x1
#define PERF_ENH_CACHE					260		// R/W
	#define PERF_ENH_CACHE_EN			0x01
#define PERF_ENH_CACHE_FLUSH			261		// R/W
	#define PERF_ENH_CACHE_FLUSH_START	0x01
#define PERF_ENH_CQ_EN					262		// R/W

#endif

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

// Module Description:  SIM SDMMC header file

#ifndef _SIM_SDMMC_H_INCLUDED
#define _SIM_SDMMC_H_INCLUDED

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include <signal.h>
#include <atomic.h>
#include <stdarg.h>
#include <stdbool.h>
#include <pthread.h>
#include <gulliver.h>
#include <hw/inout.h>
#include <sys/mman.h>
#include <sys/disk.h>
#include <sys/types.h>
#include <sys/trace.h>
#include <sys/resmgr.h>
#include <sys/procmgr.h>
#include <sys/syspage.h>

// CAM specific includes
#include <module.h>
#include <ntocam.h>
#include <sim.h>
#include <sys/dcmd_cam.h>
#include <sys/rpmb.h>
#include <hw/dcmd_sim_sdmmc.h>

#include <sim_bs.h>

#include <sdiodi.h>
#include <sd.h>
#include <mmc.h>

#ifdef SDMMC_POWMAN_SUP
#include <sys/slog2.h>
#include <powerman/pm_client.h>
#endif

//#define SDMMC_WRITE_VERIFY
#define SDMMC_TRIM_SUP
//#define SDMMC_SIM_RETRY

//#define SDMMC_TRACE
//#define SDMMC_SIM_DEBUG

#define SDMMC_TRACE_EVENT				1

#define SDMMC_SIM_VERSION				100
#define SDMMC_SIM_LETTER				'A'

#define SDMMC_RW_RETRIES				2
#define SDMMC_STACK_SIZE				16384
#define SDMMC_SCHED_PRIORITY			21
#define SDMMC_TIME_DEFAULT				5
#define SDMMC_TIME_INFINITY				0xffffffff

#define SDMMC_PM_TIMER					0x40		// Timer event

#define SDMMC_POWMAN_UPDATE				0x41
#define SDMMC_POWMAN_RECONNECT			0x42

#define SDMMC_MAX_BUS					10

#define SDMMC_MAX_HBA					8
#define SDMMC_MAX_TARGET				8
#define SDMMC_MAX_SG					128

#define SDMMC_START_CCB_MAX				25			// max loops in sdmmc_start_ccb fcn

#define SDMMC_TRIM_MAX_LBA				0xffffffff
#define SDMMC_TIMEOUT_MS_TO_NS( _to )	( (uint64_t)( _to ) * 1000LL * 1000LL )
#define SDMMC_TIMEOUT_S_TO_NS( _to )	( (uint64_t)( _to ) * 1000LL * 1000LL * 1000LL )

#define SDMMC_ADDR_ALIGNED( _addr, _align )		( ( ( _addr ) & ( ( typeof( _addr ) )( _align ) - 1 ) ) == 0 )

typedef struct _sdmmc_ctrl {
	TAILQ_HEAD(,_sim_hba)	hlist;			// linked list of hba's

#define SDMMC_CFLAG_SCAN		0x01		// auto detect interfaces
#define SDMMC_CFLAG_ENUMERATING	0x02
#define SDMMC_CFLG_ARGS_ERR		0x04		// flag to indicate arg processing error
	uint32_t				cflags;
	int						verbosity;
	uint32_t				nhba;			// number of hba's
	path_id_t				pathid_max;		// max path id
	int						priority;

	int						argc;
	char					**argv;
	struct sdio_connection	*connection;
} SDMMC_CTRL;

#define SDMMC_TARGET_MAX				8
#define SDMMC_PARTITION_MAX				8

typedef struct _sdmmc_partition {
	_Uint32t		pflags;
	_Uint32t		config;
	char			name[20];
	_Uint32t		blk_shft;
	_Uint64t		slba;			// starting lba
	_Uint64t		elba;			// ending lba
	_Uint64t		nlba;			// num lba
	_Uint64t		rc;				// Read Count
	_Uint64t		wc;				// Written Count
	_Uint64t		tc;				// TRIM Count
	_Uint64t		ec;				// Erase Count
	_Uint64t		dc;				// Discard Count
} SDMMC_PARTITION;

typedef struct _sdmmc_target {
	_Uint32t			nluns;
	_Uint32t			blksz;
	SDMMC_PARTITION		partitions[SDMMC_PARTITION_MAX];
} SDMMC_TARGET;

typedef struct _sim_sdmmc_ext {
	SIM_HBA					*hba;

#define SDMMC_EFLAG_PRESENT				(1 << 0)
#define SDMMC_EFLAG_TIMER				(1 << 1)
#define SDMMC_EFLAG_BKOPS				(1 << 2)
#define SDMMC_EFLAG_DEVNAME				(1 << 3)
#define SDMMC_EFLAG_PARTITIONS			(1 << 4)	// create hw partitions
#define SDMMC_EFLAG_ASSD_INIT			(1 << 5)
#define SDMMC_EFLAG_ASSD_SEND_STOP		(1 << 6)
#define SDMMC_EFLAG_DEV_BUSY			(1 << 7)
#define SDMMC_EFLAG_CACHE				(1 << 8)
#define SDMMC_EFLAG_PWROFF_NOTIFY		(1 << 9)
#define SDMMC_EFLAG_BKOPS_AUTO			(1 << 10)	// Device BKOPS
#define SDMMC_EFLAG_VCACHE_DIRTY		(1 << 11)
#define SDMMC_EFLAG_RELWR				(1 << 12)	// Enable Reliable Writes
#define SDMMC_EFLAG_POWMAN				(1 << 13)	// Attach to powerman for suspend/resume
#define SDMMC_EFLAG_BS					(1 << 24)
	_Uint32t				eflags;
	_Uint8t					pwroff_notify;
	_Uint8t					rsvd[7];

	CCB_SCSIIO				*nexus;
	int						start_ccb_max;			// max loops in sdmmc_start_ccb fcn

	struct sdio_device		*device;
	sdio_device_instance_t	instance;
	sdio_hc_info_t			hc_inf;
	sdio_dev_info_t			dev_inf;

	timer_t					pm_timerid;

	_Uint64t				pm_timestamp;
	_Uint64t				pm_idle_time_ns;
	_Uint64t				pm_sleep_time_ns;

#ifdef SDMMC_POWMAN_SUP
	struct powman			*powman_hdl;
#endif
	char					*powman_name;

#define SDMMC_PM_IDLE							0
#define SDMMC_PM_SLEEP							1
#define SDMMC_PM_ACTIVE							2
#define SDMMC_PM_SUSPEND						3
	_Uint32t				pm_state;

	_Uint32t				drvr_state;			// paused/running

#define BKOPS_NC_TICKS							10
#define BKOPS_IMPACTED_TICKS					5
	_Uint32t				bkops_ticks;
#define BKOPS_STATUS_OPERATIONS_NONE			0
#define BKOPS_STATUS_OPERATIONS_NON_CRITICAL	1
#define BKOPS_STATUS_OPERATIONS_IMPACTED		2
#define BKOPS_STATUS_OPERATIONS_CRITICAL		3
#define BKOPS_STATUS_OPERATIONS_INPROG			0x80000000
	_Uint32t				bkops_status;
#define SDMMC_TIME_BKOPS			( SDIO_TIME_DEFAULT	* 5 )

	SDMMC_ASSD_PROPERTIES	assd_properties;
	int						assd_active_sec_sys;

	_Uint32t				ntargs;
	SDMMC_TARGET			targets[SDMMC_TARGET_MAX];

#ifdef SDMMC_WRITE_VERIFY
#define SDMMC_VER_BSIZE		( 512 * 256 )
	char					*ver_vaddr;
	paddr64_t				ver_paddr;
#endif

} SIM_SDMMC_EXT;

// sim_sdmmc.c
extern int sdmmc_sim_args( char *options );
extern int sdmmc_sim_detach( void );
extern int sdmmc_sim_attach( CAM_ENTRY *centry );
extern int sdmmc_sim_init( SIM_HBA *hba, int path );
extern int sdmmc_sim_action( SIM_HBA *hba, CCB *ccbp );
extern void *sdmmc_driver_thread( void *hdl );
extern void sdmmc_powman_disconnect( SIM_HBA *hba );
extern int sdmmc_suspend( SIM_HBA *hba );
extern int sdmmc_resume( SIM_HBA *hba );

extern int sdmmc_bkops( SIM_HBA *hba, int tick );
extern int sdmmc_bkops_cfg( SIM_HBA *hba, int op );
extern int sdmmc_pwroff_notify( SIM_HBA *hba, uint8_t cfg );
extern int sdmmc_sense( CCB_SCSIIO *ccb, uint8_t sense, uint8_t asc, uint8_t ascq );
extern int sdmmc_unit_ready( SIM_HBA *hba, CCB_SCSIIO *ccb );
extern int sdmmc_wp_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sdmmc_erase_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sdmmc_card_register_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sdmmc_rw( SIM_HBA *hba, SDMMC_PARTITION *part, uint32_t flgs, uint64_t addr, uint32_t dlen, sdio_sge_t *sgl, uint32_t sgc, void *mhdl, uint32_t timeout );
extern int sdmmc_read_write( SIM_HBA *hba, CCB_SCSIIO *ccb, int flgs );
extern int sdmmc_reset( SIM_HBA *hba );
extern int sim_bs_partition_config( SIM_HBA *hba );
extern int sim_bs_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sim_bs_pass_through( SIM_HBA *hba, CCB_SCSIIO *ccb );
extern int sim_bs_dev_cfg( SIM_HBA *hba );

// sim_assd.c
extern int sdmmc_assd_init( SIM_HBA *hba );
extern int sdmmc_assd_apdu_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sdmmc_assd_status_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sdmmc_assd_control_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );
extern int sdmmc_assd_properties_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb );

extern char			*__progname;

extern SDMMC_CTRL sdmmc_ctrl;

#endif

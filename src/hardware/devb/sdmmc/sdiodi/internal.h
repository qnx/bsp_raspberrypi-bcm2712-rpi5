/*
 * $QNXLicenseC:
 * Copyright (c) 2007,2008,2024, BlackBerry Limited. All rights reserved.
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

#ifndef _INTERNAL_H_INCLUDED
#define _INTERNAL_H_INCLUDED

#include <sys/queue.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/slogcodes.h>
#include <sys/rpmb.h>

#include <sdiodi.h>
#include <mmc.h>
#include <sd.h>

#include <bs.h>

#include <pci/pci.h>

#define _SLOGC_SDIODI					(_SLOGC_SIM_MMC)

//#define SDIO_UC_SUPPORT				// enable once we get a card to test
//#define SDIO_APP_SPEC_FCN_EXT			// enable after we get a card to test

//#define SDIO_TRACE
#define SDIO_TRACE_EVENT				1

#define SDIO_VENDOR_ID_WILDCARD			0xffff
#define SDIO_DEVICE_ID_WILDCARD			0xffff

#define SDIO_PRIORITY					21
#define SDIO_INVALID_NUM				(-0xBAD1)
#define SDIO_STACK_SIZE					32768//16384
#define SDIO_TSTATE_CREATING			0x00
#define SDIO_TSTATE_INITIALIZED			0x01
#define SDIO_TSTATE_INIT_FAILURE		0x02

#define SDIO_NELEMS( _x )				( sizeof( (_x) ) / sizeof( (_x)[0] ) )

#define SDIO_PWR_OFF					0
#define SDIO_PWR_ON						1

#define SDIO_LDO_VCC					0
#define SDIO_LDO_VCC_IO					1

#define SDIO_CMD_RETRIES				3
#define SDIO_RESET_RETRIES				3
#define SDIO_MAX_BUS_ERRS				2
#define SDIO_DFLT_BLKSZ					512
#define SDIO_BLKSZ_512					512
#define SDIO_BLKSZ_4K					4096
#define SDIO_CLK_INIT					400000
#define SDIO_TIMEOUT_MS_TO_NS( _to )	( (uint64_t)( _to ) * 1000LL * 1000LL )

#define SDIO_CHK_GEN( _d )		( (_d)->instance.generation != (_d)->hc->generation )

typedef struct _sdio_hc				sdio_hc_t;
typedef struct _sdio_hc_cfg			sdio_hc_cfg_t;
typedef struct _sdio_hc_entry		sdio_hc_entry_t;
typedef struct _sdio_dev			sdio_dev_t;
typedef struct sdio_cmd				sdio_cmd_t;
typedef struct _pulse				sdio_event_t;
typedef struct _sdio_wspc			sdio_wspc_t;
typedef struct _sdio_ctrl			sdio_ctrl_t;
typedef struct _sdio_vendor			sdio_vendor_t;
typedef struct _sdio_product		sdio_product_t;
typedef struct _sdio_device_errata	sdio_device_errata_t;
typedef struct _sdio_pci_dev		sdio_pci_dev_t;

#define DTR_MAX_SDR104			208000000
#define DTR_MAX_SDR50			100000000
#define DTR_MAX_DDR50			50000000
#define DTR_MAX_SDR25			50000000
#define DTR_MAX_SDR12			25000000
#define DTR_MAX_HS400			200000000
#define DTR_MAX_HS200			200000000
#define DTR_MAX_HS52			52000000
#define DTR_MAX_HS26			26000000
#define DTR_MIN_LS				100000
#define DTR_HS_DEC				5000000
#define DTR_LS_DEC				50000

struct _sdio_product {
	_Uint32t		did;			// device id
	_Uint32t		class;
	_Uint32t		aflags;
	char			*name;
						// chipset init (map mem, set caps etc...)
	int				(*init)( sdio_hc_t *hc );
};

struct _sdio_vendor {
	_Uint32t		vid;
	char			*name;
	sdio_product_t	*chipsets;
};

#define DEV_ERRATA_WILDCARD		(~0)

struct _sdio_device_errata {
	char		*pnm;
	uint32_t	mid;
	uint32_t	oid;
	uint32_t	prv_s;
	uint32_t	prv_e;
	uint32_t	erev;

#define DEV_ERRATA_ACMD12			0x01
#define DEV_ERRATA_DISCARD_SUP		0x02
	uint32_t	errata;
	uint32_t	rsettle;
};

struct sdio_cmd {
	TAILQ_ENTRY(sdio_cmd)	clink;
	void					*hdl;
	_Uint32t				flags;
	_Uint32t				status;
	_Uint32t				opcode;
	_Uint32t				arg;
	_Uint32t				earg;	// extended arg
	_Uint32t				rsp[4];
	_Uint32t				blks;
	_Uint32t				blksz;
	_Uint32t				sgc;
	sdio_sge_t				*sgl;
	void					*mhdl;
	void					(*cbf)( struct sdio_device *, sdio_cmd_t *, void *);
};

struct _sdio_wspc {
	sdio_cmd_t			*cmd;		// active command
	sdio_sge_t			*sge;
	_Uint8t				*sga;
	uint32_t			nsg;
	uint32_t			sgc;
};

#define SDIO_HC_ENTRY_NFUNCS      ( ( sizeof( sdio_hc_entry_t ) - offsetof( sdio_hc_entry_t, dinit ) ) / sizeof( void *)  )

struct _sdio_hc_entry {
	_Uint32t	nentries;
	int			(*dinit)(sdio_hc_t *);

#define PM_IDLE			0
#define PM_ACTIVE		1
#define PM_SLEEP		2
#define PM_SUSPEND		3
	int			(*pm)(sdio_hc_t *, uint32_t action);
	int			(*cmd)(sdio_hc_t *, sdio_cmd_t *);
	int			(*abort)(sdio_hc_t *, sdio_cmd_t *);

#define HC_EV_TIMER		0
#define HC_EV_CD		1
#define HC_EV_TUNE		2
#define HC_EV_INTR		3
#define HC_EV_DMA		4
#define HC_EV_BS		20	// board specific events start here
	int			(*event)(sdio_hc_t *, sdio_event_t *);
#define CD_RMV			0x00		// card removed
#define CD_INS			0x01		// card inserted
#define CD_WP			0x80		// card write protected
	int			(*cd)(sdio_hc_t *);

	int			(*pwr)(sdio_hc_t *, uint32_t vdd);
	int			(*clk)(sdio_hc_t *, uint32_t clk);

#define BUS_MODE_OPEN_DRAIN		0
#define BUS_MODE_PUSH_PULL		1
	int			(*bus_mode)(sdio_hc_t *, uint32_t mode);

#define BUS_WIDTH_1				1
#define BUS_WIDTH_4				4
#define BUS_WIDTH_8				8
	int			(*bus_width)(sdio_hc_t *, uint32_t width);

#define TIMING_HS400			9
#define TIMING_HS200			8
#define TIMING_SDR104			7
#define TIMING_SDR50			6
#define TIMING_SDR25			5
#define TIMING_SDR12			4
#define TIMING_DDR50			3
#define TIMING_HS				2
#define TIMING_LS				1
	int			(*timing)(sdio_hc_t *, uint32_t timing);

#define SIGNAL_VOLTAGE_3_3		1
#define SIGNAL_VOLTAGE_3_0		2
#define SIGNAL_VOLTAGE_1_8		3
#define SIGNAL_VOLTAGE_1_2		4
	int			(*signal_voltage)(sdio_hc_t *, uint32_t sv);

#define	DRV_TYPE_B				1
#define	DRV_TYPE_A				2
#define	DRV_TYPE_C				4
#define	DRV_TYPE_D				8
	int			(*drv_type)(sdio_hc_t *, uint32_t type);
	int			(*driver_strength)(sdio_hc_t *, uint32_t timing, uint32_t type);
	int			(*tune)(sdio_hc_t *, uint32_t op);
	int			(*preset)(sdio_hc_t *, uint32_t);
};

struct _sdio_dev {
	sdio_hc_t				*hc;

	_Uint32t				dtype;			// device type

#define DEV_FLAG_PRESENT		0x001		// device present
#define DEV_FLAG_LOCKED			0x002
#define DEV_FLAG_INVALID_CARD	0x004
#define DEV_FLAG_MEDIA_CHANGE	0x008
#define DEV_FLAG_IDLE			0x010
#define DEV_FLAG_ACTIVE			0x020
#define DEV_FLAG_SLEEP			0x040
#define DEV_FLAG_HS				0x080		// high speed
#define DEV_FLAG_HS200			0x100		// high speed 200
#define DEV_FLAG_HS400			0x200		// high speed 400
#define DEV_FLAG_DDR			0x400		// DDR
#define DEV_FLAG_UHS			0x800		// UHS
#define DEV_FLAG_BKOPS			0x1000		// BKOPS
#define DEV_FLAG_SIG_ERR		0x2000		// signal switch error
#define DEV_FLAG_WRITE_PROTECT	0x4000		// write protected
#define DEV_FLAG_WCE			0x8000		// Write Cache Enable
#define DEV_FLAG_HS400ES		0x10000		// high speed 400 enhanced strobe
	_Uint32t				flags;

	_Uint32t				rsettle;

	_Uint32t				rsvd;

	_Uint64t				caps;			// see DEV_CAP_xxx

	_Uint32t				ocr;

	_Uint32t				pwd_len;
	_Uint8t					pwd[MMC_LU_PWD_SIZE];

	_Uint32t				pactive;		// active partition

	_Uint32t				rca;

	sdio_cid_t				cid;
	sdio_csd_t				csd;
	sdio_ecsd_t				ecsd;
	sd_scr_t				scr;
	sd_sds_t				sds;
	sd_switch_cap_t			swcaps;

	_Uint32t				pwr_mgnt_fcn_rsa;	// power managment function regester set address
	_Uint32t				perf_enh_fcn_rsa;	// performance enhancement function regester set address

	_Uint32t				block_length;

	_Uint32t				wp_size;
	_Uint32t				erase_size;

	_Uint32t				rel_wr_sec_c;

	_Uint32t				raw_cid[SDIO_CID_SIZE];
	_Uint32t				raw_csd[SDIO_CSD_SIZE];
	_Uint32t				raw_scr[SD_SCR_SIZE / 4 ];
	_Uint8t					raw_ecsd[MMC_EXT_CSD_SIZE];
};

struct _sdio_hc_cfg {
#define SDIO_NAME_MAX		64
	char				name[SDIO_NAME_MAX];

	_Uint32t			flags;
	int					verbosity;

	_Uint64t			caps;

	_Uint16t			vid;
	_Uint16t			did;
	_Uint32t			class;
	int					idx;

	uint32_t			sg_max;

	uint32_t			clk;

	uint32_t			bus_width;
	uint32_t			timing;

#define SDIO_MAX_IRQ		4
	int					irqs;
	int					irq[SDIO_MAX_IRQ];

#define SDIO_MAX_DMA		4
	int					dma_chnls;
	int					dma_chnl[SDIO_MAX_DMA];

#define SDIO_MAX_ADDR		6
	int					base_addrs;
	int					base_addr_size[SDIO_MAX_ADDR];
	_Uint64t			base_addr[SDIO_MAX_ADDR];

	_Uint64t			io_xlat;
	_Uint64t			mem_xlat;
	_Uint64t			bmstr_xlat;
	_Uint64t			bmstr_align;

#ifndef SDIO_PM_IDLE_TIME
	#define SDIO_PM_IDLE_TIME		100		// 100ms
#endif

#ifndef SDIO_PM_SLEEP_TIME
	#define SDIO_PM_SLEEP_TIME		10000	// 10 seconds
#endif

	_Uint32t			idle_time;		// time in ms
	_Uint32t			sleep_time;		// time in ms

	char				*options;		// board specific options
	char				*hsoptions;		// host specific options
};

#define SDIO_PCI_INTR_MSIX						2
#define SDIO_PCI_INTR_MSI						1
#define SDIO_PCI_INTR_PIN						0

struct _sdio_pci_dev {
	pci_devhdl_t		dev_hdl;
	pci_bdf_t			bdf;
	pci_cap_t			pcie;
	pci_cap_t			msi;
	pci_cap_t			msix;
 	uint32_t			msi_nirq;
	uint32_t			msix_nirq;
#define SDIO_PFLG_MSI		0x01
#define SDIO_PFLG_MSIX		0x02
	uint32_t			pflags;
};

struct _sdio_hc {							// Host Controller
	TAILQ_ENTRY(_sdio_hc)	hlink;
	sdio_hc_cfg_t		cfg;
	sdio_hc_entry_t		entry;
	sdio_dev_t			device;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	pthread_mutex_t		cd_mutex;
	_Uint32t			path;
	_Uint32t			state;

	_Uint32t			usage;
	int					priority;

	_Uint32t			generation;


#define HC_FLAG_INITIALIZED			( 1 << 1 )
#define	HC_FLAG_RST					( 1 << 2 )
#define	HC_FLAG_TUNE				( 1 << 3 )
#define	HC_FLAG_DEV_SD				( 1 << 4 )
#define	HC_FLAG_DEV_MMC				( 1 << 5 )
#define	HC_FLAG_DEV_SDIO			( 1 << 6 )
#define	HC_FLAG_DEV_TYPE			( HC_FLAG_DEV_SD | HC_FLAG_DEV_MMC | HC_FLAG_DEV_SDIO )
#define	HC_FLAG_SKIP_PWRUP			( 1 << 7 )
	_Uint32t			flags;

	_Uint64t			caps;				// Capabilities, see HC_CAP_xxx

	_Uint32t			version;

	_Uint32t			pm_state;

	_Uint64t			pm_sleep_cnt;

		// hc thread
	int					hc_chid;
	int					hc_coid;
	int					hc_tid;				// thread id
	int					hc_iid;				// interrupt id

	int					tuning_count;
	int					tuning_timerid;

	int					slot;

	sdio_pci_dev_t		pci;

	sdio_wspc_t			wspc;				// data xfer workspc

	_Uint32t			clk_min;
	_Uint32t			clk_max;
	_Uint32t			clk_init;

	_Uint32t			ocr;
	_Uint32t			clk;
	_Uint32t			vdd;
	_Uint32t			timing;				// see TIMING_xxx
	_Uint32t			drv_type;			// see DRV_TYPE_xxx
	_Uint32t			bus_mode;			// see BUS_MODE_xxx
	_Uint32t			bus_width;			// see BUS_WIDTH_xxx
	_Uint32t			signal_voltage;		// see SIGNAL_VOLTAGE_xxx

	_Uint32t			bus_errs;			// bus errors

	void				*cs_hdl;			// Chipset specfic handle
	void				*bs_hdl;			// Board specfic handle
};

#define SDIO_EV_TIMER		0
#define SDIO_EV_CD			1

#define SDIO_CD_INTERVAL	1

struct _sdio_ctrl {
#define SDIO_CFLAG_SCAN			0x1
	_Uint32t					flags;
	_Uint32t					state;
	int							priority;
	int							verbosity;

#define SDIO_HC_MAX			8
	int							nhc;
	TAILQ_HEAD(,_sdio_hc)		hlist;
	TAILQ_HEAD(,sdio_device)	dlist;

	TAILQ_HEAD(,sdio_cmd)		clist;

	sdio_connect_parm_t			connect_parm;

	pthread_mutex_t				mutex;

		// change detect thread
	int							cd_chid;
	int							cd_coid;
	int							cd_tid;
	int							cd_timerid;
	pthread_cond_t				cd_cond;
	int							cd_enum;
};

struct sdio_device {
	TAILQ_ENTRY(sdio_device)	dlink;
	sdio_device_instance_t		instance;
#define DEV_FLAG_RMV_PENDING	0x01
	_Uint32t					flags;
	_Uint32t					usage;
	void						*user;
	sdio_hc_t					*hc;
	sdio_dev_t					*dev;
};


// base.c
extern sdio_ctrl_t			sdio_ctrl;
extern const uint8_t		sdio_tbp_4bit[];
extern const uint8_t		sdio_tbp_8bit[];
extern sdio_vendor_t		sdio_vendors[];

extern int fls( int val );
extern int sdio_options( sdio_hc_t *hc, char *options );
extern ssize_t __attribute__((__format__(__printf__, 5, 6)))sdio_slogf( int opcode, int severity, int verbosity, int vlevel, const char *fmt, ... );

extern int sdio_timer_settime( int tid, uint32_t sec, uint32_t nsec, int repeat );
extern int sdio_set_thread_state( uint32_t *tstate, int state );
extern int sdio_create_thread( pthread_t *tid, pthread_attr_t *aattr, void *(*func)(void *), void *arg, int priority, uint32_t *tstate, char *name );
extern int sdio_sg_start( sdio_hc_t *hc, sdio_sge_t *sge, uint32_t nsg );
extern int sdio_sg_nxt( sdio_hc_t *hc, _Uint8t **addr, uint32_t *len, uint32_t blksz );
extern int sdio_vtop_sg( sdio_sge_t *vsg, sdio_sge_t *psg, int sgc, void *mhdl );
extern paddr64_t sdio_vtop( void *vaddr );
extern uint32_t sdio_extract_bits( uint32_t *data, int bits, int start, int size );
extern sdio_hc_t *sdio_hc_alloc( void );
extern int sdio_hc_free( sdio_hc_t *hc );
extern int sdio_hc_getsubopt( char **optionp, char * const *tokens, char **valuep );
extern sdio_product_t *sdio_hc_lookup( uint16_t vid, uint16_t did, uint32_t class, char *name );
extern int sdio_reconcile_errata( sdio_dev_t *dev, sdio_device_errata_t *errata );
extern sdio_device_errata_t *sdio_device_errata( sdio_dev_t *dev, sdio_device_errata_t *erratas );

extern int sdio_go_idle( sdio_hc_t *hc );
extern int sdio_select_card( sdio_dev_t *dev, uint32_t rca );
extern int sdio_send_csd( sdio_dev_t *dev, uint32_t *csd );
extern int sdio_all_send_cid( sdio_hc_t *hc, uint32_t *cid );

extern int sdio_clock( sdio_hc_t *hc, uint32_t clk );
extern int sdio_tune( sdio_hc_t *hc, uint32_t cmd );
extern int sdio_preset( sdio_hc_t *hc, uint32_t state );
extern int sdio_timing( sdio_hc_t *hc, uint32_t timing );
extern int sdio_bus_mode( sdio_hc_t *hc, uint32_t bus_mode );
extern int sdio_bus_width( sdio_hc_t *hc, uint32_t bus_width );
extern int sdio_drv_type( sdio_hc_t *hc, uint32_t drv_type );
extern int sdio_select_voltage( sdio_hc_t *hc, uint32_t ocr );
extern int sdio_signal_voltage( sdio_hc_t *hc, uint32_t voltage );
extern int sdio_wait_cmd( sdio_hc_t *hc, struct sdio_cmd *cmd, uint64_t tms );
extern int sdio_issue_cmd( sdio_dev_t *dev, struct sdio_cmd *cmd, uint64_t tms );

extern int _sdio_disconnect( void );
extern int _sdio_reset( sdio_dev_t *dev );
extern int _sdio_bus_error( sdio_dev_t *dev );
extern int _sdio_pwrmgnt( sdio_dev_t *dev, uint32_t pm );
extern int _sdio_retune( sdio_hc_t *hc );
extern int _sdio_set_block_count( sdio_dev_t *dev, uint32_t blkcnt, uint32_t flgs );
extern int _sdio_set_block_length( sdio_dev_t *dev, uint32_t blklen );
extern int _sdio_stop_transmission( sdio_dev_t *dev, int hpi );
extern int _sdio_send_status( sdio_dev_t *dev, uint32_t *rsp, int hpi );
extern int _sdio_send_cmd( sdio_dev_t *dev, struct sdio_cmd *cmd,
		void (*func)( struct sdio_device *, sdio_cmd_t *, void *),
		uint32_t timeout, int retries );
extern int _sdio_connect( sdio_connect_parm_t *parm, struct sdio_connection **connection );
extern int _sdio_lock_unlock( sdio_dev_t *dev, int op, uint8_t *pwd, uint32_t pwd_len );
extern int _sdio_wait_card_status( sdio_dev_t *dev, uint32_t *rsp, uint32_t mask, uint32_t val, uint32_t msec );
extern int _sdio_cache( sdio_dev_t *dev, int op, uint32_t timeout );

	// HC callbacks for change detect and cmd completion
extern int sdio_hc_event( sdio_hc_t *hc, int ev );
extern int sdio_cmd_cmplt( sdio_hc_t *hc, struct sdio_cmd *cmd, uint32_t status );
// base.c end

// mmc.c
extern int mmc_ident( sdio_hc_t *hc );
extern int mmc_init_device( sdio_hc_t *hc, uint32_t ocr, int flgs );
extern int mmc_bus_error( sdio_dev_t *dev );
extern int mmc_bkops_cfg( sdio_dev_t *dev );
extern int mmc_sleep_awake( sdio_dev_t *dev, int flgs );
extern int mmc_send_ext_csd( sdio_dev_t *dev, uint8_t *csd );
extern int mmc_set_partition( sdio_dev_t *dev, uint32_t partition );
extern int mmc_cache( sdio_dev_t *dev, int op, uint32_t timeout );
extern uint64_t mmc_erase_timeout( sdio_dev_t *dev, uint32_t etype, uint64_t nlba );
extern int mmc_erase( sdio_dev_t *dev, uint32_t partition, uint32_t flgs, uint64_t lba, uint32_t nlba );
extern int mmc_write_protect( sdio_dev_t *dev, int op, int ptype, int mode, uint32_t lba, uint32_t nlba );
extern int mmc_switch( sdio_dev_t *dev, uint32_t cmdset, uint32_t mode, uint32_t index, uint32_t value, uint32_t timeout );
extern int mmc_rpmb_rw( sdio_dev_t *dev, void *pf, uint32_t nf, uint32_t flgs );
extern int mmc_gen_man_cmd( sdio_dev_t *dev, uint8_t op, void *buf, uint32_t blklen, uint32_t blkcnt, uint32_t arg, uint32_t flgs);
extern int mmc_retune( sdio_hc_t *hc );
extern int mmc_pwroff_notify( sdio_dev_t *dev, int op, int timeout );
// mmc.c end

// sd.c
extern int sd_ident( sdio_hc_t *hc );
extern int sd_init_device( sdio_hc_t *hc, uint32_t ocr, int flgs );
extern int sd_bus_error( sdio_dev_t *dev );
extern int sd_app_cmd( sdio_dev_t *dev );
extern int sd_send_if_cond( sdio_hc_t *hc, uint32_t vhs );
extern uint64_t sd_erase_timeout( sdio_dev_t *dev, uint32_t etype, uint32_t nlba );
extern int sd_erase( sdio_dev_t *dev, uint32_t flgs, uint64_t lba, uint32_t nlba );
extern int sd_switch( sdio_dev_t *dev, int mode, int grp, uint8_t val, uint8_t *switch_status );
extern int sd_lock_unlock( sdio_dev_t *dev, int action, uint8_t *pwd, uint32_t pwd_len );
extern int sd_cache( sdio_dev_t *dev, int op, uint32_t timeout );
extern int sd_pwroff_notify( sdio_dev_t *dev, int op, int timeout );
// sd.c end

// bs.c
extern int bs_powman( sdio_hc_t *hc, int state );
extern int bs_pad_conf( sdio_hc_t *hc, int state );
extern int bs_clock_gate( sdio_hc_t *hc, int op );
extern int bs_event( sdio_hc_t *hc, sdio_event_t *ev );
extern int bs_set_ldo( sdio_hc_t *hc, int ldo, uint32_t voltage );
// bs.c end

// soc.c
extern int sdio_soc_scan( void );
extern int sdio_soc_device( sdio_hc_t *hc );
// soc.c end

// pci.c
extern int sdio_pci_init( void );
extern int sdio_pci_dinit( void );
extern int sdio_pci_scan( void );
extern int sdio_pci_device( sdio_hc_t *hc );
extern int sdio_pci_detach_device( sdio_hc_t *hc );
// pci.c end

// libc
extern uint64_t _syspage_time( clockid_t clock_id );
// card.c 
extern int sd_app_sd_status( sdio_dev_t * const dev, uint32_t * const sds );
#endif

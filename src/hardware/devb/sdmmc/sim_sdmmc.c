/*
 * $QNXLicenseC:
 * Copyright (c) 2014,2024, BlackBerry Limited. All rights reserved.
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

// Module Description:  SDMMC SIM initialization module.

#include <sim_sdmmc.h>

SDMMC_CTRL sdmmc_ctrl;

static const MODULE_ENTRY sim_module = {
	.name	= "sdmmc",
	.args	= &sdmmc_sim_args,		// called once for every time it is on the cmd line
	.attach	= &sdmmc_sim_attach,		// start setup, and call xpt_bus_register
	.detach	= &sdmmc_sim_detach
};

static CAM_SIM_ENTRY sdmmc_sim_entry = {
	.sim_init	= &sdmmc_sim_init,
	.sim_action	= &sdmmc_sim_action
};

#ifdef SDMMC_POWMAN_SUP
static pm_state_t					sdmmc_pm_state_on		= PM_DEFINE_STATE( "p_state", 1, "on" );
static pm_state_t					sdmmc_pm_state_suspend	= PM_DEFINE_STATE( "p_state", 0, "suspend" );
static pm_state_t					*sdmmc_pm_states_list[]	= { &sdmmc_pm_state_on, &sdmmc_pm_state_suspend, NULL };

#endif

#ifdef SDMMC_TRACE
void sdmmc_trace_start( void )
{
	TraceEvent( _NTO_TRACE_DELALLCLASSES );
	TraceEvent( _NTO_TRACE_CLRCLASSPID, _NTO_TRACE_KERCALL );
	TraceEvent( _NTO_TRACE_CLRCLASSTID, _NTO_TRACE_KERCALL );
	TraceEvent( _NTO_TRACE_CLRCLASSPID, _NTO_TRACE_THREAD );
	TraceEvent( _NTO_TRACE_CLRCLASSTID, _NTO_TRACE_THREAD );

#if 1
	TraceEvent( _NTO_TRACE_ADDALLCLASSES );
#else
	TraceEvent( _NTO_TRACE_ADDCLASS, _NTO_TRACE_CONTROL );
	TraceEvent( _NTO_TRACE_ADDCLASS, _NTO_TRACE_INT );
	TraceEvent( _NTO_TRACE_ADDCLASS, _NTO_TRACE_KERCALL );
	TraceEvent( _NTO_TRACE_ADDCLASS, _NTO_TRACE_PROCESS );
	TraceEvent( _NTO_TRACE_ADDCLASS, _NTO_TRACE_THREAD );
#endif

	TraceEvent( _NTO_TRACE_START );
}

void sdmmc_trace_stop( void )
{
	TraceEvent( _NTO_TRACE_STOP );
}

void sdmmc_trace_event( int event, const char *fmt, ... )
{
	char		buf[255];
	va_list		arglist;

	va_start( arglist, fmt );
	vsnprintf( buf, 254, fmt, arglist );
	va_end( arglist );
	TraceEvent( _NTO_TRACE_INSERTUSRSTREVENT, event, buf );
}
#endif

// The main routine for the SIM driver
int main( int const argc, char *argv[] )
{
	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s %d.%02d%c (%s %s)", CAM_STRINGIZE(__DEVB_NAME__),
		SDMMC_SIM_VERSION / 100, SDMMC_SIM_VERSION % 100, SDMMC_SIM_LETTER,
		__DATE__, __TIME__ );

	memset( &sdmmc_ctrl, 0, sizeof( SDMMC_CTRL ) );

	TAILQ_INIT( &sdmmc_ctrl.hlist );

	sdmmc_ctrl.argc		= argc;
	sdmmc_ctrl.argv		= argv;
	sdmmc_ctrl.priority	= SDMMC_SCHED_PRIORITY;

#ifdef SDMMC_TRACE
	sdmmc_trace_start( );
	sdmmc_trace_event( SDMMC_TRACE_EVENT, "SDMMC Trace" );
	sdmmc_trace_stop( );
#endif

	return( cam_configure( &sim_module, 1, argc, argv ) );
}

static SIM_HBA *sdmmc_alloc_hba( void )
{
	SIM_HBA			*hba;
	SIM_SDMMC_EXT	*ext;

	hba = sim_alloc_hba( sizeof( SIM_SDMMC_EXT ) );
	if( hba == NULL ) {
		return( NULL );
	}

	ext = (SIM_SDMMC_EXT *)hba->ext;

	hba->pathid			= -1;
	hba->coid			= -1;
	hba->chid			= -1;
	hba->tid			= -1;
	hba->iid			= hba->pathid;
	hba->verbosity		= sdmmc_ctrl.verbosity;
	ext->ntargs			= 0;
	hba->priority		= sdmmc_ctrl.priority;
	ext->pm_timerid		= -1;

#ifdef SDMMC_POWMAN_SUP
	ext->powman_hdl		= NULL;
#endif

	ext->start_ccb_max	= SDMMC_START_CCB_MAX;

	ext->assd_active_sec_sys = -1;

	memset( &ext->instance, SDIO_CONNECT_WILDCARD, sizeof( sdio_device_instance_t ) );

		// add hba to drivers hba list
	pthread_sleepon_lock( );
	TAILQ_INSERT_TAIL( &sdmmc_ctrl.hlist, hba, hlink );
	sdmmc_ctrl.nhba++;
	pthread_sleepon_unlock( );

//	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  hba %p", __FUNCTION__, hba );

	return( hba );
}

static int sdmmc_free_hba( SIM_HBA *hba )
{
	pthread_sleepon_lock( );
	TAILQ_REMOVE( &sdmmc_ctrl.hlist, hba, hlink );
	sdmmc_ctrl.nhba--;
	pthread_sleepon_unlock( );
	sim_free_hba( hba );

//	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  hba %p", __FUNCTION__, hba );
	return( CAM_SUCCESS );
}

#ifndef SIM_BS_PARTITION_CONFIG
static int sdmmc_partition_enum( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_PARTITION		*part;
	uint8_t				*ecsd;
	uint32_t			pidx;
	uint32_t			pconf;
	uint32_t			nlba;
	uint64_t			gp_size;
	uint64_t			wp_grp_size;

	ext				= (SIM_SDMMC_EXT *)hba->ext;
	ecsd			= sdio_get_raw_ecsd( ext->device );
	pconf			= ecsd[ECSD_PART_CONFIG] & ~ECSD_PC_ACCESS_MSK;
	wp_grp_size		= (uint64_t )ecsd[ECSD_HC_WP_GRP_SIZE ] * (uint64_t )ecsd[ECSD_ERASE_GRP_SIZE ] * MMC_ERASE_GRP_512K;

		// boot area partitions (1/2)
	for( pidx = 0; pidx < MMC_BOOT_PART_MAX; pidx++ ) {
		part			= &ext->targets[1 + pidx].partitions[0];
		part->config	= pconf | ( 1 + pidx );
		part->slba		= 0;
		part->nlba		= (uint64_t)ecsd[ECSD_BOOT_SIZE_MULT] * MMC_BOOT_SIZE_MULT / 512;
		part->elba		= part->nlba - 1;
		strlcpy( part->name, "boot", sizeof( part->name ) );
		if( ( ecsd[ECSD_PARTITIONING_SUP] & ECSD_PS_PART_EN ) ) {
			part->pflags |= SDMMC_PFLAG_ENH;
		}
		ext->ntargs++;
	}

		// RPMB area
	part			= &ext->targets[3].partitions[0];
	part->config		= pconf | 3;
	part->slba		= 0;
	part->nlba		= (uint64_t)ecsd[ECSD_RPMB_SIZE_MULT] * MMC_RPMB_SIZE_MULT / 512;
	part->elba		= part->slba + part->nlba - 1;
	strlcpy(part->name, "rpmb", sizeof(part->name));
	ext->ntargs++;

		// GP partitions
	for( pidx = 0; pidx < MMC_GP_PART_MAX; pidx++ ) {
		gp_size = (uint64_t)ecsd[ECSD_GP_SIZE + 0 + ( pidx * 3 ) ];
		gp_size |= (uint64_t)ecsd[ECSD_GP_SIZE + 1 + ( pidx * 3 ) ] << 8;
		gp_size |= (uint64_t)ecsd[ECSD_GP_SIZE + 2 + ( pidx * 3 ) ] << 16;
		nlba = ( gp_size * wp_grp_size ) / 512;

		part			= &ext->targets[ 4 + pidx].partitions[0];
		part->config	= pconf | ( 4 + pidx );
		part->slba		= 0;
		part->nlba		= nlba;
		part->elba		= nlba ? ( part->nlba - 1 ) : 0;
		strlcpy(part->name, "gp", sizeof(part->name));
		ext->ntargs++;

		if( ( ecsd[ECSD_PARTITIONS_ATTR] & ( ECSD_PA_ENH_1 << pidx ) ) ) {
			part->pflags |= SDMMC_PFLAG_ENH;
		}
	}

	return( EOK );
}
#endif

static int sdmmc_partition_config( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_TARGET	*targ;
	SDMMC_PARTITION	*part;
	uint8_t			*ecsd;
	uint64_t		nlba;
	uint32_t		tidx;
	uint32_t		blksz;

	ext				= (SIM_SDMMC_EXT *)hba->ext;
	targ			= ext->targets;
	part			= targ->partitions;

	nlba			= ext->dev_inf.sectors;
	blksz			= ext->dev_inf.sector_size;

	for( tidx = 0; tidx < SDMMC_TARGET_MAX; tidx++ ) {
		targ 			= &ext->targets[tidx];
		targ->blksz		= blksz;
	}

	if( ( ext->dev_inf.flags & DEV_FLAG_WP ) ) {
		part->pflags |= SDMMC_PFLAG_WP;
	}

	part->config	= 0;
	part->slba		= 0;
	part->nlba		= nlba;
	part->elba		= nlba - 1;
	strlcpy( part->name, "uda", sizeof( part->name ) );

	if( ext->instance.ident.dtype == DEV_TYPE_MMC ) {
		ecsd			= sdio_get_raw_ecsd( ext->device );
		part->config	= ecsd[ECSD_PART_CONFIG] & ~ECSD_PC_ACCESS_MSK;
#ifdef SIM_BS_PARTITION_CONFIG
		sim_bs_partition_config( hba );
#else
		if( ( ext->eflags & SDMMC_EFLAG_PARTITIONS ) ) {
			sdmmc_partition_enum( hba );
		}
#endif
	}

	return( CAM_SUCCESS );
}

static int sdmmc_dev_cfg( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT		*ext;
	uint8_t				*ecsd;
	int					status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= CAM_SUCCESS;

//	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s", __FUNCTION__ );

	if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) ) {
		if( sdio_cache( ext->device, ( ext->eflags & SDMMC_EFLAG_CACHE ) ? SDIO_CACHE_ENABLE : SDIO_CACHE_DISABLE, SDIO_TIME_DEFAULT ) != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error %s volatile cache", __FUNCTION__, ( ext->eflags & SDMMC_EFLAG_CACHE ) ? "Enabling" : "Disabling" );
			ext->eflags &= ~SDMMC_EFLAG_CACHE;
		}
	}

	if( ext->instance.ident.dtype == DEV_TYPE_MMC ) {
		ecsd			= sdio_get_raw_ecsd( ext->device );
		if( ( ecsd[ ECSD_BKOPS_EN ] & ECSD_BKOPS_ENABLE ) ) {	// check ECSD for manual BKOPS permanent setting
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  ECSD Manual BKOPS enabled", __FUNCTION__ );
			ext->eflags |= SDMMC_EFLAG_BKOPS;
		}

		if( sdmmc_bkops_cfg( hba, CAM_TRUE ) != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Enabling BKOPS", __FUNCTION__ );
			ext->eflags &= ~SDMMC_EFLAG_BKOPS;
		}

		if( ( ext->eflags & SDMMC_EFLAG_RELWR ) && !( ext->dev_inf.caps & DEV_CAP_WR_REL ) ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  WARNING Reliable Write Requested, but not supported by HW", __FUNCTION__ );
			ext->eflags &= ~SDMMC_EFLAG_RELWR;
		}
	}

	if( ( ext->eflags & SDMMC_EFLAG_PWROFF_NOTIFY ) ) {
		if( sdio_pwroff_notify( ext->device, ECSD_POWERED_ON, SDIO_TIME_DEFAULT ) != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Power Off Notify On", __FUNCTION__ );
			ext->eflags &= ~SDMMC_EFLAG_PWROFF_NOTIFY;
		}
	}

	if( ( ext->dev_inf.caps & DEV_CAP_ASSD ) ) {
		sdmmc_assd_init( hba );
	}

#ifdef SIM_BS_DEV_CFG
	status = sim_bs_dev_cfg( hba );
#endif

	return( status );
}

static int sdmmc_dev_dcfg( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT	*ext;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= CAM_SUCCESS;

//	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s", __FUNCTION__ );

		// Flush volatile cache
	if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) && ( ext->eflags & SDMMC_EFLAG_CACHE ) ) {
		status = sdio_cache( ext->device, SDIO_CACHE_FLUSH, SDIO_TIME_DEFAULT * 5 );
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Cache Flush", __FUNCTION__ );
		}
	}

		// Switch off Device BKOPS (eMMC 5.1)
	if( ( ext->eflags & SDMMC_EFLAG_BKOPS_AUTO ) ) {
		if( ( status = sdmmc_bkops_cfg( hba, CAM_FALSE ) ) ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Disabling auto BKOPS", __FUNCTION__ );
		}
	}

		// Send notification ECSD_POWER_OFF_SHORT/ECSD_POWER_OFF_LONG to the device
	if( ( ext->eflags & SDMMC_EFLAG_PWROFF_NOTIFY ) ) {
		status = sdio_pwroff_notify( ext->device, ext->pwroff_notify, SDIO_TIME_DEFAULT );
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Power Off Notify", __FUNCTION__ );
		}
	}

	return( status );
}

static int sdmmc_detach( SIM_HBA *hba, const int hba_free )
{
	SIM_SDMMC_EXT	*ext;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

//	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  hba %p, %s", __FUNCTION__, hba, hba_free ? "free", "keep" );

	if( hba->pathid != -1 ) {
		xpt_bus_deregister( hba->pathid );
	}

	if( hba->coid != -1 ) {
		ConnectDetach( hba->coid );
	}

	if( hba->chid != -1 ) {
		ChannelDestroy( hba->chid );
	}

	if( hba->tid != -1 ) {
		pthread_join( hba->tid, NULL );
	}

	if( ext->pm_timerid != -1 ) {
		timer_delete( ext->pm_timerid );
	}

#ifdef SDMMC_POWMAN_SUP
	sdmmc_powman_disconnect( hba );
#endif

	if( hba->simq ) {
		simq_dinit( hba->simq );
	}

	if( ext->device ) {
		sdmmc_dev_dcfg( hba );
		sdio_detach( ext->device );
	}

#ifdef SDMMC_WRITE_VERIFY
	if( ext->ver_vaddr ) {
		xpt_free( ext->ver_vaddr, SDMMC_VER_BSIZE );
		ext->ver_vaddr = NULL;
	}
#endif

	hba->pathid		= -1;
	hba->coid		= -1;
	hba->chid		= -1;
	hba->tid		= -1;
	ext->pm_timerid	= -1;
	ext->device		= NULL;
	hba->simq		= NULL;

	if( hba_free == CAM_TRUE ) {
		sdmmc_free_hba( hba );
	}
	else {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s: keep 0x%p", __FUNCTION__, hba );
	}

	return( CAM_SUCCESS );
}

static int sdmmc_reg( SIM_HBA *hba )
{
	SIM_SDMMC_EXT	*ext;
	pthread_attr_t		attr;
	struct sched_param	param;
	int					policy;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	pthread_attr_init( &attr );
	pthread_attr_setschedpolicy( &attr, SCHED_RR );
	pthread_getschedparam( pthread_self( ), &policy, &param );
	param.sched_priority = hba->priority;
	pthread_attr_setschedparam( &attr, &param );
	pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
	pthread_attr_setstacksize( &attr, CAM_STACK_SIZE );

#ifdef SDMMC_WRITE_VERIFY
	if( ( ext->ver_vaddr = xpt_alloc( XPT_ALLOC_CONTIG | XPT_ALLOC_NOCACHE, SDMMC_VER_BSIZE, NULL ) ) == MAP_FAILED ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: xpt_alloc verify buffer failure", __FUNCTION__ );
		return( CAM_FAILURE );
	}

	ext->ver_paddr = xpt_vtop( ext->ver_vaddr, NULL );
#endif

	if( cam_create_thread( &hba->tid, &attr, sdmmc_driver_thread, hba, hba->priority, &hba->state, "sdmmc_driver_thread" ) != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: sdmmc_driver_thread creation failure", __FUNCTION__ );
		return( CAM_FAILURE );
	}

	atomic_set( &ext->eflags, SDMMC_EFLAG_PRESENT );

	hba->pathid		= xpt_bus_register( &sdmmc_sim_entry, hba );

	if( hba->pathid > sdmmc_ctrl.pathid_max ) {
		sdmmc_ctrl.pathid_max = hba->pathid;
	}

	xpt_async( AC_SIM_REGISTER, hba->pathid, -1, -1, NULL, 0 );

	return( CAM_SUCCESS );
}

static int sdmmc_attach( SIM_HBA * const hba, struct sdio_connection * const connection, sdio_device_instance_t * const instance )
{
	SIM_SDMMC_EXT		*ext;
	int					status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= CAM_FAILURE;

	if( sdio_attach( connection, instance, &ext->device, hba ) == EOK ) {
		ext->instance			= *instance;
		sdio_hc_info( ext->device, &ext->hc_inf );
		sdio_dev_info( ext->device, &ext->dev_inf );
		ext->pm_idle_time_ns	= SDMMC_TIMEOUT_MS_TO_NS( ext->hc_inf.idle_time );
		ext->pm_sleep_time_ns	= SDMMC_TIMEOUT_MS_TO_NS( ext->hc_inf.sleep_time );

		sdmmc_partition_config( hba );

		sdmmc_dev_cfg( hba );

		status = sdmmc_reg( hba );
	}

	return( status );
}

static void sdmmc_sdio_insertion( struct sdio_connection * const connection, sdio_device_instance_t * const instance )
{
	SIM_HBA				*hba;
	SIM_SDMMC_EXT		*ext;

	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, sdmmc_ctrl.verbosity, 1, "%s:  ", __FUNCTION__ );

	if( !( sdmmc_ctrl.cflags & SDMMC_CFLAG_SCAN ) ||
			( sdmmc_ctrl.cflags & SDMMC_CFLAG_ENUMERATING ) ) {
		return;
	}

		// run hba list looking for non present and instance path match or path wildcard (first insertion)
	for( hba = TAILQ_FIRST( &sdmmc_ctrl.hlist ); hba; hba = TAILQ_NEXT( hba, hlink ) ) {
		ext	= (SIM_SDMMC_EXT *)hba->ext;
		if( !( ext->eflags & SDMMC_EFLAG_PRESENT ) ) {
			if( ( ext->instance.path == SDIO_CONNECT_WILDCARD ) || ( ext->instance.path == instance->path ) ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s: reuse 0x%p", __FUNCTION__, hba );
				break;
			}
		}
	}

	if( hba == NULL ) {
		hba = sdmmc_alloc_hba( );
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s: new 0x%p", __FUNCTION__, hba );
	}

	if( hba ) {
		if( sdmmc_attach( hba, connection, instance ) != CAM_SUCCESS ) {
			sdmmc_detach( hba, CAM_FALSE );
		}
	}
}

static void sdmmc_sdio_removal( struct sdio_connection * const connection, sdio_device_instance_t * const instance )
{
	SIM_HBA				*hba;
	SIM_SDMMC_EXT		*ext;
	struct sdio_device	*device;

	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, sdmmc_ctrl.verbosity, 1, "%s:  ", __FUNCTION__ );

	if( ( sdmmc_ctrl.cflags & SDMMC_CFLAG_ENUMERATING ) ) {
		return;
	}

	device = sdio_device_lookup( connection, instance );
	if( device != NULL ) {
		hba = sdio_client_hdl( device );
		ext	= (SIM_SDMMC_EXT *)hba->ext;
		atomic_clr( &ext->eflags, SDMMC_EFLAG_PRESENT );
		sdmmc_detach( hba, CAM_FALSE );
	}
}

__attribute__((used))
static int sdmmc_sdio_event( struct sdio_connection * const connection, sdio_device_instance_t * const instance, const int ev )
{
	SIM_HBA				*hba;
	SIM_SDMMC_EXT		*ext;
	struct sdio_device	*device;
	int					status;

	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, sdmmc_ctrl.verbosity, 1, "%s:  ", __FUNCTION__ );

	status = EOK;

	device = sdio_device_lookup( connection, instance );
	if( device != NULL ) {
		hba = sdio_client_hdl( device );
		ext	= (SIM_SDMMC_EXT *)hba->ext;
		switch( ev ) {
			case SDIO_EVENT_RESET:
				if( ( ext->dev_inf.caps & DEV_CAP_ASSD ) ) {
					atomic_set( &ext->eflags, SDMMC_EFLAG_ASSD_INIT );
				}
				break;

			default:
				break;
		}
	}
	return( status );
}

static int sdmmc_post_ccb( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	SIM_SDMMC_EXT	*ext;
	struct timespec	ts;

	ext = (SIM_SDMMC_EXT *)hba->ext;

	clock_gettime( CLOCK_MONOTONIC, &ts );
	ext->pm_timestamp = timespec2nsec( &ts );

	ext->nexus = NULL;

#ifdef SDMMC_TRACE
	sdmmc_trace_event( SDMMC_TRACE_EVENT, "%s:  ccb %p", __FUNCTION__, ccb );
#endif

	if( hba->verbosity > 3 ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  ccb %p",__FUNCTION__, ccb );
	}

	simq_post_ccb( hba->simq, ccb );

	return( CAM_SUCCESS );
}

int sdmmc_sense( CCB_SCSIIO * const ccb, const uint8_t sense, const uint8_t asc, const uint8_t ascq )
{
	SCSI_SENSE			*sptr;

	if( ccb->cam_ch.cam_func_code != XPT_SCSI_IO ) {
		return( CAM_REQ_CMP_ERR );
	}

	ccb->cam_scsi_status	= SCS_CHECK;
	ccb->cam_ch.cam_status	= CAM_REQ_CMP_ERR;
	sptr					= (SCSI_SENSE *)ccb->cam_sense_ptr;

	if( ( sptr == NULL ) || ( ccb->cam_ch.cam_flags & CAM_DIS_AUTOSENSE ) ) {
		return( ccb->cam_ch.cam_status );
	}

	ccb->cam_ch.cam_status |= CAM_AUTOSNS_VALID;

	memset( sptr, 0, sizeof( *sptr ) );
	sptr->error	= 0x70;		// Error code
	sptr->sense = sense;	// Sense key
	sptr->asc	= asc;		// Additional sense code (Invalid field in CDB)
	sptr->ascq	= ascq;		// Additional sense code qualifier

	return( ccb->cam_ch.cam_status );
}

static int sdmmc_error( SIM_HBA * const hba, CCB_SCSIIO * const ccb, int status )
{
	SIM_SDMMC_EXT	*ext;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	switch( status ) {
		case EIO:
			status = sdmmc_sense( ccb, SK_MEDIUM, 0x00, 0x00 );
			break;

		case ENXIO:
			status = sdmmc_sense( ccb, SK_NOT_RDY, ASC_MEDIA_NOT_PRESENT, 0 );
			break;

		case EACCES:
			status = sdmmc_sense( ccb, SK_ILLEGAL, ASC_COPY_PROTECTION, ASCQ_READ_SCRAMBLED );
			break;

		case ENODEV:
			atomic_clr( &ext->eflags, SDMMC_EFLAG_PRESENT );
			status = CAM_NO_HBA;
			break;

		case EINVAL:
			status = sdmmc_sense( ccb, SK_ILLEGAL, ASC_INVALID_FIELD, 0 );
			break;

		case EROFS:
			status = sdmmc_sense( ccb, SK_DATA_PROT, ASC_WRITE_PROTECTED, ASCQ_VU_WRITE_PROTECTED );
			break;

		case ETIMEDOUT:
			status = CAM_CMD_TIMEOUT;
			break;

		default:
			status = CAM_REQ_CMP_ERR;
			break;
	}

	return( status );
}

int sdmmc_unit_ready( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	SIM_SDMMC_EXT	*ext;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= CAM_REQ_CMP;

	if( !( ext->eflags & SDMMC_EFLAG_PRESENT ) ) {
		status = sdmmc_error( hba, ccb, ENODEV );
	}

	if( !ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun].nlba ) {
		status = sdmmc_error( hba, ccb, ENXIO );
	}

	return( status );
}

static int sdmmc_mpage_rw_err( SIM_HBA * const hba, CCB_SCSIIO * const ccb, const int op )
{
	SIM_SDMMC_EXT		*ext;
	MODE_PARM_HEADER10	*mph;
	int					status;

	ext = (SIM_SDMMC_EXT *)hba->ext;
	mph	= CAM_DATA_PTR_V( ccb->cam_data.cam_data_ptr );
	memset( mph, 0, sizeof( MODE_PARM_HEADER10 ) );

	if( op ) {
		status = sdmmc_sense( ccb, SK_ILLEGAL, ASC_INVALID_FIELD, 0 );
	}
	else {
		if( ( ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun].pflags & SDMMC_PFLAG_WP ) ) {
			mph->device_specific |= MP_DS_WP;
		}
		status = CAM_REQ_CMP;
	}

	return( status );
}

static int sdmmc_mpage_caching( SIM_HBA * const hba, CCB_SCSIIO * const ccb, const int op )
{
	SIM_SDMMC_EXT		*ext;
	MPAGE_CACHING		*mpc;
	MODE_PARM_HEADER10	*mph;
	int			status;

	ext = (SIM_SDMMC_EXT *)hba->ext;
	mph	= CAM_DATA_PTR_V( ccb->cam_data.cam_data_ptr );
	mpc	= (MPAGE_CACHING *)(mph + 1 );

	if( op ) {
		if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) ) {
			status = sdio_cache( ext->device, ( mpc->flags & MP_CACHE_WCE ) ? SDIO_CACHE_ENABLE : SDIO_CACHE_DISABLE, SDIO_TIME_DEFAULT );
			if( status == EOK ) {
				status		= CAM_REQ_CMP;
				ext->eflags	&= ~SDMMC_EFLAG_CACHE;
				if( ( mpc->flags & MP_CACHE_WCE ) ) {
					ext->eflags	|= SDMMC_EFLAG_CACHE;
				}
			}
			else {
				status = sdmmc_error( hba, ccb, status );
			}
		}
		else {
			status = sdmmc_error( hba, ccb, EINVAL );
		}

	}
	else {
		memset( mph, 0, ccb->cam_dxfer_len );
		mph->data_len[1]	= 26;
		mpc->pc_page		= MP_CACHING;
		mpc->page_length	= 18;
		mpc->flags			= ( ext->dev_inf.caps & DEV_CAP_CACHE ) ? MP_CACHE_WCE : 0;
		status				= CAM_REQ_CMP;
	}

	return( status );
}

static int sdmmc_mode_select( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	MPAGE_CACHING		*mp;
	MODE_PARM_HEADER10	*mph;
	int					status;

	mph	= CAM_DATA_PTR_V( ccb->cam_data.cam_data_ptr );
	mp	= (MPAGE_CACHING *)(mph + 1 );

	switch( mp->pc_page ) {
		case MP_CACHING:
			status = sdmmc_mpage_caching( hba, ccb, CAM_TRUE );
			break;

		default:
			status = sdmmc_error( hba, ccb, EINVAL );
			break;
	}

	return( status );
}

static int sdmmc_mode_sense( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	int				status;

	status = sdmmc_unit_ready( hba, ccb );
	if( status != CAM_REQ_CMP ) {
		return( status );
	}

	switch( ccb->cam_cdb_io.cam_cdb_bytes[2] ) {
		case MP_RW_ERR:
			status = sdmmc_mpage_rw_err( hba, ccb, CAM_FALSE );
			break;

		case MP_CACHING:
			status = sdmmc_mpage_caching( hba, ccb, CAM_FALSE );
			break;

		default:
			status = sdmmc_error( hba, ccb, EINVAL );
			break;
	}

	return( status );
}

static int sdmmc_evpd_inquiry( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	SIM_SDMMC_EXT					*ext;
	VPD_HEADER						*vh;
	BLOCK_LIMITS					*bl;
	BLOCK_DEVICE_CHARACTERISTICS	*bdc;
	uint8_t							*vpd;
	uint16_t						lps;
	int								cstatus;

	ext	= (SIM_SDMMC_EXT *)hba->ext;

	vpd	= CAM_DATA_PTR_V( ccb->cam_data.cam_data_ptr );
	vh	= (VPD_HEADER *)vpd;
	bl	= (BLOCK_LIMITS *)(vh+1);
	bdc	= (BLOCK_DEVICE_CHARACTERISTICS *)(vh+1);
	memset( vpd, 0, ccb->cam_dxfer_len );

	cstatus = CAM_REQ_CMP;

	switch( ccb->cam_cdb_io.cam_cdb_bytes[2] ) {
		case INQ_PC_VPD_SUPPORTED: {
			uint8_t		evp_sup[] = { D_DIR_ACC, INQ_PC_VPD_SUPPORTED, 0, 3,
							INQ_PC_VPD_SUPPORTED, INQ_PC_BLOCK_LIMITS,
							INQ_PC_BLOCK_DEVICE_CHARACTERISTICS };

			if( ccb->cam_dxfer_len < sizeof( evp_sup ) ) {
				return( sdmmc_error( hba, ccb, EINVAL ) );
			}
			memcpy( vpd, evp_sup, sizeof( evp_sup ) );
			break;
		}

		case INQ_PC_BLOCK_LIMITS:
			if( ccb->cam_dxfer_len < ( sizeof( *vh ) + sizeof( *bl ) ) ) {
				return( sdmmc_error( hba, ccb, EINVAL ) );
			}
			vh->peripheral		= D_DIR_ACC;
			vh->page_code		= INQ_PC_BLOCK_LIMITS;
			UNALIGNED_PUT16( vh->page_length, ENDIAN_BE16( 60 ) );

			lps = 1;
			UNALIGNED_PUT16( bl->optimal_xfer_len_granularity, ENDIAN_BE16( lps ) );

			if( ( ext->dev_inf.caps & ( DEV_CAP_TRIM | DEV_CAP_DISCARD ) ) ) {
				UNALIGNED_PUT32( bl->max_unmap_lba_count, ENDIAN_BE32( SDMMC_TRIM_MAX_LBA ) );
				UNALIGNED_PUT32( bl->max_unmap_desc_count, ENDIAN_BE32( 1 ) );
				UNALIGNED_PUT32( bl->optimal_unmap_granularity, ENDIAN_BE32( 1 ) );
			}
			break;

		case INQ_PC_BLOCK_DEVICE_CHARACTERISTICS:
			if( ccb->cam_dxfer_len < ( sizeof( *vh ) + sizeof( *bdc ) ) ) {
				return( sdmmc_error( hba, ccb, EINVAL ) );
			}
			vh->peripheral				= D_DIR_ACC;
			vh->page_code				= INQ_PC_BLOCK_DEVICE_CHARACTERISTICS;
			UNALIGNED_PUT16( vh->page_length, ENDIAN_BE16( 60 ) );

			UNALIGNED_PUT16( bdc->medium_rotation_rate, ENDIAN_BE16( 1 ) );		// 1 indicates non rotating
			bdc->nominal_form_factor	= 0;
			break;

		default:
			return( sdmmc_error( hba, ccb, EINVAL ) );
			break;
	}

	return( cstatus );
}

static int sdmmc_std_inquiry( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	SIM_SDMMC_EXT	*ext;
	SCSI_INQUIRY	*iptr;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	iptr	= CAM_DATA_PTR_V( ccb->cam_data.cam_data_ptr );

	memset( iptr, 0, sizeof( *iptr ) );

	iptr->peripheral	= D_DIR_ACC;
	iptr->rmb			= ( ext->hc_inf.caps & HC_CAP_SLOT_TYPE_EMBEDDED ) ? CAM_FALSE : CAM_TRUE;
	iptr->version		= INQ_VER_SPC3;		// SPC-3
	iptr->adlen			= 96 - 5;	// nbytes after adlen field

	strlcpy( (char *)iptr->vend_id, "SDMMC:", sizeof( iptr->vend_id ) );
	strlcpy( (char *)iptr->prod_id, (char *)ext->dev_inf.pnm, sizeof( iptr->prod_id ) );

	if( ( ext->dev_inf.prv >> 4 ) < 10 ) {
		iptr->prod_rev[0] = (uchar_t)( ( ext->dev_inf.prv >> 4 ) + '0' );
	}
	else {
		iptr->prod_rev[0] = (uchar_t)( ( ext->dev_inf.prv >> 4 ) - 10 + 'a' );
	}

	iptr->prod_rev[1] = '.';

	if( ( ext->dev_inf.prv & 0x0F) < 10 ) {
		iptr->prod_rev[2] = (uchar_t)( ( ext->dev_inf.prv & 0x0F ) + '0' );
	}
	else {
		iptr->prod_rev[2] = (uchar_t) ( ( ext->dev_inf.prv & 0x0F ) - 10 + 'a' );
	}

	if( ccb->cam_dxfer_len > 56 ) {			// check len for vend_spc
		strlcpy( (char *)iptr->vend_spc, ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun].name, sizeof( iptr->vend_spc ) );
	}

	return( CAM_REQ_CMP );
}

static int sdmmc_inquiry( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	int				status;

	if( ( ccb->cam_cdb_io.cam_cdb_bytes[1] & INQ_OPT_EVPD ) ) {
		status = sdmmc_evpd_inquiry( hba, ccb );
	}
	else {
		status = sdmmc_std_inquiry( hba, ccb );
	}

	return( status );
}

static int sdmmc_spindle( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	int				status;

	status = sdmmc_unit_ready( hba, ccb );
	if( status != CAM_REQ_CMP ) {
		return( status );
	}

	switch( ccb->cam_cdb_io.cam_cdb_bytes[4] ) {
		case LD_CMD_STOP:
		case LD_CMD_START:
			status	= CAM_REQ_CMP;
			break;

		case LD_CMD_EJECT:
		case LD_CMD_LOAD:
		default:
			status = sdmmc_error( hba, ccb, EINVAL );
			break;
	}
	return( status );
}

static int sdmmc_capacity( SIM_HBA * const hba, CCB_SCSIIO *ccb, const int action )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_TARGET	*targ;
	READ_CAPACITY	*cp;
	READ_CAPACITY16	*cp16;
	uint64_t		lba;
	int				status;
	void			*dptr;
	uint32_t		la;
	uint32_t		lps;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	dptr	= CAM_DATA_PTR_V( ccb->cam_data.cam_data_ptr );
	targ	= &ext->targets[ccb->cam_ch.cam_target_id];

		// the capacity returned is the last addressable lba, thus the nlba - 1
	lba	= targ->partitions[ccb->cam_ch.cam_target_lun].nlba - 1;

	status = sdmmc_unit_ready( hba, ccb );
	if( status != CAM_REQ_CMP ) {
		return( status );
	}

	ccb->cam_ch.cam_status	= CAM_REQ_CMP;
	ccb->cam_scsi_status	= SCS_GOOD;

	if( action == SAI_READ_CAPACITY_16 ) {
		cp16			= dptr;
		cp16->lba		= ENDIAN_BE64( lba );				// Returned logical block address
		cp16->blk_size	= ENDIAN_BE32( targ->blksz );

		la	= 0;
		lps	= 0;
		cp16->exponents		= lps;
		cp16->tp_lalba[0]	= la >> 8;
		cp16->tp_lalba[1]	= la;
		if( ( ext->dev_inf.caps & DEV_CAP_TRIM ) ) {
			cp16->tp_lalba[0] |= RD_CAP_TPE;
		}
	}
	else {
		cp				= dptr;
		if( lba >= RD_CAP_MAX_LBA32 ) {
			cp->lba		= ENDIAN_BE32( RD_CAP_MAX_LBA32 );	// Returned logical block address
		}
		else {
			cp->lba		= ENDIAN_BE32( lba );				// Returned logical block address
		}
		cp->blk_size	= ENDIAN_BE32( targ->blksz );
	}

	return( status );
}

static int sdmmc_write_same( SIM_HBA * const hba, CCB_SCSIIO *ccb )
{
	SIM_SDMMC_EXT	*ext;
	CDB				*cdb;
	SDMMC_PARTITION	*part;
	uint64_t		lba;
	uint32_t		nlba;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	cdb		= cam_cdb( ccb );
	status	= CAM_REQ_CMP;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	lba		= ENDIAN_BE64( UNALIGNED_RET64( cdb->write_same16.lba ) );
	nlba	= ENDIAN_BE32( UNALIGNED_RET32( cdb->write_same16.transfer_len ) );
	lba		+= part->slba;

	status = sdmmc_unit_ready( hba, ccb );
	if( status != CAM_REQ_CMP ) {
		return( status );
	}

	if( !( ext->dev_inf.caps & DEV_CAP_TRIM ) || !( cdb->write_same16.opt & WS_OPT_UNMAP ) ) {
		status = sdmmc_error( hba, ccb, EINVAL );
	}
	else if( ( status = sdio_erase( ext->device, part->config, MMC_ERASE_TRIM, lba, nlba ) ) ) {
		status = sdmmc_error( hba, ccb, status );
	}
	else {
		// nothing
	}

	if( status == EOK ) {
		part->tc += nlba;
	}

	return( status ? status : CAM_REQ_CMP );
}

static int sdmmc_erase12( SIM_HBA * const hba, CCB_SCSIIO *ccb )
{
	SIM_SDMMC_EXT	*ext;
	CDB				*cdb;
	SDMMC_PARTITION	*part;
	uint32_t		lba;
	uint32_t		nlba;
	uint32_t		egs;
	uint32_t		pend;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	cdb		= cam_cdb( ccb );
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];

	egs		= ext->dev_inf.erase_size / 512;
	lba		= ENDIAN_BE32( UNALIGNED_RET32( cdb->erase12.lba ) );
	nlba	= ENDIAN_BE32( UNALIGNED_RET32( cdb->erase12.transfer_len ) );
	lba		+= part->slba;
	pend	= part->slba + part->nlba - 1;

	status = sdmmc_unit_ready( hba, ccb );
	if( status == CAM_REQ_CMP ) {
		if( ( lba > pend ) || ( ( lba + nlba - 1 ) > pend ) ||
				( lba % egs ) || ( nlba % egs ) ) {
			status = sdmmc_error( hba, ccb, EINVAL );
		}
		else if( ( status = sdio_erase( ext->device, part->config, MMC_ERASE_SECURE, lba, nlba ) ) ) {
			status = sdmmc_error( hba, ccb, status );
		}
		else {
			// nothing
		}
	}

	if( status == EOK ) {
		part->ec += nlba;
	}

	return( status ? status : CAM_REQ_CMP );
}

static int sdmmc_ffu( SIM_HBA * const hba, CCB_SCSIIO *ccb )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_PARTITION	*part;
	sdio_ecsd_t		*ecsd;
	CDB				*cdb;
	uint32_t		sgc;
	sdio_sge_t		*sgp;
	sdio_sge_t		sge;
	uint32_t		flgs;
	int				status;
	uint8_t			*rawecsd;
	uint16_t		dssize;		// firmware download data sector size

	flgs	= SCF_DIR_OUT;
	cdb		= cam_cdb( ccb );
	ext		= (SIM_SDMMC_EXT *)hba->ext;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	ecsd	= sdio_get_ecsd( ext->device );
	rawecsd = sdio_get_raw_ecsd( ext->device );

	if( ( ext->instance.ident.dtype != DEV_TYPE_MMC ) || ( rawecsd[ECSD_REV] < ECSD_REV_V5 ) ||
			!( rawecsd[ECSD_SUPPORTED_MODES] & ECSD_FFU_SUPPORTED ) ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: the device doesn't support FFU!", __func__ );
		return( ENOTSUP );
	}

	if( rawecsd[ECSD_FW_CONFIG] & ECSD_FWUPDATE_DISABLED ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: FFU is disabled!", __func__ );
		return EPERM;
	}

	// Only support 0x0F(active microcode) and 0x04(download microcode)
	if( ( cdb->write_buffer.mode != WB_MODE_DNLD_MC ) && ( cdb->write_buffer.mode != WB_MODE_ACT_MC ) ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: Invalid FFU mode:%d!", __func__, cdb->write_buffer.mode );
		return( EINVAL );
	}

	status = sdmmc_unit_ready( hba, ccb );
	if( status != CAM_REQ_CMP ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: Unit not ready!", __func__ );
		return( EBUSY );
	}

	// Active firmware
	if( cdb->write_buffer.mode == WB_MODE_ACT_MC ) {
		if( ( rawecsd[ECSD_FFU_FEATURE] & ECSD_FFU_OPCODE_SUPPORTED ) ) {
			status = sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT,
						MMC_SWITCH_MODE_WRITE, ECSD_MODE_OPERATION_CODES, ECSD_FFU_INSTALL, SDIO_TIME_DEFAULT );
		}
		else {
			status = sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT,
						MMC_SWITCH_MODE_WRITE, ECSD_MODE_CONFIG, ECSD_MODE_NORMAL, SDIO_TIME_DEFAULT );
		}

		sdmmc_reset( hba );

		return( status ? status : CAM_REQ_CMP );
	}

	// Firmware download
	// Enter FFU mode for the first data chunk
	if( cdb->write_buffer.buffer_id == 0 ) {
		// Flush cache
		if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) && ( ext->eflags & SDMMC_EFLAG_CACHE ) ) {
			status = sdio_cache( ext->device, SDIO_CACHE_FLUSH, SDIO_TIME_DEFAULT * 5 );
			if( status != EOK ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Cache Flush", __FUNCTION__ );
				return( status );
			}
		}

		// Switch off Device BKOPS (eMMC 5.1)
		if( ( ext->eflags & SDMMC_EFLAG_BKOPS_AUTO ) ) {
			status = sdmmc_bkops_cfg( hba, CAM_FALSE );
			if( status != EOK ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Error Disabling auto BKOPS", __FUNCTION__ );
				return( status );
			}
		}

		// Enter FFU mode
		status = sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT,
						MMC_SWITCH_MODE_WRITE, ECSD_MODE_CONFIG, ECSD_MODE_FFU, SDIO_TIME_DEFAULT );
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Enter FFU mode failed", __FUNCTION__ );
			return( status );
		}
	}

		// Download firmware
	dssize = ( rawecsd[ECSD_DATA_SECTOR_SIZE] == ECSD_NATIVE_SECTOR_4K ) ? 4096 : 512;
	if( ( ccb->cam_dxfer_len == 0 ) || ( ( ccb->cam_dxfer_len % dssize ) != 0 ) ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: Invalid data length %d", __func__, ccb->cam_dxfer_len );
		return( EINVAL );
	}

	// Set firmware download sector size
	if( ext->dev_inf.sector_size != dssize ) {
		status =  sdio_set_block_length( ext->device, dssize );
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: Failed to set block length %d", __func__, dssize );
			return( EIO );
		}
		ext->dev_inf.sector_size = dssize;
	}

	if( ( ccb->cam_ch.cam_flags & CAM_SCATTER_VALID ) ) {
		sgc				= ccb->cam_sglist_cnt;
		sgp				= (sdio_sge_t *)ccb->cam_data.cam_sg_ptr;
	}
	else {
		sgc				= 1;
		sgp				= &sge;
		sgp->sg_count	= ccb->cam_dxfer_len;
		sgp->sg_address	= ccb->cam_data.cam_data_ptr;
	}

	if( ( ccb->cam_ch.cam_flags & CAM_DATA_PHYS ) ) {
		flgs |= SCF_DATA_PHYS;
	}

	// Write firmware data
	status = sdmmc_rw( hba, part, flgs, UNALIGNED_RET32( &rawecsd[ECSD_FFU_ARG] ),
							ccb->cam_dxfer_len, sgp, sgc, ccb->cam_req_map, ccb->cam_timeout );

	ext->dev_inf.sector_size = ecsd->blksz;

	return( status ? status : CAM_REQ_CMP );
}

static int sdmmc_cache_flush( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT	*ext;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= EOK;

	if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) && ( ext->eflags & SDMMC_EFLAG_CACHE ) && ( ext->eflags & SDMMC_EFLAG_VCACHE_DIRTY ) ) {
		status = sdio_cache( ext->device, SDIO_CACHE_FLUSH, SDIO_TIME_DEFAULT * 5 );
		if( status == EOK ) {
			ext->eflags &= ~SDMMC_EFLAG_VCACHE_DIRTY;		// clear dirty flag
		}
	}
	return( status );
}

static int sdmmc_sync( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	SIM_SDMMC_EXT	*ext;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	if( !( ext->eflags & SDMMC_EFLAG_PRESENT ) ) {
		return( CAM_REQ_CMP );
	}

	status = sdmmc_unit_ready( hba, ccb );
	if( status == CAM_REQ_CMP ) {
		if( ( status = sdmmc_cache_flush( hba ) ) ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s:  sdmmc_cache_flush Failure %d", __FUNCTION__, status );
		}
	}

	return( status ? status : CAM_REQ_CMP );
}

static int sdmmc_timer_settime( const int tid, const uint64_t nsec, const int repeat )
{
	struct timespec		ts;
	struct itimerspec	value;

	memset( &value, 0, sizeof( value ) );

	if( nsec ) {
		nsec2timespec( &ts, nsec );
		value.it_value = ts;
		if( repeat ) {
			value.it_interval.tv_sec	= ts.tv_sec;
			value.it_interval.tv_nsec	= ts.tv_nsec;
		}
	}

	if( timer_settime( tid, 0, &value, NULL ) == -1 ) {
		return( errno );
	}

	return( EOK );
}

static int sdmmc_pm( SIM_HBA * const hba, uint32_t op )
{
	SIM_SDMMC_EXT	*ext;

	ext = (SIM_SDMMC_EXT *)hba->ext;

	if( ext->pm_state == op ) {
		return( EOK );
	}

#if 0
{
	static const char	*name[4] = { "idle", "active", "sleep", "suspend" };
	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s: %s", __FUNCTION__, name[op] );
}
#endif

	switch( op ) {
		case PM_IDLE:
			sdio_pwrmgnt( ext->device, PM_IDLE );
			if( ( ext->hc_inf.caps & HC_CAP_SLEEP ) ) {
				sdmmc_timer_settime( ext->pm_timerid, ext->pm_sleep_time_ns, CAM_FALSE );
			}
			else {
				sdmmc_timer_settime( ext->pm_timerid, 0, CAM_FALSE );
			}
			break;

		case PM_SLEEP:
			sdmmc_timer_settime( ext->pm_timerid, 0, CAM_FALSE );
			if( sdio_pwrmgnt( ext->device, PM_SLEEP ) ) {			// sleep failed, re-arm timer, back to idle
				op = PM_IDLE;
				sdmmc_timer_settime( ext->pm_timerid, ext->pm_sleep_time_ns, CAM_FALSE );
			}
			break;

		case PM_ACTIVE:
			if( ( ext->pm_state == PM_SLEEP ) || ( ext->pm_state == PM_SUSPEND ) ) {
				if( ( ext->dev_inf.caps & DEV_CAP_ASSD ) ) {
					atomic_set( &ext->eflags, SDMMC_EFLAG_ASSD_INIT );
				}
			}
			sdmmc_timer_settime( ext->pm_timerid, ext->pm_idle_time_ns, CAM_TRUE );
			sdio_pwrmgnt( ext->device, PM_ACTIVE );
			break;

		case PM_SUSPEND:
			sdmmc_timer_settime( ext->pm_timerid, 0, CAM_FALSE );	// stop PM timer
			sdio_pwrmgnt( ext->device, PM_SUSPEND );
			break;

		default:
			return( EINVAL );
	}

	ext->pm_state = op;

	return( EOK );
}

static int sdmmc_set_partition_attr( SIM_HBA * const hba, const uint32_t partition, const uint32_t flags )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_TARGET		*targ;
	SDMMC_PARTITION		*part;
	uint32_t			pidx;
	uint32_t			tidx;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	for( tidx = 0; tidx <= ext->ntargs; tidx++ ) {
		targ = &ext->targets[tidx];
		for( pidx = 0; pidx <= targ->nluns; pidx++ ) {
			part			= &targ->partitions[pidx];
			if( ( part->config & MMC_PART_MSK ) == partition ) {
				part->pflags |= flags;
			}
		}
	}
	return( CAM_SUCCESS );
}

int sdmmc_reset( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT	*ext;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	sdio_reset( ext->device );

	if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) && ( ext->eflags & SDMMC_EFLAG_CACHE ) ) {
			// Mark device user partition as read only/write protected after a reset when eMMC cache is enabled.
			// This is done since we don't know if there was pending data in the eMMC cache.
		if( ( ext->eflags & SDMMC_EFLAG_VCACHE_DIRTY ) ) {
			sdmmc_set_partition_attr( hba, MMC_PART_USER, SDMMC_PFLAG_WP );
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  sdio_reset cache lost - device marked read only", __FUNCTION__ );
		}
		else {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  sdio_reset - disabling volatile cache", __FUNCTION__ );
		}
		ext->eflags	&= ~SDMMC_EFLAG_CACHE;
	}

	sdmmc_dev_cfg( hba );

	return( EOK );
}

int sdmmc_bkops_cfg( SIM_HBA * const hba, const int op )
{
	SIM_SDMMC_EXT	*ext;
	int				status;
	int				bkop;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	bkop	= 0;

	if( !( ext->dev_inf.caps & DEV_CAP_BKOPS ) ) {
		return( ENOTSUP );
	}

	if( ( ext->eflags & SDMMC_EFLAG_BKOPS ) ) {
			// set manual bkops if enabled (WARNING: permanent setting)
		bkop = ECSD_BKOPS_ENABLE;
	}

	if( op ) {
		if( ( ext->dev_inf.caps & DEV_CAP_BKOPS_AUTO ) && ( ext->eflags & SDMMC_EFLAG_BKOPS_AUTO ) ) {
			bkop |= ECSD_BKOPS_AUTO_EN;			// set bkops auto
		}
	}

	status = sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT, MMC_SWITCH_MODE_WRITE, ECSD_BKOPS_EN, bkop, SDIO_TIME_DEFAULT );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: sdio_mmc_switch ECSD_BKOPS_EN - 0x%x, status %d", __FUNCTION__, bkop, status );
	}

	return( status );
}

int sdmmc_bkops( SIM_HBA * const hba, const int tick )
{
    SIM_SDMMC_EXT   *ext;
    int             status = EOK;
    uint8_t         ecsd[MMC_EXT_CSD_SIZE];

    ext     = (SIM_SDMMC_EXT *)hba->ext;

    if( !( ext->eflags & SDMMC_EFLAG_BKOPS ) || ( ext->drvr_state == SDMMC_DRVR_PAUSE ) ) {
        return( EOK );
    }

	if( ( ext->bkops_status & BKOPS_STATUS_OPERATIONS_INPROG ) ) {
		sdmmc_pm( hba, PM_ACTIVE );
		if( !tick ) {
			// wait up to 1/2 second to complete
			status = sdio_hpi( ext->device, 500 );
			if( status ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s(%d): HPI failure", __FUNCTION__, __LINE__ );
				sdmmc_reset( hba );
			}
			ext->bkops_status = BKOPS_STATUS_OPERATIONS_NONE;
		}
		return( status );
	}

	if( tick ) {                            // Timer event, poll status
		sdmmc_pm( hba, PM_ACTIVE );

		if( ext->bkops_status ) {
			ext->bkops_ticks++;
		}

		if( sdio_send_ext_csd( ext->device, ecsd ) == EOK ) {
			if( ext->bkops_status != ecsd[ECSD_BKOPS_STATUS] ) {
				ext->bkops_ticks    = 0;
				ext->bkops_status   = ecsd[ECSD_BKOPS_STATUS];
			}
		}
	}

	switch( ext->bkops_status ) {
		case BKOPS_STATUS_OPERATIONS_NONE:
			break;

		case BKOPS_STATUS_OPERATIONS_NON_CRITICAL:
			if( ext->bkops_ticks < BKOPS_NC_TICKS ) {
				break;
			}

						// fall through

		case BKOPS_STATUS_OPERATIONS_IMPACTED:
			if( ext->bkops_ticks < BKOPS_IMPACTED_TICKS ) {
				break;
			}

						// fall through

		case BKOPS_STATUS_OPERATIONS_CRITICAL:
			// Should we wait when critial is detected via timer tick?
			//         if( ext->bkops_status == BKOPS_STATUS_OPERATIONS_CRITICAL ) {
			//             tick = 0;
			//         }

			// We could make the completion time shorter and use hpi.  Will the chip take new requests while critical?
			status = sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT, MMC_SWITCH_MODE_WRITE,
					ECSD_BKOPS_START, ECSD_BKOPS_INITIATE, tick ? 0 : SDMMC_TIME_BKOPS );
			if( status != EOK ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s(%d): BKOPS switch failed status 0x%x", __FUNCTION__, __LINE__, status );
				if( status == ETIMEDOUT ) {
					if( ( status = sdio_hpi( ext->device, 0 ) ) ) {
						cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s(%d): HPI failure status 0x%x", __FUNCTION__, __LINE__, status );
						sdmmc_reset( hba );
					}
				}
				else {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s(%d): BKOPS_START failure status 0x%x", __FUNCTION__, __LINE__, status );
				}
			}
			else {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s(%d): BKOPS_START BKOPS_STATUS_OPERATIONS_INPROG", __FUNCTION__, __LINE__ );
				ext->bkops_status   |= BKOPS_STATUS_OPERATIONS_INPROG;
				ext->bkops_ticks    = 0;
			}
			break;

		default:
			break;
	}

	return( EOK );
}

static int sdmmc_write_protect( SIM_HBA * const hba, const uint32_t op, const uint32_t partition, const uint32_t mode, uint64_t lba, uint32_t nlba, uint64_t *prot )
{
	SIM_SDMMC_EXT		*ext;
	struct sdio_device	*dev;
	struct sdio_cmd		*cmd;
	sdio_sge_t			sge;
	uint8_t				*ecsd;
	int					sz;
	int					blksz;
	int					status;
	void				*pdata;
	uint8_t				pmode;
	uint64_t			addr;
	uint64_t			wp_grp_size;

	ext			= (SIM_SDMMC_EXT *)hba->ext;
	dev			= ext->device;
	blksz		= ext->dev_inf.sector_size;
	wp_grp_size = ext->dev_inf.wp_size / 512;
	sz			= MMC_SEND_WRITE_PROT_SIZE;
	ecsd		= sdio_get_raw_ecsd( ext->device );

	if( !wp_grp_size ) {
		return( ENOTSUP );
	}

	if( ( lba % wp_grp_size ) ) {
		return( EINVAL );
	}

	status = sdio_set_partition( dev, partition );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: sdio_set_partition failure %s", __FUNCTION__, strerror( status ) );
		return( status );
	}

	if( op == MMC_SET_WRITE_PROT ) {
		switch( ( partition & MMC_PART_MSK ) ) {
			case MMC_PART_USER:
				if( ( mode & SDMMC_WP_MODE_PWR_WP_EN ) ) {	// Power-on
					pmode = ecsd[ECSD_USER_WP] & ~( ECSD_USER_WP_US_PWR_WP_EN | ECSD_USER_WP_US_PERM_WP_EN | ECSD_USER_WP_US_PWR_WP_DIS );
					pmode |= mode;
				}
				else {									// Temporary
					pmode = ecsd[ECSD_USER_WP] & ~( ECSD_USER_WP_US_PWR_WP_EN | ECSD_USER_WP_US_PERM_WP_EN );
				}

				status = sdio_mmc_switch( dev, MMC_SWITCH_CMDSET_DFLT, MMC_SWITCH_MODE_WRITE, ECSD_USER_WP, pmode, SDIO_TIME_DEFAULT );
				if( status != EOK ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: switch ext_csd_user_wp mode %d, user_wp 0x%x", __FUNCTION__, pmode, ecsd[ECSD_USER_WP] );
					return( status );
				}
				break;

			case MMC_PART_BOOT1:
			case MMC_PART_BOOT2:
				status = sdio_mmc_switch( dev, MMC_SWITCH_CMDSET_DFLT, MMC_SWITCH_MODE_WRITE, ECSD_BOOT_WP, mode, SDIO_TIME_DEFAULT );
				if( status != EOK ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: switch ext_csd_boot_wp", __FUNCTION__ );
					return( status );
				}
				break;

			default:
				return( EINVAL );
		}
	}

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	switch( op ) {
		case MMC_SET_WRITE_PROT:
		case MMC_CLR_WRITE_PROT:
			while( nlba ) {
				addr = ( ( ext->dev_inf.caps & DEV_CAP_HC ) != 0 ) ? lba : ( lba * blksz );
				sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1B | SCF_WAIT_DRDY, op, (uint32_t)addr );
				status = sdio_send_cmd( dev, cmd, NULL, SDMMC_TIME_DEFAULT * 1000, 0 );
				if( status != EOK ) {
					break;
				}

				lba		+= wp_grp_size;
				nlba	-= min( nlba, wp_grp_size );
			}
			break;

		case MMC_SEND_WRITE_PROT:
		case MMC_SEND_WRITE_PROT_TYPE:
			addr = ( ( ext->dev_inf.caps & DEV_CAP_HC ) != 0 ) ? lba : ( lba * blksz );
			if( ( op == MMC_SEND_WRITE_PROT_TYPE ) && ( ext->dev_inf.spec_rev >= ECSD_REV_V4_41 ) ) {
				sz = MMC_SEND_WRITE_PROT_TYPE_SIZE;
			}

			pdata = sdio_alloc( sz );
			if( pdata == NULL ) {
				status = ENOMEM;
				break;
			}

			sge.sg_count	= sz;
			sge.sg_address	= CAM_DATA_PTR_P( pdata );
			sdio_setup_cmd( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1, op, (uint32_t)addr );
			sdio_setup_cmd_io( cmd, SCF_DIR_IN, 1, sz, &sge, 1, NULL );
			status = sdio_send_cmd( dev, cmd, NULL, SDMMC_TIME_DEFAULT * 1000, 0 );
			if( status == EOK ) {
				*prot = ( sz == 4 ) ?	ENDIAN_BE32( *(uint32_t *)pdata ) :
										ENDIAN_BE64( *(uint64_t *)pdata );
			}

			sdio_free( pdata, sz );
			break;

		default:
			status = EINVAL;
			break;
	}

	sdio_free_cmd( cmd );

	return( status );
}

int sdmmc_rw( SIM_HBA * const hba, SDMMC_PARTITION *part, uint32_t flgs, uint64_t addr, const uint32_t dlen, sdio_sge_t * const sgl, const uint32_t sgc, void * const mhdl, uint32_t timeout )
{
	SIM_SDMMC_EXT		*ext;
	struct sdio_cmd		*cmd;
	sdio_dev_info_t		*di;
	struct sdio_device	*dev;
	int					rst;
	uint32_t			op;
	uint32_t			blks;
	uint32_t			blksz;
	int					status;
	int					bus_err;
	uint32_t			cstatus;
	uint32_t			rsp[4];

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	dev		= ext->device;
	di		= &ext->dev_inf;

	rst		= CAM_FALSE;
	bus_err	= CAM_FALSE;
	timeout	*= 1000;
	blksz	= di->sector_size;
	blks	= dlen / blksz;
	addr	= ( ( di->caps & DEV_CAP_HC ) != 0 ) ? addr : ( addr * blksz );
	op		= ( flgs & SCF_DIR_IN ) ? MMC_READ_SINGLE_BLOCK : MMC_WRITE_BLOCK;

	if( dlen > blksz ) {
		if( !( ext->hc_inf.caps & HC_CAP_ACMD12 ) ) {
			if( ( di->caps & DEV_CAP_CMD23 ) ) {
				flgs |= SCF_SBC;
			}
		}
		flgs |= SCF_MULTIBLK;
	}

	if( ( flgs & SCF_SBC_RLW ) ) {
		flgs |= SCF_SBC | SCF_MULTIBLK;		// reliable write requires set block and write multiple commands
	}

	if( ( flgs & SCF_MULTIBLK ) ) {
		op++;
	}

	if( addr > SC_RW_MAX_LBA32 ) {
		flgs |= SCF_SUA;
	}

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd_ext( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1, op, (uint32_t)addr, (uint32_t)( addr >> 32 ) );
	sdio_setup_cmd_io( cmd, flgs, blks, blksz, sgl, sgc, mhdl );
	status = sdio_send_cmd( dev, cmd, NULL, timeout, 0 );
	sdio_cmd_status( cmd, &cstatus, rsp );
	sdio_free_cmd( cmd );

	if( status ) {
		if( status == ENXIO ) {				// card has been removed
			return( status );
		}

		if( sdio_send_status( dev, rsp, 0 ) || ( ( rsp[0] & ( CDS_READY_FOR_DATA | CDS_CUR_STATE_MSK ) ) != ( CDS_READY_FOR_DATA | CDS_CUR_STATE_TRAN ) ) ) {
			if( sdio_stop_transmission( dev, 0 ) ) {
				status = ETIMEDOUT;
			}
		}
	}
	else {
		if( ( flgs & SCF_MULTIBLK ) ) {
			if( ( !( flgs & SCF_SBC ) && ( dlen > blksz ) && !( ext->hc_inf.caps & HC_CAP_ACMD12 ) ) ) {
				if( sdio_stop_transmission( dev, 0 ) ) {
					status = ETIMEDOUT;
				}
			}
		}

		if( ( status == EOK ) && ( flgs & SCF_DIR_OUT ) ) {
			status = sdio_wait_card_status( dev, rsp, CDS_READY_FOR_DATA | CDS_CUR_STATE_MSK, CDS_READY_FOR_DATA | CDS_CUR_STATE_TRAN, timeout );
			if( status != EOK ) {
				sdio_stop_transmission( dev, 0 );
			}
		}

			// don't update counts for RPMB
		if( ( part->config & MMC_PART_MSK ) != MMC_PART_RPMB ) {
			if( ( flgs & SCF_DIR_IN ) ) {
				part->rc += blks;
			}
			else {
				part->wc += blks;
			}
		}
	}

	switch( cstatus ) {
		case CS_CMD_CMP:
			break;

		case CS_CMD_TO_ERR:
		case CS_CMD_CRC_ERR:
		case CS_CMD_END_ERR:
			status	= ETIMEDOUT;		// set timeout so CAM layer will retry
			break;


		case CS_DATA_TO_ERR:
			status	= ETIMEDOUT;
			break;

		case CS_DATA_CRC_ERR:
		case CS_DATA_END_ERR:
			bus_err	= CAM_TRUE;
			status	= ETIMEDOUT;
			break;

		default:
			status	= ETIMEDOUT;
			break;
	}

	if( rsp[0] ) {
		if( ( rsp[0] & CDS_URGENT_BKOPS ) ) {
			ext->bkops_status = ECSD_BS_OPERATIONS_CRITICAL;
		}
		if( ( rsp[0] & CDS_ERROR ) ) {
			status = EIO;
		}
		if( ( rsp[0] & CDS_WP_VIOLATION ) ) {
			status = EROFS;
		}
		if( ( rsp[0] & CDS_CARD_IS_LOCKED ) ) {
			status = EACCES;
		}
		if( ( rsp[0] & CDS_CARD_ECC_FAILED ) ) {
			status = EIO;
		}
	}

	if( status ) {
			// reset when we are not ready for data and in the transfer state
		if( rst || sdio_send_status( dev, rsp, 0 ) || ( rsp[0] & ( CDS_READY_FOR_DATA | CDS_CUR_STATE_MSK ) ) != ( CDS_READY_FOR_DATA | CDS_CUR_STATE_TRAN ) ) {
			sdmmc_reset( hba );
		}

		if( bus_err && ( ext->instance.ident.dtype == DEV_TYPE_SD ) ) {	// inform sdio layer of bus error (layer will reduce transfer mode after 2 errors).
			sdio_bus_error( ext->device );
		}

		sdio_dev_info( ext->device, &ext->dev_inf );	// device info may have been updated after bus_err or reset

		if( ( status == ETIMEDOUT ) && ( flgs & SCF_SBC ) ) {
				// some SD cards have buggy firmware and will
				// hang when pre-defined block counts are used
			if( ext->instance.ident.dtype == DEV_TYPE_SD ) {
				di->caps &= ~DEV_CAP_CMD23;
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  disable CMD23", __FUNCTION__ );
			}
		}

		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  flgs 0x%x, lba %"PRIu64", len %d, status 0x%x, cstatus 0x%x, rsp[0] 0x%x",
			__FUNCTION__, flgs, addr, dlen, status, cstatus, rsp[0] );
	}

	return( status );
}

#ifdef SDMMC_WRITE_VERIFY
int sdmmc_write_verify( SIM_HBA *hba, CCB_SCSIIO *ccb )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_PARTITION	*part;
	uint32_t		lba;
	int				status;
	int				sgc;
	sdio_sge_t		wsgp;
	sdio_sge_t		*sgp;
	sdio_sge_t		sge;
	int				verification;

	char			*rptr;
	char			*wptr;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	lba		= ENDIAN_BE32( UNALIGNED_RET32( &ccb->cam_cdb_io.cam_cdb_bytes[2] ) );

	if( part->blk_shft ) {
		lba <<= part->blk_shft;
	}

	lba		+= part->slba;

	if( ( ccb->cam_ch.cam_flags & CAM_SCATTER_VALID ) ) {
		sgc				= ccb->cam_sglist_cnt;
		sgp				= (sdio_sge_t *)ccb->cam_data.cam_sg_ptr;
	}
	else {
		sgc				= 1;
		sgp				= &sge;
		sgp->sg_count	= ccb->cam_dxfer_len;
		sgp->sg_address	= ccb->cam_data.cam_data_ptr;
	}

	verification	= CAM_SUCCESS;
	rptr			= (char *)ext->ver_vaddr;
	wsgp.sg_count	= ccb->cam_dxfer_len;
	wsgp.sg_address	= ext->ver_paddr;

	if( ccb->cam_dxfer_len > SDMMC_VER_BSIZE ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  request too large (%d)", __FUNCTION__, ccb->cam_dxfer_len );
		return( CAM_REQ_CMP );
	}

	status = sdmmc_rw( hba, part, SCF_DIR_IN | SCF_DATA_PHYS, lba, ccb->cam_dxfer_len, &wsgp, 1, NULL, ccb->cam_timeout );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  sdmmc_rw failed (%d)", __FUNCTION__, status );
		return( status );
	}
	else {
		for( ; sgc; sgc--, sgp++ ) {
			wptr = (char *)sgp->sg_address;
			if( ( ccb->cam_ch.cam_flags & CAM_DATA_PHYS ) ) {
				wptr = mmap( NULL, sgp->sg_count, PROT_READ | PROT_WRITE | PROT_NOCACHE, MAP_SHARED | MAP_PHYS, -1, sgp->sg_address );
				if( wptr == MAP_FAILED ) {
					verification = CAM_FAILURE;
					break;
				}
			}

			if( memcmp( wptr, rptr, sgp->sg_count ) ) {
				verification = CAM_FAILURE;
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  write verification failed wrote", __FUNCTION__ );
				cam_dump( wptr, sgp->sg_count );
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  write verification failed read", __FUNCTION__ );
				cam_dump( rptr, sgp->sg_count );
			}

			if( ( ccb->cam_ch.cam_flags & CAM_DATA_PHYS ) ) {
				munmap( wptr, sgp->sg_count );
			}

			rptr += sgp->sg_count;
			wptr += sgp->sg_count;
		}
	}

	if( verification != CAM_SUCCESS ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  write verification failed", __FUNCTION__ );
		status = sdmmc_error( hba, ccb, EIO );
	}

	return( status ? status : CAM_REQ_CMP );
}
#endif

static void sdmmc_ccb_lba( uint8_t * const cdb, uint64_t *lba, uint32_t *nlba, uint8_t *opt )
{
	switch( cdb[0] ) {
		case SC_READ10:
		case SC_WRITE10:
			*lba = ENDIAN_BE32( UNALIGNED_RET32( &cdb[2] ) );
			*nlba = ENDIAN_BE16( UNALIGNED_RET16( &cdb[7] ) );
			break;

		case SC_READ12:
		case SC_WRITE12:
			*lba = ENDIAN_BE32( UNALIGNED_RET32( &cdb[2] ) );
			*nlba = ENDIAN_BE32( UNALIGNED_RET32( &cdb[7] ) );
			break;

		case SC_READ16:
		case SC_WRITE16:
		case SC_WR_SAME16:
			*lba = ENDIAN_BE64( UNALIGNED_RET64( &cdb[2] ) );
			*nlba = ENDIAN_BE32( UNALIGNED_RET32( &cdb[10] ) );
			break;

		default:
			*lba = 0;
			*nlba = 0;
			break;
	}

	if( opt ) {
		*opt = cdb[1];
	}
}

int sdmmc_read_write( SIM_HBA * const hba, CCB_SCSIIO *ccb, int flgs )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_PARTITION	*part;
	uint64_t		lba;
	uint32_t		nblks;
	int				status;
	int				sgc;
	uint8_t			opt;
	sdio_sge_t		*sgp;
	sdio_sge_t		sge;
#ifdef SDMMC_SIM_RETRY
	int				retry;
#endif

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];

	status = sdmmc_unit_ready( hba, ccb );
	if( status != CAM_REQ_CMP ) {
		return( status );
	}

	if( ( ext->dev_inf.flags & DEV_FLAG_CARD_LOCKED ) ) {
		return( sdmmc_error( hba, ccb, EACCES ) );
	}

	sdmmc_ccb_lba( ccb->cam_cdb_io.cam_cdb_bytes, &lba, &nblks, &opt );

	if( ( flgs & SCF_DIR_OUT ) ) {
		if( ( part->pflags & SDMMC_PFLAG_WP ) ) {
			return( sdmmc_error( hba, ccb, EROFS ) );
		}

		if( ( ext->eflags & SDMMC_EFLAG_RELWR ) || ( ( opt & RW_OPT_FUA ) && ( ext->dev_inf.caps & DEV_CAP_WR_REL ) ) ) {
			flgs |= SCF_SBC_RLW;
		}
	}

	if( ( part->config & MMC_PART_MSK ) == MMC_PART_RPMB ) {		// no read/write to RPMB
		return( CAM_PROVIDE_FAIL );
	}

	status = sdio_set_partition( ext->device, part->config );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: sdio_set_partition failure %s", __FUNCTION__, strerror( status ) );
		sdmmc_reset( hba );
		return( sdmmc_error( hba, ccb, ETIMEDOUT ) );
	}

	sdmmc_bkops( hba, CAM_FALSE );	// Check for urgent background operations

	if( ( ccb->cam_ch.cam_flags & CAM_SCATTER_VALID ) ) {
		sgc				= ccb->cam_sglist_cnt;
		sgp				= (sdio_sge_t *)ccb->cam_data.cam_sg_ptr;
	}
	else {
		sgc				= 1;
		sgp				= &sge;
		sgp->sg_count	= ccb->cam_dxfer_len;
		sgp->sg_address	= ccb->cam_data.cam_data_ptr;
	}

	if( ( ccb->cam_ch.cam_flags & CAM_DATA_PHYS ) ) {
		flgs |= SCF_DATA_PHYS;
	}

	if( part->blk_shft ) {
		lba <<= part->blk_shft;
	}

	lba		+= part->slba;

		// Currently untested and is disabled via compile time option in mmc.[ch].  Validation requires older eMMC device. 
	if( ( flgs & SCF_SBC_RLW ) && !( ext->dev_inf.caps & DEV_CAP_WR_REL_ENH ) ) {
		sdio_sge_t		rw_sge;
		uint64_t        sg_addr;
		uint32_t		sg_count;
		uint32_t		rel_wr_count;
		uint32_t		count;
		uint32_t		sgi;

		rel_wr_count = ext->dev_inf.rel_wr_sec_c * 512;

		for( sgi = 0; sgi < sgc; sgi++ ) {
			sg_addr		= sgp[sgi].sg_address;
			sg_count	= sgp[sgi].sg_count;
			while( sg_count ) {
				count = sg_count;
					// Legacy reliable write requirements:
					// The length of the write operation equals the supported reliable write size or 512B,
					// AND the start address of the reliable write operation is aligned to the length of the
					// operation (ie reliable write start address is alway multiple of its own length)

					// check that lba is aligned with relaible write sector count
				if( !SDMMC_ADDR_ALIGNED( lba, ext->dev_inf.rel_wr_sec_c ) ) {
					count = 512;
				}

				if( count > rel_wr_count ) {
					count = rel_wr_count;
				}
				else if( count <= rel_wr_count ) {
					count = 512;
				}
				else {
					// nothing
				}

				rw_sge.sg_count		= count;
				rw_sge.sg_address	= sg_addr;
				status = sdmmc_rw( hba, part, flgs, lba, count, &rw_sge, 1, ccb->cam_req_map, ccb->cam_timeout );
				if( status != EOK ) {
					status = sdmmc_error( hba, ccb, status );
					return( status );
				}
				sg_count	-= count;
				sg_addr		+= count;
				lba			+= ( count / 512 );
			}
		}
		return( CAM_REQ_CMP );
	}

#ifdef SDMMC_SIM_RETRY
	retry = SDMMC_RW_RETRIES;
	do {
		status = sdmmc_rw( hba, part, flgs, lba, ccb->cam_dxfer_len, sgp, sgc, ccb->cam_req_map, ccb->cam_timeout );
		if( status == EOK ) {
			break;
		}
	} while( --retry && status == ETIMEDOUT );

	if( status ) {
		if( status == ETIMEDOUT ) {					// map timeout to eio
			status = EIO;
		}
		status = sdmmc_error( hba, ccb, status );
	}
#else
	status = sdmmc_rw( hba, part, flgs, lba, ccb->cam_dxfer_len, sgp, sgc, ccb->cam_req_map, ccb->cam_timeout );
	if( status != EOK ) {
		status = sdmmc_error( hba, ccb, status );
	}
#endif
	return( status ? status : CAM_REQ_CMP );
}

static int sdmmc_dsn_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT	*ext;
	char			*dsn;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	dsn		= ccb->cam_devctl_data;
	ccb->cam_devctl_status = EOK;

	memset( dsn, 0, ccb->cam_devctl_size );

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
	}
	else {
		snprintf( dsn, ccb->cam_devctl_size, "%08x", ext->dev_inf.psn );
	}

	return( CAM_REQ_CMP );
}

static int sdmmc_drvr_state_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_DRVR_STATE	*ds;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	ds		= ccb->cam_devctl_data;

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
		return( CAM_REQ_CMP );
	}

	if( ds->state == SDMMC_DRVR_PAUSE ) {
		if( sdmmc_dev_dcfg( hba ) ) {
			ccb->cam_devctl_status = EIO;
			return( CAM_REQ_CMP );
		}

		sdmmc_pm( hba, PM_IDLE );

			// freeze the simq, so nothing will start until we receive the active
		ccb->cam_ch.cam_flags &= ~CAM_SIM_QFRZDIS;
		ccb->cam_ch.cam_flags |= CAM_SIM_QFREEZE;

		cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, sdmmc_ctrl.verbosity, 1, "%s:  SDMMC_DRVR_PAUSE state", __FUNCTION__ );
	}
	else {						// SDMMC_DRVR_RUN
		sdmmc_pm( hba, PM_ACTIVE );
		sdio_reset( ext->device );
		sdmmc_dev_cfg( hba );
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, sdmmc_ctrl.verbosity, 1, "%s:  SDMMC_DRVR_RUN state", __FUNCTION__ );
	}

	ext->drvr_state			= ds->state;
	ccb->cam_devctl_status	= EOK;

	return( CAM_REQ_CMP );
}

int sdmmc_wp_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_WRITE_PROTECT	*wp;
	SDMMC_PARTITION		*part;
	uint32_t			op;
	uint32_t			nlba;
	uint64_t			slba;
	uint64_t			pend;
	int					status;

	ext			= (SIM_SDMMC_EXT *)hba->ext;
	wp			= ccb->cam_devctl_data;
	part		= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	nlba		= max( 1, wp->nlba );
	slba		= part->slba + wp->lba;
	pend		= part->slba + part->nlba - 1;
	wp->mode	&= SDMMC_WP_MODE_PWR_WP_EN;	// limit mode to nothing permanent
	status		= EOK;

	switch( wp->action ) {
		case SDMMC_WP_ACTION_CLR:
			op = MMC_CLR_WRITE_PROT;
			break;
		case SDMMC_WP_ACTION_SET:
			op = MMC_SET_WRITE_PROT;
			break;
		case SDMMC_WP_ACTION_PROT:
			op = MMC_SEND_WRITE_PROT;
			break;
		case SDMMC_WP_ACTION_PROT_TYPE:
			op = MMC_SEND_WRITE_PROT_TYPE;
			break;
		default:
			op = 0;
			break;
	}

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		status = EIO;
	}
	else if( !op || ( slba > pend ) || ( ( slba + nlba - 1 ) > pend ) ) {
		status = EINVAL;
	}
	else if( ext->instance.ident.dtype != DEV_TYPE_MMC ) {
		status	= ENOTSUP;
	}
	else {
		status = sdmmc_write_protect( hba, op, part->config, wp->mode, slba, nlba, &wp->prot );
	}

	ccb->cam_devctl_status = status;

	return( CAM_REQ_CMP );
}

int sdmmc_erase_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_ERASE			*erase;
	SDMMC_PARTITION		*part;
	uint32_t			nlba;
	uint64_t			egs;
	uint64_t			slba;
	uint64_t			pend;
	int					status;

	ext				= (SIM_SDMMC_EXT *)hba->ext;
	erase			= ccb->cam_devctl_data;
	part			= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	egs				= ext->dev_inf.erase_size / 512;
	nlba			= (uint32_t)erase->nlba;
	slba			= part->slba + erase->lba;
	pend			= part->slba + part->nlba - 1;
	status			= EINVAL;

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		status = EIO;
	}
	else if( ( slba > pend ) || ( ( slba + nlba - 1 ) > pend ) ) {
		status = EINVAL;			// verify request is within partition
	}
	else {
		switch( erase->action ) {
			case SDMMC_ERASE_ACTION_NORMAL:
					// verify for erase group alignment
				if( !( slba % egs ) && !( nlba % egs ) ) {
					status = sdio_erase( ext->device, part->config, MMC_ERASE_NORM, slba, nlba );
					if( status == EOK ) {
						part->ec += nlba;
					}
				}
				break;

			case SDMMC_ERASE_ACTION_SECURE:
					// verify SECURE cap and erase group alignment
				if( ( ext->dev_inf.caps & DEV_CAP_SECURE ) && !( slba % egs ) && !( nlba % egs ) ) {
					status = sdio_erase( ext->device, part->config, MMC_ERASE_SECURE, slba, nlba );
					if( status == EOK ) {
						part->ec += nlba;
					}
				}
				break;

			case SDMMC_ERASE_ACTION_TRIM:
				if( ( ext->dev_inf.caps & DEV_CAP_TRIM ) ) {
					status = sdio_erase( ext->device, part->config, MMC_ERASE_TRIM, slba, nlba );
					if( status == EOK ) {
						part->tc += nlba;
					}
				}
				break;

			case SDMMC_ERASE_ACTION_SECURE_TRIM:
				if( ( ext->dev_inf.caps & DEV_CAP_SECURE_TRIM ) == DEV_CAP_SECURE_TRIM ) {
					status = sdio_erase( ext->device, part->config, MMC_ERASE_SECURE_TRIM, slba, nlba );
					if( status == EOK ) {
						part->tc += nlba;
					}
				}
				break;

			case SDMMC_ERASE_ACTION_SECURE_PURGE:
				if( ( ext->dev_inf.caps & DEV_CAP_SECURE_TRIM ) == DEV_CAP_SECURE_TRIM ) {
					status = sdio_erase( ext->device, part->config, MMC_ERASE_SECURE_TRIM_PURGE, slba, nlba );
				}
				break;

			case SDMMC_ERASE_ACTION_DISCARD:
				if( ( ext->dev_inf.caps & DEV_CAP_TRIM ) ) {
					status = sdio_erase( ext->device, part->config, MMC_ERASE_DISCARD, slba, nlba );
					if( status == EOK ) {
						part->dc += nlba;
					}
				}
				break;

#if 0
			case SDMMC_ERASE_ACTION_SANITIZE:
				if( ( ext->dev_inf.caps & DEV_CAP_SANITIZE ) ) {
					if( ( status = sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT, MMC_SWITCH_MODE_WRITE, ECSD_SANITIZE_START, ECSD_SANITIZE_INITIATE, SDIO_TIME_DEFAULT ) ) != EOK ) {
							// set flag to indicate we have started a sanitize
						ext->eflags |= SDMMC_EFLAG_DEV_BUSY;
					}
				}
				break;
#endif

			default:
				status = EINVAL;
				break;
		}
	}

	ccb->cam_devctl_status = status;

	return( CAM_REQ_CMP );
}

static int sdmmc_lock_unlock_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_LOCK_UNLOCK	*lock;
	int					status;

	ext				= (SIM_SDMMC_EXT *)hba->ext;
	lock			= ccb->cam_devctl_data;

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
		return( CAM_REQ_CMP );
	}
	else if( ( lock->pwd_len > SDMMC_LU_PWD_SIZE ) ||
			( ( lock->action != SDMMC_LU_ACTION_LOCK ) &&
			( lock->action != SDMMC_LU_ACTION_SET ) &&
			( lock->action != SDMMC_LU_ACTION_CLR ) &&
			( lock->action != SDMMC_LU_ACTION_UNLOCK ) &&
			( lock->action != SDMMC_LU_ACTION_ERASE ) ) ) {
		ccb->cam_devctl_status = EINVAL;
		return( CAM_REQ_CMP );
	}
	else {
		// nothing
	}

	status = sdio_lock_unlock( ext->device, lock->action, lock->pwd, lock->pwd_len );
	if( status == EOK ) {
		sdio_dev_info( ext->device, &ext->dev_inf );	// update device info
	}

	ccb->cam_devctl_status = status;

	return( CAM_REQ_CMP );
}

int sdmmc_card_register_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT			*ext;
	SDMMC_CARD_REGISTER		*cr;
	int						len;
	uint8_t					*dptr;
	uint8_t					data[512];
	uint32_t				data_sds[16] = {0};

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	cr		= ccb->cam_devctl_data;
	len		= ccb->cam_devctl_size - sizeof( SDMMC_CARD_REGISTER );
	dptr	= (uint8_t *)(cr + 1);
	ccb->cam_devctl_status = EOK;

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
		return( CAM_REQ_CMP );
	}

	if( cr->action == SDMMC_CR_ACTION_READ ) {
		switch( cr->type ) {
			case SDMMC_REG_TYPE_CID:
				memcpy( dptr, sdio_get_raw_cid( ext->device ), min( len, MMC_CID_SIZE ) );
				break;

			case SDMMC_REG_TYPE_CSD:
				memcpy( dptr, sdio_get_raw_csd( ext->device), min( len, MMC_CSD_SIZE ) );
				break;

			case SDMMC_REG_TYPE_EXT_CSD:
				if( ext->instance.ident.dtype != DEV_TYPE_MMC ) {
					ccb->cam_devctl_status	= ENOTSUP;
				}
				else {
					int		status;

					status = sdio_send_ext_csd( ext->device, data );
					if( status == EOK ) {
						memcpy( dptr, data, min( len, sizeof( data ) ) );
					}
					else if( status == ETIMEDOUT ) {	// unresponsive device, attempt reset recovery
						sdmmc_reset( hba );
						ccb->cam_devctl_status	= status;
					}
					else {
						ccb->cam_devctl_status	= EIO;
					}
				}
				break;

			case SDMMC_REG_TYPE_SCR:
				if( ext->instance.ident.dtype != DEV_TYPE_SD ) {
					ccb->cam_devctl_status	= ENOTSUP;
				}
				else {
					memcpy( dptr, sdio_get_raw_scr( ext->device ), min( len, SD_SCR_SIZE ) );
				}
				break;

			case SDMMC_REG_TYPE_SDS:
				if( ext->instance.ident.dtype != DEV_TYPE_SD ) {
					ccb->cam_devctl_status	= ENOTSUP;
				}
				else {
					if( sdio_app_read_status( ext->device,data_sds) == EOK ) {
						memcpy( dptr, data_sds, min( len, sizeof(data_sds) ) );
					}
					else {
						ccb->cam_devctl_status	= EIO;
					}
				}
				break;

			default:
				ccb->cam_devctl_status	= EINVAL;
				break;
		}
		cr->type = ext->instance.ident.dtype;
	}
	else if( cr->action == SDMMC_CR_ACTION_WRITE ) {
		if( ( cr->type == SDMMC_REG_TYPE_EXT_CSD ) && ( ext->instance.ident.dtype == DEV_TYPE_MMC ) ) {
			if( cr->address >= ECSD_REV ) {
				ccb->cam_devctl_status  = EINVAL;
			}
			else if( sdio_mmc_switch( ext->device, MMC_SWITCH_CMDSET_DFLT, MMC_SWITCH_MODE_WRITE, cr->address, *dptr, SDIO_TIME_DEFAULT ) != EOK ) {
				ccb->cam_devctl_status  = EIO;
			}
			else {
				// nothing
			}
		}
		else {
			ccb->cam_devctl_status  = ENOTSUP;
		}
	}
	else {
		ccb->cam_devctl_status	= ENOTSUP;
	}

	return( CAM_REQ_CMP );
}

static int sdmmc_verbosity_devctl( SIM_HBA *hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	CAM_VERBOSITY		*ver;

	ext = (SIM_SDMMC_EXT *)hba->ext;
	ver	= ccb->cam_devctl_data;

	if( ver->modules == CAM_MODULE_SIM ) {
		ccb->cam_devctl_status	= EOK;
		if( ver->flags ) {
			sdio_verbosity( ext->device, ver->flags, ver->verbosity );
		}
		else {
			hba->verbosity			= (int)ver->verbosity;
		}
	}

	return( CAM_REQ_CMP );
}

static int sdmmc_device_info_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_DEVICE_INFO	*sdi;
	SDMMC_PARTITION		*part;
	sdio_dev_info_t		*di;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	sdi		= ccb->cam_devctl_data;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	ccb->cam_devctl_status = EOK;
	di		= &ext->dev_inf;

	sdi->dtype				= di->dtype;
	sdi->flags				= di->flags;
	sdi->mid				= di->mid;
	sdi->oid				= di->oid;
	memcpy( sdi->pnm, di->pnm, sizeof( di->pnm ) );
	sdi->prv				= di->prv;
	sdi->psn				= di->psn;
	sdi->month				= di->month;
	sdi->year				= di->year;
	memcpy( sdi->vu, di->vu, sizeof( di->vu ) );
	sdi->rca				= di->rca;
	sdi->spec_vers			= di->spec_vers;
	sdi->spec_rev			= di->spec_rev;
	sdi->security			= di->security;
	sdi->caps				= di->caps;
	sdi->dtr				= di->dtr;
	sdi->timing				= di->timing;
	sdi->bus_width			= di->bus_width;
	sdi->sector_size		= di->sector_size;
	sdi->super_page_size	= di->super_page_size;
	sdi->native_sector_size	= di->native_sector_size;
	sdi->wp_size			= di->wp_size;
	sdi->erase_size			= di->erase_size;
	sdi->optimal_trim_size	= di->optimal_trim_size;
	sdi->optimal_read_size	= di->optimal_read_size;
	sdi->optimal_write_size	= di->optimal_write_size;
	sdi->speed_class		= di->speed_class;

#if defined( SIM_BS_DEVICE_INFO ) && defined( SIM_BS_DEVCTL )
	sim_bs_devctl( hba, ccb );
#endif

	sdi->sectors			= (uint32_t)part->nlba;
	sdi->start_sector		= (uint32_t)part->slba;

	if( part->elba >= RD_CAP_MAX_LBA32 ) {
			// return 0/0 if partition crosses a 32 bit max.
		sdi->sectors		= 0;
		sdi->start_sector	= 0;
	}
	else if( part->nlba >= RD_CAP_MAX_LBA32 ) {
			// limit sectors to 32 bit max
		sdi->sectors		= RD_CAP_MAX_LBA32;
	}
	else {
		// nothing
	}

	sdi->sectors64		= part->nlba;
	sdi->start_sector64	= part->slba;

	return( CAM_REQ_CMP );
}

static int sdmmc_dsm( SIM_HBA * const hba, SDMMC_PARTITION *part, DATA_SET_MGNT *dsm, const uint32_t dtype )
{
	SIM_SDMMC_EXT		*ext;
	DATA_SET_MGNT_RANGE *dsmr;
	uint64_t			lba;
	uint32_t			nlba;
	uint64_t			pend;
	int					status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	dsmr	= (DATA_SET_MGNT_RANGE *)(dsm + 1);
	pend	= part->slba + part->nlba - 1;
	status  = EOK;

		// Note:  dsm->nranges == 0 is used to test for support
	for( ; dsm->nranges; dsmr++, dsm->nranges-- ) {
		nlba	= dsmr->nlba;
		lba		= part->slba + dsmr->lba;
		if( ( lba > pend ) || ( ( lba + nlba - 1 ) > pend ) ) {
			status = EINVAL;
			break;
		}

		status = sdio_erase( ext->device, part->config, dtype, lba, nlba );
		if( status ) {
			break;
		}

		switch( dtype ) {
			case MMC_ERASE_TRIM:
				part->tc += nlba;
				break;

			case MMC_ERASE_DISCARD:
				part->dc += nlba;
				break;

			default:
				break;
		}
	}

	return( status );
}

static int sdmmc_dsm_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_PARTITION		*part;
	DATA_SET_MGNT		*dsm;
	int					status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	dsm		= ccb->cam_devctl_data;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		status = EIO;
	}
	else if( ccb->cam_devctl_size < ( sizeof( DATA_SET_MGNT ) + sizeof( DATA_SET_MGNT_RANGE ) * dsm->nranges ) ) {
		status = EINVAL;
	}
	else {
		switch( dsm->opt ) {
			case DSM_OPT_TRIM:
				if( !( ext->dev_inf.caps & DEV_CAP_TRIM ) ) {
					status = EINVAL;
					break;
				}
				status = sdmmc_dsm( hba, part, dsm, MMC_ERASE_TRIM );
				break;

			case DSM_OPT_DISCARD:
				if( !( ext->dev_inf.caps & DEV_CAP_DISCARD ) ) {
					status = EINVAL;
					break;
				}
				status = sdmmc_dsm( hba, part, dsm, MMC_ERASE_DISCARD );
				break;

			default:
				status = EINVAL;
				break;
		}
	}

	ccb->cam_devctl_status = status;

	return( CAM_REQ_CMP );
}

static int sdmmc_part_info_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT			*ext;
	SDMMC_PARTITION			*part;
	SDMMC_PARTITION_INFO	*pi;
	int						status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	pi		= ccb->cam_devctl_data;
	part	= &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	status	= EOK;

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		status = EIO;
	}
	else if( ccb->cam_devctl_size < ( sizeof( SDMMC_PARTITION_INFO ) ) ) {
		status = EINVAL;
	}
	else {
		switch( pi->action ) {
			case SDMMC_PI_ACTION_GET:
				pi->ptype		= part->config & MMC_PART_MSK;
				pi->pflags		= part->pflags;
				pi->start_lba	= part->slba;
				pi->num_lba		= part->nlba;
				pi->rc			= part->rc;
				pi->wc			= part->wc;
				pi->dc			= part->dc;
				pi->ec			= part->ec;
				pi->tc			= part->tc;
				break;

			case SDMMC_PI_ACTION_CLR:
				pi->ptype		= part->config & MMC_PART_MSK;
				pi->pflags		= part->pflags;
				pi->start_lba	= part->slba;
				pi->num_lba		= part->nlba;
				pi->rc			= part->rc;
				pi->wc			= part->wc;
				pi->dc			= part->dc;
				pi->ec			= part->ec;
				pi->tc			= part->tc;
				part->rc		= 0;
				part->wc		= 0;
				part->dc		= 0;
				part->ec		= 0;
				part->tc		= 0;
				break;

			default:
				status = EINVAL;
				break;
		}
	}

	ccb->cam_devctl_status = status;

	return( CAM_REQ_CMP );
}

static int sdmmc_pwr_mgnt_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT			*ext;
	SDMMC_PWR_MGNT			*pm;
	int						status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	pm		= ccb->cam_devctl_data;
	status	= EOK;

	if( sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb ) != CAM_REQ_CMP ) {
		status = EIO;
	}
	else if( ccb->cam_devctl_size < ( sizeof( SDMMC_PWR_MGNT ) ) ) {
		status = EINVAL;
	}
	else {
		switch( pm->action ) {
			case SDMMC_PM_ACTION_GET:
				pm->idle_time	= ext->pm_idle_time_ns / 1000000;
				pm->sleep_time	= ext->pm_sleep_time_ns / 1000000;
				break;

			case SDMMC_PM_ACTION_SET:
					// use chipset defaults as min limit
				if( ( pm->idle_time < ext->hc_inf.idle_time ) ||
						( pm->sleep_time < ext->hc_inf.sleep_time ) ) {
					status = EINVAL;
					break;
				}
				ext->pm_idle_time_ns	= SDMMC_TIMEOUT_MS_TO_NS( pm->idle_time );
				ext->pm_sleep_time_ns	= SDMMC_TIMEOUT_MS_TO_NS( pm->sleep_time );
				break;

			default:
				status = EINVAL;
				break;
		}
	}

	ccb->cam_devctl_status = status;

	return( CAM_REQ_CMP );
}

static int sdmmc_rpmb_info( SIM_HBA * const hba, CCB_DEVCTL *ccb, RPMB_INFO *info, const int minsz )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_PARTITION	*part;
	uint8_t			*ecsd;
	sdio_csd_t		*csd;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	if( ext->instance.ident.dtype != DEV_TYPE_MMC ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: the device doesn't support RPMB!", __FUNCTION__ );
		return( ENOTSUP );
	}

	part = &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	if( ( part->config & MMC_PART_MSK ) != MMC_PART_RPMB ) {		// not RPMB partition
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: not RPMB partition", __FUNCTION__ );
		return( ENOTSUP );
	}

	if( ccb->cam_devctl_size < minsz ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: devctl buffer too small", __FUNCTION__ );
		return( EINVAL );
	}

	ecsd = sdio_get_raw_ecsd( ext->device );
	csd  = sdio_get_csd( ext->device );

	memset( info, 0, sizeof( RPMB_INFO ) );
	info->maxio     = ( ecsd[ECSD_WR_REL_PARAM] & ECSD_WRP_EN_RPMB_REL_WR ) ? 32 : 2;
	info->start_lba = part->slba;
	info->num_lba   = part->nlba;
	info->parts     = 1;
	info->dtype     = RPMB_DTYPE_EMMC;
#if (_NTO_VERSION >= 800)
/* These elements are not present in SDP 7.1*/
	info->partitions[0].start_lba = part->slba;
	info->partitions[0].num_lba = part->nlba;
#endif
	info->flags     = ( csd->write_protect & 0x01 ) ? RPMB_FLAG_WP : 0;

	return( EOK );
}

static int sdmmc_rpmb_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT		*ext;
	SDMMC_PARTITION		*part;
	uint32_t			nf;
	int					status;
	RPMB_CMDHDR			*req;
	RPMB_INFO			info;
	rpmb_frame_jedec_t	*pf;

	ext	= (SIM_SDMMC_EXT *)hba->ext;
	req	= ccb->cam_devctl_data;

	status = sdmmc_rpmb_info( hba, ccb, &info, sizeof( RPMB_CMDHDR ) + req->nframes * RPMB_FRAME_SIZE );
	if( status != EOK ) {
		ccb->cam_devctl_status = status;
		return( CAM_REQ_CMP );
	}

	if( req->flags == RPMB_FLG_RL_WRITE ) {
		if( info.flags & RPMB_FLAG_WP ) {
			ccb->cam_devctl_status = EACCES;
			return( CAM_REQ_CMP );
		}
		if( info.maxio < req->nframes ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: frame count[%d] exceed maximum write count[%d]", __FUNCTION__, req->nframes, info.maxio );
			ccb->cam_devctl_status = EINVAL;
			return( CAM_REQ_CMP );
		}
	}

	status = sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb );
	if( status != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
		return( CAM_REQ_CMP );
	}

	sdmmc_bkops( hba, CAM_FALSE );	// Check for urgent background operations

		// Switch to RPMB partition
	part = &ext->targets[ccb->cam_ch.cam_target_id].partitions[ccb->cam_ch.cam_target_lun];
	status = sdio_set_partition( ext->device, part->config );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: sdio_set_partition failure %s", __FUNCTION__, strerror( status ) );
		sdmmc_reset( hba );
		ccb->cam_devctl_status = status;
		return( CAM_REQ_CMP );
	}

	pf = (rpmb_frame_jedec_t *)( req + 1 );
		// Write authentication key or data, use reliable write
	if( req->flags != RPMB_FLG_READ ) {
		status = sdio_mmc_rpmb_rw( ext->device, pf, req->nframes, SCF_DIR_OUT | SCF_SBC_RLW );
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s, %d: sdio_mmc_rpmb_rw failure: %s", __FUNCTION__, __LINE__, strerror( status ) );
			sdmmc_reset( hba );
			ccb->cam_devctl_status = status;
			return( CAM_REQ_CMP );
		}

		// Need to send read result request
		memset( pf, 0, RPMB_FRAME_SIZE );
		pf->req_resp = ENDIAN_BE16( RPMB_READ_RESULT );
	}

		// Send read request (data or write result)
	pf->block_count = 0;		// block count has to be zero by spec
	status = sdio_mmc_rpmb_rw( ext->device, pf, 1, SCF_DIR_OUT );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s, %d: sdio_mmc_rpmb_rw failure: %s", __FUNCTION__, __LINE__, strerror( status ) );
		sdmmc_reset( hba );
		ccb->cam_devctl_status = status;
		return( CAM_REQ_CMP );
	}

		// read data, write counter or write result
	nf = ( req->flags == RPMB_FLG_READ ) ? req->nframes : 1;
	status = sdio_mmc_rpmb_rw( ext->device, pf, nf, SCF_DIR_IN );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s, %d: sdio_mmc_rpmb_rw failure: %s", __FUNCTION__, __LINE__, strerror( status ) );
		sdmmc_reset( hba );
		ccb->cam_devctl_status = status;
		return( CAM_REQ_CMP );
	}

	ccb->cam_devctl_status = EOK;

	return( CAM_REQ_CMP );
}

static int sdmmc_rpmb_info_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	ccb->cam_devctl_status = sdmmc_rpmb_info( hba, ccb,
					(RPMB_INFO *)ccb->cam_devctl_data, sizeof( RPMB_INFO ) );

	return( CAM_REQ_CMP );
}

static int sdmmc_gencmd_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_GEN_CMD	*req;
	int				status;
	void			*buf;
	uint32_t		flags;
	uint32_t		arg;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	req		= ccb->cam_devctl_data;

	status = sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb );
	if( status != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
		return( CAM_REQ_CMP );
	}

	sdmmc_bkops( hba, CAM_FALSE );	// Check for urgent background operations

	for( ; ; ) {
		buf		= (void *)( req + 1 );
		flags	= SCF_CTYPE_ADTC | SCF_RSP_R1;
		arg		= req->arg;

		if( ( req->flags & SDMMC_GENCMD_DIR ) == SDMMC_GENCMD_READ ) {
			flags |= SCF_DIR_IN;
		}
		else {
			flags |= SCF_DIR_OUT;
		}

		status = sdio_mmc_gen_man( ext->device, MMC_GEN_CMD, buf, req->blklen, 1, arg, flags);
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s, %d: sdio_mmc_gencmd failure: %s", __FUNCTION__, __LINE__, strerror( status ) );
			sdmmc_reset( hba );
			ccb->cam_devctl_status = status;
			break;
		}

		ccb->cam_devctl_status = EOK;

		if( ( req->flags & SDMMC_GENCMD_NEXT ) == 0 ) {
			break;
		}

		req = (SDMMC_GEN_CMD *)( (uintptr_t)buf + req->blklen );
	}

	return( CAM_REQ_CMP );
}

static int sdmmc_mancmd_devctl( SIM_HBA * const hba, CCB_DEVCTL *ccb )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_MAN_CMD	*req;
	int				status;
	uint8_t			op;
	void			*buf;
	uint32_t		flags;
	uint32_t		arg;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	req		= ccb->cam_devctl_data;

	status = sdmmc_unit_ready( hba, (CCB_SCSIIO *)ccb );
	if( status != CAM_REQ_CMP ) {
		ccb->cam_devctl_status = EIO;
		return( CAM_REQ_CMP );
	}

	sdmmc_bkops( hba, CAM_FALSE );	// Check for urgent background operations

	for( ; ; ) {
		op = req->flags & SDMMC_MANCMD_CMD;
		if( ( op < MMC_MAN_CMD1 ) || ( op > MMC_MAN_CMD4 ) ) {
			ccb->cam_devctl_status = ENOTSUP;
			break;
		}

		buf		= ( req->flags & SDMMC_MANCMD_ADTC ) ? (void *)( req + 1 ) : NULL;
		flags	= 0;
		arg		= req->arg;

		if( ( req->flags & SDMMC_MANCMD_TYPE ) == SDMMC_MANCMD_ADTC ) {
			flags |= SCF_CTYPE_ADTC;
		}
		if( ( req->flags & SDMMC_MANCMD_DIR ) == SDMMC_MANCMD_READ ) {
			flags |= SCF_DIR_IN;
		}
		if( ( req->flags & SDMMC_MANCMD_RSP ) == SDMMC_MANCMD_R1 ) {
			flags |= SCF_RSP_R1;
		}
		else {
			flags |= SCF_RSP_R1B;
		}

		status = sdio_mmc_gen_man( ext->device, op, buf, req->blklen, 1, arg, flags);
		if( status != EOK ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s, %d: sdio_mmc_mancmd failure: %s", __FUNCTION__, __LINE__, strerror( status ) );
			sdmmc_reset( hba );
			ccb->cam_devctl_status = status;
			break;
		}

		ccb->cam_devctl_status = EOK;

		if( ( req->flags & SDMMC_MANCMD_NEXT ) == 0 ) {
			break;
		}

		req = (SDMMC_MAN_CMD *)( (uintptr_t)((void *)( req + 1 )) + req->blklen );
	}

	return( CAM_REQ_CMP );
}

static int sdmmc_devctl_perm( CCB_DEVCTL *ccb )
{
	int		ret;

	ret = CAM_REQ_INPROG;

	switch( ccb->cam_devctl_dcmd ) {
		case DCMD_SDMMC_DEVICE_INFO:
		case DCMD_SDMMC_ERASE:
		case DCMD_SDMMC_WRITE_PROTECT:
		case DCMD_SDMMC_CARD_REGISTER:
		case DCMD_SDMMC_ASSD_STATUS:
		case DCMD_SDMMC_ASSD_PROPERTIES:
		case DCMD_SDMMC_ASSD_CONTROL:
		case DCMD_SDMMC_ASSD_APDU:
		case DCMD_SDMMC_LOCK_UNLOCK:
		case DCMD_SDMMC_PART_INFO:
		case DCMD_SDMMC_PWR_MGNT:
		case DCMD_CAM_VERBOSITY:
		case DCMD_CAM_DEV_SERIAL_NUMBER:
		case DCMD_CAM_DATA_SET_MGNT:
		case DCMD_RPMB_INFO:
		case DCMD_RPMB_CMD:
		case DCMD_SDMMC_GEN_CMD:
		case DCMD_SDMMC_MAN_CMD:
		case DCMD_SDMMC_DRVR_STATE:
				// fail requests without RD or WR
			if( !( ccb->cam_devctl_ioflag & ( _IO_FLAG_RD | _IO_FLAG_WR ) ) ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  dcmd 0x%x, EACCES", __FUNCTION__, ccb->cam_devctl_dcmd );
				ccb->cam_devctl_status = EACCES;
				ret = CAM_REQ_CMP;
			}
			break;

		default:
			break;
	}

	return( ret );
}

static int sdmmc_devctl( SIM_HBA * const hba, CCB_DEVCTL * const ccb )
{
	int	status;

	status	= CAM_REQ_CMP;

	if( sdmmc_devctl_perm( ccb ) ) {
		return( CAM_REQ_CMP );
	}

	switch( ccb->cam_devctl_dcmd ) {
		case DCMD_SDMMC_DEVICE_INFO:
			status = sdmmc_device_info_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_ERASE:
			status = sdmmc_erase_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_WRITE_PROTECT:
			status = sdmmc_wp_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_CARD_REGISTER:
			status = sdmmc_card_register_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_ASSD_STATUS:
			status = sdmmc_assd_status_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_ASSD_PROPERTIES:
			status = sdmmc_assd_properties_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_ASSD_CONTROL:
			status = sdmmc_assd_control_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_ASSD_APDU:
			status = sdmmc_assd_apdu_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_LOCK_UNLOCK:
			status = sdmmc_lock_unlock_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_PART_INFO:
			status = sdmmc_part_info_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_PWR_MGNT:
			status = sdmmc_pwr_mgnt_devctl( hba, ccb );
			break;

		case DCMD_CAM_VERBOSITY:
			status = sdmmc_verbosity_devctl( hba, ccb );
			break;

		case DCMD_CAM_DEV_SERIAL_NUMBER:
			status = sdmmc_dsn_devctl( hba, ccb );
			break;

		case DCMD_CAM_DATA_SET_MGNT:
			status = sdmmc_dsm_devctl( hba, ccb );
			break;

		case DCMD_RPMB_INFO:
			status = sdmmc_rpmb_info_devctl( hba, ccb );
			break;

		case DCMD_RPMB_CMD:
			status = sdmmc_rpmb_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_GEN_CMD:
			status = sdmmc_gencmd_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_MAN_CMD:
			status = sdmmc_mancmd_devctl( hba, ccb );
			break;

		case DCMD_SDMMC_DRVR_STATE:
			status = sdmmc_drvr_state_devctl( hba, ccb );
			break;

		default:
#ifdef SIM_BS_DEVCTL
// Note: bs module must validate ioflags
			status = sim_bs_devctl( hba, ccb );
#endif
			break;
	}

	return( status );
}

static int sdmmc_service_action( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	int		action;
	int		status;

	action	= ccb->cam_cdb_io.cam_cdb_bytes[1] & SAI_SERVICE_ACTION_MSK;

#ifdef SDMMC_SIM_DEBUG
	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s (ptl %d:%d:%"PRIu64") action %x",
		__FUNCTION__, (int)hba->pathid, ccb->cam_ch.cam_target_id, ccb->cam_ch.cam_target_lun, action );
#endif

	switch( action ) {
		case SAI_READ_CAPACITY_16:
			status = sdmmc_capacity( hba, ccb, action );
			break;

		default:
			status = sdmmc_sense( ccb, SK_ILLEGAL, ASC_INVALID_FIELD, 0 );
			break;
	}

	return( status );
}

// interpret SCSI commands
static int sdmmc_scsi_io( SIM_HBA * const hba, CCB_SCSIIO * const ccb )
{
	SIM_SDMMC_EXT	*ext;
	int				cmd;
	int				status;

	ext = (SIM_SDMMC_EXT *)hba->ext;
	cmd = ccb->cam_cdb_io.cam_cdb_bytes[0];

	if( hba->verbosity > 3 ) {
		xpt_display_ccb( ccb, hba->verbosity );
	}

	if( ccb->cam_dxfer_len && ( ccb->cam_ch.cam_flags & CAM_DATA_PHYS ) &&
			!( ext->hc_inf.caps & HC_CAP_DMA ) ) {
		return( CAM_PROVIDE_FAIL );
	}

	switch( cmd ) {
		case SC_UNIT_RDY:
			status = sdmmc_unit_ready( hba, ccb );
			break;

		case SC_INQUIRY:
			status = sdmmc_inquiry( hba, ccb );
			break;

		case SC_SPINDLE:
			status = sdmmc_spindle( hba, ccb );
			break;

		case SC_RD_CAP:
			status = sdmmc_capacity( hba, ccb, 0 );
			break;

		case SC_READ10:
			status = sdmmc_read_write( hba, ccb, SCF_DIR_IN );
			break;

		case SC_WRITE10:
			if( ( ext->dev_inf.caps & DEV_CAP_CACHE ) && ( ext->eflags & SDMMC_EFLAG_CACHE ) ) {
					// Note:  We set the dirty flag prior to making the write request.  It is possible
					// for the request to fail, but we don't know a what point.  The write could have
					// completed, but we timeout on getting the status. The write routine may even
					// reset the device, clearing the volatile cache.
				ext->eflags |= SDMMC_EFLAG_VCACHE_DIRTY;
			}
			status = sdmmc_read_write( hba, ccb, SCF_DIR_OUT );

#ifdef SDMMC_WRITE_VERIFY
			if( status == CAM_REQ_CMP ) {
				status = sdmmc_write_verify( hba, ccb );
			}
#endif
			break;

		case SC_SYNC:
			status = sdmmc_sync( hba, ccb );
			break;

		case SC_MSELECT10:
			status = sdmmc_mode_select( hba, ccb );
			break;

		case SC_MSENSE10:
			status = sdmmc_mode_sense( hba, ccb );
			break;

		case SC_ERASE12:
			status = sdmmc_erase12( hba, ccb );
			break;

		case SC_WR_BUF:
			status = sdmmc_ffu( hba, ccb );
			break;

#ifdef SDMMC_TRIM_SUP
		case SC_WR_SAME16:
			status = sdmmc_write_same( hba, ccb );
			break;
#endif

		case SC_SERVICE_ACTION_IN16:
			status = sdmmc_service_action( hba, ccb );
			break;

		default:
#if defined SIM_BS_PASS_THROUGH
			status = sim_bs_pass_through( hba, ccb );
			if( status != CAM_REQ_INVALID ) {
				break;
			}
#endif
			return( sdmmc_error( hba, ccb, EINVAL ) );
	}

	return( status );
}

static void sdmmc_start_ccb( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT	*ext;
	CCB_SCSIIO		*ccb;
	int				scnt;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= CAM_REQ_CMP_ERR;
	scnt	= 0;

	do {
		ccb = simq_ccb_dequeue( hba->simq );
		if( ccb == NULL ) {
#ifdef SDMMC_AGGRESSIVE_PM
				// In aggressive pm mode we direct call the sdio layer,
				// so we don't have the overhead of enabling/disabling
				// the local PM timer.
			sdio_pwrmgnt( ext->device, PM_IDLE );
#endif
			break;
		}

		ext->nexus = ccb;

		sdmmc_pm( hba, PM_ACTIVE );

		switch( ccb->cam_ch.cam_func_code ) {
			case XPT_SCSI_IO:
				status = sdmmc_scsi_io( hba, ccb );
				break;

			case XPT_DEVCTL:
				status = sdmmc_devctl( hba, (CCB_DEVCTL *)ccb );
				break;

			default:
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1,
						"%s:  unsupported func code %d", __FUNCTION__, ccb->cam_ch.cam_func_code );
				status = CAM_REQ_CMP_ERR;
				break;
		}

		if( status != CAM_REQ_INPROG ) {
			ccb->cam_ch.cam_status = (uint8_t)status;
			sdmmc_post_ccb( hba, ccb );
			if( sdio_retune( ext->device ) != EOK ) {		// in case retune is needed
				sdmmc_reset( hba );
			}
		}

		// loop starting new requests until scnt reaches max
		// scnt max is needed to allow for handling of pulse
		// events ie power management, bkops
	} while( ( ext->nexus == NULL ) && ( ++scnt < ext->start_ccb_max ) );

	if( scnt == ext->start_ccb_max ) {
			// trigger a pulse to start next request
		if( MsgSendPulse( hba->coid, hba->priority, SIM_ENQUEUE, 0 ) == -1 ) {
		}
	}
}

static int sdmmc_timer( SIM_HBA * const hba )
{
	SIM_SDMMC_EXT	*ext;
	int				status;
	int				pm_state;
	uint64_t		timestamp;
	struct timespec	ts;

	ext			= (SIM_SDMMC_EXT *)hba->ext;
	status		= EOK;
	pm_state	= ext->pm_state;

	if( ext->nexus || ( pm_state == PM_SLEEP ) || ( pm_state == PM_SUSPEND ) || ( ext->drvr_state == SDMMC_DRVR_PAUSE ) ) {
		return( status );
	}

	clock_gettime( CLOCK_MONOTONIC, &ts );

	timestamp = timespec2nsec( &ts );

	if( pm_state == PM_ACTIVE ) {
		if( timestamp >= ( ext->pm_timestamp + ext->pm_idle_time_ns ) ) {
			sdmmc_pm( hba, PM_IDLE );
		}
	}
	else if( ( pm_state == PM_IDLE ) && ( ext->hc_inf.caps & HC_CAP_SLEEP ) ) {
		if( timestamp >= ( ext->pm_timestamp + ext->pm_sleep_time_ns ) ) {
			if( sdmmc_cache_flush( hba ) == EOK ) {
				sdmmc_pm( hba, PM_SLEEP );
			}
		}
	}
	else {
		// nothing
	}

	return( status );
}

static int sdmmc_simq_state( SIM_HBA * const hba, const int state )
{
	SIM_SDMMC_EXT	*ext;
	SDMMC_TARGET	*targ;
	CCB_RELSIM		ccb;
	target_id_t		tgt;
	target_id_t		lun;

	ext = (SIM_SDMMC_EXT *)hba->ext;

	memset( &ccb, 0, sizeof( ccb ) );

		// set targets
	for( tgt = 0; tgt <= ext->ntargs; tgt++ ) {
		targ = &ext->targets[tgt];
		for( lun = 0; lun <= targ->nluns; lun++ ) {
			ccb.cam_ch.cam_target_id	= tgt;
			ccb.cam_ch.cam_target_lun	= lun;
			if( state ) {
				simq_rel_simq( hba->simq, &ccb );
			}
			else {
				simq_frz_simq( hba->simq, &ccb );
			}
		}
	}

	if( state ) {
			// send pulse to start queued requests
		if( MsgSendPulse( hba->coid, hba->priority, SIM_ENQUEUE, 0 ) == -1 ) {
		}
	}

	return( EOK );
}

static void *sdmmc_ac_os_pwr_thread( void * const hdl )
{
	SIM_HBA		*hba;

	hba = hdl;

		// push up event so cam/peripheral drivers can have io-blk/filesystems flush/sync
	xpt_async( AC_OS_PWR_SIGNAL, hba->pathid, -1, -1, NULL, 0 );
	pthread_sleepon_lock( );
	hba->state = CAM_TRUE;
	pthread_sleepon_unlock( );
	return( NULL );
}

static int sdmmc_os_pwr_signal( SIM_HBA *hba )
{
	int				state;
	pthread_t		tid;
	int				status;

	hba->state	= CAM_FALSE;
	state		= CAM_FALSE;

//	cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s: start", __FUNCTION__ );

		// spin a thread to send xpt_async (blocking call), then loop processing requests until sync/flush has completed
	status = cam_create_thread( &tid, NULL, sdmmc_ac_os_pwr_thread, hba, hba->priority, NULL, "sdmmc_ac_os_pwr_thread" );
	if( status == EOK ) {
		while( state == CAM_FALSE ) {
			sdmmc_start_ccb( hba );
			pthread_sleepon_lock( );
			state = hba->state;
			pthread_sleepon_unlock( );
			if( state == CAM_FALSE ) {
				delay( 1 );
			}
		}
		pthread_join( tid, NULL );
//		cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s: complete", __FUNCTION__ );
	}
	else {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: sdmmc_ac_os_pwr_thread creation failure", __FUNCTION__ );
	}
	return( status );
}

int sdmmc_suspend( SIM_HBA * const hba )
{
	sdmmc_pm( hba, PM_ACTIVE );
	sdmmc_os_pwr_signal( hba );
	sdmmc_dev_dcfg( hba );
	sdmmc_simq_state( hba, CAM_FALSE );		// freeze simqs
	sdmmc_pm( hba, PM_SUSPEND );
	return( EOK );
}

int sdmmc_resume( SIM_HBA * const hba )
{
	sdmmc_pm( hba, PM_ACTIVE );
	sdmmc_dev_cfg( hba );
	sdmmc_simq_state( hba, CAM_TRUE );		// unfreeze simqs
	return( EOK );
}

#ifdef SDMMC_POWMAN_SUP
static int sdmmc_powman_allowed_mode_list_update( SIM_HBA *hba )
{
	SIM_SDMMC_EXT	*ext;
	struct sigevent	event;
	unsigned		req_num;
	void			**allowed_list[sizeof( sdmmc_pm_states_list ) / sizeof( sdmmc_pm_states_list[0] )];
	void			**mode;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	status = powman_get_allowed_modes( ext->powman_hdl, &req_num, allowed_list );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s  powman_get_allowed_modes failed: %s", __FUNCTION__, strerror( status ) );
		return( status );
	}

	mode = allowed_list[0];

	if( mode != NULL ) {
		if( mode == (void **)&sdmmc_pm_state_on ) {				// resume
			if( ext->pm_state == PM_SUSPEND ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  resume %s", __FUNCTION__, ext->powman_name ? ext->powman_name : __progname );
				sdmmc_resume( hba );
			}
			else {
//				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s  Already active", __FUNCTION__ );
			}
		}
		else if( mode == (void **)&sdmmc_pm_state_suspend ) {	// suspend
			if( ext->pm_state != PM_SUSPEND ) {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s:  suspend %s", __FUNCTION__, ext->powman_name ? ext->powman_name : __progname );
				sdmmc_suspend( hba );
			}
			else {
//				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s  Already suspended", __FUNCTION__ );
			}
		}
        else {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Unsupported mode", __FUNCTION__ );
			return( EINVAL );
        }
	}
	else {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  No valid allowed modes", __FUNCTION__ );
		return( EINVAL );
	}

	status = powman_set_current_mode( ext->powman_hdl, req_num, mode );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  powman_mode_set failed, error (%d) %s", __FUNCTION__, status, strerror( status ) );
		return( status );
    }

		// re-register to get the next pulse.  Note:  + 1 to give powman priority
    SIGEV_PULSE_INIT( &event, hba->coid, hba->priority + 1, SDMMC_POWMAN_UPDATE, NULL );
	status = powman_request_allowed_modes_event( ext->powman_hdl, &event );
	if( status != EOK ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Failed re-register for power events, error (%d) %s", __FUNCTION__, status, strerror( status ) );
		return( status );
	}

	return( EOK );
}

void sdmmc_powman_disconnect( SIM_HBA *hba )
{
	SIM_SDMMC_EXT	*ext;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

	if( ext->powman_hdl != NULL ) {
		powman_disconnect( ext->powman_hdl );
		powman_destroy( ext->powman_hdl );
		ext->powman_hdl = NULL;
	}
}

static int sdmmc_powman_connect( SIM_HBA *hba )
{
	SIM_SDMMC_EXT		*ext;
	int					status;
	struct sigevent		event;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= EOK;

	if( ext->powman_hdl ) {
			// setup powman reconnect notification pulse event
		SIGEV_PULSE_INIT( &event, hba->coid, hba->priority, SDMMC_POWMAN_RECONNECT, NULL );
		status = powman_connect_event( ext->powman_hdl, &event );
		if( status == EOK ) {
			status = powman_connect( ext->powman_hdl );
			if( status != EOK ) {
					// not fatal, will be notified via reconnect pulse
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_INFO, 1, 1, "%s powman_connect() failed, error (%d) %s, will retry when powerman comes online", __FUNCTION__, status, strerror( status ) );
			}

			// setup powman update notification pulse event
			SIGEV_PULSE_INIT( &event, hba->coid, hba->priority + 1, SDMMC_POWMAN_UPDATE, NULL );
			status = powman_request_allowed_modes_event( ext->powman_hdl, &event );
			if( status == EOK ) {
				return( EOK );
			}
			else {
				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s  powman_request_allowed_modes_event() failed, error (%d) %s", __FUNCTION__, status, strerror( status ) );
			}
		}
		else {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s  powman_connect_event() failed, error (%d) %s", __FUNCTION__, status, strerror( status ) );
		}
	}

	return( status );
}

static int sdmmc_powman_create( SIM_HBA *hba )
{
	SIM_SDMMC_EXT		*ext;
	int					status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;

		// Create powman object with our name, set initial powman state to on/active, initial mode 0 (on)
	ext->powman_hdl = powman_create( ext->powman_name ? ext->powman_name : __progname, "sdmmc", "storage", (void ***)sdmmc_pm_states_list, 1, (void *)sdmmc_pm_states_list[0] );
	if( ext->powman_hdl != NULL ) {
		status = sdmmc_powman_connect( hba );
		if( status == EOK ) {
			return( status );
		}
	}
	else {
		status = errno;
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s  powman_create failed, error (%d) %s", __FUNCTION__, status, strerror( status ) );
	}

	sdmmc_powman_disconnect( hba );
	return( status );
}
#endif

void *sdmmc_driver_thread( void * const hdl )
{
	SIM_HBA			*hba;
	SIM_SDMMC_EXT	*ext;
	struct _pulse	pulse;
	struct sigevent	event;
	int				rid;
	int				status;

	hba		= hdl;
	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= CAM_FALSE;

	ext->drvr_state = SDMMC_DRVR_RUN;

	hba->chid = ChannelCreate( _NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK );
	if( hba->chid == -1 ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s ChannelCreate failure %s", __FUNCTION__, strerror( errno ) );
		status = CAM_TRUE;
	}

	if( !status ) {
		hba->coid = ConnectAttach( 0, 0, hba->chid, _NTO_SIDE_CHANNEL, 0 );
		if( hba->coid == -1 ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s ConnectAttach failure %s", __FUNCTION__, strerror( errno ) );
			status = CAM_TRUE;
		}
	}

	SIGEV_PULSE_INIT( &event, hba->coid, (short)hba->priority, SDMMC_PM_TIMER, NULL );
	if( !status && ( timer_create( CLOCK_MONOTONIC, &event, &ext->pm_timerid ) == -1 ) ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  timer_create", __FUNCTION__ );
		status = CAM_TRUE;
	}

#ifdef SDMMC_POWMAN_SUP
	if( ( ext->eflags & SDMMC_EFLAG_POWMAN ) && sdmmc_powman_create( hba ) ) {
		cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  sdmmc_powman_create", __FUNCTION__ );
		status = CAM_TRUE;
	}
#endif

		// initialize SIM queue routines
	if( !status ) {
		hba->simq = simq_init( hba->coid, hba, MAX_NARROW_TARGET,
			MAX_LUN, 2, 1, 2, ( ext->eflags & SDMMC_EFLAG_BKOPS ) ? 1 : 0 );
		if( hba->simq == NULL ) {
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  simq_init failure", __FUNCTION__ );
			status = CAM_TRUE;
		}
	}

	if( status ) {
		cam_set_thread_state( &hba->state, CAM_TSTATE_INIT_FAILURE );
		return( NULL );
	}

	cam_set_thread_state( &hba->state, CAM_TSTATE_INITIALIZED );

	while( 1 ) {
		rid = MsgReceivePulse( hba->chid, &pulse, sizeof( pulse ), NULL );
		if( rid == -1 ) {
			break;
		}

		switch( pulse.code ) {
			case SIM_ENQUEUE:
				sdmmc_start_ccb( hba );
				break;

			case SIM_TIMER:
				if( ext->pm_state == PM_ACTIVE ) {
					sdmmc_bkops( hba, CAM_TRUE );
				}
				break;

			case SDMMC_PM_TIMER:
				sdmmc_timer( hba );
				break;

#ifdef SDMMC_POWMAN_SUP
			case SDMMC_POWMAN_UPDATE:
				if( ( status = sdmmc_powman_allowed_mode_list_update( hba ) ) ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  sdmmc_pm_allowed_mode_list_update() failed %s (%d)", __FUNCTION__, strerror( status ), status );
				}
				break;

			case SDMMC_POWMAN_RECONNECT:
				if( ( stat = sdmmc_powman_connect( hba ) ) ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  sdmmc_powman_connect() failed %s (%d)", __FUNCTION__, strerror( status ), status );
				}
				break;
#endif

			case _PULSE_CODE_DISCONNECT:
				return( NULL );

			default:
//				cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s: unknown pulse rid %d, type %x, subtype %d, pulse %x, value %x, scoid %x", __FUNCTION__, rid, pulse.type, pulse.subtype, pulse.code, pulse.value.sival_int, pulse.scoid );
				break;
		}

		if( ext->nexus == NULL ) {
			sdmmc_start_ccb( hba );
		}
	}

	return( NULL );
}

// This function is called by the XPT to request that the SIM be initialized.
int sdmmc_sim_init( SIM_HBA *hba, int path )
{
	hba = hba;
	path = path;

	return( CAM_SUCCESS );
}

// This fuction is called when the driver is terminating.  It is provided
// to perform any necessary cleanup before exiting.
int sdmmc_sim_detach( void )
{
	SIM_HBA			*hba;
	SIM_HBA			*nhba;

	sdmmc_ctrl.cflags |= SDMMC_CFLAG_ENUMERATING;
	if( sdio_disconnect( sdmmc_ctrl.connection ) == EBUSY ) {
		for( hba = TAILQ_FIRST( &sdmmc_ctrl.hlist ); hba; hba = nhba ) {
			nhba = TAILQ_NEXT(hba, hlink);
			sdmmc_detach( hba, CAM_TRUE );
		}
		sdio_disconnect( sdmmc_ctrl.connection );
	}

	return( CAM_SUCCESS );
}

// This function is used to reset the specified SCSI bus.  This
// request shall slways result in the SCSI RST signal being
// asserted.
__attribute__((used))
static int sdmmc_reset_bus( SIM_HBA * const hba, CCB_RESETBUS * const ccb )
{
	simq_scsi_reset( hba->simq );
	xpt_async( AC_BUS_RESET, hba->pathid, -1, -1, NULL, 0 );
	return( CAM_REQ_CMP );
}

// This function is used to reset the specified SCSI target.  This
// function should not be used in normal operation, but if I/O to a
// particular device hangs up for some reason, drivers can abort the
// I/O and reset the device before trying again.  This request shall
// always result in a bus device reset message being issued over SCSI.
__attribute__((used))
static int sdmmc_reset_dev( SIM_HBA * const hba, CCB_RESETDEV * const ccb )
{
	simq_reset_dev( hba->simq, ccb );
	xpt_async( AC_SENT_BDR, hba->pathid, -1, -1, NULL, 0 );
	return( CAM_REQ_CMP );
}

// This function is provided so that the peripheral driver can
// release a frozen SIM queue for the selected Logical Unit.
static int sdmmc_rel_simq( SIM_HBA * const hba, CCB_RELSIM * const ccb )
{
	simq_rel_simq( hba->simq, ccb );
	if( MsgSendPulse( hba->coid, hba->priority, SIM_ENQUEUE, 0 ) == -1 ) {
	}
	return( CAM_REQ_CMP );
}

// This function requests that a SCSI I/O request be terminated by
// identifying the CCB associated with the request.  This request
// does not necessarily result in a terminated I/O process message
// being issued over SCSI.
static int sdmmc_term_io( SIM_HBA *hba, CCB_TERMIO *ccb )
{
	hba = hba;
	ccb = ccb;

	return( CAM_REQ_CMP_ERR );
}

// This function is used to get information on the installed HBA
// hardware, including number of HBAs installed
static int sdmmc_path_inq( SIM_HBA * const hba, CCB_PATHINQ *ccb )
{
	SIM_SDMMC_EXT	*ext;

	ext = (SIM_SDMMC_EXT *)hba->ext;

	ccb->cam_version_num	= CAM_VERSION;
	ccb->cam_initiator_id	= 7;
	ccb->cam_hba_inquiry	= 0;
	ccb->cam_target_sprt	= 0;
	ccb->cam_hba_misc		= 0;
	ccb->cam_hba_eng_cnt	= 0;
	ccb->cam_sim_priv		= SIM_PRIV;
	ccb->cam_async_flags	= AC_BUS_RESET;
	memset( ccb->cam_vuhba_flags, 0x00, sizeof( *ccb->cam_vuhba_flags ) );

	if( !( ext->hc_inf.caps & HC_CAP_DMA ) ) {
		ccb->cam_vuhba_flags[CAM_VUHBA_FLAGS]	= CAM_VUHBA_FLAG_PTR;
	}
	else {
		ccb->cam_vuhba_flags[CAM_VUHBA_FLAGS]	= CAM_VUHBA_FLAG_PTR | CAM_VUHBA_FLAG_DMA;
		if( ( ext->hc_inf.caps & HC_CAP_DMA_MSK ) != HC_CAP_DMA64 ) {
			ccb->cam_vuhba_flags[CAM_VUHBA_EFLAGS]	|= CAM_VUHBA_EFLAG_DMA_32;
		}
	}

	ccb->cam_vuhba_flags[CAM_VUHBA_FLAGS]		|= CAM_VUHBA_FLAG_MLUN;

	if( ( ext->eflags & SDMMC_EFLAG_DEVNAME ) ) {
		ccb->cam_vuhba_flags[CAM_VUHBA_EFLAGS]	|= CAM_VUHBA_EFLAG_DEVNAME;
	}

	ccb->cam_vuhba_flags[CAM_VUHBA_EFLAGS]		|= CAM_VUHBA_EFLAG_PNP;

	ccb->cam_vuhba_flags[CAM_VUHBA_MAX_LINKED]	= 1;
	ccb->cam_vuhba_flags[CAM_VUHBA_MAX_SG]		= (uint8_t)ext->hc_inf.sg_max;

	strlcpy( (char *)ccb->cam_sim_vid, "SDMMC", sizeof( ccb->cam_sim_vid ) );
	strlcpy( (char *)ccb->cam_hba_vid, (char *)ext->hc_inf.name, sizeof( ccb->cam_hba_vid ) );

	return( CAM_REQ_CMP );
}

static int sdmmc_sim_action_devctl( SIM_HBA * const hba, CCB * const ccb )
{
	SIM_SDMMC_EXT	*ext;
	CCB_HEADER		*ch;
    CCB_DEVCTL      *dccb;
    int				cstatus;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	ch		= (CCB_HEADER *)ccb;
	dccb	= (CCB_DEVCTL *)ccb;
	cstatus	= CAM_REQ_INPROG;

	if( dccb->cam_devctl_dcmd == DCMD_SDMMC_DRVR_STATE ) {
		SDMMC_DRVR_STATE	*ds;

		ds		= dccb->cam_devctl_data;

		if( ext->drvr_state != ds->state ) {			// see if we are currently in the requested state
			ch->cam_flags |= CAM_SIM_QHEAD;				// insert request at the head of simq.

			switch( ds->state ) {
				case SDMMC_DRVR_PAUSE:
					cstatus = CAM_REQ_INPROG;			// pause request will use the normal enqueue process/pulse
					break;

				case SDMMC_DRVR_RUN: {
					CCB_RELSIM	rccb;

					rccb.cam_ch = *ch;					// copy header information, ie target,lun, etc...
					simq_ccb_enqueue( hba->simq, ccb );
					sdmmc_rel_simq( hba, &rccb );		// release/unfreeze simq
					return( CAM_REQ_CMP );
				}

				default:
					cstatus					= CAM_REQ_CMP;
					dccb->cam_devctl_status	= EINVAL;
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  DCMD_SDMMC_DRVR_STATE Invalid state %d", __FUNCTION__, ds->state );
					break;
			}
		}
		else {
			cstatus					= CAM_REQ_CMP;
			dccb->cam_devctl_status	= EOK;
		}

		if( cstatus ) {
			ch->cam_status	= cstatus;
			if( !( ch->cam_flags & CAM_DIS_CALLBACK ) && dccb->cam_cbfcnp ) {
				(dccb->cam_cbfcnp)( ccb );					// perform callback, note: simq_post_ccb not used since request hasn't be queued
			}
		}
	}

    return( cstatus );
}

// All CCB requests to the SIM are made through this function.
int sdmmc_sim_action( SIM_HBA * const hba, CCB * const ccbp )
{
	SIM_SDMMC_EXT	*ext;
	CCB_HEADER		*ccb;
	int				status;

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	ccb		= (CCB_HEADER *)ccbp;
	status	= CAM_REQ_CMP;

	if( ( ccb->cam_flags & ( CAM_CDB_PHYS | CAM_NXT_CCB_PHYS |
			CAM_CALLBCK_PHYS | CAM_SNS_BUF_PHYS ) ) ) {
		ccb->cam_status = CAM_NO_HBA;
		return( CAM_FAILURE );
	}

	if( ccb->cam_target_id > ext->ntargs ) {
		ccb->cam_status = CAM_TID_INVALID;
		return( CAM_FAILURE );
	}
	else if( ccb->cam_target_lun > ext->targets[ccb->cam_target_id].nluns ) {
		ccb->cam_status = CAM_LUN_INVALID;
		return( CAM_FAILURE );
	}
	else {
		// nothing
	}

	switch( ccb->cam_func_code ) {
		case XPT_NOOP:
			break;

		case XPT_SCSI_IO:
		case XPT_RESET_BUS:
		case XPT_RESET_DEV:
		case XPT_ABORT:
			status = CAM_REQ_INPROG;
			break;

		case XPT_DEVCTL:
			status = sdmmc_sim_action_devctl( hba, ccb );
			if( status == CAM_REQ_CMP ) {
				return( CAM_SUCCESS );
			}
            break;

		case XPT_REL_SIMQ:
			status = sdmmc_rel_simq( hba, (CCB_RELSIM *)ccb );
			break;

		case XPT_TERM_IO:
			status = sdmmc_term_io( hba, (CCB_TERMIO *)ccb );
			break;

		case XPT_PATH_INQ:
			status = sdmmc_path_inq( hba, (CCB_PATHINQ *)ccb );
			break;

		case XPT_GDEV_TYPE:
		case XPT_SDEV_TYPE:
		case XPT_SASYNC_CB:
			// These are not serviced by the SIM, the XPT should do them
			status = CAM_REQ_INVALID;
			break;

		case XPT_EN_LUN:
		case XPT_TARGET_IO:
			status = CAM_FUNC_NOTAVAIL;
			break;

		case XPT_VUNIQUE:
				// No special vendor unique commands
			status = CAM_REQ_INVALID;
			break;

		case XPT_ENG_INQ:
		case XPT_ENG_EXEC:
		default:
			status = CAM_REQ_INVALID;
			break;
	}

	ccb->cam_status = (uchar_t)status;
	if( status == CAM_REQ_INPROG ) {
#ifdef SDMMC_TRACE
		sdmmc_trace_event( SDMMC_TRACE_EVENT, "%s:  ccb %p, cmd %x", __FUNCTION__, ccb, ((CCB_SCSIIO *)ccb)->cam_cdb_io.cam_cdb_bytes[0] );
#endif
		simq_ccb_enqueue( hba->simq, (CCB_SCSIIO *)ccb );
		if( ( pthread_self( ) != hba->tid ) && ( ext->nexus == NULL ) ) {
			if( MsgSendPulse( hba->coid, hba->priority, SIM_ENQUEUE, 0 ) == -1 ) {
			}
		}
	}

	return( CAM_SUCCESS );
}

int sdmmc_sim_attach( CAM_ENTRY * const centry )
{
	SIM_HBA					*hba;
	SIM_HBA					*nhba;
	SIM_SDMMC_EXT			*ext;
	int						busno;
	sdio_device_instance_t	*inst;
	sdio_funcs_t			funcs = { .nfuncs = 3, .insertion = sdmmc_sdio_insertion, .removal = sdmmc_sdio_removal, .event = sdmmc_sdio_event };
	sdio_device_ident_t		interest = { .vid = SDIO_CONNECT_WILDCARD, .did = SDIO_CONNECT_WILDCARD, .dtype = SDIO_CONNECT_WILDCARD, .ccd = SDIO_CONNECT_WILDCARD };
	sdio_connect_parm_t		connect_parm;

	memset( &connect_parm, 0, sizeof( sdio_connect_parm_t ) );
	connect_parm.vsdio	= SDIO_VERSION;
	connect_parm.argc	= sdmmc_ctrl.argc;
	connect_parm.argv	= sdmmc_ctrl.argv;

	memcpy( &connect_parm.funcs, &funcs, sizeof( sdio_funcs_t ) );
	memcpy( &connect_parm.ident, &interest, sizeof( sdio_device_ident_t ) );

	if( ( sdmmc_ctrl.cflags & SDMMC_CFLG_ARGS_ERR ) ) {
		return( CAM_FAILURE );
	}

	sdmmc_ctrl.cflags |= SDMMC_CFLAG_ENUMERATING;

		// null insertion callback on targeted attach
	if( sdmmc_ctrl.nhba && !( sdmmc_ctrl.cflags & SDMMC_CFLAG_SCAN ) ) {
		funcs.insertion = NULL;
	}

	if( sdio_connect( &connect_parm, &sdmmc_ctrl.connection ) != EOK ) {
		return( CAM_FAILURE );
	}

	if( ( sdmmc_ctrl.cflags & SDMMC_CFLAG_SCAN ) ) {
		sdmmc_ctrl.cflags |= SDMMC_CFLAG_ENUMERATING;
		hba = TAILQ_FIRST( &sdmmc_ctrl.hlist );

		for( busno = 0; busno < SDMMC_MAX_BUS ; busno++ ) {
			if( hba == NULL ) {
				hba = sdmmc_alloc_hba( );
				if( hba == NULL ) {
					break;
				}
			}
			ext			= (SIM_SDMMC_EXT *)hba->ext;
			inst		= &ext->instance;
			inst->path	= busno;
			inst->func	= 0;

			if( sdmmc_attach( hba, sdmmc_ctrl.connection, inst ) != CAM_SUCCESS ) {
				if( ext->eflags ) {
						// don't free hba if there are command line options
					ext->instance.path = SDIO_CONNECT_WILDCARD;
					continue;
				}

				sdmmc_detach( hba, CAM_TRUE );
			}

			hba = NULL;
		}
	}
	else {		// attach to specific buses/devices
		for( hba = TAILQ_FIRST( &sdmmc_ctrl.hlist ); hba; hba = nhba ) {
			nhba				= TAILQ_NEXT( hba, hlink );
			ext					= (SIM_SDMMC_EXT *)hba->ext;
			inst				= &ext->instance;
			inst->ident.vid		= (uint16_t)( hba->cfg.Device_ID.DevID & 0xffffU );
			inst->ident.did		= (uint16_t)( hba->cfg.Device_ID.DevID >> 16 );

			if( sdmmc_attach( hba, sdmmc_ctrl.connection, inst ) != CAM_SUCCESS ) {
				sdmmc_detach( hba, CAM_TRUE );
			}
		}
	}

	sdmmc_ctrl.cflags &= ~SDMMC_CFLAG_ENUMERATING;

	sdio_enum( sdmmc_ctrl.connection, SDIO_ENUM_ENABLE );	// enable change detect

	return( CAM_SUCCESS );
}

int sdmmc_sim_args( char *options )
{
	SIM_HBA				*hba;
	SIM_SDMMC_EXT		*ext;
	int					val;
	int					opt;
	int					status;
	char				*value;

	enum {
		OPTION_WITHOUT_ARGS = 0,
			OPTION_NONE = OPTION_WITHOUT_ARGS,

		OPTION_WITH_ARGS,
			OPTION_BUSNO = OPTION_WITH_ARGS,
			OPTION_FUNC,
			OPTION_BKOPS,
			OPTION_CACHE,
			OPTION_PARTITIONS,
			OPTION_PWROFF_NOTIFY,
			OPTION_RELWR,
			OPTION_POWMAN,
			OPTION_SCCBM,
			OPTION_BS,

		OPTION_VAR_ARGS,
	};

	static char *opts[] = {
		[OPTION_NONE]			= "none",		// place holder for option without args

		[OPTION_BUSNO]			= "busno",
		[OPTION_FUNC]			= "func",
		[OPTION_BKOPS]			= "bkops",
		[OPTION_CACHE]			= "cache",
		[OPTION_PARTITIONS]		= "partitions",
		[OPTION_PWROFF_NOTIFY]	= "pwroff_notify",
		[OPTION_RELWR]			= "relwr",
		[OPTION_POWMAN]			= "powman",
		[OPTION_SCCBM]			= "sccbm",
		[OPTION_BS]				= "bs",

		NULL
	};

	if( *options == '\0' ) {		// called when command name includes the module name
		if( sdmmc_ctrl.nhba == 0 ) {
			sdmmc_ctrl.cflags |= SDMMC_CFLAG_SCAN;
		}
		return( CAM_SUCCESS );
	}

	if( sdmmc_ctrl.nhba > SDMMC_MAX_HBA ) {	// max hba's reached
		return( CAM_FAILURE );
	}

	hba = sdmmc_alloc_hba( );
	if( hba == NULL ) {
		return( CAM_FAILURE );
	}

	ext		= (SIM_SDMMC_EXT *)hba->ext;
	status	= EOK;

	while( (*options != '\0') && (status == EOK) ) {
		opt = getsubopt( &options, opts, &value );
		if( opt == -1 ) {
			if( sim_drvr_options( hba, value ) == EOK ) {
				continue;
			}
			cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s: Invalid sdmmc option '%s'", __FUNCTION__, value );
			status = EINVAL;
			break;
		}

		if( opt < OPTION_VAR_ARGS ) {
			if( opt < OPTION_WITH_ARGS ) {
				if( value != NULL ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s: Unexpected argument for '%s'", __FUNCTION__, opts[ opt ] );
					break;
				}
			}
			else {
				if( (value == NULL) || (*value == '\0') ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s: Missing argument for '%s'", __FUNCTION__, opts[ opt ] );
					break;
				}
			}
		}

		switch( opt ) {
// options without args follow


// options with args follow

			case OPTION_BUSNO:				// busno
				val = cam_parse_number( value );
				if( val == CAM_INVALID_NUM ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s:  Invalid busno", __FUNCTION__ );
					status = EINVAL;
					break;
				}
				ext->instance.path = (uint32_t)val;
				break;

			case OPTION_FUNC:				// func
				val = cam_parse_number( value );
				if( val == CAM_INVALID_NUM ) {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s:  Invalid func", __FUNCTION__ );
					status = EINVAL;
					break;
				}
				ext->instance.func = (uint32_t)val;
				break;

			case OPTION_BKOPS:				// bkops
				if( !strcmp( value, "on" ) ) {
					ext->eflags |= SDMMC_EFLAG_BKOPS;
				}
				else if( !strcmp( value, "auto" ) ) {
					ext->eflags |= SDMMC_EFLAG_BKOPS_AUTO;
				}
				else {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Invalid bkops", __FUNCTION__ );
					status = EINVAL;
				}
				break;

			case OPTION_CACHE:				// cache
#ifdef SDMMC_CACHE_SUP
				if( !strcmp( value, "on" ) ) {
					ext->eflags |= SDMMC_EFLAG_CACHE;
				}
#endif
				break;

			case OPTION_PARTITIONS:		// partitions
				if( !strcmp( value, "on" ) ) {
					ext->eflags |= SDMMC_EFLAG_PARTITIONS;
				}
				break;

			case OPTION_PWROFF_NOTIFY:		// pwroff_notify
				if( !strcmp( value, "short" ) ) {
					ext->eflags |= SDMMC_EFLAG_PWROFF_NOTIFY;
					ext->pwroff_notify = SDIO_POWER_OFF_SHORT;
				}
				else if( !strcmp( value, "long" ) ) {
					ext->eflags |= SDMMC_EFLAG_PWROFF_NOTIFY;
					ext->pwroff_notify = SDIO_POWER_OFF_LONG;
				}
				else {
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 1, 1, "%s:  Invalid power off notification mode", __FUNCTION__ );
					status = EINVAL;
				}
				break;

			case OPTION_RELWR:				// relwr reliable write
				if( !strcmp( value, "on" ) ) {
					ext->eflags |= SDMMC_EFLAG_RELWR;
				}
				break;

			case OPTION_POWMAN:				// powman
				ext->eflags |= SDMMC_EFLAG_POWMAN;
				if( ext->powman_name != NULL ) {
					free( ext->powman_name );
				}
				ext->powman_name = strdup( value );
				break;

			case OPTION_SCCBM:						// sccbm=max set max loops in sdmmc_start_ccb fcn
				val = cam_parse_number( value );
				if( (val == CAM_INVALID_NUM) || (val == 0) ) {	// 1 ..
					cam_slogf( _SLOGC_SIM_MMC, _SLOG_ERROR, 0, 0, "%s:  Invalid start ccb max", __FUNCTION__ );
					status = EINVAL;
					break;
				}
				ext->start_ccb_max = val;
				break;

			case OPTION_BS:
				status = sim_bs_args( hba, value );
				break;

// options with variable args follow

			default:
				break;
		}
	}

	if( ext->instance.path == SDIO_CONNECT_WILDCARD ) {
		sdmmc_ctrl.cflags |= SDMMC_CFLAG_SCAN;
		if( hba->priority ) {
			sdmmc_ctrl.priority = hba->priority;
		}
	}

	if( status != EOK ) {
		sdmmc_free_hba( hba );
		sdmmc_ctrl.cflags |= SDMMC_CFLG_ARGS_ERR;
	}

	return( ( status == EOK ) ? CAM_SUCCESS : CAM_FAILURE );
}

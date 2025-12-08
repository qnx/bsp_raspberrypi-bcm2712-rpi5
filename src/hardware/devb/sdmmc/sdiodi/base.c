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

// Module Description:

#include <stdio.h>
#include <errno.h>
#include <atomic.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/trace.h>
#include <sys/slogcodes.h>

#include <internal.h>

#include <xpt.h>

sdio_ctrl_t				sdio_ctrl;

// tuning block pattern for 4 bit mode
const uint8_t	sdio_tbp_4bit[] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};

// tuning block pattern for 8 bit mode
const uint8_t	sdio_tbp_8bit[] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

__attribute__((used))
static void sdio_trace_event( const uint32_t event, const char * const fmt, ... )
{
	char		buf[255];
	va_list		arglist;

	va_start( arglist, fmt );
	vsnprintf( buf, 254, fmt, arglist );
	va_end( arglist );
	TraceEvent( _NTO_TRACE_INSERTUSRSTREVENT, event, buf );
}

ssize_t __attribute__((__format__(__printf__, 5, 6))) sdio_slogf( const int opcode, const int severity, const int verbosity, const int vlevel, const char * const fmt, ... )
{
	ssize_t		ret;
	va_list		arglist;

	ret = 0;

	if( verbosity > 5 ) {
		va_start( arglist, fmt );
		vfprintf( stderr, fmt, arglist );
		va_end( arglist );
		fprintf( stderr, "\n" );
	}

	if( verbosity >= vlevel ) {
		va_start( arglist, fmt );
		ret = vslogf( opcode, severity, fmt, arglist );
		va_end( arglist );
	}

	return( ret );
}

// return position of MSB
int fls( int val )
{
	int		idx;

	idx = 32;
	while( val ) {
		if( ( val & 0x80000000u ) ) return( idx );
		val <<= 1;
		idx--;
	}
	return( 0 );
}

void *sdio_alloc( const size_t size )
{
	void		*vaddr;

	vaddr = xpt_alloc( XPT_ALLOC_CONTIG | XPT_ALLOC_NOCACHE, size, NULL );
	if( vaddr == MAP_FAILED ) {
		vaddr = NULL;
	}

	return( vaddr );
}

int sdio_free( void * const ptr, const size_t size )
{
	return( xpt_free( ptr, size ) );
}

sdio_hc_t *sdio_hc_alloc( void )
{
	sdio_hc_t		*hc;
	sdio_hc_cfg_t	*cfg;

	if( ( hc = calloc( 1, sizeof( sdio_hc_t ) ) ) ) {
		hc->path		= sdio_ctrl.nhc++;
		hc->priority	= sdio_ctrl.priority;
		cfg				= &hc->cfg;
		cfg->idx		= -1;
		cfg->caps		= ~0;
		cfg->verbosity	= sdio_ctrl.verbosity;
		cfg->idle_time	= SDIO_PM_IDLE_TIME;
		cfg->sleep_time	= SDIO_PM_SLEEP_TIME;
		hc->hc_coid		= -1;
		hc->hc_chid		= -1;
		hc->hc_tid		= -1;
		hc->tuning_timerid = -1;
		TAILQ_INSERT_TAIL( &sdio_ctrl.hlist, hc, hlink );
	}

	return( hc );
}

int sdio_hc_free( sdio_hc_t *hc )
{
//	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: hc %p", __FUNCTION__, hc );

	TAILQ_REMOVE( &sdio_ctrl.hlist, hc, hlink );
	sdio_ctrl.nhc--;
	free( hc );

	return( EOK );
}

int sdio_vtop_sg( sdio_sge_t * const vsg, sdio_sge_t * const psg, const int sgc, void * const mhdl )
{
	return( xpt_vtop_sg( (SG_ELEM *)vsg, (SG_ELEM *)psg, sgc, mhdl ) );
}

paddr64_t sdio_vtop( void * const vaddr )
{
	return( xpt_vtop( vaddr, NULL ) );
}

static char *sdio_module_args( const char *module, const int argc, char * const argv[], int occurrence )
{
	char	*cp;
	int		n;

	if( argc < 2 ) {		// need at least 2 args
		return( NULL );
	}

	for( n = 0; n < argc - 1; ++n ) {
		if( !strcmp( argv[n], module ) ) {
			if( !--occurrence ) {
				cp = strdup( argv[n + 1] );
				return( cp );
			}
		}
	}
	return( NULL );
}

static int64_t sdio_parse_number64( const char *str )
{
	char	*cp;
	int64_t	number;

	if( ( str == NULL ) || ( *str == '\0' ) ) {
		return( SDIO_INVALID_NUM );
	}

	number = strtoll( str, &cp, 0 );
	if( ( !number && ( cp == str ) ) || ( number < 0 ) || ( *cp != '\0' ) ) {
		return( SDIO_INVALID_NUM );
	}

	return( number );
}

static int sdio_parse_number( const char *str )
{
	char	*cp;
	int		number;

	if( ( str == NULL ) || ( *str == '\0' ) ) {
		return( SDIO_INVALID_NUM );
	}

	number = strtol( str, &cp, 0 );
	if( ( !number && ( cp == str ) ) || ( number < 0 ) || ( *cp != '\0' ) ) {
		return( SDIO_INVALID_NUM );
	}
	return( number );
}

static char *sdio_strchr( char *string, char token )
{
	char	*cp;
	int		escaped;

	escaped = 0;
	for( cp = string; *cp != '\0'; ++cp ) {
		if( escaped ) {
			escaped = 0;
			--cp;
			memmove( cp, cp + 1, strlen( cp ) );
		}
		else if( *cp == '\\' ) {
			escaped = !0;
		}
		else if( *cp == token ) {
			return( cp );
		}
		else {
			// nothing
		}
	}
	return( NULL );
}

static int sdio_parse_tuple( char *string, char separator, char **entry, ... )
{
	va_list	args;
	char	*cp;
	int		nargs;

	nargs = 0;
	va_start( args, entry );
	while( ( string != NULL ) || ( entry != NULL ) ) {
		cp = (string != NULL) ? sdio_strchr( string, separator ) : NULL;
		if( entry != NULL ) {
			*entry = string;
			++nargs;
			entry = va_arg( args, char ** );
		}
		if( cp != NULL ) {
			*cp++ = '\0';
		}
		string = cp;
	}
	va_end( args );
	return( nargs );
}

/**
 * Use ":" as separator, other than the regular ","
 * @param optionp Option pointer
 * @param tokens Tokens
 * @param valuep Value pointer
 *
 * @retval -1 if index out of bounds.
 * @retval Index value.
 */
int sdio_hc_getsubopt( char **optionp, char * const *tokens, char **valuep )
{
	char		*p;
	char		*opt;
	int			len;
	int			index;
	const char	*token;

	*valuep	= NULL;
	opt		= *optionp;

	for( p = opt, len = 0; *p && ( *p != ':' ); p++, len++ ) {
		if( *p == '=' ) {
			for( *valuep = ++p; *p && ( *p != ':' ); p++ ) {
				// Nothing to do
			}
			break;
		}
	}

	if( *p ) {
		*p++ = '\0';
	}

	*optionp = p;

	for( index = 0; ( token = *tokens++ ); index++ ) {
		if( ( *token == *opt ) && (len > 0) &&!strncmp( token, opt, len ) && ( token[len] == '\0' ) ) {
			return( index );
		}
	}

	*valuep = opt;

	return( -1 );
}

int sdio_set_thread_state( uint32_t *tstate, const int state )
{
	pthread_sleepon_lock( );
	*tstate = state;
	pthread_sleepon_signal( tstate );
	pthread_sleepon_unlock( );
	return( EOK );
}

int sdio_create_thread( pthread_t *tid, pthread_attr_t *aattr, void *(*func)(void *), void * const arg, const int priority, uint32_t *tstate, char * const name )
{
	pthread_t			_tid;
	pthread_attr_t		*pattr;
	pthread_attr_t		attr;
	struct sched_param	param = {0};
	int					status;

	if( tid == NULL ) {
		tid = &_tid;
	}

	if( tstate ) {
		*tstate = SDIO_TSTATE_CREATING;
	}

	pattr = aattr;
	if( aattr == NULL ) {
		pattr = &attr;
		pthread_attr_init( &attr );
		pthread_attr_setschedpolicy( &attr, SCHED_RR );
		param.sched_priority = priority;
		pthread_attr_setschedparam( &attr, &param );
		pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
		pthread_attr_setstacksize( &attr, SDIO_STACK_SIZE );
	}

	status = pthread_create( tid, pattr, func, arg );
	if( aattr == NULL ) {
		pthread_attr_destroy( &attr );
	}
	if( status ) {
		return( status );
	}

	if( name ) {
#if ( _NTO_VERSION >= 632 )
		if( pthread_setname_np( *tid, name ) ) {
//			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s:  pthread_setname_np %s", __FUNCTION__, strerror( status ) );
		}
#endif
	}

	if( tstate ) {
		pthread_sleepon_lock( );
		while( !( *tstate ) ) {
			pthread_sleepon_wait( tstate );
		}
		pthread_sleepon_unlock( );
		status = (*tstate == SDIO_TSTATE_INITIALIZED) ? EOK : EIO;
	}

	return( status );
}

uint32_t sdio_extract_bits( uint32_t * const data, const int bits, const int start, const int size )
{
	int			idx;
	int			shift;
	uint32_t	value;

	idx		= ( bits / 32 ) - ( start / 32 ) - 1;

	shift	= start & 31;
	value	= data[idx] >> shift;
	if( size + shift > 32 ) {
		value |= data[idx - 1] << ( 32 - shift );
	}

	return( value & ( ( 1llu << size ) - 1 ) );
}

int sdio_sg_start( sdio_hc_t *hc, sdio_sge_t * const sge, const uint32_t nsg )
{
	sdio_wspc_t	*wspc;

	wspc		= &hc->wspc;

	if( !nsg && ( sge == NULL ) ) {
		return( EINVAL );
	}

	wspc->nsg	= nsg;
	wspc->sge	= sge;
	wspc->sgc	= sge->sg_count;
	wspc->sga	= SDIO_DATA_PTR_V( sge->sg_address );

	return( EOK );
}

// return:  SDIO_TRUE - complete / SDIO_FALSE - more to do
int sdio_sg_nxt( sdio_hc_t *hc, uint8_t **addr, uint32_t *len, const uint32_t blksz )
{
	sdio_wspc_t	*wspc;

	wspc = &hc->wspc;

	if( !wspc->nsg && !wspc->sgc ) {		// sge's complete
		return( SDIO_TRUE );
	}

	if( !wspc->sgc ) {						// advance to next sg entry
		wspc->sge++;
		wspc->nsg--;
		wspc->sgc	= wspc->sge->sg_count;
		wspc->sga	= SDIO_DATA_PTR_V( wspc->sge->sg_address );
	}

	*addr		= wspc->sga;
	*len		= min( wspc->sgc, blksz );
	wspc->sga	+= *len;
	wspc->sgc	-= *len;

	return( SDIO_FALSE );
}

sdio_device_errata_t *sdio_device_errata( sdio_dev_t * const dev, sdio_device_errata_t * const erratas )
{
	sdio_cid_t				*cid;
	sdio_ecsd_t		*ecsd;
	sdio_device_errata_t	*errata;

	cid	= &dev->cid;
	ecsd	= &dev->ecsd;
	for( errata = erratas; errata->pnm; errata++ ) {
		if( !strncmp( cid->pnm, errata->pnm, strlen( errata->pnm ) ) ) {
			if( ( ( cid->mid == errata->mid ) || ( errata->mid == DEV_ERRATA_WILDCARD ) ) &&
					( ( cid->oid == errata->oid ) || ( errata->oid == DEV_ERRATA_WILDCARD ) ) &&
					( ( ecsd->ext_csd_rev == errata->erev ) || ( errata->erev == DEV_ERRATA_WILDCARD ) ) &&
					( cid->prv >= errata->prv_s ) && ( cid->prv <= errata->prv_e ) ) {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, 1, 1, "%s:  pnm %s, mid 0x%x, oid 0x%x, rev 0x%x:0x%x, errata 0x%x, rsettle %dms", __FUNCTION__, errata->pnm, errata->mid, errata->oid, errata->prv_s, errata->prv_e, errata->errata, errata->rsettle );
				return( errata );
			}
		}
	}

	return( NULL );
}

int sdio_reconcile_errata( sdio_dev_t * const dev, sdio_device_errata_t * const errata )
{
	sdio_hc_t	*hc;

	hc = dev->hc;

	if( ( errata->errata & DEV_ERRATA_ACMD12 ) ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, 1, 1, "%s:  disabling ACMD12", __FUNCTION__ );
		hc->caps &= ~HC_CAP_ACMD12;
	}

	return( EOK );
}

static int sdio_pwr( sdio_hc_t * const hc, const uint32_t vdd )
{
	int					bpos;
	static const char	*name[] = { "0.0", "", "", "", "", "", "", "1.7-1.9",
		"2.0", "2.1", "2.2", "2.3", "2.4", "2.5", "2.6",
		"2.7", "2.8", "2.9", "3.0", "3.1", "3.2", "3.3",
		"3.4", "3.5", "3.6", "", "", "", "", "", "" };

	if( ( bpos = ffs( vdd ) ) ) {
		bpos--;
	}

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  vdd %s (0x%x)", __FUNCTION__, name[bpos], vdd );
	return( hc->entry.pwr( hc, vdd ) );
}

int sdio_drv_type( sdio_hc_t * const hc, const uint32_t drv_type )
{
	static const char	*name[5] = { "B", "A", "C", "", "D" };

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  type %s", __FUNCTION__, name[drv_type >> 1] );

	return( hc->entry.drv_type( hc, drv_type ) );
}

int sdio_bus_mode( sdio_hc_t * const hc, const uint32_t bus_mode )
{
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  mode %s", __FUNCTION__, bus_mode ? "push pull" : "open drain" );

	return( hc->entry.bus_mode( hc, bus_mode ) );
}

int sdio_bus_width( sdio_hc_t * const hc, const uint32_t bus_width )
{
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  width %d", __FUNCTION__, bus_width );

	return( hc->entry.bus_width( hc, bus_width ) );
}

int sdio_clock( sdio_hc_t * const hc, uint32_t clk )
{
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  clk %d", __FUNCTION__, clk );

	if( clk < hc->clk_min ) {
		return( EINVAL );
	}

	if( clk > hc->clk_max ) {
		clk = hc->clk_max;
	}

	return( hc->entry.clk( hc, clk ) );
}

int sdio_timing( sdio_hc_t * const hc, const uint32_t timing )
{
	static const char	*name[11] = { "INVALID", "LS", "HS", "HS DDR50", "SDR12", "SDR25", "SDR50", "SDR104", "HS200", "HS400", "HS400ES" };

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  timing %s", __FUNCTION__, name[timing] );

	return( hc->entry.timing( hc, timing ) );
}

int sdio_signal_voltage( sdio_hc_t * const hc, const uint32_t voltage )
{
	static const char	*name[5] = { "0.0", "3.3", "3.0", "1.8", "1.2" };

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  %sV", __FUNCTION__, name[voltage] );

	return( hc->entry.signal_voltage( hc, voltage ) );
}

int _sdio_pwrmgnt( sdio_dev_t * const dev, uint32_t pm )
{
	sdio_hc_t	*hc;
	int			status;

	hc		= dev->hc;
	status	= EOK;

	if( ( hc->entry.pm == NULL ) || ( pm == hc->pm_state ) || hc->wspc.cmd ) {
		return( EOK );
	}

	switch( pm ) {
		case PM_IDLE:
//			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s: idle", __FUNCTION__ );

			if( hc->pm_state == PM_ACTIVE ) {
				hc->entry.pm( hc, pm );
			}
			else {
				return( EOK );
			}
			break;

		case PM_ACTIVE:
//			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s (path %d): active", __FUNCTION__, hc->path );

			hc->entry.pm( hc, pm );
			if( hc->pm_state == PM_SUSPEND ) {
				hc->pm_state = pm;
				_sdio_reset( dev );

					// start change detect timer
				if( !( hc->caps & HC_CAP_CD_INTR ) && !( hc->caps & HC_CAP_SLOT_TYPE_EMBEDDED ) ) {
					sdio_timer_settime( sdio_ctrl.cd_timerid, SDIO_CD_INTERVAL, 0, SDIO_TRUE );
				}
			}
			else if( ( hc->pm_state == PM_SLEEP ) && ( hc->caps & HC_CAP_SLEEP ) ) {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s (path %d): sleep to active", __FUNCTION__, hc->path );
				hc->pm_state = pm;
				if( ( dev->caps & DEV_CAP_SLEEP ) ) {
					status = mmc_sleep_awake( dev, 0 );
					if( status == EOK ) {
						status = sdio_select_card( dev, dev->rca );
					}
					if( status ) {
						sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: awake fail", __FUNCTION__ );
						status = _sdio_reset( dev );
					}
				}
				else {
					if( ( dev->flags & DEV_FLAG_PRESENT ) ) {
						status = _sdio_reset( dev );
					}
				}
			}
			else {
				// nothing
			}

			if( hc->tuning_count ) {	// start tuning timer
				if( sdio_timer_settime( hc->tuning_timerid, hc->tuning_count, 0, SDIO_FALSE ) ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 1, 1, "%s: sdio_timer_settime (tuning start)", __FUNCTION__ );
				}
			}
			break;

		case PM_SLEEP:
//			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s: sleep", __FUNCTION__ );

			if( hc->pm_state == PM_IDLE ) {		// wakeup when idle to deselect
				hc->entry.pm( hc, PM_ACTIVE );
			}

			if( ( hc->caps & HC_CAP_SLEEP ) ) {
				if( ( dev->caps & DEV_CAP_SLEEP ) ) {
					if( status == EOK ) {
						status = sdio_select_card( dev, 0 );
						if( status == EOK ) {
// Should we wait for STANDBY state before putting the device to sleep?
//							if( ( status = _sdio_wait_card_status( dev, NULL, CDS_CUR_STATE_MSK, CDS_CUR_STATE_STANDBY, SDIO_TIME_DEFAULT ) ) == EOK ) {
								if( ( dev->dtype == DEV_TYPE_MMC ) ) {
									status = mmc_sleep_awake( dev, 1 );
								}
//							}
						}

						if( status ) {
							_sdio_reset( dev );
						}
					}
				}

				if( status ) {
					pm = PM_IDLE;							// set state back to idle
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: sleep fail", __FUNCTION__ );
				}
				else {
					if( hc->tuning_count ) {				// stop tuning timer
						if( sdio_timer_settime( hc->tuning_timerid, 0, 0, SDIO_FALSE ) ) {
							sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 1, 1, "%s: sdio_timer_settime (tuning stop)", __FUNCTION__ );
						}
					}
					sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s (path %d): sleep hc cnt %"PRId64"", __FUNCTION__, hc->path, ++hc->pm_sleep_cnt );
				}
				hc->entry.pm( hc, pm );
			}
			break;

		case PM_SUSPEND:
			if( hc->tuning_count ) {				// stop tuning timer
				if( sdio_timer_settime( hc->tuning_timerid, 0, 0, SDIO_FALSE ) ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 1, 1, "%s: sdio_timer_settime (tuning stop)", __FUNCTION__ );
				}
			}
				// stop change detect timer
			if( !( hc->caps & HC_CAP_CD_INTR ) && !( hc->caps & HC_CAP_SLOT_TYPE_EMBEDDED ) ) {
				sdio_timer_settime( sdio_ctrl.cd_timerid, 0, 0, SDIO_FALSE );
			}

			hc->entry.pm( hc, pm );
			break;

		default:
			break;
	}

	hc->pm_state = pm;

	return( status );
}

int sdio_select_voltage( sdio_hc_t *hc, uint32_t ocr )
{
	sdio_dev_t *dev;
	int			bpos;
	int			status;

	ocr	&= hc->ocr;
	dev = &hc->device;

	if( ( bpos = ffs( ocr ) ) ) {
		ocr = 1U << ( bpos - 1 );
		status = sdio_pwr( hc, ocr );
	}
	else {
		ocr		= 0;
		status	= EINVAL;
	}

	dev->ocr = ocr;

	return( status );
}

int sdio_tune( sdio_hc_t * const hc, const uint32_t cmd )
{
	int	status;

	status = EOK;

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:", __FUNCTION__ );

	if( hc->entry.tune ) {
		status = hc->entry.tune( hc, cmd );
	}

	return( status );
}

int sdio_preset( sdio_hc_t * const hc, const uint32_t state )
{
	int	status;

	status = EOK;

	if( hc->entry.preset ) {
		status = hc->entry.preset( hc, state );
	}

	return( status );
}

static int sdio_power( sdio_hc_t *hc, const uint32_t pwr )
{
	switch( pwr ) {
		case SDIO_PWR_ON:
				// set a few defaults
			hc->clk				= hc->clk_init;
			hc->timing			= TIMING_LS;
			hc->bus_width		= BUS_WIDTH_1;
			hc->bus_mode		= BUS_MODE_OPEN_DRAIN;
			hc->signal_voltage	= SIGNAL_VOLTAGE_3_3;

			sdio_pwr( hc, 1U << ( fls( (int)hc->ocr ) - 1 ) );
			sdio_clock( hc, hc->clk_init );
			sdio_bus_width( hc, BUS_WIDTH_1 );
			sdio_preset( hc, SDIO_FALSE );
			sdio_timing( hc, TIMING_LS );
			sdio_bus_mode( hc, BUS_MODE_OPEN_DRAIN );
			sdio_signal_voltage( hc, SIGNAL_VOLTAGE_3_3 );
			delay( 10 );		// pwr up delay
			break;

		case SDIO_PWR_OFF:
		default:
			sdio_pwr( hc, 0 );
			break;
	}

	return( EOK );
}

int _sdio_send_status( sdio_dev_t * const dev, uint32_t * const rsp, const int hpi )
{
	struct sdio_cmd		*cmd;
	uint32_t			arg;
	int					status;

	memset( rsp, 0, SDIO_RSP_SIZE );

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	arg = ( dev->rca << 16 ) | ( hpi ? MMC_SEND_STATUS_HPI: 0 );

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, MMC_SEND_STATUS, arg );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
	if( status == EOK ) {
		memcpy( rsp, cmd->rsp, sizeof( cmd->rsp ) );
	}

	sdio_free_cmd( cmd );

	return( status );
}

int sdio_send_csd( sdio_dev_t * const dev, uint32_t *csd )
{
	struct sdio_cmd		*cmd;
	int					status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R2, MMC_SEND_CSD, dev->rca << 16 );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
	if( status == EOK ) {
		csd[0] = cmd->rsp[0];
		csd[1] = cmd->rsp[1];
		csd[2] = cmd->rsp[2];
		csd[3] = cmd->rsp[3];
	}

	sdio_free_cmd( cmd );

	return( status );
}

int sdio_select_card( sdio_dev_t *dev, const uint32_t rca )
{
	struct sdio_cmd		*cmd;
	int					status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | ( rca ? SCF_RSP_R1: SCF_RSP_NONE ),
							MMC_SEL_DES_CARD, rca << 16 );

	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );

	if( ( cmd->rsp[0] & CDS_CARD_IS_LOCKED ) ) {
		dev->flags |= DEV_FLAG_LOCKED;
	}

	sdio_free_cmd( cmd );

	return( status );
}

int _sdio_stop_transmission( sdio_dev_t * const dev, const int hpi )
{
	struct sdio_cmd		*cmd;
	uint32_t			arg;
	int					status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	arg = hpi ? ( ( dev->rca << 16 ) | MMC_STOP_TRANSMISSION_HPI ): 0;

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1B, MMC_STOP_TRANSMISSION, arg );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 /* SDIO_CMD_RETRIES */ );
	sdio_free_cmd( cmd );

	return( status );
}

int _sdio_set_block_length( sdio_dev_t *dev, const uint32_t blklen )
{
	struct sdio_cmd		*cmd;
	int					status;

		// cmd isn't supported in DDR/HS400/HC
	if( ( dev->flags & ( DEV_FLAG_DDR | DEV_FLAG_HS400 | DEV_FLAG_HS400ES) ) && ( dev->caps & DEV_CAP_HC ) ) {
		return( EOK );
	}

	if( dev->block_length == blklen ) {
		return( EOK );
	}

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, MMC_SET_BLOCKLEN, blklen );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
	if( status == EOK ) {
		dev->block_length = blklen;
	}

	sdio_free_cmd( cmd );

	return( status );
}

int _sdio_set_block_count( sdio_dev_t * const dev, const uint32_t blkcnt, const uint32_t flgs )
{
	struct sdio_cmd		*cmd;
	int					status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, MMC_SET_BLOCK_COUNT, blkcnt | flgs );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	sdio_free_cmd( cmd );

	return( status );
}

int _sdio_cache( sdio_dev_t * const dev, const int op, const uint32_t timeout )
{
	int			status;

	if( ( dev->dtype == DEV_TYPE_MMC ) ) {
		status = mmc_cache( dev, op, timeout );
	}
	else {
		status = sd_cache( dev, op, timeout );
	}

	return( status );
}

__attribute__((used))
static int sdio_set_upper_addr( sdio_dev_t * const dev, const uint32_t ua )
{
	struct sdio_cmd		*cmd;
	int					status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, SD_SET_UPPER_ADDR, ua );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );

	sdio_free_cmd( cmd );

	return( status );
}

static void sdio_rsp( sdio_dev_t * const dev, uint32_t * const rsp )
{
	sdio_hc_t			*hc;
	static const char	*state[16] = {	"IDLE", "READY", "IDENT", "STANDBY",
										"TRAN", "DATA", "RCV", "PRG", "DIS", "BTST", "SLP", "RSVD", "RSVD", "RSVD", "RSVD", "RSVD" };

	hc		= dev->hc;

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "rsp 0x%x state %s %s", rsp[0], state[ CDS_CUR_STATE( rsp[0] ) ], ( rsp[0] & CDS_READY_FOR_DATA ) ? "Ready for DATA" : "" );

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		(rsp[0] & CDS_OUT_OF_RANGE			) ? "OOR " : "",
		(rsp[0] & CDS_ADDRESS_ERROR			) ? "ADDR" : "",
		(rsp[0] & CDS_BLOCK_LEN_ERROR		) ? "BLEN " : "",
		(rsp[0] & CDS_ERASE_SEQ_ERROR		) ? "ESEQ " : "",
		(rsp[0] & CDS_ERASE_PARAM			) ? "EPARM " : "",
		(rsp[0] & CDS_WP_VIOLATION			) ? "WPV " : "",
		(rsp[0] & CDS_CARD_IS_LOCKED		) ? "CIL " : "",
		(rsp[0] & CDS_LOCK_UNLOCK_FAILED	) ? "LUF " : "",
		(rsp[0] & CDS_COM_CRC_ERROR			) ? "CRC " : "",
		(rsp[0] & CDS_ILLEGAL_COMMAND		) ? "ILC " : "",
		(rsp[0] & CDS_CARD_ECC_FAILED		) ? "ECC " : "",
		(rsp[0] & CDS_CC_ERROR				) ? "CC " : "",
		(rsp[0] & CDS_ERROR					) ? "ERR " : "",
		(rsp[0] & CDS_UNDERRUN				) ? "UR " : "",
		(rsp[0] & CDS_OVERRUN				) ? "OR " : "",
		(rsp[0] & CDS_CID_CSD_OVERWRITE		) ? "CID " : "",
		(rsp[0] & CDS_WP_ERASE_SKIP			) ? "WPES " : "",
		(rsp[0] & CDS_CARD_ECC_DISABLED		) ? "CECCD " : "",
		(rsp[0] & CDS_ERASE_RESET			) ? "ER " : "",
		(rsp[0] & CDS_SWITCH_ERROR			) ? "SE " : "",
		(rsp[0] & CDS_URGENT_BKOPS			) ? "BKOPS " : "",
		(rsp[0] & CDS_APP_CMD_S				) ? "APP " : "" );
}

int _sdio_wait_card_status( sdio_dev_t * const dev, uint32_t *rsp, const uint32_t mask, const uint32_t val, uint32_t msec )
{
	sdio_hc_t	*hc;
	int			status;
	uint32_t	resp[4];

	hc		= dev->hc;
	status	= EOK;
	rsp		= rsp ? rsp : resp;

	for( msec = max( msec, 1 ); msec; msec-- ) {
		status = _sdio_send_status( dev, rsp, SDIO_FALSE );
		if( status != EOK ) {
			break;
		}

		if( hc->cfg.verbosity > 3 ) {
			sdio_rsp( dev, rsp );
		}

		if( ( rsp[0] & CDS_ERROR_MSK ) ) {
			status = EIO;
			break;
		}

		if( ( rsp[0] & mask ) == val ) {
			status = EOK;
			break;
		}

		delay( 1 );
	}

	if( !msec ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  mask %x, val %x, card status %x", __FUNCTION__, mask, val, rsp[0] );
		sdio_rsp( dev, rsp );
		status = ETIMEDOUT;
	}

	return( status );
}

static uint64_t sdio_erase_timeout( sdio_dev_t * const dev, const uint32_t etype, const uint32_t nlba )
{
	uint64_t		timeout;

	if( dev->dtype == DEV_TYPE_MMC ) {
		timeout = mmc_erase_timeout( dev, etype, nlba );
	}
	else {
		timeout = sd_erase_timeout( dev, etype, nlba );
	}

	return( max( timeout, 1000 ) );
}

int _sdio_lock_unlock( sdio_dev_t *dev, const int op, uint8_t * const pwd, const uint32_t pwd_len )
{
	struct sdio_cmd		*cmd;
	sdio_sge_t			sge;
	uint8_t				*ld;
	uint32_t			len;
	int					status;
	int					timeout;
	uint32_t			rsp[4];

	len = pwd_len + 2;	// +2 for op field, and pwd_len field

	if( ( len & 0x1 ) ) {
		len++;
	}

	timeout = ( op & SD_LU_ERASE ) ? sdio_erase_timeout( dev, SD_ERASE_NORM, dev->csd.sectors ): SDIO_TIME_DEFAULT;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	ld = sdio_alloc( len );
	if( ld == NULL ) {
		sdio_free_cmd( cmd );
		return( ENOMEM );
	}

	ld[0] = op;
	ld[1] = pwd_len;
	if( pwd_len ) {
		memcpy( &ld[2], pwd, pwd_len );
	}

	status = _sdio_set_block_length( dev, len );
	if( status == EOK ) {
		sdio_setup_cmd( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1, MMC_LOCK_UNLOCK, 0 );
		sge.sg_count	= len;
		sge.sg_address	= SDIO_DATA_PTR_P( ld );
		sdio_setup_cmd_io( cmd, SCF_DIR_OUT, 1, len, &sge, 1, NULL );

		status = _sdio_send_cmd( dev, cmd, NULL, timeout, SDIO_CMD_RETRIES );
		_sdio_set_block_length( dev, SDIO_DFLT_BLKSZ );
	}

	if( status == EOK ) {
		status = _sdio_send_status( dev, rsp, SDIO_FALSE );
		if( status  == EOK ) {
			if( ( rsp[0] & CDS_CARD_IS_LOCKED ) ) {
				atomic_set( &dev->flags, DEV_FLAG_LOCKED );
			}

			if( ( dev->flags & DEV_FLAG_LOCKED ) && !( rsp[0] & CDS_CARD_IS_LOCKED ) ) {
				atomic_clr( &dev->flags, DEV_FLAG_LOCKED );
			}
		}
	}

	sdio_free( ld, len );
	sdio_free_cmd( cmd );

	return( status );
}

int sdio_go_idle( sdio_hc_t *hc )
{
	sdio_dev_t			*dev;
	struct sdio_cmd		*cmd;
	int					status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	dev	= &hc->device;

	sdio_setup_cmd( cmd, SCF_CTYPE_BC | SCF_RSP_NONE, MMC_GO_IDLE_STATE, 0 );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	sdio_free_cmd( cmd );

	return( status );
}

int sdio_all_send_cid( sdio_hc_t *hc, uint32_t *cid )
{
	sdio_dev_t		*dev;
	struct sdio_cmd	*cmd;
	int				status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	dev	= &hc->device;

	sdio_setup_cmd( cmd, SCF_CTYPE_BCR | SCF_RSP_R2, MMC_ALL_SEND_CID, 0 );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
	if( status == EOK ) {
		cid[0] = cmd->rsp[0];
		cid[1] = cmd->rsp[1];
		cid[2] = cmd->rsp[2];
		cid[3] = cmd->rsp[3];
	}

	sdio_free_cmd( cmd );

	return( status );
}

int _sdio_reset( sdio_dev_t *dev )
{
	sdio_hc_t	*hc;
	int			retry;
	int			status;

	hc		= dev->hc;
	status	= EOK;
	retry	= SDIO_RESET_RETRIES;

	if( ( hc->flags & HC_FLAG_RST ) ) {
		return( EBUSY );
	}

	sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 1, "%s (path %d):  ", __FUNCTION__, hc->path );

	hc->flags		|= HC_FLAG_RST;
	hc->flags		&= ~HC_FLAG_SKIP_PWRUP;
	dev->rca		= 0;
	dev->pactive	= 0;
	dev->flags		&= ~DEV_FLAG_WCE;

	do {
		sdio_power( hc, SDIO_PWR_OFF );
		sdio_power( hc, SDIO_PWR_ON );

		switch( dev->dtype ) {
			case DEV_TYPE_MMC:
				status = mmc_init_device( hc, dev->ocr, SDIO_TRUE );
				break;

			case DEV_TYPE_SD:
				status = sd_init_device( hc, dev->ocr, SDIO_TRUE );
				break;

			default:
				status = ENXIO;
				break;
		}

		if( status == ENXIO ) {						// device has been removed
			sdio_hc_event( hc, HC_EV_CD );
			break;
		}

	} while( --retry && ( status != EOK ) );

	hc->flags &= ~HC_FLAG_RST;

	return( status );
}

int _sdio_retune( sdio_hc_t *hc )
{
	int			status;

	status = EOK;

	if( hc->tuning_count ) {
		if( sdio_timer_settime( hc->tuning_timerid, hc->tuning_count, 0, SDIO_FALSE ) ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 1, 1, "%s: sdio_timer_settime (tuning start)", __FUNCTION__ );
		}
	}

	if( hc->device.dtype == DEV_TYPE_MMC ) {
		status = mmc_retune( hc );
	}
	else {
		status = sdio_tune( hc, SD_SEND_TUNING_BLOCK );
	}

	atomic_clr( &hc->flags, HC_FLAG_TUNE );

	return( status );
}

int sdio_wait_cmd( sdio_hc_t *hc, struct sdio_cmd * const cmd, const uint64_t tms )
{
	uint64_t		ct;
	int				status;
	struct timespec	abstime;

	status = EOK;

	ct = _syspage_time( CLOCK_MONOTONIC );
	nsec2timespec( &abstime, ct + SDIO_TIMEOUT_MS_TO_NS( tms ) );

	pthread_mutex_lock( &hc->mutex );
	while( cmd->status == CS_CMD_INPROG ) {
		status = pthread_cond_timedwait( &hc->cond, &hc->mutex, &abstime );
		if( status == ETIMEDOUT ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: TIMEOUT %"PRId64"ms (errno %d) CMD %d, flgs 0x%x, arg 0x%x, blks %d, blksz %d",
				__FUNCTION__, tms, status, cmd->opcode, cmd->flags, cmd->arg, cmd->blks, cmd->blksz );
			break;
		}
	}
	pthread_mutex_unlock( &hc->mutex );

	return( status );
}

int sdio_issue_cmd( sdio_dev_t * const dev, struct sdio_cmd *cmd, const uint64_t tms )
{
	sdio_hc_t		*hc;
	int				status;

	hc				= dev->hc;

#ifdef SDIO_TRACE
	sdio_trace_event( SDIO_TRACE_EVENT, "CMD %d, flgs 0x%x, arg 0x%x, blks %d, blksz %d, timeout %llums", cmd->opcode, cmd->flags, cmd->arg, cmd->blks, cmd->blksz, tms );
#endif

	if( hc->cfg.verbosity > 3 ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 3, "%s: CMD %d, flgs 0x%x, arg 0x%x, blks %d, blksz %d, timeout %" PRId64 "ms", __FUNCTION__, cmd->opcode, cmd->flags, cmd->arg, cmd->blks, cmd->blksz, tms );
	}

	pthread_mutex_lock( &hc->mutex );
	hc->wspc.cmd	= cmd;
	pthread_mutex_unlock( &hc->mutex );

	status = hc->entry.cmd( hc, cmd );
	if( status == EOK ) {
		status = sdio_wait_cmd( hc, cmd, tms );
	}

	if( status ) {
		hc->entry.abort( hc, cmd );
		pthread_mutex_lock( &hc->mutex );
		hc->wspc.cmd            = NULL;
		cmd->status             = status;
		pthread_mutex_unlock( &hc->mutex );
	}

	return( status );
}

// hc callback for command completion
int sdio_cmd_cmplt( sdio_hc_t *hc, struct sdio_cmd *cmd, const uint32_t status )
{
	static const char	*name[12] = { 	"IN PROG", "SUCCESS", "ABORTED", "ERR", "CMD IDX ERR",
										"CMD TO ERR", "CMD CRC ERR", "CMD END ERR",
										"DATA TO ERR", "DATA CRC ERR", "DATA END ERR", "CARD REMOVED" };

#ifdef SDIO_TRACE
	sdio_trace_event( SDIO_TRACE_EVENT, "CMD cmplt status %s (%d) ", name[status], status );
#endif

	if( hc->cfg.verbosity > 3 ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 4, "%s: CMD %d, flgs 0x%x, arg 0x%x, blks %d, blksz %d, status %s (%d)", __FUNCTION__, cmd->opcode, cmd->flags, cmd->arg, cmd->blks, cmd->blksz, name[status], status );
	}

	pthread_mutex_lock( &hc->mutex );
	hc->wspc.cmd	= NULL;
	cmd->status		= status;
	pthread_cond_signal( &hc->cond );
	pthread_mutex_unlock( &hc->mutex );

	return( EOK );
}

int _sdio_send_cmd( sdio_dev_t * const dev, struct sdio_cmd *cmd,
		void (*func)( struct sdio_device *, sdio_cmd_t *, void *),
		const uint32_t timeout, int retries )
{
	sdio_hc_t	*hc;
	uint64_t	acmd_caps;
	int			status;

	hc			= dev->hc;
	acmd_caps	= 0;

	status = _sdio_pwrmgnt( dev, PM_ACTIVE );
	if( status != EOK ) {
		return( status );
	}

	if( ( cmd->flags & SCF_SBC_RLW ) ) {
			// Reliable write is enabled via bit 31 in set block count,
			// so temporarily disable HC ACMD12/ACMD23 features.
		acmd_caps = hc->caps & ( HC_CAP_ACMD23 | HC_CAP_ACMD12 );
		hc->caps &= ~( HC_CAP_ACMD23 | HC_CAP_ACMD12 );
	}

	do {
		cmd->status = CS_CMD_INPROG;

		if( ( cmd->flags & SCF_APP_CMD ) ) {
			status = sd_app_cmd( dev );
			if( status != EOK ) {
				break;
			}
		}

		if( ( cmd->flags & SCF_SBC ) && !( hc->caps & HC_CAP_ACMD23 ) ) {
			status = _sdio_set_block_count( dev, cmd->blks, ( cmd->flags & SCF_SBC_RLW ) ? SBC_RL_WRITE : 0 );
			if( status != EOK ) {
				break;
			}
		}

#ifdef SDIO_UC_SUPPORT
		if( ( cmd->flags & SCF_SUA ) ) {
			if( ( status = sdio_set_upper_addr( dev, cmd->earg ) ) ) {
				break;
			}
		}
#endif

		status = sdio_issue_cmd( dev, cmd, timeout );
		if( status != EOK ) {
				// calling function should handle error recovery
			break;
		}

		switch( cmd->status  ) {
			case CS_CMD_CMP:
				status = EOK;
				break;

			case CS_CMD_TO_ERR:
				status = ETIMEDOUT;
				break;

			case CS_CARD_REMOVED:
				return( ENXIO );

			case CS_DATA_CRC_ERR:
				if( ( dev->flags & DEV_FLAG_PRESENT ) && !( hc->flags & HC_FLAG_RST )
						&& ( dev->flags & ( DEV_FLAG_UHS | DEV_FLAG_HS200 | DEV_FLAG_HS400 ) ) ) {
					atomic_set( &hc->flags, HC_FLAG_TUNE );
				}
				status = EIO;
				break;

			default:
				status = EIO;
				break;
		}

		if( status == EOK ) {
			if( ( cmd->flags & SCF_WAIT_DRDY ) && !( hc->caps & HC_CAP_BSY ) ) {
				status = _sdio_wait_card_status( dev, NULL, CDS_READY_FOR_DATA | CDS_CUR_STATE_MSK, CDS_READY_FOR_DATA | CDS_CUR_STATE_TRAN, timeout );
				if( status != EOK ) {
					break;
				}
			}
			break;
		}

		if( ( cmd->rsp[0] & CDS_ILLEGAL_COMMAND ) ) {
			break;
		}

	} while( retries-- );

	if( acmd_caps ) {				// Restore ACMD12/ACMD23 feature flags
		hc->caps |= acmd_caps;
	}

	return( status );
}

int _sdio_bus_error( sdio_dev_t * const dev )
{
	sdio_hc_t	*hc;
	int			status;

	hc		= dev->hc;
	status	= EOK;

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s:  ", __FUNCTION__ );

	if( ++hc->bus_errs > SDIO_MAX_BUS_ERRS ) {
#if 0
// only needed for testing when we simulate bus errors
		_sdio_pwrmgnt( dev, PM_ACTIVE );
#endif

		switch( dev->dtype ) {
			case DEV_TYPE_MMC:
				status = mmc_bus_error( dev );
				break;
			case DEV_TYPE_SD:
				status = sd_bus_error( dev );
				break;
			default:
				status = ENODEV;
				break;
		}
		hc->bus_errs = 0;
	}

	return( status );
}

static int _sdio_attach( sdio_hc_t *hc )
{
	sdio_dev_t		*dev;
	uint32_t		clk;
	int				dtype;
	int				status;

	memset( &hc->device, 0, sizeof( sdio_dev_t ) );
	dev				= &hc->device;
	dev->hc			= hc;
	hc->bus_errs	= 0;
	status			= ENODEV;
	dtype			= hc->flags & HC_FLAG_DEV_TYPE;

	if( !( hc->flags & HC_FLAG_SKIP_PWRUP ) ) {
		sdio_power( hc, SDIO_PWR_OFF );
	}

	for( clk = SDIO_CLK_INIT; clk; clk -= 100000 ) {
		hc->clk_init	= max( clk, hc->clk_min );

		if( !( hc->flags & HC_FLAG_SKIP_PWRUP ) ) {
			sdio_power( hc, SDIO_PWR_ON );
		}

		if( !dtype || ( dtype & HC_FLAG_DEV_SD ) ) {
			status = sd_ident( hc );
			if( status == EOK ) {
				_sdio_pwrmgnt( dev, PM_IDLE );
				dev->flags |= DEV_FLAG_PRESENT;
				dev->dtype = DEV_TYPE_SD;
				return( status );
			}
		}

		if( !dtype || ( dtype & HC_FLAG_DEV_MMC ) ) {
			status = mmc_ident( hc );
			if( status == EOK ) {
				_sdio_pwrmgnt( dev, PM_IDLE );
				dev->flags |= DEV_FLAG_PRESENT;
				dev->dtype = DEV_TYPE_MMC;
				return( status );
			}
		}

		hc->flags &= ~HC_FLAG_SKIP_PWRUP;

		sdio_power( hc, SDIO_PWR_OFF );
	}

	return( status );
}

static int sdio_cd_info( sdio_hc_t *hc )
{
	sdio_dev_t			*dev;
	sdio_cid_t			*cid;
	sdio_csd_t			*csd;
	sdio_ecsd_t			*ecsd;
	sd_switch_cap_t		*swcaps;
	sdio_hc_cfg_t		*cfg;
	static const char	*name[11] = { "INVALID", "LS", "HS", "HS DDR50", "SDR12", "SDR25", "SDR50", "SDR104", "HS200", "HS400", "HS400ES" };

	cfg		= &hc->cfg;
	dev		= &hc->device;
	cid		= &dev->cid;
	csd		= &dev->csd;
	ecsd	= &dev->ecsd;
	swcaps	= &dev->swcaps;

		// CID
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "%s CID:", ( dev->dtype == DEV_TYPE_MMC ) ? "MMC" : "SD" );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  MID 0x%x, OID 0x%x, PNM %s", cid->mid, cid->oid, cid->pnm );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  PRV 0x%x, PSN 0x%x, MDT %d-%d",
				cid->prv, cid->psn, cid->month, cid->year );

		// CSD
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "%s CSD:", ( dev->dtype == DEV_TYPE_MMC ) ? "MMC" : "SD" );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  CSD_STRUCTURE %d, SPEC_VERS %d, CCC 0x%x", csd->csd_structure, csd->spec_vers, csd->ccc );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  TAAC %d, NSAC %d, TRAN_SPEED %d", csd->taac, csd->nsac, csd->tran_speed );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  C_SIZE %d, C_SIZE_MULT %d", csd->c_size, csd->c_size_mult );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  READ_BL_LEN %d, WRITE_BL_LEN %d", csd->read_bl_len, csd->write_bl_len );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  ERASE GRP_SIZE %d, GRP_MULT %d, SIZE %d", csd->erase_grp_size, csd->erase_grp_mult, csd->sector_size );
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  blksz %d, sectors %"PRIu64", dtr %d", csd->blksz, csd->sectors, csd->dtr_max );

	if( dev->dtype == DEV_TYPE_SD ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "SD SW CAPS:" );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  bus mode 0x%x, cmd sys 0x%x",
			swcaps->bus_mode, swcaps->cmd_sys );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  drv type 0x%x, curr limit 0x%x",
			swcaps->drv_type, swcaps->curr_limit );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  dtr %d", swcaps->dtr_max_hs );
	}
	else {
		static const char	*eol[5] = { "Not Defined", "Normal", "Warning Consumed 80%", "Urgent", "Reserved" };
		static const char	*est[12] = { "Not Available", "0% - 10%", "10% - 20%", "20% - 30%", "30% - 40%", "40% - 50%",
										"50% - 60%", "60% - 70%", "70% - 80%", "80% - 90%", "90% - 100%", "Exceeded" };

		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "MMC EXT CSD:" );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  DEVICE_TYPE 0x%x, EXT_CSD_REV %d",
			ecsd->card_type, ecsd->ext_csd_rev );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  SEC_COUNT %d, dtr %d", ecsd->sectors, ecsd->dtr_max_hs );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  HC_ERASE_GRP_SIZE %d, HC_WP_GRP_SIZE %d",
			ecsd->hc_erase_group_size, ecsd->hc_wp_grp_size );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  WR_REL_SET 0x%x, WR_REL_PARAM 0x%x, REL_WR_SEC_C %d",
			dev->raw_ecsd[ECSD_WR_REL_SET], dev->raw_ecsd[ECSD_WR_REL_PARAM], dev->rel_wr_sec_c );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  STROBE 0x%x, BKOPS_EN 0x%x", ecsd->strobe, ecsd->bkops_en );
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  Driver type: 0x%x", hc->drv_type );

		if( dev->raw_ecsd[ECSD_PRE_EOL_INFO] && dev->raw_ecsd[ECSD_DEVICE_LIFE_TIME_EST_TYP_A] && dev->raw_ecsd[ECSD_DEVICE_LIFE_TIME_EST_TYP_B] ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "  Health:  Pre EOL Info %s, Device Life Time Est A %s, Device Life Time Est B %s",
				eol[ min( dev->raw_ecsd[ECSD_PRE_EOL_INFO], 4 ) ], est[ min( dev->raw_ecsd[ECSD_DEVICE_LIFE_TIME_EST_TYP_A], 11 ) ],
				est[ min( dev->raw_ecsd[ECSD_DEVICE_LIFE_TIME_EST_TYP_B], 11 ) ] );
		}
	}

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "CFG:  Timing %s, DTR %d, Bus Width %d bit", name[hc->timing], hc->clk, hc->bus_width );

	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, cfg->verbosity, 0, "Capabilites: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		( dev->caps & DEV_CAP_HC ) ? "HC " : "",
		( dev->caps & DEV_CAP_HS ) ? "HS " : "",
		( dev->caps & DEV_CAP_HS200 ) ? "HS200 " : "",
		( dev->caps & DEV_CAP_HS400) ? "HS400 " : "",
		( dev->caps & DEV_CAP_HS400ES ) ? "HS400ES " : "",
		( dev->caps & DEV_CAP_DDR50 ) ? "DDR50 " : "",
		( dev->caps & DEV_CAP_UHS ) ? "UHS " : "",
		( dev->caps & DEV_CAP_TRIM ) ? "TRIM " : "",
		( dev->caps & DEV_CAP_DISCARD ) ? "DISCARD " : "",
		( dev->caps & DEV_CAP_SECURE ) ? "SECURE " : "",
		( dev->caps & DEV_CAP_SANITIZE ) ? "SANITIZE " : "",
		( dev->caps & DEV_CAP_BKOPS ) ? "BKOPS " : "",
		( dev->caps & DEV_CAP_BKOPS_AUTO ) ? "BKOPS_AUTO " : "",
		( dev->caps & DEV_CAP_CMD23 ) ? "CMD23 " : "",
		( dev->caps & DEV_CAP_SLEEP ) ? "SLEEP " : "",
		( dev->caps & DEV_CAP_ASSD ) ? "ASSD " : "",
		( dev->caps & DEV_CAP_HPI_CMD12 ) ? "HPI CMD12 " : "",
		( dev->caps & DEV_CAP_HPI_CMD13 ) ? "HPI CMD13 " : "",
		( dev->caps & DEV_CAP_WR_REL ) ? "WR_REL " : "",
		( dev->caps & DEV_CAP_WR_REL_ENH ) ? "WR_REL_ENH " : "",
		( dev->caps & DEV_CAP_PWROFF_NOTIFY ) ? "PWR_NOTIFY " : "",
		( dev->caps & DEV_CAP_UC ) ? "UC " : "" );

	return( EOK );
}

__attribute__((used))
static int sdio_event( sdio_hc_t * const hc, const int ev )
{
	sdio_funcs_t			*funcs;
	struct sdio_device		*device;
	int						status;

	status	= EOK;
	funcs	= &sdio_ctrl.connect_parm.funcs;

	pthread_mutex_lock( &sdio_ctrl.mutex );

		// look up device based on path, generation
	for( device = TAILQ_FIRST( &sdio_ctrl.dlist ); device != NULL; device = TAILQ_NEXT( device, dlink ) ) {
		if( ( device->instance.path == hc->path ) && ( device->instance.generation == hc->generation ) ) {
			break;
		}
	}

	pthread_mutex_unlock( &sdio_ctrl.mutex );

	if( device && ( funcs->event != NULL ) ) {
		status = funcs->event( (void *)&sdio_ctrl, &device->instance, ev );
	}

	return( status );
}

static int sdio_cd( sdio_hc_t *hc )
{
	sdio_dev_t				*dev;
	sdio_funcs_t			*funcs;
	sdio_hc_entry_t			*hce;
	struct sdio_device		*device;
	sdio_device_instance_t	inst;
	int						cd;

	dev		= &hc->device;
	hce		= &hc->entry;
	funcs	= &sdio_ctrl.connect_parm.funcs;

	pthread_mutex_lock( &hc->cd_mutex );

	cd = hce->cd( hc );

	if( ( dev->flags & DEV_FLAG_MEDIA_CHANGE ) || !( cd & CD_INS ) ) {
		atomic_clr( &dev->flags, ( DEV_FLAG_MEDIA_CHANGE | DEV_FLAG_INVALID_CARD | DEV_FLAG_WRITE_PROTECT ) );
		if( ( dev->flags & DEV_FLAG_PRESENT ) ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 0, "%s:  removal path %d, cd state 0x%x", __FUNCTION__, hc->path, cd );
			atomic_clr( &dev->flags, DEV_FLAG_PRESENT );
			pthread_mutex_lock( &sdio_ctrl.mutex );

				// look up device based on path, generation
			for( device = TAILQ_FIRST( &sdio_ctrl.dlist ); device != NULL; device = TAILQ_NEXT( device, dlink ) ) {
				if( ( device->instance.path == hc->path ) && ( device->instance.generation == hc->generation ) ) {
					break;
				}
			}

			if( device ) {
					// we can't use _sdio_synchronize to set the
					// pending flag since we already hold the mutex
				atomic_set( &device->flags, DEV_FLAG_RMV_PENDING );

				while( device->usage ) {
					pthread_cond_wait( &sdio_ctrl.cd_cond, &sdio_ctrl.mutex );
				}
			}

			pthread_mutex_unlock( &sdio_ctrl.mutex );

			if( device && ( funcs->removal != NULL ) ) {
				funcs->removal( (void *)&sdio_ctrl, &device->instance );
			}

			if( !cd ) {
				sdio_power( hc, SDIO_PWR_OFF );
			}
		}
	}

	if( ( cd & CD_INS ) ) {
		if( !( dev->flags & ( DEV_FLAG_PRESENT | DEV_FLAG_INVALID_CARD ) ) ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 0, "%s:  insertion path %d, cd state 0x%x", __FUNCTION__, hc->path, cd );
			if( _sdio_attach( hc ) == EOK ) {
				sdio_cd_info( hc );
				if( ( cd & CD_WP ) && ( hc->caps & HC_CAP_CD_WP ) ) {
					atomic_set( &dev->flags, DEV_FLAG_WRITE_PROTECT );
				}
				memset( &inst, 0, sizeof( sdio_device_instance_t ) );
				inst.path			= hc->path;
				inst.func			= 0;
				inst.generation		= hc->generation;
				inst.ident.dtype	= dev->dtype;
				if( funcs->insertion ) {
					funcs->insertion( (void *)&sdio_ctrl, &inst );
				}
			}
			else {
				if( !( dev->flags & DEV_FLAG_INVALID_CARD ) ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Unsupported card inserted ", __FUNCTION__ );
					atomic_set( &dev->flags, DEV_FLAG_INVALID_CARD );
				}
			}
		}
	}

	pthread_mutex_unlock( &hc->cd_mutex );

	return( ( dev->flags & DEV_FLAG_PRESENT ) );
}

// hc callback for card detect, re-tuning
int sdio_hc_event( sdio_hc_t *hc, const int ev )
{
	switch( ev ) {
		case HC_EV_CD:					// notify sdio_cd_thread
			atomic_set( &hc->device.flags, DEV_FLAG_MEDIA_CHANGE );
#if ( __PTR_BITS__ == 64 )
			MsgSendPulsePtr( sdio_ctrl.cd_coid, hc->priority, SDIO_EV_CD, hc );
#else
			MsgSendPulse( sdio_ctrl.cd_coid, hc->priority, SDIO_EV_CD, (uintptr_t)hc );
#endif
			break;

		case HC_EV_TUNE:
			atomic_set( &hc->flags, HC_FLAG_TUNE );
			break;

		default:
			break;
	}

	return( EOK );
}

int sdio_timer_settime( const int tid, const uint32_t sec, const uint32_t nsec, const int repeat )
{
	struct itimerspec	value;

	memset( &value, 0, sizeof( value ) );

	value.it_value.tv_sec	= sec;
	value.it_value.tv_nsec	= nsec;

	if( repeat ) {
		value.it_interval.tv_sec	= sec;
		value.it_interval.tv_nsec	= nsec;
	}

	if( timer_settime( tid, 0, &value, NULL ) == -1 ) {
		return( errno );
	}

	return( EOK );
}

static int sdio_cd_enum( sdio_ctrl_t * const sc )
{
	pthread_sleepon_lock( );
	while( sc->cd_enum == SDIO_ENUM_DISABLE ) {
		pthread_sleepon_wait( &sc->cd_enum );
	}
	pthread_sleepon_unlock( );
	return( EOK );
}

static void *sdio_cd_thread( void * const hdl )
{
	sdio_ctrl_t			*sc;
	sdio_hc_t			*hc;
	int					rid;
	int					init;
	struct _pulse		pulse;
	struct sigevent		event;

	sc		= hdl;
	init	= SDIO_FAILURE;

	sc->cd_chid = ChannelCreate( _NTO_CHF_PRIVATE | _NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK );
	if( sc->cd_chid != -1 ) {
		sc->cd_coid = ConnectAttach( 0, 0, sc->cd_chid, _NTO_SIDE_CHANNEL, 0 );
		if( sc->cd_coid != -1 ) {
			SIGEV_PULSE_INIT( &event, sc->cd_coid, sc->priority, SDIO_EV_TIMER, NULL );
			if( timer_create( CLOCK_MONOTONIC, &event, &sc->cd_timerid ) != -1 ) {
				init = SDIO_SUCCESS;
			}
		}
	}

	if( init == SDIO_FAILURE ) {
			// NOTE:  calling func will cleanup/destry cd thread
		sdio_set_thread_state( &sc->state, SDIO_TSTATE_INIT_FAILURE );
		return( NULL );
	}

	sdio_set_thread_state( &sc->state, SDIO_TSTATE_INITIALIZED );

	while( 1 ) {
		rid = MsgReceivePulse( sc->cd_chid, &pulse, sizeof( pulse ), NULL );
		if( rid == -1 ) {
			break;
		}

		switch( pulse.code ) {
			case SDIO_EV_TIMER:
				for( hc = TAILQ_FIRST( &sdio_ctrl.hlist ); hc; hc = TAILQ_NEXT( hc, hlink ) ) {
					if( ( hc->pm_state != PM_SUSPEND ) && !( hc->caps & HC_CAP_CD_INTR ) && !( hc->caps & HC_CAP_SLOT_TYPE_EMBEDDED ) ) {
						sdio_cd_enum( sc );
						sdio_cd( hc );
					}
				}
				break;

			case SDIO_EV_CD:
				sdio_cd_enum( sc );
				sdio_cd( (sdio_hc_t *)pulse.value.sival_ptr );
				break;

			default:
//				sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 1, 1, "%s: unknown pulse rid %d, type %x, subtype %, pulse %x, value %x, scoid %x", __FUNCTION__, rid, pulse.type, pulse.subtype, pulse.code, pulse.value, pulse.scoid );
				break;
		}

	}

	return( NULL );
}

static void *sdio_hc_thread( void * const hdl )
{
	sdio_hc_t			*hc;
	sdio_hc_entry_t		*hce;
	int					rid;
	int					init;
	struct _pulse		pulse;
	struct sigevent		event;
	struct itimerspec	value;
	pthread_condattr_t	attr;

	hc		= hdl;
	hce		= &hc->entry;
	init	= SDIO_FAILURE;

	if( pthread_condattr_init( &attr ) == EOK ) {
		if( pthread_condattr_setclock( &attr, CLOCK_MONOTONIC ) == EOK ) {
			if( pthread_cond_init( &hc->cond, &attr ) == EOK ) {
				if( pthread_mutex_init( &hc->mutex, NULL ) == EOK ) {
					if( pthread_mutex_init( &hc->cd_mutex, NULL ) == EOK ) {
						hc->hc_chid = ChannelCreate( _NTO_CHF_PRIVATE | _NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK );
						if( hc->hc_chid != -1 ) {
							hc->hc_coid = ConnectAttach( 0, 0, hc->hc_chid, _NTO_SIDE_CHANNEL, 0 );
							if( hc->hc_coid != -1 ) {
								if( hc->tuning_count ) {
									memset( &value, 0, sizeof( value ) );
									value.it_value.tv_sec = hc->tuning_count;

									SIGEV_PULSE_INIT( &event, hc->hc_coid, hc->priority, HC_EV_TUNE, NULL );
									if( timer_create( CLOCK_MONOTONIC, &event, &hc->tuning_timerid ) != -1 ) {
										if( timer_settime( hc->tuning_timerid, 0, &value, NULL ) != -1 ) {
											init = SDIO_SUCCESS;
										}
									}
								}
								else {
									init = SDIO_SUCCESS;
								}
							}
						}
					}
				}
			}
		}
		pthread_condattr_destroy( &attr );
	}

	if( init == SDIO_FAILURE ) {
		sdio_set_thread_state( &hc->state, SDIO_TSTATE_INIT_FAILURE );
		return( NULL );
	}

	sdio_set_thread_state( &hc->state, SDIO_TSTATE_INITIALIZED );

	while( 1 ) {
		rid = MsgReceivePulse( hc->hc_chid, &pulse, sizeof( pulse ), NULL );
		if( rid == -1 ) {
			break;
		}

		switch( pulse.code ) {
			case HC_EV_TUNE:
				sdio_hc_event( hc, HC_EV_TUNE );
				break;

			default:
				if( hce->event ) {
					if( hce->event( hc, &pulse ) != EOK ) {
//						sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 1, 1, "%s: unknown pulse rid %d, type %x, subtype %, pulse %x, value %x, scoid %x", __FUNCTION__, rid, pulse.type, pulse.subtype, pulse.code, pulse.value, pulse.scoid );
					}
				}
				break;
		}
	}

	return( NULL );
}

int sdio_options( sdio_hc_t *hc, char *options )
{
	int					opt;
	int					val;
	int64_t				val64;
	uint64_t			*cap;
	uint64_t			ocap;
	uint64_t			ncap;
	int					status;
	char				*value;
	char				*argstr[2];
	sdio_hc_cfg_t		*cfg;
	char				*ltok;
	char				*delims = { ":" };
	enum {
		OPTION_WITHOUT_ARGS = 0,
			OPTION_DISABLE_BUS_MSTR = OPTION_WITHOUT_ARGS,
			OPTION_DISABLE_AC_12,
			OPTION_DISABLE_AC_23,
			OPTION_EMMC,
			OPTION_CD_NOWP,

		OPTION_WITH_ARGS,
			OPTION_PRIORITY = OPTION_WITH_ARGS,
			OPTION_HC,
			OPTION_VID,
			OPTION_DID,
			OPTION_IDX,
			OPTION_BASE_ADDR,
			OPTION_IRQ,
			OPTION_DMA,
			OPTION_OPERATION_CLK,
			OPTION_BUS_WIDTH,
			OPTION_BUS_SMART,
			OPTION_PWR_MGMT,
			OPTION_BOARD_SPECIFIC,
			OPTION_HOST_SPECIFIC,
			OPTION_DRV_TYPE,

		OPTION_VAR_ARGS,
			OPTION_VERBOSE = OPTION_VAR_ARGS,
	};

	static char *opts[] = {
		[OPTION_DISABLE_BUS_MSTR]	= "~bmstr",
		[OPTION_DISABLE_AC_12]		= "~ac12",
		[OPTION_DISABLE_AC_23]		= "~ac23",
		[OPTION_EMMC]				= "emmc",
		[OPTION_CD_NOWP]			= "nowp",

		[OPTION_PRIORITY]			= "priority",
		[OPTION_HC]					= "hc",
		[OPTION_VID]				= "vid",
		[OPTION_DID]				= "did",
		[OPTION_IDX]				= "idx",
		[OPTION_BASE_ADDR]			= "addr",
		[OPTION_IRQ]				= "irq",
		[OPTION_DMA]				= "dma",
		[OPTION_OPERATION_CLK]		= "clk",
		[OPTION_BUS_WIDTH]			= "bw",
		[OPTION_BUS_SMART]			= "timing",
		[OPTION_PWR_MGMT]			= "pm",
		[OPTION_BOARD_SPECIFIC]		= "bs",
		[OPTION_HOST_SPECIFIC]		= "hs",
		[OPTION_DRV_TYPE]			= "drv_type",

		[OPTION_VERBOSE]			= "verbose",

		NULL
	};

	cfg		= &hc->cfg;
	status	= EOK;

	while( (*options != '\0') && (status == EOK) ) {
		opt = getsubopt( &options, opts, &value );
		if( opt == -1 ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s: Invalid sdio option '%s'", __FUNCTION__, value );
			status = EINVAL;
			break;
		}

		if( opt < OPTION_VAR_ARGS ) {
			if( opt < OPTION_WITH_ARGS ) {
				if( value != NULL ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s: Unexpected argument for '%s'", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
			}
			else {
				if( (value == NULL) || (*value == '\0') ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s: Missing argument for '%s'", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
			}
		}

		switch( opt ) {

// options without args follow

			case OPTION_DISABLE_BUS_MSTR:	// ~bmstr
				cfg->caps &= ~HC_CAP_DMA;
				break;

			case OPTION_DISABLE_AC_12:		// ~ac12
				cfg->caps &= ~HC_CAP_ACMD12;
				break;

			case OPTION_DISABLE_AC_23:		// ~ac23
				cfg->caps &= ~HC_CAP_ACMD23;
				break;

			case OPTION_EMMC:				// eMMC
				hc->caps |= HC_CAP_SLOT_TYPE_EMBEDDED;
				hc->flags |= HC_FLAG_DEV_MMC;
				cfg->caps &= ~HC_CAP_CD_INTR;
				break;

			case OPTION_CD_NOWP:			// no change detect write protect
				cfg->caps &= ~HC_CAP_CD_WP;
				break;

// options with args follow

			case OPTION_PRIORITY:			// priority
				val = sdio_parse_number( value );
				if( (val == SDIO_INVALID_NUM) || (val < sched_get_priority_min( SCHED_RR )) || (val > sched_get_priority_max( SCHED_RR )) ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				hc->priority = val;
				break;

			case OPTION_HC:			// hc - host contoller
				strlcpy( cfg->name, value, sizeof( cfg->name ) );
				break;

			case OPTION_VID:			// vid - vender id
				val = sdio_parse_number( value );
				if( val == SDIO_INVALID_NUM ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				cfg->vid = val;
				break;

			case OPTION_DID:			// did - device id
				val = sdio_parse_number( value );
				if( val == SDIO_INVALID_NUM ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				cfg->did = val;
				break;

			case OPTION_IDX:			// idx
				val = sdio_parse_number( value );
				if( val == SDIO_INVALID_NUM ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				cfg->idx = val;
				break;

			case OPTION_BASE_ADDR:		// addr
				sdio_parse_tuple( value, ':', &argstr[0], &argstr[1], NULL );
				val64 = sdio_parse_number64( argstr[0] );
				if( val64 != SDIO_INVALID_NUM ) {
					cfg->base_addr[cfg->base_addrs] = val64;
					if( (argstr[1] != NULL) && (*argstr[1] != '\0') ) {
						val64 = sdio_parse_number64( argstr[1] );
						if( val64 != SDIO_INVALID_NUM ) {
							cfg->base_addr_size[cfg->base_addrs] = val64;
						}
					}
					cfg->base_addrs++;
				}
				break;

			case OPTION_IRQ:			// irq
				val = sdio_parse_number( value );
				if( val == SDIO_INVALID_NUM ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				cfg->irq[cfg->irqs++] = val;
				break;

			case OPTION_DMA:			// dma - dma channel
				val = sdio_parse_number( value );
				if( val == SDIO_INVALID_NUM ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				cfg->dma_chnl[cfg->dma_chnls++] = val;
				break;

			case OPTION_OPERATION_CLK:	// clk
				val = sdio_parse_number( value );
				if( val == SDIO_INVALID_NUM ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				cfg->clk = val;
				break;

			case OPTION_BUS_WIDTH:		// bw - bus width
				ocap	= 0;
				ncap	= 0;
				value	= strtok_r( value, delims, &ltok );
				while( value != NULL ) {
					cap		= &ocap;
					if( value[0] == '~' ) {
						value++;
						cap = &ncap;
					}
					if( !strcmp( value, "4" ) ) {
						*cap |= HC_CAP_BW4;
					}
					else if( !strcmp( value, "8" ) ) {
						*cap |= HC_CAP_BW8;
					}
					else {
						sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid bw '%s'", __FUNCTION__, value );
						status = EINVAL;
						break;
					}
					value = strtok_r( NULL, delims, &ltok );
				}
				if( ocap ) {
					cfg->caps &= ~HC_CAP_BW_MSK;
					cfg->caps |= ocap;
				}
				if( ncap ) {
					cfg->caps &= ~ncap;
				}
				break;

			case OPTION_BUS_SMART:		// timing
				ocap	= 0;
				ncap	= 0;
				value	= strtok_r( value, delims, &ltok );
				while( value != NULL ) {
					cap		= &ocap;
					if( value[0] == '~' ) {
						value++;
						cap = &ncap;
					}
					if( !strcmp( value, "hs" ) ) {
						*cap |= HC_CAP_HS;
					}
					else if( !strcmp( value, "ddr" ) ) {
						*cap |= HC_CAP_DDR50 | HC_CAP_HS;
					}
					else if( !strcmp( value, "sdr" ) ) {
						*cap |= ( HC_CAP_SDR12 | HC_CAP_SDR25 | HC_CAP_SDR50 | HC_CAP_SDR104 | HC_CAP_DDR50 );
					}
					else if( !strcmp( value, "sdr12" ) ) {
						*cap |= HC_CAP_SDR12;
					}
					else if( !strcmp( value, "sdr25" ) ) {
						*cap |= HC_CAP_SDR25;
					}
					else if( !strcmp( value, "sdr50" ) ) {
						*cap |= HC_CAP_SDR50;
					}
					else if( !strcmp( value, "sdr104" ) ) {
						*cap |= HC_CAP_SDR104;
					}
					else if( !strcmp( value, "hs200" ) ) {
						*cap |= HC_CAP_HS200;
					}
					else if( !strcmp( value, "hs400" ) ) {
						*cap |= HC_CAP_HS400;
					}
					else if( !strcmp( value, "hs400es" ) ) {
						*cap |= HC_CAP_HS400ES;
					}
					else {
						sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid timing '%s'", __FUNCTION__, value );
						status = EINVAL;
						break;
					}
					value = strtok_r( NULL, delims, &ltok );
				}
				if( ocap ) {
					cfg->caps &= ~HC_CAP_TIMING_MSK;
					cfg->caps |= ocap;
				}
				if( ncap ) {
					cfg->caps &= ~ncap;
				}
				break;

			case OPTION_PWR_MGMT:			// pm idle:sleep in ms
				sdio_parse_tuple( value, ':', &argstr[0], &argstr[1], NULL );
				val = sdio_parse_number( argstr[0] );
				if( val != SDIO_INVALID_NUM ) {
					cfg->idle_time = val;
				}
				val = sdio_parse_number( argstr[1] );
				if( val != SDIO_INVALID_NUM ) {
					cfg->sleep_time = val;
				}
				break;

			case OPTION_BOARD_SPECIFIC:		// bs options
				if( cfg->options ) {
					free( cfg->options );
				}
				cfg->options = strdup( value );
				break;

			case OPTION_HOST_SPECIFIC:		// hs options
				if( cfg->hsoptions ) {
					free( cfg->hsoptions );
				}
				cfg->hsoptions = strdup( value );
				break;

			case OPTION_DRV_TYPE:			// drv_type
				val = sdio_parse_number( value );
				if( ( val == SDIO_INVALID_NUM ) || ( val > 4 ) ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
					status = EINVAL;
					break;
				}
				hc->drv_type = val;
				break;

// options with variable args follow

			case OPTION_VERBOSE:			// verbose
				if( value != NULL ) {
					val = sdio_parse_number( value );
					if( val == SDIO_INVALID_NUM ) {
						sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Invalid %s", __FUNCTION__, opts[ opt ] );
						status = EINVAL;
						break;
					}
					cfg->verbosity = val;
				}
				else {
					cfg->verbosity++;
				}
				break;

			default:
				break;
		}
	}

	if( (cfg->vid == 0) && (cfg->name[0] == '\0') && (cfg->idx == -1) ) {
		sdio_ctrl.flags		|= SDIO_CFLAG_SCAN;
		sdio_ctrl.verbosity	= cfg->verbosity;
		if( hc->priority != SDIO_PRIORITY ) {
			sdio_ctrl.priority = hc->priority;
		}
	}

	return( status );
}

static int sdio_xpt_device_notify( sdio_hc_t *hc, const int op )
{
	sdio_hc_cfg_t	*cfg;
	XPT_DEVICE		dev;
	int				idx;
	int				status;

	status			= EOK;
	hc->device.hc	= hc;
	cfg				= &hc->cfg;

	memset( &dev, 0, sizeof( dev ) );

#ifdef SDIO_PCI_SUPPORT
	if( hc->pci.dev_hdl != NULL ) {
		dev.type	= XPT_DEVICE_TYPE_PCI;
		dev.pci.bdf	= hc->pci.bdf;
		status = op ? xpt_device_register( &dev, 0, NULL ) : xpt_device_deregister( &dev );
		if( status ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, cfg->verbosity, 0, "%s: xpt_device_%sregister failure BDF %d:%d:%d %s",
				__FUNCTION__, op ? "" : "de", PCI_BUS( dev.pci.bdf ), PCI_DEV( dev.pci.bdf ), PCI_FUNC( dev.pci.bdf ),
				strerror( status ) );
		}
	}
#endif

	if( hc->pci.dev_hdl == NULL ) {
		dev.type		= XPT_DEVICE_TYPE_MMIO;

		for( idx = 0; idx < cfg->base_addrs; idx++ ) {
			dev.mmio.phys	= cfg->base_addr[idx];
			dev.mmio.len	= cfg->base_addr_size[idx];

			status = op ? xpt_device_register( &dev, 0, NULL ) : xpt_device_deregister( &dev );
			if( status ) {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, cfg->verbosity, 0, "%s: xpt_device_%sregister phys 0x%"PRIx64" len %d failure %s",
					__FUNCTION__, op ? "" : "de", cfg->base_addr[idx], cfg->base_addr_size[idx], strerror( status ) );
			}
		}
	}

	return( status );
}

static int sdio_hc_dinit( sdio_hc_t *hc )
{
	sdio_hc_cfg_t	*cfg;

	cfg				= &hc->cfg;

//	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: hc %p", __FUNCTION__, hc );

	if( hc->entry.dinit ) {
		hc->entry.dinit( hc );
	}

	if( cfg->options ) {
		free( cfg->options );
		cfg->options = NULL;
	}

	if( cfg->hsoptions ) {
		free( cfg->hsoptions );
		cfg->hsoptions = NULL;
	}

	if( hc->hc_coid != -1 ) {
		ConnectDetach( hc->hc_coid );
	}

	if( hc->hc_chid != -1 ) {
		ChannelDestroy( hc->hc_chid );
	}

	if( hc->tuning_timerid != -1 ) {
		timer_delete( hc->tuning_timerid );
	}

	if( hc->hc_tid != -1 ) {
		pthread_join( hc->hc_tid, NULL );
	}

	sdio_xpt_device_notify( hc, SDIO_FALSE );

#ifdef SDIO_PCI_SUPPORT
	sdio_pci_detach_device( hc );
#endif

	pthread_cond_destroy( &hc->cond );
	pthread_mutex_destroy( &hc->mutex );
	pthread_mutex_destroy( &hc->cd_mutex );

	return( EOK );
}

sdio_product_t *sdio_hc_lookup( const uint16_t vid, const uint16_t did, uint32_t class, char * const name )
{
	sdio_vendor_t	*vend;
	sdio_product_t	*prod;

		// check for a match on vid, did, class
	if( vid && did ) {
		class &= ~0xff;
		for( vend = sdio_vendors; vend->vid; vend++ ) {
			if( ( vend->vid == vid ) || ( vend->vid == SDIO_VENDOR_ID_WILDCARD ) ) {
				for( prod = vend->chipsets; prod->did; prod++ ) {
					if( ( ( prod->did == SDIO_DEVICE_ID_WILDCARD ) &&
							( prod->class == class ) ) || ( prod->did == did ) ) {
						return( prod );
					}
				}
			}
		}
	}

		// check for a match on product name
	if( name[0] != '\0' ) {
		for( vend = sdio_vendors; vend->vid; vend++ ) {
			for( prod = vend->chipsets; prod->did; prod++ ) {
				if( !strcmp( prod->name, name ) ) {
					return( prod );
				}
			}
		}
	}

	return( sdio_vendors[0].chipsets );
}

static int sdio_hc_init( sdio_hc_t *hc )
{
	sdio_hc_cfg_t	*cfg;
	sdio_product_t	*prod;
	int				status;

	status			= EOK;
	hc->device.hc	= hc;
	cfg				= &hc->cfg;

	sdio_xpt_device_notify( hc, SDIO_TRUE );

	prod = sdio_hc_lookup( cfg->vid, cfg->did, cfg->class, cfg->name );
	if( prod != NULL ) {
		status = sdio_create_thread( &hc->hc_tid, NULL, sdio_hc_thread, hc, hc->priority, &hc->state, "sdio_hc_thread" );
		if( status == EOK ) {
			status = prod->init( hc );
			if( status == EOK ) {
				if( cfg->name[0] == '\0' ) {
					strlcpy( cfg->name, prod->name, sizeof( cfg->name ) );
				}
				hc->flags |= HC_FLAG_INITIALIZED;
				return( EOK );
			}
			else {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, cfg->verbosity, 0, "%s: hc init failure %s", __FUNCTION__, prod->name );
			}
		}
		else {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, cfg->verbosity, 0, "%s: sdio_hc_thread creation failure", __FUNCTION__ );
		}
	}
	else {
		if( cfg->name[0] != '\0' ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, cfg->verbosity, 0, "%s sdio_hc_lookup failure %s", __FUNCTION__, cfg->name );
		}
		else {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, cfg->verbosity, 0, "%s sdio_hc_lookup failure vid %x, did %x", __FUNCTION__, cfg->vid, cfg->did );
		}
		status = ENODEV;
	}

	return( status ? status : errno );
}

static int sdio_cd_dinit( sdio_ctrl_t *sc )
{
//	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: sc %p", __FUNCTION__, sc );

	if( sc->cd_timerid != -1 ) {
		timer_delete( sc->cd_timerid );
	}

	if( sc->cd_coid != -1 ) {
		ConnectDetach( sc->cd_coid );
	}

	if( sc->cd_chid != -1 ) {
		ChannelDestroy( sc->cd_chid );
	}

	if( sc->cd_tid != -1 ) {
		pthread_join( sc->cd_tid, NULL );
	}

	sc->cd_coid		= -1;
	sc->cd_chid		= -1;
	sc->cd_tid		= -1;
	sc->cd_timerid	= -1;

	return( EOK );
}

static int sdio_dinit( sdio_ctrl_t *sc )
{
	sdio_hc_t			*hc;
	sdio_hc_t			*nhc;
	struct sdio_cmd		*cmd;
	struct sdio_cmd		*ncmd;

	sdio_cd_dinit( sc );

	for( hc = TAILQ_FIRST( &sc->hlist ); hc; hc = nhc ) {
		nhc	= TAILQ_NEXT( hc, hlink );
		sdio_hc_dinit( hc );
		sdio_hc_free( hc );
	}

	for( cmd = TAILQ_FIRST( &sc->clist ); cmd; cmd = ncmd ) {
		ncmd	= TAILQ_NEXT( cmd, clink );
		TAILQ_REMOVE( &sc->clist, cmd, clink );
		free( cmd );
	}

	pthread_mutex_destroy( &sc->mutex );
	pthread_cond_destroy( &sc->cd_cond );

	return( EOK );
}

static int sdio_init( sdio_ctrl_t *sc )
{
	int					status;

	TAILQ_INIT( &sc->hlist );
	TAILQ_INIT( &sc->dlist );
	TAILQ_INIT( &sc->clist );
	sc->cd_coid		= -1;
	sc->cd_chid		= -1;
	sc->cd_tid		= -1;
	sc->cd_timerid	= -1;

	sc->priority	= SDIO_PRIORITY;

	status = pthread_mutex_init( &sc->mutex, NULL );
	if( status == EOK ) {
		status = pthread_cond_init( &sc->cd_cond, NULL );
		if( status == EOK ) {
			status = sdio_create_thread( &sc->cd_tid, NULL, sdio_cd_thread, sc, sc->priority, &sc->state, "sdio_cd_thread" );
			if( status == EOK ) {
				return( EOK );
			}
		}
	}

	sdio_dinit( sc );

	return( status ? status : errno );
}

int _sdio_disconnect( void )
{
	struct sdio_device	*device;

	sdio_cd_dinit( &sdio_ctrl );	// cleanup/destry cd thread

	pthread_mutex_lock( &sdio_ctrl.mutex );
	device = TAILQ_FIRST( &sdio_ctrl.dlist );
	memset( &sdio_ctrl.connect_parm, 0, sizeof( sdio_connect_parm_t ) );
	pthread_mutex_unlock( &sdio_ctrl.mutex );

	if( device ) {
		return( EBUSY );
	}

	sdio_dinit( &sdio_ctrl );

#ifdef SDIO_PCI_SUPPORT
	sdio_pci_dinit( );
#endif

	return( EOK );
}

int _sdio_connect( sdio_connect_parm_t * const parm, struct sdio_connection ** const connection )
{
	sdio_hc_t		*hc;
	char			*margs;
	int				tmr;
	int				status;
	int				occurence;

	status = sdio_init( &sdio_ctrl );
	if( status != EOK ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s: sdio_init failure", __FUNCTION__ );
		return( status );
	}

#ifdef SDIO_PCI_SUPPORT
	sdio_pci_init( );
#endif

		// parse command line args
	occurence = 1;
	while( ( margs = sdio_module_args( "sdio", parm->argc, parm->argv, occurence++ ) ) ) {
			// ignore args if we are scanning
		if( !( sdio_ctrl.flags & SDIO_CFLAG_SCAN ) ) {
			hc = sdio_hc_alloc( );
			if( hc != NULL ) {
				status = sdio_options( hc, margs );
			}
			else {
				status = ENOMEM;
			}
		}
		free( margs );
		if( status ) {
			sdio_dinit( &sdio_ctrl );
			return( status );
		}
	}

		// no args or scan required
	if( ( TAILQ_FIRST( &sdio_ctrl.hlist ) == NULL ) || ( sdio_ctrl.flags & SDIO_CFLAG_SCAN ) ) {
#ifdef SDIO_PCI_SUPPORT
		sdio_pci_scan( );
#endif
#ifdef SDIO_SOC_SUPPORT
		sdio_soc_scan( );
#endif
	}
	else {
		for( hc = TAILQ_FIRST( &sdio_ctrl.hlist ); hc; hc = TAILQ_NEXT( hc, hlink ) ) {
#ifdef SDIO_PCI_SUPPORT
			status = sdio_pci_device( hc );
			if( status == EOK ) {
				continue;
			}
#endif

#ifdef SDIO_SOC_SUPPORT
			status = sdio_soc_device( hc );
			if( status == EOK ) {
				continue;
			}
#endif
			sdio_dinit( &sdio_ctrl );
			return( status );
		}
	}

	for( hc = TAILQ_FIRST( &sdio_ctrl.hlist ), tmr = 0; hc; hc = TAILQ_NEXT( hc, hlink ) ) {
		status = sdio_hc_init( hc );
		if( status != EOK ) {
			sdio_dinit( &sdio_ctrl );
			return( status );
		}

		if( ( hc->caps & HC_CAP_CD_INTR ) ) {
#if ( __PTR_BITS__ == 64 )
			MsgSendPulsePtr( sdio_ctrl.cd_coid, hc->priority, SDIO_EV_CD, hc );
#else
			MsgSendPulse( sdio_ctrl.cd_coid, hc->priority, SDIO_EV_CD, (uintptr_t)hc );
#endif
		}
		else {
			sdio_cd( hc );
		}

		if( !( hc->caps & HC_CAP_CD_INTR ) && !( hc->caps & HC_CAP_SLOT_TYPE_EMBEDDED ) ) {
			tmr = SDIO_TRUE;
		}
	}

	if( tmr ) {
		sdio_timer_settime( sdio_ctrl.cd_timerid, SDIO_CD_INTERVAL, 0, SDIO_TRUE );
	}

	return( TAILQ_FIRST( &sdio_ctrl.hlist ) ? EOK : ENODEV );
}

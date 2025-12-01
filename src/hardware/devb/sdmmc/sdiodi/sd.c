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

// Module Description:

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <gulliver.h>

#include <internal.h>

#if 0
static const uint32_t sd_tran_speed_fu[] = { 10000, 100000, 1000000, 10000000, 0, 0, 0, 0 };
static const uint32_t sd_tran_speed_mf[] = { 0, 10, 12, 13, 15, 20, 25, 30,
											35, 40, 45, 50, 55, 60, 70, 80 };
#endif

#if 0
static const uint32_t sd_tacc_tu[] = { 1, 10, 100, 1000, 100000, 1000000, 10000000 };
static const uint32_t sd_tacc_mf[] = { 0, 10, 12, 13, 15, 20, 25, 30,
								35, 40, 45, 50, 55, 60, 70, 80 };
#endif

#if 0
static sdio_device_errata_t sd_errata[] = {
		// secusmart devices require extra 50ms after ASSD selection
	{ .pnm = "SD0", .mid = 0x27, .oid = DEV_ERRATA_WILDCARD, .prv_s = 0, .prv_e = DEV_ERRATA_WILDCARD, .erev = DEV_ERRATA_WILDCARD, .errata = 0, .rsettle = 50 },

	{ .pnm = NULL, .mid = 0, .oid = 0, .prv_s = 0, .prv_e = 0, .erev = 0, .errata = 0, .rsettle = 0 }
};
#endif

static int sd_parse_cid( sdio_dev_t * const dev, sdio_cid_t *cid, uint32_t * const rsp )
{
	int		idx;

	memset( cid, 0, sizeof( sdio_cid_t ) );

	cid->mid	= sdio_extract_bits( rsp, 128, 120, 8 );
	cid->oid	= sdio_extract_bits( rsp, 128, 104, 16 );

	for( idx = 0; idx < 5; idx++ ) {
		cid->pnm[idx] = (char)sdio_extract_bits( rsp, 128, 96 - idx * 8, 8 );
	}

	cid->psn	= sdio_extract_bits( rsp, 128, 24, 32 );
	cid->prv	= sdio_extract_bits( rsp, 128, 56, 8 );
	cid->month	= sdio_extract_bits( rsp, 128, 8, 4 );
	cid->year	= sdio_extract_bits( rsp, 128, 12, 8 ) + 2000;

	return( EOK );
}

static int sd_parse_csd( sdio_dev_t *dev, sdio_csd_t *csd, uint32_t * const rsp )
{
	uint32_t				blksz;
	uint32_t				csize;
	uint32_t				csizem;
	static const uint32_t	sd_tran_speed_fu[] = { 10000, 100000, 1000000, 10000000, 0, 0, 0, 0 };
	static const uint32_t	sd_tran_speed_mf[] = { 0, 10, 12, 13, 15, 20, 25, 30,
													35, 40, 45, 50, 55, 60, 70, 80 };

	memset( csd, 0, sizeof( sdio_csd_t ) );
	csd->csd_structure      = (uint8_t)sdio_extract_bits( rsp, 128, 126, 2 );
	csd->taac               = (uint8_t)sdio_extract_bits( rsp, 128, 112, 8 );
	csd->nsac               = (uint8_t)sdio_extract_bits( rsp, 128, 104, 8 );
	csd->tran_speed         = (uint8_t)sdio_extract_bits( rsp, 128, 96, 8 );
	csd->ccc                = (uint16_t)sdio_extract_bits( rsp, 128, 84, 12 );
	csd->read_bl_len        = (uint8_t)sdio_extract_bits( rsp, 128, 80, 4 );
	csd->read_bl_partial    = (uint8_t)sdio_extract_bits( rsp, 128, 79, 1 );
	csd->write_blk_misalign = (uint8_t)sdio_extract_bits( rsp, 128, 78, 1 );
	csd->read_blk_misalign  = (uint8_t)sdio_extract_bits( rsp, 128, 77, 1 );
	csd->dsr_imp            = (uint8_t)sdio_extract_bits( rsp, 128, 76, 1 );

	switch( csd->csd_structure ) {
		case CSD_STRUCT_VER_10:			// Standard Capacity
			csd->c_size			= sdio_extract_bits( rsp, 128, 62, 12 );
			csd->vdd_r_curr_min	= (uint8_t)sdio_extract_bits( rsp, 128, 59, 3 );
			csd->vdd_r_curr_max	= (uint8_t)sdio_extract_bits( rsp, 128, 56, 3 );
			csd->vdd_w_curr_min	= (uint8_t)sdio_extract_bits( rsp, 128, 53, 3 );
			csd->vdd_w_curr_max	= (uint8_t)sdio_extract_bits( rsp, 128, 50, 3 );
			csd->c_size_mult	= (uint8_t)sdio_extract_bits( rsp, 128, 47, 3 );
			blksz				= 1 << csd->read_bl_len;
			csize				= csd->c_size + 1;
			csizem				= 1 << ( csd->c_size_mult + 2 );
			break;

		case CSD_STRUCT_VER_20:			// High Capacity / Extended Capacity
			csd->c_size			= sdio_extract_bits( rsp, 128, 48, 22 );
			blksz				= SDIO_DFLT_BLKSZ;
			csize				= csd->c_size + 1;
			csizem				= 1024;
			dev->caps			|= DEV_CAP_HC;
			break;

		case CSD_STRUCT_VER_30:			// Ultra Capacity
			csd->c_size			= sdio_extract_bits( rsp, 128, 48, 28 );
#ifndef SDIO_UC_SUPPORT
				// limit size to 22 bits until we get a card to test
			if( csd->c_size > CSD_VER_20_CSIZE_MAX ) {
				csd->c_size = CSD_VER_20_CSIZE_MAX;
			}
#endif
			blksz				= SDIO_DFLT_BLKSZ;
			csize				= csd->c_size + 1;
			csizem				= 1024;
			dev->caps			|= DEV_CAP_HC | DEV_CAP_UC;
			break;

		default:
			return( EINVAL );
	}

	csd->erase_blk_en       = (uint8_t)sdio_extract_bits( rsp, 128, 46, 1 );
	csd->sector_size        = (uint8_t)sdio_extract_bits( rsp, 128, 39, 7 );
	csd->wp_grp_size        = (uint8_t)sdio_extract_bits( rsp, 128, 32, 7 );
	csd->wp_grp_enable      = (uint8_t)sdio_extract_bits( rsp, 128, 31, 1 );
	csd->r2w_factor         = (uint8_t)sdio_extract_bits( rsp, 128, 26, 3 );
	csd->write_bl_len       = (uint8_t)sdio_extract_bits( rsp, 128, 22, 4 );
	csd->write_bl_partial   = (uint8_t)sdio_extract_bits( rsp, 128, 21, 1 );
//	csd->file_format_grp	= (uint8_t)sdio_extract_bits( rsp, 128, 15, 1 );
//	csd->copy               = (uint8_t)sdio_extract_bits( rsp, 128, 14, 1 );
	csd->write_protect		= (uint8_t)sdio_extract_bits( rsp, 128, 12, 2 );
//	csd->file_format		= (uint8_t)sdio_extract_bits( rsp, 128, 10, 2 );

		// force to 512 byte block
	if( ( blksz > SDIO_DFLT_BLKSZ ) && ( ( blksz % SDIO_DFLT_BLKSZ ) == 0 ) ) {
		csize = csize * ( blksz / SDIO_DFLT_BLKSZ );
		blksz = SDIO_DFLT_BLKSZ;
	}

	csd->blksz		= blksz;
	csd->sectors	= csize * csizem;

	csd->dtr_max	= sd_tran_speed_fu[csd->tran_speed & 0x7] *
						sd_tran_speed_mf[(csd->tran_speed >> 3 ) & 0xf];

	return( EOK );
}

static int sd_parse_scr( sdio_dev_t *dev, sd_scr_t *scr, uint32_t *rscr )
{
	memset( scr, 0, sizeof( sd_scr_t ) );

	rscr[0] = ENDIAN_BE32( rscr[0] );
	rscr[1] = ENDIAN_BE32( rscr[1] );

	scr->scr_structure			= (uint8_t)sdio_extract_bits( rscr, 64, 60, 4 );
	scr->sd_spec				= (uint8_t)sdio_extract_bits( rscr, 64, 56, 4 );
	scr->data_stat_after_erase	= (uint8_t)sdio_extract_bits( rscr, 64, 55, 1 );
	scr->sd_security			= (uint8_t)sdio_extract_bits( rscr, 64, 52, 3 );
	scr->sd_bus_widths			= (uint8_t)sdio_extract_bits( rscr, 64, 48, 4 );

	scr->sd_spec3				= (uint8_t)sdio_extract_bits( rscr, 64, 47, 1 );
	scr->ex_security			= (uint8_t)sdio_extract_bits( rscr, 64, 43, 4 );
	scr->sd_spec4				= (uint8_t)sdio_extract_bits( rscr, 64, 42, 1 );
	scr->sd_specx				= (uint8_t)sdio_extract_bits( rscr, 64, 38, 4 );
	scr->cmd_support			= (uint8_t)sdio_extract_bits( rscr, 64, 32, 4 );

	if( ( scr->cmd_support & SCR_CMD23_SUP ) ) {
		dev->caps				|= DEV_CAP_CMD23;
	}

	return( EOK );
}

// read, write extention register
static int sd_rw_extr( sdio_dev_t * const dev, const int dir, const int mio, const int fno, const int mw, const uint32_t addr, const int len_msk, uint8_t * const extr )
{
	sdio_cmd_t		*cmd;
	int				cc;
	int				arg;
	uint8_t			*ebuf;
	int				status;
	sdio_sge_t		sge;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	ebuf = sdio_alloc( SD_EXTR_SIZE );
	if( ebuf == NULL ) {
		sdio_free_cmd( cmd );
		return( ENOMEM );
	}

	memset( ebuf, 0, SD_EXTR_SIZE );

	cc = ( dir & SCF_DIR_OUT ) ? SD_WRITE_EXTR_SINGLE : SD_READ_EXTR_SINGLE;

#if 0
[31]MIO 0:Memory, 1:I/O
[30:27] FNO
[26] MW
[25:9] ADDR
[8:0] LEN/MASK mw=1 8bits, mw=0 9bits
#endif

	arg = SD_EXTR_MIO( mio );
	arg |= SD_EXTR_FNO( fno );
	arg |= ( SD_EXTR_MWM( mw ) );
	arg |= ( SD_EXTR_ADDR( addr ) );
	arg |= ( SD_EXTR_LEN_MSK( len_msk, mw ) );

	sdio_setup_cmd( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1, cc, arg );
	sge.sg_count	= SD_EXTR_SIZE;
	sge.sg_address	= SDIO_DATA_PTR_P( ebuf );
	sdio_setup_cmd_io( cmd, dir, 1, SD_EXTR_SIZE, &sge, 1, NULL );

	if( ( dir & SCF_DIR_OUT ) ) {
		memcpy( ebuf, extr, mw ? 1 : len_msk );
	}

	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	if( status == EOK ) {
		if( ( dir & SCF_DIR_IN ) ) {
			memcpy( extr, ebuf, mw ? 1 : len_msk );
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s: Error %s %s extention register (%d) - fno %d, addr 0x%x, %s 0x%x",
			__FUNCTION__,  ( dir & SCF_DIR_OUT ) ? "writting" : "reading", mio ? "io" : "memory", status, fno, addr,
			mw ? "mask" : "len", len_msk );

	}

	sdio_free( ebuf, SD_EXTR_SIZE );

	sdio_free_cmd( cmd );

	return( status );
}

static int sd_extr_wr( sdio_dev_t * const dev, const uint32_t rsa, const uint32_t reg, uint8_t wval )
{
	return( sd_rw_extr( dev, SCF_DIR_OUT, SD_EXTR_MEM, SD_FCN_EXT_FNO( rsa ), 0, SD_FCN_EXT_SADDR( rsa ) + reg, 1, &wval ) );
}

static int sd_extr_rd( sdio_dev_t * const dev, const uint32_t rsa, const uint32_t reg, uint8_t * const rval )
{
	return( sd_rw_extr( dev, SCF_DIR_IN, SD_EXTR_MEM, SD_FCN_EXT_FNO( rsa ), 0, SD_FCN_EXT_SADDR( rsa ) + reg, 1, rval ) );
}

static int sdio_send_relative_addr( sdio_hc_t *hc, uint32_t *rca )
{
	sdio_dev_t	*dev;
	sdio_cmd_t	*cmd;
	int			status;
	int			retries;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	dev	= &hc->device;
	*rca	= 0;
	retries	= SD_SEND_RELATIVE_ADDR_RETRIES;

	do {
		sdio_setup_cmd( cmd, SCF_CTYPE_BCR | SCF_RSP_R6, SD_SEND_RELATIVE_ADDR, 0 );
		status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
		if( status != EOK ) {
			break;
		}

		*rca = cmd->rsp[0] >> 16;
		if( *rca != 0 ) {							// wait for non-zero RCA
			break;
		}
	} while( retries-- );

	if( *rca == 0 ) {								// validate RCA
		status = EIO;
	}

	sdio_free_cmd( cmd );

	return( status );
}

int sd_pwroff_notify( sdio_dev_t * const dev, const int op, int timeout )
{
	uint8_t		rval;
	int			status;

#if 0
1) Host checks support bit of Power Off Notification. If POFS=1, host may use Power Off Notification.
2) Just before host would like to shut down card power, host requests Power Off Notification to the card by setting POFN=1.
3) Host polls Power Management Status register until the card indicates ready to power off (POFR=1).
#endif

	if( op < SDIO_POWER_OFF_SHORT ) {
		return( EOK );
	}

	status = sd_extr_wr( dev, dev->pwr_mgnt_fcn_rsa, PWR_MGNT_SETTING, PWR_MGNT_SETTING_POFN );
	if( status != EOK ) {
		return( status );
	}

		// wait up to 1 second for POFR
	for( timeout = 1000; timeout; timeout-- ) {
		status = sd_extr_rd( dev, dev->pwr_mgnt_fcn_rsa, PWR_MGNT_STATUS, &rval );
		if( status == EOK ) {
			if( ( rval & PWR_MGNT_STATUS_POFR ) ) {
				break;
			}
		}
		delay( 1 );
	}

	if( !timeout ) {
		status = ETIMEDOUT;
	}

	return( status );
}

int sd_cache( sdio_dev_t *dev, const int op, uint32_t timeout )
{
	uint8_t		rval;
	int			status;

	switch( op ) {
		case SDIO_CACHE_DISABLE:
			status = sd_extr_wr( dev, dev->perf_enh_fcn_rsa, PERF_ENH_CACHE, 0 );
			if( status == EOK ) {
				dev->flags &= ~DEV_FLAG_WCE;
			}
			break;

		case SDIO_CACHE_ENABLE:
			status = sd_extr_wr( dev, dev->perf_enh_fcn_rsa, PERF_ENH_CACHE, PERF_ENH_CACHE_EN );
			if( status == EOK ) {
				dev->flags |= DEV_FLAG_WCE;
			}
			break;

		case SDIO_CACHE_FLUSH:
			if( ( dev->flags & DEV_FLAG_WCE ) ) {
				if( ( status = sd_extr_wr( dev, dev->perf_enh_fcn_rsa, PERF_ENH_CACHE_FLUSH, PERF_ENH_CACHE_FLUSH_START ) ) ) {
					return( status );
				}

					// wait up to 1 second
				for( timeout = 1000; timeout; timeout-- ) {
					status = sd_extr_rd( dev, dev->perf_enh_fcn_rsa, PERF_ENH_CACHE_FLUSH, &rval );
					if( status == EOK ) {
						if( ( rval & PERF_ENH_CACHE_FLUSH_START ) == 0 ) {
							status = EOK;
							break;
						}
						delay( 1 );
					}
				}

				if( !timeout ) {
					status = ETIMEDOUT;
				}
			}
			else {
				status = EOK;
			}
			break;

		default:
			status = EINVAL;
			break;
	}

	return( status );
}

static int sd_voltage_switch( sdio_hc_t *hc )
{
	sdio_dev_t		*dev;
	sdio_cmd_t		*cmd;
	int				status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	dev	= &hc->device;
	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, SD_VOLTAGE_SWITCH, 0 );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	sdio_free_cmd( cmd );

	return( status );
}

int sd_switch( sdio_dev_t * const dev, const int mode, const int grp, const uint8_t val, uint8_t * const switch_status )
{
	sdio_cmd_t		*cmd;
	uint32_t		arg;
	uint8_t			*sbuf;
	int				status;
	uint16_t		sw_status;
	sdio_sge_t		sge;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sbuf = sdio_alloc( SD_SF_STATUS_SIZE );
	if( sbuf == NULL ) {
		sdio_free_cmd( cmd );
		return( ENOMEM );
	}

	arg = ( mode << 31 ) | 0x00ffffffU;
	arg &= ~( 0xf << ( grp * 4 ) );
	arg |= ( val << ( grp * 4 ) );

	sdio_setup_cmd( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1, SD_SWITCH_FUNC, arg );
	sge.sg_count = SD_SF_STATUS_SIZE;
	sge.sg_address = SDIO_DATA_PTR_P( sbuf );
	sdio_setup_cmd_io( cmd, SCF_DIR_IN, 1, SD_SF_STATUS_SIZE, &sge, 1, NULL );

	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	if( status == EOK ) {
		memcpy( switch_status, sbuf, SD_SF_STATUS_SIZE );

		if( mode == SD_SF_MODE_SET ) {
				// check function busy status
			sw_status = switch_status[29 - ( grp * 2 )] | ( switch_status[28 - ( grp * 2 )] << 8 );
			if( sw_status & ( 1 << val ) ) {
				status = EBUSY;
			}
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, 0, 0, "%s: Error Switching %d", __FUNCTION__, status );
	}

	sdio_free( sbuf, SD_SF_STATUS_SIZE );
	sdio_free_cmd( cmd );

	return( status );
}

int sd_app_cmd( sdio_dev_t * const dev )
{
	sdio_cmd_t		*cmd;
	int				ctype;
	int				status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	ctype = dev->rca ? SCF_CTYPE_BCR : SCF_CTYPE_BC;

	sdio_setup_cmd( cmd, ctype | SCF_RSP_R1, SD_APP_CMD, dev->rca << 16 );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	sdio_free_cmd( cmd );

	return( status );
}

static int sd_app_send_scr( sdio_dev_t * const dev, uint32_t * const scr )
{
	sdio_cmd_t		*cmd;
	uint32_t		*sbuf;
	int				status;
	sdio_sge_t		sge;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sbuf = sdio_alloc( SD_SCR_SIZE );
	if( sbuf == NULL ) {
		sdio_free_cmd( cmd );
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1 | SCF_APP_CMD, SD_AC_SEND_SCR, 0 );
	sge.sg_count = SD_SCR_SIZE;
	sge.sg_address = SDIO_DATA_PTR_P( sbuf );
	sdio_setup_cmd_io( cmd, SCF_DIR_IN, 1, SD_SCR_SIZE, &sge, 1, NULL );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	if( status == EOK ) {
		memcpy( scr, sbuf, SD_SCR_SIZE );
	}

	sdio_free( sbuf, SD_SCR_SIZE );
	sdio_free_cmd( cmd );

	return( status );
}

int sd_app_sd_status( sdio_dev_t * const dev, uint32_t * const sds )
{
	sdio_cmd_t		*cmd;
	uint32_t		*sbuf;
	int				status;
	sdio_sge_t		sge;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sbuf = sdio_alloc( SD_STATUS_SIZE );
	if( sbuf == NULL ) {
		sdio_free_cmd( cmd );
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_ADTC | SCF_RSP_R1 | SCF_APP_CMD, SD_AC_SD_STATUS, 0 );
	sge.sg_count = SD_STATUS_SIZE;
	sge.sg_address = SDIO_DATA_PTR_P( sbuf );
	sdio_setup_cmd_io( cmd, SCF_DIR_IN, 1, SD_STATUS_SIZE, &sge, 1, NULL );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	if( status == EOK ) {
		memcpy( sds, sbuf, SD_STATUS_SIZE );
	}

	sdio_free( sbuf, SD_STATUS_SIZE );
	sdio_free_cmd( cmd );

	return( status );
}

static uint64_t sd_erase_grp_size( sdio_dev_t * const dev )
{
	uint64_t	erase_grp_size;

	if( dev->csd.erase_blk_en ) {
		erase_grp_size = 512;
	}
	else {
		erase_grp_size = ( dev->csd.sector_size + 1 ) * ( 1 << dev->csd.write_bl_len );
	}

	return( erase_grp_size );
}

uint64_t sd_erase_timeout( sdio_dev_t * const dev, const uint32_t etype, const uint32_t nlba )
{
	uint64_t		timeout;

	if( dev->sds.erase_timeout ) {
		timeout = ( dev->sds.erase_timeout * nlba ) + dev->sds.erase_offset;
	}
	else {
		timeout = SD_ERASE_TIMEOUT * nlba;
	}

	return( timeout );
}

int sd_erase( sdio_dev_t * const dev, const uint32_t flgs, uint64_t lba, uint32_t nlba )
{
	sdio_cmd_t		*cmd;
	int				status;
	uint64_t		timeout;
	uint64_t		addr;

	status			= EOK;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	while( nlba ) {
		addr = ( dev->caps & DEV_CAP_HC ) ? lba : ( lba * dev->csd.blksz );
		sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, SD_ERASE_WR_BLK_START, addr );
		status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
		if( status != EOK ) {
			break;
		}

		timeout = sd_erase_timeout( dev, flgs, nlba );
		lba		+= nlba;
		nlba	-= nlba;
		lba--;

		addr = ( dev->caps & DEV_CAP_HC ) ? lba : ( lba * dev->csd.blksz );
		sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1, SD_ERASE_WR_BLK_END, addr );
		status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
		if( status != EOK ) {
			break;
		}

		sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_RSP_R1B | SCF_WAIT_DRDY, SD_ERASE, flgs );
		status = _sdio_send_cmd( dev, cmd, NULL, timeout, 0 );
		if( status != EOK ) {
			break;
		}
	}

	sdio_free_cmd( cmd );

	return( status );
}

int sd_send_if_cond( sdio_hc_t *hc, const uint32_t vhs )
{
	sdio_dev_t		*dev;
	sdio_cmd_t		*cmd;
	int				status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	dev	= &hc->device;

	sdio_setup_cmd( cmd, SCF_CTYPE_BCR | SCF_RSP_R7, SD_SEND_IF_COND, ( vhs << 8 ) | SD_SIC_TEST_PATTERN );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, 0 );
	if( status == EOK ) {
		if( ( cmd->rsp[0] & 0xff ) != SD_SIC_TEST_PATTERN ) {
			status = EIO;
		}
	}
	else {
		if( cmd->status == CS_CMD_TO_ERR ) {
			status = ETIMEDOUT;
		}
		else {
			status = EIO;
		}
	}

	sdio_free_cmd( cmd );

	return( status );
}

static int sd_app_send_op_cond( sdio_hc_t *hc, const uint32_t ocr, uint32_t *rocr )
{
	sdio_dev_t		*dev;
	sdio_cmd_t		*cmd;
	int				retry;
	int				status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	status	= EOK;
	dev		= &hc->device;

	sdio_setup_cmd( cmd, SCF_CTYPE_BCR | SCF_RSP_R3 | SCF_APP_CMD, SD_AC_SEND_OP_COND, ocr );

	for( retry = OCR_BUSY_RETRIES; retry; retry-- ) {
		status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
		if( status != EOK ) {
			break;
		}

		if( !ocr ) {
			break;
		}

		if( ( cmd->rsp[0] & OCR_PWRUP_CMP ) ) {
			break;
		}

		delay( 10 );
	}

	if( hc->cfg.verbosity > 3 ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 0, "%s: retries %d, rsp0 0x%x", __FUNCTION__, OCR_BUSY_RETRIES - retry, cmd->rsp[0] );
	}

	if( rocr ) {
		*rocr = cmd->rsp[0];
	}

	sdio_free_cmd( cmd );

	if( !retry ) {
		status = ETIMEDOUT;
	}

	return( status );
}

static int sd_read_switch( sdio_dev_t *dev )
{
	sdio_hc_t			*hc;
	sd_switch_cap_t		*swcaps;
	int					status;
	uint8_t				ss[SD_SF_STATUS_SIZE];

	hc		= dev->hc;
	swcaps	= &dev->swcaps;

	if( !( dev->csd.ccc & CCC_SWITCH ) ) {
		return( EOK );
	}

		// get supported bus speeds
	if( ( status = sd_switch( dev, SD_SF_MODE_CHECK, SD_SF_GRP_BUS_SPD, SD_SF_CUR_FCN, ss ) ) ) {
		return( status );
	}

	if( ( ss[13] & 0x2 ) ) {
		swcaps->dtr_max_hs	= 50000000;

		if( ( hc->caps & HC_CAP_HS ) ) {
			dev->caps			|= DEV_CAP_HS;
		}
	}

	if( dev->scr.sd_spec3 ) {
		swcaps->bus_mode = ss[13] & SD_BUS_MODE_MSK;
		swcaps->drv_type = ss[9] & SD_DRV_TYPE_MSK;
		swcaps->curr_limit = ss[7] & SD_CURR_LIMIT_MSK;

		if( ( swcaps->bus_mode & SD_BUS_MODE_UHS ) ) {
			dev->caps |= DEV_CAP_UHS;
		}
	}

	swcaps->cmd_sys = ss[11];

	if( ( ss[11] & SD_CMD_SYS_EC ) || ( ss[11] & SD_CMD_SYS_ASSD ) ) {
		dev->caps |= DEV_CAP_ASSD;
	}

	return( status );
}

static int sd_read_status( sdio_dev_t *dev )
{
	int				idx;
	int				status;
	uint32_t		au;
	uint32_t		e_size;
	uint32_t		e_offset;
	uint32_t		e_timeout;
	uint32_t		sds[SD_STATUS_SIZE / 4];


	if( !( dev->csd.ccc & CCC_APP_SPEC ) ) {
		return( EOK );
	}

	if( ( status = sd_app_sd_status( dev, sds ) ) ) {
		return( status );
	}

	for( idx = 0; idx < 16; idx++ ) {
		sds[idx] = ENDIAN_BE32( sds[idx] );
	}

	au = sdio_extract_bits( sds, 512, 428, 4 );
	if( au ) {
		dev->sds.au_size = 1 << ( au + 4 );

		e_size			= sdio_extract_bits( sds, 512, 408, 16 );
		e_offset		= sdio_extract_bits( sds, 512, 400, 2 );
		e_timeout		= sdio_extract_bits( sds, 512, 402, 6 );
		if( e_size && e_timeout ) {
			dev->sds.erase_offset		= e_offset * 1000;
			dev->sds.erase_timeout		= ( e_timeout * 1000 ) / e_size;
		}
	}

	au = sdio_extract_bits( sds, 512, 392, 4 );
	if( au ) {
//		dev->sds.uhs_au_size = 1 << ( au + 4 );
	}

	dev->sds.perf_class			= sdio_extract_bits( sds, 512, 336, 4 );
	dev->sds.perf_enh			= sdio_extract_bits( sds, 512, 328, 8 );
	dev->sds.speed_class		= sdio_extract_bits( sds, 512, 440, 8 );
	dev->sds.uhs_speed_grade	= sdio_extract_bits( sds, 512, 396, 4 );
	return( status );
}

__attribute__((used))
static int sd_parse_extended_fcn( sdio_dev_t *dev )
{
	sd_fcn_ext_hdr	*feh;
	sd_fcn_ext		*fext;
	uint32_t		fcn;
	uint8_t			rval;
	int				status;
	uint8_t			efb[SD_FCN_PG_SZ * 2] = { 0 };	// 512 * 2

	feh		= (sd_fcn_ext_hdr *)efb;
	fext	= (sd_fcn_ext *)(feh + 1);

	if( !( dev->scr.cmd_support & SCR_CMD48_49_SUP ) ) {
		return( ENOTSUP );
	}

		// read general information page 0
	if( ( status = sd_rw_extr( dev, SCF_DIR_IN, SD_EXTR_MEM, 0, 0, 0, SD_FCN_PG_SZ, efb ) ) ) {
		return( status );
	}

		// read general information page 1 (optional no error check)
	sd_rw_extr( dev, SCF_DIR_IN, SD_EXTR_MEM, 0, 0, SD_FCN_PG_SZ, SD_FCN_PG_SZ, efb +  SD_FCN_PG_SZ );

	for( fcn = 1; fcn <= feh->number_extensions; fcn++ ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, dev->hc->cfg.verbosity, 0, "%s: fcn %d, sfc %d, fcc %d, fmc %d, fmn %s, pfc %d, fn %s, pne %d, nrs %d, rsa 0x%x, fno %d, saddr 0x%x",
			__FUNCTION__, fcn, fext->sfc, fext->fcc, fext->fmc, fext->fmn, fext->pfc, fext->fn, fext->pne, fext->nrs,
			fext->rsa[0], SD_FCN_EXT_FNO( fext->rsa[0] ), SD_FCN_EXT_SADDR( fext->rsa[0] ) );
		switch( fext->sfc ) {
			case SFC_PWR_MGNT:
				dev->pwr_mgnt_fcn_rsa = fext->rsa[0];
				status = sd_extr_rd( dev, dev->pwr_mgnt_fcn_rsa, PWR_MGNT_STATUS, &rval );
				if( status == EOK ) {
					if( ( rval & PWR_MGNT_STATUS_POFS ) ) {
						dev->caps |= DEV_CAP_PWROFF_NOTIFY;
					}
				}
				break;
			case SFC_PERF_ENH:
				dev->perf_enh_fcn_rsa = fext->rsa[0];
				status = sd_extr_rd( dev, dev->perf_enh_fcn_rsa, PERF_ENH_CACHE_SUP, &rval );
				if( status == EOK ) {
					if( ( rval & PERF_ENH_CACHE_SUPPORTED ) ) {
						dev->caps |= DEV_CAP_CACHE;
					}
				}
				break;
			default:
				break;
		}
		fext = (sd_fcn_ext *)&efb[fext->pne];

			// range check
		if( ( (uintptr_t)fext + sizeof( sd_fcn_ext ) ) > (uintptr_t)( efb + sizeof( efb ) ) ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, dev->hc->cfg.verbosity, 0, "%s: Range limit", __FUNCTION__ );
			break;
		}
	}

	return( status );
}

static int sd_switch_hs( sdio_dev_t * const dev )
{
	sdio_hc_t	*hc;
	int			status;
	uint8_t		sw_status[64] = { 0 };

	hc = dev->hc;

	status = sd_switch( dev, SD_SF_MODE_SET, SD_SF_GRP_BUS_SPD, 1, sw_status );
	if( status == EOK ) {
		if( ( sw_status[16] & 0xF ) != 0x1 ) {
			status = EIO;
		}
	}

	if( status ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: Error Switching", __FUNCTION__ );
	}

	return( status );
}

static int sd_app_set_bus_width( sdio_dev_t * const dev, const uint32_t width )
{
	sdio_cmd_t		*cmd;
	int				status;

	cmd = sdio_alloc_cmd( );
	if( cmd == NULL ) {
		return( ENOMEM );
	}

	sdio_setup_cmd( cmd, SCF_CTYPE_AC | SCF_APP_CMD | SCF_RSP_R1, SD_AC_SET_BUS_WIDTH, width );
	status = _sdio_send_cmd( dev, cmd, NULL, SDIO_TIME_DEFAULT, SDIO_CMD_RETRIES );
	sdio_free_cmd( cmd );

	return( status );
}

static int sd_set_bus_width( sdio_dev_t * const dev, const uint32_t width )
{
	sdio_hc_t	*hc;
	uint32_t	bus_width;
	int			status;

	hc		= dev->hc;
	status	= EOK;

	switch( width ) {
		case BUS_WIDTH_1:
			bus_width = SD_BUS_WIDTH_1;
			break;

		case BUS_WIDTH_4:
			if( ( hc->caps & HC_CAP_BW4 ) && ( dev->scr.sd_bus_widths & SCR_BUS_WIDTH_4 ) ) {
				bus_width = SD_BUS_WIDTH_4;
			}
			else {
				status = EINVAL;
			}
			break;

		case BUS_WIDTH_8:
		default:
			status = EINVAL;
			break;
	}

	if( status == EOK ) {
		status = sd_app_set_bus_width( dev, bus_width );
		if( status == EOK ) {
			sdio_bus_width( hc, BUS_WIDTH_4 );
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: Error Setting Bus Width %d", __FUNCTION__, width );
	}

	return( status );
}

static int sd_set_drv_type( sdio_dev_t * const dev, const int bus_spd, uint32_t drv_type )
{
	sdio_hc_t	*hc;
	int			idx;
	int			status;
	int			driver_strength;
	static int	timing[5]	= { TIMING_SDR12, TIMING_SDR25, TIMING_SDR50,
								TIMING_SDR104, TIMING_DDR50 };
	uint8_t		sw_status[64] = { 0 };

	hc = dev->hc;

	if( !HC_CAP_DRV_TYPES( hc->caps ) || !hc->entry.driver_strength ) {
		return( EOK );
	}

	idx			= ( bus_spd ) ? ( fls( bus_spd ) ) - 1 : 0;

	if( idx >= SDIO_NELEMS( timing ) ) {
		return( EINVAL );
	}

	drv_type		= hc->entry.driver_strength( hc, timing[idx], drv_type | SD_DRV_TYPE_B );
	driver_strength	= fls( drv_type ) - 1;
	status			= sd_switch( dev, SD_SF_MODE_SET, SD_SF_GRP_DRV_STR, driver_strength, sw_status );
	if( status == EOK ) {
		if( ( sw_status[15] & 0xF ) == driver_strength ) {
			sdio_drv_type( hc, drv_type );
		}
		else {
			status = EIO;
		}
	}

	if( status ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: Error Switching drive strength %d", __FUNCTION__, driver_strength );
	}

	return( status );
}

static int sd_set_bus_speed_mode( sdio_dev_t * const dev, const int bus_spd )
{
	sdio_hc_t		*hc;
	int				idx;
	int				status;
	static uint32_t	dtr[5]		= { DTR_MAX_SDR12, DTR_MAX_SDR25, DTR_MAX_SDR50,
									DTR_MAX_SDR104, DTR_MAX_DDR50 };
	static uint32_t	timing[5]	= { TIMING_SDR12, TIMING_SDR25, TIMING_SDR50,
									TIMING_SDR104, TIMING_DDR50 };
	uint8_t			sw_status[64] = { 0 };

	if( !bus_spd ) {
		return( EOK );
	}

	hc	= dev->hc;
	idx	= fls( bus_spd ) - 1;		// get index to fastest speed

	if( idx >= SDIO_NELEMS( timing ) ) {
		return( EINVAL );
	}

	status = sd_switch( dev, SD_SF_MODE_SET, SD_SF_GRP_BUS_SPD, idx, sw_status );
	if( status == EOK ) {
		if( ( sw_status[16] & 0xF ) == idx ) {
			sdio_timing( hc, timing[idx] );
			sdio_clock( hc, dtr[idx] );
		}
		else {
			status = EIO;
		}
	}

	if( status ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: Error Switching speed %d", __FUNCTION__, idx );
	}

	return( status );
}

static int sd_set_current_limit( sdio_dev_t * const dev, const int bus_spd, int curr_limit )
{
	sdio_hc_t	*hc;
	int			idx;
	int			status;
	uint8_t		sw_status[64] = { 0 };

	hc			= dev->hc;
	idx			= 0;

		// reconcile hc and device current limits
	curr_limit	= HC_CAP_CURRENT( hc->caps ) & curr_limit;

	if( curr_limit && ( bus_spd & ( SD_BUS_MODE_SDR50 | SD_BUS_MODE_SDR104 | SD_BUS_MODE_DDR50 ) ) ) {
		idx = fls( curr_limit ) - 1;	// get index to highest current
	}

	status = sd_switch( dev, SD_SF_MODE_SET, SD_SF_GRP_CUR_LMT, idx, sw_status );
	if( status == EOK ) {
		if( ( ( sw_status[15] >> 4 ) & 0x0F ) != idx ) {
			status = EIO;
		}
	}

	if( status ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: Error Switching speed %d, current %d", __FUNCTION__, bus_spd, idx );
	}

	return( status );
}

static int sd_init_assd( sdio_dev_t *dev )
{
	sdio_hc_t			*hc;
	sd_switch_cap_t		*swcaps;
	int					idx;
	int					status;
	uint8_t				sw_status[64] = { 0 };

	hc		= dev->hc;
	swcaps	= &dev->swcaps;

		// get index to eC/ASSD
	idx		= ffs( swcaps->cmd_sys & ( SD_CMD_SYS_EC | SD_CMD_SYS_ASSD ) ) - 1;
	status	= sd_switch( dev, SD_SF_MODE_SET, SD_SF_GRP_CMD_EXT, idx, sw_status );
	if( status == EOK ) {
		if( ( ( sw_status[16] >> 4 ) & 0x0F ) != idx ) {
			status = EIO;
		}
		else {
			if( dev->rsettle ) {	// some devices require more time
				delay( dev->rsettle );
			}
		}
	}

	if( status ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: Error Switching ASSD %d, sw_status 0x%x, status %d", __FUNCTION__, idx, sw_status[16] >> 4, status );
	}

	return( status );
}

static int sd_select_bus_mode( sdio_dev_t * const dev, const int bus_mode )
{
	sdio_hc_t		*hc;
	int				bus_spd_mode;

	hc				= dev->hc;

	if( (bus_mode & SD_BUS_MODE_SDR104) && (hc->caps & HC_CAP_SDR104) ) {
		bus_spd_mode = SD_BUS_MODE_SDR104;
	}
	else if( (bus_mode & SD_BUS_MODE_DDR50) && (hc->caps & HC_CAP_DDR50) ) {
		bus_spd_mode = SD_BUS_MODE_DDR50;
	}
	else if( (bus_mode & SD_BUS_MODE_SDR50) && (hc->caps & HC_CAP_SDR50) ) {
		bus_spd_mode = SD_BUS_MODE_SDR50;
	}
	else if( (bus_mode & SD_BUS_MODE_SDR25) && (hc->caps & HC_CAP_SDR25) ) {
		bus_spd_mode = SD_BUS_MODE_SDR25;
	}
	else {
		bus_spd_mode = SD_BUS_MODE_SDR12;
	}

	return( bus_spd_mode );
}

static int sd_init_uhs( sdio_dev_t *dev )
{
	sdio_hc_t			*hc;
	sd_switch_cap_t		*swcaps;
	int					status;
	int					bus_spd_mode;

	hc		= dev->hc;
	swcaps	= &dev->swcaps;

	status = sd_set_bus_width( dev, BUS_WIDTH_4 );
	if( status != EOK ) {
		return( status );
	}

	bus_spd_mode = sd_select_bus_mode( dev, swcaps->bus_mode );

	status = sd_set_drv_type( dev, bus_spd_mode, swcaps->drv_type );
	if( status != EOK ) {
		return( status );
	}

	status = sd_set_current_limit( dev, bus_spd_mode, swcaps->curr_limit );
	if( status != EOK ) {
		return( status );
	}

	status = sd_set_bus_speed_mode( dev, bus_spd_mode );
	if( status != EOK ) {
		return( status );
	}

	sdio_preset( hc, SDIO_TRUE );
	status = sdio_tune( hc, SD_SEND_TUNING_BLOCK );
	if( status != EOK ) {
			// tuning may have failed, but we should should be able to continue.
			// ie hc will use defaults an we will re-tune if we get a CRC error
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: tuning failure %s (%d)", __FUNCTION__, strerror( status ), status );
	}

	dev->flags |= DEV_FLAG_UHS;

	return( EOK );
}

static int sd_init_bus( sdio_hc_t *hc )
{
	sdio_dev_t		*dev;
	uint32_t		dtr;
	uint32_t		timing;
	int				status;

	dev		= &hc->device;
	dtr		= dev->csd.dtr_max;
	timing	= TIMING_LS;

	if( hc->signal_voltage == SIGNAL_VOLTAGE_1_8 ) {	// UHS card
		status = sd_init_uhs( dev );
		if( status != EOK ) {
			return( status );
		}
	}
	else {
		if( ( dev->caps & DEV_CAP_HS ) ) {				// HS card
			status = sd_switch_hs( dev );
			if( status != EOK ) {
				return( status );
			}

			timing		= TIMING_HS;
			dtr			= dev->swcaps.dtr_max_hs;
			dev->flags	|= DEV_FLAG_HS;
		}

		if( ( hc->caps & HC_CAP_BW4 ) && ( dev->scr.sd_bus_widths & SCR_BUS_WIDTH_4 ) ) {
			status = sd_set_bus_width( dev, BUS_WIDTH_4 );
			if( status != EOK ) {
				return( status );
			}
		}

		sdio_timing( hc, timing );
		sdio_clock( hc, dtr );

		status = _sdio_set_block_length( dev, SDIO_DFLT_BLKSZ );
		if( status != EOK ) {
			return( status );
		}
	}

	if( ( dev->caps & DEV_CAP_ASSD ) ) {
		status = sd_init_assd( dev );
	}

	return( status );
}

static int sd_capabilities( sdio_dev_t *dev )
{
	int		status;

	status = sd_app_send_scr( dev, dev->raw_scr );
	if( status != EOK ) {
		return( status );
	}

	status = sd_parse_scr( dev, &dev->scr, dev->raw_scr );
	if( status != EOK ) {
		return( status );
	}

	status = sd_read_status( dev );
	if( status != EOK ) {
		return( status );
	}

	status = sd_read_switch( dev );
	if( status != EOK ) {
		return( status );
	}

	dev->erase_size		= sd_erase_grp_size( dev );

#ifdef SDIO_APP_SPEC_FCN_EXT
	sd_parse_extended_fcn( dev );
#endif

	return( status );
}

int sd_lock_unlock( sdio_dev_t *dev, const int action, uint8_t * const pwd, const uint32_t pwd_len )
{
	sdio_hc_t	*hc;
	uint32_t	oflags;
	uint32_t	status;

	hc		= dev->hc;
	oflags	= dev->flags;

	if( pwd_len ) {
		memcpy( &dev->pwd, pwd, pwd_len );
		dev->pwd_len = pwd_len;
	}

	status = _sdio_lock_unlock( dev, action, pwd, pwd_len );

	if( ( dev->flags & DEV_FLAG_LOCKED ) ) {
		if( action == SD_LU_UNLOCK ) {
			status = EACCES;
		}
	}
	else {
		if( ( oflags & DEV_FLAG_LOCKED ) ) {
			status = sd_capabilities( dev );
			if( status != EOK ) {
				return( status );
			}
			status = sd_init_bus( hc );
			if( status != EOK ) {
				return( status );
			}
		}
	}

	return( status );
}

static int sd_unlock( sdio_dev_t * const dev )
{
	sdio_hc_t	*hc;
	int			status;

	hc		= dev->hc;
	status	= EOK;

	if( ( dev->flags & DEV_FLAG_LOCKED ) && dev->pwd_len ) {
		status = _sdio_lock_unlock( dev, SD_LU_UNLOCK, dev->pwd, dev->pwd_len );
		if( status != EOK ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: SD UNLOCK failure", __FUNCTION__ );
		}
	}

	return( status );
}

int sd_init_device( sdio_hc_t *hc, uint32_t ocr, const int flgs )
{
	sdio_dev_t					*dev;
	uint32_t					rocr;
	uint32_t					cid[4];
	int							status;
	sdio_device_errata_t		*errata;
	static sdio_device_errata_t	sd_errata[] = {
			// secusmart devices require extra 50ms after ASSD selection
		{ .pnm = "SD0", .mid = 0x27, .oid = DEV_ERRATA_WILDCARD, .prv_s = 0, .prv_e = DEV_ERRATA_WILDCARD, .erev = DEV_ERRATA_WILDCARD, .errata = 0, .rsettle = 50 },

		{ .pnm = NULL, .mid = 0, .oid = 0, .prv_s = 0, .prv_e = 0, .erev = 0, .errata = 0, .rsettle = 0 } };

	dev			= &hc->device;
	dev->rca	= 0;

	sd_send_if_cond( hc, SD_SIC_VHS_27_36V );
	sdio_go_idle( hc );

		// determine card version (CMD8 was added in 2.0)
	status = sd_send_if_cond( hc, SD_SIC_VHS_27_36V );
	if( status == EOK ) {
#ifdef SDIO_UC_SUPPORT
		ocr |= OCR_HCS | OCR_HO2T;			// 2.0/3.0 card, enable high/ultra capacity
#else
		ocr |= OCR_HCS;						// 2.0 card, enable high capacity
#endif
	}
	else if( status != ETIMEDOUT ) {
		return( EIO );
	}
	else {
		// nothing
	}

		// HC supports 1.8V signalling
	if( !( dev->flags & DEV_FLAG_SIG_ERR ) && HC_CAP_UHS( hc->caps ) && ( hc->caps & HC_CAP_SV_1_8V ) ) {
		if( !flgs || ( flgs && dev->swcaps.bus_mode ) ) {
			ocr |= OCR_S18R;
		}
	}

		// verify HC can supply > 150ma
	if( HC_CAP_XPC( hc->caps ) ) {
		ocr |= OCR_XPC;
	}

	while( 1 ) {
		status = sd_app_send_op_cond( hc, ocr, &rocr );
		if( status != EOK ) {
			return( status );
		}
#ifdef SDIO_UC_SUPPORT
		if( ( rocr & OCR_S18A ) && ( rocr &  ( OCR_HCS | OCR_HO2T ) ) ) {
#else
		if( ( rocr & ( OCR_HCS | OCR_S18A ) ) == ( OCR_HCS | OCR_S18A ) ) {
#endif
			status = sd_voltage_switch( hc );
			if( status != EOK ) {
				ocr &= ~OCR_S18R;
				continue;
			}
			status = sdio_signal_voltage( hc, SIGNAL_VOLTAGE_1_8 );
			if( status != EOK ) {
				dev->flags |= DEV_FLAG_SIG_ERR;
				return( status );
			}
		}
		break;
	}

	status = sdio_all_send_cid( hc, cid );
	if( status != EOK ) {
		return( status );
	}

	if( flgs && memcmp( dev->raw_cid, cid, sizeof( cid ) ) ) {
		return( ENXIO );
	}

	memcpy( dev->raw_cid, cid, sizeof( cid ) );

	status = sdio_send_relative_addr( hc, &dev->rca );
	if( status != EOK ) {
		return( status );
	}

	sdio_bus_mode( hc, BUS_MODE_PUSH_PULL );

	if( !flgs ) {
		status = sd_parse_cid( dev, &dev->cid, dev->raw_cid );
		if( status != EOK ) {
			return( status );
		}

		status = sdio_send_csd( dev, dev->raw_csd );
		if( status != EOK ) {
			return( status );
		}

		status = sd_parse_csd( dev, &dev->csd, dev->raw_csd );
		if( status != EOK ) {
			return( status );
		}

		errata = sdio_device_errata( dev, sd_errata );
		if( errata != NULL ) {
			sdio_reconcile_errata( dev, errata );
			dev->rsettle = errata->rsettle;
		}
	}

	status = sdio_select_card( dev, dev->rca );
	if( status != EOK ) {
		return( status );
	}

	if( ( status = sd_unlock( dev ) ) ) {		// unlock card if needed
		return( status );
	}

	if( ( dev->flags & DEV_FLAG_LOCKED ) ) {	// card locked, wait for unlock
		return( EOK );
	}

	if( !flgs ) {
		status = sd_capabilities( dev );
		if( status != EOK ) {
			return( status );
		}
	}

		// Attempt to detect hardware/drivers that don't support a power cycle.
		// Read switch to determine if the device is still in 1.8V signaling.
	if( ( hc->caps & HC_CAP_SV_1_8V ) && ( hc->signal_voltage != SIGNAL_VOLTAGE_1_8 ) ) {
		status = sd_read_switch( dev );
		if( status == EOK ) {
			if( ( dev->swcaps.bus_mode & SD_BUS_MODE_UHS ) ) {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  Device still in 1.8V signaling", __FUNCTION__ );
				status = sdio_signal_voltage( hc, SIGNAL_VOLTAGE_1_8 );
				if( status != EOK ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s:  sdio_signal_voltage failure", __FUNCTION__ );
				}
			}
		}
	}

	status = sd_init_bus( hc );

	return( status );
}

int sd_bus_error( sdio_dev_t *dev )
{
	sdio_csd_t			*csd;
	sd_switch_cap_t		*swcaps;

	csd		= &dev->csd;
	swcaps	= &dev->swcaps;

	if( ( dev->flags & DEV_FLAG_UHS ) ) {
		dev->flags &= ~DEV_FLAG_UHS;
		swcaps->bus_mode &= ~sd_select_bus_mode( dev, swcaps->bus_mode );
	}
	else if( ( dev->flags & DEV_FLAG_HS ) ) {
		dev->flags	&= ~DEV_FLAG_HS;
		swcaps->dtr_max_hs = max( DTR_MAX_HS26, swcaps->dtr_max_hs - DTR_HS_DEC );
	}
	else {
			//	If low speed card, reduce the speed by 10%
		csd->dtr_max -= ( csd->dtr_max + 9 ) / 10;
		if( csd->dtr_max < DTR_MIN_LS ) {
			csd->dtr_max = DTR_MIN_LS;
		}
	}

	return( sd_init_bus( dev->hc ) );
}

int sd_ident( sdio_hc_t *hc )
{
	sdio_dev_t	*dev;
	uint32_t	ocr;
	int			status;

	dev			= &hc->device;
	dev->rca	= 0;

	sdio_go_idle( hc );
	sd_send_if_cond( hc, SD_SIC_VHS_27_36V );
	status = sd_app_send_op_cond( hc, 0, &ocr );
	if( status == EOK ) {
		status = sdio_select_voltage( hc, ocr );
		if( status == EOK ) {
			status = sd_init_device( hc, dev->ocr, SDIO_FALSE );
			if( status == EOK ) {
				return( status );
			}
		}
	}

	return( status );
}

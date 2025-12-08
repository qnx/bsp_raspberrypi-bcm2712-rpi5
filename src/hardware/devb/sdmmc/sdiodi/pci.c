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

// Module Description:   PCI interface

#include <errno.h>
#include <string.h>

#include <internal.h>

#ifdef SDIO_PCI_SUPPORT

#include <pci/pci.h>
#include <pci/pci_id.h>
#include <pci/pci_ccode.h>
#include <pci/cap_pcie.h>
#include <pci/cap_msi.h>
#include <pci/cap_msix.h>

int sdio_pci_init( void )
{
	return( EOK );
}

int sdio_pci_dinit( void )
{
	return( EOK );
}

int sdio_pci_detach_device( sdio_hc_t *hc )
{
	if( hc->pci.dev_hdl != NULL ) {
		pci_err_t pci_err = pci_device_detach( hc->pci.dev_hdl );
		if( pci_err != PCI_ERR_OK ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2,
				   "%s: Failure to detach device: %s", __FUNCTION__, pci_strerror( pci_err ) );
			return( pci_err );
		}
	}
	return( EOK );
}

static int sdio_pci_enable( sdio_hc_t *hc, int cap, int max_driver_supported_interrupts )
{
	sdio_hc_cfg_t	*cfg;
	sdio_pci_dev_t	*pci;
	int				idx;
	int				max_device_supported_interrupts;
	pci_irq_t		irq[1];
	int_t			nirq = NELEMENTS( irq );
	pci_cmd_t		cmd;
	pci_err_t		pci_err;

	cfg		= &hc->cfg;
    pci		= &hc->pci;
	pci_err = PCI_ERR_OK;

	if( cap == SDIO_PCI_INTR_MSIX ) {
		max_device_supported_interrupts = pci->msix_nirq;

			// set the disposition of all other device interrupt sources to unused
		for( idx = max_driver_supported_interrupts; idx < max_device_supported_interrupts; idx++ ) {
			pci_err = cap_msix_set_irq_entry( pci->dev_hdl, pci->msix, (uint_t)idx, -1 );
			if( pci_err != PCI_ERR_OK ) {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: Disable of MSI-X interrupt entry %u failed: %s", __FUNCTION__, idx, pci_strerror( pci_err ) );
			}
		}
			// now enable the capability
		pci_err = pci_device_cfg_cap_enable( pci->dev_hdl, pci_reqType_e_MANDATORY, pci->msix );
		if( pci_err != PCI_ERR_OK ) {
			free( pci->msix );
			pci->msix = NULL;
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: Unable to enable MSI-X capabilities, %s", __FUNCTION__, pci_strerror( pci_err ) );
			return( ENXIO );
		}
		else {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 2, "%s: MSI-X capabilities enabled", __FUNCTION__ );
				// unmask the interrupts we are interested in
			for( idx = 0; idx < max_driver_supported_interrupts; idx++ ) {
				pci_err = cap_msix_unmask_irq_entry( pci->dev_hdl, pci->msix, (uint_t)idx );
				if( pci_err != PCI_ERR_OK ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: Unmask of MSI-X interrupt entry %d failed: %s", __FUNCTION__, idx, pci_strerror( pci_err ) );
				}
				else {
//					sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: MSI-X interrupt entry %d unmasked", __FUNCTION__ , idx );
				}
			}
			pci->pflags |=  SDIO_PFLG_MSIX;
		}
	}
	else if( cap == SDIO_PCI_INTR_MSI ) {
		max_device_supported_interrupts = pci->msi_nirq;

			// before enabling the MSI capability, configure the desired number of interrupts
		if( max_device_supported_interrupts > max_driver_supported_interrupts ) {
			pci_err = cap_msi_set_nirq( pci->dev_hdl, pci->msi, (uint_t)max_driver_supported_interrupts );
			if( pci_err != PCI_ERR_OK ) {
				free( pci->msi );
				pci->msi = NULL;
				sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: cap_msi_set_nirq failed, %s", __FUNCTION__, pci_strerror( pci_err ) );
			}
		}

		if( pci_err == PCI_ERR_OK ) {
			pci_err = pci_device_cfg_cap_enable( pci->dev_hdl, pci_reqType_e_MANDATORY, pci->msi );
			if( pci_err != PCI_ERR_OK ) {
				free( pci->msi );
				pci->msi = NULL;
				sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: Unable to enable MSI capabilities, %s", __FUNCTION__, pci_strerror( pci_err ) );
			}
			else {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: MSI capabilities enabled", __FUNCTION__ );
				for( idx = 0; idx < max_driver_supported_interrupts; idx++ ) {
					pci_err = cap_msi_unmask_irq_entry( pci->dev_hdl, pci->msi, (uint_t)idx );
					if( pci_err == PCI_ERR_OK ) {
						sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: PVM supported. MSI interrupt entry %u unmasked", __FUNCTION__, idx );
					}
					else if( pci_err != PCI_ERR_ENOTSUP ) {
						sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: Attempt to unmask MSI interrupt entry %u failed, %s", __FUNCTION__, idx, pci_strerror( pci_err ) );
					}
					else {
						// nothing
					}
				}
			}
			pci->pflags |=  SDIO_PFLG_MSI;
		}
	}
	else if( cap == SDIO_PCI_INTR_PIN ) {

	}
	else {
		// nothing
	}

	if( cfg->irqs == 0 ) {
		pci_err = pci_device_read_irq( pci->dev_hdl, &nirq, irq );
		if( pci_err != PCI_ERR_OK ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: pci_device_read_irq failed: %s", __FUNCTION__, pci_strerror( pci_err ) );
			return( ENXIO );
		}

		for( idx = 0; idx < nirq; idx++ ) {
			cfg->irq[idx] = irq[idx];
		}
		cfg->irqs = nirq;
	}
	else {
	}

		// need to ensure bus master is enabled
	sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: B%u:D%u:F%u, Check bus mastering is enabled", __FUNCTION__,
			PCI_BUS( hc->pci.bdf ), PCI_DEV( hc->pci.bdf ), PCI_FUNC( hc->pci.bdf ) );

	pci_err = pci_device_read_cmd( hc->pci.bdf, &cmd );
	if( pci_err != PCI_ERR_OK ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2,
				"%s: pci_device_read_cmd() failed: %s", __FUNCTION__, pci_strerror( pci_err ) );
		return( ENXIO );
	}
	else if( ( cmd & ( 1u << 2 ) ) == 0 ) {
		pci_err = pci_device_write_cmd( hc->pci.dev_hdl, cmd | ( 1u << 2 ), &cmd );
		if( pci_err != PCI_ERR_OK ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2,
					"%s: pci_device_write_cmd() failed: %s", __FUNCTION__, pci_strerror( pci_err ) );
			return( ENXIO );
		}
		else if( ( cmd & ( 1u << 2 ) ) == 0 ) {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_WARNING, hc->cfg.verbosity, 2,
					"%s: pci_device_write_cmd() ok but cmd reg is 0x%x", __FUNCTION__, cmd );
		}
		else {
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: bus mastering is now enabled", __FUNCTION__ );
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: bus mastering already enabled", __FUNCTION__ );
	}

	return( EOK );
}

static int sdio_pci_capabilities( sdio_hc_t *hc )
{
	sdio_pci_dev_t	*pci;
	int				idx;
	pci_err_t		pci_err;

	pci = &hc->pci;

	idx = pci_device_find_capid( pci->bdf, CAPID_PCIe );
	if( idx >= 0 ) {
		pci_err = pci_device_read_cap( pci->bdf, &pci->pcie, (uint_t)idx );
		if( pci_err != PCI_ERR_OK ) {
			pci->pcie = NULL;
			sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s:  Unable to read PCIe capabilities, %s", __FUNCTION__, pci_strerror( pci_err ) );
		}
		else {
			pci_err = pci_device_cfg_cap_enable( pci->dev_hdl, pci_reqType_e_MANDATORY, pci->pcie );
			if( ( pci_err != PCI_ERR_OK ) && ( pci_err != PCI_ERR_EALREADY ) ) {
				sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: Unable to enable PCIe capabilities, %s", __FUNCTION__, pci_strerror( pci_err ) );
			}
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s:  Unable to find PCIe capability", __FUNCTION__ );
	}

		// MSI-X capability
	idx = pci_device_find_capid( pci->bdf, CAPID_MSIX );
	if( idx >= 0 ) {
		pci_err = pci_device_read_cap( pci->bdf, &pci->msix, (uint_t)idx );
		if( pci_err != PCI_ERR_OK ) {
			pci->msix = NULL;
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: Unable to read MSI-X capabilities, %s", __FUNCTION__, pci_strerror( pci_err ) );
		}
		else {
			pci->msix_nirq = cap_msix_get_nirq( pci->msix );
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s:  Unable to find MSI-X capability", __FUNCTION__ );
	}

		// MSI capability
	idx = pci_device_find_capid( pci->bdf, CAPID_MSI );
	if( idx >= 0 ) {
		pci_err = pci_device_read_cap( pci->bdf, &pci->msi, (uint_t)idx );
		if( pci_err != PCI_ERR_OK ) {
			pci->msi = NULL;
			sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: Unable to read MSI capabilities, %s", __FUNCTION__, pci_strerror( pci_err ) );
		}
		else {
			pci->msi_nirq = cap_msi_get_nirq( pci->msi );
		}
	}
	else {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 5, "%s: Unable to find MSI capability", __FUNCTION__ );
	}

	return( EOK );
}

static int sdio_pci_config( sdio_hc_t *hc, sdio_product_t *prod )
{
	sdio_hc_cfg_t	*cfg;
	sdio_pci_dev_t	*pci;
	pci_ba_t		ba[NELEMENTS(cfg->base_addr)];
	int_t			nba = NELEMENTS(ba);
	int_t			nirq = NELEMENTS(cfg->irq);
	pci_err_t		pci_err;
	int				idx;
	int				cap;
	int				status;

	cfg = &hc->cfg;
	pci = &hc->pci;

	sdio_pci_capabilities( hc );

	if( pci->msix_nirq ) {						// msix
		nirq = ( cfg->irqs == 1 ) ? 1 : pci->msix_nirq;
		cap = SDIO_PCI_INTR_MSIX;
	}
	else if( pci->msi_nirq ) {					// msi
		nirq = ( cfg->irqs == 1 ) ? 1 : pci->msi_nirq;
		cap = SDIO_PCI_INTR_MSI;
	}
	else {										// int pin
		nirq = 1;
		cap = SDIO_PCI_INTR_PIN;
	}

	nirq = 1;									// remove if we find a device that supports more than 1 irq

	status = sdio_pci_enable( hc, cap, nirq );
	if( status != EOK ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: sdio_pci_enable() failed: %s",
				__FUNCTION__, strerror( status ) );

		return( status );
	}

	pci_err = pci_device_read_ba( hc->pci.dev_hdl, &nba, ba, pci_reqType_e_UNSPECIFIED );
	if( pci_err != PCI_ERR_OK ) {
		sdio_slogf( _SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 2, "%s: pci_device_read_ba() failed: %s",
				__FUNCTION__, pci_strerror( pci_err ) );
		return( ENXIO );
	}

	for( idx = 0; idx < nba; idx++ ) {			// assign base addresses
		switch( ba[idx].type ) {
			case pci_asType_e_NONE:
				break;

			case pci_asType_e_MEM:
				if( ( ba[idx].attr & pci_asAttr_e_EXPANSION_ROM ) != 0 ) {
					sdio_slogf( _SLOGC_SDIODI, _SLOG_WARNING, hc->cfg.verbosity, 4,
							"%s: Unxpected ROM detected on BA %d.", __FUNCTION__, idx );
					break;
				}
				else {
					cfg->base_addr[cfg->base_addrs] = ba[idx].addr;
					cfg->base_addr_size[cfg->base_addrs] = ba[idx].size;
					cfg->base_addrs++;
				}
				break;

			case pci_asType_e_IO:
				cfg->base_addr[cfg->base_addrs] = ba[idx].addr;
				cfg->base_addr_size[cfg->base_addrs] = ba[idx].size;
				cfg->base_addrs++;
				break;

			default:
				sdio_slogf( _SLOGC_SDIODI, _SLOG_WARNING, hc->cfg.verbosity, 3,
						"%s: Unknown BA type %d.", __FUNCTION__, idx );
				break;
		}
	}

	return( EOK );
}

int sdio_pci_device( sdio_hc_t *hc )
{
	sdio_hc_cfg_t		*cfg;
	sdio_product_t		*prod;

	cfg		= &hc->cfg;

	prod = sdio_hc_lookup( cfg->vid, cfg->did, cfg->class, "" );
	if( prod == NULL ) {
		return( ENODEV );
	}

	hc->pci.bdf = pci_device_find( 0, cfg->vid, cfg->did, PCI_CCODE_ANY );

	if( hc->pci.bdf == PCI_BDF_NONE ) {
		hc->pci.dev_hdl = NULL;
	}
	else {
		hc->pci.dev_hdl = pci_device_attach( hc->pci.bdf, pci_attachFlags_DEFAULT, NULL );
	}

	if( hc->pci.dev_hdl == NULL ) {
		hc->pci.bdf = PCI_BDF_NONE;
		return ENODEV;
	}
	else {
		sdio_pci_config( hc, prod );
		return( EOK );
	}
}

#define PCI_CCODE_PERIPHERAL_SDHC_DMA		(PCI_CCODE( PCI_CCODE_PERIPHERAL, 0x05, 0x01 ))

int sdio_pci_scan( void )
{
	sdio_hc_t					*hc;
	sdio_hc_cfg_t				*cfg;
	sdio_product_t				*prod;
	int							i;
	int							idx;
	int							status;
	pci_vid_t					vid;
	pci_did_t					did;
	pci_ccode_t					ccode;
	pci_bdf_t					bdf;
	pci_err_t					r;
	static const pci_ccode_t	sdhc_ccode[] = { PCI_CCODE_PERIPHERAL_SD_HOST_CTRL, PCI_CCODE_PERIPHERAL_SDHC_DMA };

	hc	= TAILQ_FIRST( &sdio_ctrl.hlist );
	idx	= 0;

	for( i = 0; i < sizeof( sdhc_ccode ) / sizeof( pci_ccode_t ); i++ ) {
		while( ( bdf = pci_device_find( idx, PCI_VID_ANY, PCI_DID_ANY, sdhc_ccode[i] ) ) != PCI_BDF_NONE ) {
			r = pci_device_read_vid( bdf, &vid );
			if( r != PCI_ERR_OK ) {
				continue;
			}

			r = pci_device_read_did( bdf, &did );
			if( r != PCI_ERR_OK ) {
				continue;
			}

			r = pci_device_read_ccode( bdf, &ccode );
			if( r != PCI_ERR_OK ) {
				continue;
			}

			prod = sdio_hc_lookup( vid, did, ccode, "" );
			if( prod != NULL ) {
				if( hc == NULL ) {
					hc = sdio_hc_alloc( );
					if( hc == NULL ) {
						return( ENOMEM );
					}
				}
				cfg				= &hc->cfg;
				cfg->vid		= vid;
				cfg->did		= did;
				cfg->class		= ccode;

				status = sdio_pci_device( hc );
				if( status != EOK ) {
					return( status );
				}
				hc = NULL;
			}
			++idx;
		}
	}

	return( EOK );
}

#endif

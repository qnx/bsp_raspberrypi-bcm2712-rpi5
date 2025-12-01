/*
 * $QNXLicenseC:
 * Copyright 2011, 2021, 2025 BlackBerry Limited.
 * Copyright 2016, Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
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
#include <aarch64/gic_v3.h>
#include <aarch64/gic.h>	/* startup specific */
#include <aarch64/gic_v3.h>
#include <stdbool.h>

/*
 ===============================================================================
 General comments, more cleanup and future directions

 Should use the identification registers to conditionalize some initializations
 and otherwise provide some identifying output

 Consider generalizing the APIs defined in this file so that they are not v3
 specific. Some, or perhaps all, can be applicable to v2 and perhaps v4 and new
 ones can be added for v4 and beyond. Bottom line is that I think we can have
 general gic_*() APIs and have the specific implementations of those APIs based
 on version as required

 ===============================================================================
*/

#define SET_PROPBASER_IN_GICC_INIT

/* forward declarations for internal functions */
static void gic_v3_gicc_init(unsigned cpu);
static void gic_init_version_info(void);
static struct _lpi_tbl_params_s *select_lpi_cfg_table(const unsigned cpu);

/*
 ===============================================================================
 gic_v3_intr_ppi
 gic_v3_intr_spi
 gic_v3_intr_espi

 These are the 'struct startup_intrinfo' structures for PPIs and optionally SPIs and ESPIs
 The 'gic_v3_intr_ppi' structure will be unconditionally added to the system
 when gic_v3_initialize() is called. If no SPI entries have been added with
 calls to gic_v3_spi_add_entry() from the board specific init_intrinfo(), then
 the 'gic_v3_intr_spi' structure will also be added as the default SPI entry
 If no ESPI entries have been added with calls to gic_v3_espi_add_entry() from
 the board specific init_intrinfo() and the GIC has ESPI interrupts, then
 the 'gic_v3_intr_espi' structure will also be added as the default ESPI entry


*/
static struct startup_intrinfo gic_v3_intr_ppi = {
	.vector_base     = _NTO_INTR_CLASS_EXTERNAL,
	.num_vectors     = 32,
	.cascade_vector  = _NTO_INTR_SPARE,
	.cpu_intr_base   = 0,
	.cpu_intr_stride = 0,
	.flags           = 0,
	.id              = { 0, 0, &interrupt_id_gic_v3_ppi_sr },
	.eoi             = { 0, 0, &interrupt_eoi_gic_v3_ppi_sr },
	.mask            = &interrupt_mask_gic_v3_ppi,
	.unmask          = &interrupt_unmask_gic_v3_ppi,
	.config          = &interrupt_config_gic_v3_ppi,
	.patch_data      = NULL,
#ifdef INTR_CONFIG_FLAG_SYS // we can remove this after the new header are propagated
	.local_stride	 = 0x8000,
#endif
};
/* 'gic_v3_intr_spi' will be used to create a default entry if
 * gic_v3_spi_add_entry() has never been called */
static struct startup_intrinfo gic_v3_intr_spi =
{
	.vector_base     = _NTO_INTR_CLASS_EXTERNAL + ARM_GIC_SPI_MIN_IRQ,
	.num_vectors     = 0, /* set up at runtime if required */
	.cascade_vector  = _NTO_INTR_SPARE,
	.cpu_intr_base   = 0,
	.cpu_intr_stride = 0,
	.flags           = 0,
	.id              = { 0, 0, &interrupt_id_gic_v3_spi },
	.eoi             = { 0, 0, &interrupt_eoi_gic_v3_spi_sr },
	.mask            = &interrupt_mask_gic_v3_spi,
	.unmask          = &interrupt_unmask_gic_v3_spi,
	.config          = NULL,
	.patch_data      = NULL,	/* set up at runtime if required */
};

/* 'gic_v3_intr_espi' will be used to create a default entry if
 * gic_v3_espi_add_entry() has never been called */
static struct startup_intrinfo gic_v3_intr_espi =
{
	.vector_base     = _NTO_INTR_CLASS_EXTERNAL + ARM_GIC_ESPI_MIN_IRQ,
	.num_vectors     = 0, /* set up at runtime if required */
	.cascade_vector  = _NTO_INTR_SPARE,
	.cpu_intr_base   = 0,
	.cpu_intr_stride = 0,
	.flags           = 0,
	.id              = { 0, 0, &interrupt_id_gic_v3_espi },
	.eoi             = { 0, 0, &interrupt_eoi_gic_v3_spi_sr }, /* same EOI as SPIs */
	.mask            = &interrupt_mask_gic_v3_espi,
	.unmask          = &interrupt_unmask_gic_v3_espi,
	.config          = NULL,
	.patch_data      = NULL,	/* set up at runtime if required */
};

/*
 ===============================================================================
 lpi_params

 'lpi_params' holds the initialization parameters used by gic_v3_initialize()
 for various LPI settings. A set of APIs exist which can be called prior to
 gic_v3_initialize() in order to modify/customize these parameters

*/
static arm_gic_lpi_params_t lpi_params = ARM_GIC_LPI_PARAMS_DFLT_INIT(lpi_params);

/*
 ===============================================================================
 spi_intrinfo
 espi_intrinfo
 lpi_intrinfo

 These 3 structures are used to record the 'struct startup_intrinfo' entries for
 both SPI/ESPI and LPI interrupts.
 Because it is platform specific how the SPI/ESPI and (optional) LPI interrupt types
 will be used, the gic_v3_spi_add_entry(), gic_v3_espi_add_entry(), and gic_v3_lpi_add_entry()
 can be used to segment these interrupts into differentiated groups (based on the .flags
 field for example) for different purposes
 As entries are added using these API's, they are recorded in these structures
 and then added to the system when gic_v3_initialize() is called

*/
static struct
{
	unsigned num_entries;
	struct
	{
		struct startup_intrinfo *entry;

		/*
		 * the order of fields in this structure must coincide with the patcher(s)
		 * Note that for SPIs, 'gicc_patch' is used for the memory mapped EOI callout
		 * otherwise it is not used. The GICD base vaddr and vector offset fields
		 * are always used by the SPI mask/unmask callouts so that SPI patch data
		 * therefore needs to include all 3 values
		 */
		struct __attribute__((packed,aligned(8)))
		{
			_Uint64t gicc_base_vaddr;
			_Uint64t gicd_base_vaddr;
			_Uint32t vec_off;
		} patch_data;
	} co[16];
} spi_intrinfo =
{
	.num_entries = 0,
	.co =
	{
		[0 ... (NUM_ELTS(spi_intrinfo.co) - 1)] =
		{
			.entry = NULL,
			.patch_data =
			{
				.gicc_base_vaddr = 0,
				.gicd_base_vaddr = 0,
				.vec_off = 0,
			},
		},
	},
};
static struct
{
	unsigned num_entries;
	struct
	{
		struct startup_intrinfo *entry;

		/*
		 * the order of fields in this structure must coincide with the patcher(s)
		 * Note that for ESPIs, 'gicc_patch' is used for the memory mapped EOI callout
		 * otherwise it is not used. The GICD base vaddr and vector offset fields
		 * are always used by the ESPI mask/unmask callouts so that ESPI patch data
		 * therefore needs to include all 3 values
		 */
		struct __attribute__((packed,aligned(8)))
		{
			_Uint64t gicc_base_vaddr;
			_Uint64t gicd_base_vaddr;
			_Uint32t vec_off;
		} patch_data;
	} co[16];
} espi_intrinfo =
{
	.num_entries = 0,
	.co =
	{
		[0 ... (NUM_ELTS(espi_intrinfo.co) - 1)] =
		{
			.entry = NULL,
			.patch_data =
			{
				.gicc_base_vaddr = 0,
				.gicd_base_vaddr = 0,
				.vec_off = 0,
			},
		},
	},
};
static struct
{
	unsigned num_entries;
	struct
	{
		struct startup_intrinfo *entry;
		union
		{
		/* the order of fields in this structure must coincide with the patcher(s)
		 * Note that for LPIs, 'gicc_patch' is used for the memory mapped EOI callout
		 * otherwise it is not used but it needs to be in the patch data in case
		 * The LPI patchers will skip over this first data
		 */
			struct __attribute__((packed,aligned(8)))
			{
				_Uint64t gicc_base_vaddr;
				_Uint64t gits_base_vaddr;
				_Uint64t lpi_cfgtbl_vaddr;
				_Uint64t cmd_q_vaddr;
				_Uint64t cmd_q_lock_vaddr;
				_Uint64t RDbase;	/* must accommodate an address depending on mode */
				_Uint32t RDbase_offset;	/* offset for next redistributor value */
				_Uint32t cmd_q_last_offset;
				_Uint32t num_collections;
				_Uint32t vec_off;	/* LPI vector offset correction for LPI config table access */
			} lpi_its;
			struct __attribute__((packed,aligned(8)))
			{
				_Uint64t gicc_base_vaddr;
				_Uint64t gicr_base_vaddr;
				_Uint64t lpi_cfgtbl_vaddr;
				_Uint32t gicr_shift;
				_Uint32t vec_off;	/* LPI vector offset correction for LPI config table access */
			} lpi_direct;
		} patch_data;
	} co[16];
} lpi_intrinfo =
{
	.num_entries = 0,
	.co =
	{
		[0 ... (NUM_ELTS(lpi_intrinfo.co) - 1)] =
		{
			.entry = NULL,
			.patch_data =
			{
				.lpi_its.gicc_base_vaddr = 0,
				.lpi_its.gits_base_vaddr = 0,
				.lpi_its.lpi_cfgtbl_vaddr = 0,
				.lpi_its.cmd_q_vaddr = 0,
				.lpi_its.cmd_q_lock_vaddr = 0,
				.lpi_its.RDbase = 0,
				.lpi_its.RDbase_offset = 0,
				.lpi_its.cmd_q_last_offset = 0,
				.lpi_its.num_collections = 0,
				.lpi_its.vec_off = 0,
			},
		},
	},
};

/*
 ===============================================================================
 use_memory_mapped_callouts

 This variable is used as a boolean (0 = false, !0 = true) to record whether
 or not the GICC interface will be used in the interrupt callouts

*/
static unsigned use_memory_mapped_callouts = 0;

/*
 ===============================================================================
 gicd_paddr_base
 gicr_paddr_base
 gicc_paddr_base

 GIC component physical base addresses (set with gic_v3_set_paddr()) as well as
 GICR offset shift value

*/
static paddr_t gicd_paddr_base = NULL_PADDR;
static paddr_t gicr_paddr_base = NULL_PADDR;
static paddr_t gicc_paddr_base = NULL_PADDR;
static unsigned gicr_offset_shift_val = 0;
static unsigned gicr_index_limit;

/*
 *===============================================================================
 * arm_gic_id
 *
 * GIC identification information
 *
 */
static arm_gic_id_t arm_gic_id =
{
	.arch_version = {.major = UINT8_MAX, .minor = UINT8_MAX},	/* undefined */
	.implementation.id = UINT8_MAX,
	.implementation.company = 0,
	.implementation.version = {.major = UINT8_MAX, .minor = UINT8_MAX},	/* undefined */
	.implementation.resv = 0,
};

/*
 ===============================================================================
 base_addresses_initialized

 This variable is used as a boolean (0 = false, !0 = true) to record whether
 or not gic_v3_set_paddr() has been called to initialize the base addresses for
 the distributor, redistributors and optionally the ITS. It is used to enforce
 that gic_v3_set_paddr() be the first GIC API called before initializing the GIC
 or modifying any of the initialization parameters

*/
static unsigned base_addresses_initialized = 0;

/**
 * ============================================================================
 * Set GIC base addresses
 *
 * This function MUST be called before any other gic_*() API and should only
 * be called once
 *
 * @param gicd   GIC Distributor base address (required)
 * @param gicr   GIC Re-distributor base address (required)
 * @param gits   GIC ITS 0 base address (optional, pass NULL_PADDR)
 *
 * Set the GIC component physical addresses and add them to the syspage asinfo
 * section. This function will also set up the implementation and architecture
 * version information
 *
 * Note that <gits>, if not NULL_PADDR should be the base address for the first
 * ITS (ie. instance 0). If more than 1 ITS is supported, the base addresses for
 * additional instances can be set with gic_v3_its_set_paddr()
 *
 */
void gic_v3_set_paddr_range(const paddr_t gicd, const paddr_t gicr, size_t gicr_size, const paddr_t gits)
{
	ASSERT(base_addresses_initialized == 0);

	gicd_paddr_base = gicd;
	gicr_paddr_base = gicr;
	/*
	 * Now set the GITS paddr
	 * Don't clobber previous calls to gic_v3_its_set_paddr()
	 */
	if (gits != NULL_PADDR) gic_v3_its_set_paddr(0, gits);

	base_addresses_initialized = 1;	/* don't move this */

	/* initialize the GIC implementation and version information */
	gic_init_version_info();

	/* add syspage asinfo entries for GICD and GICR */
	const unsigned off = as_find(AS_NULL_OFF, "memory", NULL);
	as_add64(gicd_paddr_base, gicd_paddr_base + 0xFFFFu, AS_ATTR_DEV, "gicd", off);

	/*
	 * there are as many GICRs/PEs as there are real processors
	 * Don't assume a 1:1 CPU to PE correlation
	 */
	gicr_offset_shift_val = 17;
	if (ARM_GIC_ARCH_VERSION_IS(gic_get_arch_version()) >= ARM_GIC_ARCH_VERSION_4_0) ++gicr_offset_shift_val;

	const unsigned max_cpu = board_smp_num_cpu();
    if (gicr_size == 0) {
	    gicr_size = (1u << gicr_offset_shift_val) * max_cpu;
        gicr_index_limit = max_cpu;
    } else {
        gicr_index_limit = gicr_size >> gicr_offset_shift_val;
    }
	as_add64(gicr_paddr_base, gicr_paddr_base + gicr_size - 1, AS_ATTR_DEV, "gicr", off);
}

void
gic_v3_set_paddr(paddr_t const gicd, paddr_t const gicr, paddr_t const gits)
{
    gic_v3_set_paddr_range(gicd, gicr, 0, gits);
}

/**
 * ============================================================================
 * Get the GIC version information
 *
 * These 2 functions return the major/minor GIC architecture and implementation
 * versions respectively
 *
 * For convenience, there are macros in aarchh64/gic.h that will allow for easy
 * major/minor version checks. See ARM_GIC_ARCH_VERSION_IS() in aarch64/gic.h
 *
 * 
 *
 */
arm_gic_version_t gic_get_arch_version(void)
{
	ASSERT(base_addresses_initialized != 0);

	return arm_gic_id.arch_version;
}
arm_gic_version_t gic_get_impl_version(void)
{
	ASSERT(base_addresses_initialized != 0);

	return arm_gic_id.implementation.version;
}

/**
 * ============================================================================
 * Get the GIC implementation product id
 *
 * This function returns the ProductID for the GIC implementation
 */
int gic_get_impl_productid(void)
{
	ASSERT(base_addresses_initialized != 0);

	return arm_gic_id.implementation.id;
}

/**
 * ============================================================================
 * Get the full GIC implementation identifier
 *
 * This function returns the full and unique identifier for the GIC
 * implementation
 *
 * @param id_str  (optional) pointer to a location where a pointer to the ID
 *                string can be returned. Ignored if NULL
 *
 * For convenience, there are macros in aarchh64/gic.h that will allow for easy
 * implementation checks. See ARM_GIC_IMPL_ID_IS() in aarch64/gic.h 
 *
 */
arm_gic_id_t gic_get_impl_id(const char **id_str)
{
	ASSERT(base_addresses_initialized != 0);

	if (id_str != NULL)
	{
		/* if the caller wants it, assemble a user friendly ID string. */
		bool use_ae = false;
		static char gic_id_str[] = "GIC-000 rmpn";
		static char gicae_id_str[] = "GIC-000AE rmpn";
		int revision_adjust = 0; /* for mapping implementation specific value to pm */

		switch (GIC_IMPL_ID_IS(arm_gic_id))
		{
			case ARM_GIC_500_r0p0:
			case ARM_GIC_500_r0p1:
			case ARM_GIC_500_r1p0:
			case ARM_GIC_500_r1p1:
			{
				gic_id_str[4] = '5';
				gic_id_str[9] = arm_gic_id.implementation.version.major + '0';
				gic_id_str[11] = arm_gic_id.implementation.version.minor + '0';
				break;
			}
			case ARM_GIC_600_r0p0:
			case ARM_GIC_600_r1p6:
				revision_adjust -= 1;
				/* FALLTHRU */
			case ARM_GIC_600_r0p1:
			case ARM_GIC_600_r0p2:
			case ARM_GIC_600_r0p3:
			case ARM_GIC_600_r1p2:
			case ARM_GIC_600_r1p3:
			case ARM_GIC_600_r1p4:
			{
				revision_adjust += 2;
				gic_id_str[4] = '6';
				gic_id_str[9] = arm_gic_id.implementation.version.major + '0';
				ASSERT(revision_adjust >= 1);
				gic_id_str[11] = (arm_gic_id.implementation.version.minor - revision_adjust) + '0';
				break;
			}
			case ARM_GIC_600AE_r0p0:
				revision_adjust -= 1;
				/* FALLTHRU */
			case ARM_GIC_600AE_r0p1:
			case ARM_GIC_600AE_r0p2:
			case ARM_GIC_600AE_r0p3:
			{
				revision_adjust += 2;
				use_ae = true;
				gicae_id_str[4] = '6';
				gicae_id_str[11] = arm_gic_id.implementation.version.major + '0';
				ASSERT(revision_adjust >= 1);
				gicae_id_str[13] = (arm_gic_id.implementation.version.minor - revision_adjust) + '0';
				break;
			}
			case ARM_GIC_V3_720AE_r0p0:
			case ARM_GIC_V3_720AE_r0p1:
			case ARM_GIC_V3_720AE_r1p0:
			case ARM_GIC_V3_720AE_r2p0:
			case ARM_GIC_V3_720AE_r2p1:
			case ARM_GIC_V4_720AE_r0p0:
			case ARM_GIC_V4_720AE_r0p1:
			case ARM_GIC_V4_720AE_r1p0:
			case ARM_GIC_V4_720AE_r2p0:
			case ARM_GIC_V4_720AE_r2p1:
				use_ae = true;
				gicae_id_str[5] = '2';
				/* FALLTHRU */
			case ARM_GIC_V3_700_r0p0:
			case ARM_GIC_V3_700_r0p1:
			case ARM_GIC_V3_700_r1p0:
			case ARM_GIC_V3_700_r2p0:
			case ARM_GIC_V3_700_r2p1:
			case ARM_GIC_V4_700_r0p0:
			case ARM_GIC_V4_700_r0p1:
			case ARM_GIC_V4_700_r1p0:
			case ARM_GIC_V4_700_r2p0:
			case ARM_GIC_V4_700_r2p1:
			{
				if (use_ae) {
					gicae_id_str[4] = '7';
					gicae_id_str[11] = arm_gic_id.implementation.version.major + '0';
					gicae_id_str[13] = arm_gic_id.implementation.version.minor + '0';
				} else {
					gic_id_str[4] = '7';
					gic_id_str[9] = arm_gic_id.implementation.version.major + '0';
					gic_id_str[11] = arm_gic_id.implementation.version.minor + '0';
				}
				break;
			}

			default:
				gic_id_str[4] = '?';
				gic_id_str[9] = arm_gic_id.implementation.version.major + '0';
				gic_id_str[11] = arm_gic_id.implementation.version.minor + '0';
				break;
		}
		if(use_ae) {
			*id_str = gicae_id_str;
		} else {
			*id_str = gic_id_str;
		}

	}
	ASSERT(sizeof(arm_gic_id) == 8);
	return arm_gic_id;
}

/**
 * ============================================================================
 * Use (optionally or not) the interrupt callouts which require the memory
 * mapped GICC register interface
 *
 * @param gicc  GIC CPU interface base address
 * @param use   boolean (0 = false, !0 = true). This parameter allows the caller
 *              to force the use of the memory mapped callouts (when use != 0)
 *              or to simply provide the GICC address in case they are required
 *              (when use == 0)
 *
 * The default is to NOT use any of the interrupt callouts which require the
 * memory mapped GICC register interface unless they are required as determined
 * in gic_v3_initialize(). If the user is unsure whether or not the memory
 * mapped callouts need to be used, they can call this API with the GICC address
 * and 'use' set to false. If it is determined in gic_v3_initialize() that the
 * memory mapped callouts are required, then the GICC address will already be
 * available. If it is determined in gic_v3_initialize() that the memory mapped
 * callouts are not required, then they will only be used if 'use' if set true
 * when this API is called
 *
 * Bottom line on using this API is
 * - if you are unsure whether memory mapped callouts are required, call this
 *   API with 'gicc' set to the GICC address and 'use' set false
 * - if you are sure, or simply want to use the memory mapped callouts, call
 *   this API with 'gicc' set to the GICC address and 'use' set true
 * - otherwise, don't call this API, its not required
 *
 */
void gic_v3_use_mm_reg_callouts(const paddr_t gicc, const unsigned use)
{
	if (gicc != NULL_PADDR)
	{
		gicc_paddr_base = gicc;
		const unsigned off = as_find(AS_NULL_OFF, "memory", NULL);
		as_add64(gicc_paddr_base, gicc_paddr_base + 0x1fffu, AS_ATTR_DEV, "gicc", off);
	}
	use_memory_mapped_callouts = (use == 0) ? 0 : 1;

	if ((debug_flag > 1) && (use != 0))
	{
		kprintf("Caller forcing use of memory mapped interrupt callouts\n");
	}
}

/**
 * ============================================================================
 * Return the number of SPIs supported by the GIC
 *
 */
unsigned gic_v3_num_spis(void)
{
	ASSERT(base_addresses_initialized != 0);

	const _Uint32t gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);
	const _Uint32t ITLines = ((gicd_typer >> 0) & ARM_GICD_TYPER_ITLN);
	/*
	 * ((ITLines + 1) * 32) - 1 represents the highest SPI INTID.
	 * The first 32 INTIDs are not SPIs, therefore the number of SPIs is
	 * less the above calculation by 32.  INTIDs >= 1020 are reserved so
	 * clip at that value - 1.
	 */
	if(((ITLines + 1U) * 32U) > (ARM_GIC_SPI_MAX_IRQ + 1U))
	{
		return (ARM_GIC_SPI_MAX_IRQ + 1U) - 32U;
	}
	return ITLines * 32U;
}

/**
 * ============================================================================
 * Return the number of ESPIs supported by the GIC
 */
uint32_t gic_v3_num_espis(void)
{
	uint32_t ret = 0;
	ASSERT(base_addresses_initialized != 0U);

	const uint32_t gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);

	if((gicd_typer & GICD_TYPER_ESPI) != 0U) {
		ret = (32U * (((gicd_typer & GICD_TYPER_ESPI_RANGE) >> GICD_TYPER_ESPI_RANGE_S) + 1U));
	}

	return ret;
}

/**
 * ============================================================================
 * Return the number of LPIs supported by the GIC
 *
 */
unsigned gic_v3_num_lpis(void)
{
	ASSERT(base_addresses_initialized != 0);

	const _Uint32t gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);
	unsigned nlpi = (gicd_typer >> 11) & 0x1f;	/* try num_LPIs bits */

	if ((gicd_typer & GICD_TYPER_LPIS) == 0) nlpi = 0;	/* not supported */
	/* if num_LPI bits is 0, get from IDbits */
	else if (nlpi == 0) nlpi = ((_Uint32t)1 << (((gicd_typer >> 19) & 0x1f) + 1)) - ARM_GIC_LPI_MIN_IRQ;
	/* otherwise 'nlpi is actually num_LPI bits so convert to want we want */
	else nlpi = (_Uint32t)1 << (nlpi + 1);

	return nlpi;
}

/**
 * ============================================================================
 * Set GIC SPI/ESPI interrupt trigger mode
 *
 * @param irq   Interrupt vector number.
 * @param edge  !0: edge-triggered, 0: level-triggered
 *
 */
void gic_v3_set_intr_trig_mode(const unsigned irq, const unsigned edge)
{
	const uint32_t num_spis = gic_v3_num_spis();
	const uint32_t num_espis = gic_v3_num_espis();
	ASSERT(base_addresses_initialized != 0);

	if ((irq >= ARM_GIC_SPI_MIN_IRQ) && (irq < (num_spis + ARM_GIC_SPI_MIN_IRQ)))
	{
		const _Uint32t mask = 0x2 << ((irq % 16) * 2);
		const _Uint32t off  = (irq / 16) * 4;

		const _Uint32t cur_val = in32(gicd_paddr_base + ARM_GICD_ICFGRn + off);
		_Uint32t val = cur_val;

		if (edge != 0) val |=  mask;
		else val &= ~mask;

		out32(gicd_paddr_base + ARM_GICD_ICFGRn + off, val);
		wait_for_rwp(gicd_paddr_base + ARM_GICD_CTLR, 0x80000000);

		if (debug_flag > 4)
		{
			kprintf("%s(%u, %s) @ ICFG<%u> [0x%x]: 0x%x -> 0x%x\n", __func__,
					irq, (edge != 0) ? "edge" : "level",
					irq / 16, gicd_paddr_base + ARM_GICD_ICFGRn + off, cur_val, val);
		}
	}
	else if ((irq >= ARM_GIC_ESPI_MIN_IRQ) && (irq < (num_espis + ARM_GIC_ESPI_MIN_IRQ)))
	{
		const _Uint32t mask = 0x2 << ((irq % 16) * 2);
		const _Uint32t off  = ((irq - ARM_GIC_ESPI_MIN_IRQ) / 16) * 4;

		const _Uint32t cur_val = in32(gicd_paddr_base + ARM_GICD_ICFGRnE + off);
		_Uint32t val = cur_val;

		if (edge != 0) val |=  mask;
		else val &= ~mask;

		out32(gicd_paddr_base + ARM_GICD_ICFGRnE + off, val);
		wait_for_rwp(gicd_paddr_base + ARM_GICD_CTLR, 0x80000000);

		if (debug_flag > 4)
		{
			kprintf("%s(%u, %s) @ ICFG<%u>E [0x%x]: 0x%x -> 0x%x\n", __func__,
					irq, (edge != 0) ? "edge" : "level",
					irq / 16, gicd_paddr_base + ARM_GICD_ICFGRnE + off, cur_val, val);
		}
	}
	else
	{
		kprintf("%s(%u, %s) invalid\n", __func__, irq, (edge != 0) ? "edge" : "level");
	}
}

/**
 * ============================================================================
 * Return whether or not LPIs are ITS based or direct
 *
 * we have a couple of TYPER bits to check to make sure LPIs are
 * supported and if so, whether they are direct or ITS based. We will
 * only concern ourselves with physical LPIs for now
 *
 * Returns true (!0) or false(0)
 *
 */
unsigned gic_v3_lpi_uses_its(void)
{
	ASSERT(base_addresses_initialized != 0);

	const _Uint64t gicr_typer = in64(gicr_paddr_base + ARM_GICR_TYPER);
	const unsigned phys_lpis = ((gicr_typer & GICR_TYPER_PLPIS) != 0);
	const unsigned direct_lpis = ((gicr_typer & GICR_TYPER_DIRECTLPI) != 0);
	return ((phys_lpis != 0) && (direct_lpis == 0)) ? 1 : 0;
}

/**
 * ============================================================================
 * Add a struct startup_intrinfo entry for SPIs
 *
 * NOTE: This function must be called before gic_v3_initialize() if you want to
 * configure SPIs in a non-default fashion, for example to assign some of them
 * for use as PCI MSIs
 *
 * @param spi_entry  Pointer to the 'struct startup_intrinfo' entry to add.
 *                   Must be declared static
 *
 * Returns 0 on success or -1 on any error
 *
 * The <spi_entry> passed to this function must have the following fields
 * properly initialized
 *
 *	.vector_base
 *	.num_vectors
 *	.flags
 *
 * Other fields will be established based on the specifics of the GIC hardware.
 * If the fields above are invalid or there are any inconsistencies with any
 * previously added entries, -1 will be returned
 *
 * Enough entries should be added to account for all supported SPIs
 *
 * Any entries which are successfully added and which have the INTR_FLAG_MSI
 * flag set in the .flags field of the entry, will be set for edge triggered
 * operation when gic_v3_initialize() is called
 *
 */
int gic_v3_spi_add_entry(struct startup_intrinfo * const spi_entry)
{
	ASSERT(base_addresses_initialized != 0);
	ASSERT(spi_intrinfo.num_entries < NUM_ELTS(spi_intrinfo.co));
	/* do range checking for normal (v3) */
	ASSERT((spi_entry->vector_base >= ARM_GIC_SPI_MIN_IRQ) && (spi_entry->vector_base <= ARM_GIC_SPI_MAX_IRQ));
	ASSERT((spi_entry->vector_base + spi_entry->num_vectors)  <= (gic_v3_num_spis() + ARM_GIC_SPI_MIN_IRQ));

	int r = 0;
	const unsigned new_start = spi_entry->vector_base;
	const unsigned new_last = new_start + spi_entry->num_vectors - 1;

	if (new_last >= (gic_v3_num_spis() + ARM_GIC_SPI_MIN_IRQ)) r = -1;
	else
	{
		if ((spi_entry->flags & INTR_FLAG_MSI) != 0)
		{
			/* make sure Message based SPIs are supported */
			const _Uint32t gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);
			if ((gicd_typer & GICD_TYPER_MBIS) == 0)
			{
				if (debug_flag > 3) kprintf("Message based SPIs are not supported\n");
				r = -1;
			}
		}
		if (r == 0)
		{
			/*
			 * check for an overlap with existing entries
			 * IMPORTANT
			 * we'll also check to make sure that the entry being added has vectors
			 * numerically larger than those previously added. There is an as yet
			 * to be determined issue if the entries are not added in this way. While
			 * the vectors are all in the syspage and get allocated to the requester
			 * properly, no interrupts will occur for the out of order entries. This
			 * appears to be a procnto problem because the correct bits are getting
			 * masked/unmasked and it is only the order of adding entries, and not
			 * anything to do with the vectors in the entries that makes a difference.
			 * The patched callouts also look fine
			 */
			unsigned i;
			for (i=0; i<spi_intrinfo.num_entries; i++)
			{
				const struct startup_intrinfo * const cur_spi_entry = spi_intrinfo.co[i].entry;
				ASSERT(cur_spi_entry != NULL);
				const unsigned cur_start = cur_spi_entry->vector_base;
				const unsigned cur_last = cur_start + cur_spi_entry->num_vectors - 1;

				if ((new_start >= cur_start) && (new_start <= cur_last))
				{
					if (debug_flag > 3)
					{
						kprintf("start overlap: new_start: %u, cur_start: %u, cur_last: %u\n",
								new_start, cur_start, cur_last);
					}
					break;
				}
				else if ((new_last >= cur_start) && (new_last <= cur_last))
				{
					if (debug_flag > 3)
					{
						kprintf("end overlap: new_last: %u, cur_start: %u, cur_last: %u\n",
								new_last, cur_start, cur_last);
					}
					break;
				}
				/*
				 * check for an out-of-order entry as described above. Note that we
				 * don't necessarily care that the entries are contiguous, just that
				 * they are in increasing order
				 */
				else if (new_start <= cur_last)
				{
					if (debug_flag > 3)
					{
						kprintf("out-of-order: new_last: %u, cur_last: %u\n",
								new_last, cur_last);
					}
					break;
				}
			}
			if (i != spi_intrinfo.num_entries) r = -1;
			else
			{
				spi_entry->cascade_vector = _NTO_INTR_SPARE;
				spi_entry->cpu_intr_base = 0;
				spi_entry->cpu_intr_stride = 0;
				spi_entry->id.genflags = 0;
				spi_entry->id.size = 0;
				spi_entry->id.rtn = &interrupt_id_gic_v3_spi;
				spi_entry->eoi.genflags = 0;
				spi_entry->eoi.size = 0;
				spi_entry->eoi.rtn = &interrupt_eoi_gic_v3_spi_sr;
				spi_entry->mask = &interrupt_mask_gic_v3_spi;
				spi_entry->unmask = &interrupt_unmask_gic_v3_spi;
				/* don't touch the config or local_stride fields. The patch_data will
				 * be established in gic_v3_initialize() */

				spi_intrinfo.co[spi_intrinfo.num_entries].entry = spi_entry;
				++spi_intrinfo.num_entries;
			}
		}
	}
	if (debug_flag > 2)
	{
		kprintf("Add SPI entry %u for vectors %u -> %u [SPI max: %u], %s\n",
				spi_intrinfo.num_entries - ((r == 0) ? 1 : 0),
				spi_entry->vector_base, spi_entry->vector_base + spi_entry->num_vectors - 1U,
				gic_v3_num_spis(), (r == 0) ? "Ok" : "Failed");
	}
	return r;
}

/**
 * ============================================================================
 * Add a struct startup_intrinfo entry for ESPIs
 *
 * NOTE: This function must be called before gic_v3_initialize() if you want to
 * configure ESPIs in a non-default fashion, for example to assign some of them
 * for use as PCI MSIs
 *
 * @param espi_entry  Pointer to the 'struct startup_intrinfo' entry to add.
 *                   Must be declared static
 *
 * Returns 0 on success or -1 on any error
 *
 * The <espi_entry> passed to this function must have the following fields
 * properly initialized
 *
 *	.vector_base
 *	.num_vectors
 *	.flags
 *
 * Other fields will be established based on the specifics of the GIC hardware.
 * If the fields above are invalid or there are any inconsistencies with any
 * previously added entries, -1 will be returned
 *
 * Enough entries should be added to account for all supported ESPIs
 *
 * Any entries which are successfully added and which have the INTR_FLAG_MSI
 * flag set in the .flags field of the entry, will be set for edge triggered
 * operation when gic_v3_initialize() is called
 *
 */
int32_t gic_v3_espi_add_entry(struct startup_intrinfo * const espi_entry)
{
	ASSERT(base_addresses_initialized != 0U);
	ASSERT(espi_intrinfo.num_entries < NUM_ELTS(espi_intrinfo.co));
	/* do range checking for extended (v3.1) */
	ASSERT((espi_entry->vector_base >= ARM_GIC_ESPI_MIN_IRQ) && (espi_entry->vector_base <= ARM_GIC_ESPI_MAX_IRQ));
	ASSERT((espi_entry->vector_base + espi_entry->num_vectors)  <= (gic_v3_num_espis() + ARM_GIC_ESPI_MIN_IRQ));

	int r = 0;
	const unsigned new_start = espi_entry->vector_base;
	const unsigned new_last = new_start + espi_entry->num_vectors - 1U;

	if (gic_v3_num_espis() == 0U)
	{
		r = -1;
	}
	else if (new_last >= (gic_v3_num_espis() + ARM_GIC_ESPI_MIN_IRQ))
	{
		r = -1;
	}
	else
	{
		if ((espi_entry->flags & INTR_FLAG_MSI) != 0U)
		{
			/* make sure Message based SPIs/ESPIs are supported */
			const _Uint32t gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);
			if ((gicd_typer & GICD_TYPER_MBIS) == 0U)
			{
				if (debug_flag > 3)
				{
					kprintf("Message based ESPIs are not supported\n");
				}
				r = -1;
			}
		}
		if (r == 0)
		{
			/*
			 * check for an overlap with existing entries
			 * IMPORTANT
			 * we'll also check to make sure that the entry being added has vectors
			 * numerically larger than those previously added. There is an as yet
			 * to be determined issue if the entries are not added in this way. While
			 * the vectors are all in the syspage and get allocated to the requester
			 * properly, no interrupts will occur for the out of order entries. This
			 * appears to be a procnto problem because the correct bits are getting
			 * masked/unmasked and it is only the order of adding entries, and not
			 * anything to do with the vectors in the entries that makes a difference.
			 * The patched callouts also look fine
			 */
			unsigned i;
			for (i=0; i<espi_intrinfo.num_entries; i++)
			{
				const struct startup_intrinfo * const cur_espi_entry = espi_intrinfo.co[i].entry;
				ASSERT(cur_espi_entry != NULL);
				const unsigned cur_start = cur_espi_entry->vector_base;
				const unsigned cur_last = cur_start + cur_espi_entry->num_vectors - 1U;
				bool break_loop = false;

				if ((new_start >= cur_start) && (new_start <= cur_last))
				{
					if (debug_flag > 3)
					{
						kprintf("start overlap: new_start: %u, cur_start: %u, cur_last: %u\n",
								new_start, cur_start, cur_last);
					}
					break_loop =true;
				}
				else if ((new_last >= cur_start) && (new_last <= cur_last))
				{
					if (debug_flag > 3)
					{
						kprintf("end overlap: new_last: %u, cur_start: %u, cur_last: %u\n",
								new_last, cur_start, cur_last);
					}
					break_loop =true;
				}
				/*
				 * check for an out-of-order entry as described above. Note that we
				 * don't necessarily care that the entries are contiguous, just that
				 * they are in increasing order
				 */
				else if (new_start <= cur_last)
				{
					if (debug_flag > 3)
					{
						kprintf("out-of-order: new_last: %u, cur_last: %u\n",
								new_last, cur_last);
					}
					break_loop =true;
				}
				else
				{
					/* Fallthrough */
				}
				if (break_loop)
				{
					break;
				}
			}
			if (i != espi_intrinfo.num_entries)
			{
				r = -1;
			}
			else
			{
				espi_entry->cascade_vector = _NTO_INTR_SPARE;
				espi_entry->cpu_intr_base = 0;
				espi_entry->cpu_intr_stride = 0;
				espi_entry->id.genflags = 0;
				espi_entry->id.size = 0;
				espi_entry->id.rtn = &interrupt_id_gic_v3_espi;
				espi_entry->eoi.genflags = 0;
				espi_entry->eoi.size = 0;
				espi_entry->eoi.rtn = &interrupt_eoi_gic_v3_spi_sr; /* same EOI as SPIs */
				espi_entry->mask = &interrupt_mask_gic_v3_espi;
				espi_entry->unmask = &interrupt_unmask_gic_v3_espi;
				/* don't touch the config or local_stride fields. The patch_data will
				 * be established in gic_v3_initialize() */

				espi_intrinfo.co[espi_intrinfo.num_entries].entry = espi_entry;
				++espi_intrinfo.num_entries;
			}
		}
	}
	if (debug_flag > 2)
	{
		kprintf("Add ESPI entry %u for vectors %u -> %u [ESPI max: %u], %s\n",
				espi_intrinfo.num_entries - ((r == 0) ? 1U : 0U),
				espi_entry->vector_base, espi_entry->vector_base + espi_entry->num_vectors - 1U,
				gic_v3_num_espis(), (r == 0) ? "Ok" : "Failed");
	}
	return r;
}

/**
 * ============================================================================
 * Add a struct startup_intrinfo entry for LPIs
 *
 * NOTE: This function must be called before gic_v3_initialize() if LPIs are
 * required. Enough entries should be added to account for all supported LPIs
 *
 * @param lpi_entry  Pointer to the 'struct startup_intrinfo' entry to add.
 *                   Must be declared static
 *
 * Returns 0 on success or -1 on any error
 *
 * The <lpi_entry> passed to this function must have the following fields
 * properly initialized
 *
 *	.vector_base
 *	.num_vectors
 *	.flags
 *
 * Other fields will be established based on the specifics of the GIC hardware.
 * If the fields above are invalid or there are any inconsistencies with any
 * previously added entries, -1 will be returned
 *
 */
int gic_v3_lpi_add_entry(struct startup_intrinfo * const lpi_entry)
{
	ASSERT(base_addresses_initialized != 0);
	ASSERT(lpi_intrinfo.num_entries < NUM_ELTS(lpi_intrinfo.co));
	ASSERT(lpi_entry->vector_base >= ARM_GIC_LPI_MIN_IRQ);
	ASSERT((lpi_entry->vector_base+lpi_entry->num_vectors-ARM_GIC_LPI_MIN_IRQ) <= gic_v3_num_lpis());

	int r = 0;
	const unsigned new_start = lpi_entry->vector_base;
	const unsigned new_last = new_start + lpi_entry->num_vectors - 1;

	if (new_last >= (gic_v3_num_lpis() + ARM_GIC_LPI_MIN_IRQ)) r = -1;
	else
	{
		/* check for an overlap with existing entries
		 * IMPORTANT
		 * we'll also check to make sure that the entry being added has vectors
		 * numerically larger than those previously added. There is an as yet
		 * to be determined issue if the entries are not added in this way. While
		 * the vectors are all in the syspage and get allocated to the requester
		 * properly, no interrupts will occur for the out of order entries. This
		 * appears to be a procnto problem because the correct bits are getting
		 * masked/unmasked and it is only the order of adding entries, and not
		 * anything to do with the vectors in the entries that makes a difference.
		 * The patched callouts also look fine
		 */
		unsigned i;
		for (i=0; i<lpi_intrinfo.num_entries; i++)
		{
			const struct startup_intrinfo * const cur_lpi_entry = lpi_intrinfo.co[i].entry;
			ASSERT(cur_lpi_entry != NULL);
			const unsigned cur_start = cur_lpi_entry->vector_base;
			const unsigned cur_last = cur_start + cur_lpi_entry->num_vectors - 1;

			if ((new_start >= cur_start) && (new_start <= cur_last))
			{
				if (debug_flag > 3)
				{
					kprintf("new_start: %u, cur_start: %u, cur_last: %u\n",
							new_start, cur_start, cur_last);
				}
				break;
			}
			else if ((new_last >= cur_start) && (new_start <= cur_last))
			{
				if (debug_flag > 3)
				{
					kprintf("new_last: %u, cur_start: %u, cur_last: %u\n",
							new_last, cur_start, cur_last);
				}
				break;
			}
			else if (new_start <= cur_last)
			{
				if (debug_flag > 3)
				{
					/*
					 * check for an out-of-order entry as described above. Note that we
					 * don't necessarily care that the entries are contiguous, just that
					 * they are in increasing order
					 */
					kprintf("out-of-order: new_last: %u, cur_last: %u\n",
							new_last, cur_last);
				}
				break;
			}
		}
		if (i != lpi_intrinfo.num_entries) r = -1;
		else
		{
			lpi_entry->cascade_vector = _NTO_INTR_SPARE;
			lpi_entry->cpu_intr_base = 0;
			lpi_entry->cpu_intr_stride = 0;
			lpi_entry->id.genflags = 0;
			lpi_entry->id.size = 0;
			lpi_entry->id.rtn = &interrupt_id_gic_v3_lpi;
			lpi_entry->eoi.genflags = 0;
			lpi_entry->eoi.size = 0;
			lpi_entry->eoi.rtn = &interrupt_eoi_gic_v3_lpi_sr;

			if (gic_v3_lpi_uses_its())
			{
				lpi_entry->mask = &interrupt_mask_gic_v3_lpi_its;
				lpi_entry->unmask = &interrupt_unmask_gic_v3_lpi_its;
				if (debug_flag > 2) kprintf("Using ITS based LPI mask/unmask callouts\n");
			}
			else
			{
				lpi_entry->mask = &interrupt_mask_gic_v3_lpi_direct;
				lpi_entry->unmask = &interrupt_unmask_gic_v3_lpi_direct;
				if (debug_flag > 2) kprintf("Using LPI direct mask/unmask callouts\n");
			}
			/* don't touch the config or local_stride fields. The patch_data will
			 * be established in gic_v3_initialize() */

			lpi_intrinfo.co[lpi_intrinfo.num_entries].entry = lpi_entry;
			++lpi_intrinfo.num_entries;

			lpi_params.num_lpis += lpi_entry->num_vectors;
			ASSERT(lpi_params.num_lpis <= gic_v3_num_lpis());
		}
	}
	if (debug_flag > 2)
	{
		kprintf("Add LPI entry %u for vectors %u -> %u, [LPI max: %u] %s\n",
				lpi_intrinfo.num_entries - ((r == 0) ? 1 : 0),
				lpi_entry->vector_base, lpi_entry->vector_base + lpi_entry->num_vectors - 1,
				gic_v3_num_lpis(), (r == 0) ? "Ok" : "Failed");
	}
	return r;
}

/**
 * ============================================================================
 * Set desired parameters for the LPI configuration table cacheability and
 * shareability
 *
 * @param params  pointer to the shareability and cacheability parameters
 * @param mmap_flags  flags to control the callout mapping. Ignored if 0
 *
 */
void gic_v3_set_lpi_cfgtbl_params(const gic_tbl_params_t * const params, const int mmap_flags)
{
	ASSERT(params != NULL);
	ASSERT(params->shareability >= arm_gic_tbl_shareability_first);
	ASSERT(params->shareability <= arm_gic_tbl_shareability_last);
	ASSERT(params->cacheability.inner >= arm_gic_tbl_cacheability_first);
	ASSERT(params->cacheability.inner <= arm_gic_tbl_cacheability_last);
	ASSERT(params->cacheability.outer >= arm_gic_tbl_cacheability_first);
	ASSERT(params->cacheability.outer <= arm_gic_tbl_cacheability_last);

	/* For now all LPI config tables will have the same shareability and cacheablility */
	for (unsigned i=0; i<lpi_params.cfg.num_tbls; i++)
	{
		lpi_params.cfg.tbl_params[i].mem = *params;
		if (mmap_flags != 0) lpi_params.cfg.tbl_params[i].mmap_flags = mmap_flags;
	}
}

/**
 * ============================================================================
 * Set desired parameters for the LPI pending table cacheability and
 * shareability
 *
 * @param params  pointer to the shareability and cacheability parameters
 *
 */
void gic_v3_set_lpi_pendtbl_params(const gic_tbl_params_t * const params)
{
	ASSERT(params != NULL);
	ASSERT(params->shareability >= arm_gic_tbl_shareability_first);
	ASSERT(params->shareability <= arm_gic_tbl_shareability_last);
	ASSERT(params->cacheability.inner >= arm_gic_tbl_cacheability_first);
	ASSERT(params->cacheability.inner <= arm_gic_tbl_cacheability_last);
	ASSERT(params->cacheability.outer >= arm_gic_tbl_cacheability_first);
	ASSERT(params->cacheability.outer <= arm_gic_tbl_cacheability_last);

	/* For now all LPI config tables will have the same shareability and cacheablility */
	for (unsigned i=0; i<lpi_params.pending.num_tbls; i++)
	{
		lpi_params.pending.tbl_params[i].mem = *params;
	}
}

/**
 * ============================================================================
 * Initialize the GIC
 *
 * This function will initialize the GIC based on all parameters which have been
 * established by the other gic_*() APIs
 *
 * As a minimum, gic_v3_set_paddr() must have been called
 *
 */
void gic_v3_initialize(void)
{
	ASSERT(base_addresses_initialized != 0);

	/*
	 * if 'use_memory_mapped_callouts' is not set, make sure that we don't
	 * need them and correct as necessary
	 */
	if ((use_memory_mapped_callouts == 0) && ((aa64_sr_rd64(id_aa64pfr0_el1) & ((_Uint64t)0xF << 24)) == 0))
	{
		kprintf("%s(), setting 'use_memory_mapped_callouts'\n", __func__);
		use_memory_mapped_callouts = 1;
	}
	/*
	 * See comments in gic_v3_use_mm_reg_callouts() for why this ASSERT is here
	 * If you hit this, you either called gic_v3_use_mm_reg_callouts() with a
	 * bogus GICC paddr or you didn't call gic_v3_use_mm_reg_callouts() and you
	 * need to
	 */
	ASSERT((use_memory_mapped_callouts == 0) || (gicc_paddr_base != NULL_PADDR));

	/*
	 * Initialize the GIC cpu map with invalid values
	 * Note that the number of elements in this 'gic_map' is PROCESSORS_MAX
	 * as specified in cpu_init_syspage_memory() and so we should probably be
	 * initializing that many values. But since this field is not an array that
	 * we can use NUM_ELTS() on and a size variable does not exist, we'll use the
	 * real number of processors on this platform as returned by board_smp_num_cpu().
	 * so that at least as many entries are initialized as there are real processors.
	 * Previously the code was using 'lsp.syspage.p->num_cpu' which can be arbitrarily
	 * made smaller with the -P command line option
	 */
	const unsigned max_cpu = board_smp_num_cpu();
	struct aarch64_gic_map_entry * const gic_map = lsp.cpu.aarch64_gic_map.p;
	for (unsigned i=0; i<max_cpu; i++)
	{
		gic_map->gic_cpu[i] = ~0ul;
	}

	/* Disable distributor */
	out32(gicd_paddr_base + ARM_GICD_CTLR, 0);
	wait_for_rwp(gicd_paddr_base + ARM_GICD_CTLR, 0x80000000);

	/* get the number of SPI interrupt lines and calculate the max INTID */
	const _Uint32t gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);
	const unsigned itlines = ((gicd_typer & ARM_GICD_TYPER_ITLN) + 1U);
	const unsigned itn = ((itlines * 32U) > (ARM_GIC_SPI_MAX_IRQ + 1U)) ? (ARM_GIC_SPI_MAX_IRQ + 1U) : (itlines * 32U);

	if (debug_flag > 2) kprintf("GICv3: gicd_typer 0x%x\n", gicd_typer);
	if (debug_flag > 1) kprintf("GICv3: itlines: %u, max INTID: %d\n", itlines, itn - 1);

	/*
	 * Disable all interrupts and clear pending state
	 * SGIs and PPIs depend on the security state. We're currently skipping them
	 * until gic_v3_gicc_init() hence the starting 'reg_idx' of 1
	 * There are 32 INTIDs per register
	 */
	for (unsigned reg_idx=1; reg_idx<itlines; reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint32t);	/* 32 bit register */
		out32(gicd_paddr_base + ARM_GICD_ICENABLERn + reg_offset, 0xFFFFFFFF);
		out32(gicd_paddr_base + ARM_GICD_ICPENDRn + reg_offset, 0xFFFFFFFF);
		/* All interrupts are in Group1 */
		out32(gicd_paddr_base + ARM_GICD_IGROUPRn + reg_offset, 0xFFFFFFFF);
		/* SPI interrupts are in Group1NS */
		out32(gicd_paddr_base + ARM_GICD_IGRPMODRn + reg_offset, 0x00000000);
	}

	/*
	 * Set default priority of all SPI interrupts to 0xA0. For SGIs and PPIs,
	 * GICR_IPRIORITYR is used and will be initialized in gic_v3_gicc_init()
	 * hence the starting 'reg_idx' of 8.
	 */
	for (unsigned reg_idx=8; reg_idx<(8*itlines); reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint32t);	/* 32 bit register */ 
		out32(gicd_paddr_base + ARM_GICD_IPRIORITYn + reg_offset, 0xA0A0A0A0);
	}

	/*
	 * Default all interrupts as level triggered. Modifications will be done as
	 * required for MSIs
	 * SGIs and PPIs depend on the security state. We're currently skipping them
	 * hence the starting 'reg_idx' of 2
	 * There are 16 INTIDs per register
	 */
	for (unsigned reg_idx=2; reg_idx<(itn/16); reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint32t);	/* 32 bit register */
		out32(gicd_paddr_base + ARM_GICD_ICFGRn + reg_offset, 0);
	}
	wait_for_rwp(gicd_paddr_base + ARM_GICD_CTLR, 0x80000000);
	
	/* get the number of ESPI interrupt lines and INTIDs */
	const unsigned espilines = ((gicd_typer & GICD_TYPER_ESPI) == 0) ? 0 : (((gicd_typer & GICD_TYPER_ESPI_RANGE) >> GICD_TYPER_ESPI_RANGE_S) + 1U);
	const unsigned espin = (32U * espilines);

	if ((debug_flag > 1) && (espin > 0U))
	{
		kprintf("GICv3: espilines: %u, max INTID: %d\n", espilines, espin + ARM_GIC_ESPI_MIN_IRQ - 1U);
	}

	/*
	 * Disable all ESPI interrupts and clear pending state
	 * There are 32 INTIDs per register
	 */
	for (unsigned reg_idx=0; reg_idx<espilines; reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint32t);	/* 32 bit register */
		out32(gicd_paddr_base + ARM_GICD_ICENABLERnE + reg_offset, 0xFFFFFFFF);
		out32(gicd_paddr_base + ARM_GICD_ICPENDRnE + reg_offset, 0xFFFFFFFF);
		/* ESPI interrupts are in Group1NS */
		out32(gicd_paddr_base + ARM_GICD_IGRPMODRnE + reg_offset, 0x00000000);
	}

	/*
	 * Set default priority of all ESPI interrupts to 0xA0.
	 */
	for (unsigned reg_idx=0; reg_idx<(8U*espilines); reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint32t);	/* 32 bit register */
		out32(gicd_paddr_base + ARM_GICD_IPRIORITYnE + reg_offset, 0xA0A0A0A0);
	}

	/*
	 * Default all interrupts as level triggered. Modifications will be done as
	 * required for MSIs
	 * There are 16 INTIDs per register
	 */
	for (unsigned reg_idx=0; reg_idx<(espin/16U); reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint32t);	/* 32 bit register */
		out32(gicd_paddr_base + ARM_GICD_ICFGRnE + reg_offset, 0);
	}
	wait_for_rwp(gicd_paddr_base + ARM_GICD_CTLR, 0x80000000);

	/* Enable GICD */
	if (gicd_typer & GICD_TYPER_SecurityExtn)
	{
		/*
		 * GIC implementation supports two security states.
		 * This must be done in two separate writes because ARE_NS being already
		 * set changes whether we should be using ENABLEGRP1A or ENABLEGRP1
		 */
		out32(gicd_paddr_base + ARM_GICD_CTLR, GICD_CTLR_ARE_NS);
		dsb();
		out32(gicd_paddr_base + ARM_GICD_CTLR, GICD_CTLR_ARE_NS | GICD_CTLR_ENABLEGRP1A_NS);
	}
	else
	{
		/*
		 * GIC implementation supports a single security state
		 * GICD_CTLR.DS is RAO/WI in this case
		 */
		out32(gicd_paddr_base + ARM_GICD_CTLR, GICD_CTLR_ARE_1s | GICD_CTLR_ENABLEGRP1_1s);
	}
	wait_for_rwp(gicd_paddr_base + ARM_GICD_CTLR, 0x80000000);

	// Only get the AffX fields from MPIDR, clear Interrupt Routing Mode.
	const _Uint64t affinity = aa64_sr_rd64(mpidr_el1) & 0xff00fffffful;

	/*
	 * Route all SPI INTIDs to the current CPU, assumed to be CPU0
	 * Must be done after ARE is set
	 * There is one register per INTID
	 */
	for (unsigned reg_idx=ARM_GIC_SPI_MIN_IRQ; reg_idx<itn; reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint64t);	/* 64 bit register */
		out64(gicd_paddr_base + ARM_GICD_IROUTERn + reg_offset, affinity);
	}

	/*
	 * Route all ESPI INTIDs to the current CPU, assumed to be CPU0
	 * Must be done after ARE is set
	 * There is one register per INTID
	 */
	for (unsigned reg_idx=0; reg_idx<espin; reg_idx++)
	{
		const unsigned reg_offset = reg_idx * sizeof(_Uint64t);	/* 64 bit register */
		out64(gicd_paddr_base + ARM_GICD_IROUTERnE + reg_offset, affinity);
		dsb();
	}

	/* map memory needed in the callouts */
	const _Uintptrt gicd_vaddr_base = callout_io_map(0x10000, gicd_paddr_base);
	if (debug_flag > 1) kprintf("GICD @ %L mapped to %L\n", gicd_paddr_base, gicd_vaddr_base);

	const size_t gicr_size = (1u << gicr_offset_shift_val) * gicr_index_limit;
	const _Uintptrt gicr_vaddr_base = callout_io_map(gicr_size, gicr_paddr_base);
	if (debug_flag > 1) kprintf("GICR @ %L mapped to %L\n", gicr_paddr_base, gicr_vaddr_base);
	/* IMPORTANT: 'gic_gicr_vaddr' is still used for the IPI callout so need to set it here */
	gic_gicr_vaddr = gicr_vaddr_base;

	_Uintptrt gicc_vaddr_base = 0;
	if (use_memory_mapped_callouts != 0)
	{
		gicc_vaddr_base = callout_io_map(ARM_GICC_SIZE, gicc_paddr_base);
		if (debug_flag > 1) kprintf("GICC @ %L mapped to %L\n", gicc_paddr_base, gicc_vaddr_base);
	}
	gic_cpu_init = gic_v3_gicc_init;

	static struct __attribute__((packed,aligned(8)))
	{
		_Uint64t gicc_base_vaddr;
		_Uint64t gicr_base_vaddr;
		_Uint32t gicr_shift;;
	} ppi_patch_data;

	set_syspage_section(&lsp.cpu.aarch64_gicr_map, (unsigned)(lsp.syspage.p->num_cpu * sizeof(_Uint32t)));

	if (use_memory_mapped_callouts == 0) gic_sendipi = &sendipi_gic_v3_sr;
	else
	{
		if (debug_flag > 2) kprintf("Using memory mapped callouts\n");

		gic_sendipi = &sendipi_gic_v3_mm;
		gic_v3_intr_ppi.id.rtn  = &interrupt_id_gic_v3_ppi_mm;
		gic_v3_intr_ppi.eoi.rtn = &interrupt_eoi_gic_v3_ppi_mm;
		gic_v3_intr_spi.eoi.rtn = &interrupt_eoi_gic_v3_spi_mm;

		ppi_patch_data.gicc_base_vaddr = gicc_vaddr_base;
	}
	ppi_patch_data.gicr_base_vaddr = gicr_vaddr_base;
	ppi_patch_data.gicr_shift = gicr_offset_shift_val;
	gic_v3_intr_ppi.patch_data = (void *)&ppi_patch_data;
	add_interrupt(&gic_v3_intr_ppi);

	/*
	 * The SPI, ESPI, and LPI callouts MUST be added after the PPI callouts as the
	 * id callouts for SPI/ESPI/LPI depend on it
	 */
	if (spi_intrinfo.num_entries == 0)
	{
		gic_v3_intr_spi.num_vectors = gic_v3_num_spis();
		const int r = gic_v3_spi_add_entry(&gic_v3_intr_spi);
		if (debug_flag > 0)
		{
			kprintf("No SPI intrinfo. Add default entry for %u -> %u vectors, %s\n",
					gic_v3_intr_spi.vector_base, gic_v3_intr_spi.vector_base + gic_v3_intr_spi.num_vectors - 1,
					(r == 0) ? "Ok" : "Failed");
		}

	}
	for (unsigned i=0; i<spi_intrinfo.num_entries; i++)
	{
		struct startup_intrinfo * const intrinfo = spi_intrinfo.co[i].entry;
		spi_intrinfo.co[i].patch_data.vec_off = intrinfo->vector_base - ARM_GIC_SPI_MIN_IRQ;
		spi_intrinfo.co[i].patch_data.gicd_base_vaddr = gicd_vaddr_base;

		if (use_memory_mapped_callouts != 0)
		{
			spi_intrinfo.co[i].patch_data.gicc_base_vaddr = gicc_vaddr_base;
			intrinfo->eoi.rtn = &interrupt_eoi_gic_v3_spi_mm;
		}

		intrinfo->patch_data = (void *)&spi_intrinfo.co[i].patch_data;

		/* set SPIs being used as MSIs to edge triggered */
		if ((intrinfo->flags & INTR_FLAG_MSI) != 0)
		{
			const unsigned start = intrinfo->vector_base;
			const unsigned end = start + intrinfo->num_vectors;

			for (unsigned irq=start; irq<end; irq++)
			{
				gic_v3_set_intr_trig_mode(irq, 1);
			}
		}
		add_interrupt(intrinfo);
	}

	const uint32_t num_espis = gic_v3_num_espis();
	if ((espi_intrinfo.num_entries == 0U) && (num_espis > 0U))
	{
		gic_v3_intr_espi.num_vectors = num_espis;
		const int r = gic_v3_espi_add_entry(&gic_v3_intr_espi);
		if (debug_flag > 0)
		{
			kprintf("No ESPI intrinfo. Add default entry for %u -> %u vectors, %s\n",
					gic_v3_intr_espi.vector_base, gic_v3_intr_espi.vector_base + gic_v3_intr_espi.num_vectors - 1U,
					(r == 0) ? "Ok" : "Failed");
		}

	}
	for (unsigned i=0; i<espi_intrinfo.num_entries; i++)
	{
		struct startup_intrinfo * const intrinfo = espi_intrinfo.co[i].entry;
		espi_intrinfo.co[i].patch_data.vec_off = intrinfo->vector_base - ARM_GIC_ESPI_MIN_IRQ;
		espi_intrinfo.co[i].patch_data.gicd_base_vaddr = gicd_vaddr_base;

		if (use_memory_mapped_callouts != 0)
		{
			espi_intrinfo.co[i].patch_data.gicc_base_vaddr = gicc_vaddr_base;
			intrinfo->eoi.rtn = &interrupt_eoi_gic_v3_spi_mm; /* same EOI as SPIs */
		}

		intrinfo->patch_data = (void *)&espi_intrinfo.co[i].patch_data;

		/* set ESPIs being used as MSIs to edge triggered */
		if ((intrinfo->flags & INTR_FLAG_MSI) != 0)
		{
			const unsigned start = intrinfo->vector_base;
			const unsigned end = start + intrinfo->num_vectors;

			for (unsigned irq=start; irq<end; irq++)
			{
				gic_v3_set_intr_trig_mode(irq, 1);
			}
		}
		add_interrupt(intrinfo);
	}

	/* check to see whether LPIs are configured */
	if (lpi_params.num_lpis > 0)
	{
		if (debug_flag > 1) kprintf("GICv3: %d LPI interrupts\n", lpi_params.num_lpis);

		ASSERT(lpi_params.num_lpis <= gic_v3_num_lpis());	/* implementation defined maximum */

		/* only support a single common LPI config for now */
		ASSERT(lpi_params.cfg.num_tbls == 1);
		for (unsigned tbl_num=0; tbl_num<lpi_params.cfg.num_tbls; tbl_num++)
		{
			/* set up a pointer to the appropriate LPI config table parameters */
			struct _lpi_tbl_params_s * const lpi_cfg_tbl_params = &lpi_params.cfg.tbl_params[tbl_num];

			/* allocate as required */
			if (lpi_cfg_tbl_params->paddr == NULL_PADDR)
			{
				/* LPI config table entries are 1 byte so size is 'lpi_params.num_lpis' */
				lpi_cfg_tbl_params->paddr = alloc_ram(NULL_PADDR, lpi_params.num_lpis, ARM_GIC_ESPI_MIN_IRQ);
			}
			/* make sure the address is valid. We're done if its not */
			ASSERT(lpi_cfg_tbl_params->paddr != NULL_PADDR);
			ASSERT((lpi_cfg_tbl_params->paddr & (_Uint64t)0xFFFFFFFFFF000) == lpi_cfg_tbl_params->paddr);

			/* set priority, enabled bit off */
			memset(MAKE_1TO1_PTR(lpi_cfg_tbl_params->paddr), 0xa2, lpi_params.num_lpis);

			DCACHE_FLUSH_FOR_GIC_ITS(MAKE_1TO1_PTR(lpi_cfg_tbl_params->paddr), lpi_params.num_lpis);

			/* map the LPI config table for use in the callouts */
			lpi_cfg_tbl_params->vaddr = (_Uintptrt)callout_memory_map(lpi_params.num_lpis,
							lpi_cfg_tbl_params->paddr, lpi_cfg_tbl_params->mmap_flags);
			if (debug_flag > 0)
			{
				kprintf("LPI config table #%u @ %L, callout vaddr: %L\n",
							tbl_num + 1, lpi_cfg_tbl_params->paddr, lpi_cfg_tbl_params->vaddr);
			}

			/*
			 * Note that in some implementations, the shareability bits are non-modifiable
			 * and if they are 0b00 or 0b11, then cacheability should be set to
			 * non-cacheable so we check whether shareability is settable and adjust
			 * the parameters accordingly
			 */
			unsigned cfg_tbl_share_bits = (in64(gicr_paddr_base + ARM_GICR_PROPBASER) >> 10) & 0x3;
			const unsigned flipped_cur_share_bits = ~cfg_tbl_share_bits & 0x3;
			out64(gicr_paddr_base + ARM_GICR_PROPBASER, (_Uint64t)flipped_cur_share_bits << 10);
			cfg_tbl_share_bits = (in64(gicr_paddr_base + ARM_GICR_PROPBASER) >> 10) & 0x3;
			const unsigned cfg_tbl_share_bits_settable = (cfg_tbl_share_bits == flipped_cur_share_bits);

			if (cfg_tbl_share_bits_settable == 0)
			{
				if (debug_flag > 1) kprintf("PROPBASER shareability bits non-settable, use non-cached attributes\n");

				lpi_cfg_tbl_params->mem.shareability = arm_gic_tbl_shareability_NONE;
				lpi_cfg_tbl_params->mem.cacheability.inner = arm_gic_tbl_cacheability_NONE;
				lpi_cfg_tbl_params->mem.cacheability.outer = arm_gic_tbl_cacheability_OUTER_IS_NORMAL_INNER;
			}

			/*
			 * build the PROPBASER register value starting with the LPI config
			 * table physical address.
			 */
			lpi_cfg_tbl_params->propbaser =  lpi_cfg_tbl_params->paddr;
			/* set inner and outer cacheability */
			lpi_cfg_tbl_params->propbaser |= ((_Uint64t)lpi_cfg_tbl_params->mem.cacheability.inner << 7);
			lpi_cfg_tbl_params->propbaser |= ((_Uint64t)lpi_cfg_tbl_params->mem.cacheability.outer << 56);
			/* set shareability */
			lpi_cfg_tbl_params->propbaser |= ((_Uint64t)lpi_cfg_tbl_params->mem.shareability << 10);

			unsigned bits = 14;	/* minimum when LPIs are supported */
			while (((_Uint64t)1 << bits) < (lpi_params.num_lpis + ARM_GIC_LPI_MIN_IRQ)) {
				++bits;
			}
			lpi_cfg_tbl_params->propbaser |= (bits - 1);

#ifndef SET_PROPBASER_IN_GICC_INIT
			/*
			 * program the PROPBASER value into the redistributor. Here is where
			 * changes will be required in order to support multiple LPI config tables.
			 * Because we are running this code as CPU 0, we program the PE associated
			 * with that CPUs PROPBASER with the LPI table parameters. In gic_v3_gicc_init()
			 * we (currently) use this same value for all other redistributors. The API
			 * that would to be added to allow for multiple LPI config tables will also need
			 * to be able assign the tables to specific redistributors and gic_v3_gicc_init()
			 * updated to use that information to program the other PE redistributors
			 */
			out64(gicr_paddr_base + ARM_GICR_PROPBASER, lpi_cfg_tbl_params->propbaser);

			if (debug_flag > 1) kprintf("Setting PROPBASER to %L for CPU0\n", in64(gicr_paddr_base + ARM_GICR_PROPBASER));
#endif
		}

		const unsigned uses_ITS = gic_v3_lpi_uses_its();

		if (debug_flag > 1) kprintf("Implementation supports %s physical LPIs\n", uses_ITS ? "ITS based" : "Direct");

		const unsigned its_num = 0;	
		if (uses_ITS != 0) gic_v3_its_initialize(its_num);

		for (unsigned i=0; i<lpi_intrinfo.num_entries; i++)
		{
			struct startup_intrinfo * const intrinfo = lpi_intrinfo.co[i].entry;

			if (use_memory_mapped_callouts != 0)
			{
				intrinfo->eoi.rtn = &interrupt_eoi_gic_v3_lpi_mm;
				/* the 'gicc_base_vaddr' field is in the same position for 'lpi_direct' and 'lpi_its' */
				lpi_intrinfo.co[i].patch_data.lpi_direct.gicc_base_vaddr = gicc_vaddr_base;
			}

			if (uses_ITS != 0)
			{
				/* LPI_CFG_XXX - callouts don't handle more than one LPI config table */
				const struct _lpi_tbl_params_s * const lpi_cfg_params = select_lpi_cfg_table(0);

				lpi_intrinfo.co[i].patch_data.lpi_its.gits_base_vaddr = gic_v3_its_base_vaddr(its_num);
				lpi_intrinfo.co[i].patch_data.lpi_its.lpi_cfgtbl_vaddr = lpi_cfg_params->vaddr;
				lpi_intrinfo.co[i].patch_data.lpi_its.cmd_q_vaddr = gic_v3_its_cmd_q_vaddr(its_num);
				lpi_intrinfo.co[i].patch_data.lpi_its.cmd_q_lock_vaddr = gic_v3_its_cmd_q_lock_vaddr(its_num);
				lpi_intrinfo.co[i].patch_data.lpi_its.RDbase = 0;
				lpi_intrinfo.co[i].patch_data.lpi_its.RDbase_offset = 1;
				/*
				 * Note: because all LPI interrupts are current routed to the
				 * same ICID and hence CPU of 0, we'll only handle that one collection
				 * in the LPI mask/unmask callouts just to avoid doing it unnecessarily
				 * for all processors right now. When alternate routing is supported,
				 * we'll need to change this to lsp.syspage.p->num_cpu
				 */
				lpi_intrinfo.co[i].patch_data.lpi_its.num_collections = 1;
				lpi_intrinfo.co[i].patch_data.lpi_its.vec_off = intrinfo->vector_base - ARM_GIC_LPI_MIN_IRQ;
				lpi_intrinfo.co[i].patch_data.lpi_its.cmd_q_last_offset = gic_v3_its_cmd_q_last_slot_offset(its_num);

				intrinfo->patch_data = (void *)&lpi_intrinfo.co[i].patch_data.lpi_its;
			}
			else
			{
				ASSERT(0);	/* this path not tested as no direct LPI implementations yet */

				/* LPI_CFG_XXX - callouts don't handle more than one LPI config table */
				const struct _lpi_tbl_params_s * const lpi_cfg_params = select_lpi_cfg_table(0);

				lpi_intrinfo.co[i].patch_data.lpi_direct.gicr_base_vaddr = gicr_vaddr_base;
				lpi_intrinfo.co[i].patch_data.lpi_direct.lpi_cfgtbl_vaddr = lpi_cfg_params->vaddr;
				lpi_intrinfo.co[i].patch_data.lpi_direct.gicr_shift = gicr_offset_shift_val;
				lpi_intrinfo.co[i].patch_data.lpi_direct.vec_off = intrinfo->vector_base - ARM_GIC_LPI_MIN_IRQ;
				intrinfo->patch_data = (void *)&lpi_intrinfo.co[i].patch_data.lpi_direct;
			}
			add_interrupt(intrinfo);
		}
	}
}


/**
 * ============================================================================
 * Initialize CPU specific parts of the GIC interface
 *
 * @param cpu CPU core index
 *
 * This function is installed into the 'gic_cpu_init' function pointer and will
 * be called at the end of startup just before transferring control to the next
 * bootstrap program
 *
 */
static void gic_v3_gicc_init(const unsigned cpu)
{
	paddr_t gic_core_gicr = gicr_paddr_base;

	/* Get core affinity */
	_Uint64t const mpidr = aa64_sr_rd64(mpidr_el1);
	_Uint64t const cpu_aff = (_Uint32t)((mpidr & 0xFFFFFF) | ((mpidr >> 8) & 0xFF000000));

	unsigned gicr_index = 0;
	do {
		/* Get GICR affinity bits (offset + 4, [32 - 63] bits of TYPER register) */
		_Uint32t const gicr_aff = in32(gic_core_gicr + ARM_GICR_TYPER + 4);
		if (cpu_aff == gicr_aff) {
			break;
		}
		gic_core_gicr += (1 << gicr_offset_shift_val);
		++gicr_index;

		ASSERT(gicr_index < gicr_index_limit);

	} while (1);
    // When there is more than one redistributor region, the top half will indicate it.
	lsp.cpu.aarch64_gicr_map.p->gicr_idx[cpu] = gicr_index;

	/* Make sure ProcessorSleep bit is off in WAKER? */
	_Uint32t gicr_waker = in32(gic_core_gicr + ARM_GICR_WAKER);
	if ((gicr_waker & (0x1 << 1)) != 0)
	{
		kprintf("** CPU %d PE is not awake\n", cpu);
		int loop = 1000;
		while ((gicr_waker & (0x1 << 2)) == 0)
		{
			gicr_waker = in32(gic_core_gicr + ARM_GICR_WAKER);
			ASSERT(--loop > 0);
		}
		out32(gic_core_gicr + ARM_GICR_WAKER, gicr_waker & ~(0x1 << 1));
	}

	/* Configure LPIs */
	if (lpi_params.num_lpis != 0)
	{
		/*
		 * each redistributor will have its own pending table. This will either
		 * be allocated now, or will have been preallocated and specified in
		 * 'lpi_params' (no API to do this yet)
		 */
		/* set up a pointer to the appropriate LPI pending table parameters */
		const unsigned params_idx = (lpi_params.pending.num_tbls > 1) ? cpu : 0;
		ASSERT(params_idx < lpi_params.pending.num_tbls);
		struct _lpi_tbl_params_s * const lpi_pend_tbl_params = &lpi_params.pending.tbl_params[params_idx];
		/* set up the 'pendtbl_paddr' to either point to local storage or the specific pending table parameters */
		paddr_t _pendtbl_paddr = NULL_PADDR;
		paddr_t * const pendtbl_paddr_p = (lpi_params.pending.num_tbls > 1) ? &lpi_pend_tbl_params->paddr : &_pendtbl_paddr;
		size_t dcache_flush_len = 0;

		/* allocate as required */
		if (*pendtbl_paddr_p == NULL_PADDR)
		{
			/*
			 * Unlike the LPI config table, the LPI pending table is sized to
			 * hold all INTIDs including the 8192 non LPI interrupts and each
			 * entry holds the pending status for 8 INTIDs.
			 * Note that the number of INTIDs supported by a redistributor can
			 * be less than that indicated by the distributor
			 */
			const size_t tbl_size = ROUND((ARM_GIC_LPI_MIN_IRQ + lpi_params.num_lpis) / 8, 1024);
			if (debug_flag > 1)
			{
				kprintf("LPI Pend table, num_lpis: %u, min required size: %u, actual size: %u\n",
						lpi_params.num_lpis, (ARM_GIC_LPI_MIN_IRQ + lpi_params.num_lpis) / 8, tbl_size);
			}
			*pendtbl_paddr_p = calloc_ram(tbl_size, 0x10000);	/* 64KB aligned */
			dcache_flush_len = tbl_size;
		}

		if (debug_flag > 1) kprintf("LPI pending table @ %L for CPU%d\n", *pendtbl_paddr_p, cpu);
		/* make sure the address is valid */
		ASSERT(*pendtbl_paddr_p != NULL_PADDR);
		ASSERT((*pendtbl_paddr_p & (_Uint64t)0xFFFFFFFFF0000) == *pendtbl_paddr_p);

		/*
		 * Note that in some implementations, the shareability bits are non-modifiable
		 * and if they are 0b00 or 0b11, then cacheability should be set to
		 * non-cacheable so we check whether shareability is settable and adjust
		 * the parameters accordingly
		 */
		unsigned pending_tbl_share_bits = (in64(gic_core_gicr + ARM_GICR_PENDBASER) >> 10) & 0x3;
		const unsigned flipped_cur_share_bits = ~pending_tbl_share_bits & 0x3;
		out64(gic_core_gicr + ARM_GICR_PENDBASER, (_Uint64t)flipped_cur_share_bits << 10);
		pending_tbl_share_bits = (in64(gic_core_gicr + ARM_GICR_PENDBASER) >> 10) & 0x3;
		const unsigned pending_tbl_share_bits_settable = (pending_tbl_share_bits == flipped_cur_share_bits);

		if (pending_tbl_share_bits_settable == 0)
		{
			if (debug_flag > 1) kprintf("PENDBASER shareability bits non-settable, use non-cached attributes\n");

			lpi_pend_tbl_params->mem.shareability = arm_gic_tbl_shareability_NONE;
			lpi_pend_tbl_params->mem.cacheability.inner = arm_gic_tbl_cacheability_NONE;
			lpi_pend_tbl_params->mem.cacheability.outer = arm_gic_tbl_cacheability_OUTER_IS_NORMAL_INNER;
		}
		/*
		 * build the PENDBASER register value starting with the LPI pending
		 * table physical address.
		 */
		_Uint64t pendbaser =  *pendtbl_paddr_p;
		/* set inner and outer cacheability */
		pendbaser |= ((_Uint64t)lpi_pend_tbl_params->mem.cacheability.inner << 7);
		pendbaser |= ((_Uint64t)lpi_pend_tbl_params->mem.cacheability.outer << 56);
		/* set shareability */
		pendbaser |= ((_Uint64t)lpi_pend_tbl_params->mem.shareability << 10);

		DCACHE_FLUSH_FOR_GIC_ITS(MAKE_1TO1_PTR(pendbaser), dcache_flush_len);
		/* Set the field, but also turn on the PTZ bit since there's nothing pending */
		out64(gic_core_gicr + ARM_GICR_PENDBASER, pendbaser | (1ul << 62));

		if (debug_flag > 1)
		{
			kprintf("Set PENDBASER to %L for CPU%d\n",
						in64(gic_core_gicr + ARM_GICR_PENDBASER), cpu);
		}

#ifndef SET_PROPBASER_IN_GICC_INIT
		if (cpu > 0)
#endif
		{
			const struct _lpi_tbl_params_s * const lpi_cfg_params = select_lpi_cfg_table(cpu);
			out64(gic_core_gicr + ARM_GICR_PROPBASER, lpi_cfg_params->propbaser);

			if (debug_flag > 1)
			{
				kprintf("Set PROPBASER to %L for CPU%d\n",
							in64(gic_core_gicr + ARM_GICR_PROPBASER), cpu);
			}
		}
		/* invalidate */
		const unsigned uses_ITS = gic_v3_lpi_uses_its();
		if (uses_ITS == 0) out64(gic_core_gicr + ARM_GICR_INVALLR, 0);
		else gic_v3_its_mapc(0, cpu, gic_core_gicr);	/* only 1 ITS instance supported */

		/* Enable LPI's */
		out32(gic_core_gicr + ARM_GICR_CTLR, 1);
		kprintf("Enable LPIs in GICR_CTLR @ %P for CPU%d\n", gic_core_gicr + ARM_GICR_CTLR, cpu);
	}

	/* SGI base address */
	gic_core_gicr += ARM_GICR_SGI_BASE_OFFSET;
	if (debug_flag > 1) {
		kprintf("cpu%d: Core GICR SGI address: 0x%x\n", cpu, gic_core_gicr);
	}

	/* SGIs and PPs are in Group1NS (GICD_CTLR.DS = 0) */
	out32(gic_core_gicr + ARM_GICR_IGROUPR0, 0xFFFFFFFF);
	out32(gic_core_gicr + ARM_GICR_IGRPMODR0, 0x00000000);

	/* Set SGI/PPI priority (all 0xA0, except for SGI0 which is 0xC0) */
	out32(gic_core_gicr + ARM_GICR_IPRIORITYR0, 0xA0A0A0C0);
	unsigned i;
	for(i = 1; i < 8; ++i) {
		out32(gic_core_gicr + ARM_GICR_IPRIORITYR0 + (i*4), 0xA0A0A0A0);
	}

	/* Clear/disable all SGI/PPI interrupts */
	out32(gic_core_gicr + ARM_GICR_ICPENDR0, 0xFFFFFFFF);
	out32(gic_core_gicr + ARM_GICR_ICENABLER0, 0xFFFFFFFF);

	/* Enable SGI 0 interrupt (used for IPI's) */
	out32(gic_core_gicr + ARM_GICR_ISENABLER0, 0x0000001);

	wait_for_rwp(gic_core_gicr - ARM_GICR_SGI_BASE_OFFSET + ARM_GICR_CTLR, 0x08);

	/* Initialize CPU GIC interface part */
	struct aarch64_gic_map_entry *gic_map = lsp.cpu.aarch64_gic_map.p;
	if (gic_sendipi == &sendipi_gic_v3_mm)
	{
		/* using memory mapped interface */
        // XXX assumes cpu is the same as gicr_index
		gic_map->gic_cpu[cpu] = ((1u << gicr_offset_shift_val)*cpu)
								+ ARM_GICR_SGI_BASE_OFFSET+ARM_GICR_ISPENDR0;

		/* set priority mask to allow all interrupts */
		out32(gicc_paddr_base + ARM_GICC_PMR, 0xF0);
		/* set binary point value */
		out32(gicc_paddr_base + ARM_GICC_BPR, 0);
		/* enable group 1 interrupts, disable IRQ/FIQ bypass signals */
		out32(gicc_paddr_base + ARM_GICC_CTLR, 0x61);
	}
	else
	{
		/* Create value for ARM_GICC_SGI1R_EL1 register used in "sendipi" callout function */
        _Uint64t const aff0 = cpu_aff & 0xfful;
        _Uint64t const aff1 = (cpu_aff >> 8) & 0xfful;
        _Uint64t const aff2 = (cpu_aff >> 16) & 0xfful;
        _Uint64t const aff3 = (cpu_aff >> 32) & 0xfful;

		_Uint64t icc_sgi1r = 1ul << (aff0 & 0x0ful);
        icc_sgi1r |= (aff1 << 16);
        icc_sgi1r |= (aff2 << 32);
        icc_sgi1r |= (aff3 << 48);

        if (aff0 >= 0x0ful) {
            _Uint64t const ctlr = aa64_sr_rd64(S3_0_C12_C12_4);
            _Uint32t const gicd_typer = in32(gicd_paddr_base + ARM_GICD_TYPER);
            if ((ctlr & (1ul << 18)) && (gicd_typer & (1u << 26))) { // RSS supported
                // Set RS field
                icc_sgi1r |= (aff0 & 0xf0ul) << (44-4);
            }
        }

		gic_map->gic_cpu[cpu] =  icc_sgi1r;

		/* Enable the system register interface (icc_sre_el1) */
		aa64_sr_wr32(S3_0_C12_C12_5, 7);
		isb();

		/* Set priority mask to allow all interrupts and enable cpu interface Set ARM_GICC_PMR = 0x0F */
		aa64_sr_wr32(S3_0_C4_C6_0, 0xf0);

		/* Set binary point value, ARM_GICC_BPR1_EL1 */
		aa64_sr_wr32(S3_0_C12_C12_3, 0);

		/* set EOImode to drop/deactivate, ARM_GICC_CTLR_EL1) */
		aa64_sr_wr32(S3_0_C12_C12_4, 0);

		/* Enable signaling of each interrupt group, ARM_GICC_IGRPEN1_ELx = 1 */
		aa64_sr_wr32(S3_0_C12_C12_7, 1);
	}

	if (debug_flag > 1) {
		kprintf("cpu%d: GICv3 cpu interface 0x%x\n", cpu, gic_map->gic_cpu[cpu]);
	}
}

/**
 * ============================================================================
 * Initialize the 'arm_gic_implementation' and 'arm_gic_arch_version' variables
 *
*/
static void gic_init_version_info(void)
{
	ASSERT(gicd_paddr_base != NULL_PADDR);

	const _Uint32t gicd_iidr = in32(gicd_paddr_base + ARM_GICD_IIDR);
	const _Uint32t gicd_pidr2 = in32(gicd_paddr_base + PIDR2);

	arm_gic_id.arch_version.major = (gicd_pidr2 >> 4) & 0xFu;
	arm_gic_id.arch_version.minor = 0;	/* assume 0 until we determine otherwise */

	arm_gic_id.implementation.company = gicd_iidr & 0xFFFu;

	switch (arm_gic_id.implementation.company)
	{
		case 0x043B:	/* ARM */
		{
			arm_gic_id.implementation.id = (gicd_iidr >> 24) & 0xFFu;
			arm_gic_id.implementation.version.major = (gicd_iidr >> 16) & 0xFu;
			arm_gic_id.implementation.version.minor = (gicd_iidr >> 12) & 0xFu;

			/* try to determine the minor version if possible */
			switch (arm_gic_id.arch_version.major)
			{
				case 1:
					break;
				case 2:
					break;
				case 3:
				{
					/* for arch version 3 minor, when can check the PPInum of the GICR */
					ASSERT(gicr_paddr_base != NULL_PADDR);
					const _Uint64t gicr0_typer = in64(gicr_paddr_base + ARM_GICR_TYPER);
					const unsigned PPInum = (gicr0_typer >> 27) & 0x1Fu;
					if (PPInum > 0) arm_gic_id.arch_version.minor = 1;
					break;
				}
				case 4:
					break;
				default:
					/* unknown GIC version. Better stop now */
					crash("Unknown/Unhandled GIC version %d\n", arm_gic_id.arch_version.major);
					break;	/* shut up compiler */
			}
			break;
		}
		/*
		 * add parsing for other implementations as required. The IIDR and PIDR
		 * information may or may not be the same for other implementations
		 */

		default:
			break;
	}
	if (debug_flag > 0)
	{
		const char *impl_id_str = NULL;
		const uint32_t pidr0 = in32(gicd_paddr_base + PIDR0);
		const uint32_t pidr1 = in32(gicd_paddr_base + PIDR1);
		const uint32_t pidr2 = in32(gicd_paddr_base + PIDR2);
		const uint32_t pidr3 = in32(gicd_paddr_base + PIDR3);
		const uint32_t pidr4 = in32(gicd_paddr_base + PIDR4);
		const uint32_t pidr5 = in32(gicd_paddr_base + PIDR5);

		gic_get_impl_id(&impl_id_str);
		kprintf("%s %s, arch v%u.%u detected\n", (arm_gic_id.implementation.company == 0x43B) ? "ARM" : "Non-ARM",
				impl_id_str, arm_gic_id.arch_version.major, arm_gic_id.arch_version.minor);
		kprintf("gic_v3: IIDR 0x%x PIDR2 0x%x impl 0x%x arch %u.%u\n",
				gicd_iidr, gicd_pidr2, arm_gic_id.implementation.company, arm_gic_id.arch_version.major, arm_gic_id.arch_version.minor);
		kprintf("gic_v3: PIDR 0x%x%x\n",
				(pidr5<<8)|(pidr4<<0), 
				((pidr3<<24)|(pidr2<<16)|(pidr1<<8)|(pidr0<<0)));
	}
}

/**
 * ============================================================================
 * Select the LPI configuration table to use for <cpu>
 *
 * @param cpu CPU core index
 *
 * This function is called from gic_v3_gicc_init() while performing per CPU/PE
 * initializations. Because it is possible to have more than one LPI
 * configuration table, this function will return a pointer to the table
 * parameters for the LPI config table which has been assigned to the
 * redistributor associated with <cpu>
 *
 * It is also called from gic_v3_initialize() when setting up the callout patch
 * data
 *
 * Currently we only support one LPI config table so we return that.
 *
 */
static struct _lpi_tbl_params_s *select_lpi_cfg_table(const unsigned cpu)
{
	return &lpi_params.cfg.tbl_params[0];
}

/*
 * $QNXLicenseC:
 * Copyright 2021,2025 BlackBerry Limited.
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
#ifndef __STARTUP_AARCH64_GIC_H__
#define __STARTUP_AARCH64_GIC_H__

#include <startup.h>

/*
 ===============================================================================
 arm_gic_version_t

 This type is used to encode major/minor version information for both the GIC
 architecture as well as the implementation into a _Uint16t compatible type
 We'll keep both a major and minor to handle the nuances of minor revisions

*/
typedef struct __attribute__((packed,aligned(2)))
{
	/* a UINT8_MAX value is invalid and indicates no information */
	_Uint8t major;
	_Uint8t minor;
} arm_gic_version_t;

/*
 ===============================================================================
 ARM_GIC_ARCH_VERSION_IS

 These macros are provided for easy architecture checks within code and can be
 used as follows

 const arm_gic_version_t arch_ver = gic_get_arch_version();
 if (ARM_GIC_ARCH_VERSION_IS(arch_ver) == ARM_GIC_ARCH_VERSION_3_1)
 {
 }

*/
#define __agav_major_shift	(offsetof(arm_gic_version_t, major) * 8)
#define __agav_minor_shift	(offsetof(arm_gic_version_t, minor) * 8)

static inline _Uint16t _arm_gic_version_is(const arm_gic_version_t version)
{
	return *((_Uint16t *)&version);
}
#define ARM_GIC_ARCH_VERSION_IS(_v_)	_arm_gic_version_is((_v_))
#define ARM_GIC_ARCH_VERSION_1_0	((_Uint16t)(((_Uint8t)1 << __agav_major_shift) | ((_Uint8t)0 << __agav_minor_shift)))
#define ARM_GIC_ARCH_VERSION_2_0	((_Uint16t)(((_Uint8t)2 << __agav_major_shift) | ((_Uint8t)0 << __agav_minor_shift)))
#define ARM_GIC_ARCH_VERSION_3_0	((_Uint16t)(((_Uint8t)3 << __agav_major_shift) | ((_Uint8t)0 << __agav_minor_shift)))
#define ARM_GIC_ARCH_VERSION_3_1	((_Uint16t)(((_Uint8t)3 << __agav_major_shift) | ((_Uint8t)1 << __agav_minor_shift)))
#define ARM_GIC_ARCH_VERSION_4_0	((_Uint16t)(((_Uint8t)4 << __agav_major_shift) | ((_Uint8t)0 << __agav_minor_shift)))

/*
 ===============================================================================
 arm_gic_id_t

 This type is used to encode package identification and version information for
 the GIC implementation into a _Uint64t compatible type.
 It effectively contains the IIDR and PIDR2 information in a form that does not
 require additional masking and shifting and allows for easier differentiation
 of architecture minor versions

*/
typedef struct __attribute__((packed,aligned(8)))
{
	arm_gic_version_t arch_version;
	struct
	{
		arm_gic_version_t version;
		_Uint16t company;
		_Uint8t id;
		_Uint8t resv;
	} implementation;
} arm_gic_id_t;

/*
 ===============================================================================
 GIC_IMPL_ID_IS

 These macros are provided for easy GIC implementation identifier checks within
 code and can be used as follows

 const arm_gic_id_t gic_id = gic_get_id();
 if (gic_id == ARM_GIC_500_r0p1)
 {
 }

*/
#define __gii_archver_shift	(offsetof(arm_gic_id_t, arch_version) * 8)
#define __gii_implver_shift	(offsetof(arm_gic_id_t, implementation.version) * 8)
#define __gii_implco_shift	(offsetof(arm_gic_id_t, implementation.company) * 8)
#define __gii_implid_shift	(offsetof(arm_gic_id_t, implementation.id) * 8)

static inline _Uint64t _gic_impl_id_is(const arm_gic_id_t impl_id)
{
	return *((_Uint64t *)&impl_id);
}
#define GIC_IMPL_ID_IS(_v_)		_gic_impl_id_is((_v_))

/* ARM implementations */
#define ARM_GIC_500_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)0 << __gii_implid_shift) | /* GIC-500 product id is 0 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_500_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)0 << __gii_implid_shift) | /* GIC-500 product id is 0 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_500_r1p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)0 << __gii_implid_shift) | /* GIC-500 product id is 0 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_500_r1p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)0 << __gii_implid_shift) | /* GIC-500 product id is 0 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)

#define ARM_GIC_600_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)3 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r0p2 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)4 << __agav_minor_shift)   /* p2 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r0p3 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)5 << __agav_minor_shift)   /* p3 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r1p2 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)4 << __agav_minor_shift)   /* p2 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r1p3 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)5 << __agav_minor_shift)   /* p3 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r1p4 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)6 << __agav_minor_shift)   /* p4 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600_r1p6 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)2 << __gii_implid_shift) | /* GIC-600 product id is 2 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)7 << __agav_minor_shift)   /* p6 */ \
				) << __gii_implver_shift \
			) \
		)

#define ARM_GIC_600AE_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)3 << __gii_implid_shift) | /* GIC-600AE product id is 3 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600AE_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)3 << __gii_implid_shift) | /* GIC-600AE product id is 3 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)3 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600AE_r0p2 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)3 << __gii_implid_shift) | /* GIC-600AE product id is 3 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)4 << __agav_minor_shift)   /* p2 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_600AE_r0p3 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)3 << __gii_implid_shift) | /* GIC-600AE product id is 3 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)5 << __agav_minor_shift)   /* p3 */ \
				) << __gii_implver_shift \
			) \
		)

#define ARM_GIC_V3_700_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_700_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_700_r1p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_700_r2p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_700_r2p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)

#define ARM_GIC_V3_720AE_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_720AE_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_720AE_r1p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_720AE_r2p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V3_720AE_r2p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_3_0) << __gii_archver_shift) | /* GIC arch version 3 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)

#define ARM_GIC_V4_700_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_700_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_700_r1p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_700_r2p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_700_r2p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)4 << __gii_implid_shift) | /* GIC-700 product id is 4 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)

#define ARM_GIC_V4_720AE_r0p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_720AE_r0p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)0 << __agav_major_shift) | /* r0 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_720AE_r1p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)1 << __agav_major_shift) | /* r1 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_720AE_r2p0 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)0 << __agav_minor_shift)   /* p0 */ \
				) << __gii_implver_shift \
			) \
		)
#define ARM_GIC_V4_720AE_r2p1 \
		( \
			((_Uint64t)(ARM_GIC_ARCH_VERSION_4_0) << __gii_archver_shift) | /* GIC arch version 4 */ \
			((_Uint64t)0x43B << __gii_implco_shift) | /*ARM implementor value */ \
			((_Uint64t)7 << __gii_implid_shift) | /* GIC-720AE product id is 7 */ \
			((_Uint64t) \
				( \
					((_Uint16t)2 << __agav_major_shift) | /* r2 */ \
					((_Uint16t)1 << __agav_minor_shift)   /* p1 */ \
				) << __gii_implver_shift \
			) \
		)


/*
 ===============================================================================
 arm_gic_tbl_shareability_e
 arm_gic_tbl_cacheability_e

 These types define the permissible values for the shareability and cacheability
 of the various GIC tables

*/
typedef enum
{
arm_gic_tbl_shareability_first = 0,

	arm_gic_tbl_shareability_NONE = 0,
	arm_gic_tbl_shareability_INNER = 1,
	arm_gic_tbl_shareability_OUTER = 2,
	arm_gic_tbl_shareability_RESV = 3,	/* treated as arm_gic_tbl_shareability_NONE */

arm_gic_tbl_shareability_end,
arm_gic_tbl_shareability_last = arm_gic_tbl_shareability_end - 1,

} arm_gic_tbl_shareability_e;

typedef enum
{
arm_gic_tbl_cacheability_first = 0,

	arm_gic_tbl_cacheability_INNER_DEVICE_nGnRnE = 0,
	arm_gic_tbl_cacheability_OUTER_IS_NORMAL_INNER = 0,
	arm_gic_tbl_cacheability_NONE = 1,
	arm_gic_tbl_cacheability_RD_ALLOC_WT = 2,
	arm_gic_tbl_cacheability_RD_ALLOC_WB = 3,
	arm_gic_tbl_cacheability_WR_ALLOC_WT = 4,
	arm_gic_tbl_cacheability_WR_ALLOC_WB = 5,
	arm_gic_tbl_cacheability_RDWR_ALLOC_WT = 6,
	arm_gic_tbl_cacheability_RDWR_ALLOC_WB = 7,

arm_gic_tbl_cacheability_end,
arm_gic_tbl_cacheability_last = arm_gic_tbl_cacheability_end - 1,

} arm_gic_tbl_cacheability_e;

/*
 ===============================================================================
 gic_tbl_params_t

 This structure defines the parameters associated with various GIC features

*/
typedef struct
{
	arm_gic_tbl_shareability_e shareability;
	struct
	{
		arm_gic_tbl_cacheability_e inner;
		arm_gic_tbl_cacheability_e outer;
	} cacheability;
} gic_tbl_params_t;

/*
 ===============================================================================
 arm_gic_lpi_params_t

 This structure defines the parameters associated with GIC LPIs

*/
typedef struct
{
	_Uint32t num_lpis;	/* 0 - implementation defined, max 4294967295 */
	/*
	 * Normally there is one LPI configuration table for all redistributors
	 * and an LPI pending table per redistributor. However the spec does not
	 * preclude multiple LPI configuration tables and in theory the attributes
	 * of each of the config and pending tables "could" be different. While
	 * we don't expect this to be the case under normal circumstances, we'll
	 * accommodate it. The default attributes for the config and pending tables
	 * will be held in the 'cfg.dflt_tbl_params' and 'pending.dflt_tbl_params'
	 * fields respectively and the 'cfg.tbl_params' and 'pending.tbl_params'
	 * pointers will be initialized to point to the corresponding one. The
	 * 'num_tbls' fields will be set to 1.
	 * If it is decided to have different attributes for either the config or
	 * the pending tables, then the 'cfg.tbl_params' and/or 'pending.tbl_params'
	 * fields will point to suitably allocated memory to hold as many
	 * 'struct _lpi_tbl_params_s' structures as required and the 'respective
	 * num_tbls' fields updated accordingly.
	 * In this case the 'cfg.dflt_tbl_params' and/or 'pending.dflt_tbl_params'
	 * fields will not be used other than to perhaps initialize the structures
	 * in the allocated memory
	 */
	struct _lpi_tbl_s
	{
		unsigned num_tbls;
		struct _lpi_tbl_params_s
		{
			paddr_t paddr;
			_Uintptrt vaddr;
			_Uint64t propbaser;	/* value of the GIRC PROPBASER register */
			int mmap_flags;
			gic_tbl_params_t mem;
		} dflt_tbl_params;
		struct _lpi_tbl_params_s *tbl_params;
	} cfg;
	struct _lpi_tbl_s pending;

} arm_gic_lpi_params_t;

/* default 'arm_gic_lpi_params_t' initializer */
#define ARM_GIC_LPI_PARAMS_DFLT_INIT(_lpi_) \
{ \
	.num_lpis = 0, \
	.cfg = \
	{ \
		.num_tbls = 1, \
		.dflt_tbl_params = \
		{ \
			.paddr = NULL_PADDR, \
			.vaddr = 0, \
			.propbaser = 0, \
			.mmap_flags = PROT_READ | PROT_WRITE | PROT_NOCACHE, \
			.mem = \
			{ \
				.shareability = arm_gic_tbl_shareability_NONE, \
				.cacheability.inner = arm_gic_tbl_cacheability_NONE, \
				.cacheability.outer = arm_gic_tbl_cacheability_NONE, \
			}, \
		}, \
		.tbl_params = &(_lpi_).cfg.dflt_tbl_params, \
	}, \
	.pending = \
	{ \
		.num_tbls = 1, \
		.dflt_tbl_params = \
		{ \
			.paddr = NULL_PADDR, \
			.vaddr = 0, \
			.propbaser = 0, \
			.mmap_flags = PROT_READ | PROT_WRITE | PROT_NOCACHE, \
			.mem = \
			{ \
				.shareability = arm_gic_tbl_shareability_NONE, \
				.cacheability.inner = arm_gic_tbl_cacheability_NONE, \
				.cacheability.outer = arm_gic_tbl_cacheability_NONE, \
			}, \
		}, \
		.tbl_params = &(_lpi_).pending.dflt_tbl_params, \
	}, \
}

/*
 ===============================================================================
 its_lock_t

 This structure defines the locking mechanism associated with ITS

*/
#define ITS_LOCK_VERSION 1

typedef struct {
	union {
        struct intrspin int_spin;
        uint_t          atomic_spin;
	} lock;
	int version;
} arm_gic_its_lock_t;

/*
 ===============================================================================
 arm_gic_its_params_t

 This structure defines the parameters associated with an individual ITS instance

*/
typedef struct
{
	paddr_t gits_paddr_base;
	_Uintptrt gits_vaddr_base;

	unsigned num_collections;

	struct
	{
		unsigned page_size;
		gic_tbl_params_t params;
	} device_tbl;
	struct
	{
		unsigned page_size;
		gic_tbl_params_t params;
		unsigned num_collections;
	} collection_tbl;
	struct
	{
		unsigned num_4k_pages;
		gic_tbl_params_t params;
		int mmap_flags;
		_Uintptrt vaddr;
		arm_gic_its_lock_t *lock_vaddr;
	} cmd_q;
} arm_gic_its_params_t;

#define ARM_GIC_ITS_PARAMS_DFLT_INIT() \
{ \
	.gits_paddr_base = NULL_PADDR, \
	.gits_vaddr_base = 0, \
	.num_collections = 0, \
	.device_tbl = \
	{ \
		.page_size = 4096, \
		.params = \
		{ \
			.shareability = arm_gic_tbl_shareability_NONE, \
			.cacheability.inner = arm_gic_tbl_cacheability_NONE, \
			.cacheability.outer = arm_gic_tbl_cacheability_NONE, \
		}, \
	}, \
	.collection_tbl = \
	{ \
		.num_collections = 0, \
		.page_size = 4096, \
		.params = \
		{ \
			.shareability = arm_gic_tbl_shareability_NONE, \
			.cacheability.inner = arm_gic_tbl_cacheability_NONE, \
			.cacheability.outer = arm_gic_tbl_cacheability_NONE, \
		}, \
	}, \
	.cmd_q = \
	{ \
		.num_4k_pages = 2, /* default to 256 command queue entries */ \
		.params = \
		{ \
			.shareability = arm_gic_tbl_shareability_NONE, \
			.cacheability.inner = arm_gic_tbl_cacheability_NONE, \
			.cacheability.outer = arm_gic_tbl_cacheability_NONE, \
		}, \
		.mmap_flags = PROT_READ | PROT_WRITE | PROT_NOCACHE, \
		.vaddr = 0, \
		.lock_vaddr = NULL, \
	}, \
}


/* GIC v3 specific APIs */
extern void gic_v3_set_paddr(const paddr_t gicd, const paddr_t gicr, const paddr_t gits);
extern void gic_v3_set_paddr_range(const paddr_t gicd, const paddr_t gicr, size_t gicr_size, const paddr_t gits);
extern void gic_v3_use_mm_reg_callouts(const paddr_t gicc, const unsigned use);
extern int gic_v3_spi_add_entry(struct startup_intrinfo * const spi_entry);
extern int32_t gic_v3_espi_add_entry(struct startup_intrinfo * const espi_entry);
extern int gic_v3_lpi_add_entry(struct startup_intrinfo * const lpi_entry);
extern unsigned gic_v3_num_lpis(void);
extern uint32_t gic_v3_num_espis(void);
extern void gic_v3_set_lpi_cfgtbl_params(const gic_tbl_params_t * const params, const int mmap_flags);
extern void gic_v3_set_lpi_pendtbl_params(const gic_tbl_params_t * const params);
extern void gic_v3_initialize(void);
extern unsigned gic_v3_lpi_uses_its(void);

/* GIC v3 ITS specific APIs */
extern void gic_v3_its_set_paddr(const unsigned its_num, const paddr_t gits);
extern arm_gic_version_t gic_get_arch_version(void);
extern arm_gic_version_t gic_get_impl_version(void);
extern int gic_get_impl_productid(void);
extern arm_gic_id_t gic_get_impl_id(const char **id_str);
extern void gic_v3_its_initialize(const unsigned its_num);
extern _Uintptrt gic_v3_its_base_vaddr(const unsigned its_num);
extern _Uintptrt gic_v3_its_cmd_q_vaddr(const unsigned its_num);
extern _Uintptrt gic_v3_its_cmd_q_lock_vaddr(const unsigned its_num);
extern unsigned gic_v3_its_cmd_q_last_slot_offset(const unsigned its_num);
extern void gic_v3_its_set_cmd_q_num_pages(const unsigned its_num, const unsigned num);
extern void gic_v3_its_set_cmd_q_params(const unsigned its_num, const gic_tbl_params_t * const cmd_q_params, const int mmap_flags);
extern void gic_v3_its_set_ct_num_ids(const unsigned its_num, const unsigned num);
extern void gic_v3_its_set_ct_page_size(const unsigned its_num, const unsigned page_size);
extern void gic_v3_its_set_ct_params(const unsigned its_num, const gic_tbl_params_t * const ct_params);
extern void gic_v3_its_set_dt_page_size(const unsigned its_num, const unsigned page_size);
extern void gic_v3_its_set_dt_params(const unsigned its_num, const gic_tbl_params_t * const dt_params);
extern void gic_v3_its_mapc(const unsigned its_num, const unsigned cpu, const unsigned gicr_base_for_cpu);
extern void gic_v3_dcache_flush_for_its(const void *start, const size_t len);

/**
 * Wait for a GICR/GICD state update to propagate
 * @param	addr	paddr containing the RWP bit
 * @param	rwp_bit	which bit is RWP indicator
 */
static __inline__ void
wait_for_rwp(paddr_t const addr, unsigned const rwp_bit) {
	//XXXX: check for never clearing
	do {
		// Wait for the RWP bit to clear
	} while(in32(addr) & rwp_bit);
}

static __inline__ void __attribute__((__unused__))
out64(_Uintptrt __addr, _Uint64t __data) {
	*(volatile _Uint64t *)__addr = __data;
}


static __inline__ _Uint64t __attribute__((__unused__))
in64(_Uintptrt __addr) {
	return *(volatile _Uint64t *)__addr;
}

#ifndef DCACHE_FLUSH_FOR_GIC_ITS
#define DCACHE_FLUSH_FOR_GIC_ITS(_start_, _len_) \
		if ((_len_) > 0) { \
			gic_v3_dcache_flush_for_its((_start_), (_len_)); \
		} while(0)
#endif

#endif	/* __STARTUP_AARCH64_GIC_H__ */

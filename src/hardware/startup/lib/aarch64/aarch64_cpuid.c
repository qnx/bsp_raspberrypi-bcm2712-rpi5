/*
 * Copyright 2014,2022,2025 BlackBerry Limited.
 * Copyright 2016, Freescale Semiconductor, Inc.
 * Copyright 2017, NXP.
 * Copyright 2018, NVIDIA CORPORATION.
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

struct aarch64_cpuid	*aarch64_cpuid[] = {
	&cpuid_a35,
	&cpuid_a53,
	&cpuid_a55,
	&cpuid_a57,
	&cpuid_a72,
	&cpuid_a720,
	&cpuid_a720ae,
	&cpuid_a75,
	&cpuid_a76,
	&cpuid_a78ae,

	&cpuid_v8_fm,
	&cpuid_v8_aem,
	&cpuid_n1,
	&cpuid_neo_v1,
	&cpuid_neo_v2,
	&cpuid_x2,
	&cpuid_x4,
	&cpuid_x925,
	&cpuid_d15,
	&cpuid_d20,

	0
};

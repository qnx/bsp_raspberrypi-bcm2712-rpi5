/*
 * $QNXLicenseC:
 * Copyright 2014, QNX Software Systems.
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

static char *erra_opts[] = {
		"meltdown",
		"~meltdown",
		"spectrev2",
		"~spectrev2",
		"ssbs",
		"~ssbs",
		NULL
};

static char *xopts[] = {
		"cachesnooped",
		"pagesize",
		NULL
};

int
cpu_handle_common_option(int opt)
{
	char *cp, *value;
	
	char *const ext_opts[] = {
		"sve",
		NULL
	};

	switch(opt) {
	case 'X':
		cp = optarg;
		while(*cp != '\0') {
			switch(getsubopt(&cp, xopts, &value)) {
			case 0:
				cache_snooped = CACHE_FLAG_SNOOPED;
				break;
			case 1:
				__page_size = strtoul( value, 0, 16 );
				if((__page_size != 0x1000) &&  (__page_size != 0x10000)) {
					crash("Unsupported pagesize=%v\n",  __page_size);
				}
				break;
			default:
				break;
			}
		}
		return 1;
	case 'E': ; // erratas
		cp = optarg;
		while(*cp != '\0') {
			switch(getsubopt(&cp, erra_opts, &value)) {
			case 0:
				meltdown_active = 1;
				break;
			case 1:
				meltdown_active = 0;
				break;
			case 2:
				spectre_v2_active = 1;
				break;
			case 3:
				spectre_v2_active = 0;
				break;
			case 4:
				ssbs_active = 1;
				break;
			case 5:
				ssbs_active = 0;
				break;
			default:
				break;
			}
		}
		return 1;
	case 'U':
		pauth_active = 1;
		return 1;
	case 'u':
		if ( strcmp("arg", optarg) == 0 ) {
			fdt_boot_option = FDT_BOOTOPT_ADDRPASS_BY_ARGS;
		}
		else if ( strcmp("reg", optarg) == 0 ) {
			fdt_boot_option = FDT_BOOTOPT_ADDRPASS_BY_REG;
		}
		else {
			/* if no option string is specified, set default boot option */
			fdt_boot_option = FDT_BOOTOPT_ADDRPASS_BY_REG;
		}
		fdt_init_bootopt();
		return 1;
	case 'Q':
		if (strcmp(optarg, "enable") == 0) {
			hypervisor_set_options(HYP_FLAG_ENABLED);
		} else if (strcmp(optarg, "disable") == 0) {
			hypervisor_set_options(HYP_FLAG_DISABLED);
		} else if (strcmp(optarg, "enable,el1-host") == 0) {
			hypervisor_set_options(HYP_FLAG_ENABLED | HYP_FLAG_EL1_HOST);
		} else if (strcmp(optarg, "enable,el2-host") == 0) {
			hypervisor_set_options(HYP_FLAG_ENABLED | HYP_FLAG_EL2_HOST);
		} else {
			hypervisor_set_options(HYP_FLAG_INVALID);
		}
		return 1;
	case 'e': // cpu extensions
		cp = optarg;
		while(*cp != '\0') {
			switch(getsubopt(&cp, ext_opts, &value)) {
			case 0:
				sve_requested = 1;
				break;
			default:
				break;
			}
		}
		return 1;
	default:
		return 0;
	}

}

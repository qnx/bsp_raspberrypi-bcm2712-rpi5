/*
 * $QNXLicenseC:
 * Copyright 2017, QNX Software Systems.
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
#include "libfdt.h"

/**
 * Go through a string and find the next space.
 * Optionally NUL-terminating the previous segment.
 * @param   str         string to look at
 * @param   end         where to store a pointer to the next segment
 * @param   terminate   whether or not to NUL-terminate the previous segment
 * @return  a pointer to the first non-space char in the previous segment.
 */
static char *
find_next_arg(char *str, char ** const end, unsigned terminate)
{
    while (*str == ' ') {
        ++str;
    }
    if (*str == '\0') {
        *end = str;
        return str;
    }

    char *next = str;
    while ((*next != ' ') && (*next != '\0')) {
        ++next;
    }

    if (*next != '\0') {
        if (terminate) {
            *next = '\0';
        }
        ++next;
    }
    *end = next;
    return str;
}

/**
 * Tweak argument structure before it is processed.
 * By default, append arguments from the multiboot command line.
 * @param   bap     argument and environment list
 * @param   name    string associated to the list
 * @param	fdt_paddr paddr for FDT data
 */
void
fdt_tweak_cmdline(struct bootargs_entry * const bap, const char * const name, paddr_t const fdt_paddr)
{
    static char *next_arg = NULL;

    if (next_arg == NULL) {
		if (fdt == NULL) {
			fdt_init(fdt_paddr);
			if (fdt == NULL) {
				return;
			}
		}
		int const chosen = fdt_path_offset(fdt, "/chosen");
		if (chosen < 0) {
			return;
		}
		int len;
		const struct fdt_property *const prop = fdt_get_property(fdt, chosen, "bootargs", &len);
		if (prop == NULL) {
			return;
		}
        next_arg = ws_alloc(len);
		if (next_arg == NULL) {
			return;
		}
		memcpy(next_arg, prop->data, len);
    }

    for (;;) {
        char * const cmdline = find_next_arg(next_arg, &next_arg, 1);
        if (*cmdline == '\0') return;
        if (!strcmp(cmdline, "--")) break;
        bootstrap_arg_adjust(bap, NULL, cmdline);
    }
}

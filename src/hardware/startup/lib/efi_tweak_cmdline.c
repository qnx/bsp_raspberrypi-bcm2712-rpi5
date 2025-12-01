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
#include <hw/uefi.h>

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
 * By default, append arguments from the EFI command line.
 * @param   bap     argument and environment list
 */

void
efi_tweak_cmdline(struct bootargs_entry * const bap)
{
    static char *next_arg = NULL;

    if (next_arg == NULL) {
        next_arg = efi_convert_command_line();
		if (next_arg == NULL) {
            return;
		}
        // Skip the first argument if it doesn't start with a '-'
        // Firmware puts the name of the image as the first argument
        char *str;
        next_arg = find_next_arg(next_arg, &str, 0);
        if (*next_arg != '-') {
            next_arg = str;
        }
    }

    for (;;) {
        char * const cmdline = find_next_arg(next_arg, &next_arg, 1);
        if (*cmdline == '\0') return;
        if (!strcmp(cmdline, "--")) break;
        bootstrap_arg_adjust(bap, NULL, cmdline);
    }
}

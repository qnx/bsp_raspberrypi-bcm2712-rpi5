/*
 * $QNXLicenseC:
 * Copyright 2023, QNX Software Systems.
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

unsigned
as_default()
{
    // On aarch64, an id register indicates the number of physical address bits
    // that the CPU can decode. We set "end" here accordingly. This is
    // important, for example, to ensure PCI address assignments and hypervisor
    // IPAs stay within that range.
    static unsigned const  bits[] = { 32, 36, 40, 42, 44, 48, 48, 48 };

    paddr_t const  end = (1ul << bits[aa64_sr_rd64(id_aa64mmfr0_el1) & 0x7ul]) - 1;

    return as_add(0, end, AS_ATTR_NONE, "memory", AS_NULL_OFF);
}

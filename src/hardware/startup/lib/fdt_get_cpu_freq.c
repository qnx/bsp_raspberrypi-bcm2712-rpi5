/*
 * $QNXLicenseC:
 * Copyright 2017, 2025 BlackBerry Limited.
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
#include <libfdt.h>

/*
 * Return CPU frequency found in Flattened Device Tree, if available.
 * Return value of 0 indicates an error occurred, since 0 is not a possible/valid cpu freq value.
*/
unsigned long
fdt_get_cpu_freq(void)
{
    int  len;
    void *f = fdt;

    if (fdt == NULL) {
        kprintf("FDT not found!\n");
        return 0;
    }

    int const node = fdt_node_offset_by_prop_value(fdt, -1, "device_type", "cpu", sizeof "cpu");
    if (node <= 0) {
        if (debug_flag > 0) {
            kprintf("FDT node 'cpu' not found!\n");
        }
        return 0;
    }

    const struct fdt_property * prop = fdt_get_property(f, node, "clock-frequency", &len);
    if (prop == NULL) {
        prop = fdt_get_property(f, node, "cpu-frequency", &len);
    }

    if (prop == NULL) {
        if (debug_flag > 0) {
            kprintf("no clock-frequency or cpu-frequency property in FDT!\n");
        }
        return 0;
    }

    if (len < sizeof(fdt32_t)) {
        if (debug_flag > 0) {
            kprintf("no valid cpu frequency property in FDT!\n");
        }
        return 0;
    }

    const fdt32_t *cnt = (const fdt32_t *)prop->data;

    return (unsigned long) fdt32_to_cpu(cnt[0]);
}

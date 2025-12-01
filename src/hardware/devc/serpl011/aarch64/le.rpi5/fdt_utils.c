/*
 * Copyright 2025, BlackBerry Limited.
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
#include "externs.h"
#include <sys/asinfo.h>
#include <libfdt.h>
#include "fdt_utils.h"

/*
 * FDT "/chosen" has:
 *     stdout-path = "serial10:115200n8" for default debug console
 * FDT "/aliases" has:
 *     serial10 = "/soc/serial@7d001000"
 * FDT node "serial@7d001000" has:
 *   "interrupts = <0x00 0x79 0x04>" for revision 1.0 targets
 *   "interrupts = <0x00 0x78 0x04>" for revision 1.1 targets
 */

struct fdt_info
{
    void *base;
    uint64_t size;
};

static int32_t map_fdt(struct asinfo_entry const * const as, char const * const name, void * const arg)
{
    int32_t ret = 0;
    struct fdt_info * const fdt = (struct fdt_info *) arg;

    (void)name;

    fdt->size = (as->end - as->start + 1UL);
    fdt->base = mmap64(NULL, (size_t)fdt->size, PROT_READ, MAP_SHARED | MAP_PHYS, NOFD, (off_t)as->start);
    if (fdt->base == (void *)MAP_FAILED) {
        fdt->base = NULL;
        ret = 1;
    }
    else {
        if (fdt_check_header(fdt->base) != 0) {
            (void)munmap(fdt->base, (size_t)fdt->size);
            fdt->base = NULL;
            (void)slogf (_SLOG_SETCODE (_SLOGC_CHAR, 0), _SLOG_ERROR, "FDT header check failed");
            ret = 1;
        }
    }
    return ret;
}

/*
 * Retrieve debug console interrupt number from FDT at runtime
 */
int32_t fdt_debug_console_irq_fix(DEV_PL011 *dev)
{
    struct fdt_info  fdt = { .base = NULL, .size = 0UL };
    int32_t ret = 0;

    // Map the FDT.
    if (walk_asinfo("fdt", map_fdt, (void *)&fdt) != 0) {
        ret = -1;
    }
    else {
        if (fdt.base == NULL) {
            ret = -2;
        }
        else {
            const int32_t chosen_offset = fdt_path_offset(fdt.base, "/chosen");
            if (chosen_offset < 0) {
                ret = -3;
            }
            else {
                const char *stdout_path_str = fdt_getprop(fdt.base, chosen_offset, "stdout-path", NULL);
                if (stdout_path_str == NULL) {
                    ret = -4;
                }
                else {
                    int32_t namelen = 0;
                    int32_t node_offset = 0;
                    int32_t prop_len = 0;
                    uint32_t irq = 0;
                    /* find default console serial from "stdout-path = "serial10:115200n8" */
                    const char *p = strchr(stdout_path_str, ':');
                    namelen = (p != NULL) ? (int32_t)(p - stdout_path_str) : (int32_t)strlen(stdout_path_str);
                    node_offset = fdt_path_offset_namelen(fdt.base, stdout_path_str, namelen);
                    if (node_offset < 0) {
                        ret = -5;
                    }
                    else {
                        struct fdt_property const * const fdt_prop = fdt_get_property(fdt.base, node_offset, "interrupts", &prop_len);
                        if (fdt_prop == NULL) {
                            ret = -6;
                        }
                        else {
                            if (prop_len != (int32_t)(3U * sizeof(fdt32_t))) {
                                ret = -7;
                            }
                            else {
                                // Extract the second cell of interrupts.
                                const fdt32_t * const data = (fdt32_t const *)fdt_prop->data;
                                irq = fdt32_to_cpu(data[1]) + 32U;  /* FDT does not include interrupt 0~31 */
                                if (dev->intr != irq) {
                                    (void)slogf (_SLOG_SETCODE (_SLOGC_CHAR, 0), _SLOG_NOTICE, "Override Serial console IRQ from %u to %u", dev->intr, irq);
                                    dev->intr = irq;
                                }
                            }
                        }
                    }
                }
            }
            (void)munmap(fdt.base, fdt.size);
        }
    }
    return ret;
}

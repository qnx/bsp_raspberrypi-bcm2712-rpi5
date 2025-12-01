/*
 * Copyright (c) 2020,2022,2024-2025, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

#include <startup.h>
#include "mbox.h"
#include "bcm2712_startup.h"

#define BCM2712_DRAM_BASE      (0UL)
#define BCM2712_DRAM_STUB_SIZE KILO(512UL)
#define BCM2712_DRAM_BASE_1    (0x100000000)
#define BCM2712_DRAM_BASE_2    (0x200000000)

/* |----------------| <-0x400000000
 * |      SDRAM     |
 * |   (for ARM)    |
 * |----------------| <-0x100000000
 * | ARM local      |
 * | peripherals    |
 * |----------------| <-0x0ff800000
 * |Main peripherals|
 * |----------------| <-0x0fc000000
 * |  SDRAM(for ARM)|
 * |----------------| <-0x040000000
 * |  SDRAM(for VC) |
 * |----------------|
 * | SDRAM(for ARM) |
 * |----------------| <-0x000000000
 */
void bcm2712_init_raminfo(void) {
    const uint32_t vcram_size = mbox_get_vc_memory();
    const paddr_t vcram_start = (paddr_t)(GIG(1UL) - vcram_size);
    const uint32_t rev = get_board_revision();
    uint64_t ramsize = 0UL;

    /* Avoid touching the stub in startup */
    avoid_ram(BCM2712_DRAM_BASE, BCM2712_DRAM_STUB_SIZE);
    /* Avoid touching the VideoCore memory in startup */
    avoid_ram(vcram_start, vcram_size);

    ramsize = MEG(get_memory_size(rev));
    if (ramsize == 0UL) {
        ramsize = MEG(256UL);
        kprintf("Warning: Failed to detect RAM size. Defaulting to %u MB.\n",
            ramsize);
    }

    if (ramsize >= GIG(4UL)) {
        add_ram(BCM2712_DRAM_BASE, GIG(4UL) - MEG(64UL)); // periphs are mapped to 0xfc000000
        if (ramsize >= GIG(8UL)) {
            add_ram(BCM2712_DRAM_BASE_1, GIG(4UL));    // for rpi5 8GB target
        }
        if (ramsize >= GIG(16UL)) {
            add_ram(BCM2712_DRAM_BASE_2, GIG(8UL));    // for rpi5 16GB target
        }
    } else {
        add_ram(BCM2712_DRAM_BASE, ramsize); // 256M, 512M, 1G, 2G
    }

    /* Reserve VideoCore memory */
    alloc_ram(vcram_start, vcram_size, 0);
    as_add_containing(vcram_start, vcram_start + vcram_size - 1, AS_ATTR_RAM, "vcram", "ram");

    /* Add a below1G tag for legacy peripherals */
    if (ramsize > GIG(1UL)) {
        as_add_containing(BCM2712_DRAM_BASE,  vcram_start - 1, AS_ATTR_RAM, "below1G", "ram");
    }

    if (debug_flag > 3) {
        kprintf("vcram start-size: %P-%P, memory size: %l\n", vcram_start, vcram_size, ramsize);
    }
}

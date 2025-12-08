/*
 * Notice on Software Maturity and Quality
 *
 * The software included in this repository is classified under our Software Maturity Standard as Experimental Software - Software Quality and Maturity Level (SQML) 1.
 *
 * As defined in the QNX Development License Agreement, Experimental Software represents early-stage deliverables intended for evaluation or proof-of-concept purposes.
 *
 * SQML 1 indicates that this software is provided without one or more of the following:
 *     - Formal requirements
 *     - Formal design or architecture
 *     - Formal testing
 *     - Formal support
 *     - Formal documentation
 *     - Certifications of any type
 *     - End-of-Life or End-of-Support policy
 *
 * Additionally, this software is not monitored or scanned under our Cybersecurity Management Standard.
 *
 * No warranties, guarantees, or claims are offered at this SQML level.
 */

/*
 * Copyright (c) 2020,2022,2024 BlackBerry Limited.
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
#include <hw/hwinfo_private.h>
#include <drvr/hwinfo.h>
#include <libfdt.h>
#include "mbox.h"
#include "bcm2712_startup.h"

#define BCM2712_RP1_ENET_BASE          0x1f00100000
#define BCM2712_RP1_ENET_SIZE          0x4000
#define BCM2712_RP1_ENET_IRQ           0xa6
#define BCM2712_HWI_CGEM               "cgem"

static void fdt_get_mac_addr(uint8_t* const mac)
{
    static char *bootargs_str = NULL;

    int const chosen = fdt_path_offset(fdt, "/chosen");
    if (chosen <= 0) {
        kprintf("Did not find chosen in FDT\n");
        return;
    }

    int len;
    const struct fdt_property *const prop = fdt_get_property(fdt, chosen, "bootargs", &len);
    if (prop == NULL) {
        kprintf("Did not find chosen bootargs in FDT\n");
        return;
    }
    bootargs_str = ws_alloc(len);
    if (bootargs_str == NULL) {
        return;
    }
    memcpy(bootargs_str, prop->data, (size_t)len);
    if (debug_flag > 4) {
        kprintf("fdt: bootargs %s\n", bootargs_str);
    }
    for (;;) {
        char * const arg = find_next_arg(bootargs_str, &bootargs_str, 0);
        char *macstr;
        if (*arg == '\0') return;
        macstr=strstr(arg, "smsc95xx.macaddr=");
        if (macstr != NULL) {
            macstr=macstr+(sizeof("smsc95xx.macaddr=")-1);
            int i;
            for (i=0; i<6; i++) {
                mac[i] = (uint8_t)strtoul(macstr, NULL, 16);
                if (macstr[2] == ':') {
                    macstr += 3;
                }
                else break;
            }
            break;
        }
    }
}

static uint32_t fdt_get_board_revision(void)
{
    const int node_offset = fdt_path_offset(fdt, "/system");
    int prop_len;
    struct fdt_property const * const prop =  fdt_get_property(fdt, node_offset, "linux,revision", &prop_len);
    if (prop == NULL)  return 0;

    fdt32_t const * const data = (fdt32_t const *)prop->data;
    return fdt32_to_cpu(data[0]);
}

static void get_enet_mac_address(uint8_t* const mac) {
    mbox_get_board_mac_address(mac);
    if (debug_flag > 1) {
        kprintf("mbox Enet MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    if ((mac[0] == 0) && (mac[1] == 0) && (mac[2] == 0)) {
        fdt_get_mac_addr(mac);
        if (debug_flag > 1) {
            kprintf("FDT: Enet MAC: %x:%x:%x:%x:%x:%x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }
}

uint32_t get_board_revision(void)
{
    static uint32_t revision = 0;

    if (revision == 0) {
        revision = mbox_get_board_revision();
        if (revision == 0) {
            revision = fdt_get_board_revision();
            if (revision == 0) {
                /* hard code to 00c04170 - 4GB target */
                /*              0xd04170 - 8GB target */
                revision = 0x00c04170;
                kprintf("Use default board revision\n");
            }
            if (debug_flag > 1) {
                kprintf("FDT: board revision 0x%x\n", revision);
            }
        }
        else {
            if (debug_flag > 1) {
                kprintf("mbox: board revision 0x%x\n", revision);
            }
        }
    }
    return revision;
}

void init_hwinfo(void)
{
    const unsigned hwi_bus_internal = 0;
    /* Add ENET */
    {
        unsigned i, hwi_off;
        uint8_t mac[6] = { 0 };
        hwiattr_enet_t attr = HWIATTR_ENET_T_INITIALIZER;
        hwiattr_common_t common_attr = HWIATTR_COMMON_INITIALIZER;
        HWIATTR_ENET_SET_NUM_IRQ(&attr, 1);

        uint32_t irqs[] = {
            BCM2712_RP1_ENET_IRQ
        };

        HWIATTR_SET_NUM_IRQ(&common_attr, NUM_ELTS(irqs));

        /* Create DWC0 */
        HWIATTR_ENET_SET_LOCATION(&attr, BCM2712_RP1_ENET_BASE, BCM2712_RP1_ENET_SIZE, 0, hwi_find_as(BCM2712_RP1_ENET_BASE, 1));
        hwi_off = hwidev_add_enet(BCM2712_HWI_CGEM, &attr, hwi_bus_internal);
        hwitag_add_common(hwi_off, &attr);
        ASSERT(hwi_find_unit(hwi_off) == 0);

        /* Add IRQ number */
        for(i = 0; i < NUM_ELTS(irqs); i++) {
            hwitag_set_ivec(hwi_off, i, irqs[i]);
        }

        get_enet_mac_address(mac);
        hwitag_add_nicaddr(hwi_off, mac, sizeof(mac));
    }

    /* add the WATCHDOG device */
    {
        unsigned hwi_off;
        hwiattr_timer_t attr = HWIATTR_TIMER_T_INITIALIZER;
        const struct hwi_inputclk clksrc_kick = {.clk = 500, .div = 1};
        HWIATTR_TIMER_SET_NUM_CLK(&attr, 1);
        HWIATTR_TIMER_SET_LOCATION(&attr, BCM2712_PM_BASE, BCM2712_PM_SIZE, 0, hwi_find_as(BCM2712_PM_BASE, 1));
        hwi_off = hwidev_add_timer("wdog", &attr,  HWI_NULL_OFF);
        ASSERT(hwi_off != HWI_NULL_OFF);
        hwitag_set_inputclk(hwi_off, 0, (struct hwi_inputclk *)&clksrc_kick);

        hwi_off = hwidev_add("wdt,options", 0, HWI_NULL_OFF);
        hwitag_add_regname(hwi_off, "write_width", 32);
        hwitag_add_regname(hwi_off, "enable_width", 32);
    }
}

#define PCIE_EXT_MIP_INTC_ADDR            0x1000131000 /* msi-controller for the PCIe extension port */
#define PCIE_EXT_MIP_INTC_SIZE            0xc0

#define MIP_INTC_CLEARED                  0x10
#define MIP_INTC_CFGL_HOST                0x20
#define MIP_INTC_CFGH_HOST                0x30
#define MIP_INTC_MASKL_HOST               0x40
#define MIP_INTC_MASKH_HOST               0x50
#define MIP_INTC_MASKL_VPU                0x60
#define MIP_INTC_MASKH_VPU                0x70
#define MIP_INTC_STATUSL_HOST             0x80
#define MIP_INTC_STATUSH_HOST             0x90
#define MIP_INTC_STATUSL_VPU              0xa0
#define MIP_INTC_STATUSH_VPU              0xb0

void init_pcie_ext_msi_controller(void)
{
    const uintptr_t base = (uintptr_t) PCIE_EXT_MIP_INTC_ADDR;

    /* set all MSI-Xs edge-triggered, masked in for the host, masked out for the VPU */
    out32(base + MIP_INTC_MASKL_HOST, 0);
    out32(base + MIP_INTC_MASKH_HOST, 0);
    out32(base + MIP_INTC_MASKL_VPU, 0xffffffff);
    out32(base + MIP_INTC_MASKH_VPU, 0xffffffff);
    out32(base + MIP_INTC_CFGL_HOST, 0xffffffff);
    out32(base + MIP_INTC_CFGH_HOST, 0xffffffff);
}

/*
 * Copyright (c) 2020,2022,2024-2025 BlackBerry Limited.
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
#include <arm/pl011.h>
#include <hw/hwinfo_private.h>
#include <drvr/hwinfo.h>
#include "mbox.h"
#include "bcm2712_startup.h"
#include "board.h"

/**
 * Go through a string and find the next space.
 * Optionally NUL-terminating the previous segment.
 * @param   str         string to look at
 * @param   end         where to store a pointer to the next segment
 * @param   terminate   whether or not to NUL-terminate the previous segment
 * @return  a pointer to the first non-space char in the previous segment.
 */
char *find_next_arg(char *str, char ** const end, const unsigned terminate) {
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

void cpu_tweak_cmdline(struct bootargs_entry *bap, const char * const name) {
    // Override this function if you want to adjust the command line arguments
    // for the given bootstrap exectuable name - see:
    // bootstrap_arg_adjust()
    // bootstrap_env_adjust()
    const static char *next_arg;

    if (!next_arg) { // only do this once
        char *str = mbox_get_cmdline();

        if (str == NULL) {
            return;
        }

        // skip vpu-inserted arguments up to dash or double-space
        do {
            next_arg = find_next_arg(str, &str, 0);
            if (next_arg[0] == '\0') break;
            if (next_arg[0] == '-') break;
            if ((next_arg[0] == ' ') && (next_arg[1] == ' ')) break;
        } while (1);

        for (;;) {
            if (next_arg[0] == '\0') break;
            if ((next_arg[0] == '-') && (next_arg[1] == '-')) break;
            bootstrap_arg_adjust(bap, NULL, next_arg);
            next_arg = find_next_arg(str, &str, 1);
        }
    }
}

int main(const int argc, char ** const argv)
{
    int opt;
    uint32_t options = 0;
    uint32_t wdt_timeout = WDT_USE_DEFAULT;
    uint32_t board_rev;

    const struct debug_device debug_devices[] = {

        {   .name = "pl011",
            .defaults[DEBUG_DEV_CONSOLE] = "0x107d001000^2.115200.44236800", /* uart 10*/
            .init = init_pl011,
            .put = put_pl011,
            .callouts[DEBUG_DISPLAY_CHAR] = &display_char_pl011,
            .callouts[DEBUG_POLL_KEY] = &poll_key_pl011,
            .callouts[DEBUG_BREAK_DETECT] = &break_detect_pl011,
        },
    };

    // Reboot mechanism
    const struct callout_slot callouts[] = {
        {
            .offset  = offsetof(struct callout_entry, reboot),
            .callout = &reboot_bcm2712
        },
    };

    add_callout_array(callouts, sizeof callouts);

    /* Initialize the default debug interface for now. */
    select_debug(&debug_devices[0], sizeof(debug_devices[0]));
    kprintf(".....\n");
    while ((opt = getopt(argc, argv, COMMON_OPTIONS_STRING "W:")) != -1) {
        switch (opt) {
            case 'W':
                options |= WDT_ENABLE;
                if (optarg) {
                    uint32_t timeout = (uint32_t) strtoul(optarg, &optarg, 0);
                    if (timeout != 0U) {
                        wdt_timeout = timeout;
                    }

                }
                break;
            default:
                handle_common_option(opt);
                break;
        }
    }

    if (options & WDT_ENABLE) {
        /*
         * Enable WDT
        */
        bcm2712_wdt_enable(wdt_timeout);
    }

    /* Re-initialize in case the "-D" option was specified. */
    select_debug(debug_devices, sizeof(debug_devices));

    cpu_freq = mbox_get_clock_rate(MBOX_CLK_ARM);
    cycles_freq = cpu_freq;
    timer_freq = 0;

    bcm2712_init_raminfo();

    /*
     * Remove RAM used by modules in the image
     */
    alloc_ram(shdr->ram_paddr, shdr->ram_size, 1);

    if (fdt_psci_configure() == 0) {
        kprintf("Failed to get FDT PSCI\n");
    }
    else {
        if (debug_flag > 3) {
            kprintf("Found FDT PSCI\n");
        }
    }

    /* Enable Hypervisor if requested (and possible) */
    hypervisor_init(0);

    init_smp();

    if (debug_flag > 3) {
        uint32_t i;

        kprintf("%u CPUs @ %uMHz\n", lsp.syspage.p->num_cpu, cpu_freq / 1000000U);
        kprintf("Temps %d\n", mbox_get_temperature(0U));
        //0=rsvd 1=core 2=sdram_c 3=sdram_p 4=sdram_i
        kprintf("Volts");
        for (i = 1U; i <= 4U; i++) {
            kprintf(" %u=%u", i, mbox_get_voltage(i));
        }
        kprintf("\n");
        kprintf("Clock");
        // 0=rsvd 1=EMMC1 2=UART 3=ARM 4=CORE 5=V3D 6=H264 7=ISP 8=SDRAM 9=PIXEL 10=PWM 12=EMMC2
        for (i = 1U; i <= 14U; i++) {
            const uint32_t clk = mbox_get_clock_rate(i);
            if (clk) {
                kprintf(" %u=%uMHz", i, clk / 1000000U);
            }
        }
        kprintf("\n");
        kprintf("Power");
        // 0=SD-Card 1=UART0 2=UART1 3=USB-HCD 4=I2C0 5=I2C1 6=I2C2 7=SPI 8=CCP2TX
        for (i = 0U; i <= 8U; i++) {
            kprintf(" %u=%u", i, mbox_get_power_state(i));
        }
        kprintf("\n");
    }

    if (shdr->flags1 & STARTUP_HDR_FLAGS1_VIRTUAL) {
        init_mmu();
    }

    init_pcie_ext_msi_controller();

    init_intrinfo();

    init_qtime();

    init_cacheattr();

    init_cpuinfo();

    init_hwinfo();

    board_rev = get_board_revision();
    add_typed_string(_CS_MACHINE, get_board_name(board_rev));
    add_typed_string(_CS_HW_PROVIDER, get_mfr_name(board_rev));
    add_typed_string(_CS_ARCHITECTURE, get_soc_name(board_rev));
    add_typed_string(_CS_HW_SERIAL, get_board_serial());
    init_gpio_aon_bcm();
    /*
     * Load bootstrap executables in the image file system and Initialize
     * various syspage pointers. This must be the _last_ initialization done
     * before transferring control to the next program.
     */
    init_system_private();

    print_syspage();

    return 0;
}

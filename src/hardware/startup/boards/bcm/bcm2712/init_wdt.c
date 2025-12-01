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
#include "bcm2712_startup.h"

#define BCM2712_PM_RSTC                    (0x1cU)
#define BCM2712_PM_RSTS                    (0x20U)
#define BCM2712_PM_WDOG                    (0x24U)

#define PM_PASSWORD                  (0x5a000000U)
#define PM_WDOG_TIME_SET             (0x000fffffU)
#define PM_RSTC_WRCFG_CLR            (0xffffffcfU)
#define PM_RSTS_HADWRH_SET           (0x00000040U)
#define PM_RSTC_WRCFG_SET            (0x00000030U)
#define PM_RSTC_WRCFG_FULL_RESET     (0x00000020U)
#define PM_RSTC_STOP                 (0x00000102U)
#define PM_WDOG_MS_TO_TICKS          (67UL)
#define WDT_MAX_TICKS                (0x000fffffU)
#define WDT_DEFAULT_TICKS            (0x00010000U)

/* Enable watchdog timer */
void bcm2712_wdt_enable(const uint32_t timeout_value)
{
    const uintptr_t base = (uintptr_t) BCM2712_PM_BASE;
    uint32_t val;
    uint64_t ticks;

    if (timeout_value  <= 1000U) {
        val = WDT_DEFAULT_TICKS;
    } else {
        ticks = timeout_value * PM_WDOG_MS_TO_TICKS;
        if (ticks > WDT_MAX_TICKS) {
            ticks = WDT_MAX_TICKS;
        }
        val = (uint32_t) ticks;
    }

    /* Disable watch dog */
    out32(base + BCM2712_PM_RSTC, PM_PASSWORD | PM_RSTC_STOP);

    /* Reload the timer value */
    out32(base + BCM2712_PM_WDOG, PM_PASSWORD | val);

	if (debug_flag > 3) {
	    kprintf("wdt: Reload the timer value %x = %x\n", base + BCM2712_PM_WDOG, PM_PASSWORD | val);
	    kprintf("wdt: read %x = %x\n", base + BCM2712_PM_RSTC, in32(base + BCM2712_PM_RSTC));
	}
    /* Enable the wdog timer */
    val = in32(base + BCM2712_PM_RSTC) & PM_RSTC_WRCFG_CLR;
    val |= (PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET);

	if (debug_flag > 3) {
	    kprintf("wdt: Enable the wdog timer %x = %x\n", base + BCM2712_PM_RSTC, val);
	}

    out32(base + BCM2712_PM_RSTC, val);
}

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

#ifndef __BCM2712_BOARD_H__
#define __BCM2712_BOARD_H__

/* Options flags */
#define WDT_ENABLE                    (1U << 0)
#define WDT_USE_DEFAULT               (15000U) /* 15000ms */

#define BCM2712_PM_BASE               (0x107d200000UL)
#define BCM2712_PM_SIZE               (0x308UL)

const unsigned get_memory_size(uint32_t board_rev);
const char *get_soc_name(uint32_t board_rev);
const char *get_mfr_name(uint32_t board_rev);
const char * get_board_name(uint32_t board_rev);
const char *get_board_serial(void);

void bcm2712_init_raminfo(void);
void bcm2712_wdt_enable(const uint32_t timeout_value);

extern void init_hwinfo(void);
extern void init_pcie_ext_msi_controller(void);
extern uint32_t get_board_revision(void);
extern void init_gpio_aon_bcm(void);

extern char *find_next_arg(char *str, char ** const end, const unsigned terminate);
extern struct callout_rtn display_char_miniuart;
extern struct callout_rtn poll_key_miniuart;
extern struct callout_rtn break_detect_miniuart;
extern struct callout_rtn reboot_bcm2712;

#endif /* __BCM2712_BOARD_H__ */

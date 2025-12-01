/*
 * Copyright (c) 2020, 2022, BlackBerry Limited.
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


/* This file can contain board specific preprocessor macros and function declarations */

#ifndef __BOARD_H

/*
 * pi5 AON gpio, FDT: "brcm,bcm2712-aon-pinctrl"
 */
#define BCM2712_AON_GPIO_BASE          (0x107d517c00UL)
#define BCM2712_AON_GPIO_SIZE          (0x40U)
#define BCM2712_AON_GPIO_NUM           (38U)
#define BCM2712_AON_GPIO_BANK_NUM      (2U)
#define BCM2712_AON_GPIO_BANK0_GPIO    (17U)
#define BCM2712_AON_GPIO_BANK1_GPIO    (6U)
#define BCM2712D0_AON_GPIO_BANK0_GPIO  (15U)
#define BCM2712D0_AON_GPIO_BANK1_GPIO  BCM2712_AON_GPIO_BANK1_GPIO

#define BCM2712_AON_PINMUX_BASE        (0x107d510700UL)
#define BCM2712_AON_PINMUX_SIZE        (0x20U)
#define BCM2712D0_AON_PINMUX_SIZE      (0x1cU)

/*
 * pi5 gpio, "brcm,bcm2712-pinctrl"
 */
#define BCM2712_GPIO_BASE              (0x107d508500UL)
#define BCM2712_GPIO_SIZE              (0x40U)
#define BCM2712_GPIO_NUM               (36U)
#define BCM2712_GPIO_BANK_NUM          (2U)
#define BCM2712_GPIO_BANK0_GPIO        (32U)
#define BCM2712_GPIO_BANK1_GPIO        (4U)

#define BCM2712_PINMUX_BASE            (0x107d504100UL)
#define BCM2712_PINMUX_SIZE            (0x30U)
#define BCM2712D0_PINMUX_SIZE          (0x20U)


#define BCM2712_GIO_DATA               (0x04U)
#define BCM2712_GIO_IODIR              (0x08U)

#define BCM2712_PAD_PULL_OFF           (0U)
#define BCM2712_PAD_PULL_DOWN          (1U)
#define BCM2712_PAD_PULL_UP            (2U)

#define DRIVE_UNSET                    UINT32_MAX
#define DRIVE_LOW                      (0U)
#define DRIVE_HIGH                     (1U)

#define PULL_UNSET                     UINT32_MAX
#define PULL_NONE                      (0U)
#define PULL_DOWN                      (1U)
#define PULL_UP                        (2U)

#define DIR_UNSET                      UINT32_MAX
#define DIR_INPUT                      (0U)
#define DIR_OUTPUT                     (1U)

#define FUNC_UNSET                     UINT32_MAX
#define FUNC_A0                        (0U)
#define FUNC_A1                        (1U)
#define FUNC_A2                        (2U)
#define FUNC_A3                        (3U)
#define FUNC_A4                        (4U)
#define FUNC_A5                        (5U
#define RP1_FSEL_SYS_RIO               (FUNC_A5)
#define FUNC_A6                        (6U)
#define FUNC_A7                        (7U)
#define FUNC_A8                        (8U)
#define NUM_ALT_FUNCS                  (8U)
#define FUNC_NULL                      (0x1f)

#define FUNC_IP                        (20U)
#define FUNC_OP                        (21U)
#define FUNC_GP                        (22U)
#define FUNC_NO                        (23U)

struct bcm2712_pinmux_t {
    uint8_t pin_num;
    uint8_t fsel;
    uint8_t pull;
    uint8_t gpio_lvl;
};


#define __BOARD_H

#endif /* __BOARD_H */

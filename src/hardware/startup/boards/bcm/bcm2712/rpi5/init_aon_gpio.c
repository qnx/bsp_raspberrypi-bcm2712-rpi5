/*
 * $QNXLicenseC:
 * Copyright 2025 QNX Software Systems.
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
#include <stdint.h>
#include <sys/types.h>
#include <libfdt.h>
#include "bcm2712_startup.h"
#include "board.h"

/* pi5 has two types of targets:
 *  - revision 1.0, uses BCM2712 C1 stepping SOC with
 *    FDT pinctrl@7d504100 node has: compatible = "brcm,bcm2712-pinctrl";
 *  - revision 1.1, uses BCM2712 D0 stepping SOC with
 *    FDT pinctrl@7d504100 node has: compatible = "brcm,bcm2712d0-pinctrl";
 */
#define BCM2712_STEPPING_C1         (0U)
#define BCM2712_STEPPING_D0         (1U)
static uint32_t bcm2712_stepping = BCM2712_STEPPING_C1;
static uint32_t gpio_bank_widths[BCM2712_AON_GPIO_BANK_NUM] = { BCM2712_AON_GPIO_BANK0_GPIO, BCM2712_AON_GPIO_BANK1_GPIO };

static const uint32_t gpio_aon_bcm_d0_to_c0[BCM2712_AON_GPIO_NUM] =
{
    0,              // 0
    1,              // 1
    2,              // 2
    3,              // 3
    4,              // 4
    5,              // 5
    6,              // 6
    UINT32_MAX,     // 7
    7,              // 8
    8,              // 9
    UINT32_MAX,     // 10
    UINT32_MAX,     // 11
    9,              // 12
    10,             // 13
    11,             // 14
    UINT32_MAX,     // 15
    UINT32_MAX,     // 16
    UINT32_MAX,     // 17
    UINT32_MAX,     // 18
    UINT32_MAX,     // 19
    UINT32_MAX,     // 20
    UINT32_MAX,     // 21
    UINT32_MAX,     // 22
    UINT32_MAX,     // 23
    UINT32_MAX,     // 24
    UINT32_MAX,     // 25
    UINT32_MAX,     // 26
    UINT32_MAX,     // 27
    UINT32_MAX,     // 28
    UINT32_MAX,     // 29
    UINT32_MAX,     // 30
    UINT32_MAX,     // 31
    32,             // 32
    33,             // 33
    34,             // 34
    35,             // 35
    36,             // 36
    37,             // 37
};

static void fdt_get_soc_stepping(void)
{
    int node_offset = fdt_node_offset_by_compatible(fdt, -1, "brcm,bcm2712d0-aon-pinctrl");

    if (node_offset >= 0) {
        bcm2712_stepping = BCM2712_STEPPING_D0;
        kprintf("fdt: found node compatible with brcm,bcm2712d0-aon-pinctrl\n");
    }
    else {
        int i;
        node_offset = -1;
        for(i=0; i<2; i++) {
            node_offset = fdt_node_offset_by_compatible(fdt, node_offset, "brcm,brcmstb-gpio");
            if (node_offset >= 0) {
                int prop_len;
                struct fdt_property const *prop = fdt_get_property(fdt, node_offset, "reg", &prop_len);
                if ((prop != NULL) && (prop_len == ((int)sizeof(fdt32_t)*2))) {
                    fdt32_t const *data = (fdt32_t const *)prop->data;
                    if (fdt32_to_cpu(*data) == (uint32_t)(BCM2712_AON_GPIO_BASE & 0xffffffffU)) {
                        prop = fdt_get_property(fdt, node_offset, "brcm,gpio-bank-widths", &prop_len);
                        if ((prop != NULL) && (prop_len == ((int)sizeof(fdt32_t)*2))) {
                            data = (fdt32_t const *)prop->data;
                            if (fdt32_to_cpu(*data) == 15U) {
                                bcm2712_stepping = BCM2712_STEPPING_D0;
                                kprintf("fdt: found node compatible with brcm,brcmstb-gpio and brcm,gpio-bank-widths: 15 ..\n");
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
        /* fix D0 Soc AON gpio bank[0] width */
        gpio_bank_widths[0] = BCM2712D0_AON_GPIO_BANK0_GPIO;
    }
}

static uint32_t get_gpio_base_offset(const uint32_t gpio, uint32_t *offset)
{
    const uint32_t bank = gpio / 32U;
    const uint32_t gpio_offset = gpio % 32U;                    // gpio within bank
    uint32_t base_offset = UINT32_MAX;

    if (bank < BCM2712_AON_GPIO_BANK_NUM) {
        if (gpio_offset < gpio_bank_widths[bank]) {
            base_offset = bank * 0x20U;
            *offset = gpio_offset;
        }
        else {
            kprintf("%s: invalid gpio:%u bank/widths: %d/%d\n", __func__, gpio, bank, gpio_bank_widths[bank]);
        }
    }

    return base_offset;
}

static uint32_t get_pinmux_base_offset(const uint32_t gpio_in, uint32_t *offset)
{
    uint32_t bank = 0U;
    uint32_t gpio, gpio_offset = 0U;     // gpio index within bank
    uint32_t base_offset = UINT32_MAX;

    if ((bcm2712_stepping == BCM2712_STEPPING_D0) && (gpio_in < BCM2712_AON_GPIO_NUM)) {
        gpio = gpio_aon_bcm_d0_to_c0[gpio_in];
    }
    else {
        gpio = gpio_in;
    }

    if (gpio != UINT32_MAX) {
        bank = gpio / 32U;
        gpio_offset = gpio % 32U;     // gpio within bank

        if (bank < BCM2712_AON_GPIO_BANK_NUM) {
            if (gpio_offset < gpio_bank_widths[bank]) {
                if (bank == 1U)
                {
                    if (gpio_offset == 4U)
                    {
                        *offset = 0U;
                        base_offset = (uint32_t)(sizeof(uint32_t));
                        if (debug_flag > 3) {
                            kprintf("%s: gpio: %u (%u/%u) bit offset: %u, base_offset: 0x%x\n",
                                    __func__, gpio, bank, gpio_offset, *offset, base_offset);
                        }
                    }
                    else if (gpio_offset == 5U)
                    {
                        *offset = 0U;
                        base_offset = 2U * (uint32_t)sizeof(uint32_t);
                        if (debug_flag > 3) {
                            kprintf("%s: gpio: %u (%u/%u) bit offset: %u, base_offset: 0x%x\n",
                                    __func__, gpio, bank, gpio_offset, *offset, base_offset);
                        }
                    }
                    else
                    {
                        *offset = gpio_offset * 4U;
                        base_offset = 0U;
                        if (debug_flag > 3) {
                            kprintf("%s: gpio: %u (%u/%u) bit offset: %u, base_offset: 0x%x\n",
                                    __func__, gpio, bank, gpio_offset, *offset, base_offset);
                        }
                    }
                }
                else {
                    *offset = (gpio_offset % 8U) * 4U;
                    base_offset = (3U * (uint32_t)sizeof(uint32_t) + (gpio_offset / 8U) * (uint32_t)sizeof(uint32_t));
                    if (debug_flag > 2) {
                        kprintf("%s: gpio: %u (%u/%u) bit offset: %u, base_offset: 0x%x\n",
                            __func__, gpio, bank, gpio_offset, *offset, base_offset);
                    }
                }
            }
            else {
                kprintf("%s: invalid gpio:%u to fit in bank/widths: %d/%d\n", __func__, gpio, bank, gpio_bank_widths[bank]);
            }
        }
    }
    return base_offset;
}

static uint32_t get_pad_base_offset(const uint32_t gpio_in, uint32_t *offset)
{
    uint32_t bank = 0U;
    uint32_t gpio, gpio_offset = 0U;     // gpio index within bank
    uint32_t base_offset = UINT32_MAX;

    if ((bcm2712_stepping == BCM2712_STEPPING_D0) && (gpio_in < BCM2712_AON_GPIO_NUM)) {
        gpio = gpio_aon_bcm_d0_to_c0[gpio_in];
    }
    else {
        gpio = gpio_in;
    }

    if (gpio != UINT32_MAX) {
        bank = gpio / 32U;
        gpio_offset = gpio % 32U;     // gpio within bank

        if (bank < BCM2712_AON_GPIO_BANK_NUM) {
            if (gpio_offset < gpio_bank_widths[bank]) {
                if (bank == 0U) {
                    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                        gpio_offset = gpio + 84U;
                    }
                    else {
                        gpio_offset = gpio + 100U;
                    }
                    *offset = (gpio_offset % 15U) * 2U;
                    base_offset = ((gpio_offset / 15U) * (uint32_t)sizeof(uint32_t));
                    if (debug_flag > 3) {
                        kprintf("%s: %u (%u/%u) bit offset: %u, base_offset: 0x%x\n",
                                __func__, gpio, bank, gpio_offset, *offset, base_offset);
                    }
                }
                else { /* no SGPIO pad control */
                }
            }
            else {
                kprintf("%s: invalid gpio:%u to fit in bank/widths: %d/%d\n", __func__, gpio, bank, gpio_bank_widths[bank]);
            }
        }
    }
    return base_offset;
}

static void set_dir(const uint32_t gpio, const uint32_t dir)
{
    uint32_t offset = 0U;
    uint32_t base_offset = get_gpio_base_offset(gpio, &offset);

    if (base_offset != UINT32_MAX) {
        uint32_t gpio_val, gpio_val_new;
        gpio_val = in32(BCM2712_AON_GPIO_BASE + base_offset + BCM2712_GIO_IODIR);
        gpio_val_new = gpio_val & ~(((uint32_t)(1U)) << offset);
        if (dir == DIR_INPUT) {
            gpio_val_new |= (((uint32_t)(1U)) << offset);
        }
        if (debug_flag > 3) {
            kprintf("%s: gpio: %u bit offset: %u dir: %d gpio_base[0x%x]: 0x%x -> 0x%x\n",
                    __func__, gpio, offset, dir, (base_offset + BCM2712_GIO_IODIR), gpio_val, gpio_val_new);
        }
        out32(BCM2712_AON_GPIO_BASE + base_offset + BCM2712_GIO_IODIR, gpio_val_new);
    }
    return;
}

static uint32_t get_dir(const uint32_t gpio)
{
    uint32_t dir = DIR_UNSET;
    uint32_t offset = 0;
    uint32_t base_offset = get_gpio_base_offset(gpio, &offset);

    if (base_offset != UINT32_MAX) {
        uint32_t gpio_val;
        gpio_val = in32(BCM2712_AON_GPIO_BASE + base_offset + BCM2712_GIO_IODIR);
        if ((gpio_val & (((uint32_t)(1U)) << offset)) != 0U) {
            dir = DIR_INPUT;
        }
        else {
            dir = DIR_OUTPUT;
        }
        if (debug_flag > 0) {
            kprintf("%s: gpio: %u bit offset: %u read gpio_base[0x%x]: 0x%x, return %d\n",
                    __func__, gpio, offset, (base_offset + BCM2712_GIO_IODIR), gpio_val, dir);
        }
    }
    return dir;
}

static void bcm2712_gpio_aon_bcm_set_fsel(uint32_t gpio, const uint32_t func_type)
{
    uint32_t offset = 0U;
    uint32_t base_offset = get_pinmux_base_offset(gpio, &offset);
    uint32_t fsel = FUNC_UNSET;
    uint32_t pinmux_val, pinmux_val_new;

    if (base_offset != UINT32_MAX) {
        if (func_type <= NUM_ALT_FUNCS) {
            fsel = func_type - FUNC_A0;
        }
        else if ((func_type == FUNC_IP) || (func_type == FUNC_OP) || (func_type == FUNC_GP)) {
            fsel = 0U;
            // Set direction first
            if (func_type == FUNC_IP) {
                set_dir(gpio, DIR_INPUT);
            }
            if (func_type == FUNC_OP) {
                set_dir(gpio, DIR_OUTPUT);
            }
        }
        else {
            kprintf("%s: gpio: %u invalid function type: %u\n", __func__, gpio, func_type);
        }

        if (fsel != FUNC_UNSET) {
            pinmux_val = in32(BCM2712_AON_PINMUX_BASE + base_offset);

            pinmux_val_new = pinmux_val & (~(((uint32_t)(0xfU)) << offset));
            pinmux_val_new |= (fsel << offset);

            if (debug_flag > 1) {
                kprintf("%s gpio: %u func: %u offset: 0x%x pinmux_val: 0x%x -> 0x%x\n",
                        __func__, gpio, func_type, offset, pinmux_val, pinmux_val_new);
            }
            out32(BCM2712_AON_PINMUX_BASE + base_offset, pinmux_val_new);
        }
    }
}

static uint32_t bcm2712_gpio_aon_bcm_get_fsel(uint32_t gpio)
{
    uint32_t offset = 0U;
    uint32_t base_offset = get_pinmux_base_offset(gpio, &offset);
    uint32_t func_type = FUNC_UNSET;

    if (base_offset != UINT32_MAX) {
        uint32_t fsel_val = (in32(BCM2712_AON_PINMUX_BASE + base_offset) >> offset) & 0xfU;

        if (fsel_val == 0U) {
            if (get_dir(gpio) == DIR_OUTPUT) {
                func_type = FUNC_OP;
                if (debug_flag > 0) {
                    kprintf("%s: gpio: %u pinmux_base: 0x%L offset: %u fsel=0x%x, get_dir(gpio) == DIR_OUTPUT return FUNC_OP\n",
                            __func__, gpio, (BCM2712_AON_PINMUX_BASE + base_offset), offset, fsel_val);
                }
            }
            else {
                func_type = FUNC_IP;
                if (debug_flag > 0) {
                    kprintf("%s: gpio: %u pinmux_base: 0x%L offset: %u fsel=0x%x, get_dir(gpio) != DIR_OUTPUT return FUNC_IP\n",
                            __func__, gpio, (BCM2712_AON_PINMUX_BASE + base_offset), offset, fsel_val);
                }
            }
        }
        else if (fsel_val <= NUM_ALT_FUNCS) {
            func_type = FUNC_A0 + fsel_val;
            if (debug_flag > 0) {
                kprintf("%s: gpio: %u pinmux_base: 0x%L offset: %u fsel=0x%x, return FUNC_A0 + %u\n",
                        __func__, gpio, (BCM2712_AON_PINMUX_BASE + base_offset), offset, fsel_val, (FUNC_A0 + fsel_val));
            }
        }
        else if (fsel_val == 0xfUL)  {// Choose one value as a considered NONE
            if (debug_flag > 0) {
                func_type = FUNC_NULL;
                kprintf("%s: gpio: %u pinmux_base: 0x%L offset: %u fsel=0x%x, return FUNC_NULL\n",
                        __func__, gpio, (BCM2712_AON_PINMUX_BASE + base_offset), offset, fsel_val);
            }
        }
        else {
            /* Unknown FSEL */
            if (debug_flag > 0) {
                kprintf("%s: gpio: %u pinmux_base: 0x%L offset: %u fsel=0x%x, return func_type=0x%x\n",
                        __func__, gpio, (BCM2712_AON_PINMUX_BASE + base_offset), offset, fsel_val, func_type);
            }
        }
    }

    return func_type;
}

static void bcm2712_gpio_aon_bcm_set_level(uint32_t gpio, uint32_t level)
{
    uint32_t offset = 0U;
    const uint32_t base_offset = get_gpio_base_offset(gpio, &offset);

    if (base_offset != UINT32_MAX) {
        uint32_t gpio_val, gpio_val_new;
        gpio_val = in32(BCM2712_AON_GPIO_BASE + base_offset + BCM2712_GIO_DATA);
        gpio_val_new = (gpio_val & ~(((uint32_t)(1U)) << offset)) | (level << offset);

        if (debug_flag > 1) {
            kprintf("%s: gpio: %u offset: %u, gpio_base[0x%x]: 0x%x -> 0x%x\n",
                    __func__, gpio, offset, (base_offset + BCM2712_GIO_DATA), gpio_val, gpio_val_new);
        }
        out32(BCM2712_AON_GPIO_BASE + base_offset + BCM2712_GIO_DATA, gpio_val_new);
    }
}

static uint32_t bcm2712_gpio_aon_bcm_get_level(uint32_t gpio)
{
    uint32_t level = DRIVE_UNSET;
    uint32_t offset = 0U;
    const uint32_t base_offset = get_gpio_base_offset(gpio, &offset);

    if (base_offset != UINT32_MAX) {
        uint32_t gpio_val;
        gpio_val = in32(BCM2712_AON_GPIO_BASE + base_offset + BCM2712_GIO_DATA);
        if ((gpio_val & (((uint32_t)(1U)) << offset)) != 0U) {
            level = 1U;
        }
        else {
            level = 0U;
        }
        if (debug_flag > 3) {
            kprintf("%s: gpio: %u offset: %u gpio_base[0x%x]: 0x%x offset: %u, return %d\n",
                    __func__, gpio, offset, (base_offset + BCM2712_GIO_DATA), gpio_val, offset, level);
        }
    }

    return level;
}

static void bcm2712_gpio_aon_bcm_set_pull(uint32_t gpio, uint32_t type)
{
    uint32_t offset = 0U;
    const uint32_t base_offset = get_pad_base_offset(gpio, &offset);
    uint32_t val, pad_val, pad_val_new = 0U;

    if (base_offset != UINT32_MAX) {

        switch (type)
        {
        case PULL_NONE:
            val = BCM2712_PAD_PULL_OFF;
            break;
        case PULL_DOWN:
            val = BCM2712_PAD_PULL_DOWN;
            break;
        case PULL_UP:
            val = BCM2712_PAD_PULL_UP;
            break;
        default:
            kprintf("%s: gpio:%u invalid pull type: %u", __func__, gpio, type);
            break;
        }

        if ((type == PULL_NONE) || (type == PULL_DOWN) || (type == PULL_UP)) {
            pad_val = in32(BCM2712_AON_PINMUX_BASE + base_offset);
            pad_val_new = pad_val & ~(((uint32_t)(3U)) << offset);
            pad_val_new |= (val << offset);

            if (debug_flag > 1) {
                kprintf("%s: gpio: %u offset: %u, pad_base: 0x%L pad_val: 0x%x -> 0x%x\n",
                        __func__, gpio, offset, (BCM2712_AON_PINMUX_BASE + base_offset), pad_val, pad_val_new);
            }
            out32(BCM2712_AON_PINMUX_BASE + base_offset, pad_val_new);
        }
    }
}

static uint32_t bcm2712_gpio_aon_bcm_get_pull(uint32_t gpio)
{
    uint32_t offset = 0U;
    const uint32_t base_offset = get_pad_base_offset(gpio, &offset);
    uint32_t pad_val;
    uint32_t pull_type = UINT32_MAX;

    if (base_offset != UINT32_MAX) {
        pad_val = in32(BCM2712_AON_PINMUX_BASE + base_offset) ;
        switch ((pad_val >> offset) & 0x3U)
        {
        case BCM2712_PAD_PULL_OFF:
            if (debug_flag > 3) {
                kprintf("%s: gpio: %u offset: 0x%x pad_base: 0x%L pad_val: 0x%x return PULL_NONE\n",
                         __func__, gpio, offset, BCM2712_AON_PINMUX_BASE + base_offset, pad_val);
            }
            pull_type = PULL_NONE;
            break;
        case BCM2712_PAD_PULL_DOWN:
            if (debug_flag > 3) {
                kprintf("%s: gpio: %u offset: 0x%x pad_base: 0x%L pad_val: 0x%x return PULL_DOWN\n",
                         __func__, gpio, offset, BCM2712_AON_PINMUX_BASE + base_offset, pad_val);
            }
            pull_type = PULL_DOWN;
            break;
        case BCM2712_PAD_PULL_UP:
            if (debug_flag > 3) {
                kprintf("%s: gpio: %u offset: 0x%x pad_base: 0x%L pad_val: 0x%x return PULL_UP\n",
                         __func__, gpio, offset, BCM2712_AON_PINMUX_BASE + base_offset, pad_val);
            }
            pull_type = PULL_UP;
            break;
        default: /* This is an error */
            if (debug_flag > 3) {
                kprintf("%s: gpio: %u offset: 0x%x pad_base: 0x%L pad_val: 0x%x return error\n",
                         __func__, gpio, offset, BCM2712_AON_PINMUX_BASE + base_offset, pad_val);
            }
            break;
        }
    }

    return pull_type;
}


void init_gpio_aon_bcm(void)
{
    static const struct bcm2712_pinmux_t pi5_aon_gpio[] = {
        { .pin_num = 5, .fsel = FUNC_A6, .pull = PULL_UP, .gpio_lvl = 0 }, /* SD_CDET_N, A6 name: SD_CARD_PRES_G */
    };

    static const struct bcm2712_pinmux_t pi5_aon_gpio_d0[] = {
        { .pin_num = 5, .fsel = FUNC_A4, .pull = PULL_UP, .gpio_lvl = 0 }, /* SD_CDET_N, A4 name: SD_CARD_PRES_G */
    };

    uint32_t i;

    fdt_get_soc_stepping();

    for(i = 0; i < NUM_ELTS(pi5_aon_gpio); i++) {
        const struct bcm2712_pinmux_t *p = (bcm2712_stepping == BCM2712_STEPPING_D0)? &pi5_aon_gpio_d0[i] : &pi5_aon_gpio[i];

        bcm2712_gpio_aon_bcm_set_fsel(p->pin_num, p->fsel);
        bcm2712_gpio_aon_bcm_set_pull(p->pin_num, p->pull);
        if (p->fsel == FUNC_OP) {
            bcm2712_gpio_aon_bcm_set_level(p->pin_num, p->gpio_lvl);
        }
        if (debug_flag > 1) {
            uint32_t fsel = bcm2712_gpio_aon_bcm_get_fsel(p->pin_num);
            if (fsel != FUNC_UNSET) {
                kprintf("AON gpio: %u fsel: %u\n", p->pin_num, fsel);
            }
            uint32_t pull_type = bcm2712_gpio_aon_bcm_get_pull(p->pin_num);
            if (pull_type != PULL_UNSET) {
                kprintf("AON gpio: %u pull: %u\n", p->pin_num, pull_type);
            }
            uint32_t level = bcm2712_gpio_aon_bcm_get_level(p->pin_num);
            if (level != DRIVE_UNSET) {
                kprintf("AON gpio: %u level: %u\n", p->pin_num, level);
            }
        }
    }
}


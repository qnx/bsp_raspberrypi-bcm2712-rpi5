/*
 * $QNXLicenseC:
 * Copyright 2020, 2024-2025, QNX Software Systems.
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

/* Module Description:  board specific interface */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/inout.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sdhci.h>

/*
 * Raspberry PI5 regulator "sd_io_1v8_reg" is controlled by gpio "SD_IOVDD_SEL" pin 3 the gpio group defined by
 * FDT gpio@7d517c00
 * For SD 3.3v operation, the gpio state is set to 0, for 1.8v operation, the gpio state is set to 1
 */
#define BCM2712_AON_GPIO_BASE         0x107d517c00ULL
#define BCM2712_AON_GPIO_SIZE         0x40UL
#define BCM2712_AON_GPIO_NUM          38
#define BCM2712_AON_GPIO_BANK_NUM     2
#define BCM2712_AON_GPIO_BANK0_GPIO   17
#define BCM2712_AON_GPIO_BANK1_GPIO   6

#define BCM2712_GIO_DATA              (0x04U)
#define BCM2712_GIO_IODIR             (0x08U)

#define BCM2712_GIO_BANK_SIZE         (0x20U)

#define GPIO_SD_IOVDD_SEL_LINE        (0x3U)
#define GPIO_SD_IOVDD_SEL_3_3V        (0x0U)
#define GPIO_SD_IOVDD_SEL_1_8V        (0x1U)

struct gpio_state_t {
    uint32_t gpio;
    uint32_t state;
};

/* Structure describing board specific configuration */
struct bcm2712_ext_t {
    int32_t (*signal_voltage)(sdio_hc_t *hc, uint32_t signal_voltage);
    int32_t (*sdhci_tune)(sdio_hc_t *hc, uint32_t op);
};

static int32_t bcm2712_bs_args(const sdio_hc_t * const hc, char *options)
{
    char        *value;
    int         opt;
    static char *opts[] = { NULL };
    int32_t     ret = EOK;

    while (options && (*options != '\0')) {
        opt = sdio_hc_getsubopt(&options, opts, &value);
        if (opt == -1) {
            (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: invalid BS options %s", __func__, options);
            ret = EINVAL;
            break;
        }
    }
    return ret;
}

static int32_t gpio_state(const sdio_hc_t * const hc, struct gpio_state_t *gpioinfo, const bool set)
{
    uint32_t bank, offset;
    uintptr_t gpio_base;
    uint32_t gpio_val;
    int32_t ret = EOK;

    if (gpioinfo == NULL) {
        ret = -1;
    }
    else {
        bank = gpioinfo->gpio / BCM2712_GIO_BANK_SIZE;
        offset = gpioinfo->gpio % BCM2712_GIO_BANK_SIZE;  // gpio within bank

        gpio_base = (uintptr_t) mmap_device_memory(NULL, BCM2712_AON_GPIO_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, BCM2712_AON_GPIO_BASE);
        if (gpio_base == (uintptr_t) MAP_FAILED) {
            (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: mmap (GPIO) failed: %s\n", __func__, strerror (errno));
            ret = -1;
        }
        else {
            gpio_val = in32(gpio_base + (bank*BCM2712_GIO_BANK_SIZE) + BCM2712_GIO_DATA);
            if (set == false) {
                if ((gpio_val & (1UL << offset)) != 0UL) {
                    gpioinfo->state = 1;
                }
                else {
                    gpioinfo->state = 0;
                }
                (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: gpio: %u gpio_base[0x%x]: 0x%x offset: %u, state %u\n",
                            __func__, gpioinfo->gpio, BCM2712_GIO_DATA, gpio_val, offset, gpioinfo->state);
            }
            else {
                gpio_val = (gpio_val & ~(1UL << offset)) | (gpioinfo->state << offset);
                out32(gpio_base + (bank*BCM2712_GIO_BANK_SIZE) + BCM2712_GIO_DATA, gpio_val);
            }
            (void)munmap_device_memory((void *)gpio_base, BCM2712_AON_GPIO_SIZE);
        }
    }
    return ret;
}

static int32_t get_vdd_sd_io_state(sdio_hc_t * const hc)
{
    struct gpio_state_t gpioinfo = { .gpio = GPIO_SD_IOVDD_SEL_LINE, .state = 0 };
    int32_t ret;

    ret = gpio_state(hc, &gpioinfo, (bool)false);
    if (ret == EOK) {
        (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 1, "%s: get SD_IOVDD_SEL gpio state=%u", __func__, gpioinfo.state);
        ret = (int)gpioinfo.state;
    }
    else {
        (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: get SD_IOVDD_SEL gpio state failed, ret=%d", __func__, ret);
        ret = -1;
    }
    return ret;
}

static int32_t set_vdd_sd_io_state(sdio_hc_t * const hc, const uint32_t state)
{
    struct gpio_state_t gpioinfo = { .gpio = GPIO_SD_IOVDD_SEL_LINE, .state = state };
    int32_t ret;

    ret = gpio_state(hc, &gpioinfo, (bool)true);
    if (ret == EOK) {
        (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_INFO, hc->cfg.verbosity, 0, "%s: set SD_IOVDD_SEL gpio state=%u", __func__, gpioinfo.state);
    }
    else {
        (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: set SD_IOVDD_SEL gpio state: %d failed, ret=%d", __func__, state, ret);
        ret = -1;
    }
    return ret;
}

/*
 * This function is a variant of sdhci_signal_voltage, it calls rpi5 specific gpio APIs
 * to switch between 3.3v and 1.8v operation
 */
static int32_t bcm2712_signal_voltage(sdio_hc_t *hc, uint32_t signal_voltage)
{
    const struct bcm2712_ext_t * const ext = (struct bcm2712_ext_t *)hc->bs_hdl;
    const int32_t gpio_sd_iovdd_sel_state = get_vdd_sd_io_state(hc);

    if (signal_voltage == (uint32_t)SIGNAL_VOLTAGE_3_3) {
        if (gpio_sd_iovdd_sel_state != GPIO_SD_IOVDD_SEL_3_3V) {
            set_vdd_sd_io_state(hc, GPIO_SD_IOVDD_SEL_3_3V);
            (void)delay(5);
            // force ext->signal_voltage to switch to SIGNAL_VOLTAGE_3_3
            hc->signal_voltage = 0;
        }
    }
    else if (signal_voltage == (uint32_t)SIGNAL_VOLTAGE_1_8) {
        if (gpio_sd_iovdd_sel_state != GPIO_SD_IOVDD_SEL_1_8V) {
            set_vdd_sd_io_state(hc, GPIO_SD_IOVDD_SEL_1_8V);
            (void)delay(5);
            // force ext->signal_voltage to switch to SIGNAL_VOLTAGE_1_8
            hc->signal_voltage = 0;
        }
    }
    else {
        // unknown gpio "SD_IOVDD_SEL" state, do nothing
    }
    return (ext->signal_voltage(hc, signal_voltage));
}

static int32_t bcm2712_sdhci_tune(sdio_hc_t *hc, uint32_t op)
{
    const struct bcm2712_ext_t * const ext = (struct bcm2712_ext_t *)hc->bs_hdl;
    int32_t ret;

    ret = ext->sdhci_tune(hc, op);
    if (ret != EOK) {
        (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: tuning issue, %s", __func__, strerror(ret));
    }

    return ret;
}

/*
 * Pi5 FDT has a non-standard CFG region with offset 0x400 (size=0x200) of host controller
 * The default map size of 4k of host controller should cover this CFG region
 */
static int32_t bcm2712_bs_cfg_init(const sdio_hc_t *hc)
{
    int32_t ret = EOK;
    const sdhci_hc_t *sdhc = (sdhci_hc_t *)hc->cs_hdl;
    if (sdhc == NULL) {
        ret = EFAULT;
    }
    else {
        const uintptr_t base = sdhc->base;
        if ((base == (uintptr_t)MAP_FAILED) || (base == (uintptr_t)NULL)) {
            ret = ENOMEM;
        }
        else {
            uint32_t rd_val, wr_val;
            /*
             * Note:
             *   If driver command argument specifies size less that 0x600, than mmap_device_memory
             *   is needed.
             */
            rd_val  = in32(base + SDIO_CFG_REG_OFFSET + SDIO_CFG_SD_PIN_SEL_REG);
            wr_val = rd_val | SDIO_CFG_SD_PIN_SEL_SD;
            out32(base + SDIO_CFG_REG_OFFSET + SDIO_CFG_SD_PIN_SEL_REG, wr_val);

            /*
             * Tunning support, select the delay line PHY as the clock source.
             */
            rd_val = in32(base + SDIO_CFG_REG_OFFSET + SDIO_CFG_MAX_50MHZ_MODE_REG);
            wr_val = (rd_val & ~SDIO_CFG_MAX_50MHZ_MODE_ENABLE) | SDIO_CFG_MAX_50MHZ_MODE_STRAP_OVERRIDE;
            out32(base + SDIO_CFG_REG_OFFSET + SDIO_CFG_MAX_50MHZ_MODE_REG, wr_val);
        }
    }
    return ret;
}

static int32_t bcm2712_bs_init(sdio_hc_t *hc)
{
    static struct bcm2712_ext_t ext;
    int32_t status = EOK;
    const sdio_hc_cfg_t * const cfg = &hc->cfg;

    hc->bs_hdl = (void *)&ext;

    if (bcm2712_bs_args(hc, cfg->options) != EOK) {
        (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: command argument, %s",
                __func__, strerror(status));
        status = EINVAL;
    }
    else {
        status = sdhci_init(hc);
        if (status == EOK) {
            status = bcm2712_bs_cfg_init(hc);
            if (status == EOK) {
                /* board specific handling of setting voltage and tuning */
                ext.signal_voltage = hc->entry.signal_voltage;
                hc->entry.signal_voltage = bcm2712_signal_voltage;
                ext.sdhci_tune = hc->entry.tune;
                hc->entry.tune = bcm2712_sdhci_tune;

                if (!(hc->caps & HC_CAP_SLOT_TYPE_EMBEDDED)) {
                    /* Overwrite some of the capabilities that are set by sdhci_init() */
                    hc->caps &= ~HC_CAP_CD_INTR;   /* HC doesn't support card detection interrupt */
                }
                (void)sdio_slogf( _SLOGC_SDIODI, _SLOG_INFO, 1, 0, "%s: controller version: 0x%x caps: %lx",
                        __func__, hc->version, hc->caps);
            }
            else {
                (void)sdio_slogf(_SLOGC_SDIODI, _SLOG_ERROR, hc->cfg.verbosity, 0, "%s: configuration, %s",
                        __func__, strerror(status));
            }
        }
    }
    return status;
}

static sdio_product_t sdio_fs_products[] = {
    { .did = SDIO_DEVICE_ID_WILDCARD, .class = 0, .aflags = 0, .name = "bcm2712", .init = bcm2712_bs_init },
};

sdio_vendor_t sdio_vendors[] = {
    { .vid = SDIO_VENDOR_ID_WILDCARD, .name = "Broadcom", .chipsets = sdio_fs_products },
    { .vid = 0, .name = NULL, .chipsets = NULL }
};

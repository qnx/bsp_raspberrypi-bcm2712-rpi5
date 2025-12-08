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
 * Copyright (c) 2024, BlackBerry Limited.
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


#include <dwc_variant.h>

/**
 *  @brief             Set SPI device configuration.
 *  @param hdl         SPI driver handler.
 *  @param spi_dev     SPI device structure pointer.
 *  @param cfg         The spi_cfg_t structure which stores the config info
 *
 *  @return            EOK --success otherwise fail
 */
int dw_setcfg(const void *const hdl, spi_dev_t *spi_dev, const spi_cfg_t *const cfg)
{
    memcpy(&(spi_dev->devinfo.cfg), cfg, sizeof(spi_cfg_t));

    return dw_cfg(hdl, spi_dev);
}

/**
 *  @brief             SPI configuration function.
 *  @param  hdl        SPI driver handler.
 *  @param  spi_dev    SPI device structure.
 *
 *  @return            EOK --success otherwise fail.
 */
int dw_cfg(const void *const hdl, spi_dev_t *spi_dev)
{
    const dw_spi_t *const spi = hdl;
    spi_cfg_t  *cfg = &spi_dev->devinfo.cfg;

    if (spi_dev->devinfo.devno >= (int)spi->cs_max) {
        spi_slogf(_SLOG_ERROR, "%s: devno(%d) should be smaller than %d", __func__, spi_dev->devinfo.devno, spi->cs_max);
        return EINVAL;
    }

    /* Data bits per word, support 4-bit to 32-bit word width */
    const uint32_t nbits = cfg->mode & SPI_MODE_WORD_WIDTH_MASK;
    if ((nbits < 4) || (nbits > 32)) {
        spi_slogf(_SLOG_ERROR, "%s: %d-bit word width is not supported by this controller", __func__, nbits);
        return EINVAL;
    }

    /* Check clock rate */
    if (cfg->clock_rate == 0) {
        spi_slogf(_SLOG_ERROR, "%s: invalid clock rate %u", __func__, cfg->clock_rate);
        return EINVAL;
    }
    if (cfg->clock_rate > spi->bus->input_clk / 2) {
        cfg->clock_rate = (uint32_t) spi->bus->input_clk;
        spi_slogf(_SLOG_WARNING, "%s: Device clock rate (%u) is larger than input clock (%lu). It will be set to (%lu)."
                , __func__, cfg->clock_rate, spi->bus->input_clk, spi->bus->input_clk / 2);
    }

    /* Set SPI_CLK only when it is different than current clock rate */
    if (spi_dev->devinfo.current_clkrate != cfg->clock_rate) {
        /* Calculate the SPI target operational speed */
        uint32_t clock_rate_div = (uint32_t) ((spi->bus->input_clk + cfg->clock_rate - 1) / cfg->clock_rate);
        clock_rate_div = min(clock_rate_div, DW_SCK_DIV_MAX);
        clock_rate_div = max(clock_rate_div, DW_SCK_DIV_MIN);

        /* Update the best rate */
        cfg->clock_rate = (uint32_t) (spi->bus->input_clk / clock_rate_div);

        /* Update current clock rate */
        spi_dev->devinfo.current_clkrate = cfg->clock_rate;

        spi_slogf(_SLOG_DEBUG1, "%s: input_clk=%lu, clock_rate=%u, div=%u", __func__,
                spi->bus->input_clk, cfg->clock_rate, clock_rate_div);
    } else {
        spi_slogf(_SLOG_DEBUG1, "%s: device clock rate is already set to %u", __func__, cfg->clock_rate);
    }

    return EOK;
}

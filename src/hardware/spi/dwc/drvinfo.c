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
 *  @brief             Get SPI driver info function.
 *  @param  hdl        SPI driver handler.
 *  @param  info       The pointer where you want to store drvinfo.
 *
 *  @return            Void.
 */
void dw_drvinfo(const void *const hdl, spi_drvinfo_t *info)
{
    const dw_spi_t  *const spi = hdl;
    const spi_ctrl_t *const spi_ctrl = spi->spi_ctrl;

    info->version = (SPI_VERSION_MAJOR << SPI_VERMAJOR_SHIFT) |
        (SPI_VERSION_MINOR << SPI_VERMINOR_SHIFT) |
        (SPI_REVISION << SPI_VERREV_SHIFT);

    strlcpy(info->name, "Designware SPI", SPI_DRVR_NAME_LEN);
    info->verbosity = spi_ctrl->global.verbosity;
}

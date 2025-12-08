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
 *  @brief             Check if SPI is busy.
 *  @param  spi        SPI driver handler.
 *
 *  @return            True:busy False:Not busy
 *
 */
bool dw_spi_busy(const dw_spi_t *const spi)
{
    const volatile uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_SR);
    const uint32_t value = *addr;
    return ((value & DW_SPI_SR_BUSY) != 0);
}

/**
 *  @brief                   Enable Designware SPI device.
 *  @param  spi              SPI driver handler.
 *  @param  enable_value     Enable value.
 *
 *  @return            Void.
 *
 *  DW_SPI_ENABLE - enable
 *  DW_SPI_DISABLE - disable
 */
void dw_spi_enable(const dw_spi_t *const spi, const uint32_t enable_value)
{
    dw_write32(spi, DW_SPI_SSIENR, enable_value);
}

/**
 *  @brief            De-select SPI slaves.
 *  @param  spi       SPI driver handler.
 *
 *  @return           0:De-select successful;-1:Device busy
 *
 *  De-select all slaves.
 */
int32_t dw_spi_deselect_slave(const dw_spi_t *const spi)
{
    /* check if spi busy */
    if (dw_spi_busy(spi)) {
        return -1;
    }

    dw_write32(spi, DW_SPI_SER, 0);
    return 0;
}

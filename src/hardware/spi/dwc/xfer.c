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
 *  @brief             Reset Designware FIFO by disabling SPI device.
 *  @param  spi        SPI driver handler.
 *
 *  @return            Void.
 *
 *  Reset designware FIFO by disabling SPI device, then enabling the device
 */
static void dw_device_reset(const dw_spi_t *const spi)
{
    dw_spi_enable(spi, DW_SPI_DISABLE);
    dw_spi_enable(spi, DW_SPI_ENABLE);
}

/**
 *  @brief            Select a SPI slave.
 *  @param  spi       SPI driver handler.
 *  @param  slv       SPI slave id.
 *
 *  @return           0:Select successful;-1:Device busy
 *
 *  Sets the bit corresponding to slv to 1.
 */
int32_t dw_spi_select_slave(const dw_spi_t *const spi, const int slv)
{
    /* check if spi busy */
    if (dw_spi_busy(spi)) {
        return -1;
    }

    dw_write32(spi, DW_SPI_SER, (1UL << slv));
    return 0;
}

/**
 *  @brief             Prepare for SPI exchange.
 *  @param spi         SPI driver handler.
 *  @param cfg         The SPI config structure.
 *  @param tnbytes     The number of transmit bytes.
 *  @param rnbytes     The number of receive bytes.
 *
 *  @return            EOK --success otherwise fail.
 */
int dw_prepare_for_transfer(dw_spi_t *spi, const spi_cfg_t *const cfg, const uint32_t tnbytes, const uint32_t rnbytes)
{
    static const uint32_t dscales[] = {0, 0, 1, 2, 2};

    /* Data bits per word */
    const uint32_t nbits = cfg->mode & SPI_MODE_WORD_WIDTH_MASK;
    if ((nbits <= SPI_MIN_WORD_WIDTH) || (nbits > SPI_MAX_WORD_WIDTH)) {
        return EINVAL;
    }

    /* Spi must be disablled to change configuration */
    dw_spi_enable(spi, DW_SPI_DISABLE);

    /* Data bytes per word */
    spi->dlen = (nbits + DW_NBIT_ROUNDER) / 8;
    spi->dscale = dscales[spi->dlen];
    if (spi->dlen == 3) {
        spi->dlen = 4;
    }

    /* Transation data length - word */
    const uint32_t nbytes = max(tnbytes, rnbytes);
    spi->xlen =  nbytes >> spi->dscale;
    if ((spi->xlen <= 0) || (nbytes != (spi->xlen << spi->dscale))) {
        return EINVAL;
    }

    /* Check clock rate */
    if (cfg->clock_rate == 0) {
        spi_slogf(_SLOG_ERROR, "%s: invalid clock rate %u", __func__, cfg->clock_rate);
        return EINVAL;
    }

    spi->cr0 = (DW_SPI_CTRLR0_FRF_MOTO_SPI << DW_SPI_CTRLR0_FRF_OFFSET) | (DW_SPI_CTRLR0_TMOD_TR << DW_SPI_CTRLR0_TMOD_OFFSET);

    /* set nbits value */
    spi->cr0 |= DW_PSSI_CTRLR0_DFS_CONFIG(spi->dlen);

    /* Update cr0 value */
    if ((cfg->mode & SPI_MODE_CPOL_1) != 0) {
        spi->cr0 |= DW_SPI_CTRLR0_SCPOL;    /* Clock Polarity */
    }

    if ((cfg->mode & SPI_MODE_CPHA_1) != 0) {
        spi->cr0 |= DW_SPI_CTRLR0_SCPHA;    /* Clock Phase */
    }

    if (spi->loopback) {
        spi->cr0 |= DW_PSSI_CTRLR0_SRL;    /* Loop-Back Mode */
    }

    if (spi->sste) {
        spi->cr0 |= DW_SPI_SSTE;            /* Slave select Toggle Enable */
    }

    const uint32_t version = dw_read32(spi, DW_SPI_VERSION);
    if (version >= DW_HSSI_102A) {
        spi->cr0 |= (1 << DW_SPI_CTRLR0_IS_MST_OFFSET); /* enable as master */
    }

    dw_write32(spi, DW_SPI_BAUDR, (uint32_t)(spi->bus->input_clk / cfg->clock_rate));
    dw_write32(spi, DW_SPI_CTRLR0, spi->cr0);
    dw_write32(spi, DW_SPI_CTRLR1, 0);

    /* Calculate the transaction time, minimal 1 us */
    spi->xtime_us = max(1, (uint64_t)nbits * 1000 * 1000 * spi->xlen / cfg->clock_rate);

    /* Reset RX and TX length */
    spi->rlen = 0;
    spi->tlen = 0;

    return EOK;
}

/**
 *  @brief             SPI transfer function.
 *  @param hdl         SPI driver handler.
 *  @param spi_dev     SPI device structure pointer.
 *  @param buf         The buffer which stores the transfer data.
 *  @param tnbytes     The number of transmit bytes.
 *  @param rnbytes     The number of receive bytes.
 *
 *  @return            EOK --success otherwise fail.
 */
int  dw_xfer(void *const hdl, spi_dev_t *const spi_dev, uint8_t *const buf, const uint32_t tnbytes, const uint32_t rnbytes)
{
    dw_spi_t             *spi = hdl;
    const spi_cfg_t *const  cfg = &spi_dev->devinfo.cfg;
    int                     status = EOK;

#ifdef  DW_DMA
    spi->dma_active = false;
#endif

    /* Module busy */
    if (dw_spi_busy(spi) == true) {
        spi_slogf(_SLOG_ERROR, "%s: Controller is busy", __func__);
        return EBUSY;
    }

    spi->pbuf = buf;

    /* Configure SPI device */
    status = dw_prepare_for_transfer(spi, cfg, tnbytes, rnbytes);
    if (status != EOK) {
        spi_slogf(_SLOG_ERROR, "%s: dw_prepare_for_transfer failed", __func__);
        return status;
    }

    /* Disable DMA transaction on SPI when non-DMA and DMA co-exits */
    dw_write32(spi, DW_SPI_DMACR, 0);

    /* Enable FIFO Interrupts and SPI */
    dw_device_reset(spi);

    /* Prefill transmit FIFO */
    spi_slogf(_SLOG_DEBUG1, "%s: Starting prefill transmit FIFO", __func__);

    const uint32_t len = min(spi->xlen, spi->fifo_len / SPI_FIFO_LEN_DIV);
    dw_write_fifo(spi, len);
    spi->tlen += len;
    const uint32_t thresh = len;

    if (spi->tlen < spi->xlen) {
        dw_write32(spi, DW_SPI_IMR, DW_SPI_INT_MASK);
        /* When the number of receive FIFO entries is greater than
           or equal to this value + 1, the receive FIFO full interrupt
           is triggered.*/
        dw_write32(spi, DW_SPI_RXFTLR, (thresh - 1U));
        /* When the number of transmit FIFO entries is less than or
           equal to this value, the transmit FIFO empty interrupt is
           triggered. */
        dw_write32(spi, DW_SPI_TXFTLR, thresh);
    } else {
        dw_write32(spi, DW_SPI_IMR, DW_SPI_INT_RXUI
                | DW_SPI_INT_RXOI | DW_SPI_INT_RXFI);
        dw_write32(spi, DW_SPI_RXFTLR,  (thresh - 1U));
    }

    dw_spi_select_slave(spi, spi_dev->devinfo.devno);

    /* Wait for exchange to finish */
    status = dw_wait(spi);
    if (status != EOK) {
        spi_slogf(_SLOG_ERROR, "%s: xfer timeout: xlen = %u, tlen = %u, rlen = %u xtime = %lu us",
                __func__, spi->xlen, spi->tlen, spi->rlen, spi->xtime_us);
    }

    dw_spi_enable(spi, DW_SPI_DISABLE);

    return status;
}

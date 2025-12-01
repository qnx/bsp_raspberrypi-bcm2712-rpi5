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

enum {
    OPTION_CS_MAX,
    OPTION_SSTE,
    OPTION_LOOPBACK,
    OPTION_RX_CHANNEL,
    OPTION_TX_CHANNEL,
    NUM_OPTIONS
};

/**
 *  @brief            Get FIFO Length.
 *  @param  spi       SPI driver handler.
 *
 *  @return           FIFO length
 *
 *  calculate SPI FIFO length using FIFO threshold method
 *  If you attempt to set bits [7:0] of this register to
 *  a value greater than or equal to the depth of the FIFO,
 *  this field is not written and retains its current value.
 *
 */
static uint32_t dw_spi_get_fifo_len(const dw_spi_t *const spi)
{
    uint32_t thr_lev_hold, left, right, i;

    thr_lev_hold = dw_read32(spi, DW_SPI_TXFTLR);

    if (thr_lev_hold != 0) {
        left = thr_lev_hold;
    } else {
        left = DW_SPI_MIN_FIFO_LENGTH;
    }
    right = DW_SPI_MAX_FIFO_LENGTH + 1;

    for (i = left; i <= right; i++) {
        dw_write32(spi, DW_SPI_TXFTLR, i);
        if (dw_read32(spi, DW_SPI_TXFTLR) != i) {
            break;
        }
    }
    dw_write32(spi, DW_SPI_TXFTLR, thr_lev_hold); /* restore old fifo threshold */

    return (i);
}

/**
 *  @brief             Attach SPI interrupts.
 *  @param  bus        The SPI bus structure.
 *
 *  @return            EOK --success otherwise fail.
 */
static int dw_attach_intr(dw_spi_t *spi) {
    const spi_bus_t *const bus = spi->bus;

    /*
     * Attach SPI interrupt
     */
    spi->iid = InterruptAttachEvent(bus->irq, &bus->evt,
            _NTO_INTR_FLAGS_TRK_MSK);
    if (spi->iid == -1) {
        spi_slogf(_SLOG_ERROR, "%s: InterruptAttachEvent failed: %s", __func__,
                strerror(errno));
        return errno;
    } else {
        spi_slogf(_SLOG_DEBUG2, "%s: InterruptAttachEvent succeeded: irq %x",
                __func__, bus->irq);
    }

    return EOK;
}

/**
 *  @brief             Process board specific options which is set in SPI config file.
 *  @param  spi        SPI driver handler.
 *  @param  options    Board specific options.
 *
 *  @return            EOK --success otherwise fail.
 */
static int dw_process_args(dw_spi_t *spi, char *options)
{
    char            *value;
    int             opt = 0;
    const char      *c;
    char            *dw_opts[] = {
        [OPTION_CS_MAX]             = "cs_max",             /* Chip select max */
        [OPTION_SSTE]               = "sste",               /* Slave select Toggle enable */
        [OPTION_LOOPBACK]           = "loopback",           /* Loopback interface for testing */
#ifdef  DW_DMA
        [OPTION_RX_CHANNEL]         = "dma_rx_chan",        /* Receive channel ID for DMA */
        [OPTION_TX_CHANNEL]         = "dma_tx_chan",        /* Transmit channel ID for DMA */
#endif
        [NUM_OPTIONS]               = NULL
    };

    if (options == NULL) {
        spi_slogf(_SLOG_WARNING, "%s: no board specific args passed", __func__);
        return EOK;
    }

    while ((options != NULL) && (*options != '\0')) {
        c = options;
        opt = getsubopt(&options, dw_opts, &value);
        if (opt == -1) {
            spi_slogf(_SLOG_ERROR, "%s: unsupported SPI device driver args: %s", __func__, c);
            return EINVAL;
        }

        switch (opt) {
            case OPTION_CS_MAX:
                if (value) {
                    spi->cs_max = (uint32_t) strtoul(value, NULL, 0);

                    if ((spi->cs_max < 1) || (spi->cs_max > 32)) {
                        spi_slogf(_SLOG_ERROR, "%s: Invalid cs_max value: %u", __func__,
                                spi->cs_max);
                        return EINVAL;
                    }
                    spi_slogf(_SLOG_DEBUG2, "%s: cs_max = %u", __func__,
                            spi->cs_max);
                } else {
                    spi_slogf(_SLOG_ERROR, "%s: no cs_max value provided",
                                                __func__);
                    return EINVAL;
                }
                break;
            case OPTION_SSTE:
                spi->sste = true;
                spi_slogf(_SLOG_DEBUG2, "%s: SSTE is enabled", __func__);
                break;
            case OPTION_LOOPBACK:
                spi->loopback = true; /* Enable loopback mode, used for testing */
                break;
#ifdef  DW_DMA
            case OPTION_RX_CHANNEL:
                spi->dma.rx_channel = (uint32_t)strtoul(value, NULL, 0);
                break;
            case OPTION_TX_CHANNEL:
                spi->dma.tx_channel = (uint32_t)strtoul(value, NULL, 0);
                break;
#endif
            default:
                spi_slogf(_SLOG_ERROR, "%s: wrong option", __func__);
                return EINVAL;
        }
    }
    return EOK;
}

/**
 *  @brief             Get and map SPI controller base address, and initialize defaults
 *  @param  spi        SPI driver handler.
 *
 *  @return            EOK --success otherwise fail.
 */
static int map_in_device_registers(dw_spi_t *spi)
{
    const void *const hw_addr = mmap_device_memory(NULL, spi->map_size, PROT_READ|PROT_WRITE|PROT_NOCACHE, MAP_SHARED, spi->pbase);
    if (hw_addr == MAP_FAILED) {
        spi_slogf(_SLOG_ERROR, "%s: mmap_device_memory failed pbase:%x, map size: %d", __func__, spi->pbase, spi->map_size);
        return ENOMEM;
    }

    spi->vbase    = (uintptr_t)hw_addr;

    return EOK;
}

/**
 *  @brief             Initialize SPI controller and devices.
 *  @param  spi        SPI driver handler.
 *
 *  @return            EOK --success otherwise fail.
 */
static int dw_init_device(dw_spi_t *const spi)
{
    int                 status;
    spi_bus_t *const    bus = spi->bus;
    spi_dev_t          *spi_dev = bus->devlist;

    /*
     * Initial device configuration with defaults from config file
     */
    while (spi_dev != NULL) {
        status = dw_cfg(spi, spi_dev);
        if (status != EOK) {
            spi_slogf(_SLOG_ERROR, "%s: dw_cfg failed", __func__);
            return status;
        }
        spi_dev = spi_dev->next;
    }

    dw_read32(spi, DW_SPI_ICR);
    dw_write32(spi, DW_SPI_IMR, DW_SPI_INT_MASK);
    dw_write32(spi, DW_SPI_CTRLR1, 0);
    dw_spi_deselect_slave(spi);

    /* get fifo size */
    spi->fifo_len = dw_spi_get_fifo_len(spi);

#ifdef  DW_DMA
    if (bus->funcs->dma_xfer != NULL) {
        /* check valid dma threshold */
        if (spi->bus->dma_thld > spi->fifo_len) {
            spi_slogf(_SLOG_WARNING, "%s: SPI DMA threshold %d is greater than fifo len %d on bus.", __func__, spi->bus->dma_thld, spi->fifo_len);
            spi->bus->dma_thld = (uint8_t)(spi->fifo_len / SPI_FIFO_LEN_DIV);
            spi_slogf(_SLOG_INFO, "%s: SPI DMA threshold will be configured as %d on bus.", __func__, spi->bus->dma_thld);
        }

        /* Init DMA */
        status = dw_spi_init_dma(spi);
        if (status != EOK) {
            spi_slogf(_SLOG_ERROR, "%s: dw_spi_init_dma failed", __func__);
        }
    }
#endif

    /* Attach SPI interrupt */
    status = dw_attach_intr(spi);
    if (status != EOK) {
        spi_slogf(_SLOG_ERROR, "%s: dw_attach_intr failed", __func__);
    }

    return status;
}

/**
 *  @brief             SPI initialization.
 *  @param  bus        The SPI bus structure.
 *
 *  @return            EOK --success otherwise fail.
 */
int spi_init(spi_bus_t *bus)
{
    dw_spi_t *spi = NULL;
    int       status = EOK;

    if (bus == NULL) {
        spi_slogf(_SLOG_ERROR, "%s: no SPI bus structure !", __func__);
        return EINVAL;
    }

    spi = calloc(1, sizeof(dw_spi_t));
    if (spi == NULL) {
        spi_slogf(_SLOG_ERROR, "%s: failed to allocate memory !", __func__);
        return ENOMEM;
    }

    /* Save spi_ctrl to driver structure */
    spi->spi_ctrl = bus->spi_ctrl;
    spi->bus      = bus;

    /* Get other SPI driver functions */
    bus->funcs->spi_fini        = dw_fini;
    bus->funcs->drvinfo         = dw_drvinfo;
    bus->funcs->devinfo         = dw_devinfo;
    bus->funcs->setcfg          = dw_setcfg;
    bus->funcs->xfer            = dw_xfer;
    bus->funcs->dma_xfer        = NULL;
    bus->funcs->dma_allocbuf    = NULL;
    bus->funcs->dma_freebuf     = NULL;

    /* Set default value */
    spi->iid          = -1;
    spi->vbase        = (uintptr_t)MAP_FAILED;

    /* Set the default value of the SPI device */
    spi->loopback          = false;
    spi->pbase             = bus->pbase;
    spi->map_size          = SPI0_SIZE;
    spi->cs_max            = SPI_CS_MAX_DEF;
    spi->sste              = false;
#ifdef  DW_DMA
    spi->dma.rx_channel    = CHANNEL_ID_MAX;
    spi->dma.tx_channel    = CHANNEL_ID_MAX;
#endif

    /* Process args, override the defaults */
    if (bus->bs != NULL) {
        status = dw_process_args(spi, bus->bs);
        if (status != EOK) {
            spi_slogf(_SLOG_ERROR, "%s: dw_process_args failed", __func__);
            dw_fini(spi);
            return status;
        }
    }

#ifdef  DW_DMA
    if ((spi->dma.tx_channel != CHANNEL_ID_MAX) && (spi->dma.rx_channel != CHANNEL_ID_MAX)) {
        bus->funcs->dma_xfer        = dw_spi_dmaxfer;
        bus->funcs->dma_allocbuf    = dw_spi_dma_allocbuf;
        bus->funcs->dma_freebuf     = dw_spi_dma_freebuf;
    }
#endif

    /* Check the device interrupt number */
    if (spi->bus->irq <= 0) {
        spi_slogf(_SLOG_ERROR, "%s: Invalid IRQ: %x", __func__, spi->bus->irq);
        return EINVAL;
    }

    /*map SPI controller base address */
    status = map_in_device_registers(spi);
    if (status != EOK) {
        spi_slogf(_SLOG_ERROR, "%s: map_in_device_registers failed", __func__);
        dw_fini(spi);
        return status;
    }

    /* Disable spi to set controls */
    dw_spi_enable(spi, DW_SPI_DISABLE);

    /* Init SPI device */
    status = dw_init_device(spi);
    if (status != EOK) {
        dw_fini(spi);
        return status;
    }
    spi_slogf(_SLOG_DEBUG2, "%s: dw_init_device succeeded",  __func__);

    /*
     * Create SPI chip select devices
     */
    status = spi_create_devs(bus->devlist);
    if (status != EOK) {
        spi_slogf(_SLOG_ERROR, "%s: Disabling SPI device", __func__, bus->devlist->devinfo.name);
        dw_fini(spi);
        return status;
    }

    /* Save driver structure to drvhdl */
    bus->drvhdl = spi;

    return status;
}

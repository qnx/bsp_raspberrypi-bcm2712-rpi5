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
 *  @brief             Read SPI FIFO for 8 bit per word mode.
 *  @param  spi        SPI driver handler.
 *  @param  buffer     Buffer to receive data.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static inline void dw_read_fifo_uint8(const dw_spi_t *const spi, uint8_t *const buffer, const uint32_t len)
{
    register uint32_t i;
    volatile const uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_DR);
    for (i = 0; i < len; i++) {
        buffer[i] = (uint8_t) *addr;
    }
}

/**
 *  @brief             Write SPI FIFO for 8 bit per word mode.
 *  @param  spi        SPI driver handler.
 *  @param  buffer     Buffer containing data.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static inline void dw_write_fifo_uint8(const dw_spi_t *const spi, const uint8_t *const buffer, const uint32_t len)
{
    register size_t i;
    volatile uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_DR);
    for (i = 0; i < len; i++) {
        *addr = buffer[i];
    }
}

/**
 *  @brief             Read SPI FIFO for 16 bit per word mode.
 *  @param  spi        SPI driver handler.
 *  @param  buffer     Buffer to receive data.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static inline void dw_read_fifo_uint16(const dw_spi_t *const spi, uint16_t *const buffer, const uint32_t len)
{
    register size_t i;
    volatile const uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_DR);
    for (i = 0; i < len; i++) {
        buffer[i] = (uint16_t) *addr;
    }
}

/**
 *  @brief             Write SPI FIFO for 16 bit per word mode.
 *  @param  spi        SPI driver handler.
 *  @param  buffer     Buffer containing data.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static inline void dw_write_fifo_uint16(const dw_spi_t *const spi, const uint16_t *const buffer, const uint32_t len)
{
    register size_t i;
    volatile uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_DR);
    for (i = 0; i < len; i++) {
        *addr = buffer[i];
    }
}

/**
 *  @brief             Read SPI FIFO for 32 bit per word mode.
 *  @param  spi        SPI driver handler.
 *  @param  buffer     Buffer to receive data.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static inline void dw_read_fifo_uint32(const dw_spi_t *const spi, uint32_t *const buffer, const uint32_t len)
{
    register size_t i;
    volatile const uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_DR);
    for (i = 0; i < len; i++) {
        buffer[i] = *addr;
    }
}

/**
 *  @brief             Write SPI FIFO for 32 bit per word mode.
 *  @param  spi        SPI driver handler.
 *  @param  buffer     Buffer containing data.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static inline void dw_write_fifo_uint32(const dw_spi_t *const spi, const uint32_t *const buffer, const uint32_t len)
{
    register size_t i;
    volatile uint32_t *const addr = (uint32_t*)(spi->vbase + DW_SPI_DR);
    for (i = 0; i < len; i++) {
        *addr = buffer[i];
    }
}

/**
 *  @brief             Write SPI FIFO.
 *  @param  spi        SPI driver handler.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
void dw_write_fifo(const dw_spi_t *const spi, const uint32_t len) {
    switch (spi->dscale) {
        case 0:
            dw_write_fifo_uint8(spi, ((uint8_t*) spi->pbuf) + spi->tlen, len);
            spi_slogf(_SLOG_DEBUG3, "%s: 8 bit %u", __func__, spi->dscale);
            break;
        case 1:
            dw_write_fifo_uint16(spi, ((uint16_t*) spi->pbuf) + spi->tlen, len);
            spi_slogf(_SLOG_DEBUG3, "%s: 16 bit %u", __func__, spi->dscale);
            break;
        case 2:
            dw_write_fifo_uint32(spi, ((uint32_t*) spi->pbuf) + spi->tlen, len);
            spi_slogf(_SLOG_DEBUG3, "%s: 32 bit %u", __func__, spi->dscale);
            break;
        default:
            spi_slogf(_SLOG_ERROR, "%s: invalid dscale %u", __func__, spi->dscale);
            break;
    }
}

/**
 *  @brief             Read SPI FIFO.
 *  @param  spi        SPI driver handler.
 *  @param  len        Data length in word.
 *
 *  @return            Void.
 */
static void dw_read_fifo(const dw_spi_t *const spi, const uint32_t len) {
    switch (spi->dscale) {
        case 0:
            dw_read_fifo_uint8(spi, ((uint8_t*) spi->pbuf) + spi->rlen, len);
            break;
        case 1:
            dw_read_fifo_uint16(spi, ((uint16_t*) spi->pbuf) + spi->rlen, len);
            break;
        case 2:
            dw_read_fifo_uint32(spi, ((uint32_t*) spi->pbuf) + spi->rlen, len);
            break;
        default:
            spi_slogf(_SLOG_ERROR, "%s: invalid dscale %u", __func__, spi->dscale);
            break;
    }
}

/**
 *  @brief             Process SPI interrupts.
 *  @param  bus        The SPI bus structure
 *
 *  @return            0: Transfer complete; 1: Transfer not complete; otherwise fail.
 */
static int dw_process_interrupts(dw_spi_t *spi) {
    uint32_t len;

    /* Get interrupt status */
    const uint32_t value = dw_read32(spi, DW_SPI_SR);

    // Check Interrupt valid
    if ((value & DW_SPI_SR_TX_ERR) != 0) {
        /* Disable SPI and mask interrupts */
        dw_spi_enable(spi, DW_SPI_DISABLE);
        dw_write32(spi, DW_SPI_IMR, 0);

        spi_slogf(_SLOG_ERROR, "%s: Transmit error", __func__);
        return SPI_INTR_ERR;
    }
    if ((value & DW_SPI_SR_DCOL) != 0) {
        /* Disable SPI and mask interrupts */
        dw_spi_enable(spi, DW_SPI_DISABLE);
        dw_write32(spi, DW_SPI_IMR, 0);

        spi_slogf(_SLOG_ERROR, "%s: Data collision error", __func__);
        return SPI_INTR_ERR;
    }

    spi_slogf(_SLOG_DEBUG3, "%s: Starting interrupt routine read",
            __func__);
    len = dw_read32(spi, DW_SPI_RXFLR);
    dw_read_fifo(spi, len);
    spi->rlen += len;

    if (spi->rlen == spi->xlen) {
        /* Disable SPI and mask interrupts */
        dw_spi_enable(spi, DW_SPI_DISABLE);
        dw_write32(spi, DW_SPI_IMR, 0);

        /* There's no more data to send, Exchange done */
        return SPI_INTR_DONE;
    }

    if (spi->tlen < spi->xlen) {
        //Get size of transmit
        len = min(len, spi->xlen - spi->tlen);
        spi_slogf(_SLOG_DEBUG3, "%s:Size of fifo: %u, Transmit transaction left: %u",
                __func__, spi->fifo_len, spi->xlen - spi->tlen);

        dw_write_fifo(spi, len);
        spi->tlen += len;
    }

    if (spi->tlen >= spi->xlen) {
        const uint32_t im = dw_read32(spi, DW_SPI_IMR);
        dw_write32(spi, DW_SPI_IMR, im & ~DW_SPI_INT_TXEI & ~DW_SPI_INT_TXOI);
        const uint32_t minlen = min(spi->xlen - spi->rlen, spi->fifo_len / SPI_FIFO_LEN_DIV);
        const uint32_t thresh = minlen - 1U;
        /* When the number of receive FIFO entries is greater than
           or equal to this value + 1, the receive FIFO full interrupt
           is triggered.*/
        dw_write32(spi, DW_SPI_RXFTLR,  thresh);
    }


    /* Transfer not complete */
    /* Don't unblock xfer function until complete */
    return SPI_INTR_CONTINUE;
}

/**
 *  @brief             Wait for SPI transfer complete.
 *  @param  spi        SPI driver handler.
 *
 *  @return            EOK --success otherwise fail.
 */
int dw_wait(dw_spi_t *const spi)
{
    struct timespec     timeout;
    const uint64_t      ns_timeout = spi->xtime_us * 50 * 1000UL;    /* timeout in ns. 50 times for transation time */
    spi_bus_t *const    bus = spi->bus;
    int                 status;
#ifdef DW_DMA
    dw_dma_t *const     dma = &spi->dma;
#endif

    nsec2timespec(&timeout, clock_gettime_mon_ns() + ns_timeout);

    spi_slogf(_SLOG_DEBUG2, "%s: Starting wait function", __func__);
    while (1) {
        /* The sem_timedwait_monotonic() function decrements the semaphore on successful completion */
        status = sem_timedwait_monotonic(bus->sem, &timeout);

        if (status == -1) {
            spi_slogf(_SLOG_ERROR, "%s: sem_timedwait_monotonic failed: %s", __func__, strerror(errno));
            status = errno;
#ifdef DW_DMA
            if (spi->dma_active) {
                dma->dma_funcs.xfer_abort(dma->rx_ch_handle);
                dma->dma_funcs.xfer_abort(dma->tx_ch_handle);
            }
#endif
            break;
        } else {
#ifdef DW_DMA
            if (spi->dma_active) {
                spi_slogf(_SLOG_DEBUG2, "%s: DMA xfer is done", __func__);

                spi->rlen = spi->xlen - dma->dma_funcs.bytes_left(dma->rx_ch_handle);

                /* xfer_complete is used by some DMA lib to clear INT flags.
                 * It returns EOK on success, -1 on failure. Errors are logged in DMA lib.
                 * */
                status = dma->dma_funcs.xfer_complete(dma->rx_ch_handle);
                if (status == -1) {
                    spi_slogf(_SLOG_ERROR, "%s: RX xfer_complete with an error", __func__);
                    status = EIO;
                    break;
                }

                status = dma->dma_funcs.xfer_complete(dma->tx_ch_handle);
                if (status == -1) {
                    spi_slogf(_SLOG_ERROR, "%s: TX xfer_complete with an error", __func__);
                    status = EIO;
                    break;
                }
                break;
            }
            else
#endif
            {
                spi_slogf(_SLOG_DEBUG3, "%s: Processing interrupts", __func__);
                /* Received SPI interrupt event */
                status = dw_process_interrupts(spi);
                InterruptUnmask(spi->bus->irq, spi->iid);

                if (status == SPI_INTR_CONTINUE) {
                    spi_slogf(_SLOG_DEBUG3, "%s: SPI transfer continue ...", __func__);
                } else if (status == SPI_INTR_DONE) {
                    /* Transfer is completed */
                    spi_slogf(_SLOG_DEBUG2, "%s: SPI transfer is completed", __func__);
                    status = EOK;
                    break;
                } else {
                    /* Transfer failed */
                    spi_slogf(_SLOG_ERROR, "%s: SPI transfer failed", __func__);
                    status = EIO;
                    break;
                }
            }
        }
    }

    return status;
}

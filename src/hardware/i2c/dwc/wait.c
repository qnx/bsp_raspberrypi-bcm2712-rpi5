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
 * Copyright (c) 2015, 2022, 2024, BlackBerry Limited.
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
 */


#include "proto.h"
#include <assert.h>
#include <string.h>
#include <atomic.h>

#define RESET_RETRY (3)
#define DW_I2C_TIMEOUT  20

int32_t dwc_i2c_wait_bus_not_busy(dwc_dev_t* const dev)
{
    int32_t timeout = DW_I2C_TIMEOUT * 10;

    while ((i2c_reg_read32(dev, DW_IC_STATUS) & DW_IC_STATUS_ACTIVITY) != 0U) {
        if (timeout <= 0) {
            logerr("timeout waiting for bus ready\n");
            return -ETIMEDOUT;
        }
        timeout--;
        (void)usleep(100);
    }

    return 0;
}

static int32_t dwc_i2c_process_intr(dwc_dev_t* const dev)
{
    uint32_t    status;
    uint32_t    numbers, reg;

    status = i2c_reg_read32(dev, DW_IC_INTR_STAT);

    if ((status & DW_IC_INTR_TX_ABRT) != 0U) {
        /* Get the Abort source and clean the ABRT interrupt bit */
        dev->abort_source = i2c_reg_read32(dev, DW_IC_TX_ABRT_SOURCE);
        (void)i2c_reg_read32(dev, DW_IC_CLR_TX_ABRT);

        /* Set status */
        if ((dev->abort_source & DW_IC_TX_ARBT_LOST) != 0U) {
            dev->status = (uint32_t)I2C_STATUS_DONE | (uint32_t)I2C_STATUS_ARBL;
        }
        else if ((dev->abort_source & DW_IC_TX_ABRT_ADDR_NOACK) != 0U) {
            dev->status = (uint32_t)I2C_STATUS_DONE | (uint32_t)I2C_STATUS_ADDR_NACK;
        }
        else if ((dev->abort_source & DW_IC_TX_ABRT_TXDATA_NOACK) != 0U) {
            dev->status = (uint32_t)I2C_STATUS_DONE | (uint32_t)I2C_STATUS_DATA_NACK;
        }
        else {
            dev->status = (uint32_t)I2C_STATUS_DONE | (uint32_t)I2C_STATUS_ABORT;
        }

        /* Disable interrupt and return interrupt event */
        i2c_reg_write32(dev, DW_IC_INTR_MASK, 0);
        return EOK;
    }

    if ((status & DW_IC_INTR_STOP_DET) != 0U) {
        /* Clean the STOP_DET interrupt */
        (void)i2c_reg_read32(dev, DW_IC_CLR_STOP_DET);

        /* Read Rx FIFO if required */
        if (dev->rdlen < dev->rxlen) {
            /* Check the received data length */
            numbers = i2c_reg_read32(dev, DW_IC_RXFLR);
            if ((dev->rdlen + numbers) != dev->rxlen) {
                /* Wrong Received Rx length */
                dev->status = (uint32_t)I2C_STATUS_ERROR;
                numbers = 0;
            }

            for ( ; numbers > 0U; numbers--) {
                reg = i2c_reg_read32(dev, DW_IC_DATA_CMD);
                *dev->rxbuf++ = (uint8_t)(reg & 0x00FFU);
                dev->rdlen++;
            }
        }

        /* Disable interrupt and return interrupt event */
        dev->status |= (uint32_t)I2C_STATUS_DONE;
        i2c_reg_write32(dev, DW_IC_INTR_MASK, 0);
        return EOK;
    }

    if ((status & DW_IC_INTR_RX_FULL) != 0U) {
        /* Note: this bit will be cleaned only when the value of DW_IC_RXFLR less than the value of DW_IC_RX_TL */
        if (dev->rdlen < dev->rxlen) {
            /* How many valid data entries in the receive FIFO */
            numbers = i2c_reg_read32(dev, DW_IC_RXFLR);

            /* Check the received data length */
            if ((dev->rdlen + numbers) > dev->rxlen) {
                /* Unexpected received length */
                numbers = dev->rxlen - dev->rdlen;
                dev->status |= (uint32_t)I2C_STATUS_ERROR;

                /* Disable DW_IC_INTR_RX_FULL interrupt */
                reg = i2c_reg_read32(dev, DW_IC_INTR_MASK) & ~DW_IC_INTR_RX_FULL;
                i2c_reg_write32(dev, DW_IC_INTR_MASK, reg);
            }

            for ( ; numbers > 0U; numbers--) {
                reg = i2c_reg_read32(dev, DW_IC_DATA_CMD);
                *dev->rxbuf++ = (uint8_t)(reg & 0x00FFU);
                dev->rdlen++;
            }
        }
        else {
            /* Disable DW_IC_INTR_RX_FULL interrupt, it should never go here */
            reg = i2c_reg_read32(dev, DW_IC_INTR_MASK) & ~DW_IC_INTR_RX_FULL;
            i2c_reg_write32(dev, DW_IC_INTR_MASK, reg);
        }
    }

    if ((status & DW_IC_INTR_TX_EMPTY) != 0U) {
        /* Note: this bit should been cleaned only when the value of DW_IC_TXFLR large than the value of DW_IC_TX_TL */
        for (uint32_t i = 0; i < dev->fifo_depth; i++) {
            if (dev->wrlen >= dev->xlen) {
                /* No more command left, disable R_TX_EMPTY interrupt */
                reg = i2c_reg_read32(dev, DW_IC_INTR_MASK) & ~DW_IC_INTR_TX_EMPTY;
                i2c_reg_write32(dev, DW_IC_INTR_MASK, reg);
                break;
            }

            if (xmit_FIFO_is_full(dev)) {
                /* TXFIFO FULL */
                break;
            }

            if (dev->wrlen < dev->txlen) {
                /* Command for master-transmit */
                if (dev->wrlen == (dev->xlen - 1U)) {
                    /* If last byte for master-transmit, force set STOP condition */
                    reg = (uint32_t)*dev->txbuf++ | DW_IC_DATA_CMD_WRITE | DW_IC_DATA_CMD_STOP;
                }
                else {
                    reg = (uint32_t)*dev->txbuf++ | DW_IC_DATA_CMD_WRITE;
                }
            }
            else if ((dev->wrlen == dev->txlen) && (0U != dev->txlen)) {
                /* First command for master-receive of isendrecv , need restart cmd */
                if (dev->wrlen == (dev->xlen - 1U)) {
                    /* If last byte for master-receive, force set STOP condition */
                    reg = DW_IC_DATA_CMD_READ | DW_IC_DATA_CMD_RESTART | DW_IC_DATA_CMD_STOP;
                }
                else {
                    reg = DW_IC_DATA_CMD_READ | DW_IC_DATA_CMD_RESTART;
                }
            }
            else {
                /* Command for master-receive */
                if (dev->wrlen == (dev->xlen - 1U)) {
                    /* If last byte for master-receive, force set STOP condition */
                    reg = DW_IC_DATA_CMD_READ | DW_IC_DATA_CMD_STOP;
                }
                else {
                    reg = DW_IC_DATA_CMD_READ;
                }
            }

            i2c_reg_write32(dev, DW_IC_DATA_CMD, reg);
            dev->wrlen++;
        }
    }

    return EAGAIN;
}

void dwc_i2c_reset(dwc_dev_t *const dev)
{
    int32_t ret;
    int32_t reset_retry = RESET_RETRY;

    /* Abort the I2C transfer */
    i2c_reg_write32(dev, DW_IC_ENABLE, DW_IC_ENABLE_ABORT | DW_IC_ENABLE_STATUS_ENABLE);

    (void)usleep(1000);

    /* Disable the adapter */
    (void)dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_DISABLE);

    do {

        /* Perform soft reset.
         * Notes:
         * If the driver isn’t using DMA, then it is OK to just leave iDMA in Reset.
         * To be 100% sure the DMA is not being used, read register BAR0 + 0xB98
         * (DMACFGREG) bit[0], which is the DMA enable bit. If it is '0', then no
         * need to worry about any pending transactions.
         * At the time this soft reset was added, DMACFGREG is 0. So DMA is NOT being
         * used.
         * Additionally, in case DMA support is added in the future, there is a need
         * to check whether or not DMA transactions need to be stopped before performing
         * soft reset.
         */

        logfyi("Assert soft reset. retry=%d\n", reset_retry);
        i2c_reg_write32(dev, DW_IC_SOFT_RESET, DW_IC_SOFT_RESET_ASSERTED);
        (void)usleep(25); //Just miniscule delay

        logfyi("De-assert soft reset.\n");
        i2c_reg_write32(dev, DW_IC_SOFT_RESET, DW_IC_SOFT_RESET_NOT_ASSERTED);

        logfyi("Re-initializing...");
        ret = dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_DISABLE);
        if (ret == 0) {
            dwc_i2c_init_registers(dev);
            logfyi("I2C reinitialized.\n");
            return;
        }
        else {
            logerr("Failed to reinitialize!\n");
        }
    } while (--reset_retry > 0);

    logerr("I2C failed to reset!\n");
}

i2c_status_t dwc_i2c_wait_complete(dwc_dev_t* const dev)
{
    int32_t err;

    // Estimate transfer time in us... The calculated time is only used for
    // the timeout, so it doesn't have to be that accurate.  At higher clock
    // rates, a calcuated time of 0 would mess-up the timeout calculation, so
    // round up to 1 us per byte, and wait at least 2.5 ms for the xfer to complete.
    const uint32_t xtime_us = max(2500U, max(1U, 10U * 1000U * 1000U / dev->scl_freq) * dev->xlen);

    const uint64_t to_ns = (uint64_t)xtime_us * 1000U * 50U;  // convert to ns and extend to 50 times of estimate time

    while (true) {
        (void)TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &to_ns, NULL);

        err = InterruptWait(0, NULL);

        if (err == -1) {
            logerr("%s: PID_%d %s(%d), stat reg %x\n", __func__, getpid(),
                    strerror(errno), errno, i2c_reg_read32(dev, DW_IC_STATUS));
            dwc_i2c_reset(dev);
            break;
        }

        if (dwc_i2c_process_intr(dev) == EAGAIN) {
            (void)InterruptUnmask(dev->irq, dev->iid);
            continue;
        }

        if ((dev->status & ((uint32_t)I2C_STATUS_ARBL | (uint32_t)I2C_STATUS_ERROR)) != 0U) {
            dwc_i2c_reset(dev);
        }

        /* Disabled interrupts */
        i2c_reg_write32(dev, DW_IC_INTR_MASK, 0);

        (void)InterruptUnmask(dev->irq, dev->iid);

        return (i2c_status_t)dev->status;
    }

    return I2C_STATUS_BUSY;
}

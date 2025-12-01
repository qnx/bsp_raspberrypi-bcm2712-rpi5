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


i2c_status_t dwc_i2c_send(void *hdl, void *buf, uint32_t len, uint32_t stop)
{
    dwc_dev_t  *dev = hdl;
    i2c_status_t    ret = I2C_STATUS_ERROR;

    (void)stop;

    if (len <= 0U) {
        return I2C_STATUS_DONE;
    }

    dev->txlen  = len;                      // how many bytes for slave transmit (isend)
    dev->rxlen  = 0;                        // how many bytes for slave receive (irecv)
    dev->xlen   = dev->txlen + dev->rxlen;  // how many bytes for total transaction (isend and irecv)
    dev->wrlen  = 0;                        // how many cmds have been write to TxFIFO
    dev->rdlen  = 0;                        // how many data have been read from RxFIFO
    dev->txbuf  = buf;
    dev->status = 0;

    /* Active I2C bus */
    if (0 != dwc_i2c_bus_active(dev)) {
        return I2C_STATUS_BUSY;
    }

    /* Clear and enable interrupts */
    (void)i2c_reg_read32(dev, DW_IC_CLR_INTR);
    i2c_reg_write32(dev, DW_IC_INTR_MASK, DW_IC_INTR_DEFAULT_MASK);

    /* Wait for transaction complete event */
    ret = dwc_i2c_wait_complete(dev);

    /* Disabled interrupts */
    i2c_reg_write32(dev, DW_IC_INTR_MASK, 0);

    /* Disable the I2C adapter */
    (void)dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_DISABLE);

    return ret;
}

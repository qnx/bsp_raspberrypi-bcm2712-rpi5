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

static i2c_status_t dwc_i2c_sendrecv(void* const hdl, void* const txbuf,
                const uint32_t txlen, void* const rxbuf, const uint32_t rxlen, const uint32_t stop)
{
    dwc_dev_t  *dev = hdl;
    i2c_status_t    ret = I2C_STATUS_ERROR;

    (void)stop;

    dev->txlen  = txlen;                    // how many bytes for slave transmit (isend)
    dev->rxlen  = rxlen;                    // how many bytes for slave receive (irecv)
    dev->xlen   = dev->txlen + dev->rxlen;  // how many bytes for total transaction (isend and irecv)
    dev->wrlen  = 0;                        // how many cmds have been write to TxFIFO
    dev->rdlen  = 0;                        // how many data have been read from RxFIFO
    dev->txbuf  = txbuf;
    dev->rxbuf  = rxbuf;
    dev->status = 0;

    if (dev->xlen <= 0U) {
        return I2C_STATUS_DONE;
    }

    /* Active I2C bus */
    if (dwc_i2c_bus_active(dev) != 0) {
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

/*
    BEWARE: This function overrides _i2c_master_sendrecv in lib/i2c/master/sendrecv.c
            and is called from _i2c_devctl in lib/i2c/master/devctl.c
*/
int _i2c_master_sendrecv(resmgr_context_t *ctp, io_devctl_t *msg, i2c_ocb_t *ocb);

int _i2c_master_sendrecv(resmgr_context_t *ctp, io_devctl_t *msg, i2c_ocb_t *ocb)
{
    i2c_dev_t           *dev = ocb->hdr.attr;
    i2c_sendrecv_t      *hdr;
    void                *txbuf, *rxbuf;
    i2c_status_t        mstatus;
    uint32_t            maxbuflen;

    if (msg->i.nbytes < sizeof(*hdr)) {
        i2c_slogf(dev->verbosity, _SLOG_ERROR, "Incomplete sendrecv hdr");
        return EINVAL;
    }

    hdr = _IO_INPUT_PAYLOAD(msg);

    if (hdr->send_len < hdr->recv_len) {
        maxbuflen = hdr->recv_len;
    }
    else {
        maxbuflen = hdr->send_len;
    }

    if (maxbuflen == 0U) {
        (void)memset(&msg->o, 0, sizeof(msg->o));
        return _RESMGR_PTR(ctp, msg, sizeof(*msg) + sizeof(*hdr));
    }

    if ((sizeof(*hdr) + hdr->send_len) > msg->i.nbytes) {
        i2c_slogf(dev->verbosity, _SLOG_ERROR, "Incomplete send data");
        return EINVAL;
    }

    if ((sizeof(*hdr) + hdr->recv_len) > msg->i.nbytes) {
        i2c_slogf(dev->verbosity, _SLOG_ERROR, "Receive buffer too short");
        return EINVAL;
    }

    if ((sizeof(*msg) + sizeof(*hdr) + maxbuflen) > ctp->msg_max_size) {
        if (maxbuflen > dev->buflen) {
            free(dev->buf);
            dev->buf = malloc(maxbuflen);
            if (dev->buf == NULL) {
                dev->buflen = 0;
                return ENOMEM;
            }
            dev->buflen = maxbuflen;
        }
    }

    if ((sizeof(*msg) + sizeof(*hdr) + hdr->send_len) > ctp->msg_max_size) {
        int status;
        status = (int32_t)resmgr_msgread(ctp, dev->buf, (size_t)hdr->send_len, sizeof(*msg) + sizeof(*hdr));
        if (status < 0) {
            return errno;
        }
        if ((uint32_t)status < hdr->send_len) {
            return EFAULT;
        }
        txbuf = dev->buf;
    } else {
        txbuf = (void *)(hdr + 1);
    }

    if ((sizeof(*msg) + sizeof(*hdr) + hdr->recv_len) > ctp->msg_max_size) {
        rxbuf = dev->buf;
    } else {
        rxbuf = (void *)(hdr + 1);
    }

    SETIOV(&ctp->iov[0], msg, sizeof(*msg) + sizeof(*hdr));
    SETIOV(&ctp->iov[1], rxbuf, hdr->recv_len);

    if (dev->bus_speed != ocb->bus_speed) {
        if (-1 == dev->mfuncs.set_bus_speed(dev->hdl, ocb->bus_speed, NULL)) {
            i2c_slogf(dev->verbosity, _SLOG_ERROR, "Bad bus speed: %d: %s", ocb->bus_speed, strerror(errno));
            return EIO;
        }
        dev->bus_speed = ocb->bus_speed;
    }

    if (-1 == dev->mfuncs.set_slave_addr(dev->hdl, hdr->slave.addr, hdr->slave.fmt)) {
        i2c_slogf(dev->verbosity, _SLOG_ERROR, "Bad slave address: %x (%s): %s", hdr->slave.addr,
                   (hdr->slave.fmt == (uint32_t)I2C_ADDRFMT_7BIT)? "7-bit": "10-bit", strerror(errno));
        return EIO;
    }
    ocb->status = I2C_STATUS_DONE;

    if ((hdr->send_len + hdr->recv_len) > 0U) {
        mstatus = dwc_i2c_sendrecv(dev->hdl, txbuf, hdr->send_len, rxbuf, hdr->recv_len, hdr->stop);
        ocb->status = mstatus;
        if (mstatus != I2C_STATUS_DONE) {
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_DONE) == 0U) {
                i2c_slogf(dev->verbosity, _SLOG_ERROR, "Master sendrecv did not terminate for slave 0x%x", hdr->slave.addr);
            }
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_ERROR) != 0U) {
                i2c_slogf(dev->verbosity, _SLOG_ERROR, "Master sendrecv timeout for slave 0x%x", hdr->slave.addr);
            }
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_ADDR_NACK) != 0U) {
                i2c_slogf(dev->verbosity, _SLOG_WARNING, "Master sendrecv address NACK for slave 0x%x", hdr->slave.addr);
            }
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_DATA_NACK) != 0U) {
                i2c_slogf(dev->verbosity, _SLOG_WARNING, "Master sendrecv data NACK for slave 0x%x", hdr->slave.addr);
            }
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_ARBL) != 0U) {
                i2c_slogf(dev->verbosity, _SLOG_WARNING, "Master sendrecv ARBL for slave 0x%x", hdr->slave.addr);
            }
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_BUSY) != 0U) {
                i2c_slogf(dev->verbosity, _SLOG_ERROR, "Master sendrecv BUSY for slave 0x%x", hdr->slave.addr);
            }
            if (((uint32_t)mstatus & (uint32_t)I2C_STATUS_ABORT) != 0U) {
                i2c_slogf(dev->verbosity, _SLOG_ERROR, "Master sendrecv ABORT for slave 0x%x", hdr->slave.addr);
            }
            /* General error code - more info is available through a devctl */
            return EIO;
        }
        dev->hdr.flags |= (uint32_t)IOFUNC_ATTR_MTIME | (uint32_t)IOFUNC_ATTR_CTIME;
    }

    (void)memset(&msg->o, 0, sizeof(msg->o));
    msg->o.ret_val = (int32_t)hdr->recv_len;
    msg->o.nbytes = (uint32_t)sizeof(*hdr) + hdr->recv_len;
    return _RESMGR_NPARTS(2);
}

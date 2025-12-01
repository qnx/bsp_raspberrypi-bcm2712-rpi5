/*
 * Copyright (c) 2015, 2022, 2024, 2025, BlackBerry Limited.
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


#ifndef I2C_DWC_PROTO_H_INCLUDED
#define I2C_DWC_PROTO_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>
#include <fcntl.h>
#include <hw/i2c.h>
#include "dwc_i2c.h"

#ifdef DWC_SUPPORT_PCI
#include <pci/pci.h>
#endif

struct i2c_dev;
struct i2c_ocb;
#define IOFUNC_ATTR_T   struct i2c_dev
#define IOFUNC_OCB_T    struct i2c_ocb
#include <sys/iofunc.h>

#define COMMON_OPTIONS_STRING   "c:f:h:Is:v"

struct scl_timing_param {
    uint32_t high;
    uint32_t low;
    uint32_t fall;
};

typedef struct {
    /* interface */
    uint64_t        pbase;          // physical base address of device registers
    uintptr_t       vbase;          // mmapped virtual address of device registers
    uint64_t        map_size;       // size of mmapped region
    int             irq;            // interrupt request number

#ifdef DWC_SUPPORT_PCI
    pci_devhdl_t    pci;
    uint32_t        idx;
    pci_vid_t       vid;
    pci_did_t       did;
    pci_cap_t       msi;
#endif

    /* Interrupt */
    int             iid;

    /* Slave address */
    unsigned        slave_addr;
    i2c_addrfmt_t   slave_addr_fmt;

    /* SCL timing parameters */
    struct scl_timing_param fast;
    struct scl_timing_param std;

    /* Clock */
    uint32_t        clock_khz;
    uint32_t        ss_hcnt;
    uint32_t        ss_lcnt;
    uint32_t        fs_hcnt;
    uint32_t        fs_lcnt;
    uint32_t        scl_freq;
    uint32_t        sda_hold_time;

    /* transaction information */
    uint8_t         *txbuf;
    uint8_t         *rxbuf;
    uint32_t        xlen;           // how many bytes for total transaction (isend and irecv)
    uint32_t        txlen;          // how many bytes for slave transmit (isend)
    uint32_t        rxlen;          // how many bytes for slave receive (irecv)
    uint32_t        wrlen;          // how many cmds have been write to TxFIFO
    uint32_t        rdlen;          // how many data have been read from RxFIFO

    /* Other information */
    uint32_t        verbose;
    uint32_t        master_cfg;
    uint32_t        status;
    uint32_t        abort_source;
    uint32_t        fifo_depth;
    uint32_t        fixed_scl;      // To set scl and hold registers to a fixed, Intel-recommended value
} dwc_dev_t;

typedef struct i2c_ocb {
    iofunc_ocb_t        hdr;
    uint32_t            bus_speed;
    i2c_status_t        status;
} i2c_ocb_t;

/*
 * BEWARE: this i2c_dev must exactly match _i2c_dev defined in lib/i2c/master/proto.h
 */
typedef struct i2c_dev {
    iofunc_attr_t       hdr;
    dispatch_t          *dpp;
    resmgr_context_t    *ctp;
    int                 id;
    i2c_master_funcs_t  mfuncs;

    void                *buf;
    uint32_t            buflen;
    uint32_t            bus_speed;
    uint32_t            default_bus_speed;
    uint32_t            verbosity;

    void                *hdl;
} i2c_dev_t;

void dwc_i2c_reset(dwc_dev_t* const dev);
void *dwc_i2c_init(int argc, char *argv[]);
int32_t dwc_i2c_probe_device(dwc_dev_t *dev);
void dwc_i2c_cleanup(dwc_dev_t *dev);
uint32_t dwc_i2c_get_input_clock(dwc_dev_t *dev);
void dwc_i2c_init_registers(dwc_dev_t *dev);
void dwc_i2c_fini(void *hdl);
void i2c_reg_write32(dwc_dev_t* const dev, const uint32_t offset, const uint32_t value);
uint32_t i2c_reg_read32(dwc_dev_t* const dev, const uint32_t offset);
i2c_status_t dwc_i2c_wait_complete(dwc_dev_t* const dev);
int32_t dwc_i2c_enable(dwc_dev_t* const dev, const uint32_t enable);
int32_t dwc_i2c_bus_active(dwc_dev_t* const dev);
int32_t dwc_i2c_parseopts(dwc_dev_t *dev, const int32_t argc, char *argv[]);
int32_t dwc_i2c_set_slave_addr(void *hdl, uint32_t addr, i2c_addrfmt_t fmt);
int32_t dwc_i2c_set_bus_speed(void *hdl, uint32_t speed, uint32_t *ospeed);
int32_t dwc_i2c_version_info(i2c_libversion_t *version);
int32_t dwc_i2c_driver_info(void *hdl, i2c_driver_info_t *info);
int32_t dwc_i2c_wait_bus_not_busy(dwc_dev_t* const dev);
i2c_status_t dwc_i2c_send(void *hdl, void *buf, uint32_t len, uint32_t stop);
i2c_status_t dwc_i2c_recv(void *hdl, void *buf, uint32_t len, uint32_t stop);
int32_t dwc_i2c_handle_common_option(dwc_dev_t *dev, int32_t opt);

static inline
bool xmit_FIFO_is_full(dwc_dev_t *dev)
{
    return (i2c_reg_read32(dev, DW_IC_STATUS) & DW_IC_STATUS_TFNF) == 0U;
}

#define logerr(fmt, ...) \
    (i2c_slogf((dev)->verbose, _SLOG_ERROR, \
           "i2c-designware " fmt, ##__VA_ARGS__))

#define logfyi(fmt, ...) \
    (i2c_slogf((dev)->verbose, _SLOG_INFO, \
           "i2c-designware " fmt, ##__VA_ARGS__))

#endif /* I2C_DWC_PROTO_H_INCLUDED */

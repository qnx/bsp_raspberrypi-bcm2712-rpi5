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
 * $QNXLicenseC:
 * Copyright 2007-2020, 2023, 2025, BlackBerry Limited.
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

#include "externs.h"
#include <sys/mman.h>
#include <sys/trace.h>

void write_port(DEV_PL011 *dev, int reg, unsigned val)
{
    switch (dev->port_size) {
        case 2:
            out32(dev->base + reg, val);
            break;
        case 1:
            out16(dev->base + reg, val);
            break;
        default:
            out8 (dev->base + reg, val);
            break;
    }
}

unsigned read_port(DEV_PL011 *dev, int reg)
{
    switch (dev->port_size) {
        case 2:
            return in32(dev->base + reg);
        case 1:
            return in16(dev->base + reg);
        default:
            return in8 (dev->base + reg);
    }
}

/*
 * Specify parameters for default devices.
 */
void *
query_default_device(TTYINIT_PL011 *dip, void *link)
{
    switch ((uintptr_t)link) {
        case 0:
            dip->tty.port = 0x10009000;
            dip->tty.intr = 36;
            return (void *)1;
        case 1:
            dip->tty.port = 0x1000A000;
            dip->tty.intr = 37;
            return (void *)2;
    }

    return NULL;
}

void
create_device(TTYINIT_PL011 *dip)
{
    DEV_PL011    *dev;

    /*
     * Get a device entry and the input/output buffers for it.
     */
    dev = (void *) malloc(sizeof(*dev));

    if (dev == NULL) {
        printf("%s(%d): Memory allocation failed\n", __func__, __LINE__);
        exit(EXIT_FAILURE);
    }

    memset(dev, 0, sizeof(*dev));

    /*
     * Get buffers.
     */
    dev->tty.ibuf.buff = malloc(dev->tty.ibuf.size = dip->tty.isize);
    if (dev->tty.ibuf.buff == NULL) {
        perror("PL011 UART: input buffer MAP_FAILED\n");
        exit(1);
    }
    dev->tty.ibuf.head = dev->tty.ibuf.buff;
    dev->tty.ibuf.tail = dev->tty.ibuf.buff;

    dev->tty.obuf.buff = malloc(dev->tty.obuf.size = dip->tty.osize);
    if (dev->tty.obuf.buff == NULL) {
        perror("PL011 UART: output buffer MAP_FAILED\n");
        exit(1);
    }
    dev->tty.obuf.head = dev->tty.obuf.buff;
    dev->tty.obuf.tail = dev->tty.obuf.buff;

    dev->tty.cbuf.buff = malloc(dev->tty.cbuf.size = dip->tty.csize);
    if (dev->tty.cbuf.buff == NULL) {
        perror("PL011 UART: canonical buffer MAP_FAILED\n");
        exit(1);
    }
    dev->tty.cbuf.head = dev->tty.cbuf.buff;
    dev->tty.cbuf.tail = dev->tty.cbuf.buff;

    dev->tty.highwater = dev->tty.ibuf.size - (dev->tty.ibuf.size < 128 ? dev->tty.ibuf.size/4 : 100);

    strlcpy(dev->tty.name, dip->tty.name, sizeof(dev->tty.name));

    dev->tty.baud = dip->tty.baud;
    dev->fifo = dip->tty.fifo;
    dev->tty.fifo = dip->tty.fifo;

    dev->intr = dip->tty.intr;
    dev->clk  = dip->tty.clk;
    dev->div  = dip->tty.div;

    dev->tty.flags = EDIT_INSERT | LOSES_TX_INTR;
    dev->tty.c_cflag = dip->tty.c_cflag;
    dev->tty.c_iflag = dip->tty.c_iflag;
    dev->tty.c_lflag = dip->tty.c_lflag;
    dev->tty.c_oflag = dip->tty.c_oflag;
    dev->tty.verbose = dip->tty.verbose;
    dev->fifosize    = FIFOSIZE;
    dev->loopback    = dip->loopback;
    dev->drt        = dip->drt;
    dev->prio        = dip->prio;
    dev->fifo_reg    = dip->fifo_reg;
    dev->dma_enable = dip->dma_enable;
    dev->dma_request_rx = dip->dma_request_rx;
    dev->dma_request_tx = dip->dma_request_tx;
    dev->dma_xfer_size = dip->dma_xfer_size;
    dev->chan_rx = dip->chan_rx;
    dev->chan_tx = dip->chan_tx;
    dev->port_size = dip->tty.port_shift;
    dev->is_debug_console = dip->is_debug_console;

#ifdef USE_DMA
    if(dev->dma_enable){
        pthread_mutex_init (&dev->dma_lock, NULL);
        if(dev->tty.ibuf.size < dev->dma_xfer_size*2){
            printf("input buffer size has to be equal or greater than 2 times of the dma buffer size\n");
            pthread_mutex_destroy (&dev->dma_lock);
            free(dev);
            return ;
        }
        dev->tty.highwater = dev->tty.ibuf.size - max(dev->tty.ibuf.size/4, dev->dma_xfer_size);

        nanospin_calibrate(0);
        //Initialize DMA libs
        if (get_dmafuncs (&dev->dmafuncs, sizeof (dev->dmafuncs)) == -1){
            printf("failed to get DMA lib functions\n");
            pthread_mutex_destroy (&dev->dma_lock);
            free(dev);
            return ;
        }
        if (dev->dmafuncs.init (NULL) == -1) {
            printf("failed to init DMA lib functions\n");
            pthread_mutex_destroy (&dev->dma_lock);
            free(dev);
            return ;
        }
        if(dev->dma_enable & DMA_TX_ENABLE){

            if ((dev->tx_chid = ChannelCreate(_NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK)) == -1 ||
                (dev->tx_coid = ConnectAttach(0, 0, dev->tx_chid, _NTO_SIDE_CHANNEL, 0)) == -1)
            {
                fprintf(stderr, "Channel create failed\n");
                pthread_mutex_destroy (&dev->dma_lock);
                free(dev);
                return ;
            }

            dev->event.sigev_notify   = SIGEV_PULSE;
            dev->event.sigev_coid      = dev->tx_coid;
            dev->event.sigev_code      = TX_DMA_PULSE;
            dev->event.sigev_priority = dev->prio+1;
            if(dev->chan_tx==-1){
                dev->dma_chn_tx = dev->dmafuncs.channel_attach (NULL, &dev->event, &dev->dma_request_tx, 0,
                    DMA_ATTACH_EVENT_ON_COMPLETE|DMA_ATTACH_ANY_CHANNEL);
            }else{
                dev->dma_chn_tx = dev->dmafuncs.channel_attach (NULL, &dev->event, &dev->dma_request_tx, dev->chan_tx,
                    DMA_ATTACH_EVENT_ON_COMPLETE|DMA_ATTACH_PRIORITY_STRICT);
            }
            if (dev->dma_chn_tx == NULL){
                printf("Unable to attach to DMA Channel dev->chan_tx %d\n", dev->chan_tx);
                pthread_mutex_destroy (&dev->dma_lock);
                free(dev);
                return ;
            }
            dev->dmafuncs.xfer_abort(dev->dma_chn_tx);
            if(dev->dmafuncs.alloc_buffer(dev->dma_chn_tx, &dev->buf_tx, dev->dma_xfer_size, 0)==-1){
                printf("%s(%d): alloc tx dma buffer failed\n", __func__, __LINE__);
                pthread_mutex_destroy (&dev->dma_lock);
                free(dev);
                return ;
            }

            dev->tinfo_tx.src_flags = DMA_ADDR_FLAG_MEMORY;
            dev->tinfo_tx.dst_flags = DMA_ADDR_FLAG_NO_INCREMENT|DMA_ADDR_FLAG_DEVICE;
            dev->tinfo_tx.dst_fragments = 1;
            dev->tinfo_tx.src_fragments = 1;
            dev->tinfo_tx.xfer_unit_size = 1;
            dev->tinfo_tx.mode_flags = 0;
            dev->dst_tx.paddr = dev->fifo_reg;
            dev->dst_tx.len = 0;
            dev->tinfo_tx.src_addrs = &dev->buf_tx;
            dev->tinfo_tx.dst_addrs = &dev->dst_tx;
        }

        if(dev->dma_enable & DMA_RX_ENABLE){
            if(dev->chan_rx==-1){
                dev->dma_chn_rx = dev->dmafuncs.channel_attach (NULL, NULL, &dev->dma_request_rx, 0,
                    DMA_ATTACH_ANY_CHANNEL);
            }else{
                dev->dma_chn_rx = dev->dmafuncs.channel_attach (NULL, NULL, &dev->dma_request_rx, dev->chan_rx,
                    DMA_ATTACH_PRIORITY_STRICT);
            }
            if (dev->dma_chn_rx == NULL){
                printf("Unable to attach to DMA Channel dev->chan_rx %d\n", dev->chan_rx);
                pthread_mutex_destroy (&dev->dma_lock);
                free(dev);
                return ;
            }
            dev->dmafuncs.xfer_abort(dev->dma_chn_rx);

            if(dev->dmafuncs.alloc_buffer(dev->dma_chn_rx, &dev->buf_rx[0], dev->dma_xfer_size, 0)==-1){
                printf("%s(%d): alloc rx dma buffer failed\n", __func__, __LINE__);
                pthread_mutex_destroy (&dev->dma_lock);
                free(dev);
                return ;
            }
            if(dev->dmafuncs.alloc_buffer(dev->dma_chn_rx, &dev->buf_rx[1], dev->dma_xfer_size, 0)==-1){
                printf("%s(%d): alloc rx dma buffer failed\n", __func__, __LINE__);
                pthread_mutex_destroy (&dev->dma_lock);
                free(dev);
                return ;
            }

            dev->tinfo_rx.dst_flags = DMA_ADDR_FLAG_MEMORY;
            dev->tinfo_rx.src_flags = DMA_ADDR_FLAG_NO_INCREMENT|DMA_ADDR_FLAG_DEVICE;
            dev->tinfo_rx.dst_fragments = 1;
            dev->tinfo_rx.src_fragments = 1;
            dev->tinfo_rx.xfer_unit_size = 1;
            dev->tinfo_rx.mode_flags = 0;
        }
    }
#endif
    /*
     * Map device registers
     */
    dev->base = (uintptr_t)mmap_device_memory (0, PL011_SIZE,
                           PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, dip->tty.port);
    if (dev->base == (uintptr_t)MAP_FAILED) {
        perror("PL011 UART: MAP_FAILED\n");
#ifdef USE_DMA
        if(dev->dma_enable)
            pthread_mutex_destroy (&dev->dma_lock);
#endif
        exit(1);
    }

    /*
     * Initialize termios cc codes to an ANSI terminal.
     */
    ttc(TTC_INIT_CC, &dev->tty, 0);

    /*
     * Initialize the device's name.
     * Assume that the basename is set in device name.  This will attach
     * to the path assigned by the unit number/minor number combination
     */
    ttc(TTC_INIT_TTYNAME, &dev->tty, (int)(SET_NAME_NUMBER(dip->tty.unit) | NUMBER_DEV_FROM_USER));

    /*
     * Only setup IRQ handler for non-pcmcia devices.
     * Pcmcia devices will have this done later when card is inserted.
     */
    if (dip->tty.port != 0 && dip->tty.intr != _NTO_INTR_SPARE) {
        ser_attach_intr(dev);
        ser_stty(dev);
    }

    /*
     * Attach the resource manager
     */
    ttc(TTC_INIT_ATTACH, &dev->tty, 0);

#ifdef USE_DMA
    if ((dev->dma_enable & DMA_RX_ENABLE) && dev->drt != 0){
        pthread_attr_t    tattr;
        struct sched_param param;
        pthread_attr_init (&tattr);
        pthread_attr_setschedpolicy (&tattr, SCHED_RR);
        param.sched_priority = dev->prio;
        pthread_attr_setschedparam (&tattr, &param);
        pthread_attr_setinheritsched (&tattr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setdetachstate (&tattr, PTHREAD_CREATE_DETACHED);
        if (pthread_create (NULL, &tattr, (void *) timer_thread, (void *)dev) != EOK)
        {
            printf("devc-serpl0o11: Cleanup thread create failed\n");
            ser_detach_intr(dev);
            pthread_mutex_destroy (&dev->dma_lock);
            free(dev);
            exit(1);
        }
    }

    if ((dev->dma_enable & DMA_TX_ENABLE) ){
        pthread_attr_t    tattr;
        struct sched_param param;
        pthread_attr_init (&tattr);
        pthread_attr_setschedpolicy (&tattr, SCHED_RR);
        param.sched_priority = dev->prio;
        pthread_attr_setschedparam (&tattr, &param);
        pthread_attr_setinheritsched (&tattr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setdetachstate (&tattr, PTHREAD_CREATE_DETACHED);
        if (pthread_create (NULL, &tattr, (void *) tx_dma_thread, (void *)dev) != EOK)
        {
            printf("devc-serpl0o11: Cleanup thread create failed\n");
            ser_detach_intr(dev);
            pthread_mutex_destroy (&dev->dma_lock);
            free(dev);
            exit(1);
        }
    }
#endif

}

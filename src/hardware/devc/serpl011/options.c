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
 * Copyright 2007-2019, 2023, 2025, BlackBerry Limited.
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

int
options(int argc, char *argv[])
{
    int         opt;
    int         numports = 0;
    void        *link;
    unsigned    rx_fifo = 2;    // 1 / 2 full, default
    unsigned    tx_fifo = 2;    // 1 / 2 full, default
    uint64_t    val = 0ULL;
    uint32_t    port_error = 0L;

    TTYINIT_PL011    devinit = {
        .tty = {
            .port = 0,
            .port_shift = 1,
            .intr = 0,
            .baud = 38400,
            .isize = 2048,
            .osize = 2048,
            .csize = 256,
            .c_cflag = 0,
            .c_iflag = 0,
            .c_lflag = 0,
            .c_oflag = 0,
            .fifo = 0,
            .clk = 24000000,
            .div = 16,
            .name = "/dev/ser",
            .reserved1 = NULL,
            .reserved2 = 0,
            .verbose = 0,
            .highwater = 0,
            .logging_path = "",
            .lflags = 0,
            .unit = 1
        },
        .drt = 0,
        .prio = 0,
        .loopback = 0,
        .fifo_reg = 0,
        .dma_enable = 0,
        .dma_request_rx = 0,
        .dma_request_tx = 0,
        .dma_xfer_size = 0,
        .chan_rx = 0,
        .chan_tx = 0,
        .is_debug_console = 0,
    };

    /*
     * Initialize the devinit to raw mode
     */
    ttc(TTC_INIT_RAW, &devinit, 0);

    while (optind < argc) {
        /*
         * Process dash options.
         */
        while ((opt = getopt(argc, argv, IO_CHAR_SERIAL_OPTIONS "t:T:D")) != -1) {
            switch (opt) {
                case 't':
                    errno = EOK;
                    val = strtoul(optarg, NULL, 0);
                    if (errno != EOK) {
                        (void)fprintf(stderr, "Invalid receive FIFO trigger level, errno: %d\n", errno);
                        break;
                    }
                    rx_fifo = (unsigned)val;
                    if (rx_fifo > 4)
                        rx_fifo = 2;
                    break;

                case 'T':
                    errno = EOK;
                    val = strtoul(optarg, NULL, 0);
                    if (errno != EOK) {
                        (void)fprintf(stderr, "Invalid number of characters to send to transmit FIFO, errno: %d\n", errno);
                        break;
                    }
                    tx_fifo = (unsigned)val;
                    if (tx_fifo > 4)
                        tx_fifo = 2;
                    break;

                case 'D':
                    devinit.is_debug_console = 1;
                    break;

                default:
                    /* the other options has parsed by ttc function */
                    (void)ttc(TTC_SET_OPTION, &devinit, opt);
                    break;
            }
        }

        devinit.tty.fifo = (rx_fifo << 3) | tx_fifo;

        /*
         * Process ports and interrupts.
         */
        while (optind < argc  &&  *(optarg = argv[optind]) != '-') {
            errno = EOK;
            val = strtoul(optarg, &optarg, 16);
            if (errno != EOK) {
                (void)fprintf(stderr, "Invalid serial port base address, errno: %d\n", errno);
                port_error = 1L;
                break;
            }
            devinit.tty.port = val;
            if (*optarg == '^') {
                errno = EOK;
                val = strtoul(optarg + 1, &optarg, 0);
                if (errno != EOK) {
                    (void)fprintf(stderr, "Invalid serial port shift, errno: %d\n", errno);
                    port_error = 1L;
                    break;
                }
                devinit.tty.port_shift = (unsigned)val;
            }
            if (*optarg == ',')  {
                errno = EOK;
                val = strtoul(optarg + 1, NULL, 0);
                if (errno != EOK) {
                    (void)fprintf(stderr, "Invalid serial port interrupt, errno: %d\n", errno);
                    port_error = 1L;
                    break;
                }
                devinit.tty.intr = (unsigned)val;
            }
            create_device(&devinit);
            devinit.tty.unit++;
            ++numports;
            ++optind;
        }
        if (port_error != 0L) {
            break;
        }
    }

    if (numports == 0) {
        link = NULL;
        devinit.tty.fifo = (rx_fifo << 3) | tx_fifo;
        while (1) {
            link = query_default_device(&devinit, link);
            if (link == NULL)
                break;
            create_device(&devinit);
            devinit.tty.unit++;
            ++numports;
        }
    }

    return numports;
}

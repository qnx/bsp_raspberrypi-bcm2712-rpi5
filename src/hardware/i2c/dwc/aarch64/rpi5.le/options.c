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


#include <unistd.h>
#include "proto.h"

int32_t dwc_i2c_parseopts(dwc_dev_t *dev, const int32_t argc, char *argv[])
{
    int32_t ret = 0;
    int32_t c;
    int32_t prev_optind;
    int32_t done = 0;

    dev->master_cfg = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
            DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_STD |
            DW_IC_CON_7BITADDR_MASTER;
    dev->scl_freq   = 0;
    dev->clock_khz  = 0;
    dev->verbose    = _SLOG_ERROR;
    dev->irq        = -1;
    dev->iid        = -1;
    dev->sda_hold_time = 0;
    dev->fixed_scl  = 0;

    dev->fast.high  = FS_THD_HIGH;
    dev->fast.low   = FS_THD_LOW;
    dev->fast.fall  = FS_SCL_FALL_TIME;

    dev->std.high   = SS_THD_HIGH;
    dev->std.low    = SS_THD_LOW;
    dev->std.fall   = SS_SCL_FALL_TIME;

    while ((done == 0) && (ret == 0)) {
        prev_optind = optind;
        c = getopt(argc, argv, COMMON_OPTIONS_STRING "p:q:");
        switch (c) {
        case (int32_t)'p':
            errno = 0; /* CERT ERR30-C */
            dev->pbase = strtoul(optarg, NULL, 0);
            if (errno != EOK) {
                ret = -1;
            }
            break;
        case (int32_t)'q':
            errno = 0; /* CERT ERR30-C */
            dev->irq = (int32_t)strtoul(optarg, NULL, 0);
            if (errno != EOK) {
                ret = -1;
            }
            break;
        case (int32_t)'?':
            if (optopt == (int32_t)'-') {
                ++optind;
                break;
            }
            ret = -1;
            break;
        case -1:
            if (prev_optind < optind) {     /* -- */
                ret = -1;
                break;
            }

            if (argv[optind] == NULL) {
                done = 1;
                break;
            }
            if (*argv[optind] != '-') {
                ++optind;
                break;
            }
            ret = -1;
            break;
        case (int32_t)':':
            ret = -1;
            break;
        default:
            if (dwc_i2c_handle_common_option(dev, c) == 0) {
                break;
            }
            ret = -1;
            break;
        }
    }

    return ret;
}

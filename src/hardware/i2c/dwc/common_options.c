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

static int32_t parse_scl_timing(struct scl_timing_param *param, char *popt)
{
    int32_t ret = -1;
    uint32_t high, low, fall;

    errno = 0; /* CERT ERR30-C */
    high = (uint32_t)strtoul(popt, &popt, 0);

    if ((errno == EOK) && (*popt == ':')) {
        errno = 0; /* CERT ERR30-C */
        low = (uint32_t)strtoul(++popt, &popt, 0);

        if ((errno == EOK) && (*popt == ':')) {
            errno = 0; /* CERT ERR30-C */
            fall = (uint32_t)strtoul(++popt, &popt, 0);

            if (errno == EOK) {
                param->high = high;
                param->low  = low;
                param->fall = fall;
                ret = 0;
            }
        }
    }

    return ret;
}

int32_t dwc_i2c_handle_common_option(dwc_dev_t *dev, int32_t opt)
{
    int32_t ret = 0;

    switch (opt) {
        case (int32_t)'c':
            errno = 0; /* CERT ERR30-C */
            dev->clock_khz = (uint32_t)strtoul(optarg, NULL, 0) / 1000U;
            if (errno != EOK) {
                ret = -1;
                break;
            }
            break;
        case (int32_t)'f':
            if (parse_scl_timing(&dev->fast, optarg) != 0) {
                logerr("failed to parse SCL timing parameter for fast mode.\n");
                ret = -1;
            }
            break;
        case (int32_t)'h':
            errno = 0; /* CERT ERR30-C */
            dev->sda_hold_time = (uint32_t)strtoul(optarg, NULL, 0);
            if (errno != EOK) {
                ret = -1;
                break;
            }
            break;
#ifdef DWC_SUPPORT_FIXED_SCL
        case (int32_t)'I':
            dev->fixed_scl = 1;
            break;
#endif
        case (int32_t)'s':
            if (parse_scl_timing(&dev->std, optarg) != 0) {
                logerr("failed to parse SCL timing parameter for std mode.\n");
                ret = -1;
            }
            break;
        case (int32_t)'v':
            dev->verbose++;
            break;
        default:
            ret = -1;
            break;
    }

    return ret;
}

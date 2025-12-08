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


int32_t dwc_i2c_set_bus_speed(void *hdl, uint32_t speed, uint32_t *ospeed)
{
    dwc_dev_t   *dev = hdl;

    if (speed == dev->scl_freq) {
        if (ospeed != NULL) {
            *ospeed = dev->scl_freq;
        }
        return 0;
    }

    if (speed == 100000U) {
        dev->scl_freq = 100000;

        /* set the bus speed */
        dev->master_cfg &= ~DW_IC_CON_SPEED_MASK;
        dev->master_cfg |= DW_IC_CON_SPEED_STD;
    }
    else if (speed == 400000U) {
        dev->scl_freq = 400000;

        /* set the bus speed */
        dev->master_cfg &= ~DW_IC_CON_SPEED_MASK;
        dev->master_cfg |= DW_IC_CON_SPEED_FAST;
    }
    else {
        errno = ERANGE;
        return -1;
    }

    if (ospeed != NULL) {
        *ospeed = dev->scl_freq;
    }

    return 0;
}

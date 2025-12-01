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

#include "proto.h"

#define DW_IC_CLOCK_PARAMS_200MHZ     200000000

uint32_t dwc_i2c_get_input_clock(dwc_dev_t *dev)
{
    (void)dev;

    return DW_IC_CLOCK_PARAMS_200MHZ / 1000;
}

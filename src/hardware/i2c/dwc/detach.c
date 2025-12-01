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

void dwc_i2c_cleanup(dwc_dev_t *dev)
{
    /* unmap the device registers */
    if (dev->vbase != 0U) {
        const int32_t rc = munmap_device_memory((void*)dev->vbase,
                        dev->map_size);
        if (rc == -1) {
            logerr("munmap_device_memory failed");
        }

        dev->vbase = 0;
    }

    free(dev); /* free device object */
}

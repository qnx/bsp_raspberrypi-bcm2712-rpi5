/*
 * Copyright (c) 2024, BlackBerry Limited.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/procmgr.h>
#include "proto.h"

int32_t verbose = 0;

static int options(const pwm_fan_dev_t * const dev, const int argc, char* const argv[])
{
    int c;

    while ((c = getopt(argc, argv, "v")) != -1) {
        switch (c) {
            case 'v':
                verbose++;
                break;
            default:
                (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "Invalid option");
                return EINVAL;
        }
    }

    return EOK;
}

int main(const int argc, char *argv[])
{
    int status;
    pwm_fan_dev_t dev = { 0 };

    dev.base = RP1_PWM1_BASE;
    dev.reg_size = RP1_PWM_SIZE;
    dev.vbase = 0;

    status = options(&dev, argc, argv);
    if (status != EOK) {
        return EXIT_FAILURE;
    }

    (void)slogf(_SLOGC_PWM, _SLOG_INFO, "Started: PWM base: 0x%lx", dev.base);

    if (-1 == ThreadCtl(_NTO_TCTL_IO_LEVEL, (void*) (_NTO_IO_LEVEL_1 | _NTO_TCTL_IO_LEVEL_INHERIT))) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "ThreadCtl failed, errno: %d", errno);
        return EXIT_FAILURE;
    }

    status = pwm_fan_init(&dev);
    if (status != EOK) {
        goto done;
    }

    status = resmgr_init(&dev);
    if (status != EOK) {
        goto done;
    }

    /* Detach process as a daemon */
    if (procmgr_daemon(EXIT_SUCCESS, PROCMGR_DAEMON_NOCLOSE) == -1) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "Failed to detaching process as a daemon, errno: %d", errno);
        status = errno;
        goto done;
    }

    /* Drop the abilities now that the resource manager is set up */
    status = procmgr_ability(0, PROCMGR_ADN_ROOT | PROCMGR_ADN_NONROOT | PROCMGR_AOP_DENY |
                                PROCMGR_AOP_LOCK | PROCMGR_AID_EOL);
    if (status != EOK) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "Dropping procmgr abilities failed, status: %d", status);
        goto done;
    }

    status = resmgr_loop_start();

    if (verbose > 0) {
        (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "Exited resource manager loop");
    }

done:

    resmgr_deinit();
    pwm_fan_deinit(&dev);
    return ((status == EOK) ? EXIT_SUCCESS : EXIT_FAILURE);
}

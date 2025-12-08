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
#include <errno.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <hw/inout.h>
#include "proto.h"

uint32_t pwm_fan_get(const pwm_fan_dev_t * const dev)
{
    return dev->pwm_ctrl;
}

int pwm_fan_set(pwm_fan_dev_t *dev , const uint32_t pwm_ctrl)
{
    if (pwm_ctrl > PWM_CTRL_MAX_VAL) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Invalid value: %u", __func__, pwm_ctrl);
        return EINVAL;
    }

    if (pwm_ctrl == 0U) {
        /* if fan is on, turn it off */
        if (dev->pwm_ctrl != 0U) {
            out32(dev->vbase + RP1_PWM_CHAN3_DUTY, 0);
            dev->pwm_ctrl = 0;
            if (verbose > 0) {
                (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Set pwm channel3 duty register: 0", __func__);
            }
        }
    }
    else {
        uint32_t val;
        dev->pwm_ctrl = pwm_ctrl;
        val = (pwm_ctrl * dev->range) / PWM_CTRL_MAX_VAL;
        out32(dev->vbase + RP1_PWM_CHAN3_DUTY, val);
        if (verbose > 0) {
            (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Set pwm channel3 duty register: 0x%x", __func__, val);
        }
    }
    return EOK;
}

void pwm_fan_deinit(const pwm_fan_dev_t * const dev)
{
    if ((dev->vbase != 0UL) && (dev->vbase != (uintptr_t)MAP_FAILED)) {
        (void)munmap_device_io(dev->vbase, dev->reg_size);
    }
}

static int pwm_clock_init(void)
{
    uintptr_t     vbase;
    /*
     * set RP1 PWM1 clock
     */
    vbase = (uintptr_t)mmap_device_io(RP1_CLOCK_MAIN_SIZE, RP1_CLOCK_MAIN_BASE);
    if (vbase == (uintptr_t)MAP_FAILED) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Couldn't mmap rp1 clock, errno: %d", __func__, errno);
        return errno;
    }

    out32(vbase + RP1_CLK_PWM1_CTRL, 0x11000840);
    if (verbose > 0) {
        (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Set pwm1 clock reg: 0x%x", __func__, 0x11000840);
    }
    (void)munmap_device_io(vbase, RP1_CLOCK_MAIN_SIZE);

    return EOK;
}

int pwm_fan_init(pwm_fan_dev_t *dev)
{
    int ret;
    uint32_t val;
    const uint32_t pwm_clk_period_ns = RP1_PWM_CLK_CYCLE_NS;

    ret = pwm_clock_init();
    if (ret != EOK) {
        return ret;
    }

    dev->vbase = (uintptr_t)mmap_device_io(dev->reg_size, dev->base);
    if (dev->vbase == (uintptr_t)MAP_FAILED) {
        (void)slogf(_SLOGC_PWM, _SLOG_ERROR, "%s Couldn't mmap rp1 pwm, errno: %d", __func__, errno);
        return errno;
    }

    /*
     * The user can enter range 0 ~ 255, while 0 means off
     */

    dev->range = RP1_PWM_CHAN3_RANGE_DEFAULT / pwm_clk_period_ns;
    out32(dev->vbase + RP1_PWM_CHAN3_RANGE, dev->range);
    out32(dev->vbase + RP1_PWM_CHAN3_PHASE, 0);
    val = dev->range / 2U;                 // chose 1/2 of range as default
    dev->pwm_ctrl = PWM_CTRL_MAX_VAL / 2U;
    out32(dev->vbase + RP1_PWM_CHAN3_DUTY, val);
    /* write PWM channel control register */
    out32(dev->vbase + RP1_PWM_CHAN3_CTRL, RP1_PWM_CHAN3_CTRL_DEFAULT);
    /* write PWM global control register */
    out32(dev->vbase + RP1_PWM_GLOBAL_CTRL, (PWM_GLOBAL_CTRL_UPDATE | PWM_GLOBAL_CTRL_CHANNEL_EN(3)));
    if (verbose > 0) {
        (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Set pwm channel3 range-phase-duty register: 0x%x-0-0x%x",
                __func__, dev->range, val);
        (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Set pwm channel3 ctrl register: 0x%x", __func__, RP1_PWM_CHAN3_CTRL_DEFAULT);
        (void)slogf(_SLOGC_PWM, _SLOG_DEBUG1, "%s Set pwm global ctrl register: 0x%x", __func__, (PWM_GLOBAL_CTRL_UPDATE | PWM_GLOBAL_CTRL_CHANNEL_EN(3)));
    }
    return EOK;
}

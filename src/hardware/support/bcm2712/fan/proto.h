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
 * Copyright (c) 2024-2025, BlackBerry Limited.
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

#ifndef _PROTO_H_INCLUDED
#define _PROTO_H_INCLUDED

#include <stdint.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>

extern int32_t verbose;
extern char* verbose_str[];

#define RESMGR_NAME                     "fan"
#define FAN_DEV_NAME                    "/dev/" RESMGR_NAME

/* RP1 Clock main registers */
#define RP1_CLOCK_MAIN_BASE             (0x1f00018000ULL)
#define RP1_CLOCK_MAIN_SIZE             (0x400ULL)

#define RP1_CLK_PWM1_CTRL               (0x84U)
#define RP1_CLK_PWM1_DIV_INT            (0x88U)
#define RP1_CLK_PWM1_DIV_FRAC           (0x8cU)
#define RP1_LK_PWM1_SEL                 (0x90U)

/* RP1 PWM registers */
#define RP1_PWM0_BASE                   (0x1f00098000ULL)
#define RP1_PWM1_BASE                   (0x1f0009c000ULL)
#define RP1_PWM_SIZE                    (0x100U)

#define RP1_PWM_GLOBAL_CTRL             (0x00U)  /* Global control bits */
#define RP1_PWM_FIFO_CTRL               (0x04U)  /* FIFO thresholding and status */
#define RP1_PWM_COMMON_RANGE            (0x08U)
#define RP1_PWM_COMMON_DUTY             (0x0cU)

#define RP1_PWM_CHAN0_CTRL              (0x14U)  /* Channel 0 control register */
#define RP1_PWM_CHAN0_RANGE             (0x18U)
#define RP1_PWM_CHAN0_PHASE             (0x1cU)
#define RP1_PWM_CHAN0_DUTY              (0x20U)

#define RP1_PWM_CHAN1_CTRL              (0x24U)  /* Channel 1 control register */
#define RP1_PWM_CHAN1_RANGE             (0x28U)
#define RP1_PWM_CHAN1_PHASE             (0x2cU)
#define RP1_PWM_CHAN1_DUTY              (0x30U)

#define RP1_PWM_CHAN2_CTRL              (0x34U)  /* Channel 2 control register */
#define RP1_PWM_CHAN2_RANGE             (0x38U)
#define RP1_PWM_CHAN2_PHASE             (0x3cU)
#define RP1_PWM_CHAN2_DUTY              (0x40U)

#define RP1_PWM_CHAN3_CTRL              (0x44U)  /* Channel 3 control register */
#define RP1_PWM_CHAN3_RANGE             (0x48U)  /* Channel 3 counter range, i.e number of pwm1 clock cycles */
#define RP1_PWM_CHAN3_PHASE             (0x4cU)
#define RP1_PWM_CHAN3_DUTY              (0x50U)

/* global control register */
#define PWM_GLOBAL_CTRL_UPDATE          (0x80000000U)
#define PWM_GLOBAL_CTRL_CHANNEL_EN(x)   (1U << x)  /* channel enable bit */


/* channel 0~3 control register */
#define PWM_CHAN_CTRL_MODE_MASK         (0x7U)
  #define PWM_CHAN_CTRL_MODE_0          (0x0U)         /* Generates 0 */
  #define PWM_CHAN_CTRL_MODE_1          (0x1U)         /* Trailing-edge mark-space PWM modulation */
  #define PWM_CHAN_CTRL_MODE_2          (0x2U)         /* Phase-correct mark-space PWM modulation */
  #define PWM_CHAN_CTRL_MODE_3          (0x3U)         /* Pulse-density encoded output */
  #define PWM_CHAN_CTRL_MODE_4          (0x4U)         /* MSB Serialiser output */
  #define PWM_CHAN_CTRL_MODE_5          (0x5U)         /* Pulse position modulated output - a single high-pulse is transmitted per cycle */
  #define PWM_CHAN_CTRL_MODE_6          (0x6U)         /* Leading-edge mark-space PWM modulation */
  #define PWM_CHAN_CTRL_MODE_7          (0x7U)         /* LSB Serialiser output */

#define PWM_CHAN_CTRL_INVERT_MASK       (1U << 3)      /* 1 - Invert the output bit */
#define PWM_CHAN_CTRL_BIND_MASK         (1U << 4)      /* 1 - bind channel to common register 0 - use own range and duty */
#define PWM_CHAN_FIFO_POP_MASK          (1U << 8)      /* 1 - Counter overflow events generate FIFO pop events */


/*
 * FDT cooling_fan entry has: "pwms = <0x5f 0x03 0xa25e 0x01>" which means:
 *  - PWM number: 3
 *  - PWM period in ns: 41566
 *  - PWM_POLARITY_INVERTED: true
 */
#define RP1_PWM_CHAN3_CTRL_DEFAULT      (PWM_CHAN_CTRL_MODE_1 | PWM_CHAN_CTRL_INVERT_MASK | PWM_CHAN_FIFO_POP_MASK)
#define RP1_PWM_CHAN3_RANGE_DEFAULT     (41566U)
/*
 * RP1 clk_pwm1 = 50Mhz, so that pwm_clk_period = 20ns
 */
#define RP1_PWM_CLK_CYCLE_NS            (20U)
#define PWM_CTRL_MAX_VAL                (255U)
#define PWM_CTRL_MAX_STRLEN             (12U)
#define _SLOGC_PWM                      _SLOG_SETCODE( _SLOG_SYSLOG, 0 )

typedef struct pwm_fan_dev_ {
    paddr_t       base;
    uint32_t      reg_size;
    uintptr_t     vbase;
    uint32_t      range;    /* the value stored in PWM range register */
    uint32_t      pwm_ctrl; /* 0~255, 0 - fan off, 255 fan on with maximum power */
} pwm_fan_dev_t;

typedef void (* event_handler_t)(void * handle);

uint32_t pwm_fan_get(const pwm_fan_dev_t * const dev);
int pwm_fan_set(pwm_fan_dev_t *dev , const uint32_t pwm_ctrl);
void pwm_fan_deinit(const pwm_fan_dev_t * const dev);
int pwm_fan_init(pwm_fan_dev_t *dev);

int resmgr_create_event(event_handler_t event_handler, void *handle, struct sigevent* event);
int resmgr_loop_start(void);
void resmgr_deinit(void);
int resmgr_init(pwm_fan_dev_t *dev);

#endif /* _PROTO_H_INCLUDED */

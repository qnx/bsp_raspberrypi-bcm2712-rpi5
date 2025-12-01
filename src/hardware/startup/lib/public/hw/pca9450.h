/*
 * Copyright 2022, 2023, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.
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

#ifndef _PCA9450_INCLUDE_H_
#define _PCA9450_INCLUDE_H_

#define PCA9450_DEVICE_ID_REG        0x00
#define PCA9450_INT1_REG             0x01
#define PCA9450_INT1_MSK_REG         0x02
#define PCA9450_STATUS1_REG          0x03
#define PCA9450_STATUS2_REG          0x04
#define PCA9450_PWRON_STAT_REG       0x05
#define PCA9450_SWRST_REG            0x06
#define PCA9450_PWRCTRL_REG          0x07
#define PCA9450_RESET_CTRL_REG       0x08
#define PCA9450_CONFIG1_REG          0x09
#define PCA9450_CONFIG2_REG          0x0A
#define PCA9450_BUCK123_DVS_REG      0x0C
#define PCA9450_BUCK1OUT_LIMIT_REG   0x0D
#define PCA9450_BUCK2OUT_LIMIT_REG   0x0E
#define PCA9450_BUCK3OUT_LIMIT_REG   0x0F
#define PCA9450_BUCK1CTRL_REG        0x10
#define PCA9450_BUCK1OUT_DVS0_REG    0x11
#define PCA9450_BUCK1OUT_DVS1_REG    0x12
#define PCA9450_BUCK2CTRL_REG        0x13
#define PCA9450_BUCK2OUT_DVS0_REG    0x14
#define PCA9450_BUCK2OUT_DVS1_REG    0x15
#define PCA9450_BUCK3CTRL_REG        0x16
#define PCA9450_BUCK3OUT_DVS0_REG    0x17
#define PCA9450_BUCK3OUT_DVS1_REG    0x18
#define PCA9450_BUCK4CTRL_REG        0x19
#define PCA9450_BUCK4OUT_REG         0x1A
#define PCA9450_BUCK5CTRL_REG        0x1B
#define PCA9450_BUCK5OUT_REG         0x1C
#define PCA9450_ BUCK6CTRL_REG       0x1D
#define PCA9450_BUCK6OUT_REG         0x1E
#define PCA9450_LDO_AD_CTRL_REG      0x20
#define PCA9450_LDO1CTRL_REG         0x21
#define PCA9450_LDO2CTRL_REG         0x22
#define PCA9450_LDO3CTRL_REG         0x23
#define PCA9450_LDO4CTRL_REG         0x24
#define PCA9450_LDO5CTRL_L_REG       0x25
#define PCA9450_LDO5CTRL_H_REG       0x26
#define PCA9450_LOADSW_CTRL_REG      0x2A
#define PCA9450_VRFLT1_STS_REG       0x2B
#define PCA9450_VRFLT2_STS_REG       0x2C
#define PCA9450_VRFLT1_MASK_REG      0x2D
#define PCA9450_VRFLT2_MASK_REG      0x2E

#endif


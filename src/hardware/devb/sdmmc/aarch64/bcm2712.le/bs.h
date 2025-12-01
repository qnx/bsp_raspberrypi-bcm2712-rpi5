/*
 * $QNXLicenseC:
 * Copyright 2020,2025, QNX Software Systems.
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

/* Module Description:  board specific header file */

#ifndef _BS_H_INCLUDED
#define _BS_H_INCLUDED

// Add new chipset externs here
#define SDIO_HC_SDHCI

#undef  SDIO_SOC_SUPPORT
#define SDIO_HWI_NAME "sdmmc"

#define SDIO_CFG_REG_OFFSET                     0x400U

#define SDIO_CFG_CTRL_REG                       0x0U
#define SDIO_CFG_CTRL_SDCD_N_TEST_EN            0x80000000U
#define SDIO_CFG_CTRL_SDCD_N_TEST_LEV           0x40000000U

#define SDIO_CFG_SD_PIN_SEL_REG                 0x44U
#define SDIO_CFG_SD_PIN_SEL_MASK                0x3U
#define SDIO_CFG_SD_PIN_SEL_SD                  0x2U
#define SDIO_CFG_SD_PIN_SEL_MMC                 0x1U

#define SDIO_CFG_MAX_50MHZ_MODE_REG             0x1acU
#define SDIO_CFG_MAX_50MHZ_MODE_STRAP_OVERRIDE  0x80000000U
#define SDIO_CFG_MAX_50MHZ_MODE_ENABLE          0x1U

#endif

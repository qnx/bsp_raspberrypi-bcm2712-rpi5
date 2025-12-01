/*
 * Copyright (c) 2008, 2022, 2024, BlackBerry Limited.
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

#ifndef _DW_VARIANT_H_
#define _DW_VARIANT_H_

#include <dwc/dwc-spi.h>

#define DW_SPI_MIN_FIFO_LENGTH                  (2)               // Min FIFO depth for designware SPI device
#define DW_SPI_MAX_FIFO_LENGTH                  (256)             // Max FIFO depth for designware SPI device

#define DW_SPI_SSTE_OFFSET                      (24)
#define DW_SPI_CTRLR0_SPI_FRF_OFFSET            (21)               // SPI frame format
/* DFS[20:16] for 32bit width */
#define DW_PSSI_CTRLR0_DFS32_OFFSET             (16)
#define DW_PSSI_CTRLR0_CFS_OFFSET               (12)
#define DW_PSSI_CTRLR0_SRL_OFFSET               (11)
#define DW_PSSI_CTRLR0_SLV_OE_OFFSET            (10)
#define DW_SPI_CTRLR0_TMOD_OFFSET               (8)
#define DW_SPI_CTRLR0_SCPOL_OFFSET              (7)
#define DW_SPI_CTRLR0_SCPHA_OFFSET              (6)
#define DW_SPI_CTRLR0_FRF_OFFSET                (4)             // Frame format to choose serial protocol
/* DFS[3:0] for 16bit width */
#define DW_PSSI_CTRLR0_DFS_OFFSET               (0)

#define DW_PSSI_CTRLR0_DFS_CONFIG(dlen)         (((dlen) < SPI_MODE_WORD_WIDTH_16 / 8) ? \
                                                (((SPI_DFS_DEFAULT * (dlen)) - 1) << DW_PSSI_CTRLR0_DFS_OFFSET) : \
                                                (((SPI_DFS_DEFAULT * (dlen)) - 1) << DW_PSSI_CTRLR0_DFS32_OFFSET))

#endif /* _DW_VARIANT_H_ */

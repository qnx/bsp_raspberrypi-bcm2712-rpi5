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


#ifndef DWC_I2C_H__INCLUDED
#define DWC_I2C_H__INCLUDED

#include "variant.h"

/*
 * Registers offset
 */
#define DW_IC_CON                           0x00
#define DW_IC_TAR                           0x04
#define DW_IC_SAR                           0x08
#define DW_IC_HS_MADDR                      0x0c
#define DW_IC_DATA_CMD                      0x10
#define DW_IC_SS_SCL_HCNT                   0x14
#define DW_IC_SS_SCL_LCNT                   0x18
#define DW_IC_FS_SCL_HCNT                   0x1c
#define DW_IC_FS_SCL_LCNT                   0x20
#define DW_IC_HS_SCL_HCNT                   0x24
#define DW_IC_HS_SCL_LCNT                   0x28
#define DW_IC_INTR_STAT                     0x2c
#define DW_IC_INTR_MASK                     0x30
#define DW_IC_RAW_INTR_STAT                 0x34
#define DW_IC_RX_TL                         0x38
#define DW_IC_TX_TL                         0x3c
#define DW_IC_CLR_INTR                      0x40
#define DW_IC_CLR_RX_UNDER                  0x44
#define DW_IC_CLR_RX_OVER                   0x48
#define DW_IC_CLR_TX_OVER                   0x4c
#define DW_IC_CLR_RD_REQ                    0x50
#define DW_IC_CLR_TX_ABRT                   0x54
#define DW_IC_CLR_RX_DONE                   0x58
#define DW_IC_CLR_ACTIVITY                  0x5c
#define DW_IC_CLR_STOP_DET                  0x60
#define DW_IC_CLR_START_DET                 0x64
#define DW_IC_CLR_GEN_CALL                  0x68
#define DW_IC_ENABLE                        0x6c
#define DW_IC_STATUS                        0x70
#define DW_IC_TXFLR                         0x74
#define DW_IC_RXFLR                         0x78
#define DW_IC_SDA_HOLD                      0x7c
#define DW_IC_TX_ABRT_SOURCE                0x80
#define DW_IC_SLV_DATA_NACK_ONLY            0x84
#define DW_IC_DMA_CR                        0x88
#define DW_IC_DMA_TDLR                      0x8c
#define DW_IC_DMA_RDLR                      0x90
#define DW_IC_SDA_SETUP                     0x94
#define DW_IC_ACK_GENERAL_CALL              0x98
#define DW_IC_ENABLE_STATUS                 0x9c
#define DW_IC_FS_SPKLEN                     0xa0
#define DW_IC_HS_SPKLEN                     0xa4
#define DW_IC_COMP_PARAM_1                  0xf4
#define DW_IC_COMP_VERSION                  0xf8
#define DW_IC_COMP_TYPE                     0xfc
#define DW_IC_SOFT_RESET                    0x204
#define DW_IC_CLOCK_PARAMS                  0x800

/*
 * Registers definition
 */
#define DW_IC_CON_MASTER                    0x01u       // must set this bit for I2C master
#define DW_IC_CON_SPEED_MASK                0x06u       // DW_IC_CON[2:1]
#define DW_IC_CON_SPEED_STD                 0x02u       // STD:  100KHz
#define DW_IC_CON_SPEED_FAST                0x04u       // FAST: 400KHz
#define DW_IC_CON_10BITADDR_MASTER          0x10u
#define DW_IC_CON_7BITADDR_MASTER           0u
#define DW_IC_CON_RESTART_EN                0x20u
#define DW_IC_CON_SLAVE_DISABLE             0x40u       // must set this bit for I2C master

#define DW_IC_TAR_10BITADDR_MASTER          0x1000u     /* 1 << 12 */

#define DW_IC_DATA_CMD_WRITE                0u
#define DW_IC_DATA_CMD_READ                 0x0100u
#define DW_IC_DATA_CMD_STOP                 0x0200u
#define DW_IC_DATA_CMD_RESTART              0x0400u

#define DW_IC_INTR_RX_UNDER                 0x001u
#define DW_IC_INTR_RX_OVER                  0x002u
#define DW_IC_INTR_RX_FULL                  0x004u
#define DW_IC_INTR_TX_OVER                  0x008u
#define DW_IC_INTR_TX_EMPTY                 0x010u
#define DW_IC_INTR_RD_REQ                   0x020u
#define DW_IC_INTR_TX_ABRT                  0x040u
#define DW_IC_INTR_RX_DONE                  0x080u
#define DW_IC_INTR_ACTIVITY                 0x100u
#define DW_IC_INTR_STOP_DET                 0x200u
#define DW_IC_INTR_START_DET                0x400u
#define DW_IC_INTR_GEN_CALL                 0x800u
#define DW_IC_INTR_DEFAULT_MASK             (DW_IC_INTR_RX_FULL     | \
                                             DW_IC_INTR_TX_EMPTY    | \
                                             DW_IC_INTR_TX_ABRT     | \
                                             DW_IC_INTR_STOP_DET)

#define DW_IC_FIFO_DEPTH                    256
#define DW_IC_COMP_PARAM_1_RX_DEPTH_F       8
#define DW_IC_COMP_PARAM_1_TX_DEPTH_F       16

#define DW_IC_TX_ABRT_7B_ADDR_NOACK         0x00000001u /* 1 << 0 */
#define DW_IC_TX_ABRT_10ADDR1_NOACK         0x00000002u /* 1 << 1 */
#define DW_IC_TX_ABRT_10ADDR2_NOACK         0x00000004u /* 1 << 2 */
#define DW_IC_TX_ABRT_TXDATA_NOACK          0x00000008u /* 1 << 3 */
#define DW_IC_TX_ABRT_GCALL_NOACK           0x00000010u /* 1 << 4 */
#define DW_IC_TX_ABRT_GCALL_READ            0x00000020u /* 1 << 5 */
#define DW_IC_TX_ABRT_SBYTE_ACKDET          0x00000080u /* 1 << 7 */
#define DW_IC_TX_ABRT_SBYTE_NORSTRT         0x00000200u /* 1 << 9 */
#define DW_IC_TX_ABRT_10B_RD_NORSTRT        0x00000400u /* 1 << 10 */
#define DW_IC_TX_ABRT_MASTER_DIS            0x00000800u /* 1 << 11 */
#define DW_IC_TX_ARBT_LOST                  0x00001000u /* 1 << 12 */
#define DW_IC_TX_ARBT_USER_ARBT             0x00010000u /* 1 << 16 */

#define DW_IC_TX_ABRT_ADDR_NOACK            (DW_IC_TX_ABRT_7B_ADDR_NOACK | \
                                             DW_IC_TX_ABRT_10ADDR1_NOACK | \
                                             DW_IC_TX_ABRT_10ADDR2_NOACK)

#define DW_IC_ENABLE_STATUS_ENABLE_MASK     0x01u
#define DW_IC_ENABLE_STATUS_ENABLE          0x01u
#define DW_IC_ENABLE_STATUS_DISABLE         0u
#define DW_IC_ENABLE_ABORT                  0x02u


/*
 * SOFT RESET (offset 0x204)
 *
 * Bits   |Access |Default |Description
 *        |Type   |        |
 * -------+-------+--------+---------------------------------------------------------------------------------------------------------------
 * 31:3   |na     |30'h0   |Reserved
 * -------+-------+--------+---------------------------------------------------------------------------------------------------------------
 * 2      |RW     |1'h0    |iDMA Software Reset Control
 *        |       |        | 0 – IP is in reset (Reset Asserted)
 *        |       |        | 1 – IP is NOT at reset (Reset Released)
 * -------+-------+--------+---------------------------------------------------------------------------------------------------------------
 * 1:0    |RW     |2'h0    |reset_i2c (reset_i2c)
 *        |       |        |I2C Host Controller reset.
 *        |       |        |Used to reset the I2C Host Controller by SW control. All I2C Configuration State and Operational State will be
 *        |       |        |forced to the Default state.
 *        |       |        |There is no timing requirement (SW can assert and de-assert in back to back transactions)
 *        |       |        |This reset does NOT impact the LPSS cluster level settings by BIOS, the PCI configuration header information,
 *        |       |        | DMA channel configuration and interrupt assignment/mapping/etc.
 *        |       |        |Driver should re-initialize registers related to Driver context following an I2C host controller reset.
 *        |       |        | 00 = I2C Host Controller is in reset (Reset Asserted)
 *        |       |        | 01 = Reserved
 *        |       |        | 10 = Reserved
 *        |       |        | 11 = I2C Host Controller is NOT at reset (Reset Released)
 */
#define DW_IC_SOFT_RESET_MASK               0x03U
#define DW_IC_SOFT_RESET_ASSERTED           0x00U
#define DW_IC_SOFT_RESET_NOT_ASSERTED       0x03U

#define DW_IC_STATUS_ACTIVITY               0x01u
#define DW_IC_STATUS_TFNF                   0x02u
#define DW_IC_STATUS_TFE                    0x04u
#define DW_IC_STATUS_RFNE                   0x08u
#define DW_IC_STATUS_RFF                    0x10u

#define DW_IC_SDA_HOLD_MIN_VERS             0x3131312AU
// According to the I2C-bus specification from Philips Semiconductors:
// A device must internally provide a hold time of at least 300 ns for the SDA signal
// to bridge the undefined region of the falling edge of SCL.
#define SDA_HOLD_TIME_VALUE_ns              350U

#define DW_IC_COMP_TYPE_VALUE               0x44570140U

#endif /* DWC_I2C_H__INCLUDED */

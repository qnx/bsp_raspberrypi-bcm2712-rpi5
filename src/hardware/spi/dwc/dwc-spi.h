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


#ifndef _DW_SPI_H_
#define _DW_SPI_H_

#include <atomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <hw/inout.h>
#include <hw/io-spi.h>

#define _SLOG_DEBUG3                             8                // excessive details for debugging

/* Default Macros */
#define SPI0_DEF_BASE                           (0x1f00050000)
#define SPI0_SIZE                               (0x130)
#define DW_SCK_DIV_MIN                          (0x2)
#define DW_SCK_DIV_MAX                          (0xffff)
#define SPI_DFS_DEFAULT                         (8)               // Default spi data frame size
#define CHANNEL_ID_MAX                          (0xffffffffu)

#define DW_NBIT_ROUNDER                         (7)

#define DW_SPI_ENABLE                           (1)               // SSI Enable
#define DW_SPI_DISABLE                          (0)               // SSI Disable

#define SPI_MIN_WORD_WIDTH                      (3)
#define SPI_MAX_WORD_WIDTH                      (32)

#define SPI_FIFO_LEN_DIV                        (2U)

/* Synopsys DW SSI component versions (FourCC sequence) */
#define DW_HSSI_102A                            (0x3130322AU)

#define DW_SPI_PRIORITY                         (0)

//*****************************************************************************************
// Register Values
//*****************************************************************************************
/* DWC SPI controller capabilities */
#define DW_SPI_CAP_CS_OVERRIDE                  (1<<0)
#define DW_SPI_CAP_DFS32                        (1<<1)

/* Register offsets (Generic for both DWC APB SSI and DWC SSI IP-cores) */
#define DW_SPI_CTRLR0                           (0x00)            // SPI Control Register 0
#define DW_SPI_CTRLR1                           (0x04)            // SPI Control Register 1

/* Enable Registers */
#define DW_SPI_SSIENR                           (0x08)            // SPI Enable Register
#define DW_SPI_MWCR                             (0x0c)            // SPI Microwire Control Register
#define DW_SPI_SER                              (0x10)            // SPI Slave Enable Register
#define DW_SPI_BAUDR                            (0x14)            // SPI Baud Rate Select Register

/* TX and RX FIFO Control Registers */
#define DW_SPI_TXFTLR                           (0x18)            // SPI Transmit FIFO Threshold Level Register
#define DW_SPI_RXFTLR                           (0x1c)            // SPI Receive  FIFO Threshold Level Register
#define DW_SPI_TXFLR                            (0x20)            // SPI Transmit FIFO Level Register
#define DW_SPI_RXFLR                            (0x24)            // SPI Receive  FIFO Level Register
#define DW_SPI_SR                               (0x28)            // SPI Status   Register

#define DW_SPI_IMR                              (0x2c)            // SPI Interrupt Mask Register
#define DW_SPI_ISR                              (0x30)            // SPI Interrupt Status Register
#define DW_SPI_RISR                             (0x34)            // SPI Raw Interrupt Status Register
#define DW_SPI_TXOICR                           (0x38)            // SPI Transmit FIFO Overflow Interrupt Clear Register
#define DW_SPI_RXOICR                           (0x3c)            // SPI Receive  FIFO Overflow Interrupt Clear Register
#define DW_SPI_RXUICR                           (0x40)            // SPI Receive FIFO Underflow Interrupt Clear Register
#define DW_SPI_MSTICR                           (0x44)            // SPI Multi-Master Interrupt Clear Register
#define DW_SPI_ICR                              (0x48)            // SPI Interrupt Clear Register
#define DW_SPI_DMACR                            (0x4c)            // DMA Control Register
#define DW_SPI_DMATDLR                          (0x50)            // DMA Transmit Data Level
#define DW_SPI_DMARDLR                          (0x54)            // DMA Receive Data Level
#define DW_SPI_IDR                              (0x58)            // SPI Identification Register
#define DW_SPI_VERSION                          (0x5c)            // SPI Version Register
#define DW_SPI_DR                               (0x60)            // SPI DATA Register for both Read and Write
#define DW_SPI_RX_SAMPLE_DLY                    (0xf0)
#define DW_SPI_CS_OVERRIDE                      (0xf4)

//****************************************************************
// CTRLR0 (DWC APB SSI) Bit fields
//****************************************************************
#define DW_SPI_CTRLR0_IS_MST_OFFSET             (31)

#define DW_PSSI_CTRLR0_DFS_MASK                 (0xf << DW_PSSI_CTRLR0_DFS_OFFSET)             // Dataframe sizer mask
#define DW_PSSI_CTRLR0_DFS32_MASK               (0x1f << DW_PSSI_CTRLR0_DFS32_OFFSET)         // Dataframe sizer mask for 32 bit mode

/* Frame Format Bit fields */
#define DW_SPI_CTRLR0_FRF_MASK                  (0x3 << DW_SPI_CTRLR0_FRF_OFFSET)
#define DW_SPI_CTRLR0_FRF_MOTO_SPI              (0x0)
#define DW_SPI_CTRLR0_FRF_TI_SSP                (0x1)
#define DW_SPI_CTRLR0_FRF_NS_MICROWIRE          (0x2)
#define DW_SPI_CTRLR0_FRF_RESV                  (0x3)

/* Mode Bit values */
#define DW_SPI_CTRLR0_MODE_MASK                 (0x3 << DW_SPI_CTRLR0_SCPHA_OFFSET)
#define DW_SPI_CTRLR0_SCPHA                     (1 << DW_SPI_CTRLR0_SCPHA_OFFSET)
#define DW_SPI_CTRLR0_SCPOL                     (1 << DW_SPI_CTRLR0_SCPOL_OFFSET)

/* Transmit Mode values */
#define DW_SPI_CTRLR0_TMOD_MASK                 (0x3 << DW_SPI_CTRLR0_TMOD_OFFSET)
#define DW_SPI_CTRLR0_TMOD_TR                   (0x0)             // Transmit & Receive
#define DW_SPI_CTRLR0_TMOD_TO                   (0x1)             // Transmit only
#define DW_SPI_CTRLR0_TMOD_RO                   (0x2)             // Receive only
#define DW_SPI_CTRLR0_TMOD_EPROMREAD            (0x3)             // EEPROM read mode

#define DW_PSSI_CTRLR0_SLV_OE                   (1 << DW_PSSI_CTRLR0_SLV_OE_OFFSET)         // DISABLE
#define DW_PSSI_CTRLR0_SRL                      (1 << DW_PSSI_CTRLR0_SRL_OFFSET)           // Serial loopback mode
#define DW_PSSI_CTRLR0_CFS                      (1 << DW_PSSI_CTRLR0_CFS_OFFSET)

#define DW_SPI_SSTE                             (1 << DW_SPI_SSTE_OFFSET)                 // Slave Select Toggle Enable

#define DW_SPI_CTRLR0_SPI_FRF_MASK              (0x3 << DW_SPI_CTRLR0_SPI_FRF_OFFSET)
#define DW_SPI_CTRLR0_SPI_FRF_STD               (0x0)      /* Standard SPI Frame Format */
#define DW_SPI_CTRLR0_SPI_FRF_DUAL              (0x1)      /* Dual SPI Frame Format */
#define DW_SPI_CTRLR0_SPI_FRF_QUAD              (0x2)      /* Quad SPI Frame Format */
#define DW_SPI_CTRLR0_SPI_FRF_OCTAL             (0x3)      /* Octal SPI Frame Format */

//****************************************************************
// Bit fields in CTRLR1.
// Used only for read only mode
//****************************************************************
#define DW_SPI_NDF_MASK                         (0xffff)

//****************************************************************
// Bit fields in SR, 7 bits
//****************************************************************
#define DW_SPI_SR_MASK                          (0x7f)
#define DW_SPI_SR_BUSY                          (1 << 0)          // Spi is busy (0x01)
#define DW_SPI_SR_TF_NOT_FULL                   (1 << 1)          // Transmit FIFO not full (0x02)
#define DW_SPI_SR_TF_EMPTY                      (1 << 2)          // Transmit FIFO empty (0x04)
#define DW_SPI_SR_RF_NOT_EMPTY                  (1 << 3)          // Receive FIFO not empty (0x08)
#define DW_SPI_SR_RF_FULL                       (1 << 4)          // Receive FIFO full (0x10)
#define DW_SPI_SR_TX_ERR                        (1 << 5)          // Transmit Error (0x20)
#define DW_SPI_SR_DCOL                          (1 << 6)          // Data collision err (0x40)

/* Bit fields in ISR, IMR, RISR, 7 bits */
#define DW_SPI_INT_MASK                         (0x1f)
#define DW_SPI_INT_TXEI                         (1 << 0)          // Transmit FIFO Empty Interrupt Mask
#define DW_SPI_INT_TXOI                         (1 << 1)          // Transmit FIFO Overflow Interrupt Mask
#define DW_SPI_INT_RXUI                         (1 << 2)          // Receive FIFO Underflow Interrupt Mask
#define DW_SPI_INT_RXOI                         (1 << 3)          // Receive FIFO Overflow Interrupt Mask
#define DW_SPI_INT_RXFI                         (1 << 4)          // Receive FIFO Full Interrupt Mask
#define DW_SPI_INT_MSTI                         (1 << 5)          // Multi-Master Contention Interrupt Mask

#define SPI_CS_MAX_DEF                          (1)

#define SPI_INTR_DONE                           (0)
#define SPI_INTR_CONTINUE                       (1)
#define SPI_INTR_ERR                            (2)

/* Bit fields in DMACR */
#define DW_SPI_DMA_RDMAE                        (1u << 0)
#define DW_SPI_DMA_TDMAE                        (1u << 1)

typedef struct {
    dma_functions_t     dma_funcs;
    dma_attach_flags    dma_flags;
    void                *rx_ch_handle;
    void                *tx_ch_handle;
    uint32_t            rx_channel;
    uint32_t            tx_channel;
} dw_dma_t;

typedef struct
{
    spi_ctrl_t     *spi_ctrl;                                     // The address of spi_ctrl structure
    spi_bus_t      *bus;                                          // The address of bus structure

    int             iid;                                          // InterruptAttachEvent ID

    uint64_t        pbase;                                        // Physical base address of device registers
    uintptr_t       vbase;                                        // Virtual address of device registers
    uint64_t        map_size;                                     // Size of device registers

    void           *pbuf;                                         // Xfer buffer
    uint32_t        xlen;                                         // Full transaction length requested by client
    uint32_t        tlen;                                         // Transmit counter (in data items)
    uint32_t        rlen;                                         // Receive counter (in data items)
    uint32_t        dlen;                                         // Word size
    uint32_t        dscale;                                       // Right shift to convert from byte count to word count
    uint64_t        xtime_us;                                     // Time for exchange in microseconds
    bool            loopback;                                     // Loopback mode

    uint32_t        cr0;                                          // DW_SPI_CTRLR0 setting
//    uint32_t        cr1;                                          // DW_SPI_CTRLR1 setting

    uint32_t        cs_max;                                       // Chip select max
    bool            sste;                                         // Slave select toggle enable
    uint32_t        fifo_len;                                     // Length of the receive fifo
#ifdef  DW_DMA
    bool            dma_active;
    dw_dma_t        dma;
#endif
} dw_spi_t;

static inline uint32_t dw_read32(const dw_spi_t *const spi, const uint32_t offset)
{
    volatile const uint32_t *const addr = (uint32_t*)(spi->vbase + offset);
    const uint32_t value = *addr;
    return value;
}

static inline void dw_write32(const dw_spi_t *const spi, const uint32_t offset, const uint32_t value)
{
    volatile uint32_t *const addr = (uint32_t*)(spi->vbase + offset);
    *addr = value;
}

/* Function proto */
int     spi_init(spi_bus_t *bus);
int     dw_cfg(const void *const hdl, spi_dev_t *spi_dev);
int     dw_wait(dw_spi_t *spi);
void    dw_write_fifo(const dw_spi_t *const spi, const uint32_t len);
bool    dw_spi_busy(const dw_spi_t *const spi);
void    dw_spi_enable(const  dw_spi_t *const spi, const uint32_t enable_value);
int32_t dw_spi_deselect_slave(const dw_spi_t *const spi);
int32_t dw_spi_select_slave(const dw_spi_t *const spi, const int slv);
int dw_prepare_for_transfer(dw_spi_t *spi, const spi_cfg_t *const cfg, const uint32_t tnbytes, const uint32_t rnbytes);

/* Function interface for io-spi */
void dw_fini(void *const hdl);
void dw_drvinfo(const void *const hdl, spi_drvinfo_t *info);
void dw_devinfo(const void *const hdl, const spi_dev_t *const spi_dev, spi_devinfo_t *const info);
int  dw_setcfg(const void *const hdl, spi_dev_t *spi_dev, const spi_cfg_t *const cfg);
int  dw_xfer(void *const hdl, spi_dev_t *const spi_dev, uint8_t *const buf, const uint32_t tnbytes, const uint32_t rnbytes);

#ifdef  DW_DMA
int dw_spi_init_dma(dw_spi_t *spi);
void dw_spi_fini_dma(dw_spi_t *spi);
int dw_spi_dmaxfer(void *const hdl, spi_dev_t *spi_dev, dma_addr_t *addr, const uint32_t tnbytes, const uint32_t rnbytes);
int dw_spi_dma_allocbuf(void *const hdl, dma_addr_t *addr, const uint32_t len);
int dw_spi_dma_freebuf(void *const hdl, dma_addr_t *addr);
#endif

#endif /* _DW_SPI_H_ */

/*
 * Copyright 2024-2025, BlackBerry Limited.
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
#include "externs.h"
#include <sys/asinfo.h>
#include <libfdt.h>
#include "fdt_utils.h"

/*
 * Raspberry PI5 PCIe RP1 supports 64 MSIX interrupt, these interrupts
 * are mapped to GICv2 interrupt 0xa0~0xdf.
 *
 * According RP1 data sheet, each UART0~5 MSIX vector has its MSIn_CFG.IACK_EN set
 * and requires its MSIn_CFG.IACK set before interrupt is unmasked.
 */
#define BCM2712_RP1_PCIE_MSIX_IRQ            (160U)
#define RP1_PCIE_MSIX_UART0_IRQ              (BCM2712_RP1_PCIE_MSIX_IRQ + 25U)
#define RP1_PCIE_MSIX_UART1_IRQ              (BCM2712_RP1_PCIE_MSIX_IRQ + 42U)
#define RP1_PCIE_MSIX_UART2_IRQ              (BCM2712_RP1_PCIE_MSIX_IRQ + 43U)
#define RP1_PCIE_MSIX_UART3_IRQ              (BCM2712_RP1_PCIE_MSIX_IRQ + 44U)
#define RP1_PCIE_MSIX_UART4_IRQ              (BCM2712_RP1_PCIE_MSIX_IRQ + 45U)
#define RP1_PCIE_MSIX_UART5_IRQ              (BCM2712_RP1_PCIE_MSIX_IRQ + 46U)

#define RP1_PCIE_MSIX_ADDR                   (0x1f00108000ULL)
#define RP1_PCIE_MSIX_SIZE                   (0x8000UL)

#define RP1_PCIE_MSIX_IRQ_25_UART0_SET_REG   (0x800U + 0x6cU)
#define RP1_PCIE_MSIX_IRQ_42_UART1_SET_REG   (0x800U + 0xb0U)
#define RP1_PCIE_MSIX_IRQ_43_UART2_SET_REG   (0x800U + 0xb4U)
#define RP1_PCIE_MSIX_IRQ_44_UART3_SET_REG   (0x800U + 0xb8U)
#define RP1_PCIE_MSIX_IRQ_45_UART4_SET_REG   (0x800U + 0xbcU)
#define RP1_PCIE_MSIX_IRQ_46_UART5_SET_REG   (0x800U + 0xc0U)
#define RP1_PCIE_MSIX_CFG_N_IACK             (1U << 2U)

static uintptr_t rpi_msix_cfg_base = (uintptr_t)MAP_FAILED;
static uint32_t  rpi_msix_cfg_set_reg = 0U;

void variant_intr_init(DEV_PL011 *dev) {

	if (dev->intr == RP1_PCIE_MSIX_UART0_IRQ) {
		rpi_msix_cfg_set_reg = RP1_PCIE_MSIX_IRQ_25_UART0_SET_REG;
	}
	else if (dev->intr == RP1_PCIE_MSIX_UART1_IRQ) {
		rpi_msix_cfg_set_reg = RP1_PCIE_MSIX_IRQ_42_UART1_SET_REG;
	}
	else if (dev->intr == RP1_PCIE_MSIX_UART2_IRQ) {
		rpi_msix_cfg_set_reg = RP1_PCIE_MSIX_IRQ_43_UART2_SET_REG;
	}
	else if (dev->intr == RP1_PCIE_MSIX_UART3_IRQ) {
		rpi_msix_cfg_set_reg = RP1_PCIE_MSIX_IRQ_44_UART3_SET_REG;
	}
	else if (dev->intr == RP1_PCIE_MSIX_UART4_IRQ) {
		rpi_msix_cfg_set_reg = RP1_PCIE_MSIX_IRQ_45_UART4_SET_REG;
	}
	else if (dev->intr == RP1_PCIE_MSIX_UART5_IRQ) {
		rpi_msix_cfg_set_reg = RP1_PCIE_MSIX_IRQ_46_UART5_SET_REG;
	}
	else {
	    /* do nothing */
	    rpi_msix_cfg_set_reg = 0U;
	}

	if (rpi_msix_cfg_set_reg != 0U) {
	    rpi_msix_cfg_base = (uintptr_t)mmap_device_memory (NULL, RP1_PCIE_MSIX_SIZE,
			   (PROT_READ | PROT_WRITE | PROT_NOCACHE), 0, RP1_PCIE_MSIX_ADDR);
		if (rpi_msix_cfg_base == (uintptr_t)MAP_FAILED) {
		    (void)slogf (_SLOG_SETCODE (_SLOGC_CHAR, 0), _SLOG_NOTICE, "Failed to map RP1 PCIe");
		}
	}

	if (dev->is_debug_console != 0U) {
	    const int32_t ret = fdt_debug_console_irq_fix(dev);
	    /* Retrieve IRQ from runtime FDT for default debug console */
	    if (ret != 0) {
            (void)slogf (_SLOG_SETCODE (_SLOGC_CHAR, 0), _SLOG_ERROR, "Unable to get debug console IRQ from FDT, ret: %d", ret);
	    }
	}

	return;
}

void variant_intr_unmask(void)
{
	if ((rpi_msix_cfg_base != (uintptr_t)MAP_FAILED)) {
	    out32(rpi_msix_cfg_base + rpi_msix_cfg_set_reg, RP1_PCIE_MSIX_CFG_N_IACK);
	}
}

int enable_perms(void) {
    return EOK;
}

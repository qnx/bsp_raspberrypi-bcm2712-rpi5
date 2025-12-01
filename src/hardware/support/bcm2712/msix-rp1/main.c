/*
 * Copyright (c) 2024, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <errno.h>
#include <hw/inout.h>

#include <pci/pci.h>
#include <pci/cap_pcie.h>
#include <pci/cap_msix.h>

#ifndef NUM_ELTS
#define NUM_ELTS(x)        (sizeof(x)/sizeof(x[0]))
#endif

#define BCM2712_MIP_INT_CONTROLLER_ADDR   (0x1000130000UL)
#define BCM2712_MIP_INT_CONTROLLER_SIZE   (0xc0U)
  #define MIP_INT_CLEARED                 (0x10U)
  #define MIP_INT_CFGL_HOST               (0x20U)
  #define MIP_INT_CFGH_HOST               (0x30U)
  #define MIP_INT_MASKL_HOST              (0x40U)
  #define MIP_INT_MASKH_HOST              (0x50U)
  #define MIP_INT_MASKL_VPU               (0x60U)
  #define MIP_INT_MASKH_VPU               (0x70U)
  #define MIP_INT_STATUSL_HOST            (0x80U)
  #define MIP_INT_STATUSH_HOST            (0x90U)
  #define MIP_INT_STATUSL_VPU             (0xa0U)
  #define MIP_INT_STATUSH_VPU             (0xb0U)

#define RP1_PCIE_MSIX_ADDR                (0x1f00108000UL)
#define RP1_PCIE_MSIX_SIZE                (0x1000U)
#define RP1_PCIE_MSIX_IRQ_SIZE            (64U)
#define RP1_PCIE_MSIX_IRQ_0_IO_BANK0      (0U)
#define RP1_PCIE_MSIX_IRQ_1_IO_BANK1      (1U)
#define RP1_PCIE_MSIX_IRQ_2_IO_BANK2      (2U)
#define RP1_PCIE_MSIX_IRQ_3_AUDIO_IN      (3U)
#define RP1_PCIE_MSIX_IRQ_4_AUDIO_OUT     (4U)
#define RP1_PCIE_MSIX_IRQ_5_PWM0          (5U)
#define RP1_PCIE_MSIX_IRQ_6_ETH           (6U)
#define RP1_PCIE_MSIX_IRQ_7_I2C0          (7U)
#define RP1_PCIE_MSIX_IRQ_8_I2C1          (8U)
#define RP1_PCIE_MSIX_IRQ_9_I2C2          (9U)
#define RP1_PCIE_MSIX_IRQ_10_I2C3         (10U)
#define RP1_PCIE_MSIX_IRQ_11_I2C4         (11U)
#define RP1_PCIE_MSIX_IRQ_12_I2C5         (12U)
#define RP1_PCIE_MSIX_IRQ_13_I2C6         (13U)
#define RP1_PCIE_MSIX_IRQ_14_I2S0         (14U)
#define RP1_PCIE_MSIX_IRQ_15_I2S1         (15U)
#define RP1_PCIE_MSIX_IRQ_16_I2S2         (16U)
#define RP1_PCIE_MSIX_IRQ_17_SDIO0        (17U)
#define RP1_PCIE_MSIX_IRQ_18_SDIO1        (18U)
#define RP1_PCIE_MSIX_IRQ_19_SPI0         (19U)
#define RP1_PCIE_MSIX_IRQ_20_SPI1         (20U)
#define RP1_PCIE_MSIX_IRQ_21_SPI2         (21U)
#define RP1_PCIE_MSIX_IRQ_22_SPI3         (22U)
#define RP1_PCIE_MSIX_IRQ_23_SPI4         (23U)
#define RP1_PCIE_MSIX_IRQ_24_SPI5         (24U)
#define RP1_PCIE_MSIX_IRQ_25_UART0        (25U)
#define RP1_PCIE_MSIX_IRQ_26_TIMER_0      (26U)
#define RP1_PCIE_MSIX_IRQ_27_TIMER_1      (27U)
#define RP1_PCIE_MSIX_IRQ_28_TIMER_2      (28U)
#define RP1_PCIE_MSIX_IRQ_29_TIMER_3      (29U)

#define RP1_PCIE_MSIX_IRQ_31_USB2         (31U)
#define RP1_PCIE_MSIX_IRQ_36_USB3         (36U)

#define RP1_PCIE_MSIX_IRQ_40_DMA          (40U)
#define RP1_PCIE_MSIX_IRQ_41_PWM1         (41U)
#define RP1_PCIE_MSIX_IRQ_42_UART1        (42U)
#define RP1_PCIE_MSIX_IRQ_43_UART2        (43U)
#define RP1_PCIE_MSIX_IRQ_44_UART3        (44U)
#define RP1_PCIE_MSIX_IRQ_45_UART4        (45U)
#define RP1_PCIE_MSIX_IRQ_46_UART5        (46U)
#define RP1_PCIE_MSIX_IRQ_47_MIPI0        (47U)
#define RP1_PCIE_MSIX_IRQ_48_MIPI1        (48U)
#define RP1_PCIE_MSIX_IRQ_49_VIDEO_OUT    (49U)
#define RP1_PCIE_MSIX_IRQ_50_PIO_0        (50U)
#define RP1_PCIE_MSIX_IRQ_51_PIO_1        (51U)
#define RP1_PCIE_MSIX_IRQ_52_ADC_FIFO     (52U)
#define RP1_PCIE_MSIX_IRQ_53_PCIE_OUT     (53U)
#define RP1_PCIE_MSIX_IRQ_54_SPI6         (54U)
#define RP1_PCIE_MSIX_IRQ_55_SPI7         (55U)
#define RP1_PCIE_MSIX_IRQ_56_SPI8         (56U)
#define RP1_PCIE_MSIX_IRQ_57_PROC_MISC    (57U)
#define RP1_PCIE_MSIX_IRQ_58_SYSCFG       (58U)
#define RP1_PCIE_MSIX_IRQ_59_CLOCKS_DEFAULT (59U)
#define RP1_PCIE_MSIX_IRQ_60_VBUSCTRL     (60U)


#define RPI_PCIE_MSIX_CFG(irq)            (0x08U + ((irq) * 0x04U))
#define RPI_PCIE_MSIX_CFG_RW              (0x000U)
#define RPI_PCIE_MSIX_CFG_SET             (0x800U)
#define RPI_PCIE_MSIX_CFG_CLR             (0xc00U)

/* GIC Interrupt Configuration Registers */
#define GICD_PADDR                        (0x107fff9000UL)
#define GICD_ICFGR                        (0xc00U)
#define RP1_PCIE_MSI_GIC_IRQ_BASE         (0xa0U)

static int_t verbose = 0;
static int_t end_loop = 0;
static void gic_set_msi_irq_edge(const uint_t irq)
{
    static uintptr_t gicd_icfg_base;
    const uint32_t conf_mask = 0x2U << (((irq + RP1_PCIE_MSI_GIC_IRQ_BASE) % 16U) * 2U);
    const uint32_t conf_off = ((irq + RP1_PCIE_MSI_GIC_IRQ_BASE) / 16U) * 4U;
    uint32_t val, oldval;
    if (irq > RP1_PCIE_MSIX_IRQ_SIZE) {
        (void)fprintf(stderr, "Invalid MSIX irq: %u\n", irq);
        return;
    }

    gicd_icfg_base = (uintptr_t) mmap_device_memory(NULL,
            (RP1_PCIE_MSIX_IRQ_SIZE * sizeof(uint32_t)),
            PROT_NOCACHE|PROT_READ|PROT_WRITE,
            0,
            (GICD_PADDR + GICD_ICFGR));
    if (gicd_icfg_base == (uintptr_t) MAP_FAILED) {
        (void)fprintf(stderr, "mmap GICD interrupt configuration failed: %s\n", strerror (errno));
        return;
    }

    oldval = in32(gicd_icfg_base + conf_off);
    val = oldval | conf_mask;
    out32(gicd_icfg_base + conf_off, val);
    if (verbose > 1) {
        (void)fprintf(stderr, "write irq: %u offset: 0x%x from 0x%x to 0x%x\n", irq, conf_off, oldval, val);
    }

    (void)munmap_device_memory((void*)gicd_icfg_base, RP1_PCIE_MSIX_IRQ_SIZE * sizeof(uint32_t));
}

static void rp1_pcie_msix_cfg(const uint_t irq, const uint_t enable)
{
    static uintptr_t rp1_pcie_base;

    if (irq > RP1_PCIE_MSIX_IRQ_SIZE) {
        (void)fprintf(stderr, "Invalid MSIX irq: %u\n", irq);
        return;
    }

    rp1_pcie_base = (uintptr_t) mmap_device_memory(NULL, RP1_PCIE_MSIX_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, RP1_PCIE_MSIX_ADDR);
    if (rp1_pcie_base == (uintptr_t) MAP_FAILED) {
        (void)fprintf(stderr, "mmap (RP1 PCIE) failed: %s\n", strerror (errno));
        return;
    }

    if (enable) {
        switch (irq) {
            case RP1_PCIE_MSIX_IRQ_6_ETH:       /* RP1 ethernet irq */
            case RP1_PCIE_MSIX_IRQ_8_I2C1:      /* RP1 I2C1 irq */
            case RP1_PCIE_MSIX_IRQ_13_I2C6:     /* RP1 I2C6 irq */
            case RP1_PCIE_MSIX_IRQ_19_SPI0:     /* RP1 SPI0 irq */
            case RP1_PCIE_MSIX_IRQ_25_UART0:    /* RP1 UART0 irq */
            case RP1_PCIE_MSIX_IRQ_42_UART1:    /* RP1 UART1 irq */
            case RP1_PCIE_MSIX_IRQ_43_UART2:    /* RP1 UART2 irq */
            case RP1_PCIE_MSIX_IRQ_44_UART3:    /* RP1 UART3 irq */
            case RP1_PCIE_MSIX_IRQ_45_UART4:    /* RP1 UART4 irq */
            case RP1_PCIE_MSIX_IRQ_46_UART5:    /* RP1 UART5 irq */
            case RP1_PCIE_MSIX_IRQ_47_MIPI0:    /* RP1 MIPI0 irq */
            case RP1_PCIE_MSIX_IRQ_48_MIPI1:    /* RP1 MIPI1 irq */
                out32(rp1_pcie_base + RPI_PCIE_MSIX_CFG_SET + RPI_PCIE_MSIX_CFG(irq), 0x9);
                break;

            case RP1_PCIE_MSIX_IRQ_31_USB2:     /* RP1 USB2.0 irq */
                out32(rp1_pcie_base + RPI_PCIE_MSIX_CFG_SET + RPI_PCIE_MSIX_CFG(irq), 0x1);
                break;

            case RP1_PCIE_MSIX_IRQ_36_USB3:     /* RP1 USB3.0 irq */
                out32(rp1_pcie_base + RPI_PCIE_MSIX_CFG_SET + RPI_PCIE_MSIX_CFG(irq), 0x1);
                break;

            default:
                (void)fprintf(stderr, "MSIX irq: %u not supported yet\n", irq);
                break;
        }
    }
    else {
        switch (irq) {
            case RP1_PCIE_MSIX_IRQ_6_ETH:       /* RP1 ethernet irq */
            case RP1_PCIE_MSIX_IRQ_8_I2C1:      /* RP1 I2C1 irq */
            case RP1_PCIE_MSIX_IRQ_13_I2C6:     /* RP1 I2C6 irq */
            case RP1_PCIE_MSIX_IRQ_19_SPI0:     /* RP1 SPI0 irq */
            case RP1_PCIE_MSIX_IRQ_25_UART0:    /* RP1 UART0 irq */
            case RP1_PCIE_MSIX_IRQ_42_UART1:    /* RP1 UART1 irq */
            case RP1_PCIE_MSIX_IRQ_43_UART2:    /* RP1 UART2 irq */
            case RP1_PCIE_MSIX_IRQ_44_UART3:    /* RP1 UART3 irq */
            case RP1_PCIE_MSIX_IRQ_45_UART4:    /* RP1 UART4 irq */
            case RP1_PCIE_MSIX_IRQ_46_UART5:    /* RP1 UART5 irq */
            case RP1_PCIE_MSIX_IRQ_47_MIPI0:    /* RP1 MIPI0 irq */
            case RP1_PCIE_MSIX_IRQ_48_MIPI1:    /* RP1 MIPI1 irq */
                out32(rp1_pcie_base + RPI_PCIE_MSIX_CFG(irq), 0x0);
                break;

            case RP1_PCIE_MSIX_IRQ_31_USB2:     /* RP1 USB2.0 irq */
                out32(rp1_pcie_base + RPI_PCIE_MSIX_CFG_SET + RPI_PCIE_MSIX_CFG(irq), 0x1);
                break;

            case RP1_PCIE_MSIX_IRQ_36_USB3:     /* RP1 USB3.0 irq */
                out32(rp1_pcie_base + RPI_PCIE_MSIX_CFG_SET + RPI_PCIE_MSIX_CFG(irq), 0x1);
                break;

            default:
                (void)fprintf(stderr, "MSIX irq: %u not supported yet\n", irq);
                break;
        }
    }

    (void)munmap_device_memory((void*)rp1_pcie_base, RP1_PCIE_MSIX_SIZE);
}

static void bcm2712_mip_intc_cfg(void)
{
    static uintptr_t mip_intc_base;

    mip_intc_base = (uintptr_t) mmap_device_memory(NULL, BCM2712_MIP_INT_CONTROLLER_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, BCM2712_MIP_INT_CONTROLLER_ADDR);
    if (mip_intc_base == (uintptr_t) MAP_FAILED) {
        (void)fprintf(stderr, "mmap (MIP INTC) failed: %s\n", strerror (errno));
        return;
    }

    out32(mip_intc_base + MIP_INT_MASKL_HOST, 0U);
    out32(mip_intc_base + MIP_INT_MASKH_HOST, 0U);
    out32(mip_intc_base + MIP_INT_MASKL_VPU, 0xffffffffU);
    out32(mip_intc_base + MIP_INT_MASKH_VPU, 0xffffffffU);
    out32(mip_intc_base + MIP_INT_CFGL_HOST, 0x82000040U);
    out32(mip_intc_base + MIP_INT_CFGH_HOST, 0x0001fc10U);

    (void)munmap_device_memory((void*)mip_intc_base, BCM2712_MIP_INT_CONTROLLER_SIZE);
}

int main(const int argc, char *const argv[])
{
    int opt, status = EXIT_SUCCESS;
    pci_err_t ret;
    pci_cap_t cap;
    const pci_bdf_t bdf = PCI_BDF(1, 0, 0);  /* Use 1:0:0 to access devices in PCIe RP1 */
    int_t idx;
    const uint8_t capid = CAPID_MSIX;
    uint_t cleanup = 0;
    uint_t irq_list[RP1_PCIE_MSIX_IRQ_SIZE] = { [0] = 6, [1] = 31, [2] = 36 };
    uint_t irq_num = 3;

    while((opt = getopt(argc, argv, "ci:v")) != -1) {
        switch(opt) {
            case 'c':
                cleanup = 1;
                break;
            case 'i':
                {
                    const char *str = optarg;
                    char *token;
                    irq_num = 0;
                    while (str != NULL) {
                        errno = EOK;
                        irq_list[irq_num] = (uint_t) strtoul(str, NULL, 0);
                        if (errno == EOK) {
                            if (irq_list[irq_num] >= RP1_PCIE_MSIX_IRQ_SIZE) {
                                (void)fprintf(stderr, "Invalid IRQ: %u, make sure IRQ is between 0 ~ %u\n", irq_list[irq_num], RP1_PCIE_MSIX_IRQ_SIZE-1U);
                                exit (EXIT_FAILURE);
                            }
                        }
                        else {
                            (void)fprintf(stderr, "Invalid IRQ entered, make sure IRQ is between 0 ~ %u\n", RP1_PCIE_MSIX_IRQ_SIZE-1U);
                            exit (EXIT_FAILURE);
                        }
                        token = strchr(str, ',');
                        if (token != NULL) {
                            *token++ = '\0';
                        }
                        str = token;
                        irq_num++;
                    }
                }
                break;
            case 'v':
                ++verbose;
                break;
            default:
                break;
        }
    }

    idx = pci_device_find_capid(bdf, capid);
    if (idx < 0) {
        (void)fprintf(stderr, "Cannot find capid %x for B%.2u:D%.2u:F%.2u\n", capid, PCI_BUS(bdf), PCI_DEV(bdf), PCI_FUNC(bdf));
        status = EXIT_FAILURE;
    }
    else {
        cap = NULL;
        ret = pci_device_read_cap(bdf, &cap, (uint_t)idx);

        if (ret != PCI_ERR_OK) {
            (void)fprintf(stderr, "Could not get capability for B%.2u:D%.2u:F%.2u\n", PCI_BUS(bdf), PCI_DEV(bdf), PCI_FUNC(bdf));
            status = EXIT_FAILURE;
        }
        else {
            const uint_t irq_total = cap_msix_get_nirq(cap);
            if (irq_num > irq_total) {
                (void)fprintf(stderr, "RP1 support %u MSI-X IRQs, please specify less IRQs to use\n", irq_total);
                status = EXIT_FAILURE;
            }
            else {
                (void)fprintf(stderr, "RP1 support %u MSI-X IRQs, setup %u IRQs\n", irq_total, irq_num);

                const pci_devhdl_t hdl = pci_device_attach(bdf, pci_attachFlags_DEFAULT, NULL);
                if (hdl == NULL) {
                    (void)fprintf(stderr, "Attach failed\n");
                }
                else {
                    uint_t i;
                    const bool_t enabled = pci_device_cfg_cap_isenabled(hdl, cap);
                    if (verbose > 0) {
                        (void)fprintf(stderr, "Capability 0x%x is currently %s\n", capid, enabled ? "enabled" : "disabled");
                    }

                    if (cleanup > 0U) {

                        ret = cap_msix_mask_irq_all(hdl, cap);
                        (void)fprintf(stderr, "cap_msix_mask_irq_all from returns %s\n", pci_strerror(ret));

                        for (i=0; i<irq_total; i++) {
                            ret = cap_msix_set_irq_entry(hdl, cap, i, -1 );
                            if (ret == PCI_ERR_OK ) {
                                if (verbose > 0) {
                                    (void)fprintf(stderr, "interrupt resource %u is disabled\n", i);
                                }
                            }
                            else {
                                (void)fprintf(stderr, "disable interrupt resource %u failed: %s\n", i, pci_strerror(ret));
                            }
                        }

                        (void)fprintf(stderr, "Disabling capid 0x%x\n", capid);
                        ret = pci_device_cfg_cap_disable(hdl, pci_reqType_e_MANDATORY, cap);
                        if (ret != PCI_ERR_OK) {
                            (void)fprintf(stderr, "cap disable failed, %s\n", pci_strerror(ret));
                        }

                        ret = pci_device_detach(hdl);
                        if (ret != PCI_ERR_OK) {
                            (void)fprintf(stderr, "pci_device_detach failed, %s\n", pci_strerror(ret));
                            status = EXIT_FAILURE;
                        }
                    }
                    else {
                        ret = cap_msix_mask_irq_all(hdl, cap);
                        if (ret != PCI_ERR_OK) {
                            (void)fprintf(stderr, "Failed to mask all IRQs, %s\n", pci_strerror(ret));
                        }
                        else {
                            if (verbose > 0) {
                                (void)fprintf(stderr, "Mask all IRQs\n");
                            }
                        }

                        for (i=0; i<irq_total; i++) {
                            ret = cap_msix_set_irq_entry(hdl, cap, i, (int_t)i);
                            if (ret == PCI_ERR_OK ) {
                                if (verbose > 1) {
                                    (void)fprintf(stderr, "set interrupt resource %u not to be shared\n", i);
                                }
                            }
                            else {
                                (void)fprintf(stderr, "set interrupt resource %u not to be shared failed: %s\n", i, pci_strerror(ret));
                            }

                            if ((i == RP1_PCIE_MSIX_IRQ_31_USB2) ||
                                (i == RP1_PCIE_MSIX_IRQ_36_USB3) ||
                                (i == RP1_PCIE_MSIX_IRQ_6_ETH) ||
                                (i == RP1_PCIE_MSIX_IRQ_25_UART0) ||
                                (i == RP1_PCIE_MSIX_IRQ_42_UART1) ||
                                (i == RP1_PCIE_MSIX_IRQ_43_UART2) ||
                                (i == RP1_PCIE_MSIX_IRQ_44_UART3) ||
                                (i == RP1_PCIE_MSIX_IRQ_45_UART4) ||
                                (i == RP1_PCIE_MSIX_IRQ_46_UART5) ||
                                (i == RP1_PCIE_MSIX_IRQ_47_MIPI0) ||
                                (i == RP1_PCIE_MSIX_IRQ_48_MIPI1)) {
                                /* set  edge triggered GIC MSIX interrupts */
                                gic_set_msi_irq_edge(i);
                            }
                        }

                        ret = cap_msix_set_nirq(hdl, cap, irq_total);
                        if (ret != PCI_ERR_OK) {
                            (void)fprintf(stderr, "Failed set number of IRQ to %u, %s\n", irq_total, pci_strerror(ret));
                        }
                        else {
                            if (verbose > 0) {
                                (void)fprintf(stderr, "Set number of MSIX irq: %u\n", irq_total);
                            }
                        }

                        ret = pci_device_cfg_cap_enable(hdl, pci_reqType_e_MANDATORY, cap);
                        if (ret != PCI_ERR_OK) {
                            (void)fprintf(stderr, "Failed to enable capid 0x%x, %s\n", capid, pci_strerror(ret));
                        }
                        else {
                            if (verbose > 0) {
                                (void)fprintf(stderr, "enabled capid 0x%x\n", capid);
                            }
                        }
                        for (i=0; i<irq_num; i++) {
                            ret = cap_msix_unmask_irq_entry (hdl, cap, irq_list[i]);
                            if (ret != PCI_ERR_OK) {
                                (void)fprintf(stderr, "cap_msix_unmask_irq_entry %u from returns %s\n", irq_list[i], pci_strerror(ret));
                            }
                            else {
                                if (verbose > 0) {
                                    (void)fprintf(stderr, "unmask IRQ: %u\n", irq_list[i]);
                                }
                            }
                        }
                        ret = cap_msix_unmask_irq_all(hdl, cap);
                        if (ret != PCI_ERR_OK) {
                            (void)fprintf(stderr, "Failed to unmask all IRQs, %s\n", pci_strerror(ret));
                        }
                        else {
                            if (verbose > 0) {
                                (void)fprintf(stderr, "Unmask all IRQs\n");
                            }
                        }

                        if (verbose > 1) {
                            pci_irq_t irq_alloc_list[RP1_PCIE_MSIX_IRQ_SIZE];
                            int_t nirq;
                            ret = pci_device_read_irq(hdl, &nirq, irq_alloc_list);
                            if (ret != PCI_ERR_OK) {
                                (void)fprintf(stderr, "Unable to determine IRQ list, %s\n", pci_strerror(ret));
                            }
                            else {
                                if (nirq < 0) {
                                    (void)fprintf(stderr, "%u IRQ(s) could not be returned, irq_list too small\n", -nirq);
                                    nirq = (int_t) NELEMENTS(irq_alloc_list);    // display the ones we did receive
                                }
                                (void)fprintf(stderr, "%u Assigned IRQ's:  ", nirq);
                                for (i=0; i<(uint_t)nirq; i++) {
                                    if ((i % 8) == 0U) {
                                        (void)fprintf(stderr, "\n\t%.2u [0x%x] ", i, irq_alloc_list[i]);
                                    }
                                    else {
                                        (void)fprintf(stderr, "[0x%x] ", irq_alloc_list[i]);
                                    }
                                }
                                (void)fprintf(stderr, "\n");
                            }
                        }

                        for (i=0; i<irq_num; i++) {
                            /* enable MSIX irq */
                            rp1_pcie_msix_cfg(irq_list[i], 1);
                        }

                        /* config MIP_controller */
                        bcm2712_mip_intc_cfg();

                        while (end_loop == 0) {
                            (void)sleep(1);
                        }
                    }
                }
            }
        }
    }
    return status;
}

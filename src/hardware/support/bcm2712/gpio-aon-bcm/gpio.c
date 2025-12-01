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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <sys/asinfo.h>
#include <libfdt.h>

#define BCM2712_AON_GPIO_BASE          (0x107d517c00UL)
#define BCM2712_AON_GPIO_SIZE          (0x40U)
#define BCM2712_AON_GPIO_NUM           (38U)
#define BCM2712_AON_GPIO_BANK_NUM      (2U)
#define BCM2712_AON_GPIO_BANK0_GPIO    (17U)
#define BCM2712D0_AON_GPIO_BANK0_GPIO  (15U)
#define BCM2712_AON_GPIO_BANK1_GPIO    (6U)

#define BCM2712_AON_PINMUX_BASE     (0x107d510700UL)
#define BCM2712_AON_PINMUX_SIZE     (0x208U)
#define BCM2712D0_AON_PINMUX_SIZE   (0x1c8U)

#define BCM2712_GIO_DATA            (0x04U)
#define BCM2712_GIO_IODIR           (0x08U)

#define BCM2712_PAD_PULL_OFF        (0U)
#define BCM2712_PAD_PULL_DOWN       (1U)
#define BCM2712_PAD_PULL_UP         (2U)

#define DRIVE_UNSET                 (0xFFFFFFFFU)
#define DRIVE_LOW                   (0U)
#define DRIVE_HIGH                  (1U)

#define PULL_UNSET                  (0xFFFFFFFFU)
#define PULL_NONE                   (0U)
#define PULL_DOWN                   (1U)
#define PULL_UP                     (2U)

#define DIR_UNSET                   (0xFFFFFFFFU)
#define DIR_INPUT                   (0U)
#define DIR_OUTPUT                  (1U)

#define FUNC_UNSET                  (0xFFFFFFFFU)
#define FUNC_A0                     (0U)
#define FUNC_A1                     (1U)
#define FUNC_A2                     (2U)
#define FUNC_A3                     (3U)
#define FUNC_A4                     (4U)
#define FUNC_A5                     (5U)
#define RP1_FSEL_SYS_RIO            (FUNC_A5)
#define FUNC_A6                     (6U)
#define FUNC_A7                     (7U)
#define FUNC_A8                     (8U)
#define NUM_ALT_FUNCS               (8U)
#define FUNC_NULL                   (0x1fU)

#define FUNC_IP                     (20U)
#define FUNC_OP                     (21U)
#define FUNC_GP                     (22U)
#define FUNC_NO                     (23U)

#define FUNC_NAME_MAX_LEN           (30U)
#define PULL_TYPE_NAME_MAX_LEN      (4U)

#define BCM2712_AON_FSEL_COUNT      (8U)

#define GPIO_MAX                    (BCM2712_AON_GPIO_NUM)
#define GPIO_BANK_NUM               (BCM2712_AON_GPIO_BANK_NUM)
#define GPIO_BANK_0_WIDTH           (BCM2712_AON_GPIO_BANK0_GPIO)
#define GPIO_BANK_1_WIDTH           (BCM2712_AON_GPIO_BANK1_GPIO)

#define GPIO_BASE                   (BCM2712_AON_GPIO_BASE)
#define GPIO_SIZE                   (BCM2712_AON_GPIO_SIZE)
#define PINMUX_BASE                 (BCM2712_AON_PINMUX_BASE)

static uintptr_t gpio_base_g;
static uintptr_t pinmux_base_g;
static int32_t debug = 0;
static uint32_t gpio_bank_widths[GPIO_BANK_NUM] = { GPIO_BANK_0_WIDTH, GPIO_BANK_1_WIDTH };

static const char* const bcm2712_aon_gpio_fsel_names[] =
{
    "IR_IN           ", "VC_SPI0_CE1_N        ", "VC_TXD3          ", "VC_SDA3           ", "TE0           ", "VC_SDA0         ", "-             ", "-             ",  // 0
    "VC_PWM0_0       ", "VC_SPI0_CE0_N        ", "VC_RXD3          ", "VC_SCL3           ", "TE1           ", "AON_PWM0        ", "VC_SCL0       ", "VC_PWM1_0     ",  // 1
    "VC_PWM0_1       ", "VC_SPI0_MISO         ", "VC_CTS3          ", "CTL_HDMI_5V       ", "FL0           ", "AON_PWM1        ", "IR_IN         ", "VC_PWM1_1     ",  // 2
    "IR_IN           ", "VC_SPI0_MOSI         ", "VC_RTS3          ", "AON_FP_4SEC_RESETB", "FL1           ", "SD_CARD_VOLT_G  ", "AON_GPCLK     ", "-             ",  // 3
    "GPCLK0          ", "VC_SPI0_SCLK         ", "VC_I2CSL_SCL_SCLK", "AON_GPCLK         ", "PM_LED_OUT    ", "AON_PWM0        ", "SD_CARD_PWR0_G", "VC_PWM0_0     ",  // 4
    "GPCLK1          ", "IR_IN                ", "VC_I2CSL_SDA_MISO", "CLK_OBSERVE       ", "AON_PWM1      ", "SD_CARD_PRES_G  ", "VC_PWM0_1     ", "-             ",  // 5
    "UART_TXD_1      ", "VC_TXD4              ", "GPCLK2           ", "CTL_HDMI_5V       ", "VC_TXD0       ", "VC_SPI3_CE0_N   ", "-             ", "-             ",  // 6
    "UART_RXD_1      ", "VC_RXD4              ", "GPCLK0           ", "AON_PWM0          ", "VC_RXD0       ", "VC_SPI3_SCLK    ", "-             ", "-             ",  // 7
    "UART_RTS_1      ", "VC_RTS4              ", "VC_I2CSL_MOSI    ", "CTL_HDMI_5V       ", "VC_RTS0       ", "VC_SPI3_MOSI    ", "-             ", "-             ",  // 8
    "UART_CTS_1      ", "VC_CTS4              ", "VC_I2CSL_CE_N    ", "AON_PWM1          ", "VC_CTS0       ", "VC_SPI3_MISO    ", "-             ", "-             ",  // 9
    "TSIO_CLK_OUT    ", "CTL_HDMI_5V          ", "SC0_AUX1         ", "SPDIF_OUT         ", "VC_SPI5_CE1_N ", "USB_PWRFLT      ", "AON_GPCLK     ", "SD_CARD_VOLT_F",  // 10
    "TSIO_DATA_IN    ", "UART_RTS_0           ", "SC0_AUX2         ", "AUD_FS_CLK0       ", "VC_SPI5_CE0_N ", "USB_VBUS_PRESENT", "VC_RTS2       ", "SD_CARD_PWR0_F",  // 11
    "TSIO_DATA_OUT   ", "UART_CTS_0           ", "VC_RTS0          ", "TSIO_VCTRL        ", "VC_SPI5_MISO  ", "USB_PWRON       ", "VC_CTS2       ", "SD_CARD_PRES_F",  // 12
    "BSC_M1_SDA      ", "UART_TXD_0           ", "VC_TXD0          ", "UUI_TXD           ", "VC_SPI5_MOSI  ", "ARM_TMS         ", "VC_TXD2       ", "VC_SDA3       ",  // 13
    "BSC_M1_SCL      ", "UART_RXD_0           ", "VC_RXD0          ", "UUI_RXD           ", "VC_SPI5_SCLK  ", "ARM_TCK         ", "VC_RXD2       ", "VC_SCL3       ",  // 14
    "IR_IN           ", "AON_FP_4SEC_RESETB   ", "VC_CTS0          ", "PM_LED_OUT        ", "CTL_HDMI_5V   ", "AON_PWM0        ", "AON_GPCLK     ", "-             ",  // 15
    "AON_CPU_STANDBYB", "GPCLK0               ", "PM_LED_OUT       ", "CTL_HDMI_5V       ", "VC_PWM0_0     ", "USB_PWRON       ", "AUD_FS_CLK0   ", "-             ",  // 16

    // Pad out the bank to 32 entries
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 17
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 18
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 19
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 20
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 21
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 22
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 23
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 24
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 25
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 26
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 27
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 28
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 29
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 30
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 31

    "HDMI_TX0_BSC_SCL", "HDMI_TX0_AUTO_I2C_SCL", "BSC_M0_SCL       ", "VC_SCL0           ", "-             ", "-               ", "-             ", "-             ",  // sgpio 0
    "HDMI_TX0_BSC_SDA", "HDMI_TX0_AUTO_I2C_SDA", "BSC_M0_SDA       ", "VC_SDA0           ", "-             ", "-               ", "-             ", "-             ",  // sgpio 1
    "HDMI_TX1_BSC_SCL", "HDMI_TX1_AUTO_I2C_SCL", "BSC_M1_SCL       ", "VC_SCL4           ", "CTL_HDMI_5V   ", "-               ", "-             ", "-             ",  // sgpio 2
    "HDMI_TX1_BSC_SDA", "HDMI_TX1_AUTO_I2C_SDA", "BSC_M1_SDA       ", "VC_SDA4           ", "-             ", "-               ", "-             ", "-             ",  // sgpio 3
    "AVS_PMU_BSC_SCL ", "BSC_M2_SCL           ", "VC_SCL5          ", "CTL_HDMI_5V       ", "-             ", "-               ", "-             ", "-             ",  // sgpio 4
    "AVS_PMU_BSC_SDA ", "BSC_M2_SDA           ", "VC_SDA5          ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // sgpio 5
};

static const char* const bcm2712_d0_aon_gpio_fsel_names[] =
{
    "IR_IN           ", "VC_SPI0_CE1_N        ", "VC_TXD0          ", "VC_SDA3           ", "UART_TXD_0    ", "VC_SDA0         ", "-             ", "-             ",  // 0
    "VC_PWM0_0       ", "VC_SPI0_CE0_N        ", "VC_RXD0          ", "VC_SCL3           ", "UART_RXD_0    ", "AON_PWM0        ", "VC_SCL0       ", "VC_PWM1_0     ",  // 1
    "VC_PWM0_1       ", "VC_SPI0_MISO         ", "VC_CTS0          ", "CTL_HDMI_5V       ", "UART_CTS_0    ", "AON_PWM1        ", "IR_IN         ", "VC_PWM1_1     ",  // 2
    "IR_IN           ", "VC_SPI0_MOSI         ", "VC_RTS0          ", "UART_RTS_0        ", "SD_CARD_VOLT_G", "AON_GPCLK       ", "-             ", "-             ",  // 3
    "GPCLK0          ", "VC_SPI0_SCLK         ", "PM_LED_OUT       ", "AON_PWM0          ", "SD_CARD_PWR0_G", "VC_PWM0_0       ", "-             ", "-             ",  // 4
    "GPCLK1          ", "IR_IN                ", "AON_PWM1         ", "SD_CARD_PRES_G    ", "VC_PWM0_1     ", "-               ", "-             ", "-             ",  // 5
    "UART_TXD_1      ", "VC_TXD2              ", "CTL_HDMI_5V      ", "GPCLK2            ", "VC_SPI3_CE0_N ", "-               ", "-             ", "-             ",  // 6
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 7
    "UART_RTS_1      ", "VC_RTS2              ", "CTL_HDMI_5V      ", "VC_SPI0_CE1_N     ", "VC_SPI3_SCLK  ", "-               ", "-             ", "-             ",  // 8
    "UART_CTS_1      ", "VC_CTS2              ", "VC_CTS0          ", "AON_PWM1          ", "VC_SPI0_CE0_N ", "VC_RTS2         ", "VC_SPI3_MOSI  ", "-             ",  // 9
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 10
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 11
    "UART_RXD_1      ", "VC_RXD2              ", "VC_RTS0          ", "VC_SPI0_MISO      ", "USB_PWRON     ", "VC_CTS2         ", "VC_SPI3_MISO  ", "-             ",  // 12
    "BSC_M1_SDA      ", "VC_TXD0              ", "UUI_TXD          ", "VC_SPI0_MOSI      ", "ARM_TMS       ", "VC_TXD2         ", "VC_SDA3       ", "-             ",  // 13
    "BSC_M1_SCL      ", "AON_GPCLK            ", "VC_RXD0          ", "UUI_RXD           ", "VC_SPI0_SCLK  ", "ARM_TCK         ", "VC_RXD2       ", "VC_SCL3       ",  // 14
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 15
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 16

    // Pad out the bank to 32 entries
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 17
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 18
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 19
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 20
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 21
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 22
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 23
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 24
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 25
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 26
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 27
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 28
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 29
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 30
    "-               ", "-                    ", "-                ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // 31

    "HDMI_TX0_BSC_SCL", "HDMI_TX0_AUTO_I2C_SCL", "BSC_M0_SCL       ", "VC_SCL0           ", "-             ", "-               ", "-             ", "-             ",  // sgpio 0
    "HDMI_TX0_BSC_SDA", "HDMI_TX0_AUTO_I2C_SDA", "BSC_M0_SDA       ", "VC_SDA0           ", "-             ", "-               ", "-             ", "-             ",  // sgpio 1
    "HDMI_TX1_BSC_SCL", "HDMI_TX1_AUTO_I2C_SCL", "BSC_M1_SCL       ", "VC_SCL0           ", "CTL_HDMI_5V   ", "-               ", "-             ", "-             ",  // sgpio 2
    "HDMI_TX1_BSC_SDA", "HDMI_TX1_AUTO_I2C_SDA", "BSC_M1_SDA       ", "VC_SDA0           ", "-             ", "-               ", "-             ", "-             ",  // sgpio 3
    "AVS_PMU_BSC_SCL ", "BSC_M2_SCL           ", "VC_SCL3          ", "CTL_HDMI_5V       ", "-             ", "-               ", "-             ", "-             ",  // sgpio 4
    "AVS_PMU_BSC_SDA ", "BSC_M2_SDA           ", "VC_SDA3          ", "-                 ", "-             ", "-               ", "-             ", "-             ",  // sgpio 5
};

static const char* const bcm2712_aon_gpio_fdt_names[GPIO_MAX] =
{
        "RP1_SDA      ", // 0
        "RP1_SCL      ", // 1
        "RP1_RUN      ", // 2
        "SD_IOVDD_SEL ", // 3
        "SD_PWR_ON    ", // 4
        "SD_CDET_N    ", // 5
        "SD_FLG_N     ", // 6
        "-            ", // 7
        "2712_WAKE    ", // 8
        "2712_STAT_LED", // 9
        "-            ", // 10
        "-            ", // 11
        "PMIC_INT     ", // 12
        "UART_TX_FS   ", // 13
        "UART_RX_FS   ", // 14
        "-            ", // 15
        "-            ", // 16
        "-            ", // 17
        "-            ", // 18
        "-            ", // 19
        "-            ", // 20
        "-            ", // 21
        "-            ", // 22
        "-            ", // 23
        "-            ", // 24
        "-            ", // 25
        "-            ", // 26
        "-            ", // 27
        "-            ", // 28
        "-            ", // 29
        "-            ", // 30
        "-            ", // 31
        "HDMI0_SCL    ", // 32
        "HDMI0_SDA    ", // 33
        "HDMI1_SCL    ", // 34
        "HDMI1_SDA    ", // 35
        "PMIC_SCL     ", // 36
        "PMIC_SDA     ", // 37
};

#define GPIO_UNKNOWN    (0xFFFFFFFFU)
static const uint32_t gpio_aon_bcm_d0_to_c0[GPIO_MAX] =
{
    0,              //  0
    1,              //  1
    2,              //  2
    3,              //  3
    4,              //  4
    5,              //  5
    6,              //  6
    GPIO_UNKNOWN,   //  7
    7,              //  8
    8,              //  9
    GPIO_UNKNOWN,   // 10
    GPIO_UNKNOWN,   // 11
    9,              // 12
    10,             // 13
    11,             // 14
    GPIO_UNKNOWN,   // 15
    GPIO_UNKNOWN,   // 16
    GPIO_UNKNOWN,   // 17
    GPIO_UNKNOWN,   // 18
    GPIO_UNKNOWN,   // 19
    GPIO_UNKNOWN,   // 20
    GPIO_UNKNOWN,   // 21
    GPIO_UNKNOWN,   // 22
    GPIO_UNKNOWN,   // 23
    GPIO_UNKNOWN,   // 24
    GPIO_UNKNOWN,   // 25
    GPIO_UNKNOWN,   // 26
    GPIO_UNKNOWN,   // 27
    GPIO_UNKNOWN,   // 28
    GPIO_UNKNOWN,   // 29
    GPIO_UNKNOWN,   // 30
    GPIO_UNKNOWN,   // 31
    32,             // 32
    33,             // 33
    34,             // 34
    35,             // 35
    36,             // 36
    37,             // 37
};
/* pi5 has two types of targets:
 *  - revision 1.0, uses BCM2712 C1 stepping SOC with
 *    FDT pinctrl@7d504100 node has: compatible = "brcm,bcm2712-pinctrl";
 *  - revision 1.1, uses BCM2712 D0 stepping SOC with
 *    FDT pinctrl@7d504100 node has: compatible = "brcm,bcm2712d0-pinctrl";
 */
#define BCM2712_STEPPING_C1         (0U)
#define BCM2712_STEPPING_D0         (1U)
static uint32_t bcm2712_stepping = BCM2712_STEPPING_C1;

struct fdt_info
{
    void *base;
    uint64_t size;
};

static int map_fdt(struct asinfo_entry * const as, char * const name, void * const arg)
{
    struct fdt_info * const fdt = arg;
    fdt->size = (as->end - as->start + 1UL);
    fdt->base = mmap64(0, (size_t)fdt->size, PROT_READ, MAP_SHARED | MAP_PHYS, NOFD, (off_t)as->start);
    if (fdt->base == (void *)MAP_FAILED) {
        (void)printf("mmap64 failed %lx %lx", as->start, fdt->size);
        return 1;
    }

    if (fdt_check_header(fdt->base) != 0) {
        (void)munmap(fdt->base, (size_t)fdt->size);
        fdt->base = (void *)MAP_FAILED;
        (void)printf("FDT header check failed");
        return 1;
    }
    return 0;
}

/*
 * Look for node pinctrl@7d504100 from FDT
 */
static void check_bcm2712_stepping(void)
{
    struct fdt_info         fdt = { (void *)MAP_FAILED, 0 };

    // Map the FDT.
    if (walk_asinfo("fdt", &map_fdt, (void *)&fdt) != 0) {
        (void)printf("FDT not in syspage asinfo\n");
        return;
    }

    if (fdt.base == (void *)MAP_FAILED) {
        return;
    }

    // Find the MMC bus node.
    int node_offset = fdt_node_offset_by_compatible(fdt.base, 0, "brcm,bcm2712d0-aon-pinctrl");
    if (node_offset >= 0) {
        bcm2712_stepping = BCM2712_STEPPING_D0;
        gpio_bank_widths[0] = BCM2712D0_AON_GPIO_BANK0_GPIO;
    }
    else {
        int i;
        node_offset = -1;
        for(i=0; i<2; i++) {
            node_offset = fdt_node_offset_by_compatible(fdt.base, node_offset, "brcm,brcmstb-gpio");
            if (node_offset >= 0) {
                int prop_len;
                struct fdt_property const *prop = fdt_get_property(fdt.base, node_offset, "reg", &prop_len);
                if ((prop != NULL) && (prop_len == (int)(sizeof(fdt32_t)*2U))) {
                    fdt32_t const * data = (fdt32_t const *)prop->data;
                    if (fdt32_to_cpu(data[0]) == (uint32_t)(BCM2712_AON_GPIO_BASE & 0xffffffff)) {
                        prop = fdt_get_property(fdt.base, node_offset, "brcm,gpio-bank-widths", &prop_len);
                        if ((prop != NULL) && (prop_len == (int)(sizeof(fdt32_t)*2UL))) {
                            data = (fdt32_t const *)prop->data;
                            if (fdt32_to_cpu(data[0]) == 15U) {
                                bcm2712_stepping = BCM2712_STEPPING_D0;
                                gpio_bank_widths[0] = BCM2712D0_AON_GPIO_BANK0_GPIO;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    (void)munmap((void *)fdt.base, fdt.size);
}

static size_t pinmux_size(void)
{
    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
        return (size_t) BCM2712D0_AON_PINMUX_SIZE;
    }
    else {
        return (size_t) BCM2712_AON_PINMUX_SIZE;
    }
}

static void print_funcs(const bool all_pins, const uint64_t pin_mask)
{
    uint32_t pin, bank, gpio_offset;
    const char * name = NULL;

    (void)printf("GPIO %-13s %-16s %-21s %-17s %-18s %-14s %-16s %-14s %-13s\n", "Name", "ALT1", "ALT2", "ALT3", "ALT4", "ALT5", "ALT6", "ALT7", "ALT8");
    (void)printf("---- ------------- ---------------- --------------------- ----------------- ------------------ -------------- ---------------- -------------- -------------\n");
    for (pin = 0; pin < GPIO_MAX; pin++) {
        bank = pin / 32U;
        gpio_offset = pin % 32U;
        if ((bank < GPIO_BANK_NUM) && (gpio_offset < gpio_bank_widths[bank])) {
            if ((all_pins) || (pin_mask & (1UL << pin))) {
                (void)printf("%-4d", pin);
                (void)printf(" %13s", bcm2712_aon_gpio_fdt_names[pin]);

                for (uint32_t alt = 0; alt < BCM2712_AON_FSEL_COUNT; alt++) {

                    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                        name = (const char *) bcm2712_d0_aon_gpio_fsel_names[(pin * BCM2712_AON_FSEL_COUNT) + alt];
                    }
                    else {
                        name = (const char *) bcm2712_aon_gpio_fsel_names[(pin * BCM2712_AON_FSEL_COUNT) + alt];
                    }

                    if (alt == 0U) {
                        (void)printf(" %16s", name);
                    }
                    else if (alt == 1U) {
                        (void)printf(" %21s", name);
                    }
                    else if (alt == 2U) {
                        (void)printf(" %17s", name);
                    }
                    else if (alt == 3U) {
                        (void)printf(" %18s", name);
                    }
                    else if (alt == 4U) {
                        (void)printf(" %14s", name);
                    }
                    else if (alt == 5U) {
                        (void)printf(" %16s", name);
                    }
                    else if (alt == 6U) {
                        (void)printf(" %14s", name);
                    }
                    else {
                        (void)printf(" %13s", name);
                    }
                }
                (void)printf("\n");
            }
        }
    }
}

static volatile uintptr_t get_gpio_base(const uint32_t gpio, uint32_t *offset)
{
    const uint32_t bank = gpio / 32U;
    const uint32_t gpio_offset = gpio % 32U;                    // gpio within bank
    uintptr_t ptr_ret = (uintptr_t)0;

    if (bank >= GPIO_BANK_NUM) {
        (void)printf("get_gpio_base invalid gpio:%u bank: %u\n", gpio, bank);
    }
    else if (gpio_offset >= gpio_bank_widths[bank]) {
        (void)printf("get_gpio_base invalid gpio:%u bank/widths: %u/%u\n", gpio, bank, gpio_bank_widths[bank]);
    }
    else {
        *offset = gpio_offset;
        ptr_ret = gpio_base_g + (bank * 0x20UL);
    }
    return ptr_ret;
}

static volatile uintptr_t get_pinmux_base(uint32_t gpio, uint32_t *offset)
{
    uint32_t bank = 0;
    uint32_t gpio_offset = 0;     // gpio within bank
    uintptr_t ptr_ret = (uintptr_t)0;
    uint32_t gpio_new = GPIO_UNKNOWN;

    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
        if (gpio < (sizeof(gpio_aon_bcm_d0_to_c0)/sizeof(gpio_aon_bcm_d0_to_c0[0]))) {
            gpio_new = gpio_aon_bcm_d0_to_c0[gpio];
        }
    }
    else {
        gpio_new = gpio;
    }

    if (gpio_new != GPIO_UNKNOWN) {

        bank = gpio_new / 32U;
        gpio_offset = gpio_new % 32U;     // gpio within bank

        /* when logging, still use orignal gpio passed in */
        if (bank >= GPIO_BANK_NUM) {
            (void)printf("get_pinmux_base invalid gpio:%u bank: %u\n", gpio, bank);
        }
        else if (gpio_offset >= gpio_bank_widths[bank]) {
            (void)printf("get_pinmux_base invalid gpio:%u to fit in bank/widths: %u/%u\n", gpio, bank, gpio_bank_widths[bank]);
        }
        else {
            if (bank == 1U)
            {
                if (gpio_offset == 4U)
                {
                    *offset = 0;
                    if (debug > 0) {
                        (void)printf("get_pinmux_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base + 0x4\n",
                                gpio, bank, gpio_offset, *offset);
                    }
                    ptr_ret = pinmux_base_g + sizeof(uint32_t);
                }
                else if (gpio_offset == 5U)
                {
                    *offset = 0;
                    if (debug > 0) {
                        (void)printf("get_pinmux_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base + 0x8\n",
                                gpio, bank, gpio_offset, *offset);
                    }
                    ptr_ret = pinmux_base_g + (2UL * sizeof(uint32_t));
                }
                else
                {
                    *offset = gpio_offset * 4U;
                    if (debug > 0) {
                        (void)printf("get_pinmux_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base\n",
                                gpio, bank, gpio_offset, *offset);
                    }
                    ptr_ret = pinmux_base_g;
                }
            }
            else {
                *offset = (gpio_offset % 8U) * 4U;
                if (debug > 0) {
                    (void)printf("get_pinmux_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base + 0x%lx\n",
                            gpio, bank, gpio_offset, *offset, (3U * sizeof(uint32_t)) + ((gpio_offset / 8U) * sizeof(uint32_t)));
                }
                ptr_ret = pinmux_base_g + (3UL * sizeof(uint32_t)) + ((gpio_offset / 8U) * sizeof(uint32_t));
            }
        }
    }
    return ptr_ret;
}

static volatile uintptr_t get_pad_base(uint32_t gpio, uint32_t *offset)
{
    uint32_t bank = 0;
    uint32_t gpio_offset = 0;     // gpio within bank
    uintptr_t ptr_ret = (uintptr_t)0;
    uint32_t gpio_new = GPIO_UNKNOWN;

    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
        if (gpio < (sizeof(gpio_aon_bcm_d0_to_c0)/sizeof(gpio_aon_bcm_d0_to_c0[0]))) {
            gpio_new = gpio_aon_bcm_d0_to_c0[gpio];
        }
    }
    else {
        gpio_new = gpio;
    }

    if (gpio_new != GPIO_UNKNOWN) {

        bank = gpio_new / 32U;
        gpio_offset = gpio_new % 32U;     // gpio within bank

        /* when logging, still use orignal gpio passed in */
        if (bank >= GPIO_BANK_NUM) {
            (void)printf("get_pad_base invalid gpio:%u bank: %u\n", gpio, bank);
        }
        else if (gpio_offset >= gpio_bank_widths[bank]) {
            (void)printf("get_pad_base invalid gpio:%u to fit in bank/widths: %u/%u\n", gpio, bank, gpio_bank_widths[bank]);
        }
        else {
            if (bank == 0U) {
                if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                    gpio_offset = gpio_new + 84U;
                }
                else {
                    gpio_offset = gpio_new + 100U;
                }
                *offset = (gpio_offset % 15U) * 2U;
                if (debug > 0) {
                    (void)printf("get_pad_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base + 0x%lx\n",
                            gpio, bank, gpio_offset, *offset, (gpio_offset / 15U) * sizeof(uint32_t));
                }
                ptr_ret = pinmux_base_g + ((gpio_offset / 15U) * sizeof(uint32_t));
            }
            else {
                /* no SGPIO pad control */
            }
        }
    }
    return ptr_ret;
}

static void set_dir(const uint32_t gpio, const uint32_t dir)
{
    uint32_t offset = 0;
    volatile const uintptr_t gpio_base = get_gpio_base(gpio, &offset);
    uint32_t gpio_val, gpio_val_new;

    if (gpio_base == (uintptr_t)0) {
        return;
    }

    gpio_val = in32(gpio_base + BCM2712_GIO_IODIR);
    gpio_val_new = gpio_val & ~(1U << offset);
    if (dir == DIR_INPUT) {
        gpio_val_new |= (1U << offset);
    }
    if (debug > 0) {
        (void)printf("set_dir gpio: %u bit offset: %u dir: %u gpio_base[0x%x]: 0x%x -> 0x%x\n",
                gpio, offset, dir, BCM2712_GIO_IODIR, gpio_val, gpio_val_new);
    }
    out32(gpio_base + BCM2712_GIO_IODIR, gpio_val_new);
    return;
}

static uint32_t get_dir(const uint32_t gpio)
{
    uint32_t offset = 0;
    volatile uintptr_t const gpio_base = get_gpio_base(gpio, &offset);
    uint32_t gpio_val;
    uint32_t dir;

    if (gpio_base == (uintptr_t)0) {
        return DIR_UNSET;
    }

    gpio_val = in32(gpio_base + BCM2712_GIO_IODIR);
    if ((gpio_val & (1U << offset)) != 0UL) {
        dir = DIR_INPUT;
    }
    else {
        dir = DIR_OUTPUT;
    }
    if (debug > 0) {
        (void)printf("get_dir gpio: %u bit offset: %u read gpio_base[0x%x]: 0x%x, return %u\n",
                gpio, offset, BCM2712_GIO_IODIR, gpio_val, dir);
    }

    return dir;
}

static uint32_t get_fsel(const uint32_t gpio)
{
    uint32_t offset = 0;
    volatile uintptr_t const pinmux_base = get_pinmux_base(gpio, &offset);
    uint32_t fsel_val;
    uint32_t func_type = FUNC_NULL;

    if (pinmux_base == (uintptr_t)0) {
        return func_type;
    }

    fsel_val = (in32(pinmux_base) >> offset) & 0xfU;

    if (fsel_val == 0U) {
        if (get_dir(gpio) == DIR_OUTPUT) {
            if (debug > 0) {
                (void)printf("get_fsel gpio: %u pinmux_base: %lx offset: %u fsel=0x%x, get_dir(gpio) == DIR_OUTPUT return FUNC_OP = %u\n",
                        gpio, (uint64_t)pinmux_base, offset, fsel_val, FUNC_OP);
            }
            func_type = FUNC_OP;
        }
        else {
            if (debug > 0) {
                (void)printf("get_fsel gpio: %u pinmux_base: %lx offset: %u fsel=0x%x, get_dir(gpio) != DIR_OUTPUT return FUNC_IP = %u\n",
                        gpio, (uint64_t)pinmux_base, offset, fsel_val, FUNC_IP);
            }
            func_type = FUNC_IP;
        }
    }
    else if (fsel_val <= NUM_ALT_FUNCS) {
        if (debug > 0) {
            (void)printf("get_fsel gpio: %u pinmux_base: %lx offset: %u fsel=0x%x, return FUNC_A0 + fsel = %u\n",
                    gpio, (uint64_t)pinmux_base, offset, fsel_val, (FUNC_A0 + fsel_val));
        }
        func_type = FUNC_A0 + fsel_val;
    }
    else if (fsel_val == 0xfUL)  {// Choose one value as a considered NONE
        if (debug > 0) {
            (void)printf("get_fsel gpio: %u pinmux_base: %lx offset: %u fsel=0x%x, return FUNC_NULL = %u\n",
                    gpio, (uint64_t)pinmux_base, offset, fsel_val, FUNC_NULL);
        }
    }
    else {
        // do nothing
    }

    /* Unknown FSEL */
    if (debug > 0) {
        (void)printf("get_fsel gpio: %u pinmux_base: %lx pinmux_bit: %u fsel=0x%x, unknown\n", gpio, (uint64_t)pinmux_base, offset, fsel_val);
    }
    return func_type;
}

static void set_fsel(const uint32_t gpio, const uint32_t func_type)
{
    uint32_t offset = 0;
    volatile uintptr_t const pinmux_base = get_pinmux_base(gpio, &offset);
    uint32_t fsel = 0;
    uint32_t pinmux_val, pinmux_val_new;

    if (pinmux_base == (uintptr_t)0) {
        return;
    }

    if ((func_type == FUNC_IP) || (func_type == FUNC_OP) || (func_type == FUNC_GP)) {
        // Set direction before switching
        if (func_type == FUNC_IP) {
            set_dir(gpio, DIR_INPUT);
        }
        if (func_type == FUNC_OP) {
            set_dir(gpio, DIR_OUTPUT);
        }
    }
    else if (func_type <= NUM_ALT_FUNCS) {
        fsel = func_type - FUNC_A0;
    }
    else {
        if (debug > 0) {
            (void)printf("set_fsel gpio: %u invalid func: %u\n", gpio, func_type);
        }
        return;
    }

    pinmux_val = in32(pinmux_base);

    pinmux_val_new = pinmux_val & (~(0xfU << offset));
    pinmux_val_new |= (fsel << offset);

    if (debug > 0) {
        (void)printf("set_fsel gpio: %u func: %u offset: 0x%x pinmux_val: 0x%x -> 0x%x\n",
                gpio, func_type, offset, pinmux_val, pinmux_val_new);
    }

    out32(pinmux_base, pinmux_val_new);

    return;
}

static uint32_t get_level(const uint32_t gpio)
{
    uint32_t offset = 0;
    volatile uintptr_t const gpio_base = get_gpio_base(gpio, &offset);
    uint32_t gpio_val;
    uint32_t level = 0;

    if (gpio_base != (uintptr_t)0) {
        gpio_val = in32(gpio_base + BCM2712_GIO_DATA);
        if ((gpio_val & (1U << offset)) != 0UL) {
            level = 1;
            if (debug > 0) {
                (void)printf("get_level gpio: %u gpio_base[0x%x]: 0x%x offset: %u, return %u\n",
                        gpio, BCM2712_GIO_DATA, gpio_val, offset, level);
            }
        }
    }
    return level;
}

static void set_drive(const uint32_t gpio, const uint32_t drive)
{
    uint32_t offset = 0;
    volatile uintptr_t const gpio_base = get_gpio_base(gpio, &offset);
    uint32_t gpio_val, gpio_val_new;

    if (gpio_base != (uintptr_t)0) {
        gpio_val = in32(gpio_base + BCM2712_GIO_DATA);
        gpio_val_new = (gpio_val & ~(1U << offset)) | (drive << offset);

        if (debug > 0) {
            (void)printf("set_drive gpio: %u offset: %u, gpio_base[0x%x]: 0x%x -> 0x%x\n",
                    gpio, offset, BCM2712_GIO_DATA, gpio_val, gpio_val_new);
        }
        out32(gpio_base + BCM2712_GIO_DATA, gpio_val_new);
    }
}

static int gpio_fsel_to_namestr(const uint32_t gpio, const uint32_t fsel, char * const name_ptr, const uint32_t name_size)
{
    int ret = EOK;
    const char *name = NULL;

    if (gpio >= GPIO_MAX) {
        return -1;
    }

    switch (fsel) {
        case FUNC_GP:
        case FUNC_A0:
            (void)snprintf(name_ptr, name_size, "GPIO");
            break;
        case FUNC_IP:
            (void)snprintf(name_ptr, name_size, "INPUT");
            break;
        case FUNC_OP:
            (void)snprintf(name_ptr, name_size, "OUTPUT");
            break;
        case FUNC_A1:
        case FUNC_A2:
        case FUNC_A3:
        case FUNC_A4:
        case FUNC_A5:
        case FUNC_A6:
        case FUNC_A7:
        case FUNC_A8:
            if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                name = (const char *) bcm2712_d0_aon_gpio_fsel_names[(gpio * BCM2712_AON_FSEL_COUNT) + fsel - FUNC_A1];
            }
            else {
                name = (const char *) bcm2712_aon_gpio_fsel_names[(gpio * BCM2712_AON_FSEL_COUNT) + fsel - FUNC_A1];
            }
            (void)snprintf(name_ptr, name_size, "ALT%u = %s", fsel, name);
            break;
        case FUNC_NULL:
            (void)snprintf(name_ptr, name_size, "none");
            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}

/*
 * type:
 *   0 = no pull
 *   1 = pull down
 *   2 = pull up
 */
static void set_pull(const uint32_t gpio, const uint32_t pull_type)
{
    uint32_t offset = 0;
    volatile uintptr_t const pad_base = get_pad_base(gpio, &offset);
    uint32_t val, pad_val, pad_val_new;

    if (pad_base == (uintptr_t)0) {
        return;
    }

    switch (pull_type)
    {
    case PULL_NONE:
        val = BCM2712_PAD_PULL_OFF;
        break;
    case PULL_DOWN:
        val = BCM2712_PAD_PULL_DOWN;
        break;
    case PULL_UP:
        val = BCM2712_PAD_PULL_UP;
        break;
    default:
        (void)printf("set_pull gpio:%u invalid pull type: %u", gpio, pull_type);
        return;
    }

    pad_val = in32(pad_base);

    pad_val_new = pad_val & ~(3U << offset);
    pad_val_new |= (val << offset);

    if (debug > 0) {
        (void)printf("set_pull gpio: %u offset: %u, pad_base: 0x%lx pad_val: 0x%x -> 0x%x\n",
                gpio, offset, (uint64_t)pad_base, pad_val, pad_val_new);
    }

    out32(pad_base, pad_val_new);
}

static uint32_t get_pull(const uint32_t gpio)
{
    uint32_t offset = 0;
    volatile uintptr_t const pad_base = get_pad_base(gpio, &offset);
    uint32_t pad_val;
    uint32_t pull_type = PULL_UNSET;

    if (pad_base == (uintptr_t)0) {
        return pull_type;
    }

    pad_val = in32(pad_base) ;
    switch ((pad_val >> offset) & 0x3U)
    {
    case BCM2712_PAD_PULL_OFF:
        if (debug > 0) {
            (void)printf("get_pull gpio: %u offset: 0x%x pad_base: 0x%lx pad_val: 0x%x return PULL_NONE\n", gpio, offset, (uint64_t)pad_base, pad_val);
        }
        pull_type = PULL_NONE;
        break;
    case BCM2712_PAD_PULL_DOWN:
        if (debug > 0) {
            (void)printf("get_pull gpio: %u offset: 0x%x pad_base: 0x%lx pad_val: 0x%x return PULL_DOWN\n", gpio, offset, (uint64_t)pad_base, pad_val);
        }
        pull_type = PULL_DOWN;
        break;
    case BCM2712_PAD_PULL_UP:
        if (debug > 0) {
            (void)printf("get_pull gpio: %u offset: 0x%x pad_base: 0x%lx pad_val: 0x%x return PULL_UP\n", gpio, offset, (uint64_t)pad_base, pad_val);
        }
        pull_type = PULL_UP;
        break;
    default: /* This is an error */
        if (debug > 0) {
            (void)printf("get_pull gpio: %u offset: 0x%x pad_base: 0x%lx pad_val: 0x%x return error\n", gpio, offset, (uint64_t)pad_base, pad_val);
        }
        break;
    }

    return pull_type;
}

static void gpio_get(const uint32_t pinnum)
{
    char func_name[FUNC_NAME_MAX_LEN] = { "" };
    char pullstr[PULL_TYPE_NAME_MAX_LEN];
    uint32_t fsel;
    uint32_t level;
    uint32_t pull;
    uint32_t bank, gpio_offset;
    uint32_t d0_gpio = 0;

    bank = pinnum / 32U;
    gpio_offset = pinnum % 32U;

    if ((pinnum < GPIO_MAX) && (bank < GPIO_BANK_NUM) && (gpio_offset < gpio_bank_widths[bank])) {

        if (bcm2712_stepping == BCM2712_STEPPING_D0) {
            if (pinnum < (sizeof(gpio_aon_bcm_d0_to_c0)/sizeof(gpio_aon_bcm_d0_to_c0[0]))) {
                d0_gpio = gpio_aon_bcm_d0_to_c0[pinnum];
            }
        }

        if (d0_gpio == GPIO_UNKNOWN) {
            /* D0 stepping, none defined gpio */
            (void)printf("GPIO%-2d/%s: pull=-- level=-- func=--\n", pinnum, bcm2712_aon_gpio_fdt_names[pinnum]);
        }
        else {
            const char* const gpio_pull_names[] = {"pn", "pd", "pu", "p?"};
            /* will keep using pinnum, since D0 stepping gpio conversion will be checked later again */
            fsel = get_fsel(pinnum);
            (void)gpio_fsel_to_namestr(pinnum, fsel, func_name, sizeof(func_name));
            level = get_level(pinnum);
            pullstr[0] = '\0';
            pull = get_pull(pinnum);
            if (pull != PULL_UNSET) {
                (void)snprintf(pullstr, sizeof(pullstr), "%s", gpio_pull_names[pull & 3U]);
            }
            else {
                (void)snprintf(pullstr, sizeof(pullstr), "--");
            }

            (void)printf("GPIO%-2d/%s: pull=%s level=%s func=%s\n",
                    pinnum, bcm2712_aon_gpio_fdt_names[pinnum], pullstr, (level==1U)? "hi":"lo", func_name);
        }
    }
}

static void gpio_set(const uint32_t pinnum, const uint32_t func_sel, const uint32_t drive, const uint32_t pull)
{
    uint32_t bank, gpio_offset;

    bank = pinnum / 32U;
    gpio_offset = pinnum % 32U;
    if ((bank >= GPIO_BANK_NUM) || (gpio_offset >= gpio_bank_widths[bank])) {
        return;
    }

    /* set function */
    if (func_sel != FUNC_UNSET) {
        set_fsel(pinnum, func_sel);
    }

    /* set output value (check pin is output first) */
    if (drive != DRIVE_UNSET) {
        if (get_fsel(pinnum) == FUNC_OP) {
            set_drive(pinnum, drive);
        } else {
            (void)printf("Can't set pin value, not an output: %u\n", pinnum);
            return;
        }
    }

    /* set pulls */
    if (pull != PULL_UNSET) {
        set_pull(pinnum, pull);
    }
    return;
}

int main(const int argc, char * const argv[])
{
    bool set = false;
    bool get = false;
    bool funcs = false;
    uint32_t pull = PULL_UNSET;
    uint32_t func_sel = FUNC_UNSET;
    uint32_t drive = DRIVE_UNSET;
    uint64_t pin_mask = 0ULL; /* Enough for 0-63 */
    bool all_pins = false;

    if (argc < 2) {
        (void)printf("Error: No arguments given - try running 'use %s'\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* argc 2 or greater, next arg must be set, get or help */
    get = strcmp(argv[1], "get") == 0;
    set = strcmp(argv[1], "set") == 0;
    funcs = strcmp(argv[1], "funcs") == 0;
    if (!set && !get && !funcs) {
        (void)printf("Error: Invalid argument \"%s\" try \"use %s\"\n", argv[1], argv[0]);
        return EXIT_FAILURE;
    }

    if ((get || funcs) && (argc > 3)) {
        (void)printf("Error: Too many arguments\n");
        return EXIT_FAILURE;
    }

    if ((set) && (argc < 4)) {
        (void)printf("Error: '%s set' expects input in the form '%s set [gpio number] [pin options]'\n",
            argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    /* expect pin number(s) next */
    if (argc > 2) {
        char * pin_opt = argv[2];
        while (pin_opt) {
            uint64_t pin, pin2;
            errno = EOK;
            pin = strtoul(pin_opt, &pin_opt, 10);
            if (((pin == 0UL) && (errno == EINVAL)) ||
                ((pin == ULONG_MAX) && (errno == ERANGE)) ||
                (pin >= GPIO_MAX)) {
                (void)printf("Error: expects gpio between 0 ~ %u\n", GPIO_MAX-1U);
                return EXIT_FAILURE;
            }

            if (*pin_opt == '-') {
                pin_opt++;
                errno = EOK;
                pin2 = strtoul(pin_opt, &pin_opt, 10);
                if (((pin2 == 0UL) && (errno == EINVAL)) ||
                    ((pin2) && (errno == ERANGE)) ||
                    (pin2 >= GPIO_MAX)) {
                    (void)printf("Error: expects gpio between 0 ~ %u\n", GPIO_MAX-1U);
                    return 1;
                }
                if (pin2 < pin) {
                    const uint64_t tmp = pin2;
                    pin2 = pin;
                    pin = tmp;
                }
            } else {
                pin2 = pin;
            }
            while (pin <= pin2) {
                pin_mask |= (1ULL << pin);
                pin++;
            }
            if (*pin_opt == '\0') {
                pin_opt = NULL;
            } else {
                if (*pin_opt != ',') {
                    break;
                }
                pin_opt++;
            }
        }
        if (pin_opt) {
            (void)printf("Error: Unknown GPIO \"%s\"\n", pin_opt);
            return EXIT_FAILURE;
        }
    }

    check_bcm2712_stepping();

    if (set) {
        /* parse set options */
        for (int arg_num = 3; arg_num < argc; arg_num++) {
            if (strcmp(argv[arg_num], "dh") == 0) {
                drive = DRIVE_HIGH;
            } else if (strcmp(argv[arg_num], "dl") == 0) {
                drive = DRIVE_LOW;
            } else if (strcmp(argv[arg_num], "gp") == 0) {
                func_sel = FUNC_GP;
            } else if (strcmp(argv[arg_num], "ip") == 0) {
                func_sel = FUNC_IP;
            } else if (strcmp(argv[arg_num], "op") == 0) {
                func_sel = FUNC_OP;
            } else if (strcmp(argv[arg_num], "no") == 0) {
                func_sel = FUNC_NO;
            } else if (strcmp(argv[arg_num], "a0") == 0) {
                func_sel = FUNC_A0;
            } else if (strcmp(argv[arg_num], "a1") == 0) {
                func_sel = FUNC_A1;
            } else if (strcmp(argv[arg_num], "a2") == 0) {
                func_sel = FUNC_A2;
            } else if (strcmp(argv[arg_num], "a3") == 0) {
                func_sel = FUNC_A3;
            } else if (strcmp(argv[arg_num], "a4") == 0) {
                func_sel = FUNC_A4;
            } else if (strcmp(argv[arg_num], "a5") == 0) {
                func_sel = FUNC_A5;
            } else if (strcmp(argv[arg_num], "a6") == 0) {
                func_sel = FUNC_A6;
            } else if (strcmp(argv[arg_num], "a7") == 0) {
                func_sel = FUNC_A7;
            } else if (strcmp(argv[arg_num], "a8") == 0) {
                func_sel = FUNC_A8;
            } else if (strcmp(argv[arg_num], "pu") == 0) {
                pull = PULL_UP;
            } else if (strcmp(argv[arg_num], "pd") == 0) {
                pull = PULL_DOWN;
            } else if (strcmp(argv[arg_num], "pn") == 0) {
                pull = PULL_NONE;
            } else {
                (void)printf("Unknown argument \"%s\"\n", argv[arg_num]);
                return EXIT_FAILURE;
            }
        }
    }

    /* end arg parsing */
    if (pin_mask == 0ULL) {
        all_pins = true;
    }

    if (funcs) {
        print_funcs(all_pins, pin_mask);
    }
    else {     /* get or set */
        gpio_base_g = (uintptr_t) mmap_device_memory(NULL, GPIO_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, GPIO_BASE);
        if (gpio_base_g == (uintptr_t) MAP_FAILED) {
            (void)printf("mmap (GPIO) failed: %s\n", strerror (errno));
            return 1;
        }
        pinmux_base_g = (uintptr_t) mmap_device_memory(NULL, pinmux_size(), PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, PINMUX_BASE);
        if (pinmux_base_g == (uintptr_t) MAP_FAILED) {
            (void)munmap_device_memory( (void *)gpio_base_g, GPIO_SIZE );
            (void)printf("pinmux_base (PINMUX) failed: %s\n", strerror (errno));
            return 1;
        }
        uint32_t pin;
        for (pin = 0; pin < GPIO_MAX; pin++) {
            if ((all_pins) || (pin_mask & (1ULL << pin))) {
                if (get) {
                    gpio_get(pin);
                }
                else {
                    gpio_set(pin, func_sel, drive, pull);
                }
            }
        }
        (void)munmap_device_memory((void *)pinmux_base_g, pinmux_size());
        (void)munmap_device_memory((void *)gpio_base_g, GPIO_SIZE);
    }

    return EXIT_SUCCESS;
}

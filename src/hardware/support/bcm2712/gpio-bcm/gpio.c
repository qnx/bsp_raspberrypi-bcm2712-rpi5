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

#define BCM2712_GPIO_BASE           (0x107d508500UL)
#define BCM2712_GPIO_SIZE           (0x40U)
#define BCM2712_GPIO_NUM            (36U)
#define BCM2712_GPIO_BANK_NUM       (2U)
#define BCM2712_GPIO_BANK0_GPIO     (32U)
#define BCM2712_GPIO_BANK1_GPIO     (4U)

#define BCM2712_PINMUX_BASE         (0x107d504100UL)
#define BCM2712_PINMUX_SIZE         (0x30U)
#define BCM2712D0_PINMUX_SIZE       (0x20U)

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

#define BCM2712_FSEL_COUNT          (8U)

#define GPIO_MAX                    (BCM2712_GPIO_NUM)
#define GPIO_BANK_NUM               (BCM2712_GPIO_BANK_NUM)
#define GPIO_BANK_0_WIDTH           (BCM2712_GPIO_BANK0_GPIO)
#define GPIO_BANK_1_WIDTH           (BCM2712_GPIO_BANK1_GPIO)

#define GPIO_BASE                   (BCM2712_GPIO_BASE)
#define GPIO_SIZE                   (BCM2712_GPIO_SIZE)
#define PINMUX_BASE                 (BCM2712_PINMUX_BASE)

static uintptr_t gpio_base_g;
static uintptr_t pinmux_base_g;
static int32_t debug = 0;
static const uint32_t gpio_bank_widths[GPIO_BANK_NUM] = { GPIO_BANK_0_WIDTH, GPIO_BANK_1_WIDTH };

static const char* const gpio_bcm_fsel_names[] =
{
    "BSC_M3_SDA             ", "VC_SDA0          ", "GPCLK0           ", "ENET0_LINK        ", "VC_PWM1_0             ", "VC_SPI0_CE1_N         ", "IR_IN          ", "-             ",  // 0
    "BSC_M3_SCL             ", "VC_SCL0          ", "GPCLK1           ", "ENET0_ACTIVITY    ", "VC_PWM1_1             ", "SR_EDM_SENSE          ", "VC_SPI0_CE0_N  ", "VC_TXD3       ",  // 1
    "PDM_CLK                ", "I2S_CLK0_IN      ", "GPCLK2           ", "VC_SPI4_CE1_N     ", "PKT_CLK0              ", "VC_SPI0_MISO          ", "VC_RXD3        ", "-             ",  // 2
    "PDM_DATA0              ", "I2S_LR0_IN       ", "VC_SPI4_CE0_N    ", "PKT_SYNC0         ", "VC_SPI0_MOSI          ", "VC_CTS3               ", "-              ", "-             ",  // 3
    "PDM_DATA1              ", "I2S_DATA0_IN     ", "ARM_RTCK         ", "VC_SPI4_MISO      ", "PKT_DATA0             ", "VC_SPI0_SCLK          ", "VC_RTS3        ", "-             ",  // 4
    "PDM_DATA2              ", "VC_SCL3          ", "ARM_TRST         ", "SD_CARD_LED_E     ", "VC_SPI4_MOSI          ", "PKT_CLK1              ", "VC_PCM_CLK     ", "VC_SDA5       ",  // 5
    "PDM_DATA3              ", "VC_SDA3          ", "ARM_TCK          ", "SD_CARD_WPROT_E   ", "VC_SPI4_SCLK          ", "PKT_SYNC1             ", "VC_PCM_FS      ", "VC_SCL5       ",  // 6
    "I2S_CLK0_OUT           ", "SPDIF_OUT        ", "ARM_TDI          ", "SD_CARD_PRES_E    ", "VC_SDA3               ", "ENET0_RGMII_START_STOP", "VC_PCM_DIN     ", "VC_SPI4_CE1_N ",  // 7
    "I2S_LR0_OUT            ", "AUD_FS_CLK0      ", "ARM_TMS          ", "SD_CARD_VOLT_E    ", "VC_SCL3               ", "ENET0_MII_TX_ERR      ", "VC_PCM_DOUT    ", "VC_SPI4_CE0_N ",  // 8
    "I2S_DATA0_OUT          ", "AUD_FS_CLK0      ", "ARM_TDO          ", "SD_CARD_PWR0_E    ", "ENET0_MII_RX_ERR      ", "SD_CARD_VOLT_C        ", "VC_SPI4_SCLK   ", "-             ",  // 9
    "BSC_M3_SCL             ", "MTSIF_DATA4_ALT1 ", "I2S_CLK0_IN      ", "I2S_CLK0_OUT      ", "VC_SPI5_CE1_N         ", "ENET0_MII_CRS         ", "SD_CARD_PWR0_C ", "VC_SPI4_MOSI  ",  // 10
    "BSC_M3_SDA             ", "MTSIF_DATA5_ALT1 ", "I2S_LR0_IN       ", "I2S_LR0_OUT       ", "VC_SPI5_CE0_N         ", "ENET0_MII_COL         ", "SD_CARD_PRES_C ", "VC_SPI4_MISO  ",  // 11
    "SPI_S_SS0B             ", "MTSIF_DATA6_ALT1 ", "I2S_DATA0_IN     ", "I2S_DATA0_OUT     ", "VC_SPI5_MISO          ", "VC_I2CSL_MOSI         ", "SD0_CLK        ", "SD_CARD_VOLT_D",  // 12
    "SPI_S_MISO             ", "MTSIF_DATA7_ALT1 ", "I2S_DATA1_OUT    ", "USB_VBUS_PRESENT  ", "VC_SPI5_MOSI          ", "VC_I2CSL_CE_N         ", "SD0_CMD        ", "SD_CARD_PWR0_D",  // 13
    "SPI_S_MOSI_OR_BSC_S_SDA", "VC_I2CSL_SCL_SCLK", "ENET0_RGMII_RX_OK", "ARM_TCK           ", "VC_SPI5_SCLK          ", "VC_PWM0_0             ", "VC_SDA4        ", "SD_CARD_PRES_D",  // 14
    "SPI_S_SCK_OR_BSC_S_SCL ", "VC_I2CSL_SDA_MISO", "VC_SPI3_CE1_N    ", "ARM_TMS           ", "VC_PWM0_1             ", "VC_SCL4               ", "GPCLK0         ", "-             ",  // 15
    "SD_CARD_PRES_B         ", "I2S_CLK0_OUT     ", "VC_SPI3_CE0_N    ", "I2S_CLK0_IN       ", "SD0_DAT0              ", "ENET0_RGMII_MDIO      ", "GPCLK1         ", "-             ",  // 16
    "SD_CARD_WPROT_B        ", "I2S_LR0_OUT      ", "VC_SPI3_MISO     ", "I2S_LR0_IN        ", "EXT_SC_CLK            ", "SD0_DAT1              ", "ENET0_RGMII_MDC", "GPCLK2        ",  // 17
    "SD_CARD_LED_B          ", "I2S_DATA0_OUT    ", "VC_SPI3_MOSI     ", "I2S_DATA0_IN      ", "SD0_DAT2              ", "ENET0_RGMII_IRQ       ", "VC_PWM1_0      ", "-             ",  // 18
    "SD_CARD_VOLT_B         ", "USB_PWRFLT       ", "VC_SPI3_SCLK     ", "PKT_DATA1         ", "SPDIF_OUT             ", "SD0_DAT3              ", "IR_IN          ", "VC_PWM1_1     ",  // 19
    "SD_CARD_PWR0_B         ", "UUI_TXD          ", "VC_TXD0          ", "ARM_TMS           ", "UART_TXD_2            ", "USB_PWRON             ", "VC_PCM_CLK     ", "VC_TXD4       ",  // 20
    "USB_PWRFLT             ", "UUI_RXD          ", "VC_RXD0          ", "ARM_TCK           ", "UART_RXD_2            ", "SD_CARD_VOLT_B        ", "VC_PCM_FS      ", "VC_RXD4       ",  // 21
    "USB_PWRON              ", "ENET0_LINK       ", "VC_CTS0          ", "MTSIF_ATS_RST     ", "UART_RTS_2            ", "USB_VBUS_PRESENT      ", "VC_PCM_DIN     ", "VC_SDA5       ",  // 22
    "USB_VBUS_PRESENT       ", "ENET0_ACTIVITY   ", "VC_RTS0          ", "MTSIF_ATS_INC     ", "UART_CTS_2            ", "I2S_DATA2_OUT         ", "VC_PCM_DOUT    ", "VC_SCL5       ",  // 23
    "MTSIF_ATS_RST          ", "PKT_CLK0         ", "UART_RTS_0       ", "ENET0_RGMII_RX_CLK", "ENET0_RGMII_START_STOP", "VC_SDA4               ", "VC_TXD3        ", "-             ",  // 24
    "MTSIF_ATS_INC          ", "PKT_SYNC0        ", "SC0_CLK          ", "UART_CTS_0        ", "ENET0_RGMII_RX_EN_CTL ", "ENET0_RGMII_RX_OK     ", "VC_SCL4        ", "VC_RXD3       ",  // 25
    "MTSIF_DATA1            ", "PKT_DATA0        ", "SC0_IO           ", "UART_TXD_0        ", "ENET0_RGMII_RXD_00    ", "VC_TXD4               ", "VC_SPI5_CE0_N  ", "-             ",  // 26
    "MTSIF_DATA2            ", "PKT_CLK1         ", "SC0_AUX1         ", "UART_RXD_0        ", "ENET0_RGMII_RXD_01    ", "VC_RXD4               ", "VC_SPI5_SCLK   ", "-             ",  // 27
    "MTSIF_CLK              ", "PKT_SYNC1        ", "SC0_AUX2         ", "ENET0_RGMII_RXD_02", "VC_CTS                ", "VC_SPI5_MOSI          ", "-              ", "-             ",  // 28
    "MTSIF_DATA0            ", "PKT_DATA1        ", "SC0_PRES         ", "ENET0_RGMII_RXD_03", "VC_RTS4               ", "VC_SPI5_MISO          ", "-              ", "-             ",  // 29
    "MTSIF_SYNC             ", "PKT_CLK2         ", "SC0_RST          ", "SD2_CLK           ", "ENET0_RGMII_TX_CLK    ", "GPCLK0                ", "VC_PWM0_0      ", "-             ",  // 30
    "MTSIF_DATA3            ", "PKT_SYNC2        ", "SC0_VCC          ", "SD2_CMD           ", "ENET0_RGMII_TX_EN_CTL ", "VC_SPI3_CE1_N         ", "VC_PWM0_1      ", "-             ",  // 31
    "MTSIF_DATA4            ", "PKT_DATA2        ", "SC0_VPP          ", "SD2_DAT0          ", "ENET0_RGMII_TXD_00    ", "VC_SPI3_CE0_N         ", "VC_TXD3        ", "-             ",  // 32
    "MTSIF_DATA5            ", "PKT_CLK3         ", "SD2_DAT1         ", "ENET0_RGMII_TXD_01", "VC_SPI3_SCLK          ", "VC_RXD3               ", "-              ", "-             ",  // 33
    "MTSIF_DATA6            ", "PKT_SYNC3        ", "EXT_SC_CLK       ", "SD2_DAT2          ", "ENET0_RGMII_TXD_02    ", "VC_SPI3_MOSI          ", "VC_SDA5        ", "-             ",  // 34
    "MTSIF_DATA7            ", "PKT_DATA3        ", "SD2_DAT3         ", "ENET0_RGMII_TXD_03", "VC_SPI3_MISO          ", "VC_SCL5               ", "-              ", "-             ",  // 35  FDT stops here
};

static const char* const gpio_bcm_d0_fsel_names[] =
{
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 0
    "VC_SCL0                ", "USB_PWRFLT       ", "GPCLK0           ", "SD_CARD_LED_E     ", "VC_SPI3_CE1_N         ", "SR_EDM_SENSE          ", "VC_SPI0_CE0_N  ", "VC_TXD0       ",  // 1
    "VC_SDA0                ", "USB_PWRON        ", "GPCLK1           ", "SD_CARD_WPROT_E   ", "VC_SPI3_CE0_N         ", "CLK_OBSERVE           ", "VC_SPI0_MISO   ", "VC_RXD0       ",  // 2
    "VC_SCL3                ", "USB_VBUS_PRESENT ", "GPCLK2           ", "SD_CARD_PRES_E    ", "VC_SPI3_MISO          ", "VC_SPI0_MOSI          ", "VC_CTS0        ", "-             ",  // 3
    "VC_SDA3                ", "VC_PWM1_1        ", "VC_SPI3_CE0_N    ", "SD_CARD_VOLT_E    ", "VC_SPI3_MOSI          ", "VC_SPI0_SCLK          ", "VC_RTS0        ", "-             ",  // 4
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 5
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 6
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 7
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 8
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 9
    "BSC_M3_SCL             ", "VC_PWM1_0        ", "VC_SPI3_CE1_N    ", "SD_CARD_PWR0_E    ", "VC_SPI3_SCLK          ", "GPCLK0                ", "-              ", "-             ",  // 10
    "BSC_M3_SDA             ", "VC_SPI3_MISO     ", "CLK_OBSERVE      ", "SD_CARD_PRES_C    ", "GPCLK1                ", "-                     ", "-              ", "-             ",  // 11
    "SPI_S_SS0B             ", "VC_SPI3_MOSI     ", "SD_CARD_PWR0_C   ", "SD_CARD_VOLT_D    ", "-                     ", "-                     ", "-              ", "-             ",  // 12
    "SPI_S_MISO             ", "VC_SPI3_SCLK     ", "SD_CARD_PRES_C   ", "SD_CARD_PWR0_D    ", "-                     ", "-                     ", "-              ", "-             ",  // 13
    "SPI_S_MOSI_OR_BSC_S_SDA", "UUI_TXD          ", "ARM_TCK          ", "VC_PWM0_0         ", "VC_SDA0"               , "SD_CARD_PRES_D        ", "-              ", "-             ",  // 14
    "SPI_S_SCK_OR_BSC_S_SCL" , "UUI_RXD          ", "ARM_TMS          ", "VC_PWM0_1         ", "VC_SCL0"               , "GPCLK0                ", "-              ", "-             ",  // 15
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 16
    "-                      ", "-                ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 17
    "SD_CARD_PRES_F         ", "VC_PWM1_0        ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 18
    "SD_CARD_PWR0_F         ", "USB_PWRFLT       ", "VC_PWM1_1        ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 19
    "VC_SDA3                ", "UUI_TXD          ", "VC_TXD0          ", "ARM_TMS           ", "VC_TXD2"               , "-                     ", "-              ", "-             ",  // 20
    "VC_SCL3                ", "UUI_RXD          ", "VC_RXD0          ", "ARM_TCK           ", "VC_RXD2"               , "-                     ", "-              ", "-             ",  // 21
    "SD_CARD_PRES_F         ", "VC_CTS0          ", "VC_SDA3          ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 22
    "VC_RTS0                ", "VC_SCL3          ", "-                ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 23
    "SD_CARD_PRES_B         ", "VC_SPI0_CE1_N    ", "ARM_TRST         ", "UART_RTS_0        ", "USB_PWRFLT            ", "VC_RTS2               ", "VC_TXD0        ", "-             ",  // 24
    "SD_CARD_WPROT_B        ", "VC_SPI0_CE0_N    ", "ARM_TCK          ", "UART_CTS_0        ", "USB_PWRON             ", "VC_CTS2               ", "VC_RXD0        ", "-             ",  // 25
    "SD_CARD_LED_B          ", "VC_SPI0_MISO     ", "ARM_TDI          ", "UART_TXD_0        ", "USB_VBUS_PRESENT      ", "VC_TXD2               ", "VC_SPI0_CE0_N  ", "-             ",  // 26
    "SD_CARD_VOLT_B         ", "VC_SPI0_MOSI     ", "ARM_TMS          ", "UART_RXD_0        ", "VC_RXD2               ", "VC_SPI0_SCLK          ",  "-             ", "-             ",  // 27
    "SD_CARD_PWR0_B         ", "VC_SPI0_SCLK     ", "ARM_TDO          ", "VC_SDA0           ", "VC_SPI0_MOSI          ", "-                     ", "-              ", "-             ",  // 28
    "ARM_RTCK               ", "VC_SCL0          ", "VC_SPI0_MISO     ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 29
    "SD2_CLK                ", "GPCLK0           ", "VC_PWM0_0        ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 30
    "SD2_CMD                ", "VC_SPI3_CE1_N    ", "VC_PWM0_1        ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 31
    "SD2_DAT0               ", "VC_SPI3_CE0_N    ", "VC_TXD3          ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 32
    "SD2_DAT1               ", "VC_SPI3_SCLK     ", "VC_RXD3          ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 33
    "SD2_DAT2               ", "VC_SPI3_MOSI     ", "VC_SDA5          ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 34
    "SD2_DAT3               ", "VC_SPI3_MISO     ", "VC_SCL5          ", "-                 ", "-                     ", "-                     ", "-              ", "-             ",  // 35
};


static const char * const bcm2712_gpio_fdt_names[GPIO_MAX] =
{
    "-             ",    // 0
    "2712_BOOT_CS_N",    // 1
    "2712_BOOT_MISO",    // 2
    "2712_BOOT_MOSI",    // 3
    "2712_BOOT_SCLK",    // 4
    "-             ",    // 5
    "-             ",    // 6
    "-             ",    // 7
    "-             ",    // 8
    "-             ",    // 9
    "-             ",    // 10
    "-             ",    // 11
    "-             ",    // 12
    "-             ",    // 13
    "PCIE_SDA      ",    // 14
    "PCIE_SCL      ",    // 15
    "-             ",    // 16
    "-             ",    // 17
    "-             ",    // 18
    "-             ",    // 19
    "PWR_GPIO      ",    // 20
    "2712_G21_FS   ",    // 21
    "-             ",    // 22
    "-             ",    // 23
    "BT_RTS        ",    // 24
    "BT_CTS        ",    // 25
    "BT_TXD        ",    // 26
    "BT_RXD        ",    // 27
    "WL_ON         ",    // 28
    "BT_ON         ",    // 29
    "WIFI_SDIO_CLK ",    // 30
    "WIFI_SDIO_CMD ",    // 31
    "WIFI_SDIO_D0  ",    // 32
    "WIFI_SDIO_D1  ",    // 33
    "WIFI_SDIO_D2  ",    // 34
    "WIFI_SDIO_D3  ",    // 35
};

static const char * const bcm2712_d0_gpio_fdt_names[GPIO_MAX] =
{
    "-             ",    // 0
    "2712_BOOT_CS_N",    // 1
    "2712_BOOT_MISO",    // 2
    "2712_BOOT_MOSI",    // 3
    "2712_BOOT_SCLK",    // 4
    "-             ",    // 5
    "-             ",    // 6
    "-             ",    // 7
    "-             ",    // 8
    "-             ",    // 9
    "-             ",    // 10
    "-             ",    // 11
    "-             ",    // 12
    "-             ",    // 13
    "-             ",    // 14
    "-             ",    // 15
    "-             ",    // 16
    "-             ",    // 17
    "-             ",    // 18
    "-             ",    // 19
    "PWR_GPIO      ",    // 20
    "2712_G21_FS   ",    // 21
    "-             ",    // 22
    "-             ",    // 23
    "BT_RTS        ",    // 24
    "BT_CTS        ",    // 25
    "BT_TXD        ",    // 26
    "BT_RXD        ",    // 27
    "WL_ON         ",    // 28
    "BT_ON         ",    // 29
    "WIFI_SDIO_CLK ",    // 30
    "WIFI_SDIO_CMD ",    // 31
    "WIFI_SDIO_D0  ",    // 32
    "WIFI_SDIO_D1  ",    // 33
    "WIFI_SDIO_D2  ",    // 34
    "WIFI_SDIO_D3  ",    // 35
};

#define GPIO_UNKNOWN    (0xFFFFFFFFU)
static const uint32_t gpio_bcm_d0_to_c0[GPIO_MAX] =
{
    GPIO_UNKNOWN,   //  0
    0,              //  1
    1,              //  2
    2,              //  3
    3,              //  4
    GPIO_UNKNOWN,   //  5
    GPIO_UNKNOWN,   //  6
    GPIO_UNKNOWN,   //  7
    GPIO_UNKNOWN,   //  8
    GPIO_UNKNOWN,   //  9
    4,              // 10
    5,              // 11
    6,              // 12
    7,              // 13
    8,              // 14
    9,              // 15
    GPIO_UNKNOWN,   // 16
    GPIO_UNKNOWN,   // 17
    10,             // 18
    11,             // 19
    12,             // 20
    13,             // 21
    14,             // 22
    15,             // 23
    16,             // 24
    17,             // 25
    18,             // 26
    19,             // 27
    20,             // 28
    21,             // 29
    22,             // 30
    23,             // 31
    24,             // 32
    25,             // 33
    26,             // 34
    27,             // 35
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
    int node_offset = fdt_node_offset_by_compatible(fdt.base, 0, "brcm,bcm2712d0-pinctrl");
    if (node_offset >= 0) {
        bcm2712_stepping = BCM2712_STEPPING_D0;
    }
    else {
    }

    (void)munmap((void *)fdt.base, fdt.size);
}

static size_t pinmux_size(void)
{
    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
        return (size_t) BCM2712D0_PINMUX_SIZE;
    }
    else {
        return (size_t) BCM2712_PINMUX_SIZE;
    }
}

static void print_funcs(const bool all_pins, const uint64_t pin_mask)
{
    uint32_t pin, bank, gpio_offset;
    const char * name = NULL;

    (void)printf("GPIO %-14s %-23s %-17s %-17s %-18s %-22s %-22s %-15s %-14s\n", "Name", "ALT1", "ALT2", "ALT3", "ALT4", "ALT5", "ALT6", "ALT7", "ALT8");
    (void)printf("---- -------------- ----------------------- ----------------- ----------------- ------------------ ---------------------- ---------------------- --------------- --------------\n");
    for (pin = 0; pin < GPIO_MAX; pin++) {
        bank = pin / 32U;
        gpio_offset = pin % 32U;
        if ((bank < GPIO_BANK_NUM) && (gpio_offset < gpio_bank_widths[bank])) {
            if ((all_pins) || (pin_mask & (1ULL << pin))) {
                (void)printf("%-4d", pin);
                if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                    (void)printf(" %14s", bcm2712_gpio_fdt_names[pin]);
                }
                else {
                    (void)printf(" %14s", bcm2712_d0_gpio_fdt_names[pin]);
                }

                for (uint32_t alt = 0; alt < BCM2712_FSEL_COUNT; alt++) {

                    if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                        name = (const char *) gpio_bcm_d0_fsel_names[(pin * BCM2712_FSEL_COUNT) + alt];
                    }
                    else {
                        name = (const char *) gpio_bcm_fsel_names[(pin * BCM2712_FSEL_COUNT) + alt];
                    }

                    if (alt == 0U) {
                        (void)printf(" %23s", name);
                    }
                    else if (alt == 1U) {
                        (void)printf(" %17s", name);
                    }
                    else if (alt == 2U) {
                        (void)printf(" %17s", name);
                    }
                    else if (alt == 3U) {
                        (void)printf(" %18s", name);
                    }
                    else if (alt == 4U) {
                        (void)printf(" %22s", name);
                    }
                    else if (alt == 5U) {
                        (void)printf(" %22s", name);
                    }
                    else if (alt == 6U) {
                        (void)printf(" %15s", name);
                    }
                    else {
                        (void)printf(" %14s", name);
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
        if (gpio < (sizeof(gpio_bcm_d0_to_c0)/sizeof(gpio_bcm_d0_to_c0[0]))) {
            gpio_new = gpio_bcm_d0_to_c0[gpio];
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
            *offset = (gpio_offset % 8U) * 4U;
            if (debug > 0) {
                (void)printf("get_pinmux_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base + 0x%lx\n",
                        gpio, bank, gpio_offset, *offset, ((bank * 4U) * sizeof(uint32_t)) + ((gpio_offset / 8U) * sizeof(uint32_t)));
            }
            ptr_ret = pinmux_base_g + ((bank * 4U) * sizeof(uint32_t)) + ((gpio_offset / 8U) * sizeof(uint32_t));
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
        if (gpio < (sizeof(gpio_bcm_d0_to_c0)/sizeof(gpio_bcm_d0_to_c0[0]))) {
            gpio_new = gpio_bcm_d0_to_c0[gpio];
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
            if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                gpio_offset = gpio_new + 65U;
            }
            else {
                gpio_offset = gpio_new + 112U;
            }
            *offset = (gpio_offset % 15U) * 2U;

            if (debug > 0) {
                (void)printf("get_pad_base gpio: %u (%u/%u) bit offset: %u, return pinmux_base + 0x%lx\n",
                        gpio, bank, gpio_offset, *offset, (gpio_offset / 15U) * sizeof(uint32_t));
            }
            ptr_ret = pinmux_base_g + ((gpio_offset / 15U) * sizeof(uint32_t));
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
                name = (const char *) gpio_bcm_d0_fsel_names[(gpio * BCM2712_FSEL_COUNT) + fsel - FUNC_A1];
            }
            else {
                name = (const char *) gpio_bcm_fsel_names[(gpio * BCM2712_FSEL_COUNT) + fsel - FUNC_A1];
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
            if (pinnum < (sizeof(gpio_bcm_d0_to_c0)/sizeof(gpio_bcm_d0_to_c0[0]))) {
                d0_gpio = gpio_bcm_d0_to_c0[pinnum];
            }
        }

        if (d0_gpio == GPIO_UNKNOWN) {
            /* D0 stepping, none defined gpio */
            (void)printf("GPIO%-2d/%s: pull=-- level=-- func=--\n", pinnum, bcm2712_d0_gpio_fdt_names[pinnum]);
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
            if (bcm2712_stepping == BCM2712_STEPPING_D0) {
                (void)printf("GPIO%-2d/%s: pull=%s level=%s func=%s\n",
                        pinnum, bcm2712_d0_gpio_fdt_names[pinnum], pullstr, (level==1U)? "hi":"lo", func_name);
            }
            else {
                (void)printf("GPIO%-2d/%s: pull=%s level=%s func=%s\n",
                        pinnum, bcm2712_gpio_fdt_names[pinnum], pullstr, (level==1U)? "hi":"lo", func_name);
            }
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
                    return EXIT_FAILURE;
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

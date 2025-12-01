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
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <hw/inout.h>

#define RP1_GPIO_BASE              (0x1f000d0000UL)
#define RP1_IO_BANK0_OFFSET        (0x00000000U)
#define RP1_IO_BANK1_OFFSET        (0x00004000U)
#define RP1_IO_BANK2_OFFSET        (0x00008000U)
#define RP1_SYS_RIO_BANK0_OFFSET   (0x00010000U)
#define RP1_SYS_RIO_BANK1_OFFSET   (0x00014000U)
#define RP1_SYS_RIO_BANK2_OFFSET   (0x00018000U)
#define RP1_PADS_BANK0_OFFSET      (0x00020000U)
#define RP1_PADS_BANK1_OFFSET      (0x00024000U)
#define RP1_PADS_BANK2_OFFSET      (0x00028000U)


#define DRIVE_UNSET       (0xFFFFFFFFU)
#define DRIVE_LOW         (0U)
#define DRIVE_HIGH        (1U)

#define PULL_UNSET        (0xFFFFFFFFU)
#define PULL_NONE         (0U)
#define PULL_DOWN         (1U)
#define PULL_UP           (2U)

#define FUNC_UNSET        (0xFFFFFFFFU)
#define FUNC_A0           (0U)
#define FUNC_A1           (1U)
#define FUNC_A2           (2U)
#define FUNC_A3           (3U)
#define FUNC_A4           (4U)
#define FUNC_A5           (5U)
#define RP1_FSEL_SYS_RIO  (FUNC_A5)
#define FUNC_A6           (6U)
#define FUNC_A7           (7U)
#define FUNC_A8           (8U)
#define NUM_ALT_FUNCS     (9U)
#define FUNC_SEL_MASK     (0x1fU)
#define FUNC_NULL         (FUNC_SEL_MASK)


#define FUNC_IP           (20U)
#define FUNC_OP           (21U)
#define FUNC_GP           (22U)
#define FUNC_NO           (23U)

#define RP1_GPIO_NUM      (54U)
#define GPIO_MIN          (0U)
#define GPIO_MAX          (RP1_GPIO_NUM)

#define RP1_PADS_OD_SET   (1U << 7)
#define RP1_PADS_IE_SET   (1U << 6)
#define RP1_PADS_PUE_SET  (1U << 3)
#define RP1_PADS_PDE_SET  (1U << 2)

/* RP1 GPIO io read*/
#define RP1_GPIO_IO_REG_STATUS_OFFSET(offset) ((((offset) * 2U) + 0U) * sizeof(uint32_t))
#define RP1_GPIO_IO_REG_CTRL_OFFSET(offset)   ((((offset) * 2U) + 1U) * sizeof(uint32_t))

/* RP1 GPIO pads read*/
#define RP1_GPIO_PADS_REG_OFFSET(offset)      (sizeof(uint32_t) + ((offset) * sizeof(uint32_t)))

/* RP1 GPIO sys_io read*/
#define RP1_GPIO_SYS_RIO_REG_OUT_OFFSET        (0x0U)
#define RP1_GPIO_SYS_RIO_REG_OE_OFFSET         (0x4U)
#define RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET    (0x8U)

#define RP1_RW_OFFSET     (0x0000U)
#define RP1_XOR_OFFSET    (0x1000U)
#define RP1_SET_OFFSET    (0x2000U)
#define RP1_CLR_OFFSET    (0x3000U)

static const char * const rp1_gpio_fsel_names[] =
{
    "SPI0_SIO3 ", "DPI_PCLK     ", "TXD1         ", "SDA0         ", "-            ", "SYS_RIO00 ", "PROC_RIO00 ", "PIO0       ", "SPI2_CE0 ",    // 0
    "SPI0_SIO2 ", "DPI_DE       ", "RXD1         ", "SCL0         ", "-            ", "SYS_RIO01 ", "PROC_RIO01 ", "PIO1       ", "SPI2_SIO1",    // 1
    "SPI0_CE3  ", "DPI_VSYNC    ", "CTS1         ", "SDA1         ", "IR_RX0       ", "SYS_RIO02 ", "PROC_RIO02 ", "PIO2       ", "SPI2_SIO0",    // 2
    "SPI0_CE2  ", "DPI_HSYNC    ", "RTS1         ", "SCL1         ", "IR_TX0       ", "SYS_RIO03 ", "PROC_RIO03 ", "PIO3       ", "SPI2_SCLK",    // 3
    "GPCLK0    ", "DPI_D0       ", "TXD2         ", "SDA2         ", "RI0          ", "SYS_RIO04 ", "PROC_RIO04 ", "PIO4       ", "SPI3_CE0 ",    // 4
    "GPCLK1    ", "DPI_D1       ", "RXD2         ", "SCL2         ", "DTR0         ", "SYS_RIO05 ", "PROC_RIO05 ", "PIO5       ", "SPI3_SIO1",    // 5
    "GPCLK2    ", "DPI_D2       ", "CTS2         ", "SDA3         ", "DCD0         ", "SYS_RIO06 ", "PROC_RIO06 ", "PIO6       ", "SPI3_SIO0",    // 6
    "SPI0_CE1  ", "DPI_D3       ", "RTS2         ", "SCL3         ", "DSR0         ", "SYS_RIO07 ", "PROC_RIO07 ", "PIO7       ", "SPI3_SCLK",    // 7
    "SPI0_CE0  ", "DPI_D4       ", "TXD3         ", "SDA0         ", "-            ", "SYS_RIO08 ", "PROC_RIO08 ", "PIO8       ", "SPI4_CE0 ",    // 8
    "SPI0_MISO ", "DPI_D5       ", "RXD3         ", "SCL0         ", "-            ", "SYS_RIO09 ", "PROC_RIO09 ", "PIO9       ", "SPI4_SIO0",    // 9
    "SPI0_MOSI ", "DPI_D6       ", "CTS3         ", "SDA1         ", "-            ", "SYS_RIO010", "PROC_RIO010", "PIO10      ", "SPI4_SIO1",    // 10
    "SPI0_SCLK ", "DPI_D7       ", "RTS3         ", "SCL1         ", "-            ", "SYS_RIO011", "PROC_RIO011", "PIO11      ", "SPI4_SCLK",    // 11
    "PWM0_CHAN0", "DPI_D8       ", "TXD4         ", "SDA2         ", "AAUD_LEFT    ", "SYS_RIO012", "PROC_RIO012", "PIO12      ", "SPI5_CE0 ",    // 12
    "PWM0_CHAN1", "DPI_D9       ", "RXD4         ", "SCL2         ", "AAUD_RIGHT   ", "SYS_RIO013", "PROC_RIO013", "PIO13      ", "SPI5_SIO1",    // 13
    "PWM0_CHAN2", "DPI_D10      ", "CTS4         ", "SDA3         ", "TXD0         ", "SYS_RIO014", "PROC_RIO014", "PIO14      ", "SPI5_SIO0",    // 14
    "PWM0_CHAN3", "DPI_D11      ", "RTS4         ", "SCL3         ", "RXD0         ", "SYS_RIO015", "PROC_RIO015", "PIO15      ", "SPI5_SCLK",    // 15
    "SPI1_CE2  ", "DPI_D12      ", "DSI0_TE_EXT  ", "-            ", "CTS0         ", "SYS_RIO016", "PROC_RIO016", "PIO16      ", "-        ",    // 16
    "SPI1_CE1  ", "DPI_D13      ", "DSI1_TE_EXT  ", "-            ", "RTS0         ", "SYS_RIO017", "PROC_RIO017", "PIO17      ", "-        ",    // 17
    "SPI1_CE0  ", "DPI_D14      ", "I2S0_SCLK    ", "PWM0_CHAN2   ", "I2S1_SCLK    ", "SYS_RIO018", "PROC_RIO018", "PIO18      ", "GPCLK1   ",    // 18
    "SPI1_MISO ", "DPI_D15      ", "I2S0_WS      ", "PWM0_CHAN3   ", "I2S1_WS      ", "SYS_RIO019", "PROC_RIO019", "PIO19      ", "-        ",    // 19
    "SPI1_MOSI ", "DPI_D16      ", "I2S0_SDI0    ", "GPCLK0       ", "I2S1_SDI0    ", "SYS_RIO020", "PROC_RIO020", "PIO20      ", "-        ",    // 20
    "SPI1_SCLK ", "DPI_D17      ", "I2S0_SDO0    ", "GPCLK1       ", "I2S1_SDO0    ", "SYS_RIO021", "PROC_RIO021", "PIO21      ", "-        ",    // 21
    "SD0CLK    ", "DPI_D18      ", "I2S0_SDI1    ", "SDA3         ", "I2S1_SDI1    ", "SYS_RIO022", "PROC_RIO022", "PIO22      ", "-        ",    // 22
    "SD0_CMD   ", "DPI_D19      ", "I2S0_SDO1    ", "SCL3         ", "I2S1_SDO1    ", "SYS_RIO023", "PROC_RIO023", "PIO23      ", "-        ",    // 23
    "SD0_DAT0  ", "DPI_D20      ", "I2S0_SDI2    ", "-            ", "I2S1_SDI2    ", "SYS_RIO024", "PROC_RIO024", "PIO24      ", "SPI2_CE1 ",    // 24
    "SD0_DAT1  ", "DPI_D21      ", "I2S0_SDO2    ", "MIC_CLK      ", "I2S1_SDO2    ", "SYS_RIO025", "PROC_RIO025", "PIO25      ", "SPI3_CE1 ",    // 25
    "SD0_DAT2  ", "DPI_D22      ", "I2S0_SDI3    ", "MIC_DAT0     ", "I2S1_SDI3    ", "SYS_RIO026", "PROC_RIO026", "PIO26      ", "SPI5_CE1 ",    // 26
    "SD0_DAT3  ", "DPI_D23      ", "I2S0_SDO3    ", "MIC_DAT1     ", "I2S1_SDO3    ", "SYS_RIO027", "PROC_RIO027", "PIO27      ", "SPI1_CE1 ",    // 27
    "SD1CLK    ", "SDA4         ", "I2S2_SCLK    ", "SPI6_MISO    ", "VBUS_EN0     ", "SYS_RIO10 ", "PROC_RIO10 ", "-          ", "-        ",    // 28
    "SD1_CMD   ", "SCL4         ", "I2S2_WS      ", "SPI6_MOSI    ", "VBUS_OC0     ", "SYS_RIO11 ", "PROC_RIO11 ", "-          ", "-        ",    // 29
    "SD1_DAT0  ", "SDA5         ", "I2S2_SDI0    ", "SPI6_SCLK    ", "TXD5         ", "SYS_RIO12 ", "PROC_RIO12 ", "-          ", "-        ",    // 30
    "SD1_DAT1  ", "SCL5         ", "I2S2_SDO0    ", "SPI6_CE0     ", "RXD5         ", "SYS_RIO13 ", "PROC_RIO13 ", "-          ", "-        ",    // 31
    "SD1_DAT2  ", "GPCLK3       ", "I2S2_SDI1    ", "SPI6_CE1     ", "CTS5         ", "SYS_RIO14 ", "PROC_RIO14 ", "-          ", "-        ",    // 32
    "SD1_DAT3  ", "GPCLK4       ", "I2S2_SDO1    ", "SPI6_CE2     ", "RTS5         ", "SYS_RIO15 ", "PROC_RIO15 ", "-          ", "-        ",    // 33
    "PWM1_CHAN2", "GPCLK3       ", "VBUS_EN0     ", "SDA4         ", "MIC_CLK      ", "SYS_RIO20 ", "PROC_RIO20 ", "-          ", "-        ",    // 34
    "SPI8_CE1  ", "PWM1_CHAN0   ", "VBUS_OC0     ", "SCL4         ", "MIC_DAT0     ", "SYS_RIO21 ", "PROC_RIO21 ", "-          ", "-        ",    // 35
    "SPI8_CE0  ", "TXD5         ", "PCIE_CLKREQ_N", "SDA5         ", "MIC_DAT1     ", "SYS_RIO22 ", "PROC_RIO22 ", "-          ", "-        ",    // 36
    "SPI8_MISO ", "RXD5         ", "MIC_CLK      ", "SCL5         ", "PCIE_CLKREQ_N", "SYS_RIO23 ", "PROC_RIO23 ", "-          ", "-        ",    // 37
    "SPI8_MOSI ", "RTS5         ", "MIC_DAT0     ", "SDA6         ", "AAUD_LEFT    ", "SYS_RIO24 ", "PROC_RIO24 ", "DSI0_TE_EXT", "-        ",    // 38
    "SPI8_SCLK ", "CTS5         ", "MIC_DAT1     ", "SCL6         ", "AAUD_RIGHT   ", "SYS_RIO25 ", "PROC_RIO25 ", "DSI1_TE_EXT", "-        ",    // 39
    "PWM1_CHAN1", "TXD5         ", "SDA4         ", "SPI6_MISO    ", "AAUD_LEFT    ", "SYS_RIO26 ", "PROC_RIO26 ", "-          ", "-        ",    // 40
    "PWM1_CHAN2", "RXD5         ", "SCL4         ", "SPI6_MOSI    ", "AAUD_RIGHT   ", "SYS_RIO27 ", "PROC_RIO27 ", "-          ", "-        ",    // 41
    "GPCLK5    ", "RTS5         ", "VBUS_EN1     ", "SPI6_SCLK    ", "I2S2_SCLK    ", "SYS_RIO28 ", "PROC_RIO28 ", "-          ", "-        ",    // 42
    "GPCLK4    ", "CTS5         ", "VBUS_OC1     ", "SPI6_CE0     ", "I2S2_WS      ", "SYS_RIO29 ", "PROC_RIO29 ", "-          ", "-        ",    // 43
    "GPCLK5    ", "SDA5         ", "PWM1_CHAN0   ", "SPI6_CE1     ", "I2S2_SDI0    ", "SYS_RIO210", "PROC_RIO210", "-          ", "-        ",    // 44
    "PWM1_CHAN3", "SCL5         ", "SPI7_CE0     ", "SPI6_CE2     ", "I2S2_SDO0    ", "SYS_RIO211", "PROC_RIO211", "-          ", "-        ",    // 45
    "GPCLK3    ", "SDA4         ", "SPI7_MOSI    ", "MIC_CLK      ", "I2S2_SDI1    ", "SYS_RIO212", "PROC_RIO212", "DSI0_TE_EXT", "-        ",    // 46
    "GPCLK5    ", "SCL4         ", "SPI7_MISO    ", "MIC_DAT0     ", "I2S2_SDO1    ", "SYS_RIO213", "PROC_RIO213", "DSI1_TE_EXT", "-        ",    // 47
    "PWM1_CHAN0", "PCIE_CLKREQ_N", "SPI7_SCLK    ", "MIC_DAT1     ", "TXD5         ", "SYS_RIO214", "PROC_RIO214", "-          ", "-        ",    // 48
    "SPI8_SCLK ", "SPI7_SCLK    ", "SDA5         ", "AAUD_LEFT    ", "RXD5         ", "SYS_RIO215", "PROC_RIO215", "-          ", "-        ",    // 49
    "SPI8_MISO ", "SPI7_MOSI    ", "SCL5         ", "AAUD_RIGHT   ", "VBUS_EN2     ", "SYS_RIO216", "PROC_RIO216", "-          ", "-        ",    // 50
    "SPI8_MOSI ", "SPI7_MISO    ", "SDA6         ", "AAUD_LEFT    ", "VBUS_OC2     ", "SYS_RIO217", "PROC_RIO217", "-          ", "-        ",    // 51
    "SPI8_CE0  ", "-            ", "SCL6         ", "AAUD_RIGHT   ", "VBUS_EN3     ", "SYS_RIO218", "PROC_RIO218", "-          ", "-        ",    // 52
    "SPI8_CE1  ", "SPI7_CE0     ", "-            ", "PCIE_CLKREQ_N", "VBUS_OC3     ", "SYS_RIO219", "PROC_RIO219", "-          ", "-        ",    // 53
};

static const char * const rp1_gpio_fdt_names[GPIO_MAX] =
{
    "ID_SDA           ",    // 0
    "ID_SCL           ",    // 1
    "-                ",    // 2
    "-                ",    // 3
    "-                ",    // 4
    "-                ",    // 5
    "-                ",    // 6
    "-                ",    // 7
    "-                ",    // 8
    "-                ",    // 9
    "-                ",    // 10
    "-                ",    // 11
    "-                ",    // 12
    "-                ",    // 13
    "-                ",    // 14
    "-                ",    // 15
    "-                ",    // 16
    "-                ",    // 17
    "-                ",    // 18
    "-                ",    // 19
    "-                ",    // 20
    "-                ",    // 21
    "-                ",    // 22
    "-                ",    // 23
    "-                ",    // 24
    "-                ",    // 25
    "-                ",    // 26
    "-                ",    // 27
    "PCIE_RP1_WAKE    ",    // 28
    "FAN_TACH         ",    // 29
    "HOST_SDA         ",    // 30
    "HOST_SCL         ",    // 31
    "ETH_RST_N        ",    // 32
    "-                ",    // 33
    "CD0_IO0_MICCLK   ",    // 34
    "CD0_IO0_MICDAT0  ",    // 35
    "RP1_PCIE_CLKREQ_N",    // 36
    "-                ",    // 37
    "CD0_SDA          ",    // 38
    "CD0_SCL          ",    // 39
    "CD1_SDA          ",    // 40
    "CD1_SCL          ",    // 41
    "USB_VBUS_EN      ",    // 42
    "USB_OC_N         ",    // 43
    "RP1_STAT_LED     ",    // 44
    "FAN_PWM          ",    // 45
    "CD1_IO0_MICCLK   ",    // 46
    "2712_WAKE        ",    // 47
    "CD1_IO1_MICDAT1  ",    // 48
    "EN_MAX_USB_CUR   ",    // 49
    "-                ",    // 50
    "-                ",    // 51
    "-                ",    // 52
    "-                ",    // 53
};

#define BLOCK_SIZE          (0x2c000)

/* Pointer to HW */
static uintptr_t gpio_base;
static const uint32_t rp1_bank_base[] = {0, 28, 34};
static int32_t debug=0;

static const uint32_t gpio_io_bank_offset[] = { RP1_IO_BANK0_OFFSET, RP1_IO_BANK1_OFFSET, RP1_IO_BANK2_OFFSET };
static const uint32_t gpio_pads_bank_offset[] = { RP1_PADS_BANK0_OFFSET, RP1_PADS_BANK1_OFFSET, RP1_PADS_BANK2_OFFSET };
static const uint32_t gpio_sys_rio_bank_offset[] = { RP1_SYS_RIO_BANK0_OFFSET, RP1_SYS_RIO_BANK1_OFFSET, RP1_SYS_RIO_BANK2_OFFSET };

static void rp1_gpio_get_bank_offset(const uint32_t gpio, uint32_t *bank, uint32_t *offset)
{
    *bank = 0;
    *offset = 0;
    if (gpio >= GPIO_MAX)
    {
        (void)printf("Invalid gpio number %u\n", gpio);
        exit (EXIT_FAILURE);
    }

    if (gpio < rp1_bank_base[1]) {
        *bank = 0;
    }
    else if (gpio < rp1_bank_base[2]) {
        *bank = 1;
    }
    else {
        *bank = 2;
    }

   *offset = gpio - rp1_bank_base[*bank];
}

static void print_funcs(const bool all_pins, const uint64_t pin_mask)
{
    uint32_t pin;
    const char * name = NULL;

    (void)printf("GPIO %-17s %-10s %-13s %-13s %-13s %-13s %-10s %-11s %-11s %-9s\n", "Name", "ALT0", "ALT1", "ALT2", "ALT3", "ALT4", "ALT5", "ALT6", "ALT7", "ALT8");
    (void)printf("---- ----------------- ---------- ------------- ------------- ------------- ------------- ---------- ----------- ----------- ---------\n");
    for (pin = GPIO_MIN; pin < GPIO_MAX; pin++) {
        if ((all_pins == true) || (pin_mask & (1UL << pin))) {
            (void)printf("%-4d", pin);
            (void)printf(" %17s", rp1_gpio_fdt_names[pin]);
            for (uint32_t alt = 0; alt < NUM_ALT_FUNCS; alt++) {
                name = (const char *) rp1_gpio_fsel_names[(pin * NUM_ALT_FUNCS) + alt];
                if (alt == FUNC_A0) {
                    (void)printf(" %10s", name);
                }
                else if (alt == FUNC_A1) {
                    (void)printf(" %13s", name);
                }
                else if (alt == FUNC_A2) {
                    (void)printf(" %13s", name);
                }
                else if (alt == FUNC_A3) {
                    (void)printf(" %13s", name);
                }
                else if (alt == FUNC_A4) {
                    (void)printf(" %13s", name);
                }
                else if (alt == FUNC_A5) {
                    (void)printf(" %10s", name);
                }
                else if (alt == FUNC_A6) {
                    (void)printf(" %11s", name);
                }
                else if (alt == FUNC_A7) {
                    (void)printf(" %11s", name);
                }
                else {
                    (void)printf(" %9s", name);
                }
            }
            (void)printf("\n");
        }
    }
}

static void set_gpio_dir(const uint32_t gpio, const uint32_t dir)
{
    uint32_t bank, offset;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    if (dir == FUNC_IP) {
        out32(gpio_base + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_CLR_OFFSET, (1u << offset));
        if (debug > 0) {
            (void)printf("%s gpio:%u bank-offset: %u-%u, write sys_rio oe register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                    (uint64_t)RP1_GPIO_BASE+ gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_CLR_OFFSET, (1u << offset));
        }
    }
    else if (dir == FUNC_OP) {
        out32(gpio_base + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_SET_OFFSET, (1u << offset));
        if (debug > 0) {
            (void)printf("%s gpio:%u bank-offset: %u-%u, write sys_rio oe register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET + RP1_SET_OFFSET, (1u << offset));
        }
    }
}


static uint32_t get_gpio_dir(const uint32_t gpio)
{
    uint32_t bank, offset;
    uint32_t sys_rio_oe_read;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    sys_rio_oe_read = in32(gpio_base + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET);

    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, read sys_rio oe register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE+ gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OE_OFFSET, sys_rio_oe_read);
    }
    if (sys_rio_oe_read & (1U << offset)) {
        return FUNC_OP;
    }
    else {
        return FUNC_IP;
    }
}

static uint32_t get_gpio_fsel(const uint32_t gpio)
{
    uint32_t bank, offset;
    uint32_t io_val;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    io_val = in32(gpio_base + gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset));
    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, io register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE+ gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), io_val);
    }
    io_val &= FUNC_SEL_MASK;
    if (io_val == RP1_FSEL_SYS_RIO) {
        return get_gpio_dir(gpio); /* will return FUNC_IP or FUNC_OP */
    }
    return io_val;
}

static void set_gpio_fsel(const uint32_t gpio, uint32_t func_sel)
{
    uint32_t bank, offset;
    uint32_t fsel;
    uint32_t io_ctrl_val;
    uint32_t orig_pad_val, pad_val;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);

    if ((func_sel == FUNC_IP) || (func_sel == FUNC_OP)) {
        set_gpio_dir(gpio, func_sel);
    }

    if ((func_sel == FUNC_IP) || (func_sel == FUNC_OP) || (func_sel == FUNC_GP)) {
        fsel = RP1_FSEL_SYS_RIO;
    }
    else if (func_sel == FUNC_NO) {
        fsel = FUNC_NULL;
    }
    else {
        fsel = func_sel;
    }

    io_ctrl_val = in32(gpio_base + gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset));
    io_ctrl_val &= ~(FUNC_SEL_MASK);
    io_ctrl_val |= (fsel & FUNC_SEL_MASK);
    out32(gpio_base + gpio_io_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), io_ctrl_val);

    pad_val = in32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset));
    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, read pad register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE + gpio_pads_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), pad_val);
    }
    orig_pad_val = pad_val;
    if (fsel == FUNC_NULL) {
        // Disable input
        pad_val &= ~RP1_PADS_IE_SET;
        // Disable peripheral func output
        pad_val |= RP1_PADS_OD_SET;
    }
    else {
        // Enable input
        pad_val |= RP1_PADS_IE_SET;
        // Enable peripheral func output
        pad_val &= ~RP1_PADS_OD_SET;
    }

    if (pad_val != orig_pad_val) {
        out32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset), pad_val);
        if (debug > 0) {
            (void)printf("%s gpio:%u bank-offset: %u-%u, write pad register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                    (uint64_t)RP1_GPIO_BASE + gpio_pads_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), pad_val);
        }
    }
}

static int get_gpio_level(const uint32_t gpio)
{
    uint32_t bank, offset;
    uint32_t pad_val;
    uint32_t sys_rio_sync_val;
    int level;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    pad_val = in32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset));
    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, read pad register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset), pad_val);
    }
    if (!(pad_val & RP1_PADS_IE_SET)) {
        if (debug > 0) {
            (void)printf("%s gpio:%u error RP1_PADS_IE_SET is not set\n", __func__, gpio);
        }
        return -1;
    }
    sys_rio_sync_val = in32(gpio_base + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET);
    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, read sys_rio register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_SYNC_IN_OFFSET, sys_rio_sync_val);
    }
    level = (sys_rio_sync_val & (1U << offset)) ? 1 : 0;

    return level;
}

static void set_gpio_drive(const unsigned gpio, const uint32_t drive)
{

    uint32_t bank, offset;
    uint32_t sys_rio_out_val;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    sys_rio_out_val = in32(gpio_base + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OUT_OFFSET);
    if (drive == DRIVE_HIGH) {
        sys_rio_out_val |= (1U << offset);
    }
    else if (drive == DRIVE_LOW) {
        sys_rio_out_val &= ~(1U << offset);
    }

    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, write sys_rio register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OUT_OFFSET, sys_rio_out_val);
    }
    out32(gpio_base + gpio_sys_rio_bank_offset[bank] + RP1_GPIO_SYS_RIO_REG_OUT_OFFSET, sys_rio_out_val);
}

static int gpio_fsel_to_namestr(const uint32_t gpio, const uint32_t fsel, char * const name)
{
    if (gpio >= GPIO_MAX) {
        return -1;
    }

    switch (fsel) {
        case FUNC_GP:
            return sprintf(name, "GPIO");
        case FUNC_IP:
            return sprintf(name, "INPUT");
        case FUNC_OP:
            return sprintf(name, "OUTPUT");
        case FUNC_A0:
        case FUNC_A1:
        case FUNC_A2:
        case FUNC_A3:
        case FUNC_A4:
        case FUNC_A5:
        case FUNC_A6:
        case FUNC_A7:
        case FUNC_A8:
            break;
        case FUNC_NULL:
            return sprintf(name, "none");
            break;
        default:
            return -1;
    }
    return sprintf(name, "%s", rp1_gpio_fsel_names[(gpio * NUM_ALT_FUNCS) + fsel]);
}

/*
 * type:
 *   0 = no pull
 *   1 = pull down
 *   2 = pull up
 */
static void gpio_set_pull(const uint32_t gpio, const uint32_t type)
{
    uint32_t bank, offset;
    uint32_t pad_val;

    if (type <= PULL_UP) {
        rp1_gpio_get_bank_offset(gpio, &bank, &offset);
        pad_val = in32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset));
        if (debug > 0) {
            (void)printf("%s gpio:%u bank-offset: %u-%u, read pad register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                    (uint64_t)RP1_GPIO_BASE + gpio_pads_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), pad_val);
        }
        pad_val &= ~(RP1_PADS_PDE_SET | RP1_PADS_PUE_SET);

        if (type == PULL_UP) {
            pad_val |= RP1_PADS_PUE_SET;
        }
        else if (type == PULL_DOWN) {
            pad_val |= RP1_PADS_PDE_SET;
        }

        out32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset), pad_val);
        if (debug > 0) {
            (void)printf("%s gpio:%u bank-offset: %u-%u, write pad register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                    (uint64_t)RP1_GPIO_BASE + gpio_pads_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), pad_val);
        }
    }
}

static uint32_t get_gpio_pull(const uint32_t gpio)
{
    uint32_t pull = PULL_NONE;
    uint32_t bank, offset;
    uint32_t pad_val;

    rp1_gpio_get_bank_offset(gpio, &bank, &offset);
    pad_val = in32(gpio_base + gpio_pads_bank_offset[bank] + RP1_GPIO_PADS_REG_OFFSET(offset));
    if (debug > 0) {
        (void)printf("%s gpio:%u bank-offset: %u-%u, read pad register:0x%lx=0x%x\n", __func__, gpio, bank, offset,
                (uint64_t)RP1_GPIO_BASE + gpio_pads_bank_offset[bank] + RP1_GPIO_IO_REG_CTRL_OFFSET(offset), pad_val);
    }

    if (pad_val & RP1_PADS_PUE_SET) {
        pull = PULL_UP;
    }
    else if (pad_val & RP1_PADS_PDE_SET) {
        pull = PULL_DOWN;
    }

    return pull;
}

#define GPIO_NAME_STRLEN  (24U)
static void gpio_get(const uint32_t pinnum)
{
    char funcname[GPIO_NAME_STRLEN] = { 0 };
    int level;
    uint32_t fsel;
    uint32_t pull;
    const char* const gpio_pull_names[] = {"NONE", "DOWN", "UP", "?"};

    level = get_gpio_level(pinnum);
    pull = get_gpio_pull(pinnum);
    fsel = get_gpio_fsel(pinnum);
    if (gpio_fsel_to_namestr(pinnum, fsel, funcname) != -1) {
        if (fsel < NUM_ALT_FUNCS) {
            if (pull <= PULL_UP) {
                (void)printf("GPIO %2u:level=%d pull=%s func=a%u %s\n", pinnum, level, gpio_pull_names[pull], fsel, funcname);
            }
            else {
                (void)printf("GPIO %2u:level=%d pull=? fsel=a%u %s\n", pinnum, level, fsel, funcname);
            }
        } else {
            if (pull <= PULL_UP) {
                (void)printf("GPIO %2u:level=%d pull=%s func=%s\n", pinnum, level, gpio_pull_names[pull], funcname);
            }
            else {
                (void)printf("GPIO %2u:level=%d pull=? func=%s\n", pinnum, level, funcname);
            }
        }
    }
}

static void gpio_set(const uint32_t pinnum, const uint32_t func_sel, const uint32_t drive, const uint32_t pull)
{
    /* set function */
    if (func_sel != FUNC_UNSET) {
        set_gpio_fsel(pinnum, func_sel);
    }

    /* set output value (check pin is output first) */
    if (drive != DRIVE_UNSET) {
        if (get_gpio_fsel(pinnum) == FUNC_OP) {
            set_gpio_drive(pinnum, drive);
        } else {
            (void)printf("Can't set pin value, not an output: %u\n", pinnum);
        }
    }

    /* set pulls */
    if (pull != PULL_UNSET) {
        gpio_set_pull(pinnum, pull);
    }
}

int main(const int argc, char * const argv[])
{
    int ret;

    /* arg parsing */
    bool set = false;
    bool get = false;
    bool funcs = false;
    uint32_t pull = PULL_UNSET;
    uint32_t func_sel = FUNC_UNSET;
    uint32_t drive = DRIVE_UNSET;
    uint64_t pin_mask = 0UL; /* Enough for 0-53 */
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
        const char * pin_opt = argv[2];
        while (pin_opt) {
            uint32_t pin, pin2;
            int len;
            ret = sscanf(pin_opt, "%u%n", &pin, &len);
            if ((ret != 1) || (pin >= GPIO_MAX)) {
                break;
            }
            pin_opt += len;

            if (*pin_opt == '-') {
                pin_opt++;
                ret = sscanf(pin_opt, "%u%n", &pin2, &len);
                if ((ret != 1) || (pin2 >= GPIO_MAX)) {
                    break;
                }
                if (pin2 < pin) {
                    const unsigned tmp = pin2;
                    pin2 = pin;
                    pin = tmp;
                }
                pin_opt += len;
            } else {
                pin2 = pin;
            }
            while (pin <= pin2) {
                pin_mask |= (1UL << pin);
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
    if (pin_mask == 0UL) {
        all_pins = true;
    }

    if (funcs) {
        print_funcs(all_pins, pin_mask);
    /* get or set */
    } else {
        gpio_base = (uintptr_t) mmap_device_memory(NULL, BLOCK_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, RP1_GPIO_BASE);
        if (gpio_base == (uintptr_t) MAP_FAILED) {
            (void)printf("mmap (GPIO) failed: %s\n", strerror (errno));
            return EXIT_FAILURE;
        }

        uint32_t pin;
        for (pin = GPIO_MIN; pin < GPIO_MAX; pin++) {
            if (all_pins == true) {
                if (pin == rp1_bank_base[0]) {
                    (void)printf("BANK0 (GPIO %u to %u):\n", rp1_bank_base[0], rp1_bank_base[1]-1U);
                }
                if (pin == rp1_bank_base[1]) {
                    (void)printf("BANK1 (GPIO %u to %u):\n", rp1_bank_base[1], rp1_bank_base[2]-1U);
                }
                if (pin == rp1_bank_base[2]) {
                    (void)printf("BANK2 (GPIO %u to %u):\n", rp1_bank_base[2], GPIO_MAX);
                }
            } else if ((pin_mask & (1UL << pin)) == 0UL) {
                continue;
            }
            if (get) {
                gpio_get(pin);
            } else {
                gpio_set(pin, func_sel, drive, pull);
            }
        }
    }

    return EXIT_SUCCESS;
}

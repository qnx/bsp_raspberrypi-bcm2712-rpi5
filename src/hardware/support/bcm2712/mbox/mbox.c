/*
 * Copyright 2020, 2024-2025, BlackBerry Limited.
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

#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>


#define MBOX_TAG_GET_FIRMWAREREV        (0x00000001U)
#define MBOX_TAG_GET_FIRMWAREVAR        (0x00000002U)
#define MBOX_TAG_GET_FIRMWAREHASH       (0x00000003U)
#define MBOX_TAG_GET_BOARDMODEL         (0x00010001U)
#define MBOX_TAG_GET_BOARDREVISION      (0x00010002U)
#define MBOX_TAG_GET_MACADDRESS         (0x00010003U)
#define MBOX_TAG_GET_BOARDSERIAL        (0x00010004U)
#define MBOX_TAG_GET_ARMMEMORY          (0x00010005U)
#define MBOX_TAG_GET_VCMEMORY           (0x00010006U)
#define MBOX_TAG_GET_CLOCKS             (0x00010007U)

#define MBOX_TAG_GET_POWERSTATE         (0x00020001U)
#define MBOX_TAG_GET_POWERTIMING        (0x00020002U)

#define MBOX_TAG_GET_CLOCKSTATE         (0x00030001U)
#define MBOX_TAG_GET_CLOCKRATE          (0x00030002U)
#define MBOX_TAG_GET_VOLTAGE            (0x00030003U)
#define MBOX_TAG_GET_MAX_CLOCKRATE      (0x00030004U)
#define MBOX_TAG_GET_MAX_VOLTAGE        (0x00030005U)
#define MBOX_TAG_GET_TEMPERATURE        (0x00030006U)
#define MBOX_TAG_GET_MIN_CLOCKRATE      (0x00030007U)
#define MBOX_TAG_GET_MIN_VOLTAGE        (0x00030008U)
#define MBOX_TAG_GET_TURBO              (0x00030009U)
#define MBOX_TAG_GET_MAX_TEMPERATURE    (0x0003000aU)
#define MBOX_TAG_GET_STC                (0x0003000bU)
#define MBOX_TAG_GET_DOMAIN_STATE       (0x00030030U)
#define MBOX_TAG_GET_GPIO_STATE         (0x00030041U)
#define MBOX_TAG_GET_GPIO_CONFIG        (0x00030043U)
#define MBOX_TAG_PERIPH_REG             (0x00030045U)
#define MBOX_TAG_GET_THROTTLED          (0x00030046U)
#define MBOX_TAG_GET_CLOCK_MEASURED     (0x00030047U)
#define MBOX_TAG_NOTIFY_REBOOT          (0x00030048U)
#define MBOX_TAG_POE_HAT_VAL            (0x00030049U)
#define MBOX_TAG_POE_HAT_VAL_OLD        (0x00030050U)
#define MBOX_TAG_NOTIFY_XHCI_RESET      (0x00030058U)
#define MBOX_TAG_NOTIFY_DISPLAY_DONE    (0x00030066U)
#define MBOX_TAG_BUTTONS_PRESSED        (0x00030088U)

#define MBOX_TAG_GET_CMDLINE            (0x00050001U)
#define MBOX_TAG_GET_DMACHAN            (0x00060001U)

/* clock ID */
#define MBOX_TAG_EMMC_CLK_ID            1
#define MBOX_TAG_UART_CLK_ID            2
#define MBOX_TAG_ARM_CLK_ID             3
#define MBOX_TAG_CORE_CLK_ID            4
#define MBOX_TAG_V3D_CLK_ID             5
#define MBOX_TAG_H264_CLK_ID            6
#define MBOX_TAG_ISP_CLK_ID             7
#define MBOX_TAG_SDRAM_CLK_ID           8
#define MBOX_TAG_PIXEL_CLK_ID           9
#define MBOX_TAG_PWM_CLK_ID             10
#define MBOX_TAG_HEVC_CLK_ID            11
#define MBOX_TAG_EMMC2_CLK_ID           12
#define MBOX_TAG_M2MC_CLK_ID            13
#define MBOX_TAG_PIXEL_BVB_CLK_ID       14
#define MBOX_TAG_VEC_CLK_ID             15
#define MBOX_TAG_DISP_CLK_ID            16
#define MBOX_TAG_NUM_CLK_ID             17

static int mbox_msg(const uint32_t tag, void *buf, const uint32_t bufsize) {
    struct {
        uint32_t hdrsize, hdrcode;
#define MBOX_PROCESS_REQUEST      0
        uint32_t tag, tagsize, tagcode;
#define MBOX_TAG_REQUEST          (0U << 31)
#define MBOX_TAG_RESPONSE         (1U << 31)
#define MBOX_TAG_NULL             0
        uint8_t buf[bufsize];
        uint32_t tagend;
    } *msg;
    static void *out = NULL;

    static uint32_t physaddr;

    typedef struct {
        uint32_t rdwr, _x04,   _x08,   _x0c;
        uint32_t poll, send, status, config;
#define MBOX_STATUS_FULL          (0x80000000U)
#define MBOX_STATUS_EMPTY         (0x40000000U)
#define MBOX_SEND_CHANNEL         (8U)
#define MBOX_SEND_CHANNEL_MASK    (0xfU)
    } mbox_t;
    static volatile mbox_t *mbox = NULL; // [0]:fromvc [1]:tovc

    if (out == NULL) {
        int fd;
        off_t offset;
        fd = posix_typed_mem_open("/sysram&below1G", O_RDWR, POSIX_TYPED_MEM_ALLOCATE_CONTIG);
        if (fd < 0) {
            (void)printf("Failed to open typed memory(%s) \n", strerror( errno ));
            exit(EXIT_FAILURE);
        }
        out = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE | PROT_NOCACHE, MAP_SHARED, fd, 0);
        if (out == MAP_FAILED) {
            (void)printf("Failed to allocate buffer!\n");
            (void)close(fd);
            exit(EXIT_FAILURE);
        }

        if (mem_offset(out, NOFD, 1, &offset, NULL) != EOK) {
            (void)munmap(out, 0x1000);
            (void)close(fd);
            (void)printf("Failed to obtain physical address\n");
            exit(EXIT_FAILURE);
        }

        physaddr = (uint32_t)offset;
        (void)close(fd);
    }

    msg = out;
    msg->hdrsize = (uint32_t)sizeof(*msg);
    msg->hdrcode = MBOX_PROCESS_REQUEST;
    msg->tag = tag;
    msg->tagsize = bufsize;
    msg->tagcode = MBOX_TAG_REQUEST;
    (void)memcpy((void*)msg->buf, buf, bufsize);
    msg->tagend = MBOX_TAG_NULL;

#define MBOX_REGS           (0x107c013880UL)
    int wait_cnt = 0;
    if (mbox == NULL) {
        mbox = mmap_device_memory(NULL, (sizeof(mbox_t)*2U), PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, MBOX_REGS);
        if (mbox == MAP_FAILED) {
            (void)printf("Failed to map mbox registers\n");
            exit (EXIT_FAILURE);
        }
    }
    if (mbox != NULL) {
        // check for space on tovc channel to send with timeout
        for (;;) {
            if ((mbox[1].status & MBOX_STATUS_FULL) == 0U) {
                break;
            }
            (void)usleep(100);
            if (++wait_cnt > 5000) {
                (void)printf("Check for tovc channel full timeout\n");
                return -1;
            }
        }

        // send
        mbox[1].rdwr = (physaddr & ~MBOX_SEND_CHANNEL_MASK) | MBOX_SEND_CHANNEL;

        // wait for tx done
        for (;;) {
            if ((mbox[1].status & MBOX_STATUS_FULL) == 0U) {
                break;
            }
            (void)usleep(100);
            if (++wait_cnt > 5000) {
                (void)printf("Wait for tx done timeout\n");
                return -1;
            }
        }

        for (;;) {
            wait_cnt = 0;
            // wait for response on fromvc channel to receive with timeout
            for (;;) {
                if ((mbox[0].status & MBOX_STATUS_EMPTY) == 0U) {
                    break;
                }
                (void)usleep(100);
                if (++wait_cnt > 5000) {
                    (void)printf("Wait for response timeout\n");
                    return -2;
                }
            }

            // is it the channel we're waiting for?
            const uint32_t rd = mbox[0].rdwr;
            if ((rd & MBOX_SEND_CHANNEL_MASK) == MBOX_SEND_CHANNEL) {
                const uint32_t tagcode = msg->tagcode;
                uint32_t len = tagcode & ~MBOX_TAG_RESPONSE;

                if (tag != MBOX_TAG_GET_FIRMWAREHASH) {
                    if (len > 0U) {
                        if (len > bufsize) {
                            // passed in bufsize is not big enough to hold return response
                            len = bufsize;
                        }
                        (void)memcpy(buf, (void*)msg->buf, len);
                    }
                    return (int)len;
                }
                else {
                    /* firmwware hash should return 20 bytes but return code indicates 8 only */
                    (void)memcpy(buf, (void*)msg->buf, bufsize);
                    return bufsize;
                }
            }
        }
        /*NOTREACHED*/
    }
    return 0;
}

static void fmt_d(const void *const buf, const uint32_t len) {
    for (unsigned i = 0; i < (len / sizeof(int32_t)); i++) {
        (void)printf("%d ", ((int32_t *) buf)[i]);
    }
    (void)printf("\n");
}

static void fmt_s(const void *const buf, const uint32_t len) {
    (void)printf("%.*s\n", len, (char *) buf);
}

static void fmt_u(const void *const buf, const uint32_t len) {
    for (unsigned i = 0; i < (len / sizeof(uint32_t)); i++) {
        (void)printf("%u ", ((uint32_t *) buf)[i]);
    }
    (void)printf("\n");
}

static void fmt_x(const void *const buf, const uint32_t len) {
    /* mac address returns 6 byte, need to round up to get last two octets */
    const uint32_t length = len + (sizeof(uint32_t) - 1U);
    for (unsigned i = 0; i < (length / sizeof(uint32_t)); i++) {
        (void)printf("%08x ", ((uint32_t *) buf)[i]);
    }
    (void)printf("\n");
}

static void fmt_x_str(const void *const buf, const uint32_t len) {
    for (unsigned i = 0; i < (len / sizeof(uint32_t)); i++) {
        (void)printf("%08x", ((uint32_t *) buf)[i]);
    }
    (void)printf("\n");
}

int main(const int argc, char **argv) {
	typedef struct {
        const char *str;
        void (*fmt)(const void * const buf, const uint32_t len);
        uint32_t tag;
        uint32_t len;
    } cmd_options_t;

	static const cmd_options_t cmd_options[] = {
        { .str="clockrate",         .fmt=&fmt_u,     .tag=MBOX_TAG_GET_CLOCKRATE,       .len=2 },
        { .str="clockratemeasured", .fmt=&fmt_u,     .tag=MBOX_TAG_GET_CLOCK_MEASURED,  .len=2 },
        { .str="clocks",            .fmt=&fmt_d,     .tag=MBOX_TAG_GET_CLOCKS,          .len=80 },
        { .str="clockstate",        .fmt=&fmt_d,     .tag=MBOX_TAG_GET_CLOCKSTATE,      .len=2 },
        { .str="cmdline",           .fmt=&fmt_s,     .tag=MBOX_TAG_GET_CMDLINE,         .len=256 },
        { .str="dmachan",           .fmt=&fmt_x,     .tag=MBOX_TAG_GET_DMACHAN,         .len=2 },
        { .str="firmwarerev",       .fmt=&fmt_x,     .tag=MBOX_TAG_GET_FIRMWAREREV,     .len=2 },
        { .str="firmwarevar",       .fmt=&fmt_x,     .tag=MBOX_TAG_GET_FIRMWAREVAR,     .len=1 },
        { .str="firmwarehash",      .fmt=&fmt_x_str, .tag=MBOX_TAG_GET_FIRMWAREHASH,    .len=5 },
        { .str="gpioconfig",        .fmt=&fmt_x,     .tag=MBOX_TAG_GET_GPIO_CONFIG,     .len=2 },
        { .str="gpiostate",         .fmt=&fmt_d,     .tag=MBOX_TAG_GET_GPIO_STATE,      .len=2 },
        { .str="macaddress",        .fmt=&fmt_x,     .tag=MBOX_TAG_GET_MACADDRESS,      .len=2 },
        { .str="maxclockrate",      .fmt=&fmt_u,     .tag=MBOX_TAG_GET_MAX_CLOCKRATE,   .len=2 },
        { .str="maxtemperature",    .fmt=&fmt_d,     .tag=MBOX_TAG_GET_MAX_TEMPERATURE, .len=2 },
        { .str="maxvoltage",        .fmt=&fmt_d,     .tag=MBOX_TAG_GET_MAX_VOLTAGE,     .len=2 },
        { .str="memory",            .fmt=&fmt_x,     .tag=MBOX_TAG_GET_ARMMEMORY,       .len=2 },
        { .str="minclockrate",      .fmt=&fmt_u,     .tag=MBOX_TAG_GET_MIN_CLOCKRATE,   .len=2 },
        { .str="minvoltage",        .fmt=&fmt_d,     .tag=MBOX_TAG_GET_MIN_VOLTAGE,     .len=2 },
        { .str="model",             .fmt=&fmt_d,     .tag=MBOX_TAG_GET_BOARDMODEL,      .len=2 },
        { .str="notifyxhcireset",   .fmt=&fmt_x,     .tag=MBOX_TAG_NOTIFY_XHCI_RESET,   .len=2 },
        { .str="powerstate",        .fmt=&fmt_d,     .tag=MBOX_TAG_GET_POWERSTATE,      .len=2 },
        { .str="powertiming",       .fmt=&fmt_d,     .tag=MBOX_TAG_GET_POWERTIMING,     .len=2 },
        { .str="revision",          .fmt=&fmt_x,     .tag=MBOX_TAG_GET_BOARDREVISION,   .len=2 },
        { .str="serial",            .fmt=&fmt_x,     .tag=MBOX_TAG_GET_BOARDSERIAL,     .len=2 },
        { .str="temperature",       .fmt=&fmt_d,     .tag=MBOX_TAG_GET_TEMPERATURE,     .len=2 },
        { .str="throttled",         .fmt=&fmt_x,     .tag=MBOX_TAG_GET_THROTTLED,       .len=1 },
        { .str="turbo",             .fmt=&fmt_x,     .tag=MBOX_TAG_GET_TURBO,           .len=2 },
        { .str="vcmemory",          .fmt=&fmt_x,     .tag=MBOX_TAG_GET_VCMEMORY,        .len=2 },
        { .str="voltage",           .fmt=&fmt_d,     .tag=MBOX_TAG_GET_VOLTAGE,         .len=2 },
    };

    while (*++argv != NULL) {
        uint32_t set = 0U;
        char *opt;
        const char *const str = *argv;
        uint32_t i, num_arg = 0, opt_arg[256+1] = { };
        uint32_t len = (uint32_t) strcspn(str, ":=");
        uint64_t val = 0ULL;

        switch (str[len]) {
            case '=':
                set = 1U;
                /*FALLTHRU*/
            case ':':
                opt = (char *)str + len;
                *opt++ = '\0';
                while (num_arg < (sizeof(opt_arg) / sizeof(opt_arg[0]))) {
                    errno = EOK;
                    val = strtoul(str, &opt, 0);
                    if (errno == EOK) {
                        opt_arg[num_arg++] = (uint32_t)val;
                    }
                    else {
                        // invalid arg, default to 0;
                        opt_arg[num_arg++] = 0U;
                    }
                    if (*opt++ != ':') {
                        break;
                    }
                }
                break;
        }
        for (i = 0; i < (sizeof(cmd_options) / sizeof(cmd_options[0])); i++) {
            if (strcmp(str, cmd_options[i].str) == 0) {
                int ret;
                uint32_t tag = cmd_options[i].tag;
                uint32_t len = cmd_options[i].len;
                if (num_arg > len) {
                    len = num_arg;
                }
                if ((set == 1U) && (tag != MBOX_TAG_NOTIFY_XHCI_RESET)) {
                    tag |= 0x8000U;
                }
                ret = mbox_msg(tag, (void *)opt_arg, (uint32_t)(len * sizeof(opt_arg[0])));
                if (ret > 0) {
                    len = (uint32_t) ret;
                    cmd_options[i].fmt(opt_arg, len);
                }
                else {
                    (void)printf("Mbox does not return valid length\n");
                }
                break;
            } else {
                int ret;
                errno = EOK;
                val = strtoul(str, &opt, 0);
                if (errno == EOK) {
                    uint32_t tag = (uint32_t) val;
                    uint32_t len = 2U;
                    if (tag != 0U) {
                        if (num_arg > len) {
                            len = num_arg;
                        }
                        if (set == 1U) {
                            tag |= 0x8000U;
                        }
                        ret = mbox_msg(tag, (void *)opt_arg,  (uint32_t)(len * sizeof(opt_arg[0])));
                        if (ret > 0) {
                            len = (uint32_t) ret;
                            fmt_x(opt_arg, len);
                        }
                        else {
                            (void)printf("Mailbox does not return valid length\n");
                        }
                        break;
                    }
                }
            }
        }
        if (i >= (sizeof(cmd_options) / sizeof(cmd_options[0]))) {
            (void)printf("%s not found?\n", str);
            return (EXIT_FAILURE);
        }
    }
    return EXIT_SUCCESS;
}

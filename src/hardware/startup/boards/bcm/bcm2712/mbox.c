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
 * Copyright (c) 2020, 2022, BlackBerry Limited.
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
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */

#include "mbox.h"
#include <startup.h>

/* mailbox 0 (from VC) and mailbox 1 (to VC) */
#define	MBOX_SIZE                   0x80

#define	MBOX_READ                   0x00
#define	MBOX_WRITE                  0x00
#define	MBOX_POLL                   0x10	/* read without popping the fifo */
#define	MBOX_ID                     0x14	/* sender ID (bottom two bits) */
#define	MBOX_STATUS                 0x18    /* status */
#define	MBOX_STATUS_FULL            0x80000000
#define	MBOX_STATUS_EMPTY           0x40000000
#define	MBOX_STATUS_LEVEL           0x400000FF
#define	MBOX_CFG                    0x1C	/* configuration */
#define	MBOX_CFG_DATAIRQEN          0x00000001
#define	MBOX_CFG_SPACEIRQEN         0x00000002
#define	MBOX_CFG_EMPTYOPIRQEN       0x00000004
#define	MBOX_CFG_MAILCLEAR          0x00000008
#define	MBOX_CFG_DATAPENDING        0x00000010
#define	MBOX_CFG_SPACEPENDING       0x00000020
#define	MBOX_CFG_EMPTYOPPENDING     0x00000040
#define	MBOX_CFG_ENOOWN             0x00000100
#define	MBOX_CFG_EOVERFLOW          0x00000200
#define	MBOX_CFG_EUNDERFLOW         0x00000400

#define	MBOX_PROCESS_REQUEST        0
#define MBOX_REQ_SUCCESS            (1U << 31)

/*
 * Mailbox calls happen before the startup timer callbacks are initialized.
 * A finite polling budget still prevents faulty firmware from hanging startup
 * forever; the value is deliberately generous for early-boot firmware.
 */
#define MBOX_POLL_LIMIT             (10000000U)

#define	MBOX_TAG_REQUEST            (0U << 31)
#define	MBOX_TAG_RESPONSE           (1U << 31)

#define	MBOX_TAG_NULL               0x00000000
#define	MBOX_TAG_GET_FIRMWAREREV    0x00000001
#define	MBOX_TAG_GET_BOARDMODEL     0x00010001
#define	MBOX_TAG_GET_BOARDREVISION  0x00010002
#define	MBOX_TAG_GET_MACADDRESS     0x00010003
#define	MBOX_TAG_GET_BOARDSERIAL    0x00010004
#define	MBOX_TAG_GET_ARMMEMORY      0x00010005
#define	MBOX_TAG_GET_VCMEMORY       0x00010006
#define	MBOX_TAG_GET_CLOCKS         0x00010007

#define MBOX_TAG_GET_POWERSTATE     0x00020001
#define MBOX_TAG_GET_POWERTIMING    0x00020002
#define MBOX_TAG_SET_POWERSTATE     0x00028001

#define	MBOX_TAG_GET_CLOCKSTATE     0x00030001
#define	MBOX_TAG_SET_CLOCKSTATE     0x00038001
#define	MBOX_TAG_GET_CLOCKRATE      0x00030002
#define	MBOX_TAG_SET_CLOCKRATE      0x00038002
#define	MBOX_TAG_GET_MIN_CLOCKRATE  0x00030007
#define	MBOX_TAG_GET_MAX_CLOCKRATE  0x00030004

#define MBOX_TAG_GET_VOLTAGE        0x00030003
#define MBOX_TAG_SET_VOLTAGE        0x00038003
#define MBOX_TAG_GET_MIN_VOLTAGE    0x00030008
#define MBOX_TAG_GET_MAX_VOLTAGE    0x00030005

#define MBOX_TAG_GET_TEMPERATURE    0x00030006
#define MBOX_TAG_GET_MAX_TEMPERATURE 0x0003000a

#define MBOX_TAG_GET_GPIO_STATE     0x00030041  // expansion gpio
#define MBOX_TAG_SET_GPIO_STATE     0x00038041  // expansion gpio
#define MBOX_TAG_GET_GPIO_CONFIG    0x00030043  // expansion gpio
#define MBOX_TAG_SET_GPIO_CONFIG    0x00038043  // expansion gpio

#define	MBOX_TAG_GET_CMDLINE        0x00050001
#define	MBOX_TAG_GET_DMACHAN        0x00060001

#define	MBOX_TAG_ALLOCATE_BUFFER    0x00040001
#define	MBOX_TAG_BLANK_SCREEN       0x00040002
#define	MBOX_TAG_GET_FB_RES         0x00040003
#define	MBOX_TAG_SET_FB_RES         0x00048003
#define	MBOX_TAG_GET_FB_VRES        0x00040004
#define	MBOX_TAG_SET_FB_VRES        0x00048004
#define	MBOX_TAG_GET_FB_DEPTH       0x00040005
#define	MBOX_TAG_SET_FB_DEPTH       0x00048005
#define	MBOX_TAG_GET_FB_PIXEL_ORDER 0x00040006
#define	MBOX_TAG_SET_FB_PIXEL_ORDER 0x00048006
#define	MBOX_TAG_GET_FB_ALPHA_MODE  0x00040007
#define	MBOX_TAG_SET_FB_ALPHA_MODE  0x00048007
#define	MBOX_TAG_GET_FB_PITCH       0x00040008

#define	MBOX_TAG_GET_EDID_BLOCK     0x00030020

#define	MBOX0_BASE                  0x00
#define	MBOX1_BASE                  0x20

#define	MBOX0_READ                  (MBOX0_BASE + MBOX_READ)
#define	MBOX0_WRITE                 (MBOX0_BASE + MBOX_WRITE)
#define	MBOX0_POLL                  (MBOX0_BASE + MBOX_POLL)
#define	MBOX0_ID                    (MBOX0_BASE + MBOX_ID)
#define	MBOX0_STATUS                (MBOX0_BASE + MBOX_STATUS)
#define	MBOX0_CFG                   (MBOX0_BASE + MBOX_READ)

#define	MBOX1_READ                  (MBOX1_BASE + MBOX_READ)
#define	MBOX1_WRITE                 (MBOX1_BASE + MBOX_WRITE)
#define	MBOX1_POLL                  (MBOX1_BASE + MBOX_POLL)
#define	MBOX1_ID                    (MBOX1_BASE + MBOX_ID)
#define	MBOX1_STATUS                (MBOX1_BASE + MBOX_STATUS)
#define	MBOX1_CFG                   (MBOX1_BASE + MBOX_READ)

struct mbox_msg_header {
    uint32_t size;
    uint32_t code;
};

struct mbox_msg_tag {
    uint32_t tag;
    uint32_t size;
    uint32_t code;
};

/*
 * Keep the complete property message in linker-owned startup storage.  The old
 * fixed address overlapped an IFS loaded at 0x80000 when a 512-byte command-line
 * response was used.  The firmware protocol requires 16-byte alignment.
 */
static uint8_t mbox_buffer[MBOX_BUFFER_SIZE] __attribute__((aligned(16)));

#define MBOX_BUFFER_ADDR            ((uintptr_t)&mbox_buffer[0])

#define MSG_INIT(msg, tagid) do { \
    if (sizeof(*(msg)) > MBOX_BUFFER_SIZE) { \
        crash("Mailbox message exceeds reserved buffer\n"); \
    } \
    memset((void *)(msg), 0, sizeof(*(msg))); \
    (msg)->header.size = sizeof(*(msg)); \
    (msg)->header.code = MBOX_PROCESS_REQUEST; \
    (msg)->tag.tag = (tagid); \
    (msg)->tag.size = sizeof((msg)->body); \
    (msg)->tag.code = MBOX_TAG_REQUEST; \
    (msg)->end = MBOX_TAG_NULL; \
} while (0)

static int mbox_send_message(const uint32_t expected_tag,
                             const uint32_t minimum_response_size) {
    volatile struct mbox_msg_header *const header =
        (volatile struct mbox_msg_header *)MBOX_BUFFER_ADDR;
    volatile struct mbox_msg_tag *const tag =
        (volatile struct mbox_msg_tag *)(MBOX_BUFFER_ADDR + sizeof(*header));
    const uintptr_t buffer_addr = MBOX_BUFFER_ADDR;
    const uint32_t request_size = header->size;
    const uint32_t request_value_size = tag->size;
    uint32_t request_addr;
    uint32_t poll;
    uint32_t rdata;

    if (((buffer_addr & 0xfU) != 0U) || (buffer_addr > UINT32_MAX) ||
        (request_size < (sizeof(*header) + sizeof(*tag) + sizeof(uint32_t))) ||
        (request_size > MBOX_BUFFER_SIZE) ||
        (minimum_response_size > request_value_size)) {
        return 0;
    }
    request_addr = (uint32_t)buffer_addr & ~0xfU;

    for (poll = 0U; poll < MBOX_POLL_LIMIT; ++poll) {
        rdata = *((volatile uint32_t *)(MBOX_REG_BASE + MBOX0_STATUS));
        if ((rdata & MBOX_STATUS_FULL) == 0U) {
            break;
        }
    }
    if (poll == MBOX_POLL_LIMIT) {
        return 0;
    }

    /* Publish the request contents before notifying the VideoCore. */
    __asm__ __volatile__("dmb sy" ::: "memory");
    *((volatile uint32_t *)(MBOX_REG_BASE + MBOX1_WRITE)) =
        request_addr | MBOX_SEND_CHANNEL;

    for (poll = 0U; poll < MBOX_POLL_LIMIT; ++poll) {
        rdata = *((volatile uint32_t *)(MBOX_REG_BASE + MBOX0_STATUS));
        if ((rdata & MBOX_STATUS_EMPTY) != 0U) {
            continue;
        }

        rdata = *((volatile uint32_t *)(MBOX_REG_BASE + MBOX0_READ));
        if (((rdata & 0xfU) == MBOX_SEND_CHANNEL) &&
            ((rdata & ~0xfU) == request_addr)) {
            break;
        }
    }
    if (poll == MBOX_POLL_LIMIT) {
        return 0;
    }

    /* Do not consume stale, truncated, or firmware-rejected responses. */
    __asm__ __volatile__("dmb sy" ::: "memory");
    if ((header->size != request_size) ||
        (header->code != MBOX_REQ_SUCCESS) ||
        (tag->tag != expected_tag) ||
        (tag->size != request_value_size) ||
        ((tag->code & MBOX_TAG_RESPONSE) == 0U) ||
        ((tag->code & ~MBOX_TAG_RESPONSE) < minimum_response_size) ||
        ((tag->code & ~MBOX_TAG_RESPONSE) > tag->size) ||
        (tag->size > (request_size - sizeof(*header) - sizeof(*tag) - sizeof(uint32_t))) ||
        (*((volatile uint32_t *)(MBOX_BUFFER_ADDR + request_size - sizeof(uint32_t))) !=
         MBOX_TAG_NULL)) {
        return 0;
    }

    return 1;
}

uint32_t mbox_get_board_revision(void) {
    volatile struct mbox_msg_get_board_revision {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t revision;
        } body;
        uint32_t end;
    } *msg = (volatile struct mbox_msg_get_board_revision *) MBOX_BUFFER_ADDR;
    msg->body.revision = 0;
    MSG_INIT(msg, MBOX_TAG_GET_BOARDREVISION);
    if (!mbox_send_message(MBOX_TAG_GET_BOARDREVISION, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.revision;
}

void mbox_get_board_mac_address(uint8_t* const address) {
    volatile struct mbox_msg_get_board_mac_address {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint8_t address[6];
            uint8_t _pad[2];
        } body;
        uint32_t end;
    } *msg = (volatile struct mbox_msg_get_board_mac_address *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_MACADDRESS);
    if (!mbox_send_message(MBOX_TAG_GET_MACADDRESS, sizeof(msg->body.address))) {
        memset(address, 0, sizeof(msg->body.address));
        return;
    }

    memcpy(address, (uint8_t*) msg->body.address, 6);
}

uint64_t mbox_get_board_serial(void) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t serial[2];
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_BOARDSERIAL);
    if (!mbox_send_message(MBOX_TAG_GET_BOARDSERIAL, sizeof(msg->body))) {
        return 0U;
    }
    return ((uint64_t)msg->body.serial[1] << 32) | (uint64_t)(msg->body.serial[0]);
}

uint32_t mbox_get_arm_memory(void) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t base;
            uint32_t size;
        } body[1];
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_ARMMEMORY);
    if (!mbox_send_message(MBOX_TAG_GET_ARMMEMORY, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body[0].size;
}

uint32_t mbox_get_vc_memory(void) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t base;
            uint32_t size;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_VCMEMORY);
    if (!mbox_send_message(MBOX_TAG_GET_VCMEMORY, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.size;
}

char *mbox_get_cmdline(void) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            char line[512 - 24];
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_CMDLINE);
    if (!mbox_send_message(MBOX_TAG_GET_CMDLINE, 0U)) {
        return NULL;
    }

    unsigned len = msg->tag.code & ~MBOX_TAG_RESPONSE;
    if (len >= sizeof msg->body.line) {
        return 0;
    }
    msg->body.line[len] = '\0';
    return (char *) msg->body.line;
}

uint32_t mbox_get_power_state(const uint32_t id) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t device_id;
            uint32_t state;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_POWERSTATE);
    msg->body.device_id = id;
    if (!mbox_send_message(MBOX_TAG_GET_POWERSTATE, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.state;
}

uint32_t mbox_set_power_state(const uint32_t id, const uint32_t state) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t device_id;
            uint32_t state;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_SET_POWERSTATE);
    msg->body.device_id = id;
    msg->body.state = state;
    if (!mbox_send_message(MBOX_TAG_SET_POWERSTATE, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.state;
}

uint32_t mbox_get_clock_rate(const uint32_t id) {
    volatile struct mbox_msg_get_clock_rate {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t clock_id;
            uint32_t rate;
        } body;
        uint32_t end;
    } *msg = (volatile struct mbox_msg_get_clock_rate *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_CLOCKRATE);
    msg->body.clock_id = id;
    if (!mbox_send_message(MBOX_TAG_GET_CLOCKRATE, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.rate;
}

uint32_t mbox_set_clock_rate(const uint32_t id, const uint32_t rate) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t clock_id;
            uint32_t rate;
            uint32_t no_turbo;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_SET_CLOCKRATE);
    msg->body.clock_id = id;
    msg->body.rate = rate;
    msg->body.no_turbo = 0;
    if (!mbox_send_message(MBOX_TAG_SET_CLOCKRATE, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.rate;
}

uint32_t mbox_get_voltage(const uint32_t id) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t voltage_id;
            int32_t offset;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_VOLTAGE);
    msg->body.voltage_id = id;
    if (!mbox_send_message(MBOX_TAG_GET_VOLTAGE, sizeof(msg->body))) {
        return 0U;
    }
    if (msg->body.offset < 0) return 0;
    return msg->body.offset;
}

int32_t mbox_get_temperature(const uint32_t id) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t temperature_id;
            int32_t millidegrees;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_TEMPERATURE);
    msg->body.temperature_id = id;
    if (!mbox_send_message(MBOX_TAG_GET_TEMPERATURE, sizeof(msg->body))) {
        return 0;
    }
    return msg->body.millidegrees;
}

uint32_t mbox_get_expgpio(const uint32_t gpio) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t gpio;
            uint32_t state;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_GET_GPIO_STATE);
    msg->body.gpio = gpio;
    msg->body.state = 0;
    if (!mbox_send_message(MBOX_TAG_GET_GPIO_STATE, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.state;
}

uint32_t mbox_set_expgpio(const uint32_t gpio, const uint32_t state) {
    volatile struct {
        struct mbox_msg_header header;
        struct mbox_msg_tag tag;
        struct {
            uint32_t gpio;
            uint32_t state;
        } body;
        uint32_t end;
    } *msg = (volatile void *) MBOX_BUFFER_ADDR;

    MSG_INIT(msg, MBOX_TAG_SET_GPIO_STATE);
    msg->body.gpio = gpio;
    msg->body.state = state;
    if (!mbox_send_message(MBOX_TAG_SET_GPIO_STATE, sizeof(msg->body))) {
        return 0U;
    }
    return msg->body.state;
}

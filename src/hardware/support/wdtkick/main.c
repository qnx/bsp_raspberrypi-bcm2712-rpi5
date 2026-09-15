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
 * $QNXLicenseC:
 * Copyright 2009, 2023, 2025, BlackBerry Limited.
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
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resmgr.h>
#include <sys/neutrino.h>
#include <hw/inout.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>
#include <errno.h>
#include <sys/procmgr.h>
#include <drvr/hwinfo.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_NUM_REG        5
#define DEFAULT_REG_LEN    0x1000

void (*write_wdt)(const uintptr_t, const uint32_t);
void (*write_enable_wdt)(const uintptr_t, const uint32_t);
uint32_t (*read_wdt)(const uintptr_t);
uint32_t (*read_write_wdt)(const uintptr_t);

static void write_wdt_32(const uintptr_t addr, const uint32_t data) {
#if defined(__X86_64__) || defined(__X86__)
    out32((uint16_t)addr, data);
#else
    out32(addr, data);
#endif
}

static void write_wdt_16(const uintptr_t addr, const uint32_t data) {
#if defined(__X86_64__) || defined(__X86__)
    out16((uint16_t)addr, (uint16_t)data);
#else
    out16(addr, (uint16_t)data);
#endif
}

static void write_wdt_8(const uintptr_t addr, const uint32_t data) {
#if defined(__X86_64__) || defined(__X86__)
    out8((uint16_t)addr, (uint8_t)data);
#else
    out8(addr, (uint8_t)data);
#endif
}

static uint32_t read_wdt_32(const uintptr_t addr) {
#if defined(__X86_64__) || defined(__X86__)
    return in32((uint16_t)addr);
#else
    return in32(addr);
#endif
}

static uint32_t read_wdt_16(const uintptr_t addr) {
#if defined(__X86_64__) || defined(__X86__)
    return (uint32_t)in16((uint16_t)addr);
#else
    return (uint32_t)in16(addr);
#endif
}

static uint32_t read_wdt_8(const uintptr_t addr) {
#if defined(__X86_64__) || defined(__X86__)
    return (uint32_t)in8((uint16_t)addr);
#else
    return (uint32_t)in8(addr);
#endif
}

typedef enum {
    BITSWAP,
    WRITE,
    NUM_OPERATIONS
} operation_id_t;

typedef enum {
    REG_OFFSET,
    REG_VAL
} reg_pairs_t;

typedef struct {
    struct sched_param threadparam;
    hwiattr_timer_t wdog_attr;
    paddr_t physbase;
    hwi_tag *tag;
    char *name;
    uintptr_t base;
    long kick_time;
    size_t len;
    int opt;
    int priority;
    int sched_type;
    int operation;
    int verbose;
    int attr;
    unsigned tag_idx;
    unsigned int idx;
    unsigned hwi_off;
    uint32_t write_width;
    uint32_t enable_width;
    uint32_t enable_mask;
    uint32_t enable_condition;
    uint32_t enable_reg_offset;
    uint32_t write_reg[MAX_NUM_REG][2];
    uint8_t write_operation[MAX_NUM_REG];
    uint32_t bitswap_value;
    uint32_t num_writes;
    bool increment_index;
    bool enable_condition_set;
} wdt_init_t;

static bool is_valid_range(const uint32_t val, const int err)
{
    if ((val != 0) && (err == ERANGE)) {
        return false;
    }
    return true;
}

static bool is_valid_separator(const char separator)
{
    if (separator == ':') {
        return true;
    }
    return false;
}

static bool is_hex_digit_char(const char value)
{
    return ((value >= '0') && (value <= '9')) ||
        ((value >= 'a') && (value <= 'f')) ||
        ((value >= 'A') && (value <= 'F'));
}

static bool parse_uint32_option(const char *const text, uint32_t *const result)
{
    char *end = NULL;
    unsigned long value;

    if ((*text < '0') || (*text > '9')) {
        return false;
    }
    errno = EOK;
    value = strtoul(text, &end, 0);
    if ((errno == ERANGE) || (end == text) || (*end != '\0') ||
            (value > UINT32_MAX)) {
        return false;
    }
    *result = (uint32_t)value;
    return true;
}

static void parse_hwinfo_init(wdt_init_t *wdi)
{
    /* Getting the WDOG Base addresss from the Hwinfo Section if available */
    wdi->hwi_off = hwi_find_device("wdog", 0);
    if (wdi->hwi_off != HWI_NULL_OFF) {
        wdi->attr = hwiattr_get_timer(wdi->hwi_off, &(wdi->wdog_attr));
        if (wdi->attr == EOK) {
            if (wdi->wdog_attr.common.location.len > wdi->len) {
                wdi->len = wdi->wdog_attr.common.location.len;
            }
            if (wdi->wdog_attr.common.location.base > 0) {
                wdi->physbase = wdi->wdog_attr.common.location.base;
            }
            wdi->kick_time = hwitag_find_clkfreq(wdi->hwi_off, NULL);
        }
    }
}

static void parse_hwinfo_opt(wdt_init_t *wdi)
{
    /* Getting the WDOG options from the Hwinfo Section if available */
    wdi->hwi_off = hwi_find_device("wdt,options", 0);
    if (wdi->hwi_off != HWI_NULL_OFF) {
        wdi->tag_idx = 0;
        while (((wdi->tag = hwi_tag_find(wdi->hwi_off, HWI_TAG_NAME_regname, &(wdi->tag_idx))) != NULL)) {
            if (strcmp("write_width",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
                wdi->write_width = wdi->tag->regname.offset;
            } else if (strcmp("enable_width",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
                wdi->enable_width = wdi->tag->regname.offset;
            } else if (strcmp("enable_mask",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
                wdi->enable_mask = wdi->tag->regname.offset;
            } else if (strcmp("enable_condition",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
                wdi->enable_condition = wdi->tag->regname.offset;
                wdi->enable_condition_set = true;
            } else if (strcmp("enable_offset",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
                wdi->enable_reg_offset = wdi->tag->regname.offset;
                if (wdi->enable_reg_offset > wdi->len) {
                    wdi->len = wdi->enable_reg_offset;
                }
            } else {
                slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : %s is not a valid Hwinfo default setting.", \
                    __hwi_find_string(wdi->tag->regname.regname));
                break;
            }
        }
    }
}

static void parse_hwinfo_bitswap(wdt_init_t *wdi)
{
    wdi->hwi_off = hwi_find_device("wdt,bitswap", 0);
    if (wdi->hwi_off != HWI_NULL_OFF) {
        wdi->operation = BITSWAP;
    } else {
        wdi->hwi_off = hwi_find_device("wdt,regwrite", 0);
        if (wdi->hwi_off != HWI_NULL_OFF) {
            wdi->operation = WRITE;
        }
    }
}

static int parse_hwinfo_value(wdt_init_t *wdi, const char *const reg_name_string)
{
    if (strcmp("value", reg_name_string) == 0) {
        wdi->write_reg[wdi->num_writes][REG_VAL] = wdi->tag->regname.offset;
    } else if (strcmp("value32_lo", reg_name_string) == 0) {
        wdi->write_reg[wdi->num_writes][REG_VAL] = wdi->tag->regname.offset;
        wdi->increment_index = false;
    } else if (strcmp("value32_hi", reg_name_string) == 0) {
        wdi->write_reg[wdi->num_writes][REG_VAL] |= ((uint32_t)wdi->tag->regname.offset << 16);
    } else {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid value entry. Please check the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    wdi->tag = hwi_tag_find(wdi->hwi_off, HWI_TAG_NAME_regname, &(wdi->tag_idx));
    if (wdi->tag != NULL) {
        if (strcmp("offset",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
            wdi->write_reg[wdi->num_writes][REG_OFFSET] = wdi->tag->regname.offset;
            if (wdi->write_reg[wdi->num_writes][REG_OFFSET] > wdi->len) {
                wdi->len = wdi->write_reg[wdi->num_writes][REG_OFFSET];
            }
        } else {
            slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Missing offset for corresponding value. "
                "Please check the command line or Hwinfo default setting.");
            return EXIT_FAILURE;
        }
    } else {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Missing offset for corresponding value. "
            "Please check the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int wd_parse_hwinfo(wdt_init_t *wdi)
{
    const char *reg_name_string;

    parse_hwinfo_init(wdi);
    parse_hwinfo_opt(wdi);
    parse_hwinfo_bitswap(wdi);

    wdi->tag_idx = 0;
    while ((wdi->tag = hwi_tag_find(wdi->hwi_off, HWI_TAG_NAME_regname, &(wdi->tag_idx))) != NULL) {
        if (wdi->num_writes >= MAX_NUM_REG) {
            slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : %s only supports up to %d kick registers. Please check "
                "the command line or Hwinfo default setting.", wdi->name, MAX_NUM_REG);
            return EXIT_FAILURE;
        }

        if (strcmp("offset", __hwi_find_string(wdi->tag->regname.regname)) == 0) {
            bool new_offset = true;
            while (new_offset) {
                new_offset = false;
                wdi->write_reg[wdi->num_writes][REG_OFFSET] = wdi->tag->regname.offset;
                if (wdi->write_reg[wdi->num_writes][REG_OFFSET] > wdi->len) {
                    wdi->len = wdi->write_reg[wdi->num_writes][REG_OFFSET];
                }
                wdi->tag = hwi_tag_find(wdi->hwi_off, HWI_TAG_NAME_regname, &(wdi->tag_idx));
                if (wdi->tag != NULL) {
                    reg_name_string = __hwi_find_string(wdi->tag->regname.regname);
                    if(strcmp("value", reg_name_string) == 0) {
                        wdi->write_reg[wdi->num_writes][REG_VAL] = wdi->tag->regname.offset;
                    } else if (strcmp("value32_lo", reg_name_string) == 0) {
                        wdi->write_reg[wdi->num_writes][REG_VAL] = wdi->tag->regname.offset;
                        wdi->increment_index = false;
                    } else if (strcmp("value32_hi", reg_name_string) == 0) {
                        wdi->write_reg[wdi->num_writes][REG_VAL] |= ((uint32_t)wdi->tag->regname.offset << 16);
                    } else if (strcmp("offset",  __hwi_find_string(wdi->tag->regname.regname)) == 0) {
                        if (wdi->operation == BITSWAP) {
                            //Bitswapping should default to 0xFFFFFFFF
                            wdi->write_reg[wdi->num_writes][REG_VAL] = 0xFFFFFFFF;
                            wdi->num_writes++;

                            if (wdi->num_writes >= MAX_NUM_REG) {
                                slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : %s only supports up to %d kick registers. Please check "
                                    "the command line or Hwinfo default setting.", wdi->name, MAX_NUM_REG);
                                return EXIT_FAILURE;
                            } else {
                                //This is a new offset entry
                                new_offset = true;
                            }
                        } else {
                            slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Missing value for corresponding offset. "
                                "Please check the command line or Hwinfo default setting.");
                            return EXIT_FAILURE;
                        }
                    } else {
                        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Missing value for corresponding offset. "
                            "Please check the command line or Hwinfo default setting.");
                        return EXIT_FAILURE;
                    }
                } else {
                    if (wdi->operation == BITSWAP) {
                        //Bitswapping should default to 0xFFFFFFFF
                        wdi->write_reg[wdi->num_writes][REG_VAL] = 0xFFFFFFFF;

                        //This is the last element in the table, so break
                        wdi->num_writes++;
                        return EXIT_SUCCESS;
                    } else {
                        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Missing value for corresponding offset. "
                            "Please check the command line or Hwinfo default setting.");
                        return EXIT_FAILURE;
                    }
                }
            }
        } else {
            reg_name_string =  __hwi_find_string(wdi->tag->regname.regname);
            if ((strcmp("value", reg_name_string) == 0) || (strcmp("value32_lo", reg_name_string) == 0) ||(strcmp("value32_hi", reg_name_string) == 0) ) {
                if (parse_hwinfo_value(wdi, reg_name_string) == EXIT_FAILURE) {
                    return EXIT_FAILURE;
                }
            } else {
                slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid offset/value entry. Please check the command line or Hwinfo default setting.");
                return EXIT_FAILURE;
            }
        }
        if (wdi->increment_index) {
            wdi->num_writes++;
        } else {
            wdi->increment_index = true;
        }
    }
    return EXIT_SUCCESS;
}

static int wd_parse_options_enable_offset(wdt_init_t *wdi)
{
    char *end = NULL;
    unsigned long mask;
    unsigned long parsed;

    errno = EOK;
    parsed = strtoul(optarg, &end, 16);
    if (!is_hex_digit_char(*optarg) || (errno == ERANGE) ||
            (end == optarg) ||
            (parsed > UINT32_MAX)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Enable offset is out of range. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    wdi->enable_reg_offset = (uint32_t)parsed;
    optarg = end;
    if (wdi->enable_reg_offset > wdi->len) {
        wdi->len = wdi->enable_reg_offset;
    }

    if (!is_valid_range(wdi->enable_reg_offset, errno)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Enable offset is out of range. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    if (!is_valid_separator(optarg[0])) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid separator for option '-E'. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    if (optarg[1] == 0) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid args for option '-E'. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    errno = EOK;
    mask = strtoul(optarg + 1, &end, 16);
    if (!is_hex_digit_char(optarg[1]) || (errno == ERANGE) ||
            (end == optarg + 1) ||
            (mask > UINT32_MAX)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Enable mask is out of range. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    wdi->enable_mask = (uint32_t)mask;
    optarg = end;

    if (optarg[0] == 0) {
        wdi->enable_condition = wdi->enable_mask; //If the enable condition is not present, set it to enable_mask
    } else if (!is_valid_separator(optarg[0])) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid separator for option '-E'. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    } else {
        unsigned long condition;

        errno = EOK;
        condition = strtoul(optarg + 1, &end, 16);
        /* Zero is a valid active-low condition; require the whole token. */
        if (!is_hex_digit_char(optarg[1]) || (errno == ERANGE) ||
                (end == optarg + 1) || (*end != '\0') ||
                (condition > UINT32_MAX)) {
            slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Enable condition is out of range. Please check "
                "the command line or Hwinfo default setting.");
            return EXIT_FAILURE;
        }
        wdi->enable_condition = (uint32_t)condition;
    }
    wdi->enable_condition_set = true;

    return EXIT_SUCCESS;
}

static int wd_parse_options_bitswap(wdt_init_t *wdi)
{
    char *end = NULL;
    bool value_present = false;
    unsigned long offset;
    unsigned long value;

    if (wdi->num_writes >= MAX_NUM_REG) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : %s only supports up to %d kick registers. Please check "
            "the command line or Hwinfo default setting.", wdi->name, MAX_NUM_REG);
        return EXIT_FAILURE;
    }

    errno = EOK;
    offset = strtoul(optarg, &end, 16);
    if (!is_hex_digit_char(*optarg) || (errno == ERANGE) ||
            (end == optarg) ||
            (offset > UINT32_MAX)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Offset to bitswap is out of range. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    wdi->write_reg[wdi->num_writes][REG_OFFSET] = (uint32_t)offset;
    optarg = end;

    if (wdi->write_reg[wdi->num_writes][REG_OFFSET] > wdi->len) {
        wdi->len = wdi->write_reg[wdi->num_writes][REG_OFFSET];
    }

    if ((optarg[0] != 0) && !is_valid_separator(optarg[0])) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid separator for option '-B'. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    errno = EOK;
    if (optarg[0] == 0) {
        wdi->bitswap_value = 0;
    } else if (optarg[1] == 0) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Missing bitswap value for option '-B'.");
        return EXIT_FAILURE;
    } else {
        value = strtoul(optarg + 1, &end, 16);
        if (!is_hex_digit_char(optarg[1]) || (errno == ERANGE) ||
                (end == optarg + 1) || (*end != '\0') ||
                (value > UINT32_MAX)) {
            slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Value to bitswap is out of range. Please check "
                "the command line or Hwinfo default setting.");
            return EXIT_FAILURE;
        }
        wdi->bitswap_value = (uint32_t)value;
        value_present = true;
    }
    if (value_present) {
        wdi->write_reg[wdi->num_writes][REG_VAL] = wdi->bitswap_value;
    } else {
        //Default to 0xFFFFFFFF for bitswap if no value entered
        wdi->write_reg[wdi->num_writes][REG_VAL] = 0xFFFFFFFF;
    }
    wdi->write_operation[wdi->num_writes] = BITSWAP;
    wdi->num_writes++;
    wdi->operation = BITSWAP;

    return EXIT_SUCCESS;
}

static int wd_parse_options_write(wdt_init_t *wdi)
{
    char *end = NULL;
    unsigned long offset;
    unsigned long value;

    if (wdi->num_writes >= MAX_NUM_REG) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : %s only supports up to %d kick registers. Please check "
            "the command line or Hwinfo default setting.", wdi->name, MAX_NUM_REG);
        return EXIT_FAILURE;
    }

    errno = EOK;
    offset = strtoul(optarg, &end, 16);
    if (!is_hex_digit_char(*optarg) || (errno == ERANGE) ||
            (end == optarg) ||
            (offset > UINT32_MAX)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Offset to write is out of range. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    wdi->write_reg[wdi->num_writes][REG_OFFSET] = (uint32_t)offset;
    optarg = end;

    if (wdi->write_reg[wdi->num_writes][REG_OFFSET] > wdi->len) {
        wdi->len = wdi->write_reg[wdi->num_writes][REG_OFFSET];
    }

    if (!is_valid_separator(optarg[0])) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid separator for option '-W'. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    errno = EOK;
    value = strtoul(optarg + 1, &end, 16);
    if (!is_hex_digit_char(optarg[1]) || (errno == ERANGE) ||
            (end == optarg + 1) || (*end != '\0') ||
            (value > UINT32_MAX)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Value to write is out of range. Please check "
            "the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    wdi->write_reg[wdi->num_writes][REG_VAL] = (uint32_t)value;

    wdi->write_operation[wdi->num_writes] = WRITE;
    wdi->num_writes++;
    wdi->operation = WRITE;

    return EXIT_SUCCESS;
}

static int wd_parse_options(const int argc, char * const argv[], wdt_init_t *wdi)
{
    int ret;
    uint64_t val = 0ULL;
    int64_t val1 = 0;
    /* Process dash options.*/
    while ((wdi->opt = getopt(argc, argv, "a:l:p:t:w:r:m:R:E:B:W:v")) != -1) {
        switch (wdi->opt) {
        case 'a':    // WDOG register physical base address
            errno = EOK;
            val = strtoull(optarg, NULL, 0);
            if (errno == EOK) {
                wdi->physbase = val;
            }
            break;
        case 'l':
            errno = EOK;
            val = strtoul(optarg, NULL, 0);
            if (errno == EOK) {
                wdi->len = (size_t)val;
            }
            break;
        case 'p':    // priority
            errno = EOK;
            val1 = strtol(optarg, NULL, 0);
            if (errno == EOK) {
                wdi->priority = (int)val1;
            }
            break;
        case 't':
        {
            char *end = NULL;
            errno = EOK;
            val = strtoul(optarg, &end, 0);
            if ((*optarg >= '0') && (*optarg <= '9') && (errno == EOK) &&
                    (end != optarg) && (*end == '\0') &&
                    (val > 0U) && (val <= UINT_MAX)) {
                wdi->kick_time = (long)val;
            } else {
                return EXIT_FAILURE;
            }
            break;
        }
        case 'w':
            if (!parse_uint32_option(optarg, &wdi->write_width)) {
                return EXIT_FAILURE;
            }
            break;
        case 'r':
            if (!parse_uint32_option(optarg, &wdi->enable_width)) {
                return EXIT_FAILURE;
            }
            break;
        case 'E':
            ret = wd_parse_options_enable_offset(wdi);
            if (ret == EXIT_FAILURE) {
                return ret;
            }
            break;
        case 'B':
            ret = wd_parse_options_bitswap(wdi);
            if (ret == EXIT_FAILURE) {
                return ret;
            }
            break;
        case 'W':
            ret = wd_parse_options_write(wdi);
            if (ret == EXIT_FAILURE) {
                return ret;
            }
            break;
        case 'v':
            wdi->verbose++;
            break;
        default:
            break;
        }
    }
    return EXIT_SUCCESS;
}

static int wd_evaluate_params(wdt_init_t const * const wdi)
{
    /*check if the params are valid*/
    if (wdi->physbase == 0) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid  WDOG register physical base address. Please check the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    if ((wdi->kick_time <= 0) || ((unsigned long)wdi->kick_time > UINT_MAX)) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid default time for watchdog timer kick. Please check the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    if (wdi->operation == -1) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : Invalid operation for watchdog timer kick. Please check the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }
    if (wdi->num_writes == 0) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick error : No registers specified for watchdog timer kick. Please check the command line or Hwinfo default setting.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int wd_setup_write_width(wdt_init_t const * const wdi)
{
    switch (wdi->write_width) {
    case 32:
        write_wdt = &write_wdt_32;
        read_write_wdt = &read_wdt_32;
        break;
    case 16:
        write_wdt = &write_wdt_16;
        read_write_wdt = &read_wdt_16;
        break;
    case 8:
        write_wdt = &write_wdt_8;
        read_write_wdt = &read_wdt_8;
        break;
    default:
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick: Invalid register write_width (%d) for watchdog timer kick."
                " Please check the command line or Hwinfo default setting.", wdi->write_width);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int wd_setup_enable_width(wdt_init_t const * const wdi)
{
    switch (wdi->enable_width) {
    case 32:
        read_wdt = &read_wdt_32;
        write_enable_wdt = &write_wdt_32;
        break;
    case 16:
        read_wdt = &read_wdt_16;
        write_enable_wdt = &write_wdt_16;
        break;
    case 8:
        read_wdt = &read_wdt_8;
        write_enable_wdt = &write_wdt_8;
        break;
    default:
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick: Invalid register enable_width (%d) for watchdog timer kick."
                " Please check the command line or Hwinfo default setting.", wdi->enable_width);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int wd_validate_mapping(wdt_init_t *wdi)
{
    const size_t write_bytes = (size_t)wdi->write_width / 8U;
    const size_t enable_bytes = (size_t)wdi->enable_width / 8U;
    size_t required = 0U;

    if (wdi->enable_mask != 0U) {
        if ((size_t)wdi->enable_reg_offset > (SIZE_MAX - enable_bytes)) return EXIT_FAILURE;
        required = (size_t)wdi->enable_reg_offset + enable_bytes;
    }
    for (uint32_t i = 0U; i < wdi->num_writes; ++i) {
        size_t end;
        const size_t access_bytes = (wdi->write_operation[i] == BITSWAP) &&
            (enable_bytes > write_bytes) ? enable_bytes : write_bytes;
        if ((size_t)wdi->write_reg[i][REG_OFFSET] > (SIZE_MAX - access_bytes)) return EXIT_FAILURE;
        end = (size_t)wdi->write_reg[i][REG_OFFSET] + access_bytes;
        if (end > required) required = end;
    }
    /* Include every byte subsequently read or written, not just its offset. */
    if (required > wdi->len) wdi->len = required;
    return EXIT_SUCCESS;
}

static int wd_configure_thread(wdt_init_t *wdi)
{
    // Enable IO capability.
    if (ThreadCtl( _NTO_TCTL_IO, NULL ) == -1) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick: failure to acquire IO privity");
        return EXIT_FAILURE;
    }
    // run in the background
    if (procmgr_daemon( EXIT_SUCCESS, PROCMGR_DAEMON_NOCLOSE | PROCMGR_DAEMON_NODEVNULL ) == -1) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"%s:  procmgr_daemon", wdi->name);
        return EXIT_FAILURE;
    }

    // configure information
    if (wdi->verbose) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO, "wdtkick: paddr = 0x%zx, size = 0x%zx, kick = %ld, priority = %d",
              wdi->physbase, wdi->len, wdi->kick_time, wdi->priority);
    }

    // Set priority
    if (pthread_getschedparam(pthread_self(),&(wdi->sched_type), &(wdi->threadparam)) != EOK) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_WARNING,"wdtkick: get priority request failed");
        return EXIT_FAILURE;
    }
    if (wdi->priority != wdi->threadparam.sched_priority) {
        wdi->threadparam.sched_priority = wdi->priority;
        if (pthread_setschedparam(pthread_self(),wdi->sched_type, &(wdi->threadparam)) != EOK) {
            slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_WARNING,"wdtkick: can't change priority");
        }
    }

    return EXIT_SUCCESS;
}

int main(const int argc, char *argv[])
{
    int ret;
    wdt_init_t wdi;        /* wdtkick init structure */

    wdi.priority = 10;    /* default priority:default 10 */
    wdi.kick_time = -1;    /* default time for watchdog timer kick */
    wdi.base = 0;
    wdi.len = DEFAULT_REG_LEN;
    wdi.physbase = 0;
    wdi.write_width = 32;
    wdi.enable_width = 32;
    wdi.enable_mask = 0;
    wdi.enable_condition = 0;
    wdi.enable_reg_offset = 0;
    wdi.num_writes = 0;
    wdi.increment_index = true;
    wdi.enable_condition_set = false;
    wdi.operation = -1;
    wdi.verbose = 0;
    wdi.name = argv[0];

    ret = wd_parse_hwinfo(&wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    /* Hwinfo follows the documented -E default when no condition tag exists. */
    if ((wdi.enable_mask != 0U) && !wdi.enable_condition_set) {
        wdi.enable_condition = wdi.enable_mask;
    }

    for (wdi.idx = 0U; wdi.idx < wdi.num_writes; ++wdi.idx) {
        wdi.write_operation[wdi.idx] = (uint8_t)wdi.operation;
    }

    ret = wd_parse_options(argc, argv, &wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    ret = wd_evaluate_params(&wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    ret = wd_setup_write_width(&wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    ret = wd_setup_enable_width(&wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    ret = wd_validate_mapping(&wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    ret = wd_configure_thread(&wdi);
    if (ret == EXIT_FAILURE) {
        return ret;
    }

    wdi.base = mmap_device_io(wdi.len, wdi.physbase);
    if (wdi.base == MAP_DEVICE_FAILED) {
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick: failed to map watchdog timer registers");
        return EXIT_FAILURE;
    }

    slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick: enable mask: 0x%x", wdi.enable_mask);

    // Configure the enable register
    if (wdi.enable_mask && ((read_wdt(wdi.base + wdi.enable_reg_offset) & wdi.enable_mask) !=
            (wdi.enable_condition & wdi.enable_mask))) {
        const uint32_t current = read_wdt(wdi.base + wdi.enable_reg_offset);
        slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_INFO,"wdtkick: enabling the watchdog timer");
        write_enable_wdt(wdi.base + wdi.enable_reg_offset,
                (current & ~wdi.enable_mask) | (wdi.enable_condition & wdi.enable_mask));
    }

    while (1) {
        for (wdi.idx = 0; wdi.idx < wdi.num_writes; wdi.idx ++) {
            if (wdi.write_operation[wdi.idx] == BITSWAP) {
                write_wdt(wdi.base + wdi.write_reg[wdi.idx][REG_OFFSET], read_write_wdt(wdi.base + wdi.write_reg[wdi.idx][REG_OFFSET]) ^ wdi.write_reg[wdi.idx][REG_VAL]);
            }
            if (wdi.write_operation[wdi.idx] == WRITE) {
                write_wdt(wdi.base + wdi.write_reg[wdi.idx][REG_OFFSET], wdi.write_reg[wdi.idx][REG_VAL]);
            }
        }
        delay((unsigned int)wdi.kick_time);
    }

    return EXIT_SUCCESS;
}

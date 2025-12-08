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
 * Copyright (c) 2020,2022,2024-2025, BlackBerry Limited.
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

#include <startup.h>
#include "mbox.h"
#include "bcm2712_startup.h"

/*
 *  Please refer to https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md
 *  New-style revision codes:
 *    NOQuuuWuFMMMCCCCPPPPTTTTTTTTRRRR in which:
 *      N         Overvoltage
 *      O         OTP Program
 *      Q         OTP Read
 *      uuu       Unused
 *      W         Warranty bit
 *      u         Unused
 *      F         New flag
 *      MMM       Memory size
 *      CCCC      Manufacturer
 *      PPPP      Processor
 *      TTTTTTTT  Type
 *      RRRR      Revision
 */
#define BOARD_REV_TYPE_SHIFT    4
#define BOARD_REV_TYPE_MASK     0xFF

#define BOARD_REV_SOC_SHIFT     12
#define BOARD_REV_SOC_MASK      0xF

#define BOARD_REV_MFR_SHIFT     16
#define BOARD_REV_MFR_MASK      0xF

#define BOARD_REV_MEM_SHIFT     20
#define BOARD_REV_MEM_MASK      0x7

const char *get_soc_name(const uint32_t board_rev)
{
    const char * const socs[] = {
        [0] = "BCM2835",
        [1] = "BCM2836",
        [2] = "BCM2837",
        [3] = "BCM2711",
        [4] = "BCM2712",
    };

    const uint32_t idx = (board_rev >> BOARD_REV_SOC_SHIFT) & BOARD_REV_SOC_MASK;
    if (idx < NUM_ELTS(socs)) {
        return socs[idx];
    }
    return "Unknown";
}

const unsigned get_memory_size(const uint32_t board_rev)
{
    const unsigned memory[] = {
        [0] = 256,
        [1] = 512,
        [2] = 1024,
        [3] = 2048,
        [4] = 4096,
        [5] = 8192,
        [6] = 16384,
    };

    const uint32_t idx = (board_rev >> BOARD_REV_MEM_SHIFT) & BOARD_REV_MEM_MASK;
    if (idx < NUM_ELTS(memory)) {
        return memory[idx];
    }
    return 0;
}

const char *get_mfr_name(const uint32_t board_rev)
{
    const char * const mfrs[] = {
        [0] = "Sony UK",
        [1] = "Egoman",
        [2] = "Embest",
        [3] = "Sony Japan",
        [4] = "Embest",
        [5] = "Stadium",
    };

    const uint32_t idx = (board_rev >> BOARD_REV_MFR_SHIFT) & BOARD_REV_MFR_MASK;
    if (idx < NUM_ELTS(mfrs)) {
        return mfrs[idx];
    }
    return "Unknown";
}

const char * get_board_name(const uint32_t board_rev)
{
    const char * const types[] = {
        [0]  = "RaspberryPi1A",
        [1]  = "RaspberryPi1B",
        [2]  = "RaspberryPi1A+",
        [3]  = "RaspberryPi1B+",
        [4]  = "RaspberryPi2B",
        [5]  = "RaspberryPiAlpha",
        [6]  = "RaspberryPiCM1",
        [7]  = "Unknown",
        [8]  = "RaspberryPi3B",
        [9]  = "RaspberryPi0",
        [10] = "RaspberryPiCM3",
        [11] = "Unknown",
        [12] = "RaspberryPi0W",
        [13] = "RaspberryPi3B+",
        [14] = "RaspberryPi3A+",
        [15] = "Unknown",
        [16] = "RaspberryPiCM3+",
        [17] = "RaspberryPi4B",
        [18] = "RaspberryPi02W",
        [19] = "RaspberryPi400",
        [20] = "RaspberryPiCM4",
        [21] = "RaspberryPiCM4S",
        [22] = "RaspberryPi--",
        [23] = "RaspberryPi5",
        [24] = "RaspberryPiCM5",
        [25] = "RaspberryPi500",
        [26] = "RaspberryPiCM5Lite",
    };

    const uint32_t idx = (board_rev >> BOARD_REV_TYPE_SHIFT) & BOARD_REV_TYPE_MASK;
    if (idx < NUM_ELTS(types)) {
        return types[idx];
    }
    return "Unknown";
}

static char serial[128];
const char *get_board_serial(void)
{
    ksprintf(serial, "%L", mbox_get_board_serial());
    return serial;
}

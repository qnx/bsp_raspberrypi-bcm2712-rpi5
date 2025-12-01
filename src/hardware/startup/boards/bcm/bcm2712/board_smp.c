/*
 * Copyright (c) 2020,2022,2024 BlackBerry Limited.
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
#include <aarch64/gic_v2.h>
#include "aarch64/psci.h"

static uint32_t psci_ver = 0;
static const uint64_t pi5_cpu_ids[4] = {0x0, 0x100, 0x200, 0x300 }; /* from pi5 FTD */

unsigned board_smp_num_cpu(void) {
    return 4;
}

uint64_t psci_cpu_id(unsigned const cpu)
{
    return pi5_cpu_ids[cpu];
}

void board_smp_init(struct smp_entry *smp, const unsigned num_cpus)
{
    smp->send_ipi = (void *)&sendipi_gic_v2;

    psci_ver = (uint32_t)psci_call(PSCI_VERSION, 0, 0, 0);
    if (debug_flag > 1) {
        kprintf("psci_ver: %x\n", psci_ver);
    }
}

int board_smp_start(const unsigned cpu, void (*start)(void))
{
#if 1
    return psci_smp_start(cpu, start);
#else
    typedef void (*start_t)(void);
    volatile start_t *cpu_start = start;
    uint64_t status;

    status = psci_call(PSCI_CPU_ON, (uint64_t)(cpu << 8), (uint64_t)start, 0);
    if (status != PSCI_SUCCESS) {
        crash("%s: psci_call PSCI_CPU_ON cpu: %lu failed! return: %L. \n", __func__, cpu, status);
    }

    if (debug_flag) {
        kprintf("%s: psci_call PSCI_CPU_ON cpu: %lu ok\n", __func__, cpu);
    }
    return 1;
#endif
}

unsigned
board_smp_adjust_num(const unsigned cpu)
{
    return cpu;
}

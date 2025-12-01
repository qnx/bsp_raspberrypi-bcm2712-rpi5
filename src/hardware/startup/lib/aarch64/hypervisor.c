/*
 * Copyright (c) 2023, BlackBerry Limited.
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
#include <startup.h>

// Declare helder assembler routines
extern void hyp_enable_el2_host(void);
extern void hyp_enable_el1_host(paddr_t const vbar_el2);
extern void drop_to_el1(void);

// Needed for EL1 host
static paddr_t     transfer_vector_table;

/**
 * This function evaluates the provided hypervisor flags and returns the closest
 * set of flags that the hardware can actually do.
 * @param option_flags HYP_FLAG_* options that control setup for hypervisor
 * @returns closest options that the hardware can do
 */
hyp_flags_t
arch_hypervisor_validate_flags(hyp_flags_t  const option_flags) {
    if (option_flags == HYP_FLAG_INVALID) {
        crash("Unrecognized hypervisor option flag\n");
    }

    int const  current_el = (aa64_sr_rd64(CurrentEL) >> 2);
    if (current_el == 1) {
        // There is nothing we can do unless we are already at EL2
        if (option_flags & HYP_FLAG_ENABLED) {
            crash("Hypervisor support requested but CPU has no EL2 support\n");
        }
        return HYP_FLAG_DISABLED;
    }

    if (option_flags & HYP_FLAG_ENABLED) {
        int const el2_host_supported = (aa64_sr_rd64(id_aa64mmfr1_el1) & 0x0f00);
        int const el2_host_requested = (option_flags & HYP_FLAG_EL2_HOST);
        int const el1_host_requested = (option_flags & HYP_FLAG_EL1_HOST);

        if (el2_host_requested && !el2_host_supported) {
            crash("Hypervisor EL2 Host requested but CPU does not support this\n");
        }
        if (el1_host_requested || !el2_host_supported) {
            return (HYP_FLAG_ENABLED | HYP_FLAG_EL1_HOST);
        }
        return (HYP_FLAG_ENABLED | HYP_FLAG_EL2_HOST);
    }

    return HYP_FLAG_DISABLED;
}


/**
 * IMPORTANT: it's critical to realize that when the CPU is put into VHE mode,
 * accesses to most EL1 registers become access to EL2 registers. This function
 * is where the switch to EL2&0 happens and if the hypervisor is being enabled
 * for VHE/Host EL2 operation, then any EL1 register configuration performed
 * prior to calling this function will not be transfered to EL2.
 *
 * @param cpunum     to distinguish boot and secondary cores
 * @param hyp_flags  init flags
 */
void
arch_hypervisor_init(unsigned const cpunum, hyp_flags_t const hyp_flags) {
    int const  current_el = (aa64_sr_rd64(CurrentEL) >> 2);

    switch(hyp_flags) {
    case HYP_FLAG_DISABLED:
        if (debug_flag && (cpunum == 0)) {
            kprintf("Hypervisor support disabled\n");
        }
        if (current_el != 1) {
            drop_to_el1();
        }
        break;
    case (HYP_FLAG_ENABLED | HYP_FLAG_EL2_HOST):
        if (debug_flag && (cpunum == 0)) {
            kprintf("Enabling EL2 host hypervisor support (VHE)\n");
        }
        hyp_enable_el2_host();
        break;
    case (HYP_FLAG_ENABLED | HYP_FLAG_EL1_HOST):
        if (cpunum == 0) {
            if (debug_flag) {
                kprintf("Enabling EL1 host hypervisor support\n");
            }

            // allocate memory for an EL2 exception vector table
            transfer_vector_table = alloc_ram(NULL_PADDR, 0x800, 0x800);
            if (transfer_vector_table == NULL_PADDR) {
                crash("No memory for EL2 exception table.\n");
            }
            as_add(transfer_vector_table, transfer_vector_table+0x7ff, AS_ATTR_RAM,
                "hypervisor_vector", as_find(AS_NULL_OFF, "memory", NULL));
        }
        hyp_enable_el1_host(transfer_vector_table);
        drop_to_el1();
        break;
    default:
        crash("Unknown required hypervisor mode\n");
        break;
    }
}

/**
 * Make sure the virtualization id field is 0.
 * There's only the one set of virtualization extensions on ARMv8.
 * @param   sysp_hypinfo    hypinfo section of syspage
 */
void
arch_hypervisor_syspage_init(struct hypinfo_entry * const sysp_hypinfo)
{
    sysp_hypinfo->flags &= ~HYPINFO_VIRT_MASK;
}

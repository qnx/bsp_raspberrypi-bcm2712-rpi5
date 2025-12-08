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


// User requested HYP_FLAG_* option flags
static hyp_flags_t  option_flags = HYP_FLAG_DISABLED;

// Flags that are required for all cores
static hyp_flags_t  required_flags  = HYP_FLAG_DISABLED;


/**
 * Sets the user requested hypervisor configuration. Must be called prior to
 * using hypervisor_init().
 *
 * @param flags HYP_FLAG_* options that control setup for hypervisor
 */
void
hypervisor_set_options(hyp_flags_t  const flags) {
    // This function is typically called prior to debug serial output being
    // setup, so we defer validating option_flags until hypervisor_init() is
    // called and debug output is possible.
    option_flags = flags;
}


/**
 * This function is used to configure hypervisor support on all cores. It
 * expects to be called first on the bootcore, and then secondary cores.
 * hypervisor init will only succeed if all secondary cores can be configured to
 * match the bootcore.
 *
 * @param cpunum       to distinguish boot and secondary cores
 * @param option_flags HYP_FLAG_* options that control setup for hypervisor
 */
void
hypervisor_init(unsigned const cpunum) {
    hyp_flags_t  selected_flags;

    selected_flags = arch_hypervisor_validate_flags(option_flags);

    // Cpu 0 determines the required flags all others must support
    if (cpunum == 0) {
        required_flags = selected_flags;
    } else {
        if (selected_flags != required_flags) {
            crash("Hypervisor setup: cpu %d, does not match cpu 0 capability");
        }
    }

    arch_hypervisor_init(cpunum, required_flags);
}


/**
 * Allows the caller to determine the required hypervisor flags
 * @return the required hypervisor flags
 */
hyp_flags_t
hypervisor_get_required_flags(void) {
    return required_flags;
}


/**
 * Initialize hypervisor syspage data
 */
void
hypervisor_syspage_init(void) {
    struct hypinfo_entry  *sysp_hypinfo = set_syspage_section(&lsp.hypinfo, sizeof(*lsp.hypinfo.p));

    if (required_flags & HYP_FLAG_ENABLED) {
        sysp_hypinfo->flags = HYPINFO_ENABLED;
    } else {
        sysp_hypinfo->flags = 0;
    }

    arch_hypervisor_syspage_init(sysp_hypinfo);
}

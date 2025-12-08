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
    .text
    .align 2

/******************************************************************************
 * Initialize EL2 for VHE and the EL2 hosted hypervisor
 * hyp_enable_el2_host(void)
 */
    .global hyp_enable_el2_host
hyp_enable_el2_host:
    // Make sure hypervisor timer is disabled
    msr     cnthp_ctl_el2,xzr

    // Configure HCR_EL2 for VHE operation
    mrs     x3,hcr_el2
    orr     x3,x3,#(1 << 27)   // turn on HCR_EL2.TGE
    orr     x3,x3,#(1 << 34)   // turn on HCR_EL2.E2H
    msr     hcr_el2,x3
    isb
    ret

/******************************************************************************
 * Initialize EL2 for an EL1 hosted hypervisor
 * hyp_enable_el1_host(x0=vbar_el2)
 */
    .global hyp_enable_el1_host
hyp_enable_el1_host:
    // Setup EL2 vector table base register
    msr     vbar_el2, x0

    /*
     * Enable HVC via HCR_EL2.HCD [29] (0=enable)
     */
    mrs     x0, hcr_el2
    bic     x0, x0, #(1 << 29)
    msr     hcr_el2, x0

    ret

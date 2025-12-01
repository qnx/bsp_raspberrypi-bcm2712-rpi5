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
#include <hw/acpi.h>

void
acpi_spcr_parse(struct debug_device * const dbgdev)
{
    unsigned len;
    paddr_t const spcr_paddr = acpi_find_table(ACPI_SPCR_SIGNATURE, &len);
    if (spcr_paddr == NULL_PADDR) return;

    acpi_spcr const * const spcr = startup_memory_map(len, spcr_paddr, PROT_READ);

    switch (spcr->Interface_Type) {
#if defined(__aarch64__)
    case ACPI_SPCR_PL011:
    case ACPI_SPCR_SBSA:
        if (spcr->Base_Address.Address_Space_ID == ACPI_GAS_ASID_SYSTEM_MEMORY) {
            dbgdev->name = "acpi_spcr";
            dbgdev->init = init_pl011;
            dbgdev->put = put_pl011;
            dbgdev->callouts[DEBUG_DISPLAY_CHAR] = &display_char_pl011;
            dbgdev->callouts[DEBUG_POLL_KEY] = &poll_key_pl011;
            dbgdev->callouts[DEBUG_BREAK_DETECT] = &break_detect_pl011;

            dbgdev->defaults[DEBUG_DEV_KDEBUG] = NULL;

            char *dev_string = ws_alloc(sizeof("0123456789abcdef^0.0.0"));
            ksprintf(dev_string, "%l^0.0.0", spcr->Base_Address.Address.sm);
            dbgdev->defaults[DEBUG_DEV_CONSOLE] = dev_string;
        }
        break;
#endif

    default:
        break;
    }

    startup_memory_unmap((void *)spcr);
}

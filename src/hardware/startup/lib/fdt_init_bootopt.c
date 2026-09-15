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
 * Copyright (c) 2016,2022,2023, BlackBerry Limited.
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
#include <errno.h>
#include <libfdt.h>
#include <sys/libfdt_private.h>

int
fdt_init_bootopt(void) {
#if defined(__aarch64__)
    char** argv;
    void *header;

    /* prevent fdt processing twice */
    if ( fdt_paddr || fdt_size ) {
        kprintf("fdt at %P was already processed.\n", fdt_paddr);
        return 0;
    }

    if ( debug_flag > 1 ) {
        kprintf("fdt_boot_option 0x%x\n", fdt_boot_option);
    }

    switch (fdt_boot_option) {
        case FDT_BOOTOPT_ADDRPASS_BY_REG:
            fdt_paddr = boot_regs[FDT_REG];
            break;
        case FDT_BOOTOPT_ADDRPASS_BY_ARGS:
            argv = (char **)boot_regs[FDT_REG + 1];
            if ((argv != NULL) && (argv[1] != NULL)) {
                char *end;
                const char *const address = argv[1];
                unsigned long long parsed;
                const int starts_with_hex_digit =
                    ((*address >= '0') && (*address <= '9')) ||
                    ((*address >= 'a') && (*address <= 'f')) ||
                    ((*address >= 'A') && (*address <= 'F'));

                errno = 0;
                parsed = strtoull(address, &end, 16);
                if ((errno == ERANGE) || (end == address) || (*end != '\0') ||
                    !starts_with_hex_digit ||
                    (parsed > (unsigned long long)NULL_PADDR)) {
                    kprintf("ERROR: invalid fdt address argument\n");
                    return 0;
                }
                fdt_paddr = (paddr_t)parsed;
            }
            break;
        default:
            kprintf("unknown fdt_boot_option 0x%x\n", fdt_boot_option);
            /* continue to boot without fdt support */
            return 0;
    }

    /* make sure the address is 4-byte aligned to eliminate any error cases */
    if ( (fdt_paddr == 0) || ((fdt_paddr % 4) != 0) ||
         (fdt_paddr > (NULL_PADDR -
                       (paddr_t)(sizeof(struct fdt_header) - 1U))) ) {
        kprintf("ERROR: invalid fdt address %P\n", fdt_paddr);
        /* continue to boot without fdt support */
        return 0;
    }

    /*
     * Map and validate only the fixed header before trusting totalsize.  This
     * avoids using attacker-controlled length fields to create the first map.
     */
    header = startup_memory_map(sizeof(struct fdt_header), fdt_paddr, PROT_READ);
    if ((header == NULL) || (fdt_check_header(header) != 0)) {
        /*
         * If there is wrong fdt header found in the given fdt contents,
         * bootup process should fail here.
         */
        crash("ERROR: No FDT data found at %P!\n", fdt_paddr);
    }
    fdt_size = (size_t)fdt_totalsize(header);
    startup_memory_unmap(header);

    if ((fdt_size < sizeof(struct fdt_header)) ||
        (fdt_size > STARTUP_FDT_MAX_SIZE) ||
        ((paddr_t)(fdt_size - 1U) > (NULL_PADDR - fdt_paddr))) {
        crash("ERROR: Invalid FDT size %u at %P!\n",
              (unsigned)fdt_size, fdt_paddr);
    }

    /* So far, the fdt option and address have been verified good enough. */
    if ( debug_flag > 1 ) {
        kprintf("fdt blob is located at %P with the size of %u\n",
                fdt_paddr, (unsigned)fdt_size);
    }

    /* fdt variable can be accessed to read fdt content from the startup code */
    fdt = startup_memory_map((unsigned)fdt_size, fdt_paddr, PROT_READ);
    if ((fdt == NULL) || (fdt_check_header(fdt) != 0) ||
        ((size_t)fdt_totalsize(fdt) != fdt_size)) {
        crash("ERROR: Failed to map valid FDT data at %P!\n", fdt_paddr);
    }
#endif

    return 0;
}

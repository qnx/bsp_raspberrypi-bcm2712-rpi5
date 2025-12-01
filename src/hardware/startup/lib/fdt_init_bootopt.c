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
#include <libfdt.h>
#include <sys/libfdt_private.h>

int
fdt_init_bootopt(void) {
#if defined(__aarch64__)
    char** argv;

    /* prevent fdt processing twice */
    if ( fdt_paddr || fdt_size ) {
        kprintf("fdt at 0x%x was already processed.\n", fdt_paddr);
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
            if ( argv ) {
                fdt_paddr = strtoul(argv[1], NULL, 16);
            }
            break;
        default:
            kprintf("unknown fdt_boot_option 0x%x\n", fdt_boot_option);
            /* continue to boot without fdt support */
            return 0;
    }

    /* make sure the address is 4-byte aligned to eliminate any error cases */
    if ( (fdt_paddr == 0) || ((fdt_paddr % 4) != 0) ) {
        kprintf("ERROR: invalid fdt address 0x%x\n", fdt_paddr);
        /* continue to boot without fdt support */
        return 0;
    }

    fdt_size = fdt_totalsize(fdt_paddr);

    if (fdt_check_header((void*)fdt_paddr) || (fdt_size == 0)) {
        /*
         * If there is wrong fdt header found in the given fdt contents,
         * bootup process should fail here.
         */
        crash("ERROR: No FDT data found at 0x%x!\n", fdt_paddr);
    }

    /* So far, the fdt option and address have been verified good enough. */
    if ( debug_flag > 1 ) {
        kprintf("fdt blob is located at 0x%x with the size of %d\n", fdt_paddr, fdt_size);
    }

    /* fdt variable can be accessed to read fdt content from the startup code */
    fdt = startup_memory_map((unsigned)fdt_size, fdt_paddr, PROT_READ);
#endif

    return 0;
}

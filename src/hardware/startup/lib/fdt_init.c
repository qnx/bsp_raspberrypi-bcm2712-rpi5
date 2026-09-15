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
#include <libfdt.h>
#include <sys/libfdt_private.h>

void
fdt_init(paddr_t paddr) {
	void	*header = 0;
	size_t	declared_size;

	if(fdt == NULL) {
		fdt_paddr = paddr;
		fdt_size = 0;

		/* Validate the fixed-header extent before the first physical mapping. */
		if((fdt_paddr == 0) || ((fdt_paddr % 4U) != 0U) ||
				(fdt_paddr > (NULL_PADDR -
					(paddr_t)(sizeof(struct fdt_header) - 1U)))) {
			return;
		}

		header = startup_memory_map(sizeof(struct fdt_header), fdt_paddr, PROT_READ);

		if((header != NULL) && (fdt_check_header(header) == 0)) {
			declared_size = (size_t)fdt_totalsize(header);
			if((declared_size >= sizeof(struct fdt_header)) &&
					(declared_size <= STARTUP_FDT_MAX_SIZE) &&
					((paddr_t)(declared_size - 1U) <=
					 (NULL_PADDR - fdt_paddr))) {
				fdt_size = declared_size;
			}
		}

		if(header != NULL) {
			startup_memory_unmap(header);
		}

		if(fdt_size != 0) {
			fdt = startup_memory_map(fdt_size, fdt_paddr, PROT_READ);
			/* The full mapping must still contain the header we validated. */
			if((fdt == NULL) || (fdt_check_header(fdt) != 0) ||
					((size_t)fdt_totalsize(fdt) != fdt_size)) {
				if(fdt != NULL) {
					startup_memory_unmap(fdt);
				}
				fdt = NULL;
				fdt_size = 0;
			}
		}
	}
}


void
fdt_asinfo(void) {
	if(fdt != NULL) {
		as_add_containing(fdt_paddr, fdt_paddr + fdt_size - 1, AS_ATTR_ROM, "fdt", "memory");
		alloc_ram(fdt_paddr, fdt_size, 1);
	}
}

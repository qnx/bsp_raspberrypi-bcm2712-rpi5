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

	if(fdt == NULL) {
		fdt_paddr = paddr;

		header = startup_memory_map(sizeof(struct fdt_header), fdt_paddr, PROT_READ);

		if(fdt_check_header(header) == 0) {
			fdt_size = fdt_totalsize(header);
		}

		startup_memory_unmap(header);

		if(fdt_size != 0) {
			fdt = startup_memory_map(fdt_size, fdt_paddr, PROT_READ);
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

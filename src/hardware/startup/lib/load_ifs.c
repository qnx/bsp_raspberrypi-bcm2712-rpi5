/*
 * $QNXLicenseC:
 * Copyright 2008, QNX Software Systems.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */





#include <startup.h>


void
load_ifs(paddr_t ifs_paddr) {
	int			comp;
	paddr_t		src;

	if(shdr == NULL) {
		crash("NULL shdr");
	}
	if(!full_image_paddr || !shdr->startup_size) {
		crash("startup: shdr: %V, image_paddr: %P, startup_size: %x\nImage header corrupt\n",
				shdr, (paddr_t)full_image_paddr, shdr->startup_size);
	}

	if (debug_flag > 0) kprintf("Loading IFS...");

	board_enable_caches();
	comp = shdr->flags1 & STARTUP_HDR_FLAGS1_COMPRESS_MASK;
	if(comp != 0) {
		src = full_imagefs_paddr;
		if(src == 0) {
			/* If imagefs_paddr is zero, then we were loaded by some code
			 * that did not do an image_setup(). This was probably because
			 * we were loaded by a non-neutrino IPL (or an emulator).
			 *
			 * We'll copy the compressed FS out of the way and then
			 * uncompress it back to where it belongs. Since we just
			 * need the temporary memory for a short period of time,
			 * we won't bother actually allocating it.
			 *
			 */
			src = find_ram(shdr->stored_size, sizeof(uint64_t), 0, 0);
			copy_memory(src, ifs_paddr, shdr->stored_size);
		}

		// If the uncompressed image-size is GREATER than allowed space of expansion,
		// vital memory space will land up being corrupted/overwritten.
		if ((full_imagefs_paddr - full_image_paddr) < shdr->imagefs_size)
			crash("\n\t *** Warning! Uncompressing this image will exceed allotted space & cause memory corruption! *** \n");

		uncompress(comp, ifs_paddr, src);
	} else if((full_imagefs_paddr != 0) &&
			 (full_imagefs_paddr != ifs_paddr)) {
		copy_memory(ifs_paddr, shdr->imagefs_paddr, shdr->imagefs_size);
	}

	board_disable_caches();
	if (debug_flag > 0) kprintf("done\n");
}

/*
* $QNXLicenseC:
* Copyright 2017, QNX Software Systems.
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

/* For 64KiB translation granule
* 512 MiB blocks
* TLB level 2 block descriptor format:
*        bit[0]: valid
*        bit[1]: 0-block,1-table
*        bit[5:2]: MemAttr
*        bit[10]: Access flag
*        bit[47:29]: addr
*
* For 4KiB translation granule
* 1 GiB blocks
* TLB level 1 block descriptor format:
*        bit[0]: valid
*        bit[1]: 0-block,1-table
*        bit[5:2]: MemAttr
*        bit[10]: Access flag
*        bit[47:30]: addr
*/

#ifndef __AARCH64_TLB_H__
#define __AARCH64_TLB_H__

//#define USE_4K_PT_SETUP (1)


#if !defined(USE_4K_PT_SETUP)
/* map 42 bits, 4TiB, 1 L2 page (64k), 512MiB/entry */
#define TLB_SIZE            (8192)
#define PAGE_SHIFT          29
#else
/* map 39 bits, 512GiB, 1 L1 page (4k), 1GiB/entry */
#define TLB_SIZE            (512)
#define PAGE_SHIFT          30
#endif
#define PAGE_SIZE           (1UL << PAGE_SHIFT)
#define BA_MASK             (((1UL<<48)-1) & ~(PAGE_SIZE-1))

#define VALID               (0x1UL << 0)
#define AF                  (0x1UL << 10)
#define ISH                 (0x3UL << 8)
#define NX                  (0x3UL << 53)

typedef struct aarch64_tlb {
    uint64_t start;
    uint64_t len;
    uint64_t attr;
} aarch64_tlb_t;

/* Construct a translation table to cover 42 bit address space
* Please NOTE that the tlb needs to be 64K aligned (0x10000)
*/
static __inline__ void aarch64_setup_tlb(aarch64_tlb_t *table, uint64_t *tlb)
{
    uint64_t base;
    uint64_t offset = 0;

    while (table && (table->start != (uint64_t)-1)) {
       base = table->start;

        /* uncacheable */
        for (; offset < (base >> PAGE_SHIFT); offset++) {
            tlb[offset] = NX | (offset << PAGE_SHIFT) | VALID | AF;
        }

        for (; (base >= table->start) && (table->len > base - table->start) && (offset < TLB_SIZE); base += PAGE_SIZE, offset++) {
            tlb[offset] = (base & BA_MASK)
                            | table->attr
                            | VALID | AF;
        }
        table++;
    }

    /* uncacheable */
    for (; offset < TLB_SIZE; offset++) {
        tlb[offset] = NX | (offset << PAGE_SHIFT) | VALID | AF;
    }
}
#endif

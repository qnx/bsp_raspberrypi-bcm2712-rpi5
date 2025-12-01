/*
 * $QNXLicenseC:
 * Copyright 2021, QNX Software Systems.
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
#include <aarch64/gic_v3.h>
#include <aarch64/gic.h>


/*
 ===============================================================================
 its_params

 'its_params' holds the initialization parameters used by gic_v3_its_initialize()
 for various ITS settings. A set of APIs exist which can be called prior to
 gic_v3_its_initialize() in order to modify/customize these parameters

 NOTE:

 Although more than one ITS instance is supported by the APIs, implementation
 wise, we currently support only 1 ITS. Many of the variables and the callouts
 will all need updates if more than one is supported but most of the other
 infrastructure is in place
 Because we have not seen a multi-ITS implementation, we don't even know what
 the end result would look like

*/
static arm_gic_its_params_t its_params[1] =
{
	[0 ... (NUM_ELTS(its_params) - 1)] = ARM_GIC_ITS_PARAMS_DFLT_INIT(),
};

/*
 ===============================================================================
 cmd_q_lock_base_vaddr

 These variables hold the base virtual and physical address for the ITS command
 queue locks page

*/
static void *cmd_q_lock_base_vaddr = NULL;
static paddr_t cmd_q_lock_base_paddr = NULL_PADDR;

/**
 * ============================================================================
 * Set the base address for an ITS instance
 *
 * @param its_num  ITS instance number
 * @param gits     GIC ITS base address
 *
 * Note that the base address for ITS instance 0 can, and usually is set from
 * the call to gic_v3_set_addr()
 *
 */
void gic_v3_its_set_paddr(const unsigned its_num, const paddr_t gits)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	its_params[its_num].gits_paddr_base = gits;
}

/**
 * ============================================================================
 * Initialize an ITS instance
 *
 * @param its_num  ITS instance number
 *
 */
void gic_v3_its_initialize(const unsigned its_num)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT(its_params[its_num].gits_paddr_base != 0);

	const paddr_t gits_paddr_base = its_params[its_num].gits_paddr_base;

	its_params[its_num].gits_vaddr_base = callout_io_map(0x10000, gits_paddr_base);

	if (debug_flag > 1)
	{
		kprintf("Setting up ITS based LPIs\nGITS @ %L, callout vaddr %L\n",
				gits_paddr_base, its_params[its_num].gits_vaddr_base);
	}
	/*
	 * Current assumptions and actions
	 * - Only 1 ITS in the system will be configured
	 * - A zeroed Device Table will be allocated to hold a predefined
	 *   number of Device IDs
	 * - A collection Table will be created with an entry for each PE
	 *   as required (may not require memory based collection table)
	 * - An ITS command queue will be created and initialized to empty
	 */
	/* set up ITS Device Table */
	/* make sure the ITS is disabled and quiescent */
	_Uint32t gits_ctlr = in32(gits_paddr_base + 0);
	gits_ctlr &= ~((_Uint32t)1 << 0);	/* disabled */
	out32(gits_paddr_base + 0, gits_ctlr);
	do
	{
		gits_ctlr = in32(gits_paddr_base + 0);
	} while ((gits_ctlr & ((_Uint32t)1 << 31)) == 0);

	unsigned dt_entry_size = 0;		/* in bytes */
	int gits_baser_dt_idx = -1;
	unsigned ct_entry_size = 0;		/* in bytes */
	int gits_baser_ct_idx = -1;

	const _Uint64t gits_typer = in64(gits_paddr_base + 8);
	const uint_t devid_bits = (((gits_typer >> 13) & 0x1Fu) + 1);
	ASSERT(devid_bits < 32);	/* protect the next calculation. Will need a larger type otherwise */
	const unsigned num_device_ids = (_Uint32t)1 << devid_bits;

	/*
	 * process each GITS_BASER register to obtain entry sizes for each table
	 * type. Currently we only look for the Device and Collection Tables
	 */
	for (unsigned i=0; i<8; i++)
	{
		const _Uint64t gits_baser = in64(gits_paddr_base + 0x100 + (i * 8));
		const _Uint64t table_type = (gits_baser >> 56) & 0x7;

		if (table_type == 1)
		{
			/* Device Table */
			ASSERT(dt_entry_size == 0);	/* can only be one of these table types */
			dt_entry_size = (unsigned)((gits_baser >> 48) & 0x1Fu) + 1;
			gits_baser_dt_idx = i;
			if (debug_flag > 1) kprintf("ITS Device Table at GITS_BASER%d\n", gits_baser_dt_idx);
		}
		else if (table_type == 4)
		{
			/* Collection Table */
			ASSERT(ct_entry_size == 0);	/* can only be one of these table types */
			ct_entry_size = (unsigned)((gits_baser >> 48) & 0x1Fu) + 1;
			gits_baser_ct_idx = i;
			if (debug_flag > 1) kprintf("ITS Collection Table at GITS_BASER%d\n", gits_baser_ct_idx);
		}
		if ((gits_baser_dt_idx > 0) && (gits_baser_ct_idx > 0)) break;
	}
	const unsigned dt_page_size = its_params[its_num].device_tbl.page_size;	/* 4K, 16K or 64K */
	const unsigned num_dt_pages = max(1, (num_device_ids * dt_entry_size) / dt_page_size);
	/* only 8 bits available for num pages. If this asserts, increase device_tbl.page_size
	 * by calling gic_v3_its_set_dt_page_size() before calling gic_v3_initialize() */
	ASSERT(num_dt_pages <= 0x100);

	_Uint64t dt = calloc_ram(num_dt_pages * dt_page_size, dt_page_size);

	/* make sure the address is valid */
	ASSERT(dt != NULL_PADDR_STARTUP);
	ASSERT((dt & (_Uint64t)0xFFFFFFFFF000) == dt);

	if (debug_flag > 1)
	{
		kprintf("Device Table @ %L, size 0x%x; num_device_ids: %d, entry_size: %d\n",
					dt, num_dt_pages * dt_page_size, num_device_ids, dt_entry_size);
	}

	_Uint64t dt_page_size_bits;
	if (dt_page_size == 4096) dt_page_size_bits = 0;
	else if (dt_page_size == 16384) dt_page_size_bits = 1;
	else if (dt_page_size == 65536) dt_page_size_bits = 2;
	else {ASSERT(0);}

	/* set page size bits */
	dt |= (dt_page_size_bits << 8);
	/* set the number of pages */
	dt |= (((_Uint64t)(num_dt_pages - 1) & 0xFFu) << 0);
	/*
	 * double check the size of the 'dt' again. The page_size (bits [9:8] of the
	 * BASER register) might be readonly and so even if the ASSERT(num_dt_pages <= 0x100)
	 * above doesn't fire, we may not be able to use a larger page_size. If not then
	 * the number of device IDs will need to be limited some other way
	 */
	{
		const unsigned page_size_bits = (dt >> 8) & (_Uint64t)0x3;
		const unsigned page_size = 0x1000 << (page_size_bits * 2);
		const unsigned num_pages = ((dt >> 0) & (_Uint64t)0xFF) + 1;
		/* make sure what we programmed is what we got */
		ASSERT((page_size * num_pages) == (num_dt_pages * dt_page_size));
	}

	/*
	 * Note that in some implementations, the shareability bits are non-modifiable
	 * and if they are 0b00 or 0b11, then cacheability should be set to
	 * non-cacheable so we check whether shareability is settable and adjust
	 * the parameters accordingly
	 */
	unsigned dt_share_bits = (in64(gits_paddr_base + 0x100 + (gits_baser_dt_idx * 8)) >> 10) & 0x3;
	const unsigned flipped_cur_share_bits = ~dt_share_bits & 0x3;
	out64(gits_paddr_base + 0x100 + (gits_baser_dt_idx * 8), (_Uint64t)flipped_cur_share_bits << 10);
	dt_share_bits = (in64(gits_paddr_base + 0x100 + (gits_baser_dt_idx * 8)) >> 10) & 0x3;
	const unsigned dt_share_bits_settable = (dt_share_bits == flipped_cur_share_bits);

	if (dt_share_bits_settable == 0)
	{
		if (debug_flag > 1)
		{
			kprintf("GITS_BASER<%d> shareability bits non-settable, use non-cached attributes\n",
					gits_baser_dt_idx);
		}
		its_params[its_num].device_tbl.params.shareability = arm_gic_tbl_shareability_NONE;
		its_params[its_num].device_tbl.params.cacheability.inner = arm_gic_tbl_cacheability_NONE;
		its_params[its_num].device_tbl.params.cacheability.outer = arm_gic_tbl_cacheability_OUTER_IS_NORMAL_INNER;
	}
	/* set shareability and cacheability */
	dt |= ((_Uint64t)its_params[its_num].device_tbl.params.shareability << 10);
	dt |= ((_Uint64t)its_params[its_num].device_tbl.params.cacheability.inner << 59);
	dt |= ((_Uint64t)its_params[its_num].device_tbl.params.cacheability.outer << 53);

	/* indirect (1 level, flat) table */
	dt &= ~((_Uint64t)1 << 62);
	/* mark it valid */
	dt |= ((_Uint64t)1 << 63);

	/* write the GITS_BASER for the Device Table */
	DCACHE_FLUSH_FOR_GIC_ITS(MAKE_1TO1_PTR(dt), num_dt_pages * dt_page_size);
	out64(gits_paddr_base + 0x100 + (gits_baser_dt_idx * 8), dt);
	/* the Page_Size field could be implementation defined read-only so assert
	 * if we do not get what we expect */
	ASSERT(((in64(gits_paddr_base + 0x100 + (gits_baser_dt_idx * 8)) >> 8) & 3) == dt_page_size_bits);
	if (debug_flag > 1)
	{
		kprintf("GITS_BASER<%d> for Device Table set to %L\n",
				gits_baser_dt_idx, in64(gits_paddr_base + 0x100 + (gits_baser_dt_idx * 8)));
	}

	/* handle the collection table */
	if (debug_flag > 1) kprintf("Memory based collection table %ssupported\n", (gits_baser_ct_idx < 0) ? "not " : " ");

	/* collect some information from the GITS_TYPER */
	const unsigned num_hw_collections = (gits_typer >> 24) & 0xFFu;
	its_params[its_num].num_collections = num_hw_collections;

	if (gits_baser_ct_idx >= 0)
	{
		const unsigned cil = (gits_typer >> 36) & 1;
		const unsigned cid_bits = (cil == 1) ? (((gits_typer >> 32) & 0xF) + 1) : 16;
		/*
		 * I suppose we can have more collection IDs than there are PE/CPUs in
		 * the system but currently we use a 1:1 mapping of ICID to CPU so
		 * there is no point in creating more than we need and also avoids sending
		 * MAPC commands to non-existent PEs
		 */
		const unsigned num_mem_collections = min((1u << cid_bits), lsp.syspage.p->num_cpu);

		its_params[its_num].collection_tbl.num_collections = num_mem_collections;
		its_params[its_num].num_collections += num_mem_collections;

		if (debug_flag > 1)
		{
			if (lsp.syspage.p->num_cpu < (1u << cid_bits))
			{
				kprintf("Memory collections supported by CIDbits is %u but limiting to num CPUs of %u\n",
						(1u << cid_bits), lsp.syspage.p->num_cpu);
			}
			kprintf("%u memory and %u hardware based collections supported, total %u\n",
					num_mem_collections, num_hw_collections, its_params[its_num].num_collections);
		}
		/*
		 * Allocate memory for the Collection Table
		 * Note, since memory collections are supported, the CCT bit (2) in
		 * conjunction with 'num_hw_collections' indicates whether collections
		 * are in hardware and memory, or just memory.
		 * I have only seen implementations which support one or the other and
		 * am not sure what to do in case of when they are in both so we'll
		 * assert if this occurs
		 */
		ASSERT(num_hw_collections == 0);	/* don't really care about CCT bit */

		const unsigned ct_page_size = its_params[its_num].collection_tbl.page_size;	/* 4K, 16K or 64K */
		const unsigned num_ct_pages = max(1, (num_mem_collections * ct_entry_size) / ct_page_size);
		/* only 8 bits available for num pages. If this asserts, increase collection_tbl.page_size
		 * by calling gic_v3_its_set_ct_page_size() before calling gic_v3_initialize() */
		ASSERT(num_ct_pages <= 0x100);

		_Uint64t ct = calloc_ram(num_ct_pages * ct_page_size, ct_page_size);

		/* make sure the address is valid */
		ASSERT(ct != NULL_PADDR_STARTUP);
		ASSERT((ct & (_Uint64t)0xFFFFFFFFF000) == ct);

		if (debug_flag > 1)
		{
			kprintf("Collection Table @ %L, size 0x%x; num_mem_collections: %d, entry_size: %d\n",
						ct, num_ct_pages * ct_page_size, num_mem_collections, ct_entry_size);
		}

		_Uint64t ct_page_size_bits;
		if (ct_page_size == 4096) ct_page_size_bits = 0;
		else if (ct_page_size == 16384) ct_page_size_bits = 1;
		else if (ct_page_size == 65536) ct_page_size_bits = 2;
		else {ASSERT(0);}

		/* set page size bits */
		ct |= (ct_page_size_bits << 8);
		/* set the number of pages */
		ct |= (((_Uint64t)(num_ct_pages - 1) & 0xFFu) << 0);
		/*
		 * double check the size of the 'ct' again. The page_size (bits [9:8] of the
		 * BASER register) might be readonly and so even if the ASSERT(num_ct_pages <= 0x100)
		 * above doesn't fire, we may not be able to use a larger page_size. If not then
		 * the number of collection IDs will need to be limited some other way
		 */
		{
			const unsigned page_size_bits = (ct >> 8) & (_Uint64t)0x3;
			const unsigned page_size = 0x1000 << (page_size_bits * 2);
			const unsigned num_pages = ((ct >> 0) & (_Uint64t)0xFF) + 1;
			/* make sure what we programmed is what we got */
			ASSERT((page_size * num_pages) == (num_ct_pages * ct_page_size));
		}
		/*
		 * Note that in some implementations, the shareability bits are non-modifiable
		 * and if they are 0b00 or 0b11, then cacheability should be set to
		 * non-cacheable so we check whether shareability is settable and adjust
		 * the parameters accordingly
		 */
		unsigned ct_share_bits = (in64(gits_paddr_base + 0x100 + (gits_baser_ct_idx * 8)) >> 10) & 0x3;
		const unsigned flipped_cur_share_bits = ~ct_share_bits & 0x3;
		out64(gits_paddr_base + 0x100 + (gits_baser_ct_idx * 8), (_Uint64t)flipped_cur_share_bits << 10);
		ct_share_bits = (in64(gits_paddr_base + 0x100 + (gits_baser_ct_idx * 8)) >> 10) & 0x3;
		const unsigned ct_share_bits_settable = (ct_share_bits == flipped_cur_share_bits);

		if (ct_share_bits_settable == 0)
		{
			if (debug_flag > 1)
			{
				kprintf("GITS_BASER<%d> shareability bits non-settable, use non-cached attributes\n",
						gits_baser_ct_idx);
			}
			its_params[its_num].collection_tbl.params.shareability = arm_gic_tbl_shareability_NONE;
			its_params[its_num].collection_tbl.params.cacheability.inner = arm_gic_tbl_cacheability_NONE;
			its_params[its_num].collection_tbl.params.cacheability.outer = arm_gic_tbl_cacheability_OUTER_IS_NORMAL_INNER;
		}
		/* set shareability and cacheability */
		ct |= ((_Uint64t)its_params[its_num].collection_tbl.params.shareability << 10);
		ct |= ((_Uint64t)its_params[its_num].collection_tbl.params.cacheability.inner << 59);
		ct |= ((_Uint64t)its_params[its_num].collection_tbl.params.cacheability.outer << 53);

		/* indirect (1 level, flat) table */
		ct &= ~((_Uint64t)1 << 62);
		/* mark it valid */
		ct |= ((_Uint64t)1 << 63);

		/* write the GITS_BASER for the Collection Table */
		DCACHE_FLUSH_FOR_GIC_ITS(MAKE_1TO1_PTR(ct), num_ct_pages * ct_page_size);
		out64(gits_paddr_base + 0x100 + (gits_baser_ct_idx * 8), ct);
		/* the Page_Size field could be implementation defined read-only so assert
		 * if we do not get what we expect */
		ASSERT(((in64(gits_paddr_base + 0x100 + (gits_baser_ct_idx * 8)) >> 8) & 3) == ct_page_size_bits);
		if (debug_flag > 1)
		{
			kprintf("GITS_BASER<%d> for Collection Table set to %L\n",
					gits_baser_ct_idx, in64(gits_paddr_base + 0x100 + (gits_baser_ct_idx * 8)));
		}
	}
	ASSERT(its_params[its_num].num_collections > 0);
	/*
	 * we need to issue the MAPC command to map ICIDs to CPUs. That will be
	 * done from gic_v3_gicc_init() when we also invalidate any entries in the
	 * redistributors
	 */

	/*
	 * Create the ITS command queue. It must be a multiple of 4KB and
	 * aligned to a 64KB boundary. One page provides for 128 entries
	 * The first time a command queue is allocated, we'll also allocate an
	 * extra page at the end of it for implementing the per ITS command queue
	 * spinlocks. Each lock will exist in its own 128 byte block ensuring no
	 * cache thrashing should there be more than one ITS instance
	*/
	const size_t extra = (cmd_q_lock_base_vaddr == NULL) ? 4096 : 0;
	const size_t its_cmd_q_size = its_params[its_num].cmd_q.num_4k_pages * 4096;
	_Uint64t its_cmd_q = calloc_ram(its_cmd_q_size + extra, 65536);

	/* make sure the address is valid */
	ASSERT(its_cmd_q != NULL_PADDR_STARTUP);
	ASSERT((its_cmd_q & (_Uint64t)0xFFFFFFFFF0000) == its_cmd_q);

	its_params[its_num].cmd_q.vaddr = (_Uintptrt)callout_memory_map(its_cmd_q_size, its_cmd_q, its_params[its_num].cmd_q.mmap_flags);

	if (debug_flag > 1)
	{
		kprintf("its_cmd_q @ %L, callout vaddr: %L, %u entries\n", its_cmd_q,
					its_params[its_num].cmd_q.vaddr, its_cmd_q_size / 32);
	}

	/* lock page must be cachable */
	if (cmd_q_lock_base_vaddr == NULL)
	{
		/*
		 * put the lock page for all ITS instances at the end of the command
		 * queue of the first instance
		 */
		ASSERT(cmd_q_lock_base_paddr == NULL_PADDR);
		cmd_q_lock_base_paddr = its_cmd_q + its_cmd_q_size;
		ASSERT(extra != 0);
		cmd_q_lock_base_vaddr = callout_memory_map(extra, cmd_q_lock_base_paddr, PROT_READ | PROT_WRITE);
		if (debug_flag > 1)
		{
			kprintf("cmd_q_lock page @ %L, vaddr: %L allocated with ITS %u\n",
					its_cmd_q + its_cmd_q_size, cmd_q_lock_base_vaddr, its_num);
		}
	}
	its_params[its_num].cmd_q.lock_vaddr = (void *)((_Uintptrt)cmd_q_lock_base_vaddr + (its_num * 128));
	arm_gic_its_lock_t *lock = (arm_gic_its_lock_t*)( cmd_q_lock_base_paddr + (its_num * 128) ) ;
	lock->version = ITS_LOCK_VERSION;

	if (debug_flag > 1)
	{
		kprintf("cmd_q_lock_base_vaddr @ %L, callout vaddr: %L for ITS %u\n",
				cmd_q_lock_base_vaddr, its_params[its_num].cmd_q.lock_vaddr, its_num);
	}

	_Uint64t gits_cbaser = in64(gits_paddr_base + 0x80);
	gits_cbaser &= ~((_Uint64t)0xFFFFFFFFFF000);	/* align check already done in above ASSERT */
	gits_cbaser |= its_cmd_q;
	/* set the number of pages */
	gits_cbaser &= ~((_Uint64t)0xFF << 0);
	gits_cbaser |= ((_Uint64t)(its_params[its_num].cmd_q.num_4k_pages - 1) << 0);	/* num pages field is less 1 */
	/*
	 * set the shareability as outer shareable.
	 * Note that in some implementations, these bits are non-modifiable
	 * and if they are 0b00 or 0b11, then cacheability should be set to
	 * non-cacheable
	 */
	gits_cbaser &= ~((_Uint64t)3 << 10);
	/* check whether shareability is settable */
	unsigned cmd_q_share_bits = (in64(gits_paddr_base + 0x80) >> 10) & 0x3;
	const unsigned flipped_cmd_q_share_bits = ~cmd_q_share_bits & 0x3;
	out64(gits_paddr_base + 0x80, (_Uint64t)flipped_cmd_q_share_bits << 10);
	cmd_q_share_bits = (in64(gits_paddr_base + 0x80) >> 10) & 0x3;
	const unsigned cmd_q_share_bits_settable = (cmd_q_share_bits == flipped_cmd_q_share_bits);

	if (cmd_q_share_bits_settable == 0)
	{
		if (debug_flag > 1) kprintf("CBASER shareability bits non-settable, use non-cached attributes\n");

		its_params[its_num].cmd_q.params.shareability = arm_gic_tbl_shareability_NONE;
		its_params[its_num].cmd_q.params.cacheability.inner = arm_gic_tbl_cacheability_NONE;
		its_params[its_num].cmd_q.params.cacheability.outer = arm_gic_tbl_cacheability_OUTER_IS_NORMAL_INNER;
	}
	gits_cbaser |= ((_Uint64t)its_params[its_num].cmd_q.params.shareability << 10);
	/* set the inner and outer cacheability the same and as read/write allocate, write-back */
	gits_cbaser &= ~((_Uint64t)7 << 59);
	gits_cbaser |= ((_Uint64t)its_params[its_num].cmd_q.params.cacheability.inner << 59);
	gits_cbaser &= ~((_Uint64t)7 << 53);
	gits_cbaser |= ((_Uint64t)its_params[its_num].cmd_q.params.cacheability.outer << 53);

	/* mark the table as valid */
	gits_cbaser |= ((_Uint64t)1 << 63);

	DCACHE_FLUSH_FOR_GIC_ITS(MAKE_1TO1_PTR(its_cmd_q), its_cmd_q_size + extra);

	/* before writing out the GITS_CBASER value, make sure queue looks empty
	 * Note that CREADR will be zeroed when CBASER is written */
	out64(gits_paddr_base + 0x88, 0);
	out64(gits_paddr_base + 0x80, gits_cbaser);

	/*
	 * Finally, create the HWINFO entry for the ITS. For each ITS instance, we
	 * add an entry with the name 'GIC_ITS' followed by 2 location tags, one
	 * for the paddr of the GIC ITS registers and one for the paddr of the
	 * command queue lock
	 */
	const unsigned hwi_its_off = hwidev_add("GIC_ITS", 0, HWI_NULL_OFF);
	if (hwi_its_off != HWI_NULL_OFF)
	{
		hwitag_add_location(hwi_its_off, gits_paddr_base, 0x20000, 0, 0);
		const paddr_t its_cmd_q_lock_addr = cmd_q_lock_base_paddr + (its_num * 128);
		hwitag_add_location(hwi_its_off, its_cmd_q_lock_addr, 4, 0, 0);
		if (debug_flag > 2)
		{
			kprintf("added HWI tag for GIC_ITS/lock = %L/%L\n",
						gits_paddr_base, its_cmd_q_lock_addr);
		}
#if 0	/* test for > 1 entry */
		{
			const unsigned hwi_its_off = hwidev_add("GIC_ITS", 0, HWI_NULL_OFF);
			if (hwi_its_off != HWI_NULL_OFF)
			{
				hwitag_add_location(hwi_its_off, gits_paddr_base + 0x4444, 0x20000, 0, 0);
				const paddr_t its_cmd_q_lock_addr = cmd_q_lock_base_paddr + ((its_num + 1) * 128);
				hwitag_add_location(hwi_its_off, its_cmd_q_lock_addr, 4, 0, 0);
				if (debug_flag > 0)
				{
					kprintf("added HWI tag for GIC_ITS/lock = %L/%L\n",
							gits_paddr_base + 0x4444, its_cmd_q_lock_addr);
				}
			}
		}
#endif
	}
}

/**
 * ============================================================================
 * Return the mapped base address for ITS instance <its_num>
 *
 * @param its_num  ITS instance number
 *
 */
_Uintptrt gic_v3_its_base_vaddr(const unsigned its_num)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	return (_Uintptrt)its_params[its_num].gits_vaddr_base;
}

/**
 * ============================================================================
 * Return the mapped command queue address for ITS instance <its_num>
 *
 * @param its_num  ITS instance number
 *
 */
_Uintptrt gic_v3_its_cmd_q_vaddr(const unsigned its_num)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	return (_Uintptrt)its_params[its_num].cmd_q.vaddr;
}

/**
 * ============================================================================
 * Return the mapped command queue lock address for ITS instance <its_num>
 *
 * @param its_num  ITS instance number
 *
 */
_Uintptrt gic_v3_its_cmd_q_lock_vaddr(const unsigned its_num)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	return (_Uintptrt)its_params[its_num].cmd_q.lock_vaddr;
}

/**
 * ============================================================================
 * Return the offset of the last command queue slot. This value is used to
 * patch the ITS LPI callouts
 *
 * @param its_num  ITS instance number
 *
 */
unsigned gic_v3_its_cmd_q_last_slot_offset(const unsigned its_num)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT(its_params[its_num].gits_paddr_base != 0);

	const _Uint64t gits_cbaser = in64(its_params[its_num].gits_paddr_base + 0x80);
	const unsigned num_4k_pages = ((gits_cbaser >> 0) & 0xFF) + 1;
	const unsigned num_slots = num_4k_pages * (4096 / 32);
	ASSERT(num_slots >= 128);	/* command queue must be at least one page */
	return (num_slots - 1) * 32;
}

/**
 * ============================================================================
 * Set desired parameters for the ITS Device Table cacheability and shareability
 *
 * @param its_num    ITS instance number
 * @param dt_params  pointer to the shareability and cacheability parameters
 *
 */
void gic_v3_its_set_dt_params(const unsigned its_num, const gic_tbl_params_t * const dt_params)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT(dt_params != NULL);
	ASSERT(dt_params->shareability >= arm_gic_tbl_shareability_first);
	ASSERT(dt_params->shareability <= arm_gic_tbl_shareability_last);
	ASSERT(dt_params->cacheability.inner >= arm_gic_tbl_cacheability_first);
	ASSERT(dt_params->cacheability.inner <= arm_gic_tbl_cacheability_last);
	ASSERT(dt_params->cacheability.outer >= arm_gic_tbl_cacheability_first);
	ASSERT(dt_params->cacheability.outer <= arm_gic_tbl_cacheability_last);

	its_params[its_num].device_tbl.params = *dt_params;
}

/**
 * ============================================================================
 * Set desired page size for the ITS Device Table
 *
 * @param its_num    ITS instance number
 * @param page_size  page size in bytes (4k, 16k or 64k, default is 4k)
 *
 */
void gic_v3_its_set_dt_page_size(const unsigned its_num, const unsigned page_size)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT((page_size == 4096) || (page_size == 16384) || (page_size == 65536));

	its_params[its_num].device_tbl.page_size = page_size;
}

/**
 * ============================================================================
 * Set desired parameters for the ITS Collection Table cacheability and
 * shareability
 *
 * @param its_num    ITS instance number
 * @param dt_params  pointer to the shareability and cacheability parameters
 *
 */
void gic_v3_its_set_ct_params(const unsigned its_num, const gic_tbl_params_t * const ct_params)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT(ct_params != NULL);
	ASSERT(ct_params->shareability >= arm_gic_tbl_shareability_first);
	ASSERT(ct_params->shareability <= arm_gic_tbl_shareability_last);
	ASSERT(ct_params->cacheability.inner >= arm_gic_tbl_cacheability_first);
	ASSERT(ct_params->cacheability.inner <= arm_gic_tbl_cacheability_last);
	ASSERT(ct_params->cacheability.outer >= arm_gic_tbl_cacheability_first);
	ASSERT(ct_params->cacheability.outer <= arm_gic_tbl_cacheability_last);

	its_params[its_num].collection_tbl.params = *ct_params;
}

/**
 * ============================================================================
 * Set desired page size for the ITS Collection Table
 *
 * @param its_num    ITS instance number
 * @param page_size  page size in bytes (4k, 16k or 64k, default is 4k)
 *
 */
void gic_v3_its_set_ct_page_size(const unsigned its_num, const unsigned page_size)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT((page_size == 4096) || (page_size == 16384) || (page_size == 65536));

	its_params[its_num].collection_tbl.page_size = page_size;
}

/**
 * ============================================================================
 * Set desired number of collection IDs for the ITS Collection Table
 *
 * @param its_num    ITS instance number
 * @param dt_params  number of collection IDs
 *
 */
void gic_v3_its_set_ct_num_ids(const unsigned its_num, const unsigned num)
{
	ASSERT(its_num < NUM_ELTS(its_params));

	its_params[its_num].collection_tbl.num_collections = num;
}

/**
 * ============================================================================
 * Set desired parameters for the ITS Command Queue cacheability and shareability
 *
 * @param its_num       ITS instance number
 * @param cmd_q_params  pointer to the shareability and cacheability parameters
 * @param mmap_flags    flags to control the callout mapping. Ignored if 0
 *
 */
void gic_v3_its_set_cmd_q_params(const unsigned its_num, const gic_tbl_params_t * const cmd_q_params, const int mmap_flags)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT(cmd_q_params != NULL);
	ASSERT(cmd_q_params->shareability >= arm_gic_tbl_shareability_first);
	ASSERT(cmd_q_params->shareability <= arm_gic_tbl_shareability_last);
	ASSERT(cmd_q_params->cacheability.inner >= arm_gic_tbl_cacheability_first);
	ASSERT(cmd_q_params->cacheability.inner <= arm_gic_tbl_cacheability_last);
	ASSERT(cmd_q_params->cacheability.outer >= arm_gic_tbl_cacheability_first);
	ASSERT(cmd_q_params->cacheability.outer <= arm_gic_tbl_cacheability_last);

	its_params[its_num].cmd_q.params = *cmd_q_params;
	if (mmap_flags != 0) its_params[its_num].cmd_q.mmap_flags = mmap_flags;
}

/**
 * ============================================================================
 * Set desired number of 4K pages for the ITS Command Queue
 *
 * @param its_num  ITS instance number
 * @param num      number of 4K pages
 *
 * The size of a command queue entry is 32 bytes so there is room for 128
 * commands in each 4K page
 *
 */
void gic_v3_its_set_cmd_q_num_pages(const unsigned its_num, const unsigned num)
{
	ASSERT(its_num < NUM_ELTS(its_params));

	its_params[its_num].cmd_q.num_4k_pages = num;
}

/**
 * ============================================================================
 * Create a Collection Table Entry for routing of interrupts to a specific CPU
 *
 * @param its_num            the ITS instance
 * @param cpu                the processor to create the entry for
 * @param gicr_base_for_cpu  the paddr of the re-distributor for <cpu>
 *
 * As each processor is brought online, an ICID to re-distributor mapping will
 * be created. The ICID will identify (using a 1:1 mapping) the logical CPU
 * (as known by the OS) and the mapping will be to the re-distributor to which
 * that logical CPU is associated, either by the physical address of the
 * re-distributor or by the processor number contained within the redistributor
 * TYPER register
 *
 * Note re synchronization
 * We don't take the ITS command queue lock for issuing of these commands
 * because in the context in which this function is called, only one processor
 * is executing and we wait for the ITS to process all of the commands before
 * returning
 *
 */
static unsigned next_cmd_q_slot(unsigned * const cur_slot, const unsigned num_slots, _Uintptrt * const q_addr, const _Uintptrt q_base)
{
	const unsigned next_slot = (*cur_slot + 1) % num_slots;
	if (next_slot == 0) *q_addr = q_base;
//kprintf("cur_slot: %u, next_slot: %u, q_addr: 0x%L\n", *cur_slot, next_slot, *q_addr);
	*cur_slot = next_slot;
	return next_slot;
}
void gic_v3_its_mapc(const unsigned its_num, const unsigned cpu, const unsigned gicr_base_for_cpu)
{
	ASSERT(its_num < NUM_ELTS(its_params));
	ASSERT(its_params[its_num].gits_paddr_base != 0);

	/* enable the ITS as required */
	const paddr_t gits_paddr_base = its_params[its_num].gits_paddr_base;
	_Uint32t gits_ctlr = in32(gits_paddr_base + 0);
	if ((gits_ctlr & 0x1) == 0)
	{
		if (debug_flag > 0) kprintf("Enabling ITS %u\n", its_num);
		/* wait for the ITS to be quiescent */
		while ((gits_ctlr & ((_Uint32t)1 << 31)) == 0)
		{
			gits_ctlr = in32(gits_paddr_base + 0);
		}
		gits_ctlr |= ((_Uint32t)1 << 0);	/* enabled */
		out32(gits_paddr_base + 0, gits_ctlr);
	}
	else if (debug_flag > 0) kprintf("ITS %u already Enabled\n", its_num);

	const unsigned its_cmd_q_num_slots = (its_params[its_num].cmd_q.num_4k_pages * 4096) / 32;
	const _Uintptrt tmp_its_cmd_q_base = (_Uintptrt)MAKE_1TO1_PTR(in64(gits_paddr_base + 0x80) & 0xFFFFFFFFFF000);
	if (debug_flag > 0) kprintf("ITS queue at %L, num slots: %u\n", (_Uint64t)tmp_its_cmd_q_base, its_cmd_q_num_slots);

/*
 * these 2 macros presuppose the existence of 'its_cmd_q_num_slots', 'tmp_its_cmd_q_base'
 * and 'gits_paddr_base' variables as above. They are a convenience for issuing multiple
 * commands to the queue and then executing them
*/
#define NEXT_CMD_Q_SLOT(_cur_slot_, _q_addr_) \
		next_cmd_q_slot(&(_cur_slot_), its_cmd_q_num_slots, (_Uintptrt *)(&(_q_addr_)), tmp_its_cmd_q_base)

#define EXECUTE_CMD_Q(_cur_slot_, _q_addr_) \
		do { \
			const unsigned next_slot = NEXT_CMD_Q_SLOT((_cur_slot_), (_q_addr_)); \
			if (debug_flag > 0) kprintf("update CWRITER to 0x%x\n", next_slot * 32); \
			/* set the Retry bit (bit 0) in case the ITS is stalled. No harm if its not */ \
			out64(gits_paddr_base + 0x88, (next_slot * 32) | 1); \
		} while(0)


	const _Uint64t gits_typer = in64(gits_paddr_base + 8);
	const unsigned cil_bit = (gits_typer >> 36) & 1;
	const unsigned icid_bits = (cil_bit == 1) ? (((gits_typer >> 32) & 0xFu) + 1) : 16;
	const _Uint64t icid_mask = ((_Uint64t)1 << icid_bits) - 1;
	const unsigned pta_bit = (gits_typer >> 19) & 1;
	const _Uint64t RDbase_mask = (((_Uint64t)1 << 52) - 1) ^ 0xFFFFu;	/* 64KB aligned */
	const unsigned processor_num = (in64(gicr_base_for_cpu + ARM_GICR_TYPER) >> 8) & 0xFFFF;
	const _Uint64t RDbase = (pta_bit == 1) ? gicr_base_for_cpu : (processor_num << max(16, icid_bits));
	const unsigned icid = cpu;	/* 1:1 relationship */

/*
kprintf("CPU %u, Processor # %u, GICR base: %L, GITS_typer: %L, icid_mask: %L, icid: %u, RDbase_mask: %L, RDbase: %L\n",
		cpu, processor_num, gicr_base_for_cpu, gits_typer, icid_mask, icid, RDbase_mask, RDbase);
*/
	/*
	 * Initialize the 2 required variables (cur_slot and tmp_its_cmd_q) for
	 * issuing the MAPC, SYNC and INVALL commands based on current ITS settings
	 */
	unsigned cur_slot = in64(gits_paddr_base + 0x88) / 32;
	ASSERT(cur_slot < its_cmd_q_num_slots);
	_Uint64t *tmp_its_cmd_q = (_Uint64t *)(tmp_its_cmd_q_base + (cur_slot * 32));
	const _Uint64t *addr_for_flush = MAKE_1TO1_PTR(tmp_its_cmd_q);
	/*
	 * now we need to issue the MAPC command to map the ICID to RDbase
	 * If GITS_TYPER[19] is 0, the RDbase is the Processor number otherwise as
	 * provided in the GICR_TYPER register otherwise its the physical address of
	 * the distributor for the processor. Regardless we will always create a 1:1
	 * mapping between the ICID and the CPU number. That is, ICID0 --> CPU0,
	 * ICID1 --> CPU1, etc.
	 */
	if (debug_flag > 0) kprintf("Issue MAPC/SYNC/INVALL commands for ICID %u\n", icid);

	*tmp_its_cmd_q++ = 0x9;	/* MAPC */
	*tmp_its_cmd_q++ = 0;
	*tmp_its_cmd_q++ = (icid & icid_mask) | (RDbase & RDbase_mask) | ((_Uint64t)1 << 63);
	*tmp_its_cmd_q++ = 0;

	NEXT_CMD_Q_SLOT(cur_slot, tmp_its_cmd_q);

	*tmp_its_cmd_q++ = 0x5;	/* SYNC */
	*tmp_its_cmd_q++ = 0;
	*tmp_its_cmd_q++ = (RDbase & RDbase_mask);
	*tmp_its_cmd_q++ = 0;

	NEXT_CMD_Q_SLOT(cur_slot, tmp_its_cmd_q);

	*tmp_its_cmd_q++ = 0xD;	/* INVALL */
	*tmp_its_cmd_q++ = 0;
	*tmp_its_cmd_q++ = icid & icid_mask;
	*tmp_its_cmd_q++ = 0;

	DCACHE_FLUSH_FOR_GIC_ITS(addr_for_flush, (uintptr_t)tmp_its_cmd_q - (uintptr_t)addr_for_flush);
	/* execute the commands */
	EXECUTE_CMD_Q(cur_slot, tmp_its_cmd_q);

	if (debug_flag > 0) kprintf("Waiting for all commands to be processed");

	_Uint8t loop = 0;
	while(++loop != 0)	/* give it (1u << (8 * sizeof(loop))) -1 tries */
	{
		const _Uint64t creader = in64(gits_paddr_base + 0x90);
		const _Uint64t cwriter = in64(gits_paddr_base + 0x88);
		if (debug_flag > 1)
		{
			kprintf("%s%u GITS_CWRITER/CREADER at offsets %d/%d",
						(loop == 1) ? "\n\t" : "\r\t", loop, cwriter, creader);
		}
		if (creader == cwriter) break;
		else if ((creader & 1) != 0) out64(gits_paddr_base + 0x88, cwriter | 1);
	}
	if (debug_flag > 0)
	{
		if (loop == 0) kprintf(" ... Failed in %u tries\n", (1u << (8 * sizeof(loop))) - 1);
		else kprintf(" ... Done in %u tries\n", loop);
	}
	ASSERT(loop != 0);
}

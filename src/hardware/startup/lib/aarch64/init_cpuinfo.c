/*
 * Copyright 2014,2025, BlackBerry Limited.
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


#define	CPUID_IS_ARM(x)		((((x) >> 24) & 0xff) == 0x41)
#define	ARM_VARIANT(x)		(((x) >> 20) & 0xf)
#define	ARM_REVISION(x)		((x) & 0xf)

#define	AA64PFR0_SVE(x)		((((x) >> 32) & 0xfUL) == 0x1)
#define	AA64PFR0_SIMD(x)	((((x) >> 20) & 0xfUL) != 0xfUL)
#define	AA64PFR0_FP(x)		((((x) >> 16) & 0xfUL) != 0xfUL)
#define AA64ISAR0_LSE(x)	((((x) >> 20) & 0xfUL) == 0x002)
#define AA64ISAR0_AES(x)	((((x) >> 4) & 0xfUL) == 0x1)
#define AA64ISAR0_PMULL(x)	((((x) >> 4) & 0xfUL) == 0x2)
#define AA64ISAR0_SHA1(x)	((((x) >> 8) & 0xfUL) == 0x1)
#define AA64ISAR0_SHA256(x)	((((x) >> 12) & 0xfUL) == 0x1)
#define AA64ISAR0_SHA512(x)	((((x) >> 12) & 0xfUL) == 0x2)
#define AA64ISAR0_SHA3(x)	((((x) >> 32) & 0xfUL) == 0x1)
#define AA64ISAR0_RNDR(x)	((((x) >> 60) & 0xfUL) == 0x1)

#ifndef AARCH64_CPU_FLAG_SVE
#define AARCH64_CPU_FLAG_SVE	0x10000000UL
#endif

void init_one_cpuinfo(unsigned cpunum);

/*
 * Read cache size id register for this level and cache type
 */
static inline void
cache_sizes(unsigned csel, unsigned *nsets, unsigned *assoc, unsigned *lsize)
{
	aa64_sr_wr32(csselr_el1, csel);
	isb();
	unsigned const ccsidr = aa64_sr_rd32(ccsidr_el1);
	*nsets = ((ccsidr >> 13) & 0x7fff) + 1;
	*assoc = ((ccsidr >> 3) & 0x3ff) + 1;
	*lsize = 1 << ((ccsidr & 7) + 4);
}

static void
init_cache(struct cpuinfo_entry *cpu, unsigned cpunum)
{
	unsigned	clidr;
	unsigned	nsets;
	unsigned	lsize;
	unsigned	assoc;

	/*
	 * Cache maintenance instructions operate on all levels so we only
	 * add cacheattr information for the L1 cache(s).
	 */
	clidr = aa64_sr_rd32(clidr_el1);
	if (clidr & 4) {
		cache_sizes(0, &nsets, &assoc, &lsize);
		cpu->data_cache = add_cache_ways(cpu->data_cache,
									CACHE_FLAG_UNIFIED | cache_snooped,
									lsize, nsets * assoc, assoc,
									&cache_armv8_dcache);
	} else {
		if (clidr & 1) {
			cache_sizes(1, &nsets, &assoc, &lsize);
			cpu->ins_cache = add_cache_ways(cpu->ins_cache,
										CACHE_FLAG_INSTR,
										lsize, nsets * assoc, assoc,
										&cache_armv8_icache);
		}
		if (clidr & 2) {
			cache_sizes(0, &nsets, &assoc, &lsize);
			cpu->data_cache = add_cache_ways(cpu->data_cache,
										CACHE_FLAG_DATA | cache_snooped,
										lsize, nsets * assoc, assoc,
										&cache_armv8_dcache);
		}
	}

	if (debug_flag) {
		int			i;
		unsigned	ctype;
		unsigned	ctr;
		static const char *const l1ip[] = {
			"unknown", "AIVIVT", "VIPT", "PIPT"
		};

		ctr = aa64_sr_rd32(dczid_el0);
		if ((ctr & (1u << 4)) == 0) {
			kprintf("cpu%d: DCZID_EL0=0x%x (%d bytes)\n",
					cpunum, ctr, 1 << ((ctr & 0xf) + 2));
		}

		ctr = aa64_sr_rd32(ctr_el0);
		kprintf("cpu%d: CWG=%d ERG=%d Dminline=%d Iminline=%d %s\n",
			cpunum,
			(ctr >> 24) & 0xf,
			(ctr >> 20) & 0xf,
			(ctr >> 16) & 0xf,
			ctr & 0xf,
			l1ip[(ctr >> 14) & 3]);

		kprintf("cpu%d: CLIDR=%w LoUU=%d LoC=%d LoUIS=%d\n",
			cpunum,
			clidr,
			(clidr >> 27) & 7,
			(clidr >> 24) & 7,
			(clidr >> 21) & 7);

		for (i = 0, ctype = clidr; i < 7; i++, ctype >>= 3) {
			unsigned	t = (ctype & 7);

			if (t == 0 || t > 4) {
				/*
				 * Cache not present or has a bogus value
				 */
				break;
			}

			if (t & 4) {
				cache_sizes((i << 1), &nsets, &assoc, &lsize);
				kprintf("cpu%d: L%d Unified %dK linesz=%d set/way=%d/%d\n",
					cpunum, i+1,
					lsize * nsets * assoc / 1024,
					lsize, nsets, assoc);
			} else {
				/*
				 * Might have separate I/D caches at each level
				 */
				if (t & 1) {
					cache_sizes((i << 1) | 1, &nsets, &assoc, &lsize);
					kprintf("cpu%d: L%d Icache %dK linesz=%d set/way=%d/%d\n",
						cpunum, i+1,
						lsize * nsets * assoc / 1024,
						lsize, nsets, assoc);
				}
				if (t & 2) {
					cache_sizes((i << 1), &nsets, &assoc, &lsize);
					kprintf("cpu%d: L%d Dcache %dK linesz=%d set/way=%d/%d\n",
						cpunum, i+1,
						lsize * nsets * assoc / 1024,
						lsize, nsets, assoc);
				}
			}
		}
	}
}

void
init_one_cpuinfo(unsigned cpunum)
{
	struct cpuinfo_entry	*cpu;
	unsigned				cpuid;
	unsigned long			mpidr;
	const char				*name;
	unsigned long			pfr;
	unsigned long			isar0;


	cpu = &lsp.cpuinfo.p[cpunum];

	/*
	 * Get the CPUID and processor name
	 */
	cpuid = aa64_sr_rd32(midr_el1);
	cpu->cpu = cpuid;
	struct aarch64_cpuid *const id = aarch64_cpuid_find();
	name = id ? id->name : "Unknown";
	cpu->name = add_string(name);

	mpidr = aa64_sr_rd64(mpidr_el1);
	cpu->smp_hwcoreid = mpidr;

	if (cpu_freq != 0) {
		cpu->speed = cpu_freq / 1000000;
	} else {
		cpu->speed = aarch64_cpuspeed();
	}

	if (debug_flag) {
		kprintf("cpu%d: MPIDR=%v\n", cpunum, mpidr);
		if (CPUID_IS_ARM(cpuid)) {
			kprintf("cpu%d: MIDR=%w %s r%dp%d\n",
					cpunum, cpuid, name,
					ARM_VARIANT(cpuid), ARM_REVISION(cpuid));
		} else {
			kprintf("cpu%d: MIDR=%w %s\n", cpunum, cpuid, name);
		}
	}

	cpu->flags = 0;
	if (shdr->flags1 & STARTUP_HDR_FLAGS1_VIRTUAL) {
		cpu->flags |= CPU_FLAG_MMU;
	}
	if (lsp.syspage.p->num_cpu > 1) {
		cpu->flags |= AARCH64_CPU_FLAG_SMP;
	}

	/*
	 * Detect if we have FP/SIMD.
	 * Both share the same register file so set CPU_FLAG_FPU.
	 */
	pfr = aa64_sr_rd64(id_aa64pfr0_el1);
	if (AA64PFR0_SIMD(pfr) || AA64PFR0_FP(pfr)) {
		cpu->flags |= CPU_FLAG_FPU;
		if (AA64PFR0_SIMD(pfr)) {
			cpu->flags |= AARCH64_CPU_FLAG_SIMD;
		}
	}

	/*
	 * Detect if we have SVE and the user enables it.
	 */
	if(sve_requested != 0) {
		pfr = aa64_sr_rd64(id_aa64pfr0_el1);
		if (AA64PFR0_SVE(pfr)) {
			cpu->flags |= AARCH64_CPU_FLAG_SVE;
		} else {
			kprintf("WARN: SVE requested, but not supported by CPU\n");
		}
	}

	/*
	 * Enable EL0 access to CNTVCT_EL0 for ClockCycles()
	 */
	aa64_sr_wr32(cntkctl_el1, 1 << 1);

	/*
	 * Set legacy ARMv7 flags for Aarch32 binary compatibility
	 */
	if (cpu->flags & AARCH64_CPU_FLAG_SMP) {
		cpu->flags |= AARCH32_CPU_FLAG_V7_MP;
	}
	if (cpu->flags & CPU_FLAG_FPU) {
		cpu->flags |= AARCH32_CPU_FLAG_VFP_D32;
	}
	cpu->flags |= AARCH32_CPU_FLAG_V6|AARCH32_CPU_FLAG_V7|AARCH32_CPU_FLAG_IDIV;

	/*
	 * Enable Spectre V2 fix
	 */
	if (spectre_v2_active > 0) {
		if ((id != NULL) && ((id->midr == cpuid_a57.midr) || (id->midr == cpuid_a72.midr))) {
			cpu->flags |= AARCH64_CPU_SPECTRE_V2_MMU;
		} else {
			cpu->flags |= AARCH64_CPU_SPECTRE_V2_FIX;
		}
	}

	/*
	 * Enable SSBS support
	 */
	if (ssbs_active > 0) {
		cpu->flags |= AARCH64_CPU_SSBS;
	}


	/* Detect if we have LSE atomic instructions */
	isar0 = aa64_sr_rd64(id_aa64isar0_el1);
	if (AA64ISAR0_LSE(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_LSE;
	}
	if (AA64ISAR0_AES(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_AES;
	}
	if (AA64ISAR0_PMULL(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_PMULL | AARCH64_CPU_FLAG_AES;
	}
	if (AA64ISAR0_SHA1(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_SHA1;
	}
	if (AA64ISAR0_SHA256(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_SHA256;
	}
	if (AA64ISAR0_SHA512(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_SHA512 | AARCH64_CPU_FLAG_SHA256;
	}
	if (AA64ISAR0_SHA3(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_SHA3;
	}
	if (AA64ISAR0_RNDR(isar0)) {
		cpu->flags |= AARCH64_CPU_FLAG_RNDR;
	}
#ifdef AARCH64_CPU_PAUTH
	/* Detect if we have pointer authentication instructions */
	if(pauth_active != 0) {
		unsigned long isar1 = aa64_sr_rd64(id_aa64isar1_el1) & (0xfful << 4);
		if (isar1 != 0) {
			cpu->flags |= AARCH64_CPU_PAUTH;
		}
	}
#endif

	unsigned ctr = aa64_sr_rd32(ctr_el0);
	if ((ctr & (1U << 29)) != 0U) {
		cpu->flags |= AARCH64_CPU_FLAG_ICACHE_COHERENT;
	}

	/*
	 * Set up cache callouts
	 */
	init_cache(cpu, cpunum);

	/*
	 * Set up GIC CPU interface for this cpu
	 */
	if (gic_cpu_init) {
		gic_cpu_init(cpunum);
	}

	/*
     * Perform any cpu-specific cpu information setup
     */
	if (id && id->cpuinfo) {
		id->cpuinfo(cpunum, cpu);
	}

	/*
	 * zero CPACR_EL1, which will disable exception for EVT, FPU, and SVE.
	 */
	aa64_sr_wr32(cpacr_el1, 0);

    /* Dump ID registers */
    uint64_t * const dump_space =
        &lsp.cpu.aarch64_idreg_dump.p->idreg[cpunum*AARCH64_IDDUMP_ENTRIES];
    asm (
        "mrs    x0, S3_0_C0_C1_0\n"
        "mrs    x1, S3_0_C0_C1_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C1_2\n"
        "mrs    x1, S3_0_C0_C1_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C1_4\n"
        "mrs    x1, S3_0_C0_C1_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C1_6\n"
        "mrs    x1, S3_0_C0_C1_7\n"
        "stp    x0, x1, [%[dump]], #16\n"

        "mrs    x0, S3_0_C0_C2_0\n"
        "mrs    x1, S3_0_C0_C2_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C2_2\n"
        "mrs    x1, S3_0_C0_C2_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C2_4\n"
        "mrs    x1, S3_0_C0_C2_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C2_6\n"
        "mrs    x1, S3_0_C0_C2_7\n"
        "stp    x0, x1, [%[dump]], #16\n"

        "mrs    x0, S3_0_C0_C3_0\n"
        "mrs    x1, S3_0_C0_C3_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C3_2\n"
        "mrs    x1, S3_0_C0_C3_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C3_4\n"
        "mrs    x1, S3_0_C0_C3_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C3_6\n"
        "mrs    x1, S3_0_C0_C3_7\n"
        "stp    x0, x1, [%[dump]], #16\n"

        "mrs    x0, S3_0_C0_C4_0\n"
        "mrs    x1, S3_0_C0_C4_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C4_2\n"
        "mrs    x1, S3_0_C0_C4_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C4_4\n"
        "mrs    x1, S3_0_C0_C4_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C4_6\n"
        "mrs    x1, S3_0_C0_C4_7\n"
        "stp    x0, x1, [%[dump]], #16\n"

        "mrs    x0, S3_0_C0_C5_0\n"
        "mrs    x1, S3_0_C0_C5_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C5_2\n"
        "mrs    x1, S3_0_C0_C5_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C5_4\n"
        "mrs    x1, S3_0_C0_C5_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C5_6\n"
        "mrs    x1, S3_0_C0_C5_7\n"
        "stp    x0, x1, [%[dump]], #16\n"

        "mrs    x0, S3_0_C0_C6_0\n"
        "mrs    x1, S3_0_C0_C6_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C6_2\n"
        "mrs    x1, S3_0_C0_C6_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C6_4\n"
        "mrs    x1, S3_0_C0_C6_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C6_6\n"
        "mrs    x1, S3_0_C0_C6_7\n"
        "stp    x0, x1, [%[dump]], #16\n"

        "mrs    x0, S3_0_C0_C7_0\n"
        "mrs    x1, S3_0_C0_C7_1\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C7_2\n"
        "mrs    x1, S3_0_C0_C7_3\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C7_4\n"
        "mrs    x1, S3_0_C0_C7_5\n"
        "stp    x0, x1, [%[dump]], #16\n"
        "mrs    x0, S3_0_C0_C7_6\n"
        "mrs    x1, S3_0_C0_C7_7\n"
    : "=m" (* (uint64_t (*)[]) dump_space) : [dump] "r" (dump_space) : "x0", "x1");
}

void
init_cpuinfo()
{
	struct cpuinfo_entry	*cpu;
	unsigned				num;
	unsigned				i;

	num = lsp.syspage.p->num_cpu;

	cpu = set_syspage_section(&lsp.cpuinfo, sizeof(*lsp.cpuinfo.p) * num);
	for (i = 0; i < num; i++) {
		cpu[i].ins_cache  = system_icache_idx;
		cpu[i].data_cache = system_dcache_idx;
	}

    // 6 bits worth of ID reg space, minus C0, so 56 entries
    set_syspage_section(&lsp.cpu.aarch64_idreg_dump,
        (unsigned)(num * sizeof(_Uint64t) * AARCH64_IDDUMP_ENTRIES));

	init_one_cpuinfo(0);
}

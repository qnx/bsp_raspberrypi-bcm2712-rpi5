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
 * $QNXLicenseC:
 * Copyright 2014, QNX Software Systems.
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

//
// NOTE: This file is shared between pidin and the startup library.
// Ordinarily, we'd put it in a library that both code references, but
// that would complicate building startups in a manner that we don't
// want. Instead, a copy of this file is placed in both locations
// and a distinct print_sysp.h file in both directories provide the
// 'glue' to hook up this code with the particularities of the environment.
// If you make any changes to this file, you should also copy the new
// version to the other location (after first checking that no-one's
// modified the other copy and forgotten to update this location).
// Normally this won't be a problem since the file will only change in
// response to system page layout changes and the copy in startup will
// be modified first and then copied to the pidin tree.
//
// This file is in:
//	utils/p/pidin/print_sysp.c <=> hardware/startup/lib/print_sysp.c




#define NUM_ELTS(__array)	(sizeof(__array)/sizeof(__array[0]))
#define MAX_BUFF_SIZE		80U

#include "print_sysp.h"

static void
print_typed_strings(void) {
	struct typed_strings_entry	*string = _SYSPAGE_ENTRY(PSP_SYSPAGE, typed_strings);
	void		*type_ptr;
	unsigned	type;
	unsigned	i;

	i = 0;
	for( ;; ) {
		type_ptr = &string->data[i];
		type = (*(uint32_t *)type_ptr);
		if(type == _CS_NONE) break;
		i += sizeof(uint32_t);
		kprintf("  off:%d type:%d string:'%s'\n", i-sizeof(uint32_t), type, &string->data[i]);
		i += strlen(&string->data[i]) + 1;
		i = (i + (sizeof(uint32_t)-1)) & ~(sizeof(uint32_t)-1);
	}
}


static void
print_strings(void) {
	char		*p = _SYSPAGE_ENTRY(PSP_SYSPAGE,strings)->data;
	char		*start = p;
	unsigned	off;
	unsigned	len;
	char		buff[MAX_BUFF_SIZE];

	kprintf(" ");
	off = 1;
	while(*p != '\0') {
		PSP_SNPRINTF(buff, MAX_BUFF_SIZE, " [%d]'%s'", (unsigned)(p - start), p);
		buff[MAX_BUFF_SIZE - 1] = '\0';
		len = (unsigned)strlen(buff);
		if((off + len) >= MAX_BUFF_SIZE) {
			kprintf("\n ");
			off = 1;
		}
		kprintf("%s", buff);
		off += len;
		p += strlen(p) + 1;
	}
	kprintf("\n");
}


static void
print_system_private(void) {
	struct system_private_entry	*private = _SYSPAGE_ENTRY(PSP_SYSPAGE,system_private);

	kprintf("  syspage ptr user:%v kernel:%v\n", private->user_syspageptr, private->kern_syspageptr);
	kprintf("  cpupage ptr user:%v kernel:%v spacing:%d\n", private->user_cpupageptr, private->kern_cpupageptr, private->cpupage_spacing);
	kprintf("  kdebug info:%v callback:%v num_ready:%u\n", private->kdebug_info, private->kdebug_call, private->num_ready);
	kprintf("  pagesize:%x flags:%x, kdinfo:%x\n",
			private->pagesize,
			private->private_flags,
			private->kdump_info);
}


static char *
get_string(unsigned off) {
	return &_SYSPAGE_ENTRY(PSP_SYSPAGE,strings)->data[off];
}


#if !defined(PSP_STARTUP)
static void
asinfo_string_name(struct asinfo_entry *base, struct asinfo_entry *curr) {
	struct list {
		struct list	*next;
		struct asinfo_entry	*as;
	}		*chain, *new;

	chain = NULL;
	for( ;; ) {
		uint16_t   own;
		new = alloca(sizeof(*chain));
		new->next = chain;
		new->as = curr;
		chain = new;
		own = curr->owner;
		if(own == AS_NULL_OFF) break;
		curr = (struct asinfo_entry *)((uint8_t *)base + own);
	}
	while(chain != NULL) {
		kprintf("/%s", get_string(chain->as->name));
		chain = chain->next;
	}
}
#endif

static void
print_an_asinfo(const syspage_entry_info *const info, unsigned const elsize) {
	struct asinfo_entry 	*as = (void *)((uintptr_t)PSP_SYSPAGE + info->entry_off);
	int						num;
	int						i;

	num = info->entry_size / elsize;
	for(i = 0; i < num; ++i) {
		kprintf("  %w) %L-%L o:%w ",
				i*sizeof(*as),
				as->start,
				as->end,
				as->owner);
#if defined(PSP_STARTUP)
		kprintf("%d\n", as->name);
#else
		asinfo_string_name((void *)((uintptr_t)PSP_SYSPAGE + info->entry_off), as);
		kprintf("\n");
#endif
		as = SYSPAGE_ARRAY_ADJ_OFFSET(asinfo, as, elsize);
	}
}

static void
print_asinfo(void) {
	print_an_asinfo((syspage_entry_info *)&PSP_SYSPAGE->asinfo, _SYSPAGE_ELEMENT_SIZE(PSP_SYSPAGE, asinfo));
}


static void
print_hwinfo(void) {
	hwi_tag				*tag = (hwi_tag *)_SYSPAGE_ENTRY(PSP_SYSPAGE,hwinfo);
	void				*base;
	void				*next;
	char				*name;

	while(tag->prefix.size != 0) {
		next = (hwi_tag *)((uint32_t *)tag +
			tag->prefix.size);
		base = (void *)(&tag->prefix + 1);
		name = get_string(tag->prefix.name);
		kprintf("  %d) size:%d tag:%d(%s)",
				hwi_tag2off(tag),
				tag->prefix.size,
				tag->prefix.name, name);
		if(*name >= 'A' && *name <= 'Z') {
			base = (void *) (&tag->item + 1);
			kprintf(" isize:%d, iname:%d(%s), owner:%d, kids:%d",
					tag->item.itemsize,
					tag->item.itemname,
					get_string(tag->item.itemname),
					tag->item.owner, tag->item.kids);
		}
		if(base != next) {
			kprintf("\n    ");
			while(base < next) {
				uint8_t		*p = base;

				kprintf(" %b", *p);
				base = p + 1;
			}
		}
		kprintf("\n");
		tag = next;
	}
}


static void
print_qtime(void) {
	struct qtime_entry *qtime = _SYSPAGE_ENTRY(PSP_SYSPAGE,qtime);

	kprintf("  boot:%x CPS:%L period/scale:%d/-%d intr:%d\n",
		qtime->boot_time,
		qtime->cycles_per_sec,
		qtime->timer_period,
		-(int)qtime->timer_scale,
		(int)qtime->intr
		);
	kprintf("  flags:%x epoch:%d rr_mul:%d adj count/inc:%d/%d\n",
		qtime->flags,
		qtime->epoch,
		qtime->rr_interval_mul,
		qtime->adjust.tick_count,
		qtime->adjust.tick_nsec_inc);

#if !defined(PSP_STARTUP)
	kprintf("  nsec_inc:%x\n",
			qtime->nsec_inc);
#endif
	kprintf("  nsec_tod_adj:%L load_max:%L\n", qtime->nsec_tod_adjust, qtime->timer_load_max);
}


static void
print_an_cpuinfo(const syspage_entry_info *const info, unsigned const elsize) {
	struct cpuinfo_entry *cpu = (void *)((uintptr_t)PSP_SYSPAGE + info->entry_off);
	unsigned i;

	for(i = 0; i < (unsigned)PSP_SYSPAGE->num_cpu; ++i) {
		kprintf("  %d) cpu:%x flg:%x spd:%d hwid:%x cache i/d:%d/%d name:%d\n",
			i,
			cpu->cpu,
			cpu->flags,
			cpu->speed,
			cpu->smp_hwcoreid,
			cpu->ins_cache,
			cpu->data_cache,
			cpu->name);
		cpu = SYSPAGE_ARRAY_ADJ_OFFSET(cpuinfo, cpu, elsize);
	}
}

static void
print_cpuinfo(void) {
	print_an_cpuinfo((syspage_entry_info *)&PSP_SYSPAGE->cpuinfo, _SYSPAGE_ELEMENT_SIZE(PSP_SYSPAGE, cpuinfo));
}

static void
print_an_cacheattr(const syspage_entry_info *const info, unsigned const elsize) {
	struct cacheattr_entry *cache = (void *)((uintptr_t)PSP_SYSPAGE + info->entry_off);
	int						num;
	int						i;

	num = _SYSPAGE_ENTRY_SIZE(PSP_SYSPAGE,cacheattr) / elsize;
	for(i = 0; i < num; ++i ) {
		kprintf("  %d) flags:%b size:%w #lines:%w ways:%w control:%v next:%d\n",
			i,
			cache->flags,
			cache->line_size,
			cache->num_lines,
			cache->ways,
			cache->control,
			cache->next);
		cache = SYSPAGE_ARRAY_ADJ_OFFSET(cacheattr, cache, elsize);
	}
}

static void
print_cacheattr(void) {
	print_an_cacheattr((syspage_entry_info *)&PSP_SYSPAGE->cacheattr, _SYSPAGE_ELEMENT_SIZE(PSP_SYSPAGE, cacheattr));
}


static void
print_callout(void) {
	struct callout_entry	*call = _SYSPAGE_ENTRY(PSP_SYSPAGE,callout);
	unsigned				i;

	kprintf("  reboot:%v watchdog:%v\n",
		call->reboot,
		call->debug_watchdog);
	for(i = 0; i < 2; ++i) {
    struct debug_callout	*dbg = &call->debug[i];

    kprintf("  %d) display:%v poll:%v break:%v\n", i,
        dbg->display_char,
        dbg->poll_key,
        dbg->break_detect);
	}
}

static void
print_intrgen(char *name, struct __intrgen_data *gen) {
	kprintf("     %s => flags:%w, size:%w, rtn:%v\n",
		name,
		gen->genflags,
		gen->size,
		gen->rtn);
}

static void
print_intrinfo(void) {
 	struct intrinfo_entry *intr = _SYSPAGE_ENTRY(PSP_SYSPAGE,intrinfo);
	int						num;
	int						i;

	const unsigned elsize = _SYSPAGE_ELEMENT_SIZE(PSP_SYSPAGE, intrinfo);
	num = _SYSPAGE_ENTRY_SIZE(PSP_SYSPAGE, intrinfo) / elsize;
	for( i = 0; i < num; ++i ) {
		kprintf("  %d) vector_base:%x, #vectors:%d, cascade_vector:%x\n",
				i,
				intr->vector_base,
				intr->num_vectors,
				intr->cascade_vector);
		kprintf("     cpu_intr_base:%x, cpu_intr_stride:%d, flags:%w, local_stride:%d\n",
				intr->cpu_intr_base,
				intr->cpu_intr_stride,
				intr->flags,
				intr->local_stride);
		print_intrgen(" id", &intr->id);
		print_intrgen("eoi", &intr->eoi);

		kprintf("     mask:%v, unmask:%v, config:%v\n",
			intr->mask,
			intr->unmask,
			intr->config);
		intr = SYSPAGE_ARRAY_ADJ_OFFSET(intrinfo, intr, elsize);
	}
}

static void
print_smp(void) {
	struct smp_entry *smp = _SYSPAGE_ENTRY(PSP_SYSPAGE,smp);

	kprintf("  send_ipi:%v\n", smp->send_ipi);
}

static void
print_hypinfo(void)
{
    const struct hypinfo_entry * const hypinfo = _SYSPAGE_ENTRY(PSP_SYSPAGE, hypinfo);

    const char *virt_name;
    if (PSP_SYSPAGE->type == SYSPAGE_AARCH64) {
        virt_name = "ARM";
    } else {
        switch (HYPINFO_VIRT_GET(hypinfo->flags)) {
        case HYPINFO_VIRT_VMX:
            virt_name = "VMX";
            break;
        case HYPINFO_VIRT_SVM:
            virt_name = "SVM";
            break;
        default:
            virt_name = "Unknown";
            break;
        }
    }

    kprintf("  virt:%s flags:%L\n", virt_name, hypinfo->flags);
}

static void
print_cluster(void)
{
    const struct cluster_entry *cluster = _SYSPAGE_ENTRY(PSP_SYSPAGE, cluster);
    unsigned const elsize = _SYSPAGE_ELEMENT_SIZE(PSP_SYSPAGE, cluster);
    unsigned const num = _SYSPAGE_ENTRY_SIZE(PSP_SYSPAGE, cluster) / elsize;

    for (unsigned i = 0; i < num; ++i) {
        kprintf("  %d) cpumask:%L name:%d(%s)\n",
            i, cluster->cpumask, cluster->name, get_string(cluster->name));
        cluster = SYSPAGE_ARRAY_ADJ_OFFSET(cluster, cluster, elsize);
    }
}

#define SYSPAGE_TYPE_MASK	0x0000ffffu
#define ARRAY_SECTION		0x80000000u
#define INFO_SECTION		0x40000000u
#define IMPLICIT_DISABLE    0x20000000u
#define EXPLICIT_DISABLE    0x10000000u
#define EXPLICIT_ENABLE     0x08000000u

struct debug_syspage_section {
	const char 		*name;
	unsigned	    loc;
	unsigned	    flags;
	void			(*print)(void);
};

#define PRT_SYSPAGE_RTN(name, array)	\
	{ #name, (unsigned)offsetof(struct syspage_entry, name), INFO_SECTION|(array), print_##name }

#define CPU_PRT_SYSPAGE_RTN(upper_cpu, lower_cpu, flags, name)	\
	{ #name, (unsigned)offsetof(struct syspage_entry, un.lower_cpu.name), \
		(flags) + (SYSPAGE_##upper_cpu+1), \
		lower_cpu##_print_##name }

static struct debug_syspage_section sp_section[] = {
	PRT_SYSPAGE_RTN(system_private,0),
	PRT_SYSPAGE_RTN(qtime,0),
	PRT_SYSPAGE_RTN(callout,0),
	PRT_SYSPAGE_RTN(cpuinfo,ARRAY_SECTION),
	PRT_SYSPAGE_RTN(cacheattr,ARRAY_SECTION),
	PRT_SYSPAGE_RTN(asinfo,ARRAY_SECTION),
	PRT_SYSPAGE_RTN(hwinfo,0),
	PRT_SYSPAGE_RTN(typed_strings,0),
	PRT_SYSPAGE_RTN(strings,0),
	PRT_SYSPAGE_RTN(intrinfo,ARRAY_SECTION),
	PRT_SYSPAGE_RTN(smp,0),
    PRT_SYSPAGE_RTN(hypinfo,0),
    PRT_SYSPAGE_RTN(cluster,ARRAY_SECTION),
// This second include of print_sysp.h will cause the CPU_PRT_SYSPAGE_RTN
// definitions for the various routines to be added.
#include "print_sysp.h"
};

void
print_syspage_enable(const char *name) {
	unsigned	i;
	unsigned	on_bit;
	unsigned	off_mask;

	if(*name == '~') {
		++name;
		on_bit = EXPLICIT_DISABLE;
		off_mask = ~EXPLICIT_ENABLE;
	} else {
		on_bit = EXPLICIT_ENABLE;
		off_mask = ~EXPLICIT_DISABLE;
	}
	for(i = 0; i < NUM_ELTS(sp_section); ++i) {
		if(strcmp(sp_section[i].name, name) == 0) {
			sp_section[i].flags &= off_mask;
			sp_section[i].flags |= on_bit;
		}
		if(on_bit & EXPLICIT_ENABLE) {
			// If we have an explict enable, we mark all the sections
			// as implicitly disabled so that we don't print them out
			// unless we end up with an explict enablement of the entry
			sp_section[i].flags |= IMPLICIT_DISABLE;
		}
	}
}

void
print_syspage_sections(void) {
	unsigned	i;
	unsigned	flags;
	unsigned	type;

	kprintf("Header size=0x%x, Total Size=0x%x, Version=%d.%d, #Cpu=%d, Type=%d\n",
		PSP_SYSPAGE->size,
		PSP_SYSPAGE->total_size,
        PSP_SYSPAGE->version.major,
        PSP_SYSPAGE->version.minor,
		PSP_SYSPAGE->num_cpu,
		PSP_SYSPAGE->type);

	for(i = 0; i < NUM_ELTS(sp_section); ++i) {
		flags = sp_section[i].flags;
		if(flags & EXPLICIT_ENABLE) {
			flags &= ~IMPLICIT_DISABLE;
		}
		type = flags & SYSPAGE_TYPE_MASK;
		if((type != 0) && (type != (PSP_SYSPAGE->type + 1))) {
			// Not a section on this system page
			flags |= EXPLICIT_DISABLE;
		}
		if(sp_section[i].loc > PSP_SYSPAGE->size) {
			// Section isn't present on this system page
			flags |= EXPLICIT_DISABLE;
		}
		if(!(flags & (EXPLICIT_DISABLE|IMPLICIT_DISABLE))) {
			kprintf("Section:%s ", sp_section[i].name);
			if(sp_section[i].flags & INFO_SECTION) {
				syspage_array_info	*info;

				info = (void *)((uint8_t *)PSP_SYSPAGE + sp_section[i].loc);
				kprintf("offset:0x%x size:0x%x", info->entry_off, info->entry_size);
				if(flags & ARRAY_SECTION) {
					kprintf(" elsize:0x%x", info->element_size);
				}
				kprintf("\n");
				if(info->entry_size > 0 && PSP_VERBOSE(2)) {
					sp_section[i].print();
				}
			} else {
				kprintf("offset:0x%x\n", sp_section[i].loc);
				if(PSP_VERBOSE(2)) {
					sp_section[i].print();
				}
			}
		}
	}
}

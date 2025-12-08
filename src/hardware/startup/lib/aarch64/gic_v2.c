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
 * Copyright 2014-2020, QNX Software Systems.
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
#include <aarch64/gic_v2.h>

/*
 * GICv2 support code
 */

static struct startup_intrinfo gic_v2_ppi = {
    .vector_base      = _NTO_INTR_CLASS_EXTERNAL,
    .num_vectors      = 0,
    .cascade_vector   = _NTO_INTR_SPARE,
    .cpu_intr_base    = 0,
    .cpu_intr_stride  = 0,
    .flags            = 0,
    .id  = { 0, 0, &interrupt_id_gic_v2_ppi},
    .eoi = { 0, 0, &interrupt_eoi_gic_v2},
    .mask             = &interrupt_mask_gic_v2_ppi,
    .unmask           = &interrupt_unmask_gic_v2_ppi,
    .config           = &interrupt_config_gic_v2_ppi,
    .local_stride	 = 0x8000
};

static struct startup_intrinfo gic_v2_spi = {
    .vector_base      = _NTO_INTR_CLASS_EXTERNAL + 32U,
    .num_vectors      = 0,
    .cascade_vector   = _NTO_INTR_SPARE,
    .cpu_intr_base    = 0,
    .cpu_intr_stride  = 0,
    .flags            = 0,
    .id  = { 0, 0, &interrupt_id_gic_v2_spi},
    .eoi = { 0, 0, &interrupt_eoi_gic_v2},
    .mask             = &interrupt_mask_gic_v2_spi,
    .unmask           = &interrupt_unmask_gic_v2_spi,
    .config           = NULL,
    .local_stride	 = 0,
};

static unsigned
gic_v2_cpunum(void)
{
	unsigned	mask;
	unsigned	i;

	/*
	 * Figure out our GIC cpu interface number for sending IPIs.
	 * GICv2 supports only 8 processors and ITARGETSR0 byte 0 contains the
	 * target mask for this cpu's ID0 SGI interrupt.
	 */
	mask = in32(gic_gicd + ARM_GICD_ITARGETSRn) & 0xff;
	if ((mask == 0) && (lsp.syspage.p->num_cpu == 1)) {
		// uniprocessor
		return 0;
	}
	for (i = 0; i < 8; i++) {
		if (mask == (1 << i)) {
			return i;
		}
	}
	crash("gic_v2_cpunum: no target cpu detected %x\n", in32(gic_gicd + ARM_GICD_ITARGETSRn));
	return 0;
}

static void
gic_v2_gicc_init(unsigned cpu)
{
	struct aarch64_gic_map_entry	*gic_map = lsp.cpu.aarch64_gic_map.p;

	if (gic_gicd == 0 || gic_gicc == 0) {
		crash("gic not initialised");
	}
	gic_map->gic_cpu[cpu] = gic_v2_cpunum();
	if (debug_flag) {
		kprintf("cpu%d: GICv2 cpu interface %d\n", cpu, gic_map->gic_cpu[cpu]);
	}

    /* Set SGI/PPI priority (all 0xA0, except for SGI0 which is 0xC0) */
    out32(gic_gicd + ARM_GICD_IPRIORITYn, 0xA0A0A0C0);
	unsigned i;
	for(i = 1; i < 8; ++i) {
		out32(gic_gicd + ARM_GICD_IPRIORITYn + (i*4), 0xA0A0A0A0);
	}

	/*
	 * Disable all banked PPI interrupts
	 * Enable all SGI interrupts
	 */
	out32(gic_gicd + ARM_GICD_ICENABLERn, 0xffff0000);
	out32(gic_gicd + ARM_GICD_ISENABLERn, 0x0000ffff);

	/*
	 * Set priority mask to allow all interrupts and enable cpu interface
	 */
	out32(gic_gicc + ARM_GICC_PMR, 0xF0);
	out32(gic_gicc + ARM_GICC_CTLR, ARM_GICC_CTLR_EN);
}

void
gic_v2_init(paddr_t gicd, paddr_t gicc)
{
	struct aarch64_gic_map_entry	*gic_map = lsp.cpu.aarch64_gic_map.p;
	unsigned	gic_cpu;
	unsigned	itn;
	unsigned	i;

	unsigned const off = as_find(AS_NULL_OFF, "memory", NULL);
	as_add64(gicd, gicd + 0xfffu, AS_ATTR_DEV, "gicd", off);
	as_add64(gicc, gicc + 0x1fffu, AS_ATTR_DEV, "gicc", off);

	gic_gicd = gicd;
	gic_gicc = gicc;
	gic_cpu_init = gic_v2_gicc_init;
	gic_sendipi = &sendipi_gic_v2;


	gic_gicd_vaddr = callout_io_map(0x1000, gicd);
	gic_gicc_vaddr = callout_io_map(0x2000, gicc);

	/*
	 * Initialise the GIC cpu map with invalid values.
	 * gic_v2_gicc_init() will set the mapping for each cpu as it comes up.
	 */
	for (i = 0; i < lsp.syspage.p->num_cpu; i++) {
		gic_map->gic_cpu[i] = ~0u;
	}
	gic_cpu = gic_v2_cpunum();

	/*
	 * Disable distributor
	 */
	out32(gicd + ARM_GICD_CTLR, 0);

	/*
	 * Calculate number of interrupt lines.
	 */
	 gic_v2_ppi.num_vectors = 32U; // always 32

	itn = ((in32(gicd + ARM_GICD_TYPER) & ARM_GICD_TYPER_ITLN) + 1) * 32;
	gic_v2_spi.num_vectors = min(1020, itn) - gic_v2_ppi.num_vectors;
	unsigned const last_spi = gic_v2_ppi.num_vectors + gic_v2_spi.num_vectors;

	if (debug_flag) {
		kprintf("GICv2: %d interrupts\n", itn);
	}

	/*
	 * Disable all SPI interrupts and clear pending state
	 */
	for (i = 32; i < itn; i += 32) {
		out32(gicd + ARM_GICD_ICENABLERn + (i * 4 / 32), 0xffffffff);
		out32(gicd + ARM_GICD_ICPENDRn + (i * 4 / 32), 0xffffffff);
	}

	/*
	 * Set default priority of all SPI interrupts to 0xa0
	 */
	 // XXX shouldn't i start at 32 ?
	for (i = 8; i < last_spi; i += 4) {
		out32(gicd + ARM_GICD_IPRIORITYn + i, 0xa0a0a0a0);
	}

	/*
	 * Route all SPI interrupts to cpu0
	 */
	if (debug_flag) {
		kprintf("GICv2: routing SPIs to gic cpu %d\n", gic_cpu);
	}
	for (i = 32; i < last_spi; i += 4) {
		out32(gicd + ARM_GICD_ITARGETSRn + i, (0x01010101u << gic_cpu));
	}

	/*
	 * Default all PPI/SPI interrupts as level triggered
	 */
	for (i = 16; i < itn; i += 16) {
		out32(gicd + ARM_GICD_ICFGRn + (i * 4 / 16), 0);
	}

	/*
	 * Enable distributor - cpu interface is initialised via init_cpuinfo().
	 */
	out32(gicd + ARM_GICD_CTLR, ARM_GICD_CTLR_EN);

	/*
	 * Add the interrupt callouts
	 */
	add_interrupt(&gic_v2_ppi);
	add_interrupt(&gic_v2_spi);
}

/* Get number of GIC SPIs */
int
gic_v2_num_spis(void)
{
	return ((in32(gic_gicd + ARM_GICD_TYPER) & ARM_GICD_TYPER_ITLN) + 1) * 32;
}

/**
 * Set GIC interrupt trigger mode
 *
 * @param irq   Interrupt vector number.
 * @param edge  1: edge-triggered; 0 : level-triggered
 */
void
gic_v2_set_intr_trig_mode(int irq, int edge)
{

	if (gic_gicd) {
		if (irq >= 32 && irq < gic_v2_num_spis() + 32) {
			uint32_t oval, val;
			uint32_t mask = 0x2 << ((irq % 16) * 2);
			uint32_t off  = (irq / 16) * 4;

			oval = val = in32(gic_gicd + ARM_GICD_ICFGRn + off);
			if (edge) {
				val |=  mask;
			} else {
				val &= ~mask;
			}

			out32(gic_gicd + ARM_GICD_ICFGRn + off, val);

			if (debug_flag > 5) {
				kprintf("%s: irq %d type %s: 0x%x: 0x%x -> 0x%x\n", __FUNCTION__,
							 irq, edge ? "edge" : "level", gic_gicd + ARM_GICD_ICFGRn + off, oval, val);
			}
		} else {
			kprintf("%s: irq %d type %s: invalid irq (32-%d)\n", __FUNCTION__, irq, edge ? "edge" : "level", gic_v2_num_spis() + 32 - 1);
		}
	}
}

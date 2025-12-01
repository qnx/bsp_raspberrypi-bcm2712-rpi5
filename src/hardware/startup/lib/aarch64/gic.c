/*
 * $QNXLicenseC:
 * Copyright 2014, 2021 QNX Software Systems.
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

/*
 * Global variables for GIC support.
 */
 void	(*gic_cpu_init)(unsigned);		// CPU interface initialisation
/*
 * Note
 * These next variables are only used for GICv2 and in some board copies
 * of the old GICv3 implementations
 */
paddr_t		gic_gicd;					// GIC distributor
paddr_t		gic_gicc;					// GIC cpu interface (GICv2 only)
paddr_t		gic_gicr;					// GIC cpu redistributor (v3 only)
struct	callout_rtn	*gic_sendipi;		// send_ipi callout
uintptr_t	gic_gicd_vaddr;				// callout vaddr for GICD regs
uintptr_t	gic_gicc_vaddr;				// callout vaddr for GICC regs (v2 only)
unsigned	gic_gicr_shift;				// shift value for GICR spacing (v3 only)
/*
 * ths next one is also needed by the GICv3 IPI callout so must stay for now
*/
uintptr_t	gic_gicr_vaddr;				// callout vaddr for GICR regs (v3 IPI only)

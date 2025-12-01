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

/*
 * ARM Generic Interrupt Controller v2
 */

#ifndef	__ARM_GIC_V2_H_INCLUDED
#define	__ARM_GIC_V2_H_INCLUDED

/*
 * -----------------------------------------------------------------------
 * Interrupt Distributor registers
 * -----------------------------------------------------------------------
 */

#define	ARM_GICD_SIZE			0x1000

#define	ARM_GICD_CTLR			0x0000
#define	ARM_GICD_TYPER			0x0004
#define ARM_GICD_IIDR			0x0008
#define ARM_GICD_IGROUPRn		0x0080
#define	ARM_GICD_ISENABLERn		0x0100
#define	ARM_GICD_ICENABLERn		0x0180
#define	ARM_GICD_ISPENDRn		0x0200
#define	ARM_GICD_ICPENDRn		0x0280
#define	ARM_GICD_ISACTIVERn		0x0300
#define	ARM_GICD_ICACTIVERn		0x0380
#define	ARM_GICD_IPRIORITYn		0x0400
#define	ARM_GICD_ITARGETSRn		0x0800
#define	ARM_GICD_ICFGRn			0x0C00
#define	ARM_GICD_SGIR			0x0F00
#define ARM_GICD_CPENDSGIRn		0x0F10
#define ARM_GICD_SPENDSGIRn		0x0F20
#define ARM_GICD_PIDR4			0x0FD0
#define ARM_GICD_PIDR5			0x0FD4
#define ARM_GICD_PIDR6			0x0FD8
#define ARM_GICD_PIDR7			0x0FDC
#define ARM_GICD_PIDR0			0x0FE0
#define ARM_GICD_PIDR1			0x0FE4
#define ARM_GICD_PIDR2			0x0FE8
#define ARM_GICD_PIDR3			0x0FEC
#define ARM_GICD_CIDR0			0x0FF0
#define ARM_GICD_CIDR1			0x0FF4
#define ARM_GICD_CIDR2			0x0FF8
#define ARM_GICD_CIDR3			0x0FFC

#define ARM_GICD_TYPER_ITLN		0x1F
#define	ARM_GICD_CTLR_EN		0x00000001

/*
 * -----------------------------------------------------------------------
 * CPU interface registers
 * -----------------------------------------------------------------------
 */

#define ARM_GICC_SIZE		0x2000

#define	ARM_GICC_CTLR		0x00
#define	ARM_GICC_PMR		0x04
#define	ARM_GICC_BBPR		0x08
#define	ARM_GICC_IAR		0x0C
#define	ARM_GICC_EOIR		0x10
#define	ARM_GICC_RPR		0x14
#define	ARM_GICC_HPPIR		0x18
#define	ARM_GICC_ABPR		0x1C
#define	ARM_GICC_AIAR		0x20
#define	ARM_GICC_AEOIR		0x24
#define	ARM_GICC_AHPPIR		0x28
#define	ARM_GICC_APR0		0xD0
#define	ARM_GICC_NSAPR0		0xE0
#define	ARM_GICC_IIDR		0xFC
#define	ARM_GICC_DIR		0x1000

/*
 * CPU_CTRL bits
 */
#define	ARM_GICC_CTLR_EN		0x00000001

/*
 * CPU_IACK bits
 */
#define	ARM_GICC_IAR_IDMASK	0x000003ff


#endif	/* __ARM_GIC_V2_H_INCLUDED */

/* __SRCVERSION("gic.h $Rev: 870901 $"); */

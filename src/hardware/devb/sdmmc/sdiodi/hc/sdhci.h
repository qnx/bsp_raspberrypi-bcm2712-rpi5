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
 * Copyright 2007, 2008, QNX Software Systems.
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

#ifndef	_SDHCI_H_INCLUDED
#define	_SDHCI_H_INCLUDED

#include <gulliver.h>
#include <internal.h>

#if !defined(__X86__) && !defined(__X86_64__)

#ifndef sdhci_in8
#define sdhci_in8			(in8)
#endif
#ifndef sdhci_in16
#define sdhci_in16			(in16)
#endif
#ifndef sdhci_in32
#define sdhci_in32			(in32)
#endif
#ifndef sdhci_in32s
#define sdhci_in32s			(in32s)
#endif
#ifndef sdhci_out8
#define sdhci_out8			(out8)
#endif
#ifndef sdhci_out16
#define sdhci_out16			(out16)
#endif
#ifndef sdhci_out32
#define sdhci_out32			(out32)
#endif
#ifndef sdhci_out32s
#define sdhci_out32s		(out32s)
#endif

#else

static __inline__ _Uint8t __attribute__((__unused__))
sdhci_in8(_Uintptrt const __addr) {
	_Uint8t	__data;

	__data = *(volatile _Uint8t *)__addr;
	return(__data);
}

static __inline__ _Uint16t __attribute__((__unused__))
sdhci_in16(_Uintptrt const __addr) {
	_Uint16t	__data;

	__data = *(volatile _Uint16t *)__addr;
	return( ENDIAN_LE16( __data ) );
}

static __inline__ _Uint32t __attribute__((__unused__))
sdhci_in32(_Uintptrt const __addr) {
	_Uint32t	__data;

	__data = *(volatile _Uint32t *)__addr;
	return( ENDIAN_LE32( __data ) );
}

static __inline__ void * __attribute__((__unused__))
sdhci_in32s(void * const __buff, unsigned __len, _Uintptrt const __addr) {
	_Uint32t	*__p = __buff;

	--__p;
	while(__len > 0) {
		*++__p = *(volatile _Uint32t *)__addr;
		--__len;
	}
	return(__p+1);
}

static __inline__ void __attribute__((__unused__))
sdhci_out8(_Uintptrt const __addr, _Uint8t __data) {
	*(volatile _Uint8t *)__addr = __data;
}

static __inline__ void __attribute__((__unused__))
sdhci_out16(_Uintptrt const __addr, _Uint16t __data) {
	*(volatile _Uint16t *)__addr = ENDIAN_LE16( __data );
}

static __inline__ void __attribute__((__unused__))
sdhci_out32(_Uintptrt const __addr, _Uint32t __data) {
	*(volatile _Uint32t *)__addr = ENDIAN_LE32( __data );
}

static __inline__ void * __attribute__((__unused__))
sdhci_out32s(const void * const __buff, unsigned __len, _Uintptrt const __addr) {
	_Uint32t	*__p = (_Uint32t *)__buff;

	--__p;
	while(__len > 0) {
		*(volatile _Uint16t *)__addr = *++__p;
		--__len;
	}
	return(__p+1);
}

#endif

#define SDHCI_CLK_MIN			400000
#define SDHCI_CLK_MAX			52000000

#define SDHCI_TUNING_RETRIES	40
#define SDHCI_TUNING_TIMEOUT	150

#define SDHCI_SIZE				4096

#define	SDHCI_SDMA_ARG2			0x00
#define	SDHCI_32BIT_BLK_CNT		0x00	/* >= v4 */

#define	SDHCI_BLK				0x04
	#define SDHCI_BLK_SDMA_BOUNDARY_512K	( 7 << 12 )
	#define SDHCI_BLK_BLKSIZE_MASK			0x00000fff
	#define SDHCI_BLK_BLKCNT_MASK			0xffff0000
	#define SDHCI_BLK_BLKCNT_SHIFT			16

#define	SDHCI_ARG				0x08

#define	SDHCI_CMD				0x0C		// Command and transfer mode register
	#define	SDHCI_CMD_DE			(1 <<  0)	// DMA Enable
	#define	SDHCI_CMD_BCE			(1 <<  1)	// Block Count Enable
	#define	SDHCI_CMD_ACMD12		(1 <<  2)	// Auto CMD12 Enable
	#define	SDHCI_CMD_ACMD23		(1 <<  3)	// Auto CMD23 Enable
	#define	SDHCI_CMD_DDIR			(1 <<  4)	// Data Transfer Direction Read
	#define	SDHCI_CMD_MBS			(1 <<  5)	// Multi Block Select

	#define	SDHCI_CMD_RSP_TYPE_136	(1 << 16)	// Response length 136 bit
	#define	SDHCI_CMD_RSP_TYPE_48	(2 << 16)	// Response length 48 bit
	#define	SDHCI_CMD_RSP_TYPE_48b	(3 << 16)	// Response length 48 bit with busy after response
	#define	SDHCI_CMD_CCCE			(1 << 19)	// Comamnd CRC Check Enable
	#define	SDHCI_CMD_CICE			(1 << 20)	// Comamnd Index Check Enable
	#define	SDHCI_CMD_DP			(1 << 21)	// Data Present
	#define	SDHCI_CMD_TYPE_CMD12	(3 << 22)	// CMD12 or CMD52 "I/O Abort"

#define	SDHCI_RESP0				0x10
#define	SDHCI_RESP1				0x14
#define	SDHCI_RESP2				0x18
#define	SDHCI_RESP3				0x1C

#define	SDHCI_DATA				0x20

#define	SDHCI_PSTATE			0x24	// Present State

	#define SDHCI_PSTATE_CLSL_MSK	(1 << 24)
	#define SDHCI_PSTATE_DLSL_MSK	(0xF << 20)
	#define	SDHCI_PSTATE_WP			(1 << 19)
	#define	SDHCI_PSTATE_CD			(1 << 18)
	#define	SDHCI_PSTATE_CSS		(1 << 17)
	#define	SDHCI_PSTATE_CI			(1 << 16)
	#define	SDHCI_PSTATE_BRE		(1 << 11)	// Buffer Read Ready
	#define	SDHCI_PSTATE_BWE		(1 << 10)	// Buffer Write Ready
	#define	SDHCI_PSTATE_RTA		(1 <<  9)	// Read Transfer Active
	#define	SDHCI_PSTATE_WTA		(1 <<  8)	// Write Transfer Active
	#define	SDHCI_PSTATE_RTR		(1 <<  4)	// Re-Tuning Request
	#define	SDHCI_PSTATE_DLA		(1 <<  2)	// mmci_dat line active
	#define	SDHCI_PSTATE_DATI		(1 <<  1)	// Command inhibit
	#define	SDHCI_PSTATE_CMDI		(1 <<  0)	// Command inhibit
	#define	SDHCI_CARD_STABLE		(SDHCI_PSTATE_CD | SDHCI_PSTATE_CSS | SDHCI_PSTATE_CI)

#define	SDHCI_HCTL				0x28	// Host Control
	#define	SDHCI_HCTL_LED			(1 << 0)	// LED Control
	#define	SDHCI_HCTL_DTW1			(0 << 1)	// Data Bus Width 1 bit
	#define	SDHCI_HCTL_DTW4			(1 << 1)	// Data Bus Width 4 bit
	#define	SDHCI_HCTL_HSE			(1 << 2)	// High Speed Enable
	#define SDHCI_HCTL_DMA_MSK  	(3 << 3)    // DMA select mask
	#define SDHCI_HCTL_SDMA      	(0 << 3)    // SDMA select
	#define SDHCI_HCTL_ADMA32		(2 << 3)	// 32 bit ADMA select (< v4.00)
	#define SDHCI_HCTL_ADMA64		(3 << 3)	// 64 bit ADMA select (< v4.00)
	#define SDHCI_HCTL_ADMA2		(2 << 3)	// Enable ADMA2 (>= v4.00 and v4-mode enabled)
	#define SDHCI_HCTL_ADMA2_ADMA3	(3 << 3)	// Enable ADMA2 and ADMA3 (>= v4.00 and v4-mode enabled)
	#define	SDHCI_HCTL_MMC8			(1 << 3)	// Data Bus Width 8 bit (Intel)
	#define SDHCI_HCTL_BW8			(1 << 5)	// 8 bit bus select
	#define	SDHCI_HCTL_SDBP			(1 << 8)	// SD bus power
	#define	SDHCI_HCTL_SDVS1V8		(5 << 9)	// SD bus voltage 1.8V
	#define	SDHCI_HCTL_SDVS3V0		(6 << 9)	// SD bus voltage 3.0V
	#define	SDHCI_HCTL_SDVS3V3		(7 << 9)	// SD bus voltage 3.3V
	#define	SDHCI_HCTL_SDVS_MSK		(7 << 9)

#define	SDHCI_BLKGAPCTL			0x2A

#define	SDHCI_WAKECTL			0x2B

#define	SDHCI_SYSCTL			0x2C	// Clock Control/Timeout/Software reset
	#define	SDHCI_SYSCTL_ICE		(1 << 0)	// Internal Clock Enable
	#define	SDHCI_SYSCTL_ICS		(1 << 1)	// Internal Clock Stable
	#define	SDHCI_SYSCTL_CEN		(1 << 2)	// Clock Enable
	#define	SDHCI_SYSCTL_PLLEN		(1 << 3)	// PLL Enable
	#define	SDHCI_SYSCTL_CGS_PROG	(1 << 5)	// Clock Generator Select (programmable)
	#define SDHCI_SYSCTL_CLK( _c )	( ( ( (_c) << 8 ) & 0xff00 ) | ( ( (_c) >> 2 ) & 0xc0 ) )
	#define SDHCI_SYSCTL_SDCLKFS_MSK	0xffc0
	#define	SDHCI_SYSCTL_DTO_MSK	(0xF << 16)	// Data timeout counter value and busy timeout
	#define	SDHCI_SYSCTL_DTO_MAX	(0xE << 16)	// Timeout = TCF x 2^27
	#define	SDHCI_SYSCTL_SRA		(1 << 24)	// Software reset for all
	#define	SDHCI_SYSCTL_SRC		(1 << 25)	// Software reset for mmci_cmd line
	#define	SDHCI_SYSCTL_SRD		(1 << 26)	// Software reset for mmci_dat line

	#define SDHCI_CLK_MAX_DIV_SPEC_VER_2	256
	#define SDHCI_CLK_MAX_DIV_SPEC_VER_3	2046

#define	SDHCI_IS				0x30	// Interrupt status register
#define	SDHCI_IE				0x34	// Interrupt SD enable register
#define	SDHCI_ISE				0x38	// Interrupt signal enable register
	#define	SDHCI_INTR_CC			(1 <<  0)	// Command Complete
	#define	SDHCI_INTR_TC			(1 <<  1)	// Transfer Complete
	#define	SDHCI_INTR_BGE			(1 <<  2)	// Block Gap Event
	#define SDHCI_INTR_DMA			(1 <<  3)	// DMA interupt
	#define	SDHCI_INTR_BWR			(1 <<  4)	// Buffer Write Ready interrupt
	#define	SDHCI_INTR_BRR			(1 <<  5)	// Buffer Read Ready interrupt
	#define SDHCI_INTR_CINS			(1 <<  6)	// Card Insertion
	#define SDHCI_INTR_CREM			(1 <<  7)	// Card Removal
	#define	SDHCI_INTR_CIRQ			(1 <<  8)	// Card interrupt
	#define	SDHCI_INTR_OBI			(1 <<  9)	// Out-Of-Band interrupt
	#define SDHCI_INTR_BSR			(1 << 10)	// Boot Status
	#define SDCHI_INTR_RETUNE		(1 << 12)	// Re-Tuning
	#define	SDHCI_INTR_ERRI			(1 << 15)	// Error interrupt
	#define	SDHCI_INTR_CTO			(1 << 16)	// Command Timeout error
	#define	SDHCI_INTR_CCRC			(1 << 17)	// Command CRC error
	#define	SDHCI_INTR_CEB			(1 << 18)	// Command End Bit error
	#define	SDHCI_INTR_CIE			(1 << 19)	// Command Index error
	#define	SDHCI_INTR_DTO			(1 << 20)	// Data Timeout error
	#define	SDHCI_INTR_DCRC			(1 << 21)	// Data CRC error
	#define	SDHCI_INTR_DEB			(1 << 22)	// Data End Bit error
	#define	SDHCI_INTR_ACE			(1 << 24)	// ACMD12 error
	#define SDHCI_INTR_ADMAE		(1 << 25)	// ADMA Error
	#define SDHCI_INTR_SDMAE		(1 << 28)	// SDMA Error
	#define SDHCI_INTR_DFLTS		(SDHCI_INTR_ADMAE | SDHCI_INTR_ACE | SDHCI_INTR_CIE |	\
									 SDHCI_INTR_CEB | SDHCI_INTR_CCRC | SDHCI_INTR_CTO |		\
									 SDHCI_INTR_ERRI /* | SDHCI_INTR_CINS | SDHCI_INTR_CREM */ )
	#define SDHCI_INTR_ALL			0x33ff87ff
	#define SDHCI_INTR_CLR_MSK		0x117f80f3

#define	SDHCI_AC12				0x3C

#define	SDHCI_HCTL2				0x3E	// Host Control 2
	#define SDHCI_HCTL2_PRESET_VAL		(1 << 15)
	#define SDHCI_HCTL2_ASYNC_INT		(1 << 14)
	#define SDHCI_HCTL2_64BIT_ADDR		(1 << 13)
	#define SDHCI_HCTL2_V4_MODE			(1 << 12)
	#define SDHCI_HCTL2_ACMD23_EN		(1 << 11)
	#define SDHCI_HCTL2_ADMA2_26BIT		(1 << 10)
	#define SDHCI_HCTL2_TUNED_CLK		(1 << 7)
	#define SDHCI_HCTL2_EXEC_TUNING		(1 << 6)

	#define SDHCI_HCTL2_DRV_TYPE_MSK	(0x3 << 4)
	#define SDHCI_HCTL2_DRV_TYPE_D		(0x3 << 4)
	#define SDHCI_HCTL2_DRV_TYPE_C		(0x2 << 4)
	#define SDHCI_HCTL2_DRV_TYPE_A		(0x1 << 4)
	#define SDHCI_HCTL2_DRV_TYPE_B		(0x0 << 4)

	#define SDHCI_HCTL2_SIG_1_8V		(1 << 3)

	#define SDHCI_HCTL2_MODE( _c )	( (_c) & SDHCI_HCTL2_MODE_MSK )
	#define SDHCI_HCTL2_MODE_MSK		(0x7 << 0)
	#define SDHCI_HCTL2_MODE_HS400		(0x5 << 0)
	#define SDHCI_HCTL2_MODE_DDR50		(0x4 << 0)
	#define SDHCI_HCTL2_MODE_HS200		(0x3 << 0)
	#define SDHCI_HCTL2_MODE_SDR104		(0x3 << 0)
	#define SDHCI_HCTL2_MODE_SDR50		(0x2 << 0)
	#define SDHCI_HCTL2_MODE_SDR25		(0x1 << 0)

#define	SDHCI_CAP				0x40	// Capability Registers bits 0 to 31
	#define	SDHCI_CAP_CS_MSK		(0x3 << 30)
	#define	SDHCI_CAP_CS_SHARED		(0x2 << 30)	// Shared Slot
	#define	SDHCI_CAP_CS_EMBEDDED	(0x1 << 30)	// Embedded Slot
	#define	SDHCI_CAP_CS_RMB		(0x0 << 30)	// Removable Card Slot
	#define	SDHCI_CAP_64BIT_ADDR	(1 << 28)	// 64 bit system address support
	#define	SDHCI_CAP_S18			(1 << 26)	// 1.8V support
	#define	SDHCI_CAP_S30			(1 << 25)	// 3.0V support
	#define	SDHCI_CAP_S33			(1 << 24)	// 3.3V support
	#define	SDHCI_CAP_SRS			(1 << 23)	// Suspend/Resume support
	#define	SDHCI_CAP_DMA			(1 << 22)	// DMA support
	#define	SDHCI_CAP_HS			(1 << 21)	// High-Speed support
	#define	SDHCI_CAP_ADMA1			(1 << 20)	// ADMA1 support
	#define	SDHCI_CAP_ADMA2			(1 << 19)	// ADMA2 support
	#define	SDHCI_CAP_EXTMBUS		(1 << 18)	// Extended media bus support
	#define	SDHCI_CAP_MBL512		(0 << 16)	// Max block length 512
	#define	SDHCI_CAP_MBL2048		(2 << 16)	// Max block length 2048
	#define SDHCI_CAP_BASE_CLK( _c, _v )	( ( ( ( _v ) >= SDHCI_SPEC_VER_3 ) ?	\
											( ( ( _c ) >> 8 ) & 0xff ) :			\
											( ( ( _c ) >> 8 ) & 0x3f ) ) * 1000 * 1000 )

#define	SDHCI_CAP2				0x44	// bits 32 to 64
	#define SDHCI_CAP_CLK_MULT( _cap )	( ( ( _cap ) & 0xff0000 ) >> 16 )
	#define SDHCI_CAP_TUNING_MODE( _cap )	( ( ( _cap ) & 0xe000 ) >> 14 )
	#define SDHCI_CAP_TUNE_SDR50	(1 << 13)
	#define SDHCI_CAP_TIMER_COUNT( _cap )	( ( ( _cap ) & 0xf00 ) >> 8 )

	#define SDHCI_CAP_DRIVE_TYPE_D	(1 << 6)
	#define SDHCI_CAP_DRIVE_TYPE_C	(1 << 5)
	#define SDHCI_CAP_DRIVE_TYPE_A	(1 << 4)
	#define SDHCI_CAP_DDR50			(1 << 2)
	#define SDHCI_CAP_SDR104		(1 << 1)
	#define SDHCI_CAP_SDR50			(1 << 0)

#define	SDHCI_MCCAP				0x48	// Maximum Current Capabilities
	#define SDHCI_MCCAP_330( _cap )	( (_cap) & 0x0000ff )
	#define SDHCI_MCCAP_300( _cap )	( ( (_cap) & 0x00ff00 ) >> 8 )
	#define SDHCI_MCCAP_180( _cap )	( ( (_cap) & 0xff0000 ) >> 16 )
	#define SDHCI_MCCAP_MULT		4	// 4mA steps

#define SDHCI_ADMA_ES			0x54
#define SDHCI_ADMA_ADDRL  		0x58
#define SDHCI_ADMA_ADDRH	  	0x5C
#define	SDHCI_SLOT_IS			0xFC

#define	SDHCI_HCTL_VERSION		0xFE
	#define SDHCI_SPEC_VER_MSK		0xff
	#define SDHCI_SPEC_VER_420		0x5
	#define SDHCI_SPEC_VER_410		0x4
	#define SDHCI_SPEC_VER_4		0x3
	#define SDHCI_SPEC_VER_3		0x2
	#define SDHCI_SPEC_VER_2		0x1
	#define SDHCI_SPEC_VER_1		0x0


#define SDHCI_ADMA2_MAX_XFER	(1024 * 60)

// 32 bit ADMA descriptor defination
typedef struct _sdhci_adma32_t {
	uint16_t	attr;
	uint16_t	len;
	uint32_t	addr;
} sdhci_adma32_t;

// 64 bit ADMA descriptor defination
typedef struct _sdhci_adma64_t {
	uint16_t	attr;
	uint16_t	len;
	uint32_t	addr_lo;
	uint32_t	addr_hi;
} sdhci_adma64_t;

// 64 bit ADMA descriptor definition (for v4 mode)
typedef struct _sdhci_v4_adma64_t {
	uint16_t	attr;
	uint16_t	len;
	uint32_t	addr_lo;
	uint32_t	addr_hi;
	uint32_t	reserved; /* sbz */
} sdhci_v4_adma64_t;

#define SDHCI_ADMA2_VALID	(1 << 0)	// valid
#define SDHCI_ADMA2_END		(1 << 1)	// end of descriptor, transfer complete interrupt will be generated
#define SDHCI_ADMA2_INT		(1 << 2)	// generate DMA interrupt, will not be used
#define SDHCI_ADMA2_NOP		(0 << 4)	// no OP, go to the next desctiptor
#define SDHCI_ADMA2_TRAN	(2 << 4)	// transfer data
#define SDHCI_ADMA2_LINK	(3 << 4)	// link to another descriptor

// extra 32 bit on top of the descriptor for V4 mode
#define SDHCI_ADMA2_64_DESC_SZ(sdhc)	\
		(( (sdhc)->flags & SF_V4_MODE ) ? sizeof( sdhci_v4_adma64_t ) : sizeof( sdhci_adma64_t ))

typedef	struct _sdhci_hc {
	void			*bshdl;
	uintptr_t		base;
#define SF_USE_SDMA		0x01
#define SF_USE_ADMA		0x02
#define SF_TUNE_SDR50	0x04
#define SF_USE_ADMA64	0x08
#define SF_DIS_64BIT	0x10
#define SF_V4_MODE		0x20
#define SF_ICE_PERSIST	0x40
	uint32_t		flags;
	uint32_t		clk_mul;

#define SDCHI_TUNING_MODE_1	0x0
#define SDCHI_TUNING_MODE_2	0x1
#define SDCHI_TUNING_MODE_3	0x2
	int				tuning_mode;

#define ADMA_DESC_MAX		256
	sdio_sge_t		sgl[ADMA_DESC_MAX];
	sdhci_adma64_t	*adma;
	paddr64_t		admap;
} sdhci_hc_t;

extern int sdhci_init( sdio_hc_t *hc );
extern int sdhci_dinit( sdio_hc_t *hc );
extern int sdhci_getsubopt( char **optionp, char * const *tokens, char **valuep );
extern int sdhci_reset( sdio_hc_t *hc, uint32_t rst );

#endif

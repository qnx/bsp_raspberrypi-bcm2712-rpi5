/*
 * $QNXLicenseC:
 * Copyright 2007-2019, QNX Software Systems.
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

#ifndef _SERPL011_EXTERNS_H_INCLUDED_
#define _SERPL011_EXTERNS_H_INCLUDED_

#ifdef DEFN
	#define EXT
	#define INIT1(a)				= { a }
#else
	#define EXT extern
	#define INIT1(a)
#endif

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <termios.h>
#include <devctl.h>
#include <sys/dcmd_chr.h>
#include <sys/iomsg.h>
#include <atomic.h>
#include <hw/inout.h>
#include <sys/io-char.h>
#include <sys/trace.h>
#include <sys/slog.h>
#include <sys/slogcodes.h>

#include <arm/pl011.h>
#include <variant.h>
#include <hw/dma.h>

#define DEFAULT_PRIORITY	24

typedef struct dev_pl011 {
	TTYDEV				tty;
	struct dev_pl011	*next;
	uintptr_t			base;
	unsigned			intr;
	int					port_size;
	int					iid;
	int					fifosize;
	unsigned			clk;
	unsigned			div;
	unsigned			fifo;
	unsigned			baud;
	unsigned			lcr_h;
	unsigned			ibrd;
	unsigned			fbrd;
	unsigned			cr;
	unsigned			imr;
	unsigned			loopback;
	unsigned			drt;
	unsigned			prio;
	unsigned			fifo_reg;
	unsigned			dma_enable;
	unsigned			dma_state;
	unsigned			dma_request_rx;
	unsigned			dma_request_tx;
	unsigned			dma_xfer_size;
	unsigned			chan_rx;
	unsigned			chan_tx;
	unsigned			is_debug_console;

#ifdef USE_DMA
	dma_addr_t			buf_rx[2];
	dma_addr_t			src_rx;
	dma_addr_t			buf_tx;
	dma_addr_t			dst_tx;
	dma_transfer_t		tinfo_tx;
	dma_transfer_t		tinfo_rx;
	int					tx_chid;
	int					tx_coid;
	struct sigevent		event;
	unsigned			buffer0;
	void				*dma_chn_tx;
	void				*dma_chn_rx;
	dma_functions_t		dmafuncs;
	pthread_mutex_t		dma_lock;
#endif
} DEV_PL011;

typedef struct ttyinit_pl011 {
	TTYINIT		tty;
	unsigned	drt;
	unsigned	prio;
	unsigned	loopback;
	unsigned	fifo_reg;
	unsigned	dma_enable;
	unsigned	dma_request_rx;
	unsigned	dma_request_tx;
	unsigned	dma_xfer_size;
	unsigned	chan_rx;
	unsigned	chan_tx;
	unsigned	is_debug_console;
} TTYINIT_PL011;

EXT TTYCTRL				ttyctrl;

#include "proto.h"

#endif  /* _SERPL011_EXTERNS_H_INCLUDED_ */

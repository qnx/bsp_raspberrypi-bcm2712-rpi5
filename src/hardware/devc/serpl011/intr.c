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

#include "externs.h"

#define	PL011_RX_ERROR (PL011_DR_OE | PL011_DR_BE | PL011_DR_PE | PL011_DR_FE)

#ifdef USE_DMA
static int errCnt;

void *timer_thread (void *data)
{

	timer_t				timer_id;
	struct itimerspec	itime;
	struct sigevent		timer_event;
	struct _pulse		pulse;
	iov_t				iov;
	int					rcvid;
	int					chid, coid;
	DEV_PL011			*dev = data;

	if ((chid = ChannelCreate(_NTO_CHF_DISCONNECT | _NTO_CHF_UNBLOCK)) == -1 ||
		(coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0)) == -1)
	{
		fprintf(stderr, "Channel create failed\n");
		return NULL;
	}

	SIGEV_PULSE_INIT (&timer_event, coid, dev->prio, TIMER_PULSE, NULL);
	if(timer_create(CLOCK_MONOTONIC, &timer_event, &timer_id)==-1){
		fprintf(stderr, "Timer create failed");
		return NULL;
	}
	itime.it_value.tv_sec = 0;
	itime.it_value.tv_nsec = dev->drt *1000000;
	itime.it_interval.tv_sec = 0;
	itime.it_interval.tv_nsec = dev->drt *1000000;
	timer_settime(timer_id, 0, &itime, NULL);

	SETIOV(&iov, &pulse, sizeof(pulse));

	for (;;) {
		if ((rcvid = MsgReceivev(chid, &iov, 1, NULL)) == -1)
			continue;
		switch (pulse.code) {
			case TIMER_PULSE:
#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 199, read_pl011(dev, PL011_MIS), read_pl011(dev, PL011_FR));
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 200, dev->tty.ibuf.cnt, dev->tty.obuf.cnt);
#endif
				/* Data ready time out */
				if (rx_interrupt(dev)){
					iochar_send_event(&dev->tty);
				}
				if(errCnt>0){
					slogf (_SLOG_SETCODE (_SLOGC_CHAR, 0), _SLOG_ERROR,
						"%s: Number of times the DMA_ERIS returns error (%d)", __FUNCTION__, errCnt);
					errCnt = 0;
				}
				break;
			default:
				break;
		}
	}

	return NULL;
}

void *tx_dma_thread (void *data)
{
	struct _pulse		pulse;
	iov_t				iov;
	int					rcvid;
	DEV_PL011			*dev = data;

	SETIOV(&iov, &pulse, sizeof(pulse));

	for (;;) {
		if ((rcvid = MsgReceivev(dev->tx_chid, &iov, 1, NULL)) == -1)
			continue;
		switch (pulse.code) {
			case TX_DMA_PULSE:
#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 399, read_pl011(dev, PL011_MIS), read_pl011(dev, PL011_FR));
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 400, dev->tty.ibuf.cnt, dev->tty.obuf.cnt);
#endif
				tx_interrupt(dev);
				break;
			default:
				break;
		}
	}
	return NULL;
}

#endif /* USE_DMA */

int tx_interrupt(DEV_PL011 *dev)
{
	dev->tty.un.s.tx_tmr = 0;	/* clear Timeout */

#ifdef USE_DMA
	if((dev->dma_enable & DMA_TX_ENABLE) && (dev->dma_state & DMA_TX_ACTIVE)){
		int nbytes=-1;
		if(dev->dmafuncs.xfer_complete(dev->dma_chn_tx)==1){
			nbytes = 0;
			atomic_clr(&dev->dma_state, DMA_TX_ACTIVE);
		}else{
			unsigned char *buf = (unsigned char *)dev->buf_tx.vaddr;
			int		xlen = dev->buf_tx.len;
			nbytes = dev->dmafuncs.bytes_left(dev->dma_chn_tx);
			if(nbytes>0){
				memmove(buf, buf+(xlen-nbytes), nbytes);
				dev->dst_tx.len = dev->buf_tx.len = nbytes;
				dev->tinfo_tx.xfer_bytes= nbytes;
				if(dev->dmafuncs.setup_xfer (dev->dma_chn_tx, &dev->tinfo_tx)==-1){
					//error starting DMA?
#ifdef MDEBUG
					TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 271, 0, dev->tinfo_tx.xfer_bytes);
#endif
				}
				dev->dmafuncs.xfer_start(dev->dma_chn_tx);
			}else{
				atomic_clr(&dev->dma_state, DMA_TX_ACTIVE);
			}
		}
#ifdef MDEBUG
		TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 201, 0, nbytes);
#endif
	}
#endif /* USE_DMA */

	return (tto(&dev->tty, TTO_DATA, 0x80));
}

int rx_interrupt(DEV_PL011 *dev)
{
	unsigned	key=0, rsr;
	int			status = 0;
	int			cnt = 0;

#ifdef USE_DMA
	unsigned char *buf;
	int		buffer0;
	if(dev->dma_enable & DMA_RX_ENABLE){
		pthread_mutex_lock (&dev->dma_lock);
		if(dev->dma_state & DMA_RX_ACTIVE){
			int nbytes=-1;
			int ret = 0;
			buffer0 = dev->buffer0;
			atomic_clr(&dev->dma_state, DMA_RX_ACTIVE);
			if(!(dev->dma_state & DMA_RX_STOP)){
				atomic_set(&dev->dma_state, DMA_RX_STOP);
				ret = dev->dmafuncs.xfer_complete(dev->dma_chn_rx);
				if(ret == -1){
					key = TTI_OVERRUN;
					errCnt++;
				}else{
					if(ret == 1){
						nbytes = dev->dma_xfer_size;
					}else{
						nbytes = dev->dma_xfer_size - dev->dmafuncs.bytes_left(dev->dma_chn_rx);
					}
				}
				rsr = read_pl011(dev, PL011_RSR) & 0xF;
				if (rsr || key) {
					write_pl011(dev, PL011_ECR, 0);

					/*
					 * Save error as out-of-band data that can be read by devctl()
					 */
					dev->tty.oband_data |= rsr ;
					atomic_set(&dev->tty.flags, OBAND_DATA);
					if (rsr & PL011_RSR_OE)
						key |= TTI_OVERRUN;
					else if (key & PL011_RSR_BE)
						key |= TTI_BREAK;
					else if (key & PL011_RSR_PE)
						key |= TTI_PARITY;
					else if (key & PL011_RSR_FE)
						key |= TTI_FRAME;

				}

#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 202, nbytes, rsr);
#endif
				cnt = nbytes;
				buf =(buffer0)?((unsigned char *)dev->buf_rx[0].vaddr):((unsigned char *)dev->buf_rx[1].vaddr);
				if(nbytes>0 && nbytes<=dev->dma_xfer_size){
					status |= tti2 (&dev->tty, buf, nbytes, key);
				}else if(key){
					status |= tti (&dev->tty, key);
				}
#ifdef MDEBUG
				//log the begining of the 2 bytes and end of 2 bytes
				if(nbytes>0)
					TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 400, buf[0], buf[1]);
				if(nbytes>2)
					TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 401, buf[nbytes-2], buf[nbytes-1]);
#endif
#ifdef MDEBUG
{
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 203, nbytes,dev->tty.ibuf.cnt);
}
#endif
			}

			if(!(dev->tty.flags & IHW_PAGED)){
				//restart the new transfer before handling stop
				dev->tinfo_rx.dst_addrs = &dev->buf_rx[buffer0];
				if(dev->dmafuncs.setup_xfer (dev->dma_chn_rx, &dev->tinfo_rx)==-1){
#ifdef MDEBUG
					TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 202, cnt, 0);
#endif
				}

				dev->dmafuncs.xfer_start (dev->dma_chn_rx);
				nanospin_ns(200);
				dev->buffer0 ^= 1;
				atomic_clr(&dev->dma_state, DMA_RX_STOP);
#ifdef MDEBUG
				{
					TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 204, nbytes,dev->tty.ibuf.cnt);
				}
#endif
			}
			atomic_set(&dev->dma_state, DMA_RX_ACTIVE);
		}

#ifdef MDEBUG
//		TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 205, dev->tty.flags, dev->tty.ibuf.cnt);
		TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 206, dev->tty.c_cflag, status);
#endif
		pthread_mutex_unlock (&dev->dma_lock);
		return status;
	}
#endif
	while ((read_pl011(dev, PL011_FR) & PL011_FR_RXFE) == 0){
		key = read_pl011(dev, PL011_DR);
		rsr = key & PL011_RX_ERROR;
		key &= 0xFF;

		if (rsr != 0) {
			write_pl011(dev, PL011_ECR, 0);

			/*
			 * Save error as out-of-band data that can be read by devctl()
			 */
			dev->tty.oband_data |= rsr >> 8;
			atomic_set(&dev->tty.flags, OBAND_DATA);

			if (rsr & PL011_DR_OE)
				key |= TTI_OVERRUN;
			else if (rsr & PL011_DR_BE)
				key |= TTI_BREAK;
			else if (rsr & PL011_DR_PE)
				key |= TTI_PARITY;
			else if (rsr & PL011_DR_FE)
				key |= TTI_FRAME;
		}
		status |= tti(&dev->tty, key);
		cnt++;
	}

#ifdef MDEBUG
	TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 204, cnt, status);
#endif
	return status;
}

static int do_interrupt(DEV_PL011 *dev)
{
	int			status = 0;
	unsigned	iir;

	iir = read_pl011(dev, PL011_MIS);
#ifdef MDEBUG
	TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 208, iir, read_pl011(dev, PL011_IMSC));
#endif

	write_pl011(dev, PL011_ICR, iir);
#ifdef USE_DMA
	if (!(dev->dma_enable & DMA_RX_ENABLE) && (iir & (PL011_MIS_RTMIS | PL011_MIS_RXMIS))){
		status |= rx_interrupt(dev);
	}
#else
	if (iir & (PL011_MIS_RTMIS | PL011_MIS_RXMIS)){
		status |= rx_interrupt(dev);
	}
#endif
	if (iir & (PL011_MIS_TXMIS|PL011_MIS_TXFES)){
		status |= tx_interrupt(dev);
	}

	return status;
}
/*
 * Serial interrupt event handler
 */
static int interrupt_event_handler(message_context_t *msgctp, int code, unsigned flags, void *handle)
{
	DEV_PL011	*dev = handle;
	int status = EOK;

	status = do_interrupt(dev);

	/* Interrupt event handler does not return a tte event,
	 * so we must issue the tte pulse manually
	 */
	if(status) {
		iochar_send_event(&dev->tty);
	}

	variant_intr_unmask();

	InterruptUnmask(dev->intr, dev->iid);

	return (EOK);
}

void
ser_attach_intr(DEV_PL011 *dev)
{
	int sigevcode;
	struct sigevent event;

	/*
	 * Disable the UART and clear interrupts
	 */
	if(!dev->loopback) {
		while (read_pl011(dev, PL011_FR) & PL011_FR_BUSY)
			;
	}

	variant_intr_init(dev);

	write_pl011(dev, PL011_ICR, 0x7FF);
	/* Associate a pulse which will call the event handler. */
	if((sigevcode = pulse_attach(ttyctrl.dpp,
			MSG_FLAG_ALLOC_PULSE, 0, &interrupt_event_handler, dev)) == -1) {
		slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_ERROR,
				"io-char: Unable to attach event pulse(%d)", errno);
		return;
	}

	SIGEV_PULSE_INIT(&event, ttyctrl.coid, dev->prio, sigevcode, 0);
	dev->iid = InterruptAttachEvent(dev->intr, &event, _NTO_INTR_FLAGS_TRK_MSK);
	if (dev->iid == -1) {
		slogf(_SLOG_SETCODE(_SLOGC_CHAR, 0), _SLOG_ERROR,
				"Unable to attach InterruptEvent: %s", strerror(errno));
	}
}

void
ser_detach_intr(DEV_PL011 *dev)
{
	/*
	 * Disable the UART and clear interrupts
	 */
	while (read_pl011(dev, PL011_FR) & PL011_FR_BUSY)
		;
	write_pl011(dev, PL011_CR, 0);
	write_pl011(dev, PL011_ICR, 0x7FF);

	InterruptDetach(dev->iid);
	dev->intr = _NTO_INTR_SPARE;
}

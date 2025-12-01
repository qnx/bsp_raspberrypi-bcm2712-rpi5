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

int
tto(TTYDEV *ttydev, int action, int arg1)
{
	TTYBUF			*bup = &ttydev->obuf;
	DEV_PL011		*dev = (DEV_PL011 *)ttydev;
	unsigned		reg=0, status = 0;
	unsigned char	c;
	int				bytes=0;
#ifdef USE_DMA
	static uint32_t byte_cnt = 0;
#endif

	switch (action) {
		case TTO_STTY:
			ser_stty(dev);
#ifdef MDEBUG
			TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 100, arg1, 0);
#endif
			return 0;

		case TTO_CTRL:
			if (arg1 & _SERCTL_BRK_CHG) {
				reg = read_pl011(dev, PL011_LCR_H);
				if (arg1 &_SERCTL_BRK)
					reg |= PL011_LCR_H_BRK;
				else
					reg &= ~PL011_LCR_H_BRK;
				write_pl011(dev, PL011_LCR_H, reg);
#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 220, arg1, reg);
#endif
			}
			if (arg1 & _SERCTL_DTR_CHG) {
				reg = read_pl011(dev, PL011_CR);

				if (arg1 & _SERCTL_DTR)
					reg |= PL011_CR_DTR;
				else
					reg &= ~PL011_CR_DTR;

				write_pl011(dev, PL011_CR, reg);
#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 221, arg1, reg);
#endif
			}
			if ((arg1 & _SERCTL_RTS_CHG) && (dev->tty.c_cflag & IHFLOW)) {
#ifdef NOAUTO_RTS
				reg = read_pl011(dev, PL011_CR);

				if (arg1 & _SERCTL_RTS)
					reg |= PL011_CR_RTS;
				else
					reg &= ~PL011_CR_RTS;
				write_pl011(dev, PL011_CR, reg);

#endif

#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 222, arg1, reg);
#endif
			}
#ifdef MDEBUG
			TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 223, arg1, dev->tty.c_cflag);
#endif

			return 0;

		case TTO_LINESTATUS:
			reg = read_pl011(dev, PL011_FR);
			if (dev->tty.c_cflag & IHFLOW)
				if (reg & PL011_FR_DSR)
					status |= _LINESTATUS_SER_RTS;
			if (dev->tty.c_cflag & OHFLOW)
				if (reg & PL011_FR_CTS)
					status |= _LINESTATUS_SER_CTS;
			if (reg & PL011_FR_DCD)
				status |= _LINESTATUS_SER_CD;
#ifdef MDEBUG
			TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 225, reg, dev->tty.c_cflag);
#endif
			return status;

		case TTO_DATA:	/* FALLTHROUGH */
		case TTO_EVENT:
			break;

		default:
			return 0;
	}
#ifdef USE_DMA
	if(dev->dma_enable & DMA_TX_ENABLE){
		unsigned char *buf = (unsigned char *)dev->buf_tx.vaddr;
		/* DMA Transaction in progress, wait for ENDTX interrupt */
		if (dev->dma_state & DMA_TX_ACTIVE){
			dev->tty.un.s.tx_tmr = 3;		/* Timeout 3 */
			return 0;
		}

		while (bup->cnt > 0 && byte_cnt < dev->dma_xfer_size){
			/*
			 * If the OSW_PAGED_OVERRIDE flag is set then allow
			 * transmit of character even if output is suspended via
			 * the OSW_PAGED flag. This flag implies that the next
			 * character in the obuf is a software flow control
			 * charater (STOP/START).
			 * Note: tx_inject sets it up so that the contol
			 *       character is at the start (tail) of the buffer.
			 */
			if (dev->tty.flags & (OHW_PAGED | OSW_PAGED) && !(dev->tty.xflags & OSW_PAGED_OVERRIDE))
				break;

			if (dev->tty.c_oflag & OPOST || dev->tty.xflags & OSW_PAGED_OVERRIDE)
			{
				/*
				 * Get the next character to print from the output buffer
				 */
				dev_lock (&dev->tty);
				c = tto_getchar (&dev->tty);
				dev_unlock (&dev->tty);

				buf[byte_cnt++] = c;

				/*
				 * Clear the OSW_PAGED_OVERRIDE flag as we only want
				 * one character to be transmitted in this case.
				 */
				if (dev->tty.xflags & OSW_PAGED_OVERRIDE)
					atomic_clr (&dev->tty.xflags, OSW_PAGED_OVERRIDE);
			}
			else
			{
				int	buf_n, first_pass;

				buf_n = min (bup->cnt, dev->dma_xfer_size - byte_cnt);
				first_pass = &bup->buff[bup->size] - bup->tail;
				if (buf_n <= first_pass)
				{
					memcpy (buf + byte_cnt, bup->tail, buf_n);
					bup->tail += buf_n;
					if (bup->tail == &bup->buff[bup->size])
						bup->tail = bup->buff;
				}
				else
				{
					memcpy (buf + byte_cnt, bup->tail, first_pass);
					memcpy (buf + byte_cnt + first_pass, bup->buff, buf_n - first_pass);
					bup->tail = bup->buff + (buf_n - first_pass);
				}
				bup->cnt -= buf_n;
				byte_cnt += buf_n;
			}
		}

		if (byte_cnt && !(dev->tty.flags & (OHW_PAGED | OSW_PAGED)))
		{
			atomic_set(&dev->dma_state, DMA_TX_ACTIVE);
			/* Configure DMA buffer address and transfer size */
			dev->dst_tx.len = dev->buf_tx.len = byte_cnt;
			dev->tinfo_tx.xfer_bytes= byte_cnt;
			byte_cnt = 0;

#ifdef MDEBUG
			TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 230, dev->tinfo_tx.xfer_bytes, bup->cnt);
#endif
			if(dev->dmafuncs.setup_xfer (dev->dma_chn_tx, &dev->tinfo_tx)==-1){
				//error starting DMA?
#ifdef MDEBUG
				TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 231, dev->tinfo_tx.xfer_bytes, bup->cnt);
#endif
			}
			dev->dmafuncs.xfer_start(dev->dma_chn_tx);
#if 0
			if ((dev->imr & PL011_IMSC_TXIM) == 0) {
				nanospin_ns(1000);
				dev->imr |= PL011_IMSC_TXIM;
				write_pl011(dev, PL011_ICR, PL011_IMSC_TXIM);
				write_pl011(dev, PL011_IMSC, dev->imr);
			}
#endif
		}else{
			if (dev->imr & PL011_IMSC_TXIM) {
				dev->imr &= ~PL011_IMSC_TXIM;
				write_pl011(dev, PL011_IMSC, dev->imr);
			}
		}
		/* Check client lists for notify conditions */
		 return (tto_checkclients(&dev->tty));
	}
#endif

	/*
	 * If the OSW_PAGED_OVERRIDE flag is set then allow
	 * transmit of character even if output is suspended via
	 * the OSW_PAGED flag. This flag implies that the next
	 * character in the obuf is a software flow control
	 * charater (STOP/START).
	 * Note: tx_inject sets it up so that the contol
	 *       character is at the start (tail) of the buffer.
	 *
	 */
	if (dev->tty.flags & (OHW_PAGED|OSW_PAGED) && !(dev->tty.xflags & OSW_PAGED_OVERRIDE)){
#ifdef MDEBUG
		TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 230, dev->tty.flags, dev->tty.xflags);
#endif
		return 0;
	}

	while (((read_pl011(dev, PL011_FR) & PL011_FR_TXFF) == 0) && (bup->cnt > 0) && bytes++<1024) {
		unsigned int error;
		/*
		 * Get the next character to print from the output buffer
		 */
		dev_lock(&dev->tty);
		c = tto_getchar(&dev->tty);
		dev_unlock(&dev->tty);

		dev->tty.un.s.tx_tmr = iochar_tick_cnt(150, &error);   /* Timeout 150ms */
		if (error > TICKSIZE_ERR_TOLERANCE) {
			slogf (_SLOG_SETCODE (_SLOGC_CHAR, 0), _SLOG_WARNING,
					"%s: %s TX timer deviation = %u ms", __func__, dev->tty.name, error);
		}

		write_pl011(dev, PL011_DR, c);
		/* Clear the OSW_PAGED_OVERRIDE flag as we only want
		 * one character to be transmitted in this case.
		 */
		if (dev->tty.xflags & OSW_PAGED_OVERRIDE) {
			atomic_clr(&dev->tty.xflags, OSW_PAGED_OVERRIDE);
			break;
		}
	}
	if (bup->cnt > 0) {
		if ((dev->imr & PL011_IMSC_TXIM) == 0) {
			dev->imr |= PL011_IMSC_TXIM;
			write_pl011(dev, PL011_IMSC, dev->imr);
		}
	}
	else {
		if (dev->imr & PL011_IMSC_TXIM) {
			dev->imr &= ~PL011_IMSC_TXIM;
			write_pl011(dev, PL011_IMSC, dev->imr);
		}
	}
#ifdef MDEBUG
	TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 231, bytes, bup->cnt);
#endif

	/* Check client lists for notify conditions */
	return (tto_checkclients(&dev->tty));
}

void ser_stty(DEV_PL011 *dev)
{
	unsigned	lcr_h = PL011_LCR_H_FEN;
	unsigned	cr = PL011_CR_RXE | PL011_CR_TXE | PL011_CR_UARTEN;
	unsigned	ibrd, fbrd;

	if (dev->tty.c_cflag & OHFLOW){
		cr |= PL011_CR_CTSEn;
	}

	if (dev->tty.c_cflag & IHFLOW)
		cr |= PL011_CR_RTSEn;

	if(dev->div==8){
		cr |=PL011_CR_OVSFACT;
	}

	if(dev->loopback){
		cr |= PL011_CR_LBE;
	}

	switch (dev->tty.c_cflag & CSIZE) {
		case CS8: lcr_h |= PL011_LCR_H_WLEN8; break;
		case CS7: lcr_h |= PL011_LCR_H_WLEN7; break;
		case CS6: lcr_h |= PL011_LCR_H_WLEN6; break;
		case CS5: lcr_h |= PL011_LCR_H_WLEN5; break;
	}

	if (dev->tty.c_cflag & CSTOPB)
		lcr_h |= PL011_LCR_H_STP2;

	if (dev->tty.c_cflag & PARENB) {
		lcr_h |= PL011_LCR_H_PEN;
		if ((dev->tty.c_cflag & PARODD) == 0) {
			lcr_h |= PL011_LCR_H_EPS;
		}
	}
#ifdef MDEBUG
{
	TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, 226, dev->tty.baud, dev->tty.c_cflag);
}
#endif

	ibrd = dev->ibrd;
	fbrd = dev->fbrd;

	if (dev->baud != dev->tty.baud) {
		unsigned clock = dev->clk / dev->div;

		dev->baud = dev->tty.baud;

		ibrd = clock / dev->baud;
		fbrd = (((clock % dev->baud) * 64) + ((dev->baud + 1) >> 1)) / dev->baud;
	}

	if ((cr == dev->cr) && (lcr_h == dev->lcr_h) && (ibrd == dev->ibrd) && (fbrd == dev->fbrd))
		return;

	dev->imr = PL011_IMSC_RXIM | PL011_IMSC_RTIM;
	dev->lcr_h = lcr_h;
	dev->ibrd  = ibrd;
	dev->fbrd  = fbrd;
	dev->cr    = cr;

	if(!dev->loopback){
		int t = 10000000;
		while ((read_pl011(dev, PL011_FR) & PL011_FR_BUSY) && t--)
			;
		if(t<=0){
			printf("timeout waiting for chip ready %x\n", read_pl011(dev, PL011_FR));
		}
	}
	write_pl011(dev, PL011_CR, 0);			/* Disable UART */
	write_pl011(dev, PL011_DMACR, 0);		/* Disable DMA */
	write_pl011(dev, PL011_LCR_H, read_pl011(dev, PL011_LCR_H) & ~PL011_LCR_H_FEN); /* Flush FIFO */

	if(dev->fifosize && dev->fifo){
		write_pl011(dev, PL011_IFLS, dev->fifo);	/* set fifo trigger level */
		write_pl011(dev, PL011_TIMEOUT, 0x1ff);	/* set timeout */
	}

#ifdef USE_DMA
	if(dev->dma_enable){
		unsigned trigger=0, dma_en=0;

		//start RX DMA
		if((dev->dma_enable & DMA_RX_ENABLE) ){
			pthread_mutex_lock (&dev->dma_lock);
			if(dev->dma_state & DMA_RX_ACTIVE){
				atomic_clr(&dev->dma_state, DMA_RX_ACTIVE);
				dev->dmafuncs.xfer_complete(dev->dma_chn_rx);
			}
			dev->src_rx.paddr = dev->fifo_reg;
			dev->src_rx.len = dev->dma_xfer_size;
			dev->tinfo_rx.src_addrs = &dev->src_rx;
			dev->tinfo_rx.dst_addrs = &dev->buf_rx[0];
			dev->buffer0	= 1;
			dev->buf_rx[0].len = dev->dma_xfer_size;
			dev->buf_rx[1].len = dev->dma_xfer_size;
			dev->tinfo_rx.xfer_bytes= dev->dma_xfer_size;
			if(dev->dmafuncs.setup_xfer (dev->dma_chn_rx, &dev->tinfo_rx)==-1){
				printf("setup rx dma failed, errno %d\n", errno);
			}
			trigger |= 0x2<<3; /* 4 byte trigger */
			dma_en |= PL011_DMACR_RXE;
			dev->dmafuncs.xfer_start(dev->dma_chn_rx);
			nanospin_ns(200);
			atomic_set(&dev->dma_state, DMA_RX_ACTIVE);
			pthread_mutex_unlock (&dev->dma_lock);
		}

		if(dev->dma_enable & DMA_TX_ENABLE){
			trigger |= 0x2; /* 4 byte trigger */
			dma_en |= PL011_DMACR_TXE;
		}
		write_pl011(dev, PL011_DMAWM, trigger);
		//write_pl011(dev, PL011_DMAWM, 0x12);
		write_pl011(dev, PL011_DMACR, dma_en);
	}
#endif
	write_pl011(dev, PL011_ICR, 0x7FF);		/* Clear all status */
	write_pl011(dev, PL011_IBRD, ibrd);
	write_pl011(dev, PL011_FBRD, fbrd);
	write_pl011(dev, PL011_LCR_H, lcr_h);
#ifdef USE_DMA
	if((dev->dma_enable & DMA_RX_ENABLE) ){
		dev->imr = 0;
	}
	write_pl011(dev, PL011_LCR_R, lcr_h);
#endif
	write_pl011(dev, PL011_CR, cr);
	/*
	 * Enable Rx interrupts. Tx interrupt is managed via tto().
	 */
	write_pl011(dev, PL011_IMSC, dev->imr);
}

int drain_check(TTYDEV *ttydev, uintptr_t *count) {
	TTYBUF			*bup = &ttydev->obuf;
	DEV_PL011		*dev = (DEV_PL011 *)ttydev;

	// if the device has DRAINED, return 1
	if ((bup->cnt == 0) && (read_pl011(dev, PL011_FR) & PL011_FR_TXFE)){
		return 1;
	}

	// if the device has not DRAINED, set a timer based on 50ms counts
	// wait for the time it takes for one character to be transmitted
	// out the shift register.  We do this dynamically since the
	// baud rate can change.
	if (count != NULL)
		*count = (ttydev->baud == 0) ? 0 : ((IO_CHAR_DEFAULT_BITSIZE * 20) / ttydev->baud) + 1;
	return 0;
}

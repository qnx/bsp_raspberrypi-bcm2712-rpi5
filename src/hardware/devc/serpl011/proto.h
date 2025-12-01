/*
 * $QNXLicenseC:
 * Copyright 2007-2020, 2023, BlackBerry Limited.
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

void create_device(TTYINIT_PL011 *dip);
void ser_stty(DEV_PL011 *dev);
void ser_attach_intr(DEV_PL011 *dev);
void ser_detach_intr(DEV_PL011 *dev);
void write_port(DEV_PL011 *dev, int reg, unsigned val);
unsigned read_port(DEV_PL011 *dev, int reg);
int rx_interrupt(DEV_PL011 *dev);
int tx_interrupt(DEV_PL011 *dev);
void *timer_thread (void *data);
void *tx_dma_thread (void *data);
void *query_default_device(TTYINIT_PL011 *dip, void *link);
int options(int argc, char *argv[]);

void variant_intr_init(DEV_PL011 *dev);
void variant_intr_unmask(void);
int enable_perms(void);

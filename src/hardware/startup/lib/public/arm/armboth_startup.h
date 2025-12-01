/*
 * Copyright 2016,2022, BlackBerry Limited.
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

int     fdt_qtime(unsigned *freqp, unsigned *intrp);
int     fdt_psci_configure(void);

extern struct callout_rtn	sendipi_gic_v2;

/*
 * ------------------------------------------------------------------
 * PSCI reboot callout support
 * ------------------------------------------------------------------
 */
extern struct callout_rtn	reboot_psci_hvc;
extern struct callout_rtn	reboot_psci_smc;

/*
 * ------------------------------------------------------------------
 * PSCI Support
 * ------------------------------------------------------------------
 */
int psci_smp_start(unsigned cpu, void (*start)(void));
_Uint64t psci_cpu_id(unsigned cpu);
extern int psci_cpu_on_cmd;

/*
 * ------------------------------------------------------------------
 * PrimeCell PL011 UART support
 * ------------------------------------------------------------------
 */
extern void init_pl011(unsigned, const char *, const char *);
extern void put_pl011(int);

extern struct callout_rtn	display_char_pl011;
extern struct callout_rtn	poll_key_pl011;
extern struct callout_rtn	break_detect_pl011;

/*
 * ------------------------------------------------------------------
 * support routines for boards that start all their CPU's right away at
 * the startup entry address
 * ------------------------------------------------------------------
 */

extern unsigned long spin_bootstrap_id;
unsigned spin_smp_num_cpu(void);
int spin_smp_start(unsigned cpu, void (*start)(void));

extern unsigned is_uefi_boot(void);

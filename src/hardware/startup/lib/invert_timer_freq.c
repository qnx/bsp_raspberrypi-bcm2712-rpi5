/*
 * $QNXLicenseC:
 * Copyright 2008, QNX Software Systems. 
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

//
// Convert the timer frequency (given in Hz) into a timer period and
// store the result into the qtime structure with the highest possible
// precision. This code is system independent and should not have to
// be changed by end users.
//
void
invert_timer_freq(struct qtime_entry *qtime, unsigned freq) {
	int			scale;
	unsigned	period;
	unsigned	rem;
	unsigned	div;
	unsigned	quot;

	if(freq == 0) {
		crash("Timer frequency is zero\n");
	}
	quot = 1;
	scale = 0;
	period = 0;

	/* to ensure (quot = rem * 10) doesn't overflow ... */
	if (freq > (UINT_MAX/10)) {
		--scale;
		freq /= 10;
	}

	for(;;) {
		div = quot / freq;
		period = period * 10 + div;
		if(period >= ((UINT_MAX/10) - 10)) break;
		rem = quot % freq;
		if(rem == 0) break;
		--scale;
		quot = rem * 10;
	} 
	qtime->timer_period = period;
	qtime->timer_scale = scale;
}

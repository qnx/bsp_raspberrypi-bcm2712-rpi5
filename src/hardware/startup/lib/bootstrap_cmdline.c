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
 * Copyright 2008,2025, BlackBerry Limited.
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
 * Only a caller that owns spare bytes may enable growth.  The default is the
 * advertised used size, which keeps legacy/foreign bootstrap blocks no-growth.
 */
static struct bootargs_entry *capacity_bap;
static size_t capacity_bytes;

static size_t
bootargs_used_size(const struct bootargs_entry *bap) {
	return (size_t)bap->size_lo + ((size_t)bap->size_hi << 8);
}

static size_t
bootargs_capacity(const struct bootargs_entry *bap) {
	return (bap == capacity_bap) ? capacity_bytes : bootargs_used_size(bap);
}

int
bootstrap_args_validate(const struct bootargs_entry *bap, size_t capacity) {
	size_t used = bootargs_used_size(bap);
	const char *p;
	const char *end;
	unsigned count;

	if ((used < offsetof(struct bootargs_entry, args)) || (used > capacity) ||
			(bap->argc < 0) || (bap->envc < 0)) {
		return 0;
	}
	p = bap->args;
	end = (const char *)bap + used;
	count = (unsigned)bap->argc + (unsigned)bap->envc;
	while (count-- != 0U) {
		size_t len;

		if (p >= end) return 0;
		len = strnlen(p, (size_t)(end - p));
		if (len == (size_t)(end - p)) return 0;
		p += len + 1U;
	}
	return 1;
}

void
tweak_cmdline_with_capacity(struct bootargs_entry *bap, const char *name,
		size_t capacity) {
	struct bootargs_entry *saved_bap = capacity_bap;
	size_t saved_capacity = capacity_bytes;

	capacity_bap = bap;
	capacity_bytes = capacity;
	tweak_cmdline(bap, name);
	capacity_bap = saved_bap;
	capacity_bytes = saved_capacity;
}

static int
do_adjust(struct bootargs_entry *bap, const char *old, const char *new, 
				unsigned start, int8_t *countp, size_t capacity) {
	unsigned 		total_len;
	unsigned 		add_len;
	unsigned 		sub_len;
	int				diff;
	char			*end;
	char			*p;
	unsigned		num;
	size_t			len;
	size_t			new_len;

	total_len = (unsigned)bootargs_used_size(bap);
	if (capacity > UINT16_MAX) {
		capacity = UINT16_MAX;
	}
	/* Prove every advertised string is contained before walking or moving it. */
	if (!bootstrap_args_validate(bap, capacity)) {
		return 0;
	}
	num = (unsigned)*countp;
	end = (char *)bap + total_len;

	p = bap->args;
	while(start != 0) {
		if (p >= end) return 0;
		len = strnlen(p, (size_t)(end - p));
		if (len == (size_t)(end - p)) return 0;
		p += len + 1;
		--start;
	};
	while(num != 0) {
		if (p >= end) return 0;
		len = strnlen(p, (size_t)(end - p));
		if (len == (size_t)(end - p)) return 0;
		sub_len = (unsigned)len + 1;
		if((old != NULL) && (strcmp(old, p) == 0)) goto do_operation;
		p += sub_len;
		--num;
	}
	if(old == NULL) {
		// adding new one
		sub_len = 0;
		goto do_operation;
	}
	return 0;

do_operation:		
	if(new == NULL) {
		// deleting entry
		add_len = 0;
	} else { 
		// adding/replacing entry
		new_len = strnlen(new, capacity);
		if (new_len >= capacity) {
			return 0;
		}
		add_len = (unsigned)new_len + 1U;
	}
	diff = (int)add_len - (int)sub_len;
	/* Growth is legal only inside capacity explicitly owned by this object. */
	if ((diff > 0) && ((size_t)diff > (capacity - total_len))) {
		return 0;
	}
	if ((old == NULL) && (*countp == INT8_MAX)) {
		return 0;
	}

	/* Move the tail between the old and new slots without reading before args. */
	memmove(p + add_len, p + sub_len, (size_t)(end - (p + sub_len)));
	if(add_len > 0U) {
		memcpy(p, new, add_len);
	}
	if (old == NULL) {
		*countp += 1;
	}
	else if (new == NULL) {
		*countp -= 1;
	}
	total_len = (unsigned)((int)total_len + diff);
	bap->size_lo = total_len & 0xff;
	bap->size_hi = total_len >> 8;
	return 1;
}


int
bootstrap_arg_adjust(struct bootargs_entry *bap, const char *old, const char *new) {
	return do_adjust(bap, old, new, 0, &bap->argc, bootargs_capacity(bap));
}


int
bootstrap_env_adjust(struct bootargs_entry *bap, const char *old, const char *new) {
	return do_adjust(bap, old, new, bap->argc, &bap->envc, bootargs_capacity(bap));
}

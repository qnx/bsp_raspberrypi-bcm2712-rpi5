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

static void	(*print_char)(int) = dummy_print_char;

static void
one_char(char c) {
	if(c == '\n') print_char('\r');
	print_char(c);
}

const static char	c[] = "0123456789abcdef";

static void
vmsg(const char *fmt, va_list args) {
	char 			*vs;
	uint64_t		num;
	unsigned		radix;
	int				dig;
	char			buf[64];
	
	for(; *fmt; ++fmt) {
		if(*fmt != '%') {
			one_char(*fmt);
			continue;
		}
		radix = dig = 0;
		switch(*++fmt) {
		case 'b':
			num = va_arg(args, unsigned);
			dig = sizeof(uint8_t)*2;
			radix = 16;
			break;
		case 'w':
			num = va_arg(args, unsigned);
			dig = sizeof(uint16_t)*2;
			radix = 16;
			break;
		case 'P':
			num = va_arg(args, paddr_t);
			dig = sizeof(paddr_t)*2;
			radix = 16;
			break;
		case 'x':
			num = va_arg(args, unsigned);
			dig = sizeof(unsigned)*2;
			radix = 16;
			break;
		case 'l':
			num = va_arg(args, unsigned long);
			dig = sizeof(unsigned long)*2;
			radix = 16;
			break;
		case 'L':
			num = va_arg(args, uint64_t);
			dig = sizeof(uint64_t)*2;
			radix = 16;
			break;
		case 'v':/* abbreviated hex */
			num = va_arg(args, uintptr_t);
			dig = sizeof(uintptr_t)*2;
			radix = 16;
			break;
		case 'd':
		case 'u':
			num = va_arg(args, unsigned);
			radix = 10;
			break;
		case 's':
			vs = va_arg(args, char *);
			while(*vs) {
				one_char(*vs++);
			}
			continue;
		default:
			one_char(*fmt);
			continue;
		}
		vs = &buf[sizeof(buf)];

		*--vs = '\0';
		do {
			*--vs = c[num % radix];
			num /= radix;
		} while(num);
		for(dig -= &buf[sizeof(buf)-1] - vs; dig > 0; --dig) {
			one_char('0');
		}
		while(*vs) {
			one_char(*vs++);
		}
	}
}

void
kprintf(const char *fmt, ...) {
	va_list args;
	
	va_start(args, fmt);
	vmsg(fmt, args);
	va_end(args);
}	

static char *string_ptr;
static size_t ksnprinf_len = 0;
static size_t ksnprintf_max_len = 0;

static void
string_print_char(int c) {
	*string_ptr++ = c;
}

static void
string_print_n_char(const int ch) {
	if(ksnprinf_len < ksnprintf_max_len) {
		*string_ptr++ = (char)ch;
		ksnprinf_len++;
	}
}


void
ksprintf(char *buff, const char *fmt, ...) {
	va_list args;
	void	(*old_print_char)(int) = print_char;

	print_char = string_print_char;
	string_ptr = buff;
	va_start(args, fmt);
	vmsg(fmt, args);
	va_end(args);
	*string_ptr = '\0';
	print_char = old_print_char;
}

void
ksnprintf(char * const buff, const size_t num, const char * const fmt, ...) {
	va_list args;
	void	(*old_print_char)(int) = print_char;

	print_char = string_print_n_char;
	ksnprinf_len = 0;
	ksnprintf_max_len = num;
	string_ptr = buff;
	va_start(args, fmt);
	vmsg(fmt, args);
	va_end(args);
	*string_ptr = '\0';
	ksnprinf_len = 0;
	ksnprintf_max_len = 0;
	print_char = old_print_char;
}

void
crash(const char *fmt, ...) {
	va_list args;
	
	va_start(args, fmt);
	vmsg(fmt, args);
	va_end(args);
	crash_done();
}	

void
set_print_char(void (*prt)(int)) {
	print_char = prt;
}

void	(*
get_print_char(void))(int) {
	return print_char;
}

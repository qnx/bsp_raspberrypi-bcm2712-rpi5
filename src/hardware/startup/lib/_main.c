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
#include <errno.h>

#ifndef CPU_PADDR_BITS
#define CPU_PADDR_BITS 32
#endif

unsigned					paddr_bits = CPU_PADDR_BITS;
int							debug_flag = 0;
struct reserved_ram			*reserved_ram_list = NULL;
unsigned long				cpu_freq;
unsigned long				cycles_freq;
unsigned long				timer_freq;
unsigned long				timer_prog_time;
chip_info					dbg_device[2];
unsigned					patch_channel;
struct startup_header		*shdr;
char						**_argv;
int							_argc;
unsigned					max_cpus = ~0;
unsigned					system_icache_idx = CACHE_LIST_END;
unsigned					system_dcache_idx = CACHE_LIST_END;
chip_info					timer_chip;
unsigned					(*timer_start)(void);
unsigned					(*timer_diff)(unsigned start);
struct syspage_entry		*_syspage_ptr;
unsigned					misc_flags;
int							secure_system;
uintptr_t					first_bootstrap_start_vaddr = ~(uintptr_t)0;
PADDR_T						full_image_paddr;
PADDR_T						full_ram_paddr;
PADDR_T						full_imagefs_paddr;
static						uintptr_t stack_addr;

void						(*uefi_io_suspend_f)(void) = NULL;
void						(*uefi_io_resume_f)(void) = NULL;
void						(*uefi_exit_boot_services_f)(void) = NULL;
void						_main(void);

extern struct bootargs_entry	boot_args;	//filled in by mkifs

extern int	main(int argc,char **argv,char **envv);

static char				*argv[20], *envv[20];

static void
setup_cmdline(void) {
	char		*args;
	int			i, argc, envc;

	//
	// Find and construct argument and environment vectors for ourselves
	//

	tweak_cmdline(&boot_args, "startup");

	argc = envc = 0;
	args = boot_args.args;
	for(i = 0; i < boot_args.argc; ++i) {
		if(i < sizeof argv / sizeof *argv - 1) argv[argc++] = args;
		while(*args++) ;
	}
	argv[argc] = 0;

	for(i = 0; i < boot_args.envc; ++i) {
		if (i < sizeof envv / sizeof *envv - 1) envv[envc++] = args;
		while(*args++) ;
	}
	envv[envc] = 0;
	_argc = argc;
	_argv = argv;

}

static void
stack_check_init(void) {
	extern void *stack_addr_lo;
	stack_addr = (uintptr_t)&stack_addr_lo + ((boot_args.size_hi << 8) | boot_args.size_lo);
}

void
_main(void) {
	void *syspage_mem = NULL;

	shdr = (struct startup_header *)(uintptr_t)boot_args.shdr_addr;
	if(shdr == (void *)SHDR_ADDR_64) {
		// The boot image was loaded above 4G, so the 32 bits of the shdr_addr
		// field isn't enough to hold the address of the startup header. The
		// mkifs program hides the 8 byte address of the structure following
		// the args/env strings in this case/.
		unsigned const boot_size = boot_args.size_lo | (boot_args.size_hi << 8);
		uint64_t real_shdr;
		// extract with memmove() to avoid alignment problems
		memmove(&real_shdr, (uint8_t *)&boot_args + boot_size - sizeof(real_shdr), sizeof(real_shdr));
		shdr = (struct startup_header *)(uintptr_t)real_shdr;
	}

	full_image_paddr = shdr->image_paddr + shdr->addr_off;
	full_ram_paddr = shdr->ram_paddr + shdr->addr_off;
	if(shdr->imagefs_paddr != 0) {
		full_imagefs_paddr = shdr->imagefs_paddr + shdr->addr_off;
	}


	board_init();

	setup_cmdline();

	stack_check_init();

	cpu_startup();

	#define INIT_SYSPAGE_SIZE 0x600
	syspage_mem = ws_alloc(INIT_SYSPAGE_SIZE);
	if(!syspage_mem) {
		crash("No memory for syspage.\n");
	}
	init_syspage_memory(syspage_mem, INIT_SYSPAGE_SIZE);

	if(full_imagefs_paddr != 0) {
		avoid_ram(full_imagefs_paddr, shdr->stored_size);
	}

	main(_argc, _argv, envv);

	//
	// Copy the local version of the system page we've built to the real
	// system page location we allocated in init_system_private().
	//
	write_syspage_memory();

	//
	// Tell the AP's that that the syspage is now present.
	//
	smp_hook_rtn();

	startnext();
}

static void
hook_dummy(void) {
}

void						(*smp_hook_rtn)(void) = hook_dummy;


// Replacement for some C library stuff to minimize startup size
int errno;

int *
__get_errno_ptr(void) { return &errno; }


size_t
__stackavail(void)
{
	const uintptr_t sp = rdsp();
#ifdef TRACE_ALLOCA
	kprintf("%s(), top = %v, sp = %v, free = %u\n", __FUNCTION__, stack_addr, sp, sp - stack_addr);
#endif
	return (size_t)(sp - stack_addr);
}

void
abort(void) { crash("ABORT"); for( ;; ) {} }


#ifdef TRACE_ALLOCA		/* see startup.h */

void *__alloca_tmp;

#define BLOWN_STACK_MSG		"\tBlown startup stack [lo/hi/cur: "
const char * const blown_startup_stack(const size_t req_sz, const uintptr_t sp)
{
	extern void *stack_addr_hi;
	const size_t aligned_req_sz = __ALLOCA_ALIGN(req_sz) + __ALLOCA_OVERHEAD;
	static char msg[] = {BLOWN_STACK_MSG "xxxxxxxxxxxxxxxx/xxxxxxxxxxxxxxxx/xxxxxxxxxxxxxxxx], request xxxxxxx of xxxxxxx available\n"};

	ksprintf(&msg[sizeof(BLOWN_STACK_MSG) - 1], "%v/%v/%v], request %u of %u available\n",
						stack_addr, (uintptr_t)&stack_addr_hi, sp, (unsigned)aligned_req_sz, (unsigned)(sp - stack_addr));
	return msg;
}
#endif

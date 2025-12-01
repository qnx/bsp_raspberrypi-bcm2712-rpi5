/*
 * $QNXLicenseC:
 * Copyright 2008, 2020, QNX Software Systems.
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
#include <hw/uefi.h>


void uefi_print_str(char *str)
{
	if ((efi_system_table != NULL) &&
		(efi_system_table->ConOut != NULL) &&
		(efi_system_table->ConOut->OutputString != NULL))
	{
		const unsigned len = strlen(str);
		unsigned carriage_ret = 0;
		unsigned i;

		for (i=0; i<len; i++) {
			if (str[i] == '\n') ++carriage_ret;
		}
		_Char16t *uefi_str = alloca((2 * len) + ((1 + carriage_ret) * sizeof(*uefi_str)));

		if (uefi_str != NULL)
		{
			unsigned j;
			for (i=j=0; i<len; i++, j++) {
				uefi_str[j] = str[i];
				if (str[i] == '\n') uefi_str[++j] = '\r';
			}
			uefi_str[j] = '\0';

			efi_system_table->ConOut->OutputString(efi_system_table->ConOut, uefi_str);
		}
	}
}

void uefi_print_char(int c)
{
	if ((efi_system_table != NULL) &&
		(efi_system_table->ConOut != NULL) &&
		(efi_system_table->ConOut->OutputString != NULL))
	{
#if 0
		_Char16t uefi_char = (_Char16t)c;
		efi_system_table->ConOut->OutputString(efi_system_table->ConOut, &uefi_char);
#else
		static _Char16t uefi_str[128];	/* min size is 2 */
		static unsigned cur_pos = 0;

		 uefi_str[cur_pos++] = c;
		 if ((cur_pos == (NUM_ELTS(uefi_str) - 1)) || (c == '\r'))
		 {
			 uefi_str[cur_pos] = '\0';
			 efi_system_table->ConOut->OutputString(efi_system_table->ConOut, uefi_str);
			/* Ensure interrupts are kept disabled */
			InterruptDisable();
			 cur_pos = 0;
		 }
#endif
	}
}

void uefi_io_flush(void)
{
	uefi_print_char('\r');
}

/*
 * The suspend/resume are matched and should only be called by the boot processor
 *
 * They are required because the AP's cannot seem to use the boot services so in
 * order to defer exiting boot services as long as possible so we get as much
 * output as possible, we suspend/resume around start_aps()
*/
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cur_ConOut = NULL;
static unsigned suspend_cnt = 0;

static void _uefi_io_suspend(void)
{
	if (suspend_cnt++ == 0)
	{
		uefi_io_flush();
		cur_ConOut = efi_system_table->ConOut;
		efi_system_table->ConOut = NULL;
	}
}
static void _uefi_io_resume(void)
{
	if (suspend_cnt == 1)
	{
		efi_system_table->ConOut = cur_ConOut;
	}
	--suspend_cnt;
}

void uefi_io_init(unsigned channel, const char *init, const char *defaults)
{
	extern void (*uefi_io_suspend_f)(void);
	extern void (*uefi_io_resume_f)(void);

	/*
	 * This option forces ExitBootServices to execute at the end of startup
	 * in order to keep the graphic console active as long as possible.
	 * CAUTION: This flag may crash startup and prevent the board from booting
	 *          to procnto. Use this flag for debug purpose only.
	 */
	if(strstr(init, "lateexit") != NULL) {
		uefi_exit_init();
	}

	uefi_io_suspend_f = _uefi_io_suspend;
	uefi_io_resume_f = _uefi_io_resume;
}

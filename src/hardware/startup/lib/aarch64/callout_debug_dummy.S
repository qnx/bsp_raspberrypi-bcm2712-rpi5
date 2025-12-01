#
# Copyright 2020, QNX Software Systems.
#
# Licensed under the Apache License, Version 2.0 (the "License"). You
# may not reproduce, modify or distribute this software except in
# compliance with the License. You may obtain a copy of the License
# at: http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
#
# This file may contain contributions from others, either as
# contributors under the License or as licensors under other terms.
# Please review this entire file for other proprietary rights or license
# notices, as well as the QNX Development Suite License Guide at
# http://licensing.qnx.com/license-guide/ for other information.
#

/*
 * polled serial I/O
 */

#include <aarch64/callout.ah>
/*
 * -----------------------------------------------------------------------
 * void	display_char_dummy(struct sypage_entry *, char)
 *
 * x0: syspage pointer
 * x1: character
 * -----------------------------------------------------------------------
 */
CALLOUT_START(display_char_dummy, 0, 0)
ret
CALLOUT_END(display_char_dummy)

/*
 * -----------------------------------------------------------------------
 * char	poll_key_dummy(struct sypage_entry *)
 *
 * x0: syspage pointer
 * -----------------------------------------------------------------------
 */
CALLOUT_START(poll_key_dummy, 0, 0)
mov		w0, #-1
ret
CALLOUT_END(poll_key_dummy)

/*
 * -----------------------------------------------------------------------
 * int break_detect_dummy(struct syspage_entry *)
 *
 * x0: syspage pointer
 *
 * Return 1 if break detected, 0 otherwise
 * -----------------------------------------------------------------------
 */
CALLOUT_START(break_detect_dummy, 0, 0)
mov		w0, #0
ret
CALLOUT_END(break_detect_dummy)

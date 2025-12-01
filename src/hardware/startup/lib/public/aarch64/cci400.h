/*
 * $QNXLicenseC:
 * Copyright 2016, QNX Software Systems.
 * Copyright 2017 NXP
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

/*
 * ARM Cache Coherent Interconnect 400
 */

#ifndef CCI400_H_
#define CCI400_H_

/*
 * -----------------------------------------------------------------------
 * Cache Coherent Interconnect 400 (CCI400) registers
 * -----------------------------------------------------------------------
 */
#define ARM_CCI400_CONTROL_OVERRIDE                 0x00090000
#define ARM_CCI400_CONTROL_OVERRIDE_TERMINATE_BARRIER_TR_MASK (0x01 << 3)
#define ARM_CCI400_CONTROL_OVERRIDE_DISABLE_SPECULATIVE_FETCH_MASK (0x01 << 2)
#define ARM_CCI400_SPECULATION_CONTROL              0x00090004
#define ARM_CCI400_SECURE_ACCESS                    0x00090008
#define ARM_CCI400_SECURE_ACCESS_NS_ACCESS_EN_MASK  (0x01 << 0)

#define ARM_CCI400_STATUS                           0x0009000C
#define ARM_CCI400_STATUS_CHANGE_PENDING_MASK       (0x01 << 0)
#define ARM_CCI400_IMPRECISE_ERROR                  0x00090010
#define ARM_CCI400_CTLR_OVERRIDE                    0x00090000
#define ARM_CCI400_PERFORMANCE_MONITOR_CONTROL      0x00090100

/* Slave interface 0 registers */
#define ARM_CCI400_SI0_SNOOP_CONTROL                0x00091000
#define ARM_CCI400_SI0_SHAREABLE_OVERRIDE           0x00091004
#define ARM_CCI400_SI0_RD_CHANNEL_QOS_OVERRIDE      0x00091100
#define ARM_CCI400_SI0_WR_CHANNEL_QOS_OVERRIDE      0x00091104
#define ARM_CCI400_SI0_QOS_CONTROL                  0x0009110C
#define ARM_CCI400_SI0_MAX_OT                       0x00091110
#define ARM_CCI400_SI0_REGULATOR_TARGET             0x000911303
#define ARM_CCI400_SI0_QOS_REGULATOR_SCALE_FACTOR   0x00091134
#define ARM_CCI400_SI0_QOS_RANGE                    0x00091138

/* Slave interface 1 registers */
#define ARM_CCI400_SI1_SNOOP_CONTROL                0x00092000
#define ARM_CCI400_SI1_SNOOP_CONTROL_DVM_MSG_EN_MASK (0x01 << 1)
#define ARM_CCI400_SI1_SNOOP_CONTROL_ISSUING_OF_SNOOP_EN_MASK (0x01 << 0)
#define ARM_CCI400_SI1_SHAREABLE_OVERRIDE           0x00092004
#define ARM_CCI400_SI1_RD_CHANNEL_QOS_OVERRIDE      0x00092100
#define ARM_CCI400_SI1_WR_CHANNEL_QOS_OVERRIDE      0x00092104
#define ARM_CCI400_SI1_QOS_CONTROL                  0x0009210C
#define ARM_CCI400_SI1_MAX_OT                       0x00092110
#define ARM_CCI400_SI1_REGULATOR_TARGET             0x00092130
#define ARM_CCI400_SI1_QOS_REGULATOR_SCALE_FACTOR   0x00092134
#define ARM_CCI400_SI1_QOS_RANGE                    0x00092138

/* Slave interface 2 registers */
#define ARM_CCI400_SI2_SNOOP_CONTROL                0x00093000
#define ARM_CCI400_SI2_SNOOP_CONTROL_DVM_MSG_EN_MASK (0x01 << 1)
#define ARM_CCI400_SI2_SNOOP_CONTROL_ISSUING_OF_SNOOP_EN_MASK (0x01 << 0)
#define ARM_CCI400_SI2_SHAREABLE_OVERRIDE           0x00093004
#define ARM_CCI400_SI2_RD_CHANNEL_QOS_OVERRIDE      0x00093100
#define ARM_CCI400_SI2_WR_CHANNEL_QOS_OVERRIDE      0x00093104
#define ARM_CCI400_SI2_QOS_CONTROL                  0x0009310C
#define ARM_CCI400_SI2_MAX_OT                       0x00093110
#define ARM_CCI400_SI2_REGULATOR_TARGET             0x0003130
#define ARM_CCI400_SI2_QOS_REGULATOR_SCALE_FACTOR   0x00003134
#define ARM_CCI400_SI2_QOS_RANGE                    0x000903138

/* Slave interface 3 registers */
#define ARM_CCI400_SI3_SNOOP_CONTROL                0x00094000
#define ARM_CCI400_SI3_SNOOP_CONTROL_DVM_MSG_EN_MASK (0x01 << 1)
#define ARM_CCI400_SI3_SNOOP_CONTROL_ISSUING_OF_SNOOP_EN_MASK (0x01 << 0)
#define ARM_CCI400_SI3_SHAREABLE_OVERRIDE           0x00094004
#define ARM_CCI400_SI3_RD_CHANNEL_QOS_OVERRIDE      0x00094100
#define ARM_CCI400_SI3_WR_CHANNEL_QOS_OVERRIDE      0x00094104
#define ARM_CCI400_SI3_QOS_CONTROL                  0x0009410C
#define ARM_CCI400_SI3_MAX_OT                       0x00094110
#define ARM_CCI400_SI3_REGULATOR_TARGET             0x00094130
#define ARM_CCI400_SI3_QOS_REGULATOR_SCALE_FACTOR   0x00094134
#define ARM_CCI400_SI3_QOS_RANGE                    0x00094138

/* Slave interface 4 registers */
#define ARM_CCI400_SI4_SNOOP_CONTROL                0x00095000
#define ARM_CCI400_SI4_SNOOP_CONTROL_DVM_MSG_EN_MASK (0x01 << 1)
#define ARM_CCI400_SI4_SNOOP_CONTROL_ISSUING_OF_SNOOP_EN_MASK (0x01 << 0)
#define ARM_CCI400_SI4_SHAREABLE_OVERRIDE           0x00095004
#define ARM_CCI400_SI4_RD_CHANNEL_QOS_OVERRIDE      0x00095100
#define ARM_CCI400_SI4_WR_CHANNEL_QOS_OVERRIDE      0x00095104
#define ARM_CCI400_SI4_QOS_CONTROL                  0x0009510C
#define ARM_CCI400_SI4_MAX_OT                       0x00095110
#define ARM_CCI400_SI4_REGULATOR_TARGET             0x00095130
#define ARM_CCI400_SI4_QOS_REGULATOR_SCALE_FACTOR   0x00095134
#define ARM_CCI400_SI4_QOS_RANGE                    0x00095138

/* Cycle counter registers */
#define ARM_CCI400_CYCLE_COUNTER                    0x00099004
#define ARM_CCI400_COUNT_CONTROL                    0x00099008
#define ARM_CCI400_OVERFLOW_FLAG_STATUS             0x0009900C

/* Performance counter 0 registers */
#define ARM_CCI400_PC0_EVENT_SELECT                 0x0009A000
#define ARM_CCI400_PC0_EVENT_COUNT                  0x0009A004
#define ARM_CCI400_PC0_COUNTER_CONTROL              0x0009A008
#define ARM_CCI400_PC0_OVERFLOW_FLAG_STATUS         0x0009A00C

/* Performance counter 1 registers */
#define ARM_CCI400_PC1_EVENT_SELECT                 0x0009B000
#define ARM_CCI400_PC1_EVENT_COUNT                  0x0009B004
#define ARM_CCI400_PC1_COUNTER_CONTROL              0x0009B008
#define ARM_CCI400_PC1_OVERFLOW_FLAG_STATUS         0x0009B00C

/* Performance counter 2 registers */
#define ARM_CCI400_PC2_EVENT_SELECT                 0x0009C000
#define ARM_CCI400_PC2_EVENT_COUNT                  0x0009C004
#define ARM_CCI400_PC2_COUNTER_CONTROL              0x0009C008
#define ARM_CCI400_PC2_OVERFLOW_FLAG_STATUS         0x0009C00C

/* Performance counter 3 registers */
#define ARM_CCI400_PC3_EVENT_SELECT                 0x0009D000
#define ARM_CCI400_PC3_EVENT_COUNT                  0x0009D004
#define ARM_CCI400_PC3_COUNTER_CONTROL              0x0009D008
#define ARM_CCI400_PC3_OVERFLOW_FLAG_STATUS         0x0009D00C

#endif    /* CCI400_H_ */

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
 * Copyright (c) 2015, 2022, 2024, 2025, BlackBerry Limited.
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
 */


#include <sys/slog.h>
#include <sys/slogcodes.h>
#include "proto.h"

#define BUSY_RETRY (3)

uint32_t i2c_reg_read32(dwc_dev_t* const dev, const uint32_t offset)
{
	return *(volatile uint32_t *)(dev->vbase + offset);
}

void i2c_reg_write32(dwc_dev_t* const dev, const uint32_t offset,
		const uint32_t value)
{
	*(volatile uint32_t *)(dev->vbase + offset) = value;
}

int32_t dwc_i2c_enable(dwc_dev_t* const dev, const uint32_t enable)
{
	int32_t timeout = 1000;

	do {
		i2c_reg_write32(dev, DW_IC_ENABLE, enable);
		if ((i2c_reg_read32(dev, DW_IC_ENABLE_STATUS) &
				DW_IC_ENABLE_STATUS_ENABLE_MASK) == enable) {
			return 0;
		}

		/*
		 * Wait 10 times the signaling period of the highest I2C
		 * transfer supported by the driver (for 400KHz this is 25us).
		 */
		(void)usleep(25);
	} while (timeout-- != 0);

	logerr("%s: timeout in %sabling I2C adapter\n",
			__func__, (enable != 0U) ? "en" : "dis");
	return -ETIMEDOUT;
}

static uint32_t i2c_dw_scl_hcnt(const uint32_t ic_clk, const uint32_t tsymbol,
		const uint32_t tf, const int32_t cond, const int32_t offset)
{
	/*
	 * DesignWare I2C core doesn't seem to have solid strategy to meet
	 * the tHD;STA timing spec. Configuring _HCNT based on tHIGH spec
	 * will result in violation of the tHD;STA spec.
	 */
	if (cond != 0) {
		/*
		 * Conditional expression:
		 *
		 *   IC_[FS]S_SCL_HCNT + (1+4+3) >= IC_CLK * tHIGH
		 *
		 * This is based on the DW manuals, and represents an ideal
		 * configuration.  The resulting I2C bus speed will be
		 * faster than any of the others.
		 *
		 * If your hardware is free from tHD;STA issue, try this one.
		 */
		return (((ic_clk * tsymbol) + 500000U) / 1000000U)
						- 8U + (uint32_t)offset;
	} else {
		/*
		 * Conditional expression:
		 *
		 *   IC_[FS]S_SCL_HCNT + 3 >= IC_CLK * (tHD;STA + tf)
		 *
		 * This is just experimental rule; the tHD;STA period turned
		 * out to be proportinal to (_HCNT + 3).  With this setting,
		 * we could meet both tHIGH and tHD;STA timing specs.
		 *
		 * If unsure, you'd better to take this alternative.
		 *
		 * The reason why we need to take into account "tf" here,
		 * is the same as described in i2c_dw_scl_lcnt().
		 */
		return (((ic_clk * (tsymbol + tf)) + 500000U) / 1000000U)
						- 3U + (uint32_t)offset;
	}
}

static uint32_t i2c_dw_scl_lcnt(const uint32_t ic_clk, const uint32_t tlow,
		const uint32_t tf, const int32_t offset)
{
	/*
	 * Conditional expression:
	 *
	 *   IC_[FS]S_SCL_LCNT + 1 >= IC_CLK * (tlow + tf)
	 *
	 * DW I2C core starts counting the SCL CNTs for the LOW period
	 * of the SCL clock (tlow) as soon as it pulls the SCL line.
	 * In order to meet the tLOW timing spec, we need to take into
	 * account the fall time of SCL signal (tf).  Default tf value
	 * should be 0.3 us, for safety.
	 */
	return (((ic_clk * (tlow + tf)) + 500000U) / 1000000U)
						- 1U + (uint32_t)offset;
}

int32_t dwc_i2c_bus_active(dwc_dev_t* const dev)
{
	/* Wait Bus Idle */
	int32_t busy_retry = BUSY_RETRY;
	int32_t ret;

	ret = dwc_i2c_wait_bus_not_busy(dev);
	while ((ret != 0) && (busy_retry >= 0)) {

		logerr("Busy detected. retry=%d", busy_retry);
		if (busy_retry-- == 0) {
			dwc_i2c_reset(dev);
			logfyi("One last try to check for busy...");
		}
		ret = dwc_i2c_wait_bus_not_busy(dev);
	}

	if (ret != 0) {
		logerr("Can't get out of busy.");
		return (int32_t)I2C_STATUS_BUSY;
	}

	if (busy_retry < BUSY_RETRY) {
		logfyi("Successfully unstuck from busy.");
	}

	/* Disable the adapter */
	if (0 != dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_DISABLE)) {
		dwc_i2c_reset(dev);
		return (int32_t)I2C_STATUS_BUSY;
	}

	/* Set slave address */
	if (dev->slave_addr_fmt == I2C_ADDRFMT_7BIT) {
		i2c_reg_write32(dev, DW_IC_TAR, dev->slave_addr);
	} else {
		i2c_reg_write32(dev, DW_IC_TAR,
				dev->slave_addr | DW_IC_TAR_10BITADDR_MASTER);
	}

	/* set speed and master mode */
	i2c_reg_write32(dev, DW_IC_CON, dev->master_cfg);

	/* Enforce disabled interrupts (due to HW issues) */
	i2c_reg_write32(dev, DW_IC_INTR_MASK, 0);

	/* Enable the adapter */
	if (0 != dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_ENABLE)) {
		dwc_i2c_reset(dev);
		return (int32_t)I2C_STATUS_BUSY;
	}

	return 0;
}

static int32_t map_in_device_registers(dwc_dev_t *dev)
{
	const void* const hw_addr = mmap_device_memory(NULL, dev->map_size,
					PROT_READ | PROT_WRITE | PROT_NOCACHE,
					MAP_SHARED, dev->pbase);
	if (hw_addr == MAP_FAILED) {
		logerr( "mmap_device_memory failed");
		return -1;
	}

	dev->vbase = (uintptr_t)hw_addr;

	if (dev->verbose != 0U) {
		logfyi("mmap_device_memory size %u @ %08x to %p\n",
			(uint32_t)dev->map_size, (uint32_t)dev->pbase,
			(void*)dev->vbase);
	}

	return 0;
}

static void dwc_i2c_set_fifo_threshold(dwc_dev_t *dev)
{
	uint32_t reg, tx_fifo_depth, rx_fifo_depth;

	reg = i2c_reg_read32(dev, DW_IC_COMP_PARAM_1);
	tx_fifo_depth = ((reg >> DW_IC_COMP_PARAM_1_TX_DEPTH_F) & 0xffU) + 1U;
	rx_fifo_depth = ((reg >> DW_IC_COMP_PARAM_1_RX_DEPTH_F) & 0xffU) + 1U;

	dev->fifo_depth = min(tx_fifo_depth, rx_fifo_depth);

	i2c_reg_write32(dev, DW_IC_TX_TL, dev->fifo_depth / 2U);
	i2c_reg_write32(dev, DW_IC_RX_TL, dev->fifo_depth / 2U);
}

void* dwc_i2c_init(int argc, char *argv[])
{
	dwc_dev_t *dev;
	uint32_t reg;
	uintptr_t iolevel;

	iolevel = _NTO_IO_LEVEL_NONE | _NTO_TCTL_IO_LEVEL_INHERIT;
	if (ThreadCtl(_NTO_TCTL_IO_LEVEL, (void *)iolevel) == -1) {
		perror("ThreadCtl");
		return NULL;
	}

	dev = calloc(1, sizeof(*dev));  // allocate device object
	if (dev == NULL) {
		return NULL;
	}

	if (dwc_i2c_parseopts(dev, argc, argv) == -1) {
		goto fail_cleanup;
	}

	if (dwc_i2c_probe_device(dev) == -1) {
		goto fail_cleanup;
	}

	if (map_in_device_registers(dev) != 0) {
		goto fail_cleanup;
	}

	/* check I2C component type*/
	reg = i2c_reg_read32(dev, DW_IC_COMP_TYPE);
	if (reg != DW_IC_COMP_TYPE_VALUE) {
		logerr("%s: Unknown I2C component type: 0x%08x",
				__func__, reg);
		goto fail_cleanup;
	}

	/* Attach interrupt */
	struct sigevent intrevent;

	SIGEV_INTR_INIT(&intrevent);

	dev->iid = InterruptAttachEvent(dev->irq, &intrevent,
				_NTO_INTR_FLAGS_TRK_MSK);
	if (dev->iid == -1) {
		logerr("%s: InterruptAttachEvent", __func__);
		goto fail_cleanup;
	}
	logfyi("connected to IRQ %u", dev->irq);

	/* Disable the I2C adapter */
	(void)dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_DISABLE);

	dwc_i2c_init_registers(dev);

	return dev;

fail_cleanup:
	dwc_i2c_cleanup(dev);

	return NULL;
}

void dwc_i2c_init_registers(dwc_dev_t *dev)
{
	uint32_t reg;

	/* Get I2C input clock */
	if (dev->clock_khz == 0U) {
		dev->clock_khz = dwc_i2c_get_input_clock(dev);
	}

	logfyi("Set clock to %ld Khz", dev->clock_khz);

#ifdef DWC_SUPPORT_FIXED_SCL
	/* Set SCL timing parameters for standard-mode */
	/* Intel Recommended setting; SF Case # 00168315 */
	if (dev->fixed_scl != 0U) {
		dev->ss_hcnt = FIXED_SS_HCNT;
		dev->ss_lcnt = FIXED_SS_LCNT;
	} else
#endif
	{
		dev->ss_hcnt = i2c_dw_scl_hcnt(dev->clock_khz,
				dev->std.high,
				dev->std.fall,
				0,      // 0: DW default, 1: Ideal
				0);     // No offset
		dev->ss_lcnt = i2c_dw_scl_lcnt(dev->clock_khz,
				dev->std.low,
				dev->std.fall,
				0);     // No offset
	}

	logfyi("DW_IC_SS_SCL_HCNT %08x ", dev->ss_hcnt);
	logfyi("DW_IC_SS_SCL_LCNT %08x ", dev->ss_lcnt);

	i2c_reg_write32(dev, DW_IC_SS_SCL_HCNT, dev->ss_hcnt);
	i2c_reg_write32(dev, DW_IC_SS_SCL_LCNT, dev->ss_lcnt);

#ifdef DWC_SUPPORT_FIXED_SCL
	/* Set SCL timing parameters for fast-mode */
	/* Intel Recommended setting; SF Case # 00168315 */
	if (dev->fixed_scl != 0U) {
		dev->fs_hcnt = FIXED_FS_HCNT;
		dev->fs_lcnt = FIXED_FS_LCNT;
	} else
#endif
	{
		dev->fs_hcnt = i2c_dw_scl_hcnt(dev->clock_khz,
				dev->fast.high,
				dev->fast.fall,
				0,      // 0: DW default, 1: Ideal
				0);     // No offset
		dev->fs_lcnt = i2c_dw_scl_lcnt(dev->clock_khz,
				dev->fast.low,
				dev->fast.fall,
				0);     // No offset
	}

	logfyi("DW_IC_FS_SCL_HCNT %08x ", dev->fs_hcnt);
	logfyi("DW_IC_FS_SCL_LCNT %08x ", dev->fs_lcnt);

	i2c_reg_write32(dev, DW_IC_FS_SCL_HCNT, dev->fs_hcnt);
	i2c_reg_write32(dev, DW_IC_FS_SCL_LCNT, dev->fs_lcnt);

	/* Configure SDA Hold Time */
	reg = i2c_reg_read32(dev, DW_IC_COMP_VERSION);
	if (reg >= DW_IC_SDA_HOLD_MIN_VERS) {
#ifdef DWC_SUPPORT_FIXED_SCL
		/* Intel Recommended setting; SF Case # 00168315 */
		if (dev->fixed_scl != 0U) {
			const uint32_t sda_hold_time = 28;
			i2c_reg_write32(dev, DW_IC_SDA_HOLD, sda_hold_time);

			logfyi("DW_IC_SDA_HOLD %08x ", sda_hold_time);
		} else
#endif
		{
			/*
			 * The I2C-bus specification requires at least 300 ns
			 * for the SDA hold time
			 */
			const uint32_t sda_hold_time = max(dev->sda_hold_time,
					((dev->clock_khz * SDA_HOLD_TIME_VALUE_ns) + 500000U) / 1000000U);
			if (sda_hold_time < dev->fs_lcnt) {
				i2c_reg_write32(dev, DW_IC_SDA_HOLD, sda_hold_time);

				logfyi("DW_IC_SDA_HOLD %08x ", sda_hold_time);
			} else {
				logerr("%s: SDA hold time %u too large for SCL_LCNT %u",
					__func__, sda_hold_time, dev->fs_lcnt);
			}
		}
	} else {
		logerr("%s: Hardware too old to adjust SDA hold time!",
				__func__);
	}

	/* Configure Tx/Rx FIFO threshold */
	dwc_i2c_set_fifo_threshold(dev);

	/* set default bus speed */
	errno = EOK; /* CERT ERR30-C */
	if ((dwc_i2c_set_bus_speed(dev, 100000, NULL) != 0) || (errno != EOK)) {
		logerr("%s: Failed to set bus speed.", __func__);
	}

	/* Clear and Disable i2c Interrupt */
	(void)i2c_reg_read32(dev, DW_IC_CLR_INTR);
	i2c_reg_write32(dev, DW_IC_INTR_MASK, 0);
}

void dwc_i2c_fini(void *hdl)
{
	dwc_dev_t* const dev = hdl;

	/* hdl can be NULL if some error occurred during initialization. */
	if (hdl == NULL) {
		return;
	}

	(void)dwc_i2c_enable(dev, DW_IC_ENABLE_STATUS_DISABLE);

	(void)InterruptDetach(dev->iid);

	dwc_i2c_cleanup(dev);
}

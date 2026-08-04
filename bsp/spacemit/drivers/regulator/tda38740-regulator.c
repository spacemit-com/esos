/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <rtconfig.h>
#include <riscv-ops.h>

#ifdef RT_USING_PM

#include <drivers/i2c.h>
#include <drivers/pm.h>
#include "regulator.h"
#include "../i2c/i2c-k1.h"

#define TDA38740_SYSSUSP_REG		0x5c
#define TDA38740_SYSSUSP_MASK		RT_BIT(1)
#define TDA38740_PAGE_REG		0xff
#define TDA38740_MAX_PM_DEVICES		4
#define TDA38740_I2C_POLL_TIMEOUT_US	100000

static struct rt_device *tda38740_pm_devices[TDA38740_MAX_PM_DEVICES];
static rt_uint8_t tda38740_pm_device_count;

static rt_uint16_t tda38740_i2c_addr(struct spacemit_regulator *sr)
{
	if (sr->slave_addr == 0x42)
		return 0x12;

	if (sr->slave_addr == 0x4f)
		return 0x1f;

	return sr->slave_addr;
}

static inline struct spacemit_i2c_dev *tda38740_to_i2c_dev(struct rt_i2c_bus_device *dev)
{
	return rt_container_of(dev, struct spacemit_i2c_dev, dev);
}

static inline rt_uint32_t tda38740_i2c_read_reg(struct spacemit_i2c_dev *i2c,
		int reg)
{
	return readl(i2c->mapbase + reg);
}

static inline void tda38740_i2c_write_reg(struct spacemit_i2c_dev *i2c,
		int reg, rt_uint32_t val)
{
	writel(val, i2c->mapbase + reg);
}

static void tda38740_i2c_clear_status(struct spacemit_i2c_dev *i2c)
{
	tda38740_i2c_write_reg(i2c, REG_SR, SPACEMIT_I2C_INT_STATUS_MASK);
}

static void tda38740_i2c_flush_fifo(struct spacemit_i2c_dev *i2c)
{
	tda38740_i2c_write_reg(i2c, REG_WFIFO_WPTR, 0);
	tda38740_i2c_write_reg(i2c, REG_WFIFO_RPTR, 0);
	tda38740_i2c_write_reg(i2c, REG_RFIFO_WPTR, 0);
	tda38740_i2c_write_reg(i2c, REG_RFIFO_RPTR, 0);
}

static void tda38740_i2c_disable(struct spacemit_i2c_dev *i2c)
{
	i2c->i2c_ctrl_reg_value = tda38740_i2c_read_reg(i2c, REG_CR) & ~CR_IUE;
	tda38740_i2c_write_reg(i2c, REG_CR, i2c->i2c_ctrl_reg_value);
}

static void tda38740_i2c_init_fifo(struct spacemit_i2c_dev *i2c)
{
	rt_uint32_t cr_val = CR_FIFOEN | CR_GCD | CR_SCLE | CR_MSDE;

	if (i2c->fast_mode)
		cr_val |= CR_MODE_FAST;
	if (i2c->high_mode)
		cr_val |= CR_MODE_HIGH | CR_GPIOEN;

	tda38740_i2c_write_reg(i2c, REG_CR, cr_val);
}

static rt_err_t tda38740_i2c_wait_idle(struct spacemit_i2c_dev *i2c)
{
	rt_uint32_t timeout = TDA38740_I2C_POLL_TIMEOUT_US;

	while (tda38740_i2c_read_reg(i2c, REG_SR) & (SR_UB | SR_IBB)) {
		if (timeout-- == 0)
			return -RT_ETIMEOUT;

		rt_hw_us_delay(1);
	}

	return RT_EOK;
}

static rt_err_t tda38740_i2c_poll_write(struct rt_i2c_bus_device *bus,
		rt_uint16_t addr, const rt_uint8_t *buf, rt_size_t len)
{
	struct spacemit_i2c_dev *i2c;
	rt_uint32_t status;
	rt_uint32_t timeout = TDA38740_I2C_POLL_TIMEOUT_US;
	rt_size_t i;

	if (!bus || !buf || len == 0 || len + 1 > SPACEMIT_I2C_TX_FIFO_DEPTH)
		return -RT_EINVAL;

	i2c = tda38740_to_i2c_dev(bus);

	if (tda38740_i2c_wait_idle(i2c) != RT_EOK)
		return -RT_ETIMEOUT;

	tda38740_i2c_disable(i2c);
	tda38740_i2c_flush_fifo(i2c);
	tda38740_i2c_init_fifo(i2c);
	tda38740_i2c_clear_status(i2c);

	tda38740_i2c_write_reg(i2c, REG_WFIFO,
		((addr & 0x7f) << 1) | WFIFO_CTRL_TB | WFIFO_CTRL_START);

	for (i = 0; i < len; i++) {
		rt_uint32_t data = buf[i] | WFIFO_CTRL_TB;

		if (i == len - 1)
			data |= WFIFO_CTRL_STOP;

		tda38740_i2c_write_reg(i2c, REG_WFIFO, data);
	}

	tda38740_i2c_write_reg(i2c, REG_CR,
		tda38740_i2c_read_reg(i2c, REG_CR) | CR_IUE);

	while (timeout-- > 0) {
		status = tda38740_i2c_read_reg(i2c, REG_SR);

		if (status & (SR_BED | SR_ALD | SR_RXOV)) {
			tda38740_i2c_clear_status(i2c);
			tda38740_i2c_disable(i2c);
			return -RT_ERROR;
		}

		if (status & (SR_MSD | SR_TXDONE)) {
			tda38740_i2c_clear_status(i2c);
			tda38740_i2c_disable(i2c);
			return RT_EOK;
		}

		rt_hw_us_delay(1);
	}

	tda38740_i2c_disable(i2c);
	return -RT_ETIMEOUT;
}

static rt_err_t tda38740_write_byte_reg(struct spacemit_regulator *sr,
		rt_uint8_t reg, rt_uint8_t val)
{
	rt_uint8_t buf[2];
	rt_uint16_t addr = tda38740_i2c_addr(sr);

	buf[0] = reg;
	buf[1] = val;

	if (tda38740_i2c_poll_write(sr->handle_driver, addr, buf, sizeof(buf)) != RT_EOK) {
		rt_kprintf("%s:%d, write 0x%x->0x%x reg 0x%x failed\n",
			__func__, __LINE__, sr->slave_addr, addr, reg);
		return -RT_ERROR;
	}

	return RT_EOK;
}

static rt_err_t tda38740_write_word_reg(struct spacemit_regulator *sr,
		rt_uint8_t reg, rt_uint16_t val)
{
	rt_uint8_t buf[3];
	rt_uint16_t addr = tda38740_i2c_addr(sr);

	buf[0] = reg;
	buf[1] = (rt_uint8_t)(val & 0xff);
	buf[2] = (rt_uint8_t)(val >> 8);

	if (tda38740_i2c_poll_write(sr->handle_driver, addr, buf, sizeof(buf)) != RT_EOK) {
		rt_kprintf("%s:%d, write 0x%x->0x%x reg 0x%x failed\n",
			__func__, __LINE__, sr->slave_addr, addr, reg);
		return -RT_ERROR;
	}

	return RT_EOK;
}

static rt_err_t tda38740_syssusp_update(struct spacemit_regulator *sr, rt_bool_t set)
{
	rt_uint16_t val;
	rt_err_t ret;

	ret = tda38740_write_byte_reg(sr, TDA38740_PAGE_REG, 0x00);
	if (ret != RT_EOK)
		return ret;

	if (set)
		val = TDA38740_SYSSUSP_MASK;
	else
		val = 0;

	return tda38740_write_word_reg(sr, TDA38740_SYSSUSP_REG, val);
}

static rt_bool_t tda38740_lpm_mode(rt_uint8_t mode)
{
	return mode == PM_SLEEP_MODE_DEEP ||
		mode == PM_SLEEP_MODE_STANDBY ||
		mode == PM_SLEEP_MODE_SHUTDOWN;
}

static int tda38740_pm_suspend(const struct rt_device *device, rt_uint8_t mode)
{
	struct regulator_dynamic *rd = rt_container_of(device, struct regulator_dynamic, dev);
	rt_err_t ret;

	if (!tda38740_lpm_mode(mode))
		return RT_EOK;

	ret = tda38740_syssusp_update(rd->sr, RT_FALSE);
	if (ret != RT_EOK)
		rt_kprintf("tda38740: suspend update failed, ret=%d\n", ret);

	return RT_EOK;
}

static void tda38740_pm_resume(const struct rt_device *device, rt_uint8_t mode)
{
	struct regulator_dynamic *rd = rt_container_of(device, struct regulator_dynamic, dev);
	rt_err_t ret;

	if (!tda38740_lpm_mode(mode))
		return;

	ret = tda38740_syssusp_update(rd->sr, RT_TRUE);
	if (ret != RT_EOK)
		rt_kprintf("tda38740: resume update failed, ret=%d\n", ret);
}

static const struct rt_device_pm_ops tda38740_pm_ops = {
	.suspend = tda38740_pm_suspend,
	.resume = tda38740_pm_resume,
};

void tda38740_pm_device_register(struct rt_device *dev)
{
	struct regulator_dynamic *rd = rt_container_of(dev, struct regulator_dynamic, dev);

	if (tda38740_pm_device_count >= TDA38740_MAX_PM_DEVICES) {
		rt_kprintf("tda38740: too many pm devices, skip name=%s slave=0x%x\n",
			rd->param.name, rd->sr->slave_addr);
		return;
	}

	tda38740_pm_devices[tda38740_pm_device_count++] = dev;
}

static int tda38740_pm_late_init(void)
{
	rt_uint8_t i;

	for (i = 0; i < tda38740_pm_device_count; i++) {
		rt_pm_device_register(tda38740_pm_devices[i], &tda38740_pm_ops);
	}

	return 0;
}
INIT_APP_EXPORT(tda38740_pm_late_init);

#endif
/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <rtconfig.h>
#include <riscv-ops.h>
#include <register_defination.h>
#include <drivers/i2c.h>
#include <drivers/regulator.h>
#include <drivers/regulator_dm.h>
#include "regulator.h"

static struct regulator_linear_range p1_buck_ranges[] = {
	[0] = REGULATOR_LINEAR_RANGE(500000, 0x0, 0xaa, 5000),
	[1] = REGULATOR_LINEAR_RANGE(1375000, 0xab, 0xfe, 25000),
};

static struct regulator_linear_range p1_ldo_ranges[] = {
	[0] = REGULATOR_LINEAR_RANGE(500000, 0xb, 0x7f, 25000),
};

static struct regulator_linear_range is6608_buck_ranges[] = {
	[0] = REGULATOR_LINEAR_RANGE(534000, 0x10b, 0x1f4, 2000),
};

static struct regulator_linear_range tda38740_buck_ranges[] = {
	[0] = REGULATOR_LINEAR_RANGE(531216, 0x88, 0x11a, 3906),
};

static struct regulator_linear_range is6615a_buck_ranges[] = {
	[0] = REGULATOR_LINEAR_RANGE(531216, 0x110, 0x234, 1953),
};

static struct regulator_linear_range au4562_buck_ranges[] = {
	[0] = REGULATOR_LINEAR_RANGE(540000, 0x6c, 0xdc, 5000),
};

static const struct regulator_desc p1_regulator_descs[]  = {
	REGULATOR_DESC_COMMON(P1_ID_DCDC1_2,
			255, P1_BUCK1_VSEL_REG, P1_BUCK_VSEL_MASK,
			P1_BUCK1_CTRL_REG, P1_BUCK_EN_MASK,
			P1_BUCK1_SVSEL_REG, P1_BUCK_SVSEL_MASK,
			p1_buck_ranges),
	REGULATOR_DESC_COMMON(P1_ID_DCDC3,
			255, P1_BUCK3_VSEL_REG, P1_BUCK_VSEL_MASK,
			P1_BUCK3_CTRL_REG, P1_BUCK_EN_MASK,
			P1_BUCK3_SVSEL_REG, P1_BUCK_SVSEL_MASK,
			p1_buck_ranges),
	REGULATOR_DESC_COMMON(P1_ID_DCDC4,
			255, P1_BUCK4_VSEL_REG, P1_BUCK_VSEL_MASK,
			P1_BUCK4_CTRL_REG, P1_BUCK_EN_MASK,
			P1_BUCK4_SVSEL_REG, P1_BUCK_SVSEL_MASK,
			p1_buck_ranges),
	REGULATOR_DESC_COMMON(P1_ID_DCDC5,
			255, P1_BUCK5_VSEL_REG, P1_BUCK_VSEL_MASK,
			P1_BUCK5_CTRL_REG, P1_BUCK_EN_MASK,
			P1_BUCK5_SVSEL_REG, P1_BUCK_SVSEL_MASK,
			p1_buck_ranges),
	REGULATOR_DESC_COMMON(P1_ID_DCDC6,
			255, P1_BUCK6_VSEL_REG, P1_BUCK_VSEL_MASK,
			P1_BUCK6_CTRL_REG, P1_BUCK_EN_MASK,
			P1_BUCK6_SVSEL_REG, P1_BUCK_SVSEL_MASK,
			p1_buck_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO1,
			128, P1_ALDO1_VOLT_REG, P1_ALDO_VSEL_MASK,
			P1_ALDO1_CTRL_REG, P1_ALDO_EN_MASK,
			P1_ALDO1_SVOLT_REG, P1_ALDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO2,
			128, P1_ALDO2_VOLT_REG, P1_ALDO_VSEL_MASK,
			P1_ALDO2_CTRL_REG, P1_ALDO_EN_MASK,
			P1_ALDO2_SVOLT_REG, P1_ALDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO3,
			128, P1_ALDO3_VOLT_REG, P1_ALDO_VSEL_MASK,
			P1_ALDO3_CTRL_REG, P1_ALDO_EN_MASK,
			P1_ALDO3_SVOLT_REG, P1_ALDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO4,
			128, P1_ALDO4_VOLT_REG, P1_ALDO_VSEL_MASK,
			P1_ALDO4_CTRL_REG, P1_ALDO_EN_MASK,
			P1_ALDO4_SVOLT_REG, P1_ALDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO5,
			128, P1_DLDO1_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO1_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO1_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO6,
			128, P1_DLDO2_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO2_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO2_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO7,
			128, P1_DLDO3_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO3_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO3_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO8,
			128, P1_DLDO4_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO4_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO4_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO9,
			128, P1_DLDO5_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO5_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO5_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO10,
			128, P1_DLDO6_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO6_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO6_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),
	REGULATOR_DESC_COMMON(P1_ID_LDO11,
			128, P1_DLDO7_VOLT_REG, P1_DLDO_VSEL_MASK,
			P1_DLDO7_CTRL_REG, P1_DLDO_EN_MASK,
			P1_DLDO7_SVOLT_REG, P1_DLDO_SVSEL_MASK,
			p1_ldo_ranges),

};

static const struct regulator_desc is6608_regulator_descs[]  = {
	/* leaf */
	REGULATOR_DESC_COMMON(EXTERN_LEAF_X100,
			4096, IS6608_BUCK1_VOLT_REG, IS6608_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			is6608_buck_ranges),
};

static const struct regulator_desc tda38740_regulator_descs[]  = {
	/* leaf */
	REGULATOR_DESC_COMMON(EXTERN_LEAF_A100,
			4096, TDA38740_BUCK1_VOLT_REG, TDA38740_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			tda38740_buck_ranges),
	REGULATOR_DESC_COMMON(EXTERN_LEAF_X100,
			4096, TDA38740_BUCK1_VOLT_REG, TDA38740_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			tda38740_buck_ranges),
};

static const struct regulator_desc is6615a_regulator_descs[]  = {
	/* leaf */
	REGULATOR_DESC_COMMON(EXTERN_LEAF_A100,
			4096, IS6615A_BUCK1_VOLT_REG, IS6615A_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			is6615a_buck_ranges),
	REGULATOR_DESC_COMMON(EXTERN_LEAF_X100,
			4096, IS6615A_BUCK1_VOLT_REG, IS6615A_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			is6615a_buck_ranges),
};

static const struct regulator_desc au4562_regulator_descs[]  = {
	/* leaf */
	REGULATOR_DESC_COMMON(EXTERN_LEAF_A100,
			4096, AU4562_BUCK1_VOLT_REG, AU4562_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			au4562_buck_ranges),
	REGULATOR_DESC_COMMON(EXTERN_LEAF_X100,
			4096, AU4562_BUCK1_VOLT_REG, AU4562_BUCK1_VSEL_MSK,
			0, 0,
			0, 0,
			au4562_buck_ranges),
};

static struct dtb_compatible_array __compatible[] = {
	{ .compatible = "p1-regulator", .data = (void *)p1_regulator_descs },
	{}
};

static struct dtb_compatible_array __dcdc_compatible[] = {
	{ .compatible = "regulator-is6608", .data = (void *)is6608_regulator_descs },
	{ .compatible = "regulator-tda38740-1", .data = (void *)tda38740_regulator_descs },
	{ .compatible = "regulator-tda38740-2", .data = (void *)tda38740_regulator_descs },
	{ .compatible = "regulator-is6615a-1", .data = (void *)is6615a_regulator_descs },
	{ .compatible = "regulator-is6615a-2", .data = (void *)is6615a_regulator_descs },
	{ .compatible = "regulator-au4562-1", .data = (void *)au4562_regulator_descs },
	{ .compatible = "regulator-au4562-2", .data = (void *)au4562_regulator_descs },
};

#define PMIC_TYPE_MASK			0x7

static rt_err_t regulator_dynamic_enable(struct rt_regulator_node *reg)
{
	int index;
	rt_uint8_t val, val_temp[2], cmd;
	struct rt_i2c_msg msgs[2];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;

	/* regulator index */
	index = reg->param->index;

	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	msgs[0].buf = (rt_uint8_t *)&desc[index].enable_reg;
	msgs[0].len = 1;

	/* read the value */
	msgs[1].addr  = sr->slave_addr;
	msgs[1].flags = RT_I2C_RD;
	msgs[1].buf = &val;
	msgs[1].len = 1;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	val &= ~(desc[index].enable_msk);
	val |= (1 << (ffs(desc[index].enable_msk) - 1));

	/* write value */
	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	val_temp[0] = desc[index].enable_reg;
	val_temp[1] = val;
	msgs[0].buf = val_temp;
	msgs[0].len = 2;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 1) != 1) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	return 0;
}

static rt_err_t regulator_dynamic_disable(struct rt_regulator_node *reg)
{
	int index;
	rt_uint8_t val, val_temp[2], cmd;
	struct rt_i2c_msg msgs[2];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;

	/* regulator index */
	index = reg->param->index;

	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	msgs[0].buf = (rt_uint8_t *)&desc[index].enable_reg;
	msgs[0].len = 1;

	/* read the value */
	msgs[1].addr  = sr->slave_addr;
	msgs[1].flags = RT_I2C_RD;
	msgs[1].buf = &val;
	msgs[1].len = 1;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	val &= ~(desc[index].enable_msk);
	val |= (0 << (ffs(desc[index].enable_msk) - 1));

	/* write value */
	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	val_temp[0] = desc[index].enable_reg;
	val_temp[1] = val;
	msgs[0].buf = val_temp;
	msgs[0].len = 2;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 1) != 1) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	return 0;
}

static rt_bool_t regulator_dynamic_is_enabled(struct rt_regulator_node *reg)
{
	int index;
	rt_uint8_t val, cmd;
	struct rt_i2c_msg msgs[2];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;

	/* regulator index */
	index = reg->param->index;

	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	msgs[0].buf = (rt_uint8_t *)&desc[index].enable_reg;
	msgs[0].len = 1;

	/* read the value */
	msgs[1].addr  = sr->slave_addr;
	msgs[1].flags = RT_I2C_RD;
	msgs[1].buf = &val;
	msgs[1].len = 1;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	val &= desc[index].enable_msk;

	return (val ? true : false);
}

static int linear_range_get_value(const struct regulator_linear_range *r, unsigned int selector,
				  unsigned int *val)
{
	if (r->min_sel > selector || r->max_sel < selector)
		return -RT_EINVAL;

	*val = r->min + (selector - r->min_sel) * r->step;

	return 0;
}

static int linear_range_get_value_array(const struct regulator_linear_range *r, int ranges,
					unsigned int selector, unsigned int *val)
{
	int i;

	for (i = 0; i < ranges; i++) {
		if (r[i].min_sel <= selector && r[i].max_sel >= selector)
			return linear_range_get_value(&r[i], selector, val);
	}

	return -RT_EINVAL;
}

static int regulator_desc_list_voltage_linear_range(const struct regulator_desc *desc,
						    unsigned int selector)
{
	unsigned int val;
	int ret;

	RT_ASSERT(desc->n_linear_ranges);

	ret = linear_range_get_value_array(desc->linear_ranges,
					   desc->n_linear_ranges, selector,
					   &val);
	if (ret)
		return ret;

	return val;
}

static unsigned int linear_range_get_max_value(const struct regulator_linear_range *r)
{
	return r->min + (r->max_sel - r->min_sel) * r->step;
}

static int linear_range_get_selector_high(const struct regulator_linear_range *r,
					  unsigned int val, unsigned int *selector,
					  bool *found)
{
	*found = false;

	if (linear_range_get_max_value(r) < val)
		return -RT_EINVAL;

	if (r->min > val) {
		*selector = r->min_sel;
		return 0;
	}

	*found = true;

	if (r->step == 0)
		*selector = r->max_sel;
	else
		*selector = DIV_ROUND_UP(val - r->min, r->step) + r->min_sel;

	return 0;
}

static int regulator_map_voltage_linear_range(const struct regulator_desc *desc,
					      int min_uV, int max_uV)
{
	const struct regulator_linear_range *range;
	int ret = -RT_EINVAL;
	unsigned int sel;
	bool found;
	int voltage, i;

	if (!desc->n_linear_ranges) {
		RT_ASSERT(!desc->n_linear_ranges);
		return -RT_EINVAL;
	}

	for (i = 0; i < desc->n_linear_ranges; i++) {
		range = &desc->linear_ranges[i];

		ret = linear_range_get_selector_high(range, min_uV, &sel,
						     &found);
		if (ret)
			continue;

		ret = sel;

		/*
		 * Map back into a voltage to verify we're still in bounds.
		 * If we are not, then continue checking rest of the ranges.
		 */
		voltage = regulator_desc_list_voltage_linear_range(desc, sel);
		if (voltage >= min_uV && voltage <= max_uV)
			break;
	}

	if (i == desc->n_linear_ranges)
		return -RT_EINVAL;

	return ret;
}

static rt_err_t regulator_dynamic_set_voltage(struct rt_regulator_node *reg, int min_uvolt, int max_uvolt)
{
	int index, sel;
	rt_uint8_t val, cmd, val_temp[2];
	struct rt_i2c_msg msgs[2];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;

	/* regulator index */
	index = reg->param->index;

	sel = regulator_map_voltage_linear_range(&desc[index], min_uvolt, max_uvolt);
	if (sel >= 0) {
		sel <<= ffs(desc[index].vsel_msk) - 1;

		msgs[0].addr  = sr->slave_addr;
		msgs[0].flags = RT_I2C_WR;
		msgs[0].buf = (rt_uint8_t *)&desc[index].vsel_reg;
		msgs[0].len = 1;

		/* read the value */
		msgs[1].addr  = sr->slave_addr;
		msgs[1].flags = RT_I2C_RD;
		msgs[1].buf = &val;
		msgs[1].len = 1;

		if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
			rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
			return -RT_ERROR;
		}

		val &= ~(desc[index].vsel_msk);
		val |= sel;

		/* write value */
		msgs[0].addr  = sr->slave_addr;
		msgs[0].flags = RT_I2C_WR;
		val_temp[0] = desc[index].vsel_reg;
		val_temp[1] = val;
		msgs[0].buf = val_temp;
		msgs[0].len = 2;

		if (rt_i2c_transfer(sr->handle_driver, msgs, 1) != 1) {
			rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
			return -RT_ERROR;
		}
	} else {
		rt_kprintf("%s:%d, set the wrong voltage\n", __func__, __LINE__);
		return -RT_EINVAL;
	}

	return 0;
}

static int regulator_dynamic_get_voltage(struct rt_regulator_node *reg)
{
	int index, sel;
	rt_uint8_t val, cmd;
	struct rt_i2c_msg msgs[2];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;

	/* regulator index */
	index = reg->param->index;

	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	msgs[0].buf = (rt_uint8_t *)&desc[index].vsel_reg;
	msgs[0].len = 1;

	/* read the value */
	msgs[1].addr  = sr->slave_addr;
	msgs[1].flags = RT_I2C_RD;
	msgs[1].buf = &val;
	msgs[1].len = 1;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	val &= desc[index].vsel_msk;
	val >>= (ffs(desc[index].vsel_msk) - 1);

	return regulator_desc_list_voltage_linear_range(&desc[index], val);
}

static const struct rt_regulator_ops regulator_dynamic_ops =
{
	.enable = regulator_dynamic_enable,
	.disable = regulator_dynamic_disable,
	.is_enabled = regulator_dynamic_is_enabled,
	.get_voltage = regulator_dynamic_get_voltage,
	.set_voltage = regulator_dynamic_set_voltage
};

static rt_err_t regulator_independ_enable(struct rt_regulator_node *reg)
{
	return 0;
}

static rt_err_t regulator_independ_disable(struct rt_regulator_node *reg)
{
	return 0;
}

static int regulator_independ_get_voltage(struct rt_regulator_node *reg)
{
	int index, sel, ret;
	rt_uint16_t val_temp;
	rt_uint8_t val[2], cmd;
	struct rt_i2c_msg msgs[2];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;

	/* regulator index */
	index = reg->param->index;

	if (strncmp(reg->supply_name, "adcdc", 5) == 0) {
		val[0] = 0x0;
		if (strncmp(reg->supply_name, "adcdc1", 6) == 0)
			val[1] = 0x1;
		else if (strncmp(reg->supply_name, "adcdc2", 6) == 0)
			val[1] = 0x0;;
		msgs[0].addr  = sr->slave_addr;
		msgs[0].flags = RT_I2C_WR;
		msgs[0].buf = val;
		msgs[0].len = 2;

		if (rt_i2c_transfer(sr->handle_driver, msgs, 1) != 1) {
			rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
			return -RT_ERROR;
		}
	}

	msgs[0].addr  = sr->slave_addr;
	msgs[0].flags = RT_I2C_WR;
	msgs[0].buf = (rt_uint8_t *)&desc[index].vsel_reg;
	msgs[0].len = 1;

	/* read the value */
	msgs[1].addr  = sr->slave_addr;
	msgs[1].flags = RT_I2C_RD;
	msgs[1].buf = val;
	msgs[1].len = 2;

	if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
		rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
		return -RT_ERROR;
	}

	val_temp = ((rt_uint16_t)val[1] << 8) | val[0];

	val_temp &= desc[index].vsel_msk;
	val_temp >>= (ffs(desc[index].vsel_msk) - 1);

	ret = regulator_desc_list_voltage_linear_range(&desc[index], val_temp);

	return ret;
}

static rt_err_t regulator_independ_set_voltage(struct rt_regulator_node *reg, int min_uvolt, int max_uvolt)
{
	int index, sel;
	rt_uint16_t val_temp = 0;
	rt_uint8_t val[3], cmd;
	struct rt_i2c_msg msgs[3];
	struct regulator_desc *desc;
	struct regulator_dynamic *rd = (struct regulator_dynamic *)reg;
	struct spacemit_regulator *sr = rd->sr;

	desc = (struct regulator_desc *)sr->priv_data;
	/* regulator index */
	index = reg->param->index;

	if (strncmp(reg->supply_name, "adcdc", 5) == 0) {
		val[0] = 0x0;
		if (strncmp(reg->supply_name, "adcdc1", 6) == 0)
			val[1] = 0x1;
		else if (strncmp(reg->supply_name, "adcdc2", 6) == 0)
			val[1] = 0x0;;
		msgs[0].addr  = sr->slave_addr;
		msgs[0].flags = RT_I2C_WR;
		msgs[0].buf = val;
		msgs[0].len = 2;

		if (rt_i2c_transfer(sr->handle_driver, msgs, 1) != 1) {
			rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
			return -RT_ERROR;
		}
	}

	sel = regulator_map_voltage_linear_range(&desc[index], min_uvolt, max_uvolt);
	if (sel >= 0) {
		sel <<= ffs(desc[index].vsel_msk) - 1;

		msgs[0].addr  = sr->slave_addr;
		msgs[0].flags = RT_I2C_WR;
		msgs[0].buf = (rt_uint8_t *)&desc[index].vsel_reg;
		msgs[0].len = 1;

		/* read the value */
		msgs[1].addr  = sr->slave_addr;
		msgs[1].flags = RT_I2C_RD;
		msgs[1].buf = val;
		msgs[1].len = 2;

		if (rt_i2c_transfer(sr->handle_driver, msgs, 2) != 2) {
			rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
			return -RT_ERROR;
		}

		val_temp = ((rt_uint16_t)val[1] << 8) | val[0];
		val_temp &= ~(desc[index].vsel_msk);
		val_temp |= sel;

		/* set the value */
		msgs[0].addr  = sr->slave_addr;
		msgs[0].flags = RT_I2C_WR;
		val[0] = desc[index].vsel_reg;
		val[2] = (val_temp & 0xff00) >> 8;
		val[1] = val_temp & 0xff;
		msgs[0].buf = val;
		msgs[0].len = 3;

		if (rt_i2c_transfer(sr->handle_driver, msgs, 1) != 1) {
			rt_kprintf("%s:%d, transfer error\n", __func__, __LINE__);
			return -RT_ERROR;
		}
	} else {
		rt_kprintf("%s:%d, set the wrong voltage\n", __func__, __LINE__);
		return -RT_EINVAL;
	}

	return RT_EOK;
}

static const struct rt_regulator_ops regulator_independ_ops =
{
	.enable = regulator_independ_enable,
	.disable = regulator_independ_disable,
	.get_voltage = regulator_independ_get_voltage,
	.set_voltage = regulator_independ_set_voltage
};

static rt_int32_t spacemit_regulator_probe(void)
{
	int ret;
	rt_int32_t i, val, j = 0;
	char *string, *strend;
	const char *selected_pmic;
	rt_int32_t size;
	struct spacemit_regulator *sr;
	struct rt_regulator_node *rnp;
	struct dtb_node *compatible_node, *child_node;
	struct dtb_node *dtb_head_node = get_dtb_node_head();
	rt_uint32_t reg, mask;

	for (i = 0; i < sizeof(__compatible) / sizeof(__compatible[0]); ++i) {
		compatible_node = dtb_node_find_compatible_node(dtb_head_node,
			__compatible[i].compatible);
		if (compatible_node != RT_NULL) {
			sr = (struct spacemit_regulator *)rt_calloc(1, sizeof(struct spacemit_regulator));
			if (sr == RT_NULL) {
				rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
				return -RT_EINVAL;
			}

			/* get the handle driver */
			for_each_property_string_extend(compatible_node, "bind_driver", string, strend, size) {
				sr->handle_driver = rt_i2c_bus_device_find(string);
				if (sr->handle_driver == RT_NULL) {
					rt_kprintf("%s:%d, the bind driver has not registered\n", __func__, __LINE__);
					return -RT_EINVAL;
				}
			}

			/* get the slave address */
			dtb_node_read_u32_array(compatible_node, "slave_addr", &sr->slave_addr, 1);

			sr->priv_data = (void *)__compatible[i].data;

			if (dtb_node_get_dtb_node_compatible_match(compatible_node, "p1-regulator")) {
				/* this is the parent node */
				dtb_node_read_u32_array(compatible_node, "num_regulators", &val, 1);

				sr->rd = (struct regulator_dynamic *)rt_calloc(val, sizeof(struct regulator_dynamic));
				if (sr->rd == RT_NULL) {
					rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
					return -RT_EINVAL;
				}

				child_node = compatible_node;

				j = 0;

                		for_each_node_child(child_node) {
					if(!dtb_node_get_dtb_node_compatible_match(child_node, "regulator-dynamic"))
						continue;

					regulator_dtb_parse(child_node, &sr->rd[j].param);

					rnp = &sr->rd[j].parent;
					rnp->supply_name = sr->rd[j].param.name;
					rnp->ops = &regulator_dynamic_ops;
					rnp->param = &sr->rd[j].param;
					rnp->dev = &sr->rd[j].dev;
					rnp->dev->node = child_node;
					rnp->priv = &sr->rd[j];

					sr->rd[j].sr = sr;

					/* register the regulator */
					ret = rt_regulator_register(rnp);
					if (ret) {
						rt_kprintf("%s:%d, register regulator error\n", __func__, __LINE__);
						return -RT_EINVAL;
					}

					++j;
				}
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(__dcdc_compatible); ++i) {
		compatible_node = dtb_node_find_compatible_node(dtb_head_node,
			__dcdc_compatible[i].compatible);
		if (compatible_node != RT_NULL) {
			sr = (struct spacemit_regulator *)rt_calloc(1, sizeof(struct spacemit_regulator));
			if (sr == RT_NULL) {
				rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
				return -RT_EINVAL;
			}

			/* get the handle driver */
			for_each_property_string_extend(compatible_node, "bind_driver", string, strend, size) {
				sr->handle_driver = rt_i2c_bus_device_find(string);
				if (sr->handle_driver == RT_NULL) {
					rt_kprintf("%s:%d, the bind driver has not registered\n", __func__, __LINE__);
					return -RT_EINVAL;
				}
			}

			/* get the slave address */
			dtb_node_read_u32_array(compatible_node, "slave_addr", &sr->slave_addr, 1);

			sr->priv_data = (void *)__dcdc_compatible[i].data;

			sr->rd = (struct regulator_dynamic *)rt_calloc(1, sizeof(struct regulator_dynamic));
			if (sr->rd == RT_NULL) {
				rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
				return -RT_EINVAL;
			}

			/* only has one node */
			regulator_dtb_parse(compatible_node, &sr->rd->param);

			rnp = &sr->rd->parent;
			rnp->supply_name = sr->rd->param.name;
			rnp->ops = &regulator_independ_ops;
			rnp->param = &sr->rd->param;
			rnp->dev = &sr->rd->dev;
			rnp->dev->node = compatible_node;

			sr->rd->sr = sr;
			rnp->priv = sr->rd;

			/* register the regulator */
			ret = rt_regulator_register(rnp);
			if (ret) {
				rt_kprintf("%s:%d, register regulator error\n", __func__, __LINE__);
				return -RT_EINVAL;
			}
		}
	}

	return 0;
}
INIT_DEVICE_EXPORT(spacemit_regulator_probe);

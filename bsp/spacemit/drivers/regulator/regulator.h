#ifndef __RT_SPACEMIT_REGULATOR_H__
#define __RT_SPACEMIT_REGULATOR_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/regulator.h>

#define P1_BUCK_VSEL_MASK		0xff
#define P1_BUCK_EN_MASK		        0x1

#define P1_BUCK1_CTRL_REG		0x47
#define P1_BUCK2_CTRL_REG		0x4a
#define P1_BUCK3_CTRL_REG		0x4d
#define P1_BUCK4_CTRL_REG		0x50
#define P1_BUCK5_CTRL_REG		0x53
#define P1_BUCK6_CTRL_REG		0x56

#define P1_BUCK1_VSEL_REG		0x48
#define P1_BUCK2_VSEL_REG		0x4b
#define P1_BUCK3_VSEL_REG		0x4e
#define P1_BUCK4_VSEL_REG		0x51
#define P1_BUCK5_VSEL_REG		0x54
#define P1_BUCK6_VSEL_REG		0x57

#define P1_BUCK1_SVSEL_REG		0x49
#define P1_BUCK2_SVSEL_REG		0x4c
#define P1_BUCK3_SVSEL_REG		0x4f
#define P1_BUCK4_SVSEL_REG		0x52
#define P1_BUCK5_SVSEL_REG		0x55
#define P1_BUCK6_SVSEL_REG		0x58

#define P1_BUCK_SVSEL_MASK		0xff

#define P1_ALDO1_CTRL_REG		0x5b
#define P1_ALDO2_CTRL_REG		0x5e
#define P1_ALDO3_CTRL_REG		0x61
#define P1_ALDO4_CTRL_REG		0x64

#define P1_ALDO1_VOLT_REG		0x5c
#define P1_ALDO2_VOLT_REG		0x5f
#define P1_ALDO3_VOLT_REG		0x62
#define P1_ALDO4_VOLT_REG		0x65

#define P1_ALDO1_SVOLT_REG		0x5d
#define P1_ALDO2_SVOLT_REG		0x60
#define P1_ALDO3_SVOLT_REG		0x63
#define P1_ALDO4_SVOLT_REG		0x66
#define P1_ALDO_SVSEL_MASK		0x7f

#define P1_ALDO_EN_MASK		        0x1
#define P1_ALDO_VSEL_MASK		0x7f

#define P1_DLDO1_CTRL_REG		0x67
#define P1_DLDO2_CTRL_REG		0x6a
#define P1_DLDO3_CTRL_REG		0x6d
#define P1_DLDO4_CTRL_REG		0x70
#define P1_DLDO5_CTRL_REG		0x73
#define P1_DLDO6_CTRL_REG		0x76
#define P1_DLDO7_CTRL_REG		0x79

#define P1_DLDO1_VOLT_REG		0x68
#define P1_DLDO2_VOLT_REG		0x6b
#define P1_DLDO3_VOLT_REG		0x6e
#define P1_DLDO4_VOLT_REG		0x71
#define P1_DLDO5_VOLT_REG		0x74
#define P1_DLDO6_VOLT_REG		0x77
#define P1_DLDO7_VOLT_REG		0x7a

#define P1_DLDO1_SVOLT_REG		0x69
#define P1_DLDO2_SVOLT_REG		0x6c
#define P1_DLDO3_SVOLT_REG		0x6f
#define P1_DLDO4_SVOLT_REG		0x72
#define P1_DLDO5_SVOLT_REG		0x75
#define P1_DLDO6_SVOLT_REG		0x78
#define P1_DLDO7_SVOLT_REG		0x7b

#define P1_DLDO_SVSEL_MASK		0x7f

#define P1_DLDO_EN_MASK		        0x1
#define P1_DLDO_VSEL_MASK		0x7f

#define P1_GPIO_ODR_REG			0x1
#define P1_GPIO_ODR_MSK			0x3f

#define IS6608_BUCK1_VOLT_REG		0x21
#define IS6608_BUCK1_VSEL_MSK		0xfff
#define TDA38740_BUCK1_VOLT_REG		0x21
#define TDA38740_BUCK1_VSEL_MSK		0xfff
#define IS6615A_BUCK1_VOLT_REG		0x21
#define IS6615A_BUCK1_VSEL_MSK		0xfff
#define AU4562_BUCK1_VOLT_REG		0x21
#define AU4562_BUCK1_VSEL_MSK		0xfff

struct regulator_linear_range {
	unsigned int min;
	unsigned int min_sel;
	unsigned int max_sel;
	unsigned int step;
};

struct regulator_desc {
	int n_voltages;
	int vsel_reg;
	int vsel_msk;
	int enable_reg;
	int enable_msk;
	int vsel_sleep_reg;
	int vsel_sleep_msk;
	int n_linear_ranges;
	const struct regulator_linear_range *linear_ranges;
};

enum k3_regulator_index {
	EXTERN_P5V,
	EXTERN_DCDC,
	EXTERN_1V8,
	EXTERN_3V3,
	EXTERN_X100,
	EXTERN_A100,
	P1_ID_DCDC1_2,
	P1_ID_DCDC3,
	P1_ID_DCDC4,
	P1_ID_DCDC5,
	P1_ID_DCDC6,
	P1_ID_LDO1,
	P1_ID_LDO2,
	P1_ID_LDO3,
	P1_ID_LDO4,
	P1_ID_LDO5,
	P1_ID_LDO6,
	P1_ID_LDO7,
	P1_ID_LDO8,
	P1_ID_LDO9,
	P1_ID_LDO10,
	P1_ID_LDO11,
	EXTERN_LEAF_A100,
	EXTERN_LEAF_X100,
};

struct spacemit_regulator;

struct regulator_dynamic {
	struct rt_regulator_node parent;
	struct rt_regulator_param param;
	struct rt_device dev;
	struct spacemit_regulator *sr;
	rt_uint32_t enabled;
	rt_uint32_t voltage;
};

struct spacemit_regulator {
	struct regulator_dynamic *rd;
	/* using i2c */
	int slave_addr;
	struct rt_i2c_bus_device *handle_driver;
	void *priv_data;
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)    (sizeof(x) / sizeof(x[0]))
#endif

#ifdef RT_USING_PM
void tda38740_pm_device_register(struct rt_device *dev);
#endif

/* Initialize struct linear_range for regulators */
#define REGULATOR_LINEAR_RANGE(_min_uV, _min_sel, _max_sel, _step_uV)   \
{                                                                       \
        .min            = _min_uV,                                      \
        .min_sel        = _min_sel,                                     \
        .max_sel        = _max_sel,                                     \
        .step           = _step_uV,                                     \
}

#define REGULATOR_DESC_COMMON(_id, _nv, _vr, _vm, _er, _em, _vs, _vsm, _lr) \
	[_id] = {							\
		.n_voltages     = (_nv),				\
		.vsel_reg       = (_vr),				\
		.vsel_msk       = (_vm),				\
		.vsel_sleep_reg = (_vs),				\
		.vsel_sleep_msk = (_vsm),				\
		.enable_reg	= (_er),				\
		.enable_msk	= (_em),				\
		.linear_ranges	= (_lr),				\
		.n_linear_ranges	= ARRAY_SIZE(_lr),		\
	}

#endif /* __RT_SPACEMIT_REGULATOR_H__ */

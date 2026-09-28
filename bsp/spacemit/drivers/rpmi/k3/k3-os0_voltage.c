/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>
#include <dtb_head.h>
#include <drivers/dtb_node.h>
#include <drivers/regulator.h>
#include <drivers/regulator_dm.h>
#include <regulator.h>
#include "../spacemit-rpmi.h"
#include "k3-os0_voltage.h"

#define MAX_RPMI_VOTAGE_NUMBER		32

#define PARENT_ID_MASK		0xffff
#define MIN_SEL_MASK		0xffff0000

#define PARENT_ID_OFFSET	(0)
#define MIN_SEL_OFFSET		(16)

struct rt_regulator
{
	struct rt_regulator_node *reg_np;
};

struct rt_regulator *regulator_ptr[MAX_RPMI_VOTAGE_NUMBER];

struct rpmi_voltage_data rpmi_vdata[MAX_RPMI_VOTAGE_NUMBER];

#define for_each_property_of_node(dn, pp) \
	for (pp = dn->properties; pp != NULL; pp = pp->next)

static rt_int32_t  _k3_os0_voltage_init(void *priv)
{
	int j = 0;
	/* platform releated, get the registers or other thing what you want */
	struct spacemit_rpmi_voltage_config *config = priv;
	struct rt_device *dev;
	struct dtb_property *pp;
	struct dtb_node *np, *fdt_aliases;
	struct dtb_node *dtb_head_node = get_dtb_node_head();
	phandle supply_phandle;
	struct spacemit_regulator *sr;
	struct regulator_dynamic *dr;
	struct rt_regulator_node *reg_np;
	struct rpmi_voltage_level_liner *rpmi_level_liner;
	int index;

	/* get the alaises and find all the regulators */
	fdt_aliases = dtb_node_get_dtb_node_by_path(dtb_head_node, "/aliases");
	if (fdt_aliases < 0) {
		rt_kprintf("Get alias error\n");
		return -RT_EINVAL;
	}

	for_each_property_of_node(fdt_aliases, pp) {

		if ((rt_strncmp(pp->name, "regulator-fixed", 15) != 0) && (rt_strncmp(pp->name, "regulator-dynamic", 17) != 0))
			continue;
		
		dtb_node_read_u32(fdt_aliases, pp->name, &supply_phandle);
		
		np = dtb_node_find_node_by_phandle(supply_phandle);
		if (!np) {
			rt_kprintf("%s:%d: %s, get node error\n", __func__, __LINE__, pp->value);
			return -RT_EINVAL;
		}

		reg_np = rt_dtb_data(np);
		dr = reg_np->priv;
		sr = dr->sr;
		index = reg_np->param->index;

		++j;

		/* if the regulator is the fixed, then the voltage is fix also */
		if (strncmp(pp->name, "regulator-fixed", 15) != 0) {
			rpmi_vdata[index].level_count = (((struct regulator_desc *)sr->priv_data)[index]).n_linear_ranges;

			rpmi_vdata[index].voltage_level_array = rt_calloc(rpmi_vdata[index].level_count,
					sizeof(struct rpmi_voltage_level_liner));

			for (int i = 0; i < rpmi_vdata[index].level_count; ++i) {
				rpmi_level_liner = (struct rpmi_voltage_level_liner *)&(rpmi_vdata[index].voltage_level_array[i * sizeof(struct rpmi_voltage_level_liner) / sizeof(rt_uint32_t)]);

				rpmi_level_liner->min_voltage = (((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[i].min;
				rpmi_level_liner->max_voltage =
					 ((((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[i].max_sel -
					 (((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[i].min_sel) *
					 (((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[i].step +
					 (((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[i].min;
				rpmi_level_liner->step = (((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[i].step;
			}

			rpmi_vdata[index].transition_latency_ms |= ((((struct regulator_desc *)sr->priv_data)[index]).linear_ranges[0].min_sel << MIN_SEL_OFFSET) & MIN_SEL_MASK;
		} else {
			rpmi_vdata[index].level_count = 1;
			rpmi_vdata[index].voltage_level_array = rt_calloc(rpmi_vdata[index].level_count,
					sizeof(struct rpmi_voltage_level_liner));

			rpmi_level_liner = (struct rpmi_voltage_level_liner *)&rpmi_vdata[index].voltage_level_array[0];
			rpmi_level_liner->min_voltage = reg_np->param->min_uvolt;
			rpmi_level_liner->max_voltage = reg_np->param->max_uvolt;
			rpmi_level_liner->step = 0;
		}

		rpmi_vdata[index].voltage_type = (1 << RPMI_VOLTAGE_TYPE_LINEAR);
		rpmi_vdata[index].name = reg_np->param->name;

		/* get the regulator */
		regulator_ptr[index] = rt_regulator_get(config->node, rpmi_vdata[index].name);

		if (!reg_np->parent)
			rpmi_vdata[index].parent_id = -1;
		else {
			rpmi_vdata[index].parent_id = reg_np->parent->param->index;
		}

		/* used for parent id */
		rpmi_vdata[index].transition_latency_ms |= rpmi_vdata[index].parent_id & PARENT_ID_MASK;
	}

	config->domain_count = j;
	config->voltage_data = rpmi_vdata;

	return 0;
}

/** Set the voltage state enable/disable/others */
static enum rpmi_error spacemit_set_config(void *priv, rpmi_uint32_t domain_id, enum rpmi_voltage_state state)
{
	struct rt_regulator *reg = regulator_ptr[domain_id];
	int ret;
	switch (state) {
		case RPMI_VOLTAGE_STATE_DISABLED:
			ret = rt_regulator_disable(reg);
			break;
		case RPMI_VOLTAGE_STATE_ENABLED:
			ret = rt_regulator_enable(reg);
			break;
		default:
			ret = -RT_EINVAL;
			break;
	}
	if (!ret)
		ret = RPMI_SUCCESS;
	else
		ret = RPMI_ERR_FAILED;

	return ret;
}

static enum rpmi_error spacemit_get_config(void *priv, rpmi_uint32_t domain_id, enum rpmi_voltage_state *state)
{
	struct rt_regulator *reg = regulator_ptr[domain_id];
	if (!reg)
		return RPMI_ERR_INVALID_PARAM;
	if (rt_regulator_is_enabled(reg))
		*state = RPMI_VOLTAGE_STATE_ENABLED;
	else
		*state = RPMI_VOLTAGE_STATE_DISABLED;
	return 0;
}

static enum rpmi_error spacemit_set_voltage_level(void *priv, rpmi_uint32_t domain_id, rpmi_uint32_t level)
{
	struct rt_regulator *reg = regulator_ptr[domain_id];
	int ret;

	ret = rt_regulator_set_voltage(reg, level, level);
	if (!ret)
		ret = RPMI_SUCCESS;
	else
		ret = RPMI_ERR_INVALID_PARAM;

	return ret;
}

static enum rpmi_error spacemit_get_voltage_level(void *priv, rpmi_uint32_t domain_id, rpmi_uint32_t *level)
{
	struct rt_regulator *reg = regulator_ptr[domain_id];

	*level = rt_regulator_get_voltage(reg);
	return 0;
}

static struct rpmi_voltage_platform_ops k3_os0_voltage_pops = {
	.set_config = spacemit_set_config,
	.get_config = spacemit_get_config,
	.set_voltage_level = spacemit_set_voltage_level,
	.get_voltage_level = spacemit_get_voltage_level,
};

static struct spacemit_rpmi_voltage_ops k3_os0_voltage_ops = {
	.name = "k3-os0-rpmi-voltage",
	.init = _k3_os0_voltage_init,
	.voltage_ops = &k3_os0_voltage_pops,
};

static rt_int32_t k3_os0_voltage_init(void)
{
	rt_list_init(&k3_os0_voltage_ops.list);

	spacemit_rpmi_voltage_register(&k3_os0_voltage_ops.list);

	return 0;
}
INIT_COMPONENT_EXPORT(k3_os0_voltage_init);

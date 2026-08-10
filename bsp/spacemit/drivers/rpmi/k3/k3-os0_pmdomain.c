/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <riscv-ops.h>
#include <dtb_head.h>
#include <drivers/dtb_node.h>
#include "../spacemit-rpmi.h"

#define DEVICE_POWER_STATE_OFFSET 0xF0
#define AUDIO_DOMAIN_INDEX	0x2

struct rt_domain_data {
	uint32_t offset;
	uint32_t bit_hw_mode;
	uint32_t bit_sleep2;
	uint32_t bit_sleep1;
	uint32_t bit_isolation;
	uint32_t bit_pwr_stat;
	uint32_t bit_hw_pwr_stat;
	uint32_t bit_auto_pwr_on;
	uint32_t use_hw;
	int dummy;
	enum rpmi_device_power_state current_state;
};

static rt_int32_t  _k3_os0_domain_init(void *priv)
{
	int ret = 0;
	/* platform releated, get the registers or other thing what you want */
	struct spacemit_rpmi_domain_config *config = priv;
	struct dtb_property *pp;
	struct dtb_node *node;
	struct rt_domain_data *ptr;
	struct rpmi_device_power_attrs *attr;

	/* get the domain count */
	dtb_node_read_u32(config->node, "num_domains", &config->domain_count);

	/* get the register base */
	config->base = (void *)dtb_node_get_addr_index(config->node, 0);
	if (config->base < 0) {
		rt_kprintf("%s:%d, get power domain base failed\n", __func__, __LINE__);
		return -RT_EINVAL;
	}

	config->priv = (void *)rt_calloc(config->domain_count, sizeof(struct rt_domain_data));
	if (!config->priv) {
		rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
		return -RT_EINVAL;
	}

	config->domain_data = (struct rpmi_device_power_attrs *)rt_calloc(config->domain_count,
				sizeof(struct rpmi_device_power_attrs));
	if (!config->domain_data) {
		rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
		return -RT_EINVAL;
	}

	node = config->node;
	ptr = (struct rt_domain_data *)config->priv;
	attr = (struct rpmi_device_power_attrs *)config->domain_data;

	for_each_node_child(node) {
		attr->name = node->name;
		ret = dtb_node_read_u32(node, "bit_isolation", &ptr->bit_isolation);
		if (ret) {
			/* dummy power domain */
			ptr->dummy = 1;
		} else {
			dtb_node_read_u32(node, "bit_sleep1", &ptr->bit_sleep1);
			dtb_node_read_u32(node, "bit_sleep2", &ptr->bit_sleep2);
			dtb_node_read_u32(node, "bit_hw_mode", &ptr->bit_hw_mode);
			dtb_node_read_u32(node, "bit_pwr_stat", &ptr->bit_pwr_stat);
			dtb_node_read_u32(node, "bit_hw_pwr_stat", &ptr->bit_hw_pwr_stat);
			dtb_node_read_u32(node, "offset", &ptr->offset);
			dtb_node_read_u32(node, "bit_auto_pwr_on", &ptr->bit_auto_pwr_on);
			dtb_node_read_u32(node, "use_hw", &ptr->use_hw);
		}

		++ptr;
		++attr;
	}

	return 0;
}

/** Set the power domain state ON_STATE */
static enum rpmi_error spacemit_set_state(void *priv, rpmi_uint32_t domain_id, enum rpmi_device_power_state state)
{
	struct spacemit_rpmi_domain_config *config = priv;
	struct rt_domain_data *ptr = config->priv;
	rt_uint32_t val;
	rt_int32_t loop;

	if (state == RPMI_DEVICE_POWER_STATE_ON) {
		/* Audio power switch is managed by RCPU itself; kernel must not control it */
		if ((ptr[domain_id].dummy) || (domain_id == AUDIO_DOMAIN_INDEX)) {
			ptr[domain_id].current_state = 1;
			return 0;
		}

		if (ptr[domain_id].use_hw == 0) {
			val = readl((config->base + ptr[domain_id].offset));
			val |= (1 << ptr[domain_id].bit_sleep1);
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(20);

			val = readl((config->base + ptr[domain_id].offset));
			val |= (1 << ptr[domain_id].bit_sleep2) | (1 << ptr[domain_id].bit_sleep1);
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(20);

			val = readl((config->base + ptr[domain_id].offset));
			val |= (1 << ptr[domain_id].bit_isolation);
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(10);

			for (loop = 10000; loop >= 0; --loop) {
				val = readl((config->base + DEVICE_POWER_STATE_OFFSET));
				if ((val & (1 << ptr[domain_id].bit_pwr_stat)) != 0)
					break;
				rt_hw_us_delay(4);
			}

			if (loop < 0) {
				rt_kprintf("%s:%d\n", __func__, __LINE__);
				return -RT_ETIMEOUT;
			}
		} else {
			val = readl((config->base + ptr[domain_id].offset));
			val |= (1 << ptr[domain_id].bit_hw_mode) |
			       (1 << ptr[domain_id].bit_auto_pwr_on);
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(290);

			for (loop = 10000; loop >= 0; --loop) {
				val = readl((config->base + DEVICE_POWER_STATE_OFFSET));
				if ((val & (1 << ptr[domain_id].bit_hw_pwr_stat)) != 0)
					break;
				rt_hw_us_delay(4);
			}

			if (loop < 0) {
				rt_kprintf("%s:%d\n", __func__, __LINE__);
				return -RT_ETIMEOUT;
			}
		}

		ptr[domain_id].current_state = 1;
	} else {
		/* Audio power switch is managed by RCPU itself; kernel must not control it */
		if ((ptr[domain_id].dummy) || (domain_id == AUDIO_DOMAIN_INDEX)) {
			ptr[domain_id].current_state = 0;
			return 0;
		}

		if (ptr[domain_id].use_hw == 0) {
			val = readl((config->base + ptr[domain_id].offset));
			val &= ~(1 << ptr[domain_id].bit_isolation);
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(15);

			val = readl((config->base + ptr[domain_id].offset));
			val &= ~((1 << ptr[domain_id].bit_sleep1) | (1 << ptr[domain_id].bit_sleep2));
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(20);

			for (loop = 10000; loop >= 0; --loop) {
				val = readl((config->base + DEVICE_POWER_STATE_OFFSET));
				if ((val & (1 << ptr[domain_id].bit_pwr_stat)) == 0)
					break;
				rt_hw_us_delay(4);
			}

			if (loop < 0) {
				rt_kprintf("%s:%d\n", __func__, __LINE__);
				return -RT_ETIMEOUT;
			}
		} else {
			val = readl((config->base + ptr[domain_id].offset));
			val &= ~(1 << ptr[domain_id].bit_auto_pwr_on);
			val &= ~(1 << ptr[domain_id].bit_hw_mode);
			writel(val, (config->base + ptr[domain_id].offset));
			rt_hw_us_delay(290);

			for (loop = 10000; loop >= 0; --loop) {
				val = readl((config->base + DEVICE_POWER_STATE_OFFSET));
				if ((val & (1 << ptr[domain_id].bit_hw_pwr_stat)) == 0)
					break;
				rt_hw_us_delay(4);
			}

			if (loop < 0) {
				rt_kprintf("%s:%d\n", __func__, __LINE__);
				return -RT_ETIMEOUT;
			}
		}

		ptr[domain_id].current_state = 0;
	}

	return 0;
}

static enum rpmi_error spacemit_get_state(void *priv, rpmi_uint32_t domain_id, enum rpmi_device_power_state *state)
{
	struct spacemit_rpmi_domain_config *config = priv;
	struct rt_domain_data *ptr = config->priv;
	rt_uint32_t val;

	if (!ptr[domain_id].dummy) {
		val = readl((config->base + DEVICE_POWER_STATE_OFFSET));
		if (ptr[domain_id].use_hw == 0)
			ptr[domain_id].current_state = (val & (1 << ptr[domain_id].bit_pwr_stat)) ? 1 : 0;
		else
			ptr[domain_id].current_state = (val & (1 << ptr[domain_id].bit_hw_pwr_stat)) ? 1 : 0;
	}
	*state = ptr[domain_id].current_state;

	return 0;
}

static struct rpmi_domain_platform_ops k3_os0_domain_pops = {
	.set_state = spacemit_set_state,
	.get_state = spacemit_get_state,
};

static struct spacemit_rpmi_domain_ops k3_os0_device_power_ops = {
	.name = "k3-os0-rpmi-domain",
	.init = _k3_os0_domain_init,
	.domain_ops = &k3_os0_domain_pops,
};

static rt_int32_t k3_os0_device_power_init(void)
{
	rt_list_init(&k3_os0_device_power_ops.list);

	spacemit_rpmi_domain_register(&k3_os0_device_power_ops.list);

	return 0;
}
INIT_DEVICE_EXPORT(k3_os0_device_power_init);

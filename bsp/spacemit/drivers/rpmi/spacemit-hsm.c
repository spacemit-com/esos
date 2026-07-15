/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtservice.h>
#include <dtb_node.h>
#include <riscv-ops.h>
#include <register_defination.h>
#include "spacemit-rpmi.h"

static rt_list_t rpmi_hsm_list = RT_LIST_OBJECT_INIT(rpmi_hsm_list);
extern struct rt_mutex rpmi_hsm_mtx;

struct spacemit_multiple_os *multiple_os_array;

/* hsm releated */
static enum rpmi_hart_hw_state hsm_get_hw_state(void* priv,
	rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->hsm_ops->hart_get_hw_state(priv, hart_index);
}

static enum rpmi_error hsm_hart_start_prepare(void* priv,
	rpmi_uint32_t hart_index,
	rpmi_uint64_t start_addr)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->hsm_ops->hart_start_prepare(priv, hart_index, start_addr);
}

static void hsm_hart_start_finalize(void* priv,
	rpmi_uint32_t hart_index,
	rpmi_uint64_t start_addr)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	config->hsm_ops->hart_start_finalize(priv, hart_index, start_addr);
}

static enum rpmi_error hsm_hart_stop_prepare(void* priv,
	rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->hsm_ops->hart_stop_prepare(priv, hart_index);
}

static void hsm_hart_stop_finalize(void* priv, rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	config->hsm_ops->hart_stop_finalize(priv, hart_index);
}

static enum rpmi_error hsm_hart_suspend_prepare(
	void* priv,
	rpmi_uint32_t hart_index,
	const struct rpmi_hsm_suspend_type* suspend_type,
	rpmi_uint64_t resume_addr)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->hsm_ops->hart_suspend_prepare(priv, hart_index, suspend_type, resume_addr);
}

static void hsm_hart_suspend_finalize(
	void* priv,
	rpmi_uint32_t hart_index,
	const struct rpmi_hsm_suspend_type* suspend_type,
	rpmi_uint64_t resume_addr)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->hsm_ops->hart_suspend_finalize(priv, hart_index, suspend_type, resume_addr);
}


/* HSM platform operations */
static struct rpmi_hsm_platform_ops hsm_ops = {
	.hart_get_hw_state = hsm_get_hw_state,
	.hart_start_prepare = hsm_hart_start_prepare,
	.hart_start_finalize = hsm_hart_start_finalize,
	.hart_stop_prepare = hsm_hart_stop_prepare,
	.hart_stop_finalize = hsm_hart_stop_finalize,
	.hart_suspend_prepare = hsm_hart_suspend_prepare,
	.hart_suspend_finalize = hsm_hart_suspend_finalize
};

static rt_int32_t spacemit_rpmi_get_hsm_config(struct dtb_node *node, void *con, char *match)
{
	rt_int32_t i = 0, ret = 0;
	struct dtb_node *node_ptr = node;
	rt_int32_t property_size;
	rt_uint32_t u32_value;
	rt_uint32_t *u32_ptr;
	const void* prop_data;
	rt_int32_t prop_len;
	struct spacemit_rpmi_config *c = (struct spacemit_rpmi_config *)con;
	struct spacemit_rpmi_hsm_config *config = &c->hsm_config;
	struct spacemit_rpmi_hsm_ops *pos = RT_NULL;

	config->node = node;
	config->event = multiple_os_array->multiple_event;
	config->mulos = multiple_os_array;

	multiple_os_array->hsm[multiple_os_array->os_count++] = config;

	/* get the os index */
	prop_data = dtb_node_get_property(node, "bootcore_index", &u32_value);
	if (!prop_data) {
		rt_kprintf("%s:%d, get os index failed\n", __func__, __LINE__);
		return -RT_EINVAL;
	}

	config->bootcore_index = fdt32_to_cpu(*(uint32_t*)prop_data);

	/* get the start hardid */
	for_each_property_cell(node, "hartids", u32_value, u32_ptr, property_size) {
		config->hartids[i++] = u32_value;
	}

	config->hartcnt = i;

	/* get the suspend type */
	i = 0;
	for_each_node_child(node_ptr) {
		if (!dtb_node_get_dtb_node_compatible_match(node_ptr, "riscv,idle-state"))
			continue;

		/* get the property */
		prop_data = dtb_node_get_property(node_ptr, "riscv,sbi-suspend-param", &prop_len);
		if (prop_data && prop_len >= sizeof(uint32_t)) {
			config->stype[i].type = fdt32_to_cpu(*(uint32_t*)prop_data);
		}

		prop_data = dtb_node_get_property(node_ptr, "entry-latency-us", &prop_len);
		if (prop_data && prop_len >= sizeof(uint32_t)) {
			config->stype[i].info.entry_latency_us = fdt32_to_cpu(*(uint32_t*)prop_data);
		}

		prop_data = dtb_node_get_property(node_ptr, "exit-latency-us", &prop_len);
		if (prop_data && prop_len >= sizeof(uint32_t)) {
			config->stype[i].info.exit_latency_us = fdt32_to_cpu(*(uint32_t*)prop_data);
		}

		prop_data = dtb_node_get_property(node_ptr, "min-residency-us", &prop_len);
		if (prop_data && prop_len >= sizeof(uint32_t)) {
			config->stype[i].info.min_residency_us = fdt32_to_cpu(*(uint32_t*)prop_data);
		}

		prop_data = dtb_node_get_property(node_ptr, "wakeup-latency-us", &prop_len);
		if (prop_data && prop_len >= sizeof(uint32_t)) {
			config->stype[i].info.wakeup_latency_us = fdt32_to_cpu(*(uint32_t*)prop_data);
		}

		if (dtb_node_get_dtb_node_property(node_ptr, "local-timer-stop", RT_NULL))
			config->stype[i].info.flags = RPMI_HSM_SUSPEND_INFO_FLAGS_TIMER_STOP;
		++i;
	}

	config->type_cnt = i;

	/* support system suspend ? */
	if (dtb_node_get_dtb_node_property(node, "risv,support-syssup", RT_NULL)) {
		config->support_syssup = 1;
	}

	/* initialize the platform related resources */
	rt_mutex_take(&rpmi_hsm_mtx, RT_WAITING_FOREVER);
	rt_list_for_each_entry(pos, &rpmi_hsm_list, list) {
		if (!rt_strcmp(pos->name, match))
			break;
	}
	rt_mutex_release(&rpmi_hsm_mtx);

	if (pos) {
		config->hsm_ops = pos->hsm_ops;
		config->syssup_ops = pos->syssup_ops;
		ret = pos->init((void *)config);
	}

	return ret;
}

/* system suspend releated */
struct rpmi_system_suspend_type system_suspend_types[2] = {
	{ .type = RPMI_SYSSUSP_TYPE_SUSPEND_TO_RAM,
		.attr = RPMI_SYSSUSP_ATTRS_FLAGS_RESUMEADDR | RPMI_SYSSUSP_ATTRS_FLAGS_SUSPENDTYPE },
	{ .type = RPMI_SYSSUSP_TYPE_SUSPEND_TO_DISK,
		.attr = RPMI_SYSSUSP_ATTRS_FLAGS_SUSPENDTYPE },
};

/* System suspend platform operations (stubs for hardware-related functions) */
static enum rpmi_error syssusp_prepare(
		void* priv,
		rpmi_uint32_t hart_index,
		const struct rpmi_system_suspend_type* syssusp_type,
		rpmi_uint64_t resume_addr)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->syssup_ops->system_suspend_prepare(priv, hart_index, syssusp_type, resume_addr);
}

static rpmi_bool_t syssusp_ready(void* priv, rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->syssup_ops->system_suspend_ready(priv, hart_index);
}

static void syssusp_finalize(
		void* priv,
		rpmi_uint32_t hart_index,
		const struct rpmi_system_suspend_type* syssusp_type,
		rpmi_uint64_t resume_addr)
{
        /* Hardware-related stub */
	struct spacemit_rpmi_hsm_config *config = priv;

	config->syssup_ops->system_suspend_finalize(priv, hart_index, syssusp_type, resume_addr);
}

static rpmi_bool_t syssusp_can_resume(void* priv, rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->syssup_ops->system_suspend_can_resume(priv, hart_index);
}

static enum rpmi_error syssusp_resume(
		void* priv,
		rpmi_uint32_t hart_index,
		const struct rpmi_system_suspend_type* syssusp_type,
		rpmi_uint64_t resume_addr)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	return config->syssup_ops->system_suspend_resume(priv, hart_index, syssusp_type, resume_addr);
}

/* System suspend platform operations */
static struct rpmi_syssusp_platform_ops syssusp_ops = {
	.system_suspend_prepare = syssusp_prepare,
	.system_suspend_ready = syssusp_ready,
	.system_suspend_finalize = syssusp_finalize,
	.system_suspend_can_resume = syssusp_can_resume,
	.system_suspend_resume = syssusp_resume
};

static rt_int32_t spacemit_rpmi_register_hsm_service(void *con, struct rpmi_context *cntx)
{
	rt_int32_t ret;
	struct rpmi_hsm* hsm = NULL;
	struct rpmi_service_group *group = NULL;
	struct spacemit_rpmi_config *c = (struct spacemit_rpmi_config *)con;
	struct spacemit_rpmi_hsm_config *config = &c->hsm_config;

	hsm = rpmi_hsm_create(config->hartcnt, config->hartids, config->type_cnt, config->stype, &hsm_ops, config);
	if (!hsm) {
		rt_kprintf("create the hsm failed\n");
		return -RT_EINVAL;
	}

	/* Create HSM service group */
	group = rpmi_service_group_hsm_create(hsm);
	if (!group) {
		rt_kprintf("Failed to create HSM service group\n");
		return -RT_EINVAL;
	}

	/* Add HSM service group to context */
	ret = rpmi_context_add_group(cntx, group);
	if (ret != RPMI_SUCCESS) {
		rt_kprintf("Failed to add HSM service group (ret=%d)\n", ret);
		return -RT_EINVAL;
	}

	if (config->support_syssup) {
		/* create the system suspend services */
		group = rpmi_service_group_syssusp_create(hsm, 2, system_suspend_types,
				&syssusp_ops, config);
		if (!group) {
			rt_kprintf("ERROR: Failed to create System Suspend service group\n");
			return -RT_EINVAL;
		}

		if (rpmi_context_add_group(cntx, group) != RPMI_SUCCESS) {
                	rt_kprintf("ERROR: Failed to add System Suspend service group\n");
			return -RT_EINVAL;
		}
	}

	return 0;
}

struct spacemit_rpmi_func rpmi_hsm_func = {
	.rmpi_get_configuration = spacemit_rpmi_get_hsm_config,
	.rpmi_register_service = spacemit_rpmi_register_hsm_service,
};

rt_int32_t spacemit_rpmi_hsm_register(rt_list_t *node)
{
	rt_mutex_take(&rpmi_hsm_mtx, RT_WAITING_FOREVER);
	rt_list_insert_after(&rpmi_hsm_list, node);
	rt_mutex_release(&rpmi_hsm_mtx);

	return 0;
}

static void spacemit_multiple_os_poll(void *priv)
{
	int ret, i;
	rt_uint32_t e, msk = 0;
	unsigned long long _entry;
	struct spacemit_multiple_os *config = (struct spacemit_multiple_os *)priv;

	for (i = 0; i < config->os_count; ++i)
		msk |= (1 << config->hsm[i]->bootcore_index);

	while(1) {
		ret = rt_event_recv(config->multiple_event,
				/**
				 * bit0: os0 power event
				 * bit4: os1 power event
				 * bit8: os2 power event
				 * ....
				 */
				/* one os only by now */
				msk,
				RT_EVENT_FLAG_AND | RT_EVENT_FLAG_CLEAR,
				RT_WAITING_FOREVER, &e);

		/* Let rcpu1 enter low power mode */
		mbox_send_message(multiple_os_array->mtx_chan, &ret);

		/* wait rcpu1 power down */
		rt_sem_take(multiple_os_array->msem, RT_WAITING_FOREVER);

		if (multiple_os_array->hibernate_pending) {
			multiple_os_array->hibernate_pending = 0;
			/* trigger PM_SLEEP_MODE_SHUTDOWN so RCPU0 goes suspend to disk */
			rt_pm_request(PM_SLEEP_MODE_SHUTDOWN);
			rt_pm_release(RT_PM_DEFAULT_DEEPSLEEP_MODE);
			rt_pm_release(RT_PM_DEFAULT_SLEEP_MODE);
		} else {
			/* trigger the system suspend (STR) */
			rt_pm_release(RT_PM_DEFAULT_SLEEP_MODE);
		}

		/* will enter idle thread */
		rt_sem_take((rt_sem_t)multiple_os_array->dev.user_data, RT_WAITING_FOREVER);

		/* wakeup rcpu1 */
		spacemit_wakeup_rcpu1();

		/* wait rcpu1 power up */
		rt_sem_take(multiple_os_array->msem, RT_WAITING_FOREVER);

		/* waitup cluster2 */
		_entry = spacemit_get_c2_bootenty();
		/* set bootentry */
		spacemit_set_c2_bootenty((unsigned long long)_start_warm_dummy);

		spacemit_wakeup_c2();

		/* wait c2 power up */
		spacemit_wait_c2_pwrup();

		/* retore the bootentry */
		spacemit_set_c2_bootenty((unsigned long long)_entry);

		/* waitup cluster3 */
		_entry = spacemit_get_c3_bootenty();
		/* set bootentry */
		spacemit_set_c3_bootenty((unsigned long long)_start_warm_dummy);

		spacemit_wakeup_c3();

		/* wait c3 power up */
		spacemit_wait_c3_pwrup();

		/* retore the bootentry */
		spacemit_set_c3_bootenty((unsigned long long)_entry);

		/* wakeup AP */
		for (i = 0; i < config->os_count; ++i) {
			/* send the wakeup event to other os */
			rt_sem_release(config->hsm[i]->cmwk_sem);
		}
	}
}

/* initialize an event to dealing with the multiple os's syspend */
static int k3_multiple_os_power_init(void)
{
	multiple_os_array = (struct spacemit_multiple_os *)rt_calloc(1, sizeof(struct spacemit_multiple_os));
	if (multiple_os_array == RT_NULL) {
		rt_kprintf("%s:%d, No memory\n", __func__, __LINE__);
		return -RT_ENOMEM;
	}

	/* create a event */
	multiple_os_array->multiple_event = rt_event_create("multiple_event", RT_IPC_FLAG_FIFO);

	return 0;
}
INIT_PREV_EXPORT(k3_multiple_os_power_init);

static void lpm_rx_callback(struct mbox_client *cl, void *data)
{
	rt_sem_release(multiple_os_array->msem);
}

static int k3_multiple_os_power_lunch(void)
{
	char *string, *strend;
	rt_int32_t size;
	struct dtb_node *compatible_node;
	struct dtb_node *dtb_head_node = get_dtb_node_head();

	if (read_csr(mhartid) == 1)
		return 0;

	multiple_os_array->multiple_tid = rt_thread_create("multiple_thread",
			spacemit_multiple_os_poll,
			(void *)multiple_os_array,
			2048,
			RT_THREAD_PRIORITY_MAX / 3,
			20);
	if (!multiple_os_array->multiple_tid) {
		rt_kprintf("Failed to create multiple os dealing thread\n");
		return -RT_EINVAL;
	}


	compatible_node = dtb_node_find_compatible_node(dtb_head_node, "spacemit,rslpm");
	if (compatible_node != RT_NULL) {
		/* check the status */
		if (!dtb_node_device_is_available(compatible_node))
			return -RT_EINVAL;
		for_each_property_string_extend(compatible_node, "mbox-names", string, strend, size) {
			if (rt_strcmp(string, "tx") == 0) {
				multiple_os_array->mtx_client.dev = compatible_node;
				multiple_os_array->mtx_client.tx_block = true;
				multiple_os_array->mtx_client.rx_callback = RT_NULL;
				multiple_os_array->mtx_chan = mbox_request_channel_byname(&multiple_os_array->mtx_client, string);
			} else {
				multiple_os_array->mrx_client.dev = compatible_node;
				multiple_os_array->mrx_client.tx_block = false;
				multiple_os_array->mrx_client.rx_callback = lpm_rx_callback;
				multiple_os_array->mrx_chan = mbox_request_channel_byname(&multiple_os_array->mrx_client, string);
			}
		}
	}

	rt_device_register(&multiple_os_array->dev, "lpmdev", RT_DEVICE_FLAG_RDWR);

	multiple_os_array->dev.user_data = (void *)rt_sem_create("lpmcomm", 0, RT_IPC_FLAG_FIFO);
	if (!multiple_os_array->dev.user_data) {
		rt_kprintf("create low power common sem error\n");
		return -RT_EINVAL;
	}

	multiple_os_array->msem = rt_sem_create("lpmsem", 0, RT_IPC_FLAG_FIFO);
	if (!multiple_os_array->msem) {
		rt_kprintf("create low power sem error\n");
		return -RT_EINVAL;
	}

	rt_thread_startup(multiple_os_array->multiple_tid);

	return 0;
}
INIT_COMPONENT_EXPORT(k3_multiple_os_power_lunch);

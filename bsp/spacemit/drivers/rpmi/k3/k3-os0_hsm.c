/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <drivers/pm.h>
#include <rtthread.h>
#include <dtb_head.h>
#include <riscv-ops.h>
#include <riscv-plic.h>
#include "k3_hsm.h"
#include <register_defination.h>
#include "../spacemit-rpmi.h"

static unsigned long long _c0_entry = RT_NULL;

struct rpmi_hsm_hart {
	/** Lock to protect this structure and perform platform operations */
	void *lock;

	/** Current HSM hart state */
	enum rpmi_hsm_hart_state state;

	/** Current hart start parameter */
	rpmi_uint64_t start_addr;

	/** Current hart suspend parameter */
	const struct rpmi_hsm_suspend_type *suspend_type;
	rpmi_uint64_t resume_addr;
};

struct rpmi_hsm {
	/** Whether HSM instance is non-leaf (or hierarchical) instance */
	rpmi_bool_t is_non_leaf;

	union {
		/** Details required by leaf instance */
		struct {
			/** Number of harts */
			rpmi_uint32_t hart_count;

			/** Array of hart IDs */
			const rpmi_uint32_t *hart_ids;

			/** Array of harts */
			struct rpmi_hsm_hart *harts;

			/** Number of suspend types */
			rpmi_uint32_t suspend_type_count;

			/** Array of suspend types */
			const struct rpmi_hsm_suspend_type *suspend_types;

			/**
			 * Platform HSM operations
			 *
			 * Note: These operations are called with harts[i]->lock held
			 */
			const struct rpmi_hsm_platform_ops *ops;

			/** Private data of platform HSM operations */
			void *ops_priv;
		} leaf;

		/** Details required by non-leaf instance */
		struct {
			/** Number of child instances */
			rpmi_uint32_t child_count;

			/** Array of child instance pointers */
			struct rpmi_hsm **child_array;
		} nonleaf;
	};
};

static enum rpmi_hart_hw_state k3_hsm_get_hw_state(void* priv,
	rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = priv;

	if (config->hsm->leaf.harts[hart_index].state == 0xffffffff) {
		if (hart_index == 0) {
			return RPMI_HART_HW_STATE_STARTED;
		} else {
			return RPMI_HART_HW_STATE_STOPPED;
		}
	}

	/* this is a fake value */
	return RPMI_HART_HW_STATE_STARTED;
}

static enum rpmi_error k3_hsm_hart_start_prepare(void* priv,
	rpmi_uint32_t hart_index,
	rpmi_uint64_t start_addr)
{
	return 0;
}

static void k3_hsm_hart_start_finalize(void* priv,
	rpmi_uint32_t hart_index,
	rpmi_uint64_t start_addr)
{
	return;
}

static enum rpmi_error k3_hsm_hart_stop_prepare(void* priv,
	rpmi_uint32_t hart_index)
{
	return 0;
}

static void k3_hsm_hart_stop_finalize(void* priv, rpmi_uint32_t hart_index)
{
	return;
}

static enum rpmi_error k3_hsm_hart_suspend_prepare(
	void* priv,
	rpmi_uint32_t hart_index,
	const struct rpmi_hsm_suspend_type* suspend_type,
	rpmi_uint64_t resume_addr)
{
	return 0;
}

static void k3_hsm_hart_suspend_finalize(
	void* priv,
	rpmi_uint32_t hart_index,
	const struct rpmi_hsm_suspend_type* suspend_type,
	rpmi_uint64_t resume_addr)
{
	/* do nothing */
}

struct rpmi_hsm_platform_ops k3_os0_hsm_pops = {
	.hart_get_hw_state = k3_hsm_get_hw_state,
	.hart_start_prepare = k3_hsm_hart_start_prepare,
	.hart_start_finalize = k3_hsm_hart_start_finalize,
	.hart_stop_prepare = k3_hsm_hart_stop_prepare,
	.hart_stop_finalize = k3_hsm_hart_stop_finalize,
	.hart_suspend_prepare = k3_hsm_hart_suspend_prepare,
	.hart_suspend_finalize = k3_hsm_hart_suspend_finalize
};

static void spacemit_m2_enter_exit(int vector, void *param)
{
	unsigned int val;
	struct spacemit_rpmi_hsm_config *config = param;
	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(config->bootcore_index);

	if ((vector == AP_C0_M2_ENTER_INT_NUM) ||
			(vector == AP_C1_M2_ENTER_INT_NUM) ||
			(vector == AP_C2_M2_ENTER_INT_NUM) ||
			(vector == AP_C3_M2_ENTER_INT_NUM)) {

			/* clear the pending */
			spacemit_cx_m2_enter_wait(config->bootcore_index);

			rt_sem_release(config->cm2_etr_sem);

		return;
	}

	/* assert corex */
	spacemit_assert_corex(config->bootcore_index);

	/* clear the pending and wakeup the cluster */
	spacemit_cx_m2_int_disabled(config->bootcore_index);

	/* send the signal */
	rt_sem_release(config->cm2_ext_sem);
}

static rt_int32_t _k3_os0_hsm_init(void *priv)
{
	char *tmp;
	struct spacemit_rpmi_hsm_config *config = priv;

	tmp = rt_calloc(1, 64);
	rt_snprintf(tmp, 64, "Cr%d_sem", config->bootcore_index);

	config->cm2_etr_sem = rt_sem_create(tmp, 0, RT_IPC_FLAG_FIFO);

	tmp = rt_calloc(1, 64);
	rt_snprintf(tmp, 64, "Ce%d_sem", config->bootcore_index);

	config->cm2_ext_sem = rt_sem_create(tmp, 0, RT_IPC_FLAG_FIFO);

	tmp = rt_calloc(1, 64);
	rt_snprintf(tmp, 64, "CWK%d_sem", config->bootcore_index);

	config->cmwk_sem = rt_sem_create(tmp, 0, RT_IPC_FLAG_FIFO);

	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(config->bootcore_index);

	switch (cluster_id) {
	case 0:
		/* exit m2 */
		rt_hw_interrupt_install(AP_C0_M2_EXIT_INT_NUM, spacemit_m2_enter_exit, priv, "c0_m2_exit");
		rt_hw_interrupt_install(AP_C0_M2_ENTER_INT_NUM, spacemit_m2_enter_exit, priv, "c0_m2_enter");
		/* rt_hw_interrupt_umask(AP_C0_M2_EXIT_INT_NUM); */
		rt_hw_interrupt_umask(AP_C0_M2_ENTER_INT_NUM);
		config->cm2_ext_vector = AP_C0_M2_EXIT_INT_NUM;
	break;
	case 1:
		rt_hw_interrupt_install(AP_C1_M2_EXIT_INT_NUM, spacemit_m2_enter_exit, priv, "c1_m2_exit");
		rt_hw_interrupt_install(AP_C1_M2_ENTER_INT_NUM, spacemit_m2_enter_exit, priv, "c1_m2_enter");
		rt_hw_interrupt_umask(AP_C1_M2_EXIT_INT_NUM);
		rt_hw_interrupt_umask(AP_C1_M2_ENTER_INT_NUM);
		config->cm2_ext_vector = AP_C1_M2_EXIT_INT_NUM;
	break;
	case 2:
		rt_hw_interrupt_install(AP_C2_M2_EXIT_INT_NUM, spacemit_m2_enter_exit, priv, "c2_m2_exit");
		rt_hw_interrupt_install(AP_C2_M2_ENTER_INT_NUM, spacemit_m2_enter_exit, priv, "c2_m2_enter");
		rt_hw_interrupt_umask(AP_C2_M2_EXIT_INT_NUM);
		rt_hw_interrupt_umask(AP_C2_M2_ENTER_INT_NUM);
		config->cm2_ext_vector = AP_C2_M2_EXIT_INT_NUM;
	break;
	case 3:
		rt_hw_interrupt_install(AP_C3_M2_EXIT_INT_NUM, spacemit_m2_enter_exit, priv, "c3_m2_exit");
		rt_hw_interrupt_install(AP_C3_M2_ENTER_INT_NUM, spacemit_m2_enter_exit, priv, "c3_m2_enter");
		rt_hw_interrupt_umask(AP_C3_M2_EXIT_INT_NUM);
		rt_hw_interrupt_umask(AP_C3_M2_ENTER_INT_NUM);
		config->cm2_ext_vector = AP_C3_M2_EXIT_INT_NUM;
        break;
	default:
        	break;
	}

	return 0;
}

/* System suspend platform operations (stubs for hardware-related functions) */
static enum rpmi_error syssusp_prepare(
		void* priv,
		rpmi_uint32_t hart_index,
		const struct rpmi_system_suspend_type* syssusp_type,
		rpmi_uint64_t resume_addr)
{
	unsigned int val;

	struct spacemit_rpmi_hsm_config *config = (struct spacemit_rpmi_hsm_config *)priv;
	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(config->bootcore_index);

	if (hart_index == 0) {
		/* vote cluster2 power down */
		spacemit_vote_powrdown_cluster(8);
	}

	spacemit_cx_m2_int_enable(config->bootcore_index);

	return 0;
}

static rpmi_bool_t syssusp_ready(void* priv, rpmi_uint32_t hart_index)
{
	struct spacemit_rpmi_hsm_config *config = (struct spacemit_rpmi_hsm_config *)priv;

	rt_sem_take(config->cm2_etr_sem, RT_WAITING_FOREVER);

	return true;
}

static void syssusp_finalize(
		void* priv,
		rpmi_uint32_t hart_index,
		const struct rpmi_system_suspend_type* syssusp_type,
		rpmi_uint64_t resume_addr)
{
	struct spacemit_rpmi_hsm_config *config = (struct spacemit_rpmi_hsm_config *)priv;

	if (syssusp_type->type == RPMI_SYSSUSP_TYPE_SUSPEND_TO_DISK)
		config->mulos->hibernate_pending = 1;

	rt_event_send(config->event, (1 << config->bootcore_index));
}

static rpmi_bool_t syssusp_can_resume(void* priv, rpmi_uint32_t hart_index)
{
	return true;
}

void c0boot_entry_dummy(unsigned int hartid)
{
	typedef void (*_jump_entry)(void);
	_jump_entry ptr = (_jump_entry)_c0_entry;

	spacemit_set_c0_bootenty(_c0_entry);

	ptr();
}

extern void _c0start_warm_dummy(void);

static enum rpmi_error syssusp_resume(
		void* priv,
		rpmi_uint32_t hart_index,
		const struct rpmi_system_suspend_type* syssusp_type,
		rpmi_uint64_t resume_addr)
{
	unsigned int val;
	struct spacemit_rpmi_hsm_config *config = priv;

	/* wait resume signle */
	rt_sem_take(config->cm2_ext_sem, RT_WAITING_FOREVER);

	/* we should first let the rcpu1 wakeup, so wait for the notify by spacmeit-hsm layer */
	rt_sem_take(config->cmwk_sem, RT_WAITING_FOREVER);

	_c0_entry = spacemit_get_c0_bootenty();
	spacemit_set_c0_bootenty((unsigned long long)_c0start_warm_dummy);

	/* de-assert bootcore */
	spacemit_deassert_corex(config->bootcore_index);

	if (hart_index == 0)
		/* mask the Cluster0 M2 exit interrupt */
		rt_hw_interrupt_mask(AP_C0_M2_EXIT_INT_NUM);

	return 0;
}

/* System suspend platform operations */
static struct rpmi_syssusp_platform_ops k3_os0_syssup_ops = {
	.system_suspend_prepare = syssusp_prepare,
	.system_suspend_ready = syssusp_ready,
	.system_suspend_finalize = syssusp_finalize,
	.system_suspend_can_resume = syssusp_can_resume,
	.system_suspend_resume = syssusp_resume
};

static struct spacemit_rpmi_hsm_ops k3_os0_hsm_ops = {
	.name = "k3-os0-rpmi-hsm",
	.init = _k3_os0_hsm_init,
	.hsm_ops = &k3_os0_hsm_pops,
	.syssup_ops = &k3_os0_syssup_ops,
};

static rt_int32_t k3_os0_hsm_init(void)
{

	rt_list_init(&k3_os0_hsm_ops.list);

	spacemit_rpmi_hsm_register(&k3_os0_hsm_ops.list);

	return 0;
}
INIT_DEVICE_EXPORT(k3_os0_hsm_init);

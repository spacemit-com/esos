/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RPMI_PLATFORM_DEFIN_H__
#define __RPMI_PLATFORM_DEFIN_H__

#include <librpmi.h>
#include <dtb_node.h>
#include <librpmi_env.h>
#include <rtdevice.h>
#include <rtdef.h>

#define HSM_SUSP_BASE_MASK		0x7fffffff
#define HSM_SUSP_NON_RET_BIT		0x80000000
#define HSM_SUSP_PLAT_BASE		0x10000000

#define HSM_SUSPEND_RET_DEFAULT		0x00000000
#define HSM_SUSPEND_RET_PLATFORM	HSM_SUSP_PLAT_BASE
#define HSM_SUSPEND_RET_LAST		HSM_SUSP_BASE_MASK
#define HSM_SUSPEND_NON_RET_DEFAULT	HSM_SUSP_NON_RET_BIT
#define HSM_SUSPEND_NON_RET_PLATFORM	(HSM_SUSP_NON_RET_BIT | HSM_SUSP_PLAT_BASE)
#define HSM_SUSPEND_NON_RET_LAST	(HSM_SUSP_NON_RET_BIT | HSM_SUSP_BASE_MASK)

#define MAX_HSM_SUSPEND_TYPE		6

#define HSM_SUSPEND_CPU_RET		HSM_SUSP_PLAT_BASE
#define HSM_SUSPEND_CLUSTER_RET		(HSM_SUSPEND_CPU_RET | 0x1000000)
#define HSM_SUSPEND_CPU_NON_RET		HSM_SUSPEND_NON_RET_PLATFORM
#define HSM_SUSPEND_CLUSTER_NON_RET	(HSM_SUSPEND_CPU_NON_RET | 0x1000000)
#define HSM_SUSPEND_HOME_SCREEN_NON_RET	(HSM_SUSPEND_CPU_NON_RET | 0x2000000)

#define PLATFROM_MAX_OS			4
#define HSM_SUSPEND_MAX_HARTIDS		16

struct spacemit_multiple_os;

/* RPMI HSM structures  */
struct spacemit_rpmi_hsm_config {
	rt_uint32_t hartids[HSM_SUSPEND_MAX_HARTIDS];
	struct rpmi_hsm_suspend_type stype[MAX_HSM_SUSPEND_TYPE];
	rt_int32_t hartcnt;
	rt_int32_t type_cnt;
	rt_int32_t support_syssup;
	struct dtb_node *node;
	struct rpmi_hsm *hsm;
	struct rpmi_hsm_platform_ops *hsm_ops;
	struct rpmi_syssusp_platform_ops *syssup_ops;
	rt_event_t event;
	rt_sem_t cm2_ext_sem;
	rt_sem_t cm2_etr_sem;
	rt_sem_t cmwk_sem;
	unsigned int cm2_ext_vector;
	rt_uint32_t bootcore_index;
	struct spacemit_multiple_os *mulos;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_hsm_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_hsm_platform_ops *hsm_ops;
	struct rpmi_syssusp_platform_ops *syssup_ops;
	rt_list_t list;
};

/* RPMI clock structures */
struct spacemit_rpmi_clk_config {
	rt_int32_t num_clk;
	struct rpmi_clock_data *clk_data;
	struct dtb_node *node;
	struct rpmi_clock_platform_ops *ops;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_clk_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_clock_platform_ops *clk_ops;
	rt_list_t list;
};

/* RPMI voltage structures */
struct spacemit_rpmi_voltage_config {
	rt_int32_t domain_count;
	struct rpmi_voltage_data *voltage_data;
	struct dtb_node *node;
	struct rpmi_voltage_platform_ops *ops;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_voltage_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_voltage_platform_ops *voltage_ops;
	rt_list_t list;
};

/* RPMI device power structures */
struct spacemit_rpmi_domain_config {
	struct dtb_node *node;
	rt_int32_t domain_count;
	void *base;
	struct rpmi_device_power_attrs *domain_data;
	struct rpmi_domain_platform_ops *ops;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_domain_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_domain_platform_ops *domain_ops;
	rt_list_t list;
};

/* RPMI rtc structures */
struct spacemit_rpmi_rtc_config {
	struct dtb_node *node;
	struct rpmi_rtc_platform_ops *ops;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_rtc_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_rtc_platform_ops *rtc_ops;
	rt_list_t list;
};

/* RPMI pwrkey structures */
struct spacemit_rpmi_pwrkey_config {
	struct dtb_node *node;
	struct rpmi_pwrkey_platform_ops *ops;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_pwrkey_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_pwrkey_platform_ops *pwrkey_ops;
	rt_list_t list;
};

/* RPMI sysreset structures */
struct spacemit_rpmi_sysreset_config {
	struct dtb_node *node;
	struct rpmi_sysreset_platform_ops *ops;
	rt_int32_t reset;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_sysreset_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_sysreset_platform_ops *sysreset_ops;
	rt_list_t list;
};

/* RPMI msi structures */
struct spacemit_rpmi_msi_config {
	struct dtb_node *node;
	rt_uint32_t num_msi, p2a_index;
	struct rpmi_sysmsi_platform_ops *ops;
	/* reserved for future use */
	void *priv;
};

struct spacemit_rpmi_msi_ops {
	char *name;
	rt_int32_t (*init)(void *priv);
	struct rpmi_sysmsi_platform_ops *msi_ops;
	rt_list_t list;
};

struct spacemit_multiple_os {
	struct spacemit_rpmi_hsm_config *hsm[PLATFROM_MAX_OS];
	int os_count;
	int hibernate_pending;
	rt_event_t multiple_event;
	rt_thread_t multiple_tid;
	rt_sem_t msem;
	struct rt_device dev;
	struct mbox_client mtx_client, mrx_client;
	struct mbox_chan *mtx_chan, *mrx_chan;
};

struct spacemit_rpmi_config {
	rpmi_uintptr_t shmem_base;        /* Shared memory base address */
	rpmi_uint32_t shmem_size;         /* Shared memory size */
	rpmi_uint32_t slot_size;          /* Message slot size */
	rpmi_uint32_t a2p_queue_size;     /* AP to RCPU queue size */
	rpmi_uint32_t p2a_queue_size;     /* RCPU to AP queue size */
	struct spacemit_rpmi_hsm_config hsm_config;
	struct spacemit_rpmi_clk_config clk_config;
	struct spacemit_rpmi_voltage_config voltage_config;
	struct spacemit_rpmi_domain_config domain_config;
	struct spacemit_rpmi_rtc_config rtc_config;
	struct spacemit_rpmi_msi_config msi_config;
	struct spacemit_rpmi_pwrkey_config pwrkey_config;
	struct spacemit_rpmi_sysreset_config sysreset_config;
};

struct spacemit_rpmi_func {
	int (*rmpi_get_configuration)(struct dtb_node *mode, void *config, char *match);
	int (*rpmi_register_service)(void *config, struct rpmi_context *cntx);
};

/* Define RPMI private data structure */
struct spacemit_rpmi_priv {
	struct dtb_node *node;
	struct mbox_client client;
	struct mbox_chan *chan; /* Changed to match the header definition */
	struct rpmi_context *cntx;
	rt_thread_t tid;
	rt_sem_t sem;
	struct spacemit_rpmi_config config;
};

int spacemit_rpmi_hsm_register(rt_list_t *node);
int spacemit_rpmi_clk_register(rt_list_t *node);
int spacemit_rpmi_voltage_register(rt_list_t *node);
int spacemit_rpmi_domain_register(rt_list_t *node);
int spacemit_rpmi_rtc_register(rt_list_t *node);
int spacemit_rpmi_pwrkey_register(rt_list_t *node);
int spacemit_rpmi_sysreset_register(rt_list_t *node);
int spacemit_rpmi_msi_register(rt_list_t *node);
extern void _start_warm_dummy(void);
extern void spacemit_wait_c2_pwrup(void);
extern void spacemit_set_c2_bootenty(unsigned long long _entry);
extern unsigned long long spacemit_get_c2_bootenty(void);
extern int spacemit_wakeup_c2(void);

extern void spacemit_wait_c3_pwrup(void);
extern void spacemit_set_c3_bootenty(unsigned long long _entry);
extern unsigned long long spacemit_get_c3_bootenty(void);
extern int spacemit_wakeup_c3(void);
extern void spacemit_wakeup_rcpu1(void);


#endif /* __RPMI_PLATFORM_DEFIN_H__ */

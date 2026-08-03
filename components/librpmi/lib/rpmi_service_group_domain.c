/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Spacemit.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define RPMI_DEVICE_POWER_STATE_INVALID		(-1ULL)
/** Voltage name max length including null char */
#define RPMI_DEVICE_POWER_NAME_MAX_LEN		16

/** Convert list node pointer to struct rpmi_device_power instance pointer */
#define to_rpmi_device_power(__node)	\
	container_of((__node), struct rpmi_device_power, node)

/* A device power instance */
struct rpmi_device_power {
	/* Clock node */
	struct rpmi_dlist node;
	/* Lock to invoke the platform operations to
	 * protect this structure */
	void *lock;
	/* Domain ID */
	rpmi_uint32_t id;
	/* Current domain state */
	enum rpmi_device_power_state current_state;
	/* Device Power static attributes/data */
	const struct rpmi_device_power_attrs *cdata;
};

/** RPMI Clock Service Group instance */
struct rpmi_device_power_group {
	/* Total Voltage domain count */
	rpmi_uint32_t domain_count;
	/* Pointer to power domain tree */
	struct rpmi_device_power *device_power_tree;
	/* Common domain platform operations (called with holding the lock)*/
	const struct rpmi_domain_platform_ops *ops;
	/* Private data of platform power domain operations */
	void *ops_priv;
	struct rpmi_service_group group;
};

/** Get a struct rpmi_clock instance pointer from power domain id */
static inline struct rpmi_device_power *
rpmi_get_device_domain(struct rpmi_device_power_group *device_power_group, rpmi_uint32_t domain_id)
{
	return &device_power_group->device_power_tree[domain_id];
}

static enum rpmi_error rpmi_device_power_get_attrs(struct rpmi_device_power_group *devpwrgrp,
					    rpmi_uint32_t domainid,
					    struct rpmi_device_power_attrs *attrs)
{
	struct rpmi_device_power *domain;

	if (!attrs) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	domain = rpmi_get_device_domain(devpwrgrp, domainid);
	if (!domain) {
		DPRINTF("%s: power instance with domainid-%u not found\n",
			__func__, domainid);
		return RPMI_ERR_INVALID_PARAM;
	}

	attrs->name = domain->cdata->name;
	attrs->transition_latency = domain->cdata->transition_latency;

	return RPMI_SUCCESS;
}

static enum rpmi_error __rpmi_device_power_set_state(struct rpmi_device_power_group *devpwrgrp,
					      struct rpmi_device_power *dev_power,
					      enum rpmi_device_power_state state)
{
	enum rpmi_error ret = 0;
	struct rpmi_dlist *pos;

	rpmi_env_lock(dev_power->lock);

	if (state == RPMI_DEVICE_POWER_STATE_OFF) {
		/* If power domain already disabled? use the cached state */
		if (dev_power->current_state == RPMI_DEVICE_POWER_STATE_OFF) {
			rpmi_env_unlock(dev_power->lock);
			return RPMI_ERR_ALREADY;
		}

		ret = devpwrgrp->ops->set_state(devpwrgrp->ops_priv, dev_power->id, state);
		if (ret) {
			rpmi_env_unlock(dev_power->lock);
			return ret;
		}

		dev_power->current_state = state;

	} else if (state == RPMI_DEVICE_POWER_STATE_ON) {

		/* If power domain is already enabled? use the cached state */
		if (dev_power->current_state == RPMI_DEVICE_POWER_STATE_ON) {
			rpmi_env_unlock(dev_power->lock);
			return RPMI_ERR_ALREADY;
		}

		ret = devpwrgrp->ops->set_state(devpwrgrp->ops_priv, dev_power->id, state);
		if (ret) {
			rpmi_env_unlock(dev_power->lock);
			return ret;
		}

		dev_power->current_state = state;
	}

done:
	rpmi_env_unlock(dev_power->lock);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_device_power_set_state(struct rpmi_device_power_group *devpwrgrp,
				     rpmi_uint32_t domainid,
				     enum rpmi_device_power_state state)
{
	enum rpmi_error ret;
	struct rpmi_device_power *dev_power = rpmi_get_device_domain(devpwrgrp, domainid);
	if (!dev_power)
		return RPMI_ERR_INVALID_PARAM;

	ret = __rpmi_device_power_set_state(devpwrgrp, dev_power, state);

	return ret;
}

static enum rpmi_error rpmi_device_power_get_state(struct rpmi_device_power_group *devpwrgrp,
				     rpmi_uint32_t domainid,
				     enum rpmi_device_power_state *state)
{
	enum rpmi_error ret;
	struct rpmi_device_power *dev_power = rpmi_get_device_domain(devpwrgrp, domainid);

	if (!dev_power || !state)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(dev_power->lock);

	if (devpwrgrp->ops->get_state) {
		ret = devpwrgrp->ops->get_state(devpwrgrp->ops_priv, domainid, state);
		if (ret == RPMI_SUCCESS)
			dev_power->current_state = *state;
	} else {
		*state = dev_power->current_state;
	}

	rpmi_env_unlock(dev_power->lock);

	return RPMI_SUCCESS;
}

/**
 * Initialize the power tree from provided
 * static platform power data.
 *
 * This function initializes the hierarchical structures
 * to represent the power association in the platform.
 **/
static struct rpmi_device_power *
rpmi_pm_domain_tree_init(rpmi_uint32_t domain_count,
		     const struct rpmi_device_power_attrs *domain_tree_data,
		     const struct rpmi_domain_platform_ops *ops,
		     void *ops_priv)
{
	int ret;
	rpmi_uint32_t domainid;
	enum rpmi_device_power_state state;
	struct rpmi_device_power *dev_power;

	struct rpmi_device_power *dev_power_tree =
		rpmi_env_zalloc(sizeof(struct rpmi_device_power) * domain_count);

	/* initialize all clocks instances */
	for (domainid = 0; domainid < domain_count; domainid++) {
		dev_power = &dev_power_tree[domainid];
		dev_power->id = domainid;
		dev_power->cdata = &domain_tree_data[domainid];

		RPMI_INIT_LIST_HEAD(&dev_power->node);

		/* all the votage domains defualt to disabled */
		dev_power->current_state = RPMI_DEVICE_POWER_STATE_OFF;

		dev_power->lock = rpmi_env_alloc_lock();
	}

	return dev_power_tree;
}

/*****************************************************************************
 * RPMI Voltage Serivce Group Functions
 ****************************************************************************/
static enum rpmi_error
rpmi_domain_sg_get_num_domains(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	struct rpmi_device_power_group *devpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, devpwrgrp->domain_count);

	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_domain_sg_get_attributes(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error ret;
	rpmi_uint32_t flags = 0;
	struct rpmi_device_power_attrs device_power_attrs;
	struct rpmi_device_power_group *devpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= devpwrgrp->domain_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = rpmi_device_power_get_attrs(devpwrgrp, domainid, &device_power_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	resp[2] = rpmi_to_xe32(trans->is_be, device_power_attrs.transition_latency);
	resp[1] = rpmi_to_xe32(trans->is_be, flags);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	if (device_power_attrs.name)
		rpmi_env_strncpy((char *)&resp[3], device_power_attrs.name, RPMI_DEVICE_POWER_NAME_MAX_LEN);

	resp_dlen = 7 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_domain_sg_set_state(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t cfg, new_state;
	struct rpmi_device_power_group *devpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
			       ((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= devpwrgrp->domain_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	cfg = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[1]);

	/* get command from 0th index bit in config field */
	new_state = (cfg & 0b1) ? RPMI_DEVICE_POWER_STATE_ON : RPMI_DEVICE_POWER_STATE_OFF;

	/* change power config synchronously */
	status = rpmi_device_power_set_state(devpwrgrp, domainid, new_state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

done:
	*response_datalen = sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_domain_sg_get_state(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error status;
	enum rpmi_device_power_state state;
	struct rpmi_device_power_group *devpwrgrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= devpwrgrp->domain_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = rpmi_device_power_get_state(devpwrgrp, domainid, &state);
	if (status) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	/** RPMI config field only return enabled or disabled state */
	state = (state == RPMI_DEVICE_POWER_STATE_ON)? 1 : 0;

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_device_power_services[RPMI_DEVICE_POWER_SRV_ID_MAX] = {
	[RPMI_DEVICE_POWER_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_DEVICE_POWER_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_DEVICE_POWER_SRV_GET_NUM_DOMAINS] = {
		.service_id = RPMI_DEVICE_POWER_SRV_GET_NUM_DOMAINS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_domain_sg_get_num_domains,
	},
	[RPMI_DEVICE_POWER_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_DEVICE_POWER_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_domain_sg_get_attributes,
	},
	[RPMI_DEVICE_POWER_SRV_SET_STATE] = {
		.service_id = RPMI_DEVICE_POWER_SRV_SET_STATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_domain_sg_set_state,
	},
	[RPMI_DEVICE_POWER_SRV_GET_STATE] = {
		.service_id = RPMI_DEVICE_POWER_SRV_GET_STATE,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_domain_sg_get_state,
	},
};

struct rpmi_service_group *
rpmi_service_group_domain_create(rpmi_uint32_t domain_count,
				const struct rpmi_device_power_attrs *domain_tree_data,
				const struct rpmi_domain_platform_ops *ops,
				void *ops_priv)
{
	struct rpmi_device_power_group *devpwrgrp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!domain_count || !domain_tree_data || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate power service group */
	devpwrgrp = rpmi_env_zalloc(sizeof(*devpwrgrp));
	if (!devpwrgrp) {
		DPRINTF("%s: failed to allocate device power service group instance\n",
			__func__);
		return NULL;
	}

	devpwrgrp->device_power_tree = rpmi_pm_domain_tree_init(domain_count,
						 domain_tree_data,
						 ops,
						 ops_priv);
	if (!devpwrgrp->device_power_tree) {
		DPRINTF("%s: failed to initialize device power domain tree\n", __func__);
		rpmi_env_free(devpwrgrp);
		return NULL;
	}

	devpwrgrp->domain_count = domain_count;
	devpwrgrp->ops = ops;
	devpwrgrp->ops_priv = ops_priv;

	group = &devpwrgrp->group;
	group->name = "device_power";
	group->servicegroup_id = RPMI_SRVGRP_DEVICE_POWER;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_DEVICE_POWER_SRV_ID_MAX;
	group->services = rpmi_device_power_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = devpwrgrp;

	return group;
}

void rpmi_service_group_domain_destroy(struct rpmi_service_group *group)
{
	rpmi_uint32_t domainid;
	struct rpmi_device_power_group *devpwrgrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	devpwrgrp = group->priv;

	for (domainid = 0; domainid < devpwrgrp->domain_count; domainid++) {
		rpmi_env_free_lock(devpwrgrp->device_power_tree[domainid].lock);
	}

	rpmi_env_free(devpwrgrp->device_power_tree);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}

/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ventana Micro Systems Inc.
 */

#include <librpmi.h>
#include "librpmi_internal.h"
#include "librpmi_internal_list.h"

#ifdef DEBUG
#define DPRINTF(msg...)		rpmi_env_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define RPMI_VOLTAGE_LEVEL_INVALID		(-1ULL)
/** Voltage name max length including null char */
#define RPMI_VOLTAGE_NAME_MAX_LEN		16

/** Convert list node pointer to struct rpmi_voltage instance pointer */
#define to_rpmi_voltage(__node)	\
	container_of((__node), struct rpmi_voltage, node)

/* A voltage instance */
struct rpmi_voltage {
	/* Clock node */
	struct rpmi_dlist node;
	/* Lock to invoke the platform operations to
	 * protect this structure */
	void *lock;
	/* Domain ID */
	rpmi_uint32_t id;
	/* Domain enable count on behalf of child domain */
	rpmi_uint32_t enable_count;
	/* Current domain state */
	enum rpmi_voltage_state current_state;
	/* Parent voltage instance pointer */
	struct rpmi_voltage *parent;
	/* Child domain count */
	rpmi_uint32_t child_count;
	/* Voltage static attributes/data */
	const struct rpmi_voltage_data *cdata;
	/* Child domain list */
	struct rpmi_dlist child_domain;
};

/** RPMI Clock Service Group instance */
struct rpmi_voltage_group {
	/* Total Voltage domain count */
	rpmi_uint32_t domain_count;
	/* Pointer to voltage domain tree */
	struct rpmi_voltage *voltage_domain_tree;
	/* Common voltage platform operations (called with holding the lock)*/
	const struct rpmi_voltage_platform_ops *ops;
	/* Private data of platform voltage domain operations */
	void *ops_priv;
	struct rpmi_service_group group;
};

/** Get a struct rpmi_clock instance pointer from voltage domain id */
static inline struct rpmi_voltage *
rpmi_get_voltage_domain(struct rpmi_voltage_group *voltage_group, rpmi_uint32_t domain_id)
{
	return &voltage_group->voltage_domain_tree[domain_id];
}

static enum rpmi_error rpmi_voltage_get_attrs(struct rpmi_voltage_group *voltagegrp,
					    rpmi_uint32_t domainid,
					    struct rpmi_voltage_attrs *attrs)
{
	struct rpmi_voltage *voltage;

	if (!attrs) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return RPMI_ERR_INVALID_PARAM;
	}

	voltage = rpmi_get_voltage_domain(voltagegrp, domainid);
	if (!voltage) {
		DPRINTF("%s: voltage instance with domainid-%u not found\n",
			__func__, domainid);
		return RPMI_ERR_INVALID_PARAM;
	}

	attrs->name = voltage->cdata->name;
	attrs->type = voltage->cdata->voltage_type;
	attrs->level_count = voltage->cdata->level_count;
	attrs->transition_latency = voltage->cdata->transition_latency_ms;
	attrs->level_array = voltage->cdata->voltage_level_array;

	return RPMI_SUCCESS;
}

static enum rpmi_error __rpmi_voltage_set_level(struct rpmi_voltage_group *voltagegrp,
				    struct rpmi_voltage *voltage,
				    rpmi_uint32_t level)
{
	enum rpmi_error ret;

	if (voltage->current_state == RPMI_VOLTAGE_STATE_DISABLED)
		return RPMI_ERR_DENIED;

	ret = voltagegrp->ops->set_voltage_level(voltagegrp->ops_priv, voltage->id, level);
	if (ret)
		return ret;

	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_voltage_set_level(struct rpmi_voltage_group *voltagegrp,
				    rpmi_uint32_t domainid,
				    rpmi_uint32_t level)
{
	enum rpmi_error ret;

	struct rpmi_voltage *voltage = rpmi_get_voltage_domain(voltagegrp, domainid);
	if (!voltage) {
		DPRINTF("%s: voltage domain instance with domainid-%u not found\n",
			__func__, domainid);
		return RPMI_ERR_INVALID_PARAM;
	}

	rpmi_env_lock(voltage->lock);
	ret = __rpmi_voltage_set_level(voltagegrp, voltage, level);
	rpmi_env_unlock(voltage->lock);

	return ret;
}

static enum rpmi_error rpmi_voltage_get_level(struct rpmi_voltage_group *voltagegrp,
				    rpmi_uint32_t domainid,
				    rpmi_uint32_t *level)
{
	enum rpmi_error ret;
	struct rpmi_voltage *voltage = rpmi_get_voltage_domain(voltagegrp, domainid);
	if (!voltage || !level)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(voltage->lock);
	ret = voltagegrp->ops->get_voltage_level(voltagegrp->ops_priv, voltage->id, level);
	rpmi_env_unlock(voltage->lock);

	return ret;
}

static enum rpmi_voltage_state
rpmi_voltage_read_state(struct rpmi_voltage_group *voltagegrp,
			struct rpmi_voltage *voltage)
{
	enum rpmi_voltage_state state;

	if (!voltagegrp->ops || !voltagegrp->ops->get_config)
		return voltage->current_state;

	if (voltagegrp->ops->get_config(voltagegrp->ops_priv, voltage->id, &state))
		return voltage->current_state;

	return state;
}

static enum rpmi_error __rpmi_voltage_set_state(struct rpmi_voltage_group *voltagegrp,
					      struct rpmi_voltage *voltage,
					      enum rpmi_voltage_state state)
{
	enum rpmi_error ret = 0;
	struct rpmi_dlist *pos;

	rpmi_env_lock(voltage->lock);

	voltage->current_state = rpmi_voltage_read_state(voltagegrp, voltage);
	/**
	 * To disable a voltage domain:
	 * - Must not be disabled already if its a leaf or independent voltage domain.
	 * - If voltage domain is a parent then all child voltage domains must be in disabled
	 *   state already.
	 **/
	if (state == RPMI_VOLTAGE_STATE_DISABLED) {
		/* If voltage domain already disabled? use the cached state */
		if (voltage->current_state == RPMI_VOLTAGE_STATE_DISABLED) {
			rpmi_env_unlock(voltage->lock);
			return RPMI_ERR_ALREADY;
		}
#if 0
		/* If the voltage domain has no child or its a parent with single enable
		 * count then - disable, update cache and return */
		if (!voltage->child_count || voltage->enable_count == 1) {
			ret = voltagegrp->ops->set_config(voltagegrp->ops_priv, voltage->id, state);
			if (ret) {
				rpmi_env_unlock(voltage->lock);
				return ret;
			}

			voltage->current_state = state;
			voltage->enable_count -= 1;
			goto done;
		}

		/* Check if all child clocks are disabled, otherwise deny */
		rpmi_list_for_each(pos, &voltage->child_domain) {
			struct rpmi_voltage *cc = to_rpmi_voltage(pos);
			if (cc->current_state == RPMI_VOLTAGE_STATE_ENABLED) {
				rpmi_env_unlock(voltage->lock);
				return RPMI_ERR_DENIED;
			}
		}

		ret = voltagegrp->ops->set_config(voltagegrp->ops_priv, voltage->id, state);
		if (ret) {
			rpmi_env_unlock(voltage->lock);
			return ret;
		}

		voltage->current_state = state;
		voltage->enable_count -= 1;
#else

		ret = voltagegrp->ops->set_config(voltagegrp->ops_priv, voltage->id, state);
		if (ret) {
			rpmi_env_unlock(voltage->lock);
			return ret;
		}
		voltage->current_state = state;
#endif
		/* FIXME: We are only traversing voltage domain sub-tree of requested
		 * voltage. Need to traverse the parents to check if the disable
		 * condition is met or not and disable if rules are met */
	} else if (state == RPMI_VOLTAGE_STATE_ENABLED) {
		/**
		 * To enable a voltage domain:
		 * - Must not be enabled already if its a independent voltage domain
		 * - If a child voltage domain(at any level in voltage domain tree) then all its parent
		 *   must be enabled.
		 */

		/* If voltage domain is already enabled? use the cached state */
		if (voltage->current_state == RPMI_VOLTAGE_STATE_ENABLED) {
			rpmi_env_unlock(voltage->lock);
			return RPMI_ERR_ALREADY;
		}

		ret = voltagegrp->ops->set_config(voltagegrp->ops_priv, voltage->id, state);
		if (ret) {
			rpmi_env_unlock(voltage->lock);
			return ret;
		}

		voltage->current_state = state;
		goto done;
	}

done:
	rpmi_env_unlock(voltage->lock);
	return RPMI_SUCCESS;
}

static enum rpmi_error rpmi_voltage_set_state(struct rpmi_voltage_group *voltagegrp,
				     rpmi_uint32_t domainid,
				     enum rpmi_voltage_state state)
{
	enum rpmi_error ret;
	struct rpmi_voltage *voltage = rpmi_get_voltage_domain(voltagegrp, domainid);
	if (!voltage)
		return RPMI_ERR_INVALID_PARAM;

	ret = __rpmi_voltage_set_state(voltagegrp, voltage, state);

	return ret;
}

static enum rpmi_error rpmi_voltage_get_state(struct rpmi_voltage_group *voltagegrp,
				     rpmi_uint32_t domainid,
				     enum rpmi_voltage_state *state)
{
	struct rpmi_voltage *voltage = rpmi_get_voltage_domain(voltagegrp, domainid);

	if (!voltage || !state)
		return RPMI_ERR_INVALID_PARAM;

	rpmi_env_lock(voltage->lock);

	voltage->current_state = rpmi_voltage_read_state(voltagegrp, voltage);
	*state = voltage->current_state;

	rpmi_env_unlock(voltage->lock);

	return RPMI_SUCCESS;
}

/**
 * Initialize the voltage tree from provided
 * static platform voltage data.
 *
 * This function initializes the hierarchical structures
 * to represent the voltage association in the platform.
 **/
static struct rpmi_voltage *
rpmi_voltage_domain_tree_init(rpmi_uint32_t domain_count,
		     const struct rpmi_voltage_data *domain_tree_data,
		     const struct rpmi_voltage_platform_ops *ops,
		     void *ops_priv)
{
	int ret;
	rpmi_uint32_t domainid;
	rpmi_uint32_t level;
	enum rpmi_voltage_state state;
	struct rpmi_voltage *voltage;

	struct rpmi_voltage *voltage_tree =
		rpmi_env_zalloc(sizeof(struct rpmi_voltage) * domain_count);

	/* initialize all clocks instances */
	for (domainid = 0; domainid < domain_count; domainid++) {
		voltage = &voltage_tree[domainid];
		voltage->id = domainid;
		voltage->cdata = &domain_tree_data[domainid];

		RPMI_INIT_LIST_HEAD(&voltage->node);
		RPMI_INIT_LIST_HEAD(&voltage->child_domain);

		/* all the votage domains defualt to disabled */
		voltage->current_state = RPMI_VOLTAGE_STATE_DISABLED;

		voltage->lock = rpmi_env_alloc_lock();
	}

	/* Once all voltage domain instances initialized, link the voltage domains based
	 * on the voltage domain tree hierarchy based on the provided voltage domain tree
	 * data */
	for (domainid = 0; domainid < domain_count; domainid++) {
		voltage = &voltage_tree[domainid];
		if (voltage->cdata->parent_id != -1U) {
			voltage->parent = &voltage_tree[voltage->cdata->parent_id];
			rpmi_list_add_tail(&voltage->node,
					   &voltage->parent->child_domain);
			voltage->parent->child_count += 1;
		}
		else {
			voltage->parent = NULL;
		}
	}

	return voltage_tree;
}

/*****************************************************************************
 * RPMI Voltage Serivce Group Functions
 ****************************************************************************/
static enum rpmi_error
rpmi_voltage_sg_get_num_domains(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);
	resp[1] = rpmi_to_xe32(trans->is_be, voltagegrp->domain_count);

	*response_datalen = 2 * sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_voltage_sg_get_attributes(struct rpmi_service_group *group,
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
	struct rpmi_voltage_attrs voltage_attrs;
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= voltagegrp->domain_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = rpmi_voltage_get_attrs(voltagegrp, domainid, &voltage_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	/* encode VOLTAGE_FORMAT */
	if (voltage_attrs.type & RPMI_VOLTAGE_FLAGS_FORMAT_LINEAR)
		flags |= RPMI_VOLTAGE_FLAGS_FORMAT_LINEAR;

	if (voltage_attrs.type & RPMI_VOLTAGE_FLAGS_FORMAT_CAN_CHANGE)
		flags |= RPMI_VOLTAGE_FLAGS_FORMAT_CAN_CHANGE;

	resp[3] = rpmi_to_xe32(trans->is_be, voltage_attrs.transition_latency);
	resp[2] = rpmi_to_xe32(trans->is_be, voltage_attrs.level_count);
	resp[1] = rpmi_to_xe32(trans->is_be, flags);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	if (voltage_attrs.name)
		rpmi_env_strncpy((char *)&resp[4], voltage_attrs.name, RPMI_VOLTAGE_NAME_MAX_LEN);

	resp_dlen = 8 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_voltage_sg_get_supp_levels(struct rpmi_service_group *group,
			     struct rpmi_service *service,
			     struct rpmi_transport *trans,
			     rpmi_uint16_t request_datalen,
			     const rpmi_uint8_t *request_data,
			     rpmi_uint16_t *response_datalen,
			     rpmi_uint8_t *response_data)
{
	enum rpmi_error ret;
	rpmi_uint32_t i = 0, j = 0;
	rpmi_uint32_t level_count;
	rpmi_uint32_t resp_dlen = 0, voltage_level_idx = 0;
	rpmi_uint32_t max_levels, remaining = 0, returned = 0;
	const rpmi_uint32_t *level_array;
	struct rpmi_voltage_attrs voltage_attrs;
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= voltagegrp->domain_count) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	ret = rpmi_voltage_get_attrs(voltagegrp, domainid, &voltage_attrs);
	if (ret) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)ret);
		goto done;
	}

	level_count = voltage_attrs.level_count;
	level_array = voltage_attrs.level_array;
	if (!level_count || !level_array) {
		resp_dlen = sizeof(*resp);
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_NOTSUPP);
		goto done;
	}

	voltage_level_idx = rpmi_to_xe32(trans->is_be,
				    ((const rpmi_uint32_t *)request_data)[1]);

	if (voltage_attrs.type & RPMI_VOLTAGE_FLAGS_FORMAT_LINEAR) {
		/* max, min and step */
		for (i = 0; i < 3; i++) {
			resp[4 + i] = rpmi_to_xe32(trans->is_be,
				(rpmi_uint32_t)(level_array[i + voltage_level_idx * sizeof(struct rpmi_voltage_level_liner) / sizeof(level_array[0])]));
		}
		returned = 1;
		remaining = level_count - (voltage_level_idx + returned);
		resp_dlen = (4 * sizeof(*resp)) +
				(returned * sizeof(struct rpmi_voltage_level_liner));

	}

	else if (voltage_attrs.type & RPMI_VOLTAGE_FLAGS_FORMAT_DISCRETE) {
		if (voltage_level_idx > level_count) {
			resp[0] = rpmi_to_xe32(trans->is_be,
					       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
			resp_dlen = sizeof(*resp);
			goto done;
		}

		/* max levels a rpmi message can accommodate */
		max_levels =
		(RPMI_MSG_DATA_SIZE(trans->slot_size) - (4 * sizeof(*resp))) /
					sizeof(struct rpmi_voltage_level_discrete);
		remaining = level_count - voltage_level_idx;
		if (remaining > max_levels)
			returned = max_levels;
		else
			returned = remaining;

		for (i = voltage_level_idx, j = 0; i <= (voltage_level_idx + returned - 1); i++, j++) {
			resp[4 + j] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)(level_array[i]));
		}

		remaining = level_count - (voltage_level_idx + returned);
		resp_dlen = (4 * sizeof(*resp)) +
				(returned * sizeof(struct rpmi_voltage_level_discrete));
	}
	else {
		DPRINTF("%s: invalid rate format for clk-%u\n", __func__, clkid);
		return RPMI_ERR_FAILED;
	}

	resp[3] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)returned);
	resp[2] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)remaining);
	/* No flags currently supported */
	resp[1] = rpmi_to_xe32(trans->is_be, 0);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

done:
	*response_datalen = resp_dlen;

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_voltage_sg_set_config(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t cfg, new_state;
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
			       ((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= voltagegrp->domain_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	cfg = rpmi_to_xe32(trans->is_be,
				((const rpmi_uint32_t *)request_data)[1]);

	/* get command from 0th index bit in config field */
	new_state = (cfg & 0b1) ? RPMI_VOLTAGE_STATE_ENABLED : RPMI_VOLTAGE_STATE_DISABLED;

	/* change voltage config synchronously */
	status = rpmi_voltage_set_state(voltagegrp, domainid, new_state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

done:
	*response_datalen = sizeof(*resp);

	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_voltage_sg_get_config(struct rpmi_service_group *group,
			 struct rpmi_service *service,
			 struct rpmi_transport *trans,
			 rpmi_uint16_t request_datalen,
			 const rpmi_uint8_t *request_data,
			 rpmi_uint16_t *response_datalen,
			 rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error status;
	enum rpmi_voltage_state state;
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= voltagegrp->domain_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = rpmi_voltage_get_state(voltagegrp, domainid, &state);
	if (status) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	/** RPMI config field only return enabled or disabled state */
	state = (state == RPMI_VOLTAGE_STATE_ENABLED)? 1 : 0;

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)state);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_voltage_sg_set_level(struct rpmi_service_group *group,
		       struct rpmi_service *service,
		       struct rpmi_transport *trans,
		       rpmi_uint16_t request_datalen,
		       const rpmi_uint8_t *request_data,
		       rpmi_uint16_t *response_datalen,
		       rpmi_uint8_t *response_data)
{
	enum rpmi_error status;
	rpmi_uint32_t level_u32;
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= voltagegrp->domain_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	level_u32 = rpmi_to_xe32(trans->is_be,
			      ((const rpmi_uint32_t *)request_data)[1]);

	if (level_u32 == RPMI_VOLTAGE_LEVEL_INVALID || level_u32 == 0) {
	    resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		goto done;
	}

	status = rpmi_voltage_set_level(voltagegrp, domainid, level_u32);
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);

done:
	*response_datalen = sizeof(*resp);
	return RPMI_SUCCESS;
}

static enum rpmi_error
rpmi_voltage_sg_get_level(struct rpmi_service_group *group,
		       struct rpmi_service *service,
		       struct rpmi_transport *trans,
		       rpmi_uint16_t request_datalen,
		       const rpmi_uint8_t *request_data,
		       rpmi_uint16_t *response_datalen,
		       rpmi_uint8_t *response_data)
{
	rpmi_uint16_t resp_dlen;
	enum rpmi_error status;
	rpmi_uint32_t level_u32;
	struct rpmi_voltage_group *voltagegrp = group->priv;
	rpmi_uint32_t *resp = (void *)response_data;

	rpmi_uint32_t domainid = rpmi_to_xe32(trans->is_be,
				     ((const rpmi_uint32_t *)request_data)[0]);

	if (domainid >= voltagegrp->domain_count) {
		resp[0] = rpmi_to_xe32(trans->is_be,
				       (rpmi_uint32_t)RPMI_ERR_INVALID_PARAM);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	status = rpmi_voltage_get_level(voltagegrp, domainid, &level_u32);
	if(status) {
		resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)status);
		resp_dlen = sizeof(*resp);
		goto done;
	}

	resp[1] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)(level_u32));
	resp[0] = rpmi_to_xe32(trans->is_be, (rpmi_uint32_t)RPMI_SUCCESS);

	resp_dlen = 2 * sizeof(*resp);

done:
	*response_datalen = resp_dlen;
	return RPMI_SUCCESS;
}

static struct rpmi_service rpmi_voltage_services[RPMI_VOLTAGE_SRV_MAX_COUNT] = {
	[RPMI_VOLTAGE_SRV_ENABLE_NOTIFICATION] = {
		.service_id = RPMI_VOLTAGE_SRV_ENABLE_NOTIFICATION,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = NULL,
	},
	[RPMI_VOLTAGE_SRV_GET_NUM_DOMAINS] = {
		.service_id = RPMI_VOLTAGE_SRV_GET_NUM_DOMAINS,
		.min_a2p_request_datalen = 0,
		.process_a2p_request = rpmi_voltage_sg_get_num_domains,
	},
	[RPMI_VOLTAGE_SRV_GET_ATTRIBUTES] = {
		.service_id = RPMI_VOLTAGE_SRV_GET_ATTRIBUTES,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_voltage_sg_get_attributes,
	},
	[RPMI_VOLTAGE_SRV_GET_SUPPORTED_LEVELS] = {
		.service_id = RPMI_VOLTAGE_SRV_GET_SUPPORTED_LEVELS,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_voltage_sg_get_supp_levels,
	},
	[RPMI_VOLTAGE_SRV_SET_CONFIG] = {
		.service_id = RPMI_VOLTAGE_SRV_SET_CONFIG,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_voltage_sg_set_config,
	},
	[RPMI_VOLTAGE_SRV_GET_CONFIG] = {
		.service_id = RPMI_VOLTAGE_SRV_GET_CONFIG,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_voltage_sg_get_config,
	},
	[RPMI_VOLTAGE_SRV_SET_LEVEL] = {
		.service_id = RPMI_VOLTAGE_SRV_SET_LEVEL,
		.min_a2p_request_datalen = 8,
		.process_a2p_request = rpmi_voltage_sg_set_level,
	},
	[RPMI_VOLTAGE_SRV_GET_LEVEL] = {
		.service_id = RPMI_VOLTAGE_SRV_GET_LEVEL,
		.min_a2p_request_datalen = 4,
		.process_a2p_request = rpmi_voltage_sg_get_level,
	},
};

struct rpmi_service_group *
rpmi_service_group_voltage_create(rpmi_uint32_t domain_count,
				const struct rpmi_voltage_data *voltage_tree_data,
				const struct rpmi_voltage_platform_ops *ops,
				void *ops_priv)
{
	struct rpmi_voltage_group *voltagegrp;
	struct rpmi_service_group *group;

	/* All critical parameters should be non-NULL */
	if (!domain_count || !voltage_tree_data || !ops) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return NULL;
	}

	/* Allocate voltage service group */
	voltagegrp = rpmi_env_zalloc(sizeof(*voltagegrp));
	if (!voltagegrp) {
		DPRINTF("%s: failed to allocate voltage service group instance\n",
			__func__);
		return NULL;
	}

	voltagegrp->voltage_domain_tree = rpmi_voltage_domain_tree_init(domain_count,
						 voltage_tree_data,
						 ops,
						 ops_priv);
	if (!voltagegrp->voltage_domain_tree) {
		DPRINTF("%s: failed to initialize voltage domain tree\n", __func__);
		rpmi_env_free(voltagegrp);
		return NULL;
	}

	voltagegrp->domain_count = domain_count;
	voltagegrp->ops = ops;
	voltagegrp->ops_priv = ops_priv;

	group = &voltagegrp->group;
	group->name = "voltage";
	group->servicegroup_id = RPMI_SRVGRP_VOLTAGE;
	group->servicegroup_version =
		RPMI_BASE_VERSION(RPMI_SPEC_VERSION_MAJOR, RPMI_SPEC_VERSION_MINOR);
	/* Allowed for both M-mode and S-mode RPMI context */
	group->privilege_level_bitmap = RPMI_PRIVILEGE_M_MODE_MASK | RPMI_PRIVILEGE_S_MODE_MASK;
	group->max_service_id = RPMI_VOLTAGE_SRV_MAX_COUNT;
	group->services = rpmi_voltage_services;
	group->lock = rpmi_env_alloc_lock();
	group->priv = voltagegrp;

	return group;
}

void rpmi_service_group_voltage_destroy(struct rpmi_service_group *group)
{
	rpmi_uint32_t domainid;
	struct rpmi_voltage_group *voltagegrp;

	if (!group) {
		DPRINTF("%s: invalid parameters\n", __func__);
		return;
	}

	voltagegrp = group->priv;

	for (domainid = 0; domainid < voltagegrp->domain_count; domainid++) {
		rpmi_env_free_lock(voltagegrp->voltage_domain_tree[domainid].lock);
	}

	rpmi_env_free(voltagegrp->voltage_domain_tree);
	rpmi_env_free_lock(group->lock);
	rpmi_env_free(group->priv);
}

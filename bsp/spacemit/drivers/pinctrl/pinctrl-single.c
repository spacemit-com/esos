/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>
#include <strings.h>
#include <riscv-ops.h>

#define DRIVER_NAME                     "pinctrl-single"
#define PCS_MUX_PINS_NAME               "pinctrl-single,pins"
#define PCS_MUX_BITS_NAME               "pinctrl-single,bits"
#define PCS_REG_NAME_LEN                ((sizeof(rt_uint64_t) * 2) + 3)
#define PCS_OFF_DISABLED                ~0U
#define BITS_PER_BYTE           8
#define ENOTSUPP        524     /* Operation is not supported */

#undef ARRAY_SIZE
#define ARRAY_SIZE(ar)     (sizeof(ar)/sizeof(ar[0]))

/**
 * struct pcs_pingroup - pingroups for a function
 * @np:         pingroup device node pointer
 * @name:       pingroup name
 * @gpins:      array of the pins in the group
 * @ngpins:     number of pins in the group
 * @node:       list node
 */
struct pcs_pingroup {
	struct dtb_node *np;
	const char *name;
	rt_uint32_t *gpins;
	rt_uint32_t ngpins;
	rt_list_t node;
};

/**
 * struct pcs_func_vals - mux function register offset and value pair
 * @reg:        register virtual address
 * @val:        register value
 */
struct pcs_func_vals {
	uintptr_t *reg;
	rt_uint32_t val;
	rt_uint32_t mask;
};

/**
 * struct pcs_conf_vals - pinconf parameter, pinconf register offset
 * and value, enable, disable, mask
 * @param:      config parameter
 * @val:        user input bits in the pinconf register
 * @enable:     enable bits in the pinconf register
 * @disable:    disable bits in the pinconf register
 * @mask:       mask bits in the register value
 */
struct pcs_conf_vals {
	enum pin_config_param param;
	rt_uint32_t val;
	rt_uint32_t enable;
	rt_uint32_t disable;
	rt_uint32_t mask;
};

/**
 * struct pcs_conf_type - pinconf property name, pinconf param pair
 * @name:       property name in DTS file
 * @param:      config parameter
 */
struct pcs_conf_type {
	const char *name;
	enum pin_config_param param;
};

/**
 * struct pcs_function - pinctrl function
 * @name:       pinctrl function name
 * @vals:       register and vals array
 * @nvals:      number of entries in vals array
 * @pgnames:    array of pingroup names the function uses
 * @npgnames:   number of pingroup names the function uses
 * @node:       list node
 */
struct pcs_function {
	const char *name;
	struct pcs_func_vals *vals;
	rt_uint32_t nvals;
	const char **pgnames;
	int npgnames;
	struct pcs_conf_vals *conf;
	int nconfs;
	rt_list_t node;
};

/**
 * struct pcs_gpiofunc_range - pin ranges with same mux value of gpio function
 * @offset:     offset base of pins
 * @npins:      number pins with the same mux value of gpio function
 * @gpiofunc:   mux value of gpio function
 * @node:       list node
 */
struct pcs_gpiofunc_range {
	rt_uint32_t offset;
	rt_uint32_t npins;
	rt_uint32_t gpiofunc;
	rt_list_t node;
};

/**
 * struct pcs_data - wrapper for data needed by pinctrl framework
 * @pa:         pindesc array
 * @cur:        index to current element
 *
 * REVISIT: We should be able to drop this eventually by adding
 * support for registering pins individually in the pinctrl
 * framework for those drivers that don't need a static array.
 */
struct pcs_data {
	struct pinctrl_pin_desc *pa;
	int cur;
};

/**
 * struct pcs_name - register name for a pin
 * @name:       name of the pinctrl register
 *
 * REVISIT: We may want to make names optional in the pinctrl
 * framework as some drivers may not care about pin names to
 * avoid kernel bloat. The pin names can be deciphered by user
 * space tools using debugfs based on the register address and
 * SoC packaging information.
 */
struct pcs_name {
	char name[PCS_REG_NAME_LEN];
};

/**
 * struct pcs_device - pinctrl device instance
 * @res:        resources
 * @base:       virtual address of the controller
 * @size:       size of the ioremapped area
 * @dev:        device entry
 * @pctl:       pin controller device
 * @mutex:      mutex protecting the lists
 * @width:      bits per mux register
 * @fmask:      function register mask
 * @fshift:     function register shift
 * @foff:       value to turn mux off
 * @fmax:       max number of functions in fmask
 * @is_pinconf: whether supports pinconf
 * @bits_per_pin:number of bits per pin
 * @names:      array of register names for pins
 * @pins:       physical pins on the SoC
 * @pgtree:     pingroup index radix tree
 * @ftree:      function index radix tree
 * @pingroups:  list of pingroups
 * @functions:  list of functions
 * @gpiofuncs:  list of gpio functions
 * @ngroups:    number of pingroups
 * @nfuncs:     number of functions
 * @desc:       pin controller descriptor
 * @read:       register read function to use
 * @write:      register write function to use
 */
struct pcs_device {
	uintptr_t base[2];
	rt_uint32_t size;
	struct dtb_node *dev;
	struct pinctrl_dev *pctl;
	struct rt_mutex mutex;
	uint32_t width;
	uint32_t fmask;
	rt_uint32_t fshift;
	uint32_t foff;
	rt_uint32_t fmax;
	bool bits_per_mux;
	bool is_pinconf;
	rt_uint32_t bits_per_pin;
	struct pcs_name *names;
	struct pcs_data pins;
	struct radix_tree_root pgtree;
	struct radix_tree_root ftree;
	rt_list_t pingroups;
	rt_list_t functions;
	rt_list_t gpiofuncs;
	rt_uint32_t ngroups;
	rt_uint32_t nfuncs;
	struct pinctrl_desc desc;
	rt_uint32_t (*read)(void *reg);
	void (*write)(rt_uint32_t val, void *reg);
};

static enum pin_config_param pcs_bias[] = {
	PIN_CONFIG_BIAS_PULL_DOWN,
	PIN_CONFIG_BIAS_PULL_UP,
};

static struct dtb_compatible_array __compatible[] = {
	{ .compatible = "pinctrl-single", .data = (void *)false },
	{ .compatible = "pinconf-single", .data = (void *)true },
	{ },
};

static rt_uint32_t pcs_readb(void *reg)
{
	return 0;
}

static rt_uint32_t pcs_readw(void *reg)
{
	return 0;
}

static rt_uint32_t pcs_readl(void *reg)
{
        return readl(reg);
}

static void pcs_writeb(rt_uint32_t val, void *reg)
{
	return;
}

static void pcs_writew(rt_uint32_t val, void *reg)
{
	return;
}

static void pcs_writel(rt_uint32_t val, void *reg)
{
        writel(val, reg);
}

static int pcs_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct pcs_device *pcs;

	pcs = pinctrl_dev_get_drvdata(pctldev);

	return pcs->ngroups;
}

static const char *pcs_get_group_name(struct pinctrl_dev *pctldev,
                                        rt_uint32_t gselector)
{
	struct pcs_device *pcs;
	struct pcs_pingroup *group;

	pcs = pinctrl_dev_get_drvdata(pctldev);
	group = radix_tree_lookup(&pcs->pgtree, gselector);
	if (!group) {
		rt_kprintf("%s could not find pingroup%i\n", __func__, gselector);
		return NULL;
	}

	return group->name;
}

static int pcs_get_group_pins(struct pinctrl_dev *pctldev,
                                        rt_uint32_t gselector,
                                        const rt_uint32_t **pins,
                                        rt_uint32_t *npins)
{
	struct pcs_device *pcs;
	struct pcs_pingroup *group;

	pcs = pinctrl_dev_get_drvdata(pctldev);
	group = radix_tree_lookup(&pcs->pgtree, gselector);
	if (!group) {
                rt_kprintf("%s could not find pingroup%i\n", __func__, gselector);
		return -RT_EINVAL;
	}

	*pins = group->gpins;
	*npins = group->ngpins;

	return 0;
}

static void pcs_dt_free_map(struct pinctrl_dev *pctldev,
                                struct pinctrl_map *map, rt_uint32_t num_maps)
{
	rt_free(map);
}

#define PARAMS_FOR_BITS_PER_MUX 3

/**
 * pcs_get_pin_by_offset() - get a pin index based on the register offset
 * @pcs: pcs driver instance
 * @offset: register offset from the base
 *
 * Note that this is OK as long as the pins are in a static array.
 */
static int pcs_get_pin_by_offset(struct pcs_device *pcs, rt_uint32_t offset)
{
	rt_uint32_t index;

	if (offset >= pcs->size) {
		rt_kprintf("mux offset out of range: 0x%x (0x%x)\n", offset, pcs->size);
		return -RT_EINVAL;
	}

	if (pcs->bits_per_mux)
		index = (offset * BITS_PER_BYTE) / pcs->bits_per_pin;
	else
		index = offset / (pcs->width / BITS_PER_BYTE);

	return index;
}

/**
 * pcs_add_function() - adds a new function to the function list
 * @pcs: pcs driver instance
 * @np: device node of the mux entry
 * @name: name of the function
 * @vals: array of mux register value pairs used by the function
 * @nvals: number of mux register value pairs
 * @pgnames: array of pingroup names for the function
 * @npgnames: number of pingroup names
 */
static struct pcs_function *pcs_add_function(struct pcs_device *pcs,
                                        struct dtb_node *np,
                                        const char *name,
                                        struct pcs_func_vals *vals,
                                        rt_uint32_t nvals,
                                        const char **pgnames,
                                        rt_uint32_t npgnames)
{
	struct pcs_function *function;
	function = rt_malloc(sizeof(*function));
	if (!function)
		return NULL;

	function->name = name;
	function->vals = vals;
	function->nvals = nvals;
	function->pgnames = pgnames;
	function->npgnames = npgnames;

	rt_mutex_take(&pcs->mutex, RT_WAITING_FOREVER);
        rt_list_insert_after(&pcs->functions, &function->node);
        radix_tree_insert(&pcs->ftree, pcs->nfuncs, function);
        pcs->nfuncs++;
        rt_mutex_release(&pcs->mutex);

        return function;
}

static void pcs_remove_function(struct pcs_device *pcs,
                                struct pcs_function *function)
{
	int i;

	rt_mutex_take(&pcs->mutex, RT_WAITING_FOREVER);
	for (i = 0; i < pcs->nfuncs; i++) {
		struct pcs_function *found;

		found = radix_tree_lookup(&pcs->ftree, i);
		if (found == function)
			radix_tree_delete(&pcs->ftree, i);
        }

	rt_list_remove(&function->node);
        rt_mutex_release(&pcs->mutex);
}

/**
 * pcs_add_pingroup() - add a pingroup to the pingroup list
 * @pcs: pcs driver instance
 * @np: device node of the mux entry
 * @name: name of the pingroup
 * @gpins: array of the pins that belong to the group
 * @ngpins: number of pins in the group
 */
static int pcs_add_pingroup(struct pcs_device *pcs,
                                        struct dtb_node *np,
                                        const char *name,
                                        rt_uint32_t *gpins,
                                        rt_uint32_t ngpins)
{
	struct pcs_pingroup *pingroup;

	pingroup = rt_malloc(sizeof(*pingroup));
	if (!pingroup)
		return -RT_ENOMEM;

	pingroup->name = name;
	pingroup->np = np;
	pingroup->gpins = gpins;
	pingroup->ngpins = ngpins;

	rt_mutex_take(&pcs->mutex, RT_WAITING_FOREVER);
	rt_list_insert_after(&pcs->pingroups, &pingroup->node);
	radix_tree_insert(&pcs->pgtree, pcs->ngroups, pingroup);
	pcs->ngroups++;
        rt_mutex_release(&pcs->mutex);

	return 0;
}

/**
 * pcs_free_pingroups() - free memory used by pingroups
 * @pcs: pcs driver instance
 */
static void pcs_free_pingroups(struct pcs_device *pcs)
{
	rt_list_t *pos, *tmp;
	int i;

	rt_mutex_take(&pcs->mutex, RT_WAITING_FOREVER);
	for (i = 0; i < pcs->ngroups; i++) {
		struct pcs_pingroup *pingroup;

		pingroup = radix_tree_lookup(&pcs->pgtree, i);
		if (!pingroup)
			continue;

		radix_tree_delete(&pcs->pgtree, i);
	}

	rt_list_for_each_safe(pos, tmp, &pcs->pingroups) {
		struct pcs_pingroup *pingroup;

		pingroup = rt_list_entry(pos, struct pcs_pingroup, node);
		rt_list_remove(&pingroup->node);
	}

	rt_mutex_release(&pcs->mutex);
}

static int pcs_parse_bits_in_pinctrl_entry(struct pcs_device *pcs,
                                                struct dtb_node *np,
                                                struct pinctrl_map **map,
                                                rt_uint32_t *num_maps,
                                                const char **pgnames)
{
	rt_uint32_t *pins;
	struct pcs_func_vals *vals;
	const rt_uint32_t *mux;
	int size, rows, index = 0, found = 0, res = -RT_ENOMEM;
	int npins_in_row;
	struct pcs_function *function;

	mux = dtb_node_get_property(np, PCS_MUX_BITS_NAME, &size);
        if (!mux) {
                rt_kprintf("no valid property for %s\n", np->name);
                return -RT_EINVAL;
        }

	if (size < (sizeof(*mux) * PARAMS_FOR_BITS_PER_MUX)) {
		rt_kprintf("bad data for %s\n", np->name);
		return -RT_EINVAL;
	}

	/* Number of elements in array */
	size /= sizeof(*mux);

	rows = size / PARAMS_FOR_BITS_PER_MUX;
	npins_in_row = pcs->width / pcs->bits_per_pin;

	vals = rt_malloc(sizeof(*vals) * rows * npins_in_row);
        if (!vals)
                return -RT_ENOMEM;

        pins = rt_malloc(sizeof(*pins) * rows * npins_in_row);
        if (!pins)
                goto free_vals;

	while (index < size) {
		rt_uint32_t offset, val;
		rt_uint32_t mask, bit_pos, val_pos, mask_pos, submask;
		rt_uint32_t pin_num_from_lsb;
		int pin;

		offset = fdt32_to_cpu(*(mux++));
		++index;
		val = fdt32_to_cpu(*(mux++));
		++index;
		mask = fdt32_to_cpu(*(mux++));
		++index;

		/* Parse pins in each row from LSB */
		while (mask) {
			bit_pos = ffs(mask);
			pin_num_from_lsb = bit_pos / pcs->bits_per_pin;
			mask_pos = ((pcs->fmask) << (bit_pos - 1));
			val_pos = val & mask_pos;
			submask = mask & mask_pos;
			mask &= ~mask_pos;

			if (submask != mask_pos) {
				rt_kprintf("Invalid submask 0x%x for %s at 0x%x\n", submask, np->name, offset);
				continue;
			}

			vals[found].mask = submask;
			vals[found].reg = (uintptr_t *)(pcs->base[0] + offset);
			vals[found].val = val_pos;

			pin = pcs_get_pin_by_offset(pcs, offset);
			if (pin < 0) {
				rt_kprintf("could not add functions for %s %ux\n", np->name, offset);
				break;
			}
			pins[found++] = pin + pin_num_from_lsb;
		}
	}

	pgnames[0] = np->name;
	function = pcs_add_function(pcs, np, np->name, vals, found, pgnames, 1);
	if (!function)
		goto free_pins;

	res = pcs_add_pingroup(pcs, np, np->name, pins, found);
	if (res < 0)
		goto free_function;

	(*map)->type = PIN_MAP_TYPE_MUX_GROUP;
 	(*map)->data.mux.group = np->name;
	(*map)->data.mux.function = np->name;

	if (pcs->is_pinconf) {
                rt_kprintf("pinconf not supported\n");
		goto free_pingroups;
	}

	*num_maps = 1;
	return 0;

free_pingroups:
	pcs_free_pingroups(pcs);
	*num_maps = 1;
free_function:
	pcs_remove_function(pcs, function);

free_pins:
	rt_free(pins);

free_vals:
	rt_free(vals);

	return res;
}

/*
 * check whether data matches enable bits or disable bits
 * Return value: 1 for matching enable bits, 0 for matching disable bits,
 *               and negative value for matching failure.
 */
static int pcs_config_match(rt_uint32_t data, rt_uint32_t enable, rt_uint32_t disable)
{
	int ret = -RT_EINVAL;

	if (data == enable)
		ret = 1;
	else if (data == disable)
		ret = 0;

	return ret;
}

static void add_config(struct pcs_conf_vals **conf, enum pin_config_param param,
                       rt_uint32_t value, rt_uint32_t enable, rt_uint32_t disable,
                       rt_uint32_t mask)
{
	(*conf)->param = param;
	(*conf)->val = value;
	(*conf)->enable = enable;
	(*conf)->disable = disable;
	(*conf)->mask = mask;
	(*conf)++;
}

static void add_setting(rt_uint64_t **setting, enum pin_config_param param,
                        rt_uint32_t arg)
{
	**setting = pinconf_to_config_packed(param, arg);
	(*setting)++;
}

/* add pinconf setting with 2 parameters */
static void pcs_add_conf2(struct pcs_device *pcs, struct dtb_node *np,
                          const char *name, enum pin_config_param param,
                          struct pcs_conf_vals **conf, rt_uint64_t **settings)
{
	uint32_t value[2];
	rt_uint32_t shift;
	int ret;

	ret = dtb_node_read_u32_array(np, name, value, 2);
	if (ret)
		return;

	/* set value & mask */
	value[0] &= value[1];
	shift = ffs(value[1]) - 1;

	/* skip enable & disable */
	add_config(conf, param, value[0], 0, 0, value[1]);
	add_setting(settings, param, value[0] >> shift);
}

/* add pinconf setting with 4 parameters */
static void pcs_add_conf4(struct pcs_device *pcs, struct dtb_node *np,
                          const char *name, enum pin_config_param param,
                          struct pcs_conf_vals **conf, rt_uint64_t **settings)
{
	uint32_t value[4];
	int ret;

	/* value to set, enable, disable, mask */
	ret = dtb_node_read_u32_array(np, name, value, 4);
	if (ret)
		return;

        if (!value[3]) {
		rt_kprintf("mask field of the property can't be 0\n");
		return;
	}

	value[0] &= value[3];
	value[1] &= value[3];
	value[2] &= value[3];
	ret = pcs_config_match(value[0], value[1], value[2]);
	if (ret < 0)
                rt_kprintf("failed to match enable or disable bits\n");

	add_config(conf, param, value[0], value[1], value[2], value[3]);
	add_setting(settings, param, ret);
}

static int pcs_parse_pinconf(struct pcs_device *pcs, struct dtb_node *np,
                             struct pcs_function *func,
                             struct pinctrl_map **map)

{
	struct pinctrl_map *m = *map;
	int i = 0, nconfs = 0;
	rt_uint64_t *settings = NULL, *s = NULL;
	struct pcs_conf_vals *conf = NULL;
	struct pcs_conf_type prop2[] = {
		{ "pinctrl-single,drive-strength", PIN_CONFIG_DRIVE_STRENGTH, },
		{ "pinctrl-single,slew-rate", PIN_CONFIG_SLEW_RATE, },
		{ "pinctrl-single,input-schmitt", PIN_CONFIG_INPUT_SCHMITT, },
	};

	struct pcs_conf_type prop4[] = {
		{ "pinctrl-single,bias-pullup", PIN_CONFIG_BIAS_PULL_UP, },
		{ "pinctrl-single,bias-pulldown", PIN_CONFIG_BIAS_PULL_DOWN, },
		{ "pinctrl-single,input-schmitt-enable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, },
	};

	/* If pinconf isn't supported, don't parse properties in below. */
	if (!pcs->is_pinconf)
		return 0;

	/* cacluate how much properties are supported in current node */
	for (i = 0; i < ARRAY_SIZE(prop2); i++) {
		if (dtb_node_get_property(np, prop2[i].name, NULL))
			nconfs++;
	}

	for (i = 0; i < ARRAY_SIZE(prop4); i++) {
		if (dtb_node_get_property(np, prop4[i].name, NULL))
			nconfs++;
	}

	if (!nconfs)
		return 0;

	func->conf = rt_malloc(sizeof(struct pcs_conf_vals) * nconfs);
	if (!func->conf)
		return -RT_ENOMEM;

	func->nconfs = nconfs;
	conf = &(func->conf[0]);
	m++;
	settings = rt_malloc(sizeof(rt_uint64_t) * nconfs);
	if (!settings)
		return -RT_ENOMEM;

	s = &settings[0];

	for (i = 0; i < ARRAY_SIZE(prop2); i++)
		pcs_add_conf2(pcs, np, prop2[i].name, prop2[i].param,
				&conf, &s);

	for (i = 0; i < ARRAY_SIZE(prop4); i++)
		pcs_add_conf4(pcs, np, prop4[i].name, prop4[i].param,
				&conf, &s);

	m->type = PIN_MAP_TYPE_CONFIGS_GROUP;
	m->data.configs.group_or_pin = np->name;
	m->data.configs.configs = settings;
	m->data.configs.num_configs = nconfs;
	
	return 0;
}

/**
 * smux_parse_one_pinctrl_entry() - parses a device tree mux entry
 * @pcs: pinctrl driver instance
 * @np: device node of the mux entry
 * @map: map entry
 * @num_maps: number of map
 * @pgnames: pingroup names
 *
 * Note that this binding currently supports only sets of one register + value.
 *
 * Also note that this driver tries to avoid understanding pin and function
 * names because of the extra bloat they would cause especially in the case of
 * a large number of pins. This driver just sets what is specified for the board
 * in the .dts file. Further user space debugging tools can be developed to
 * decipher the pin and function names using debugfs.
 *
 * If you are concerned about the boot time, set up the static pins in
 * the bootloader, and only set up selected pins as device tree entries.
 */
static int pcs_parse_one_pinctrl_entry(struct pcs_device *pcs,
                                                struct dtb_node *np,
                                                struct pinctrl_map **map,
                                                rt_uint32_t *num_maps,
                                                const char **pgnames)
{
	rt_uint32_t *pins;
	struct pcs_func_vals *vals;
	const rt_uint32_t *mux;
	int size, rows, index = 0, found = 0, res = -RT_ENOMEM;
	struct pcs_function *function;

	mux = dtb_node_get_property(np, PCS_MUX_PINS_NAME, &size);
	if ((!mux) || (size < sizeof(*mux) * 2)) {
                rt_kprintf("bad data for mux %s\n", np->name);
		return -RT_EINVAL;
	}

	size /= sizeof(*mux);   /* Number of elements in array */
	rows = size / 2;

	vals = rt_malloc(sizeof(*vals) * rows);
	if (!vals)
		return -RT_ENOMEM;

	pins = rt_malloc(sizeof(*pins) * rows);
	if (!pins)
		goto free_vals;

	while (index < size) {
		rt_uint32_t offset, val;
		int pin;

		offset = fdt32_to_cpu(*(mux++));
		index++;
		val = fdt32_to_cpu(*(mux++));
		index++;
		vals[found].reg = (void *)(pcs->base[0] + offset);
		vals[found].val = val;

		pin = pcs_get_pin_by_offset(pcs, offset);
		if (pin < 0) {
			rt_kprintf("could not add functions for %s %ux\n", np->name, offset);
			break;
		}
		pins[found++] = pin;
	}

	pgnames[0] = np->name;
	function = pcs_add_function(pcs, np, np->name, vals, found, pgnames, 1);
	if (!function)
		goto free_pins;

	res = pcs_add_pingroup(pcs, np, np->name, pins, found);
	if (res < 0)
		goto free_function;

	(*map)->type = PIN_MAP_TYPE_MUX_GROUP;
	(*map)->data.mux.group = np->name;
	(*map)->data.mux.function = np->name;

	if (pcs->is_pinconf) {
		res = pcs_parse_pinconf(pcs, np, function, map);
		if (res)
			goto free_pingroups;
		*num_maps = 2;
	} else {
		*num_maps = 1;
	}

	return 0;

free_pingroups:
	pcs_free_pingroups(pcs);
	*num_maps = 1;
free_function:
	pcs_remove_function(pcs, function);

free_pins:
	rt_free(pins);

free_vals:
	rt_free(vals);

	return res;
}

/**
 * pcs_dt_node_to_map() - allocates and parses pinctrl maps
 * @pctldev: pinctrl instance
 * @np_config: device tree pinmux entry
 * @map: array of map entries
 * @num_maps: number of maps
 */
static int pcs_dt_node_to_map(struct pinctrl_dev *pctldev, struct dtb_node *np_config,
		struct pinctrl_map **map, rt_uint32_t *num_maps)
{
	struct pcs_device *pcs;
	const char **pgnames;
	int ret;

	pcs = pinctrl_dev_get_drvdata(pctldev);

	/* create 2 maps. One is for pinmux, and the other is for pinconf. */
	*map = rt_malloc(sizeof(struct pinctrl_map) * 2);
	if (!*map)
		return -RT_ENOMEM;

	*num_maps = 0;

	pgnames = rt_malloc(sizeof(*pgnames));
	if (!pgnames) {
		ret = -RT_ENOMEM;
		goto free_map;
	}

	if (pcs->bits_per_mux) {
		ret = pcs_parse_bits_in_pinctrl_entry(pcs, np_config, map, num_maps, pgnames);
		if (ret < 0) {
                        rt_kprintf("no pins entries for %s\n", np_config->name);
			goto free_pgnames;
		}
	} else {
		ret = pcs_parse_one_pinctrl_entry(pcs, np_config, map, num_maps, pgnames);
		if (ret < 0) {
                        rt_kprintf("no pins entries for %s\n", np_config->name);
			goto free_pgnames;
		}
	}

	return 0;

free_pgnames:
        rt_free(pgnames);
free_map:
        rt_free(*map);

        return ret;
}

static const struct pinctrl_ops pcs_pinctrl_ops = {
	.get_groups_count = pcs_get_groups_count,
	.get_group_name = pcs_get_group_name,
	.get_group_pins = pcs_get_group_pins,
	.dt_node_to_map = pcs_dt_node_to_map,
	.dt_free_map = pcs_dt_free_map,
};

static int pcs_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct pcs_device *pcs;

	pcs = pinctrl_dev_get_drvdata(pctldev);

	return pcs->nfuncs;
}

static const char *pcs_get_function_name(struct pinctrl_dev *pctldev,
                                                rt_uint32_t fselector)
{
	struct pcs_device *pcs;
	struct pcs_function *func;

	pcs = pinctrl_dev_get_drvdata(pctldev);
	func = radix_tree_lookup(&pcs->ftree, fselector);
	if (!func) {
                rt_kprintf("%s could not find function%i\n", __func__, fselector);
		return NULL;
	}

	return func->name;
}

static int pcs_get_function_groups(struct pinctrl_dev *pctldev,
                                        rt_uint32_t fselector,
                                        const char * const **groups,
                                        rt_uint32_t * const ngroups)
{
	struct pcs_device *pcs;
	struct pcs_function *func;

	pcs = pinctrl_dev_get_drvdata(pctldev);
	func = radix_tree_lookup(&pcs->ftree, fselector);
	if (!func) {
                rt_kprintf("%s could not find function%i\n", __func__, fselector);
		return -RT_EINVAL;
	}

	*groups = func->pgnames;
	*ngroups = func->npgnames;

	return 0;
}

static int pcs_get_function(struct pinctrl_dev *pctldev, rt_uint32_t pin,
                            struct pcs_function **func)
{
	struct pcs_device *pcs = pinctrl_dev_get_drvdata(pctldev);
	struct pin_desc *pdesc = pin_desc_get(pctldev, pin);
	const struct pinctrl_setting_mux *setting;
	rt_uint32_t fselector;

	/* If pin is not described in DTS & enabled, mux_setting is NULL. */
	setting = pdesc->mux_setting;
	if (!setting)
		return -ENOTSUPP;
	fselector = setting->func;
	*func = radix_tree_lookup(&pcs->ftree, fselector);
	if (!(*func)) {
		rt_kprintf("%s could not find function%i\n", __func__, fselector);
		return -ENOTSUPP;
	}

	return 0;
}

static int pcs_enable(struct pinctrl_dev *pctldev, rt_uint32_t fselector,
        rt_uint32_t group)
{
	struct pcs_device *pcs;
	struct pcs_function *func;
	int i;

	pcs = pinctrl_dev_get_drvdata(pctldev);
	/* If function mask is null, needn't enable it. */
	if (!pcs->fmask)
		return 0;

	func = radix_tree_lookup(&pcs->ftree, fselector);
	if (!func)
		return -RT_EINVAL;

	/* rt_kprintf("enabling %s function%i\n", func->name, fselector); */

	for (i = 0; i < func->nvals; i++) {
		struct pcs_func_vals *vals;
		rt_uint32_t val, mask;

		vals = &func->vals[i];
		val = pcs->read(vals->reg);

		if (pcs->bits_per_mux)
			mask = vals->mask;
		else
			mask = pcs->fmask;

		val &= ~mask;
		val |= (vals->val & mask);
		pcs->write(val, vals->reg);
	}

	return 0;
}

static void pcs_disable(struct pinctrl_dev *pctldev, rt_uint32_t fselector,
                                        rt_uint32_t group)
{
	struct pcs_device *pcs;
	struct pcs_function *func;
	int i;

	pcs = pinctrl_dev_get_drvdata(pctldev);
	/* If function mask is null, needn't disable it. */
	if (!pcs->fmask)
		return;

	func = radix_tree_lookup(&pcs->ftree, fselector);
	if (!func) {
		rt_kprintf("%s could not find function%i\n", __func__, fselector);
		return;
	}

	/*
	 * Ignore disable if function-off is not specified. Some hardware
	 * does not have clearly defined disable function. For pin specific
	 * off modes, you can use alternate named states as described in
	 * pinctrl-bindings.txt.
	 */
	if (pcs->foff == PCS_OFF_DISABLED) {
		rt_kprintf("ignoring disable for %s function%i\n", func->name, fselector);
		return;
	}

	/* rt_kprintf("disabling function%i %s\n", fselector, func->name); */

	for (i = 0; i < func->nvals; i++) {
		struct pcs_func_vals *vals;
		rt_uint32_t val;

		vals = &func->vals[i];
		val = pcs->read(vals->reg);
		val &= ~pcs->fmask;
		val |= pcs->foff << pcs->fshift;
		pcs->write(val, vals->reg);
	}
}

static int pcs_request_gpio(struct pinctrl_dev *pctldev,
                            struct pinctrl_gpio_range *range, rt_uint32_t pin)
{
	struct pcs_device *pcs = pinctrl_dev_get_drvdata(pctldev);
	struct pcs_gpiofunc_range *frange = NULL;
	rt_list_t *pos, *tmp;
	int mux_bytes = 0;
	rt_uint32_t data;

	/* If function mask is null, return directly. */
	if (!pcs->fmask)
		return -ENOTSUPP;

	rt_list_for_each_safe(pos, tmp, &pcs->gpiofuncs) {
		frange = rt_list_entry(pos, struct pcs_gpiofunc_range, node);
		if (pin >= frange->offset + frange->npins
				|| pin < frange->offset)
			continue;
		mux_bytes = pcs->width / BITS_PER_BYTE;
		data = pcs->read((void *)(pcs->base[0] + pin * mux_bytes)) & ~pcs->fmask;
		data |= frange->gpiofunc;
		pcs->write(data, (void *)(pcs->base[0] + pin * mux_bytes));
		break;
	}

	return 0;
}

static const struct pinmux_ops pcs_pinmux_ops = {
	.get_functions_count = pcs_get_functions_count,
	.get_function_name = pcs_get_function_name,
	.get_function_groups = pcs_get_function_groups,
	.enable = pcs_enable,
	.disable = pcs_disable,
	.gpio_request_enable = pcs_request_gpio,
};

static int pcs_pinconf_get(struct pinctrl_dev *pctldev, rt_uint32_t pin, rt_uint64_t *config);
static int pcs_pinconf_set(struct pinctrl_dev *pctldev, rt_uint32_t pin, rt_uint64_t config);

/* Clear BIAS value */
static void pcs_pinconf_clear_bias(struct pinctrl_dev *pctldev, rt_uint32_t pin)
{
	rt_uint64_t config;
	int i;

	for (i = 0; i < ARRAY_SIZE(pcs_bias); i++) {
		config = pinconf_to_config_packed(pcs_bias[i], 0);
		pcs_pinconf_set(pctldev, pin, config);
	}
}

/*
 * Check whether PIN_CONFIG_BIAS_DISABLE is valid.
 * It's depend on that PULL_DOWN & PULL_UP configs are all invalid.
 */
static bool pcs_pinconf_bias_disable(struct pinctrl_dev *pctldev, rt_uint32_t pin)
{
	rt_uint64_t config;
	int i;

	for (i = 0; i < ARRAY_SIZE(pcs_bias); i++) {
		config = pinconf_to_config_packed(pcs_bias[i], 0);
		if (!pcs_pinconf_get(pctldev, pin, &config))
			goto out;
	}

	return true;
out:
	return false;
}

static int pcs_pinconf_get(struct pinctrl_dev *pctldev,
                                rt_uint32_t pin, rt_uint64_t *config)
{
	struct pcs_device *pcs = pinctrl_dev_get_drvdata(pctldev);
	struct pcs_function *func;
	enum pin_config_param param;
	rt_uint32_t offset = 0, data = 0, i, j, ret;

	ret = pcs_get_function(pctldev, pin, &func);
	if (ret)
		return ret;

	for (i = 0; i < func->nconfs; i++) {
		param = pinconf_to_config_param(*config);
		if (param == PIN_CONFIG_BIAS_DISABLE) {
			if (pcs_pinconf_bias_disable(pctldev, pin)) {
				*config = 0;
				return 0;
			} else {
				return -ENOTSUPP;
			}
		} else if (param != func->conf[i].param) {
			continue;
		}

		offset = pin * (pcs->width / BITS_PER_BYTE);
		data = pcs->read((void *)(pcs->base[0] + offset)) & func->conf[i].mask;
		switch (func->conf[i].param) {
		/* 4 parameters */
		case PIN_CONFIG_BIAS_PULL_DOWN:
		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			if ((data != func->conf[i].enable) ||
					(data == func->conf[i].disable))
				return -ENOTSUPP;
			*config = 0;
			break;
		/* 2 parameters */
		case PIN_CONFIG_INPUT_SCHMITT:
			for (j = 0; j < func->nconfs; j++) {
				switch (func->conf[j].param) {
				case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
					if (data != func->conf[j].enable)
						return -ENOTSUPP;
				break;
				default:
					break;
				}
			}
			*config = data;
			break;
		case PIN_CONFIG_DRIVE_STRENGTH:
		case PIN_CONFIG_SLEW_RATE:
		default:
			*config = data;
			break;
		}
		return 0;
	}
	return -ENOTSUPP;
}

static int pcs_pinconf_set(struct pinctrl_dev *pctldev,
                                rt_uint32_t pin, rt_uint64_t config)
{
        struct pcs_device *pcs = pinctrl_dev_get_drvdata(pctldev);
        struct pcs_function *func;
        rt_uint32_t offset = 0, shift = 0, i, data, ret;
        rt_uint16_t arg;

        ret = pcs_get_function(pctldev, pin, &func);
        if (ret)
                return ret;

        for (i = 0; i < func->nconfs; i++) {
                if (pinconf_to_config_param(config) == func->conf[i].param) {
                        offset = pin * (pcs->width / BITS_PER_BYTE);
                        data = pcs->read((void *)(pcs->base[0] + offset));
                        arg = pinconf_to_config_argument(config);
                        switch (func->conf[i].param) {
                        /* 2 parameters */
                        case PIN_CONFIG_INPUT_SCHMITT:
                        case PIN_CONFIG_DRIVE_STRENGTH:
                        case PIN_CONFIG_SLEW_RATE:
                                shift = ffs(func->conf[i].mask) - 1;
                                data &= ~func->conf[i].mask;
                                data |= (arg << shift) & func->conf[i].mask;
                                break;
                        /* 4 parameters */
                        case PIN_CONFIG_BIAS_DISABLE:
                                pcs_pinconf_clear_bias(pctldev, pin);
                                break;
                        case PIN_CONFIG_BIAS_PULL_DOWN:
                        case PIN_CONFIG_BIAS_PULL_UP:
                                if (arg)
                                        pcs_pinconf_clear_bias(pctldev, pin);
                                /* fall through */
                        case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
                                data &= ~func->conf[i].mask;
                                if (arg)
                                        data |= func->conf[i].enable;
                                else
                                        data |= func->conf[i].disable;
                                break;
                        default:
                                return -ENOTSUPP;
                        }
                        pcs->write(data, (void *)(pcs->base[0] + offset));
                        return 0;
                }
        }
        return -ENOTSUPP;
}

static int pcs_pinconf_group_get(struct pinctrl_dev *pctldev,
                                rt_uint32_t group, rt_uint64_t *config)
{
	const rt_uint32_t *pins;
	rt_uint32_t npins, old = 0;
	int i, ret;

	ret = pcs_get_group_pins(pctldev, group, &pins, &npins);
	if (ret)
		return ret;

	for (i = 0; i < npins; i++) {
		if (pcs_pinconf_get(pctldev, pins[i], config))
			return -ENOTSUPP;
		/* configs do not match between two pins */
		if (i && (old != *config))
			return -ENOTSUPP;
		old = *config;
	}

	return 0;
}

static int pcs_pinconf_group_set(struct pinctrl_dev *pctldev,
                                rt_uint32_t group, rt_uint64_t config)
{
	const rt_uint32_t *pins;
	rt_uint32_t npins;
	int i, ret;

	ret = pcs_get_group_pins(pctldev, group, &pins, &npins);
	if (ret)
		return ret;

	for (i = 0; i < npins; i++) {
		if (pcs_pinconf_set(pctldev, pins[i], config))
			return -ENOTSUPP;
	}

	return 0;
}

static const struct pinconf_ops pcs_pinconf_ops = {
	.pin_config_get = pcs_pinconf_get,
	.pin_config_set = pcs_pinconf_set,
	.pin_config_group_get = pcs_pinconf_group_get,
	.pin_config_group_set = pcs_pinconf_group_set,
	.is_generic = true,
};

/**
 * pcs_add_pin() - add a pin to the static per controller pin array
 * @pcs: pcs driver instance
 * @offset: register offset from base
 */
static int pcs_add_pin(struct pcs_device *pcs, rt_uint32_t offset,
                rt_uint32_t pin_pos)
{
	struct pinctrl_pin_desc *pin;
	struct pcs_name *pn;
	int i;

	i = pcs->pins.cur;
	if (i >= pcs->desc.npins) {
		rt_kprintf("too many pins, max %i\n", pcs->desc.npins);
		return -RT_ENOMEM;
	}

	pin = &pcs->pins.pa[i];
	pn = &pcs->names[i];
	rt_sprintf(pn->name, "%lx.%d", (rt_uint64_t)pcs->base[0] + offset, pin_pos);
	pin->name = pn->name;
	pin->number = i;
	pcs->pins.cur++;

	return i;
}

/**
 * pcs_allocate_pin_table() - adds all the pins for the pinctrl driver
 * @pcs: pcs driver instance
 *
 * In case of errors, resources are freed in pcs_free_resources.
 *
 * If your hardware needs holes in the address space, then just set
 * up multiple driver instances.
 */
static int pcs_allocate_pin_table(struct pcs_device *pcs)
{
	int mux_bytes, nr_pins, i;
	int num_pins_in_register = 0;

	mux_bytes = pcs->width / BITS_PER_BYTE;

	if (pcs->bits_per_mux) {
		pcs->bits_per_pin = fls(pcs->fmask);
		nr_pins = (pcs->size * BITS_PER_BYTE) / pcs->bits_per_pin;
		num_pins_in_register = pcs->width / pcs->bits_per_pin;
	} else {
		nr_pins = pcs->size / mux_bytes;
	}

	/* rt_kprintf("allocating %i pins\n", nr_pins); */
	pcs->pins.pa = rt_malloc(sizeof(*pcs->pins.pa) * nr_pins);
	if (!pcs->pins.pa)
		return -RT_ENOMEM;

        pcs->names = rt_malloc(sizeof(struct pcs_name) * nr_pins);
	if (!pcs->names)
		return -RT_ENOMEM;

	pcs->desc.pins = pcs->pins.pa;
	pcs->desc.npins = nr_pins;

	for (i = 0; i < pcs->desc.npins; i++) {
		rt_uint32_t offset;
		int res;
		int byte_num;
		int pin_pos = 0;

		if (pcs->bits_per_mux) {
			byte_num = (pcs->bits_per_pin * i) / BITS_PER_BYTE;
			offset = (byte_num / mux_bytes) * mux_bytes;
			pin_pos = i % num_pins_in_register;
		} else {
			offset = i * mux_bytes;
		}

		res = pcs_add_pin(pcs, offset, pin_pos);
		if (res < 0) {
			rt_kprintf("error adding pins: %i\n", res);
			return res;
		}
	}

	return 0;
}

static int pcs_add_gpio_func(struct dtb_node *node, struct pcs_device *pcs)
{
	const char *propname = "pinctrl-single,gpio-range";
	const char *cellname = "#pinctrl-single,gpio-range-cells";
	struct fdt_phandle_args gpiospec;
	struct pcs_gpiofunc_range *range;
	int ret, i;

	for (i = 0; ; i++) {
		ret = dtb_node_parse_phandle_with_args(node, propname, cellname, i, &gpiospec);
		/* Do not treat it as error. Only treat it as end condition. */
		if (ret) {
			ret = 0;
			break;
		}

		range = rt_malloc(sizeof(*range));
		if (!range) {
			ret = -RT_ENOMEM;
			break;
		}

		range->offset = gpiospec.args[0];
		range->npins = gpiospec.args[1];
		range->gpiofunc = gpiospec.args[2];
		rt_mutex_take(&pcs->mutex, RT_WAITING_FOREVER);
		rt_list_insert_after(&pcs->gpiofuncs, &range->node);
		rt_mutex_release(&pcs->mutex);
	}

	return ret;
}

int spacemit_pcs_init(void)
{
	int i = 0, j = 0, ret;
	rt_uint32_t *u32_ptr;
	struct pcs_device *pcs;
	int property_size;
	rt_uint64_t u32_value;
	struct dtb_node *dtb_head_node = get_dtb_node_head();
	struct dtb_node *compatible_node;

	for (i = 0; i < sizeof(__compatible) / sizeof(__compatible[0]); ++i) {
		if (__compatible[i].compatible) {
			compatible_node = dtb_node_find_compatible_node(dtb_head_node, __compatible[i].compatible);

			if (compatible_node != RT_NULL) {
				if (!dtb_node_device_is_available(compatible_node))
					continue;

				/* get the clk : done by AP */
				/* get the reset : done by AP */
				/* get the register */
				pcs = rt_malloc(sizeof(struct pcs_device));
				if (!pcs) {
					rt_kprintf("%s:%d, alloc pcs failed\n", __func__, __LINE__);
					return -RT_ENOMEM;
				}

				pcs->dev = compatible_node;
				rt_mutex_init(&pcs->mutex, "pcs_mut", RT_IPC_FLAG_PRIO);

				rt_list_init(&pcs->pingroups);
				rt_list_init(&pcs->functions);
				rt_list_init(&pcs->gpiofuncs);
				pcs->is_pinconf = __compatible[i].data;

				ret = dtb_node_read_u32(compatible_node, "pinctrl-single,register-width", &pcs->width);
				if (ret) {
					rt_kprintf("%s:%d, register width not specified\n", __func__, __LINE__);
					return -RT_EINVAL;
				}

				ret = dtb_node_read_u32(compatible_node, "pinctrl-single,function-mask", &pcs->fmask);
				if (!ret) {
					pcs->fshift = ffs(pcs->fmask) - 1;
					pcs->fmax = pcs->fmask >> pcs->fshift;
				} else {
					/* If mask property doesn't exist, function mux is invalid. */
					pcs->fmask = 0;
					pcs->fshift = 0;
					pcs->fmax = 0;
				}

				ret = dtb_node_read_u32(compatible_node, "pinctrl-single,function-off", &pcs->foff);
				if (ret)
					pcs->foff = PCS_OFF_DISABLED;

				pcs->bits_per_mux = dtb_node_read_bool(compatible_node, "pinctrl-single,bit-per-mux");

				for_each_property_cell(compatible_node, "reg", u32_value, u32_ptr, property_size) {
					pcs->base[j++] = u32_value;
				}
				pcs->size = pcs->base[j-1];

				j = 0;


				INIT_RADIX_TREE(&pcs->pgtree, RT_NULL);
				INIT_RADIX_TREE(&pcs->ftree, RT_NULL);

				switch (pcs->width) {
				case 8:
					pcs->read = pcs_readb;
					pcs->write = pcs_writeb;
					break;
				case 16:
					pcs->read = pcs_readw;
					pcs->write = pcs_writew;
					break;
				case 32:
					pcs->read = pcs_readl;
					pcs->write = pcs_writel;
					break;
				default:
					break;
				}

				pcs->desc.name = DRIVER_NAME;
				pcs->desc.pctlops = &pcs_pinctrl_ops;
				pcs->desc.pmxops = &pcs_pinmux_ops;
				if (pcs->is_pinconf)
					pcs->desc.confops = &pcs_pinconf_ops;
				/* pcs->desc.owner = THIS_MODULE; */

				ret = pcs_allocate_pin_table(pcs);
				if (ret < 0) {
					rt_kprintf("%s:%d, allocate pin table failed\n", __func__, __LINE__);
					return -RT_EINVAL;
				}

				pcs->pctl = pinctrl_register(&pcs->desc, pcs->dev, pcs);
				if (!pcs->pctl) {
					rt_kprintf("could not register single pinctrl driver\n");
					ret = -RT_EINVAL;
					return ret;
				}

				ret = pcs_add_gpio_func(compatible_node, pcs);
				if (ret < 0) {
					rt_kprintf("pcs add gpio function failed\n");
					return -RT_EINVAL;
				}
			}
		}
	}

	return 0;
}
// INIT_DEVICE_EXPORT(spacemit_pcs_init);

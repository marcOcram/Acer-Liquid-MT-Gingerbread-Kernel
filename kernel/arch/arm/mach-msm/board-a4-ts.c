/* linux/arch/arm/mach-msm/board-a4-ts.c
 *
 * Copyright (C) 2009 ACER Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/cypress_ts.h>
#include <mach/vreg.h>
#include "board-a4.h"
#include "smd_private.h"

static struct msm_gpio ts_config_data[] = {
	{ GPIO_CFG(A4_GPIO_CYP_TP_RST, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "ISSP_TP_RESET"},
	{ GPIO_CFG(A4_GPIO_CYP_TP_IRQ1, 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "CTP_INT1"},
	{ GPIO_CFG(A4_GPIO_CYP_TP_IRQ2, 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "CTP_INT2"},
	{ GPIO_CFG(A4_GPIO_CYP_TP_ISSP_SCLK, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "ISSP_TP_SCLK"},
	{ GPIO_CFG(A4_GPIO_CYP_TP_ISSP_SDATA, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "ISSP_TP_SDATA"},
};

static struct vreg *vreg_ts_ldo15;

static int a4_ts_hw_init(int on)
{
	int rc;

	if (on) {
		vreg_ts_ldo15 = vreg_get(NULL, "gp6");

		if (IS_ERR(vreg_ts_ldo15)) {
			pr_err("%s: vreg_ldo15 get failed (%ld)\n",
					__func__, PTR_ERR(vreg_ts_ldo15));
			return -ENOTSUPP;
		}

		rc = vreg_set_level(vreg_ts_ldo15, 2850);
		if (rc) {
			pr_err("%s: vreg set level failed (%d)\n",
					__func__, rc);
			return rc;
		}

		rc = vreg_enable(vreg_ts_ldo15);
		if (rc) {
			pr_err("%s: vreg enable failed (%d)\n",
					__func__, rc);
			return rc;
		}

		return msm_gpios_request_enable(ts_config_data,
				ARRAY_SIZE(ts_config_data));
	} else {
		msm_gpios_disable_free(ts_config_data,
				ARRAY_SIZE(ts_config_data));
		return 0;
	}
}

static int a4_ts_power(int ch)
{
	pr_debug("%s: enter\n", __func__);

	switch (ch) {
	case TS_VDD_POWER_OFF:
	case TS_VDD_POWER_ON:
		/* Do nothing */
		break;
	case TS_RESET:
		/* Reset chip */
		gpio_set_value(A4_GPIO_CYP_TP_RST, 1);
		msleep(1);
		gpio_set_value(A4_GPIO_CYP_TP_RST, 0);
		msleep(1);
		gpio_set_value(A4_GPIO_CYP_TP_RST, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
static bool a4_enable_fw_update(void)
{
	return 0;
}
#endif

static struct cypress_i2c_platform_data a4_cypress_ts_data = {
	.abs_x_min = 0,
	.abs_x_max = 479,
	.abs_y_min = 0,
	.abs_y_max = 799,
	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_id_min = 0,
	.abs_id_max = 0,
	.y_max = 929,
	.points_max = 4,
	.irqflags = IRQF_TRIGGER_FALLING,
	.hw_init = a4_ts_hw_init,
	.power = a4_ts_power,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
	.enable_fw_update = a4_enable_fw_update,
#endif
};

static ssize_t a4_virtual_keys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
			__stringify(EV_KEY) ":" __stringify(KEY_HOME)  ":70:890:80:80"
			":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH)  ":180:890:80:80"
			":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)   ":310:890:80:80"
			":" __stringify(EV_KEY) ":" __stringify(KEY_MENU)   ":425:890:80:80"
			"\n");
}

static struct kobj_attribute a4_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.cypress-ts",
		.mode = S_IRUGO,
	},
	.show = &a4_virtual_keys_show,
};

static struct attribute *a4_properties_attrs[] = {
	&a4_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group a4_properties_attr_group = {
	.attrs = a4_properties_attrs,
};


static struct i2c_board_info a4_ts_board_info[] = {
	{
		I2C_BOARD_INFO("cypress-ts", 0x4D),
		.platform_data = &a4_cypress_ts_data,
	},
};

void __init acer_ts_init(void)
{
	int ret = 0;
	struct kobject *properties_kobj;
	acer_smem_flag_t *acer_smem_flag;

	acer_smem_flag = (acer_smem_flag_t *)(smem_alloc(SMEM_ID_VENDOR0, sizeof(acer_smem_flag_t)));
	if (acer_smem_flag != NULL && acer_smem_flag->acer_hw_version < ACER_HW_VERSION_PVT)
		a4_ts_board_info[0].irq = MSM_GPIO_TO_INT(A4_GPIO_CYP_TP_IRQ1);
	else
		a4_ts_board_info[0].irq = MSM_GPIO_TO_INT(A4_GPIO_CYP_TP_IRQ2);

	i2c_register_board_info(0, a4_ts_board_info,
			ARRAY_SIZE(a4_ts_board_info));

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&a4_properties_attr_group);

	if (!properties_kobj || ret)
		pr_err("failed to create board_properties\n");
}

/* linux/arch/arm/mach-msm/board-a5-ts.c
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
#include "board-a5.h"
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
#include "smd_private.h"
#endif

static struct msm_gpio ts_config_data[] = {
	{ GPIO_CFG(A5_GPIO_CYP_TP_RST, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "ISSP_TP_RESET"},
	{ GPIO_CFG(A5_GPIO_CYP_TP_IRQ, 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "CTP_INT"},
	{ GPIO_CFG(A5_GPIO_CYP_TP_ISSP_SCLK, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "ISSP_TP_SCLK"},
	{ GPIO_CFG(A5_GPIO_CYP_TP_ISSP_SDATA, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "ISSP_TP_SDATA"},
};

static struct vreg *vreg_ts_ldo8;
static struct vreg *vreg_ts_ldo15;

static int a5_ts_hw_init(int on)
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

		vreg_ts_ldo8 = vreg_get(NULL, "gp7");

		if (IS_ERR(vreg_ts_ldo8)) {
			pr_err("%s: vreg_ldo8 get failed (%ld)\n",
					__func__, PTR_ERR(vreg_ts_ldo8));
			return -ENOTSUPP;
		}

		rc = vreg_set_level(vreg_ts_ldo8, 1800);
		if (rc) {
			pr_err("%s: vreg set level failed (%d)\n",
					__func__, rc);
			return rc;
		}

		rc = vreg_enable(vreg_ts_ldo8);
		if (rc) {
			pr_err("%s: vreg enable failed (%d)\n",
					__func__, rc);
			return rc;
		}

		return msm_gpios_request_enable(ts_config_data,
				ARRAY_SIZE(ts_config_data));
	} else {
		rc = vreg_disable(vreg_ts_ldo15);
		if (rc) {
			pr_err("%s: vreg disable failed (%d)\n",
					__func__, rc);
			return rc;
		}

		rc = vreg_disable(vreg_ts_ldo8);
		if (rc) {
			pr_err("%s: vreg disable failed (%d)\n",
					__func__, rc);
			return rc;
		}

		msm_gpios_disable_free(ts_config_data,
				ARRAY_SIZE(ts_config_data));
		return 0;
	}
}

static int a5_ts_power(int ch)
{
	int rc;

	pr_debug("%s: enter\n", __func__);

	switch (ch) {
	case TS_VDD_POWER_OFF:
		rc = vreg_disable(vreg_ts_ldo15);
		if (rc) {
			pr_err("%s: vreg enable failed (%d)\n",
					__func__, rc);
			return rc;
		}

		rc = vreg_disable(vreg_ts_ldo8);
		if (rc) {
			pr_err("%s: vreg enable failed (%d)\n",
					__func__, rc);
			return rc;
		}
		gpio_set_value(A5_GPIO_CYP_TP_RST, 0);
		break;
	case TS_VDD_POWER_ON:
		rc = vreg_enable(vreg_ts_ldo15);
		if (rc) {
			pr_err("%s: vreg enable failed (%d)\n",
					__func__, rc);
			return rc;
		}
		rc = vreg_enable(vreg_ts_ldo8);
		if (rc) {
			pr_err("%s: vreg enable failed (%d)\n",
					__func__, rc);
			return rc;
		}
		break;
	case TS_RESET:
		/* Reset chip */
		gpio_set_value(A5_GPIO_CYP_TP_RST, 1);
		msleep(1);
		gpio_set_value(A5_GPIO_CYP_TP_RST, 0);
		msleep(1);
		gpio_set_value(A5_GPIO_CYP_TP_RST, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
static bool a5_enable_fw_update(void)
{
	return 0;
}
#endif

static struct cypress_i2c_platform_data a5_cypress_ts_data = {
	.abs_x_min = 0,
	.abs_x_max = 479,
	.abs_y_min = 0,
	.abs_y_max = 1023,
	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_id_min = 0,
	.abs_id_max = 15,
	.y_max = 1023,
	.points_max = 4,
	.irqflags = IRQF_TRIGGER_FALLING,
	.hw_init = a5_ts_hw_init,
	.power = a5_ts_power,
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
	.enable_fw_update = a5_enable_fw_update,
#endif
};

static struct i2c_board_info a5_ts_board_info[] = {
	{
		I2C_BOARD_INFO("cypress-ts", 0x4D),
		.irq = MSM_GPIO_TO_INT(A5_GPIO_CYP_TP_IRQ),
		.platform_data = &a5_cypress_ts_data,
	},
};

void __init acer_ts_init(void)
{
	i2c_register_board_info(0, a5_ts_board_info,
			ARRAY_SIZE(a5_ts_board_info));
}
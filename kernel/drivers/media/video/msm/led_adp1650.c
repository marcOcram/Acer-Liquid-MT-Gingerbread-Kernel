/* Copyright (C) 2010, Acer Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/slab.h>
#include "led_adp1650.h"

struct adp1650_work {
	struct work_struct work;
};
static struct adp1650_work *adp1650_flash;
static struct i2c_client *adp1650_client;

/* Modify them for changing default flash driver ic output current */
#define DEFAULT_FLASH_HIGH_CURRENT	ADP1650_I_FL_1000mA
#define DEFAULT_FLASH_LOW_CURRENT	ADP1650_I_FL_300mA
#define DEFAULT_TORCH_CURRENT		ADP1650_I_TOR_100mA

static int32_t adp1650_i2c_txdata(unsigned short saddr, unsigned char *txdata,
					int length)
{
	struct i2c_msg msg[] = {
		{
		 .addr = saddr,
		 .flags = 0,
		 .len = length,
		 .buf = txdata,
		 },
	};

	if (i2c_transfer(adp1650_client->adapter, msg, 1) < 0) {
		CDBG("%s failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int32_t adp1650_i2c_write_b(unsigned short saddr, unsigned short baddr,
					unsigned short bdata)
{
	int32_t rc = -EIO;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = baddr;
	buf[1] = bdata;
	rc = adp1650_i2c_txdata(saddr, buf, 2);

	if (rc < 0)
		CDBG("%s failed, saddr = 0x%x, baddr = 0x%x,\
			 bdata = 0x%x!\n", __func__, saddr, baddr, bdata);

	return rc;
}

static int32_t adp1650_set_drv_ic_output(uint32_t flashc, uint32_t torchc)
{
	int32_t rc = -EIO;

	CDBG("%s called.\n", __func__);
	rc = adp1650_i2c_write_b(adp1650_client->addr,
				 ADP1650_REG_CURRENT_SET,
				 (flashc << ADP1650_I_FL_SHIFT) | torchc);
	if (rc < 0)
		CDBG("%s: Set flash driver ic current failed\n", __func__);

	return rc;
}

int32_t adp1650_flash_mode_control(int ctrl)
{
	int32_t rc = -EIO;
	unsigned short reg = 0, data = 0;

	CDBG("%s called.\n", __func__);
	reg = ADP1650_REG_OUTPUT_MODE;
	data = (ADP1650_IL_PEAK_2P75A << ADP1650_IL_PEAK_SHIFT) |
	       (ADP1650_STR_LV_LEVEL_SENSITIVE << ADP1650_STR_LV_SHIFT) |
	       (ADP1650_FREQ_FB_1P5MHZ_NOT_ALLOWED << ADP1650_FREQ_FB_SHIFT) |
	       (ADP1650_STR_MODE_SW << ADP1650_STR_MODE_SHIFT);

	switch (ctrl) {
	case MSM_CAMERA_LED_OFF:
		/* Disable flash light output */
		data |= ADP1650_OUTPUT_EN_OFF << ADP1650_OUTPUT_EN_SHIFT;
		data |= ADP1650_LED_MODE_STANDBY;
		rc = adp1650_i2c_write_b(adp1650_client->addr, reg, data);
		if (rc < 0)
			CDBG("%s: Disable flash light failed\n", __func__);
		break;

	case MSM_CAMERA_LED_HIGH:
		/* Enable flash light output at high current (1000 mA)*/
		adp1650_set_drv_ic_output(DEFAULT_FLASH_HIGH_CURRENT,
					  DEFAULT_TORCH_CURRENT);
		data |= ADP1650_OUTPUT_EN_ON << ADP1650_OUTPUT_EN_SHIFT;
		data |= ADP1650_LED_MODE_FLASH;
		rc = adp1650_i2c_write_b(adp1650_client->addr, reg, data);
		if (rc < 0)
			CDBG("%s: Enable flash light failed\n", __func__);
		break;

	case MSM_CAMERA_LED_LOW:
		/* Enable flash light output at low current (300 mA)*/
		adp1650_set_drv_ic_output(DEFAULT_FLASH_LOW_CURRENT,
					  DEFAULT_TORCH_CURRENT);
		data |= ADP1650_OUTPUT_EN_ON << ADP1650_OUTPUT_EN_SHIFT;
		data |= ADP1650_LED_MODE_FLASH;
		rc = adp1650_i2c_write_b(adp1650_client->addr, reg, data);
		if (rc < 0)
			CDBG("%s: Enable flash light failed\n", __func__);
		break;

	default:
		CDBG("%s: Illegal flash light parameter\n", __func__);
		break;
	}

	return rc;
}

int32_t adp1650_torch_mode_control(int ctrl)
{
	int32_t rc = -EIO;
	unsigned short reg = 0, data = 0;

	CDBG("%s called.\n", __func__);
	reg = ADP1650_REG_OUTPUT_MODE;
	data = (ADP1650_IL_PEAK_2P75A << ADP1650_IL_PEAK_SHIFT) |
	       (ADP1650_STR_LV_LEVEL_SENSITIVE << ADP1650_STR_LV_SHIFT) |
	       (ADP1650_FREQ_FB_1P5MHZ_NOT_ALLOWED << ADP1650_FREQ_FB_SHIFT) |
	       (ADP1650_STR_MODE_SW << ADP1650_STR_MODE_SHIFT);

	switch (ctrl) {
	case 0:
		/* Disable torch light output */
		data |= ADP1650_OUTPUT_EN_OFF << ADP1650_OUTPUT_EN_SHIFT;
		data |= ADP1650_LED_MODE_STANDBY;
		rc = adp1650_i2c_write_b(adp1650_client->addr, reg, data);
		if (rc < 0)
			CDBG("%s: Disable torch light failed\n", __func__);
		break;

	case 1:
		/* Enable torch light output */
		adp1650_set_drv_ic_output(DEFAULT_FLASH_HIGH_CURRENT,
					  DEFAULT_TORCH_CURRENT);
		data |= ADP1650_OUTPUT_EN_ON << ADP1650_OUTPUT_EN_SHIFT;
		data |= ADP1650_LED_MODE_ASSIST;
		rc = adp1650_i2c_write_b(adp1650_client->addr, reg, data);
		if (rc < 0)
			CDBG("%s: Enable torch light failed\n", __func__);
		break;

	default:
		CDBG("%s: Illegal torch light parameter\n", __func__);
		break;
	}

	return rc;
}

static int adp1650_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	int rc = 0;

	CDBG("%s called.\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("%s: i2c_check_functionality failed.\n", __func__);
		goto probe_failure;
	}

	adp1650_flash = kzalloc(sizeof(struct adp1650_work), GFP_KERNEL);
	if (!adp1650_flash) {
		CDBG("%s: kzalloc failed.\n", __func__);
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, adp1650_flash);
	adp1650_client = client;

	mdelay(50);

	CDBG("%s successed.\n", __func__);
	return 0;

probe_failure:
	CDBG("%s failed. rc = %d\n", __func__, rc);
	return rc;
}

static const struct i2c_device_id adp1650_i2c_id[] = {
	{"adp1650", 0},
	{}
};

static struct i2c_driver adp1650_i2c_driver = {
	.id_table = adp1650_i2c_id,
	.probe = adp1650_i2c_probe,
	.remove = __exit_p(adp1650_i2c_remove),
	.driver = {
		   .name = "adp1650",
		  },
};

int adp1650_open_init(void)
{
	int rc = 0;

	CDBG("%s called.\n", __func__);
	rc = i2c_add_driver(&adp1650_i2c_driver);
	if (rc < 0 || adp1650_client == NULL) {
		pr_err("%s adp1650 i2c driver failed. rc = %d\n", __func__, rc);
		rc = -ENOTSUPP;
	}

	return rc;
}


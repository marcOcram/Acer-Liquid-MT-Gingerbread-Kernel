/* drivers/i2c/chips/smb136.c - summit ic smb136
 *
 * Copyright (C) 2010 Acer Corporation.
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
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c/smb136.h>
#include "../../arch/arm/mach-msm/smd_private.h"

static struct i2c_client *this_client;
static unsigned int smb136_ready;

static int smb_i2c_read(char *buf, int count)
{
	if (1 != i2c_master_send(this_client, buf, 1)) {
		pr_err("[smb136]i2c_read --> Send reg. info error:%d\n", count);
		return -EIO;
	}

	if (count != i2c_master_recv(this_client, buf, count)) {
		pr_err("[smb136]i2c_read --> get response error:%d\n", count);
		return -EIO;
	}
	return 0;
}

static int smb_i2c_write(char *buf, int count)
{
	if (count != i2c_master_send(this_client, buf, count)) {
		pr_err("[smb136] i2c_write --> Send reg. info error:%d\n", count);
		return -EIO;
	}
	return 0;
}

static int mod_reg(char reg, char mask, char data)
{
	char buf[2];

	buf[0] = reg;
	if (smb_i2c_read(buf, 1) < 0)
		return -EIO;
	buf[1] = buf[0] & mask;
	buf[1] |= data;
	buf[0] = reg;
	if (smb_i2c_write(buf, 2) < 0)
		return -EIO;

	return 0;
}

int smb136_recharge(void)
{
	char buf[2];
	char tmp5, tmp31;

	tmp5 = REG05;
	if (smb_i2c_read(&tmp5, 1) < 0)
		return -EIO;

	tmp31 = REG31;
	if (smb_i2c_read(&tmp31, 1) < 0)
		return -EIO;

	buf[0] = REG05;
	buf[1] = 0x1c;
	smb_i2c_write(buf, 2);
	msleep(1000);
	buf[0] = REG05;
	buf[1] = 0x18;
	smb_i2c_write(buf, 2);
	msleep(1000);

	buf[0] = REG31;
	buf[1] = tmp31;
	smb_i2c_write(buf, 2);

	buf[0] = REG05;
	buf[1] = tmp5;
	smb_i2c_write(buf, 2);
	return 0;
}
EXPORT_SYMBOL(smb136_recharge);

static int smb136_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[smb136] i2c_check_functionality error!\n");
		return -ENOTSUPP;
	}

	this_client = client;
	smb136_ready = 1;

	smb136_control(ALLOW_VOLATILE_WRITE, 0);
	smb136_control(AICL_DETECTION_THRESHOLD, VOL_425);
	smb136_control(STAT_OUT_DISABLE, STAT_DISABLE);

#ifdef CONFIG_MACH_ACER_A5
	/*set termination current to 35mA*/
	mod_reg(REG00, 0xf9, 0x06);
	/*terminate charging current when charging full*/
	mod_reg(REG03, 0xbf, 0x00);
	smb136_control(POWER_DETECTION, POWER_DET_DIS);
#endif

#ifdef CONFIG_MACH_ACER_A4
	smb136_control(INPUT_CURRENT_LIMIT, MA_1000);
	smb136_control(COMPLETE_CHARGER_TIMEOUT, MIN_764);
	smb136_control(POWER_DETECTION, POWER_DET_DIS);
#endif
	return 0;
}

char smb136_control(enum smb_commands command, char data)
{
	char buf;
	char ret = 0;

	if (!smb136_ready) {
		pr_err("SMB136 is not ready!\n");
		return -EAGAIN ;
	}

	switch (command) {
	case ALLOW_VOLATILE_WRITE:
		ret = mod_reg(REG31, 0x7f, VOLATILE_WRITE);
		break;
	case INPUT_CURRENT_LIMIT:
		ret = mod_reg(REG01, 0x1f, data);
		break;
	case COMPLETE_CHARGER_TIMEOUT:
		ret = mod_reg(REG09, 0xf3, data);
		break;
	case AICL_DETECTION_THRESHOLD:
		ret = mod_reg(REG01, 0xfc, data);
		break;
	case POWER_DETECTION:
		ret = mod_reg(REG06, 0xdf, data);
		break;
	case RD_FULL_CHARGING:
		buf = REG36;
		if (smb_i2c_read(&buf, 1) < 0) {
			ret = -EIO;
			break;
		}

		if (buf & CHARGER_TERMINATION)
			ret = 1;
		break;
	case USBIN_MODE:
		ret = mod_reg(REG31, 0xf3, data);
		break;
	case PIN_CONTROl:
		ret = mod_reg(REG05, 0xef, data);
		break;
	case RD_IS_CHARGING:
		buf = REG36;
		if (smb_i2c_read(&buf, 1) < 0) {
			ret = -EIO;
			break;
		}

		if (buf & IS_CHARGE)
			ret = 1;
		break;
	case STAT_OUT_DISABLE:
		ret = mod_reg(REG31, 0xfe, data);
		break;
	case RD_CHG_TYPE:
		buf = REG34;
		if (smb_i2c_read(&buf, 1) < 0) {
			ret = -EIO;
			break;
		}

		if (buf & CHG_TYPE_AC)
			ret = CHG_TYPE_AC;
		else
			ret = CHG_TYPE_USB;
		break;
	default:
			return -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(smb136_control);

static int smb136_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id smb136_id[] = {
	{ "smb136", 0 },
	{ }
};

static struct i2c_driver smb136_driver = {
	.probe     = smb136_probe,
	.remove    = smb136_remove,
	.id_table  = smb136_id,
	.driver    = {
	.name      = "smb136",
	},
};

static int __init smb136_init(void)
{
	int res = 0;

	smb136_ready = 0;
	res = i2c_add_driver(&smb136_driver);
	if (res) {
		pr_err("%s: i2c_add_driver failed!\n", __func__);
		return res;
	}

	return 0;
}

static void __exit smb136_exit(void)
{
	i2c_del_driver(&smb136_driver);
}
module_init(smb136_init);
module_exit(smb136_exit);

MODULE_AUTHOR("Jacob Lee");
MODULE_DESCRIPTION("SMB136 driver");
MODULE_LICENSE("GPL");

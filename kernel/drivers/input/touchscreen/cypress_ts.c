/*
 *  Cypress Touch Screen Driver
 *
 *  Copyright (c) 2008 CYPRESS
 *  Copyright (c) 2008 Dan Liang
 *  Copyright (c) 2008 TimeSys Corporation
 *  Copyright (c) 2008 Justin Waters
 *
 *  Based on touchscreen code from Cypress Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/input.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <mach/board.h>
#include <linux/cypress_ts.h>
#include <linux/slab.h>
#include <asm/mach-types.h>

static struct workqueue_struct *cypress_wq;

struct cypress_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct  work;
	int (*hw_init)(int on);
	int (*power)(int ch);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
	int user_panel;
	bool (*enable_fw_update)(void);
	struct work_struct  work2;
#endif
	int prev_points;
	uint abs_x_max;
	uint abs_pressure_max;
	uint abs_id_max;
	uint y_max;
	uint points_max;
};

struct _pos {
	uint x;
	uint y;
	uint z;
	uint id;
};

enum {
	CANDO_PALNEL = 0xA,
	LGD_PANEL = 0xB,
	A4_PANEL = 0xC,
	USER_DEF_PANEL = 0xD,
	USEREX_DEF_PANEL = 0xE,
};

#define FILEPATH_CANDO	"/system/etc/a5_tp_fw_cando.hex"
#define FILEPATH_LGD	"/system/etc/a5_tp_fw_lgd.hex"
#define FILEPATH_A4	"/system/etc/a4_tp_fw.hex"
#define FILEPATH_USER	"/sdcard/a5_tp_fw.hex"
#define FILEPATH_USEREX	"/mnt/external_sd/a5_tp_fw.hex"

static struct _pos pos[4];

static int read_fw_version(struct cypress_ts_data *ts, uint8_t *data)
{
	uint8_t wdata[1] = {0x1C};
	int retry = 5;

	/* vote to turn on power */
	if (ts->power(TS_VDD_POWER_ON))
		pr_err("%s: power on failed\n", __func__);

	msleep(500);

	/* 0x1C, 0x1D, 0x1F: FW version */
	wdata[0] = 0x1C;
	while (retry > 0) {
		if (1 != i2c_master_send(ts->client, wdata, 1))
			pr_info("%s: i2c send err\n", __func__);

		if (4 != i2c_master_recv(ts->client, data, 4)) {
			pr_info("%s: i2c recv err\n", __func__);
			retry--;
		} else
			break;
		msleep(500);
	}

	/* vote to turn off power because update is finish */
	if (ts->power(TS_VDD_POWER_OFF))
		pr_err("%s: power off failed\n", __func__);

	if (ts->power(TS_RESET))
		pr_err("%s: reset failed\n", __func__);

	if (retry == 0)
		return -EIO;
	else
		return 0;
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
static int firmware_update_func(struct cypress_ts_data *ts, uint8_t panel)
{
	char file[30];
	signed char rc;

	if (panel == CANDO_PALNEL)
		strcpy(file, FILEPATH_CANDO);
	else if (panel == A4_PANEL)
		strcpy(file, FILEPATH_A4);
	else if (panel == USER_DEF_PANEL)
		strcpy(file, FILEPATH_USER);
	else if (panel == USEREX_DEF_PANEL)
		strcpy(file, FILEPATH_USEREX);
	else
		strcpy(file, FILEPATH_LGD);

	/* vote to turn on power */
	if (ts->power(TS_VDD_POWER_ON))
		pr_err("%s: power on failed\n", __func__);

	rc = download_firmware_main(file);

	/* vote to turn off power because update is finish */
	if (ts->power(TS_VDD_POWER_OFF))
		pr_err("%s: power off failed\n", __func__);

	if (rc)
		return rc;

	if (ts->power(TS_RESET))
		pr_info("%s: fail to reset tp\n", __func__);

	return 0;
}

static void firmware_work_func(struct work_struct *work2)
{
	struct cypress_ts_data *ts =
		container_of(work2, struct cypress_ts_data, work2);
	uint8_t rdata[4] = {0};
	uint8_t rdata_old[4] = {0};
	int rc;

	msleep(8000);
	rc = read_fw_version(ts, rdata_old);
	if (rc)
		pr_err("%s: read fw version fail\n", __func__);

	pr_info("%s: ver: %x.%x.%x\n",
		__func__, rdata_old[0], rdata_old[1], rdata_old[3]);
	if (rdata_old[0] == LGD_PANEL || rc)
		rc = firmware_update_func(ts, LGD_PANEL);
	else if (rdata_old[0] == CANDO_PALNEL)
		rc = firmware_update_func(ts, CANDO_PALNEL);
	else
		goto original_fw_err;

	if (rc)
		goto i2c_err;

	rc = read_fw_version(ts, rdata);
	if (rc) {
		pr_err("%s: read fw version fail\n", __func__);
		goto i2c_err;
	} else
		pr_info("old ver: %x.%x.%x, new ver: %x.%x.%x\n",
			rdata_old[0], rdata_old[1], rdata_old[3],
			rdata[0], rdata[1], rdata[3]);
i2c_err:
	pr_info("A5 FW update fail\n");
original_fw_err:
	pr_info("Original FW is wrong, ver: %x.%x.%x\n",
		rdata_old[0], rdata_old[1], rdata_old[3]);
}

static ssize_t cypress_fw_update_store(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cypress_ts_data *ts = dev_get_drvdata(device);

	if (!strcmp(buf, "CANDO\n"))
		ts->user_panel = CANDO_PALNEL;
	else if (!strcmp(buf, "LGD\n"))
		ts->user_panel = LGD_PANEL;
	else if (!strcmp(buf, "USER\n"))
		ts->user_panel = USER_DEF_PANEL;
	else if (!strcmp(buf, "USER_EX\n"))
		ts->user_panel = USEREX_DEF_PANEL;
	else if (!strcmp(buf, "AUTO\n"))
		ts->user_panel = 0;

	pr_info("%s: user_panel = 0x%x\n", __func__, ts->user_panel);

	return count;
}

static ssize_t cypress_fw_update_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	struct cypress_ts_data *ts = dev_get_drvdata(device);
	uint8_t rdata[4] = {0};
	uint8_t rdata_old[4] = {0};
	int rc, retry = 0;

	rc = read_fw_version(ts, rdata_old);
	if (rc)
		pr_err("%s: read fw version fail\n", __func__);

	do {
		if (machine_is_acer_a4()) {
			rc = firmware_update_func(ts, A4_PANEL);
		} else if (ts->user_panel)
			rc = firmware_update_func(ts, ts->user_panel);
		else {
			pr_info("%s: ver: %x.%x.%x\n",
				__func__, rdata_old[0], rdata_old[1], rdata_old[3]);
			if (rdata_old[0] == LGD_PANEL || rc)
				rc = firmware_update_func(ts, LGD_PANEL);
			else if (rdata_old[0] == CANDO_PALNEL)
				rc = firmware_update_func(ts, CANDO_PALNEL);
			else
				goto original_fw_err;
		}

		msleep(1000);

		rc = read_fw_version(ts, rdata);
		if (!rc) {
			return sprintf(buf, "new ver: %x.%x.%x\n",
					rdata[0], rdata[1], rdata[3]);
		}
		pr_info("%s: retry %d times\n", __func__, retry);
	} while (retry++ < 10);

	return sprintf(buf, "Firmware update fail\n");
original_fw_err:
	return sprintf(buf, "Original FW is wrong, ver: %x.%x.%x\n",
		rdata_old[0], rdata_old[1], rdata_old[3]);
}
#endif

static ssize_t cypress_fw_version_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	struct cypress_ts_data *ts = dev_get_drvdata(device);
	uint8_t rdata[4] = {0};
	int rc;

	rc = read_fw_version(ts, rdata);
	if (rc) {
		pr_info("%s: read fw version fail\n", __func__);
		goto i2c_err;
	}

	return sprintf(buf, "%x.%x.%x\n", rdata[0], rdata[1], rdata[3]);

i2c_err:
	return sprintf(buf, "Unknown firmware\n");
}

static ssize_t cypress_sensitivity_store(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cypress_ts_data *ts = dev_get_drvdata(device);
	uint8_t wdata[2] = {0};

	wdata[0] = 0x1B; /* 0x1B: sensitivity*/
	wdata[1] = (uint8_t)simple_strtoul(buf, NULL, 0);
	if (2 != i2c_master_send(ts->client, wdata, 2))
		goto i2c_err;

	return count;

i2c_err:
	pr_err("%s: i2c error\n", __func__);
	return 0;
}

static ssize_t cypress_sensitivity_show(struct device *device,
		struct device_attribute *attr,
		char *buf)
{
	struct cypress_ts_data *ts = dev_get_drvdata(device);
	uint8_t wdata[1] = {0};
	uint8_t rdata[1] = {0};

	wdata[0] = 0x1B; /* 0x1B: sensitivity*/
	if (1 != i2c_master_send(ts->client, wdata, 1))
		goto i2c_err;

	if (1 != i2c_master_recv(ts->client, rdata, 1))
		goto i2c_err;

	return sprintf(buf, "sensitivity: %d\n", rdata[0]);

i2c_err:
	pr_err("%s: i2c error\n", __func__);
	return 0;
}


static struct device_attribute ts_ver_attrs =
__ATTR(version, S_IRUGO, cypress_fw_version_show, NULL);

static struct device_attribute ts_sensitivity_attrs =
__ATTR(sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
	cypress_sensitivity_show, cypress_sensitivity_store);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
static struct device_attribute ts_update_attrs =
__ATTR(update, S_IRUGO | S_IWUSR | S_IWGRP,
	cypress_fw_update_show, cypress_fw_update_store);
#endif

static int cypress_set_power_state(int status, struct i2c_client *client)
{
	uint8_t wdata[2] = {0};

	pr_debug("%s: status: %x\n", __func__, status);
	switch (status) {
	case INIT_STATE:
		/* TODO: read fw version in initial state */
		break;
	case SUSPEND_STATE:
		/* set deep sleep mode */
		wdata[0] = 0;
		wdata[1] = 2;
		if (2 != i2c_master_send(client, wdata, 2))
			goto i2c_err;
		break;
	default:
		break;
	}

	return 0;

i2c_err:
	pr_err("%s: i2c error (%d)\n", __func__, status);
	return -ENXIO;
}

static void cypress_work_func(struct work_struct *work)
{
	struct cypress_ts_data *ts =
		container_of(work, struct cypress_ts_data, work);
	uint8_t rdata[19] = {0};
	uint8_t wdata[1] = {0};
	int points = 0;
	int i;

	wdata[0] = 0x02;
	if (1 != i2c_master_send(ts->client, wdata, 1))
		goto i2c_err;
	if (9 != i2c_master_recv(ts->client, rdata, 9)) {
		pr_err("%s: i2c recv error\n", __func__);
		goto i2c_err;
	}

	points = ((rdata[0] & 0x0F) > ts->points_max)
		? ts->points_max : (rdata[0] & 0x0F);

	if (points > 0) {
		pos[0].x = rdata[1] << 8 | rdata[2];
		pos[0].y = rdata[3] << 8 | rdata[4];
		pos[0].z = rdata[5];
		pos[0].id = (rdata[6] >> 4) & 0x0F;
	}

	if (points > 1) {
		wdata[0] = 0x08;
		if (1 != i2c_master_send(ts->client, wdata, 1))
			goto i2c_err;
		if (19 != i2c_master_recv(ts->client, rdata, 19))
			goto i2c_err;

		pos[1].x = rdata[1] << 8 | rdata[2];
		pos[1].y = rdata[3] << 8 | rdata[4];
		pos[1].z = rdata[5];
		pos[1].id = rdata[0] & 0x0F;
		pos[2].x = rdata[8] << 8 | rdata[9];
		pos[2].y = rdata[10] << 8 | rdata[11];
		pos[2].z = rdata[12];
		pos[2].id = (rdata[11] >> 4) & 0x0F;
		pos[3].x = rdata[14] << 8 | rdata[15];
		pos[3].y = rdata[16] << 8 | rdata[17];
		pos[3].z = rdata[18];
		pos[3].id = rdata[13] & 0x0F;
	}

	for (i = 0; i < points; i++) {
		/*
		   pr_info("x%d = %u,  y%d = %u, z%d = %u, id%d=%u,  points = %d\n",
		   i, pos[i].x, i, pos[i].y, i, pos[i].z, i, pos[i].id, points);
		 */
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
			(pos[i].x > ts->abs_x_max) ? 0 : pos[i].x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
			(pos[i].y > ts->y_max) ? 0 : pos[i].y);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR,
			(pos[i].z > ts->abs_pressure_max) ? 0 : pos[i].z/2);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
			(pos[i].z > ts->abs_pressure_max) ? 0 : pos[i].z);
		if (ts->abs_id_max > 0)
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
				((pos[i].id-1) > ts->abs_id_max) ? 0 : (pos[i].id-1));
		input_mt_sync(ts->input_dev);
	}

	for (i = 0; i < ts->prev_points - points; i++) {
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
		input_mt_sync(ts->input_dev);
	}

	ts->prev_points = points;
	input_sync(ts->input_dev);

i2c_err:
	enable_irq(ts->client->irq);
}

static irqreturn_t cypress_ts_interrupt(int irq, void *dev_id)
{
	struct cypress_ts_data *ts = dev_id;

	disable_irq_nosync(ts->client->irq);
	queue_work(cypress_wq, &ts->work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void cypress_early_suspend(struct early_suspend *h)
{
	struct cypress_ts_data *ts;

	pr_info("%s: enter\n", __func__);
	ts = container_of(h, struct cypress_ts_data, early_suspend);

	cypress_set_power_state(SUSPEND_STATE, ts->client);

	if (ts->power(TS_VDD_POWER_OFF))
		pr_err("%s: power off failed\n", __func__);
}

void cypress_early_resume(struct early_suspend *h)
{
	struct cypress_ts_data *ts;

	pr_info("%s: enter\n", __func__);
	ts = container_of(h, struct cypress_ts_data, early_suspend);

	if (ts->power(TS_VDD_POWER_ON))
		pr_err("%s: power on failed\n", __func__);

	if (ts->power(TS_RESET))
		pr_err("%s: reset failed\n", __func__);
}
#endif

static int cypress_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cypress_ts_data *ts = NULL;
	struct cypress_i2c_platform_data *pdata;
	int ret = 0;

	pr_info("%s: enter\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct cypress_ts_data), GFP_KERNEL);
	if (!ts) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	memset(pos, 0, sizeof(pos));
	INIT_WORK(&ts->work, cypress_work_func);
	ts->client = client;
	strlcpy(client->name, CYPRESS_TS_DRIVER_NAME,
		strlen(CYPRESS_TS_DRIVER_NAME));
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;

	if (pdata) {
		ts->hw_init = pdata->hw_init;
		ts->power = pdata->power;
		ts->abs_x_max = pdata->abs_x_max;
		ts->abs_pressure_max = pdata->abs_pressure_max;
		ts->abs_id_max = pdata->abs_id_max;
		ts->y_max = pdata->y_max;
		ts->points_max = pdata->points_max;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
		ts->enable_fw_update = pdata->enable_fw_update;
#endif

		ret = ts->hw_init(1);
		if (ret) {
			pr_err("%s: hw init failed\n", __func__);
			goto err_hw_init_failed;
		}

		ret = ts->power(TS_RESET);
		if (ret) {
			pr_err("%s: reset failed\n", __func__);
			goto err_power_on_failed;
		}
	}

	if (cypress_set_power_state(INIT_STATE, ts->client) != 0) {
		pr_info("%s: set mode  failed\n", __func__);
		goto err_power_on_failed;
	}

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		pr_err("%s: Failed to allocate input device\n", __func__);
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = CYPRESS_TS_DRIVER_NAME;
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(ABS_MT_TOUCH_MAJOR, ts->input_dev->keybit);

	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
		pdata->abs_pressure_min, pdata->abs_pressure_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
		pdata->abs_pressure_min, pdata->abs_pressure_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
		pdata->abs_x_min, pdata->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
		pdata->abs_y_min, pdata->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID,
		pdata->abs_id_min, pdata->abs_id_max, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		pr_err("%s: Unable to register %s input device\n",
			__func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	if (client->irq) {
		ret = request_irq(client->irq, cypress_ts_interrupt,
			pdata->irqflags, client->name, ts);
		if (ret) {
			pr_err("%s: Unable to register %s irq\n",
				__func__, ts->input_dev->name);
			ret = -ENOTSUPP;
			goto err_request_irq;
		}
	}

	if (device_create_file(&client->dev, &ts_ver_attrs))
		pr_err("%s: device_create_file ts_ver_attrs error\n", __func__);

	if (device_create_file(&client->dev, &ts_sensitivity_attrs))
		pr_err("%s: device_create_file ts_sensitivity_attrs error\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
	if (device_create_file(&client->dev, &ts_update_attrs))
		pr_err("%s: device_create_file ts_update_attrs error\n", __func__);

	if (ts->enable_fw_update()) {
		INIT_WORK(&ts->work2, firmware_work_func);
		queue_work(cypress_wq, &ts->work2);
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ts->early_suspend.suspend = cypress_early_suspend;
	ts->early_suspend.resume = cypress_early_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	pr_info("%s: probe done\n", __func__);
	return 0;

err_request_irq:
	free_irq(client->irq, ts);
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
err_power_on_failed:
err_hw_init_failed:
	ts->hw_init(0);
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int cypress_remove(struct i2c_client *client)
{
	struct cypress_ts_data *ts = i2c_get_clientdata(client);

	pr_info("%s: enter\n", __func__);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	if (ts->hw_init(0))
		pr_err("%s: hw deinit failed\n", __func__);
	kfree(ts);
	return 0;
}

static const struct i2c_device_id cypress_id[] = {
	{ CYPRESS_TS_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver cypress_ts_driver = {
	.probe		= cypress_probe,
	.remove		= cypress_remove,
	.id_table	= cypress_id,
	.driver		= {
		.name = CYPRESS_TS_DRIVER_NAME,
	},
};

static int __init cypress_init(void)
{
	pr_info("%s: enter\n", __func__);
	cypress_wq = create_singlethread_workqueue("cypress_wq");
	if (!cypress_wq)
		return -ENOMEM;

	return i2c_add_driver(&cypress_ts_driver);
}

static void __exit cypress_exit(void)
{
	pr_info("%s: enter\n", __func__);
	i2c_del_driver(&cypress_ts_driver);
	if (cypress_wq)
		destroy_workqueue(cypress_wq);
}

module_init(cypress_init);
module_exit(cypress_exit);

MODULE_AUTHOR("Peng Chang <Peng_Chang@acer.com.tw>");
MODULE_DESCRIPTION("CYPRESS driver");
MODULE_LICENSE("GPL v2");


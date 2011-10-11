/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2008 Bosch Sensortec GmbH
 * All Rights Reserved
 */

#if defined(CONFIG_ACER_DEBUG)
#define DEBUG
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>

#include "adxl346.h"

#define ADXL346_GPIO 50
#define ADXL346_IOC_MAGIC 'B'
#define ADXL346_READ_ACCEL_XYZ _IOWR(ADXL346_IOC_MAGIC,1,short)

#define ADXL346_IOC_MAXNR 49

#define ADXL346_I2C_NAME "adxl346"
#define ADXL346_DEVICE_NAME "adxl346"

static const struct i2c_device_id adxl346_id[] = {
	{ ADXL346_I2C_NAME, 0 },
	{ }
};



static struct adxl346_data* adxl346_data;

static int adxl346_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int adxl346_remove(struct i2c_client *client);
static void adxl346_i2c_delay(unsigned int msec);
#ifdef adxl_suspend
static int adxl346_suspend(struct i2c_client *client, pm_message_t mesg);
static int adxl346_resume(struct i2c_client *client);
#else
static int adxl346_pm_suspend(struct device *dev);
static int adxl346_pm_resume(struct device *dev);
#endif

#ifndef adxl_suspend
static struct i2c_client *adxl_client;
static struct dev_pm_ops adxl_device_pm_ops = {
	.suspend = adxl346_pm_suspend,
	.resume = adxl346_pm_resume,
};
#endif

/* new style I2C driver struct */
static struct i2c_driver adxl346_driver = {
	.probe = adxl346_probe,
	.remove = __devexit_p(adxl346_remove),
	.id_table = adxl346_id,
#ifdef adxl_suspend
	.suspend = adxl346_suspend,
	.resume = adxl346_resume,
#endif
	.driver = {
		.name = ADXL346_I2C_NAME,
		.owner = THIS_MODULE,
#ifndef adxl_suspend
		.pm = &adxl_device_pm_ops,
#endif
	},
};

/*	i2c delay routine for eeprom	*/
static inline void adxl346_i2c_delay(unsigned int msec)
{
	mdelay(msec);
}

/*	open command for BMA150 device file	*/
static int adxl346_open(struct inode *inode, struct file *file)
{
	if( adxl346_data->client == NULL)
	{
		pr_err("[adxl346] I2C driver not install (adxl346_open)\n");
		return -1;
	}
	pr_debug("[adxl346] ADXL346 has been opened\n");
	return 0;
}

/*	release command for ADXL346 device file	*/
static int adxl346_close(struct inode *inode, struct file *file)
{
	pr_debug("[adxl346] adxl346 has been closed\n");
	return 0;
}

/*	ioctl command for ADXL346 device file	*/
static int adxl346_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	unsigned char data[6];
	if (_IOC_TYPE(cmd) != ADXL346_IOC_MAGIC) {
		pr_err("[adxl346] cmd magic type error\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) >= ADXL346_IOC_MAXNR) {
		pr_err("[adxl346] cmd number error\n");
		return -ENOTTY;
	}
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if (err) {
		pr_err("[adxl346] cmd access_ok error\n");
		return -EFAULT;
	}
	/* check adxl346_client */
	if ( adxl346_data->client == NULL) {
		pr_err("[adxl346] I2C driver not install (adxl346_ioctl)\n");
		return -EFAULT;
	}

	/* cmd mapping */
	switch(cmd)
	{
		case ADXL346_READ_ACCEL_XYZ :
			err = adxl346_read_accel_xyz((adxl346_acc_t*)data, adxl346_data);
			if (copy_to_user((adxl346_acc_t*)arg,(adxl346_acc_t*)data,6)!=0) {
				pr_err("[adxl346] adxl346_READ_ACCEL_XYZ: copy_to_user error\n");
				return -EFAULT;
			}
			return err;
		default :
			pr_err("[adxl346] ioctl cmd not found\n");
			return -EFAULT;
	}

	return 0;
}

static const struct file_operations adxl346_fops = {
	.owner = THIS_MODULE,
	.open = adxl346_open,
	.release = adxl346_close,
	.ioctl = adxl346_ioctl,
};

static struct miscdevice adxl346_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = ADXL346_DEVICE_NAME,
	.fops = &adxl346_fops,
};

static int adxl346_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res;

	pr_debug("%s ++ entering\n", __FUNCTION__);
	adxl346_data = kzalloc(sizeof(struct adxl346_data),GFP_KERNEL);
	if (NULL==adxl346_data) {
		res = -ENOMEM;
		goto out;
	}
	adxl_client = client;
	adxl346_data->client = client;
	/* check i2c functionality is workable */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[adxl346] i2c_check_functionality error!\n");
		res = -ENOTSUPP;
		goto out_free_mem;
	}
	strlcpy(client->name, ADXL346_I2C_NAME, I2C_NAME_SIZE);
	i2c_set_clientdata(client, adxl346_data);
	/* check bma150 chip is workable */

	res = i2c_smbus_read_word_data(adxl346_data->client, 0x00);
	if (res < 0) {
		pr_err("[adxl346] i2c_smbus_read_word_data(0x00) is failure! error code[%d]\n", res);
		goto out_free_mem;
	} else if ((res&0xFF) != 0xE6) {
		pr_err("[adxl346] adxl346 is not registered 0x%x!\n", res);
		res = -ENXIO;
		goto out_free_mem;
	}
	adxl346_Initial(adxl346_data);			/*ADXL346 Initialization*/
	pr_err("[adxl346] adxl346_init()done\n");

	/* register misc device */
	res = misc_register(&adxl346_dev);
	if (res < 0) {
		pr_err("[adxl346]: adxl346_dev register failed! error code:[%d]\n", res);
		goto out_unreg_irq;
	}
	pr_info("[adxl346] probe done\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;

out_unreg_irq:
	free_irq(client->irq, adxl346_data);
out_free_mem:
	kfree(adxl346_data);
out:
	pr_err("[adxl346] probe error\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return res;
}

static int adxl346_remove(struct i2c_client *client)
{
	misc_deregister(&adxl346_dev);
	free_irq(client->irq, adxl346_data);
	gpio_free(ADXL346_GPIO);
	kfree(adxl346_data);
	pr_info("[ADXL346] remove done\n");
	return 0;
}

#ifdef adxl_suspend
static int adxl346_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_debug("%s ++ entering\n", __FUNCTION__);
	disable_irq(client->irq);
	pr_debug("[ADXL346] low power suspend init done.\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;
}

static int adxl346_resume(struct i2c_client *client)
{
	pr_debug("%s ++ entering\n", __FUNCTION__);
	enable_irq(client->irq);
	pr_debug("[adxl346] normal resume init done.\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;
}
#else
static int adxl346_pm_suspend(struct device *dev)
{
	char g_buf[2] = { 0x2D, 0x00};
	/*set gyro into sleep mode*/
	char buf[2] = { 0x3E, 0x40 };
	struct i2c_msg msg[] = {
		{
			.addr = 0x69,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
	};
	i2c_transfer(adxl_client->adapter, msg, 1);
	/*set g-sensor into standby mode*/
	i2c_master_send(adxl_client, g_buf, 2);
	disable_irq(adxl_client->irq);
	pr_debug("%s ++ entering\n", __FUNCTION__);
	pr_debug("[ADXL346] low power suspend init done.\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;
}

static int adxl346_pm_resume(struct device *dev)
{
	char g_buf[2] = { 0x2D, 0x08};
	pr_debug("%s ++ entering\n", __FUNCTION__);
	i2c_master_send(adxl_client, g_buf, 2);
	enable_irq(adxl_client->irq);
	pr_debug("[adxl346] normal resume init done.\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;
}
#endif
static int __init adxl346_init(void)
{
	int res;
	pr_info("[adxl346] adxl346 init\n");
	res = i2c_add_driver(&adxl346_driver);
	if (res) {
		pr_err("[adxl346] %s: Driver Initialisation failed\n", __FILE__);
	}
	return res;
}

static void __exit adxl346_exit(void)
{
	i2c_del_driver(&adxl346_driver);
	pr_info("[adxl346]adxl346 exit\n");
}


MODULE_AUTHOR("Bin Du <bin.du@cn.bosch.com>");
MODULE_DESCRIPTION("ADXL346 driver");
MODULE_LICENSE("GPL");

module_init(adxl346_init);
module_exit(adxl346_exit);


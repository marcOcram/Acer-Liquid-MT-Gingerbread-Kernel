#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include "isl29018.h"

#define  BIT_16_RESOLUTION  65536
#define  BIT_12_RESOLUTION  4096
#define  BIT_08_RESOLUTION  256
#define  BIT_04_RESOLUTION  16

#define  RANGE1  1000
#define  RANGE2  4000
#define  RANGE3  16000
#define  RANGE4  64000

static struct i2c_client *private_isl29018_client;

struct isl29018_infomation {
	uint32_t resolution;
	uint32_t range;
	int operation_mode;
};

/* Data for I2C driver */
struct isl_data {
	struct i2c_client *client;
	struct work_struct work;
	wait_queue_head_t wait;
	struct input_dev *ps_input_dev;
	struct input_dev *ls_input_dev;
	struct isl29018_infomation info;
	bool ps_enabled;
	bool ls_enabled;

	struct work_struct sensor_work;
	wait_queue_head_t sensor_wait;
	bool timer_enabled;
	int delay_time;
};
static struct isl_data *isl_data;

#define	PS_POLLING_TIME	60
#if defined(CONFIG_MACH_ACER_A5)
#define	LS_POLLING_TIME	500
#define	FILTER_PS_TH	20
#define	DARK_LS_ADC_LEVEL	7
#define	LS_ADC_INTERVAL		4
#else
#define	LS_POLLING_TIME	200
#define	DARK_LS_ADC_LEVEL	5
#define	DARK_LS_ADC	1
#define	LS_ADC_INTERVAL		3
#endif
struct timer_list sensor_timer;

/*
 * client: target client
 * buf: target register
 * count: length of response
 */
static int i2c_read(struct i2c_client *client, char *buf, int count)
{
	if (1 != i2c_master_send(client, buf, 1)) {
		pr_err("[ISL] i2c_read --> Send reg. info error\n");
		return -1;
	}

	if (count != i2c_master_recv(client, buf, count)) {
		pr_err("[ISL] i2c_read --> get response error\n");
		return -1;
	}
	return 0;
}

/*
 * client: target client
 * buf: target register with command
 * count: length of transmitting
 */
static int i2c_write(struct i2c_client *client, char *buf, int count)
{
	if (count != i2c_master_send(client, buf, count)) {
		pr_err("[ISL] i2c_write --> Send reg. info error\n");
		return -1;
	}
	return 0;
}

static void sensor_timer_func(unsigned long unused)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);
	if (cdata->ps_enabled ) {
		cdata->delay_time = PS_POLLING_TIME;
	} else if (cdata-> ls_enabled) {
		cdata->delay_time = LS_POLLING_TIME;
	}
	schedule_work(&isl_data->sensor_work);
	mod_timer(&sensor_timer, jiffies + msecs_to_jiffies(cdata->delay_time));
}

static int isl_enable(void)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	int rc=0;
	pr_debug("%s\n", __func__);

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	if (!cdata->timer_enabled){
		setup_timer(&sensor_timer, sensor_timer_func, 0);
		mod_timer(&sensor_timer, jiffies + msecs_to_jiffies(5));
		cdata->timer_enabled=1;
	}
	return rc;
}

static int isl_power_down(struct i2c_client *client)
{
	int rs = 0;
	unsigned char reg_buf[2] = {0};

	reg_buf[0] = 0x00;  //Addr: 0x00
	reg_buf[1] = 0x00;
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL] i2c_write reg 0x00 fail in %s()\n", __func__);
	}
	return rs;
}

static int isl_disable(void)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	int rc = 0;
	pr_debug("%s\n", __func__);

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);
	if (cdata->ps_enabled == 0 && cdata->ls_enabled == 0){
		cdata->timer_enabled=0;
		cdata->delay_time = 0xFFFF;
		if (del_timer(&sensor_timer)){
			pr_err("del sensor timer OK\n");
		}
		isl_power_down(client);
	}
	return rc;
}

static int isl_ls_open(struct inode *inode, struct file *file)
{
	struct i2c_client *client = private_isl29018_client;
	if (client == NULL){
		pr_err("[ISL] I2C driver not install (isl_open)\n");
		return -1;
	}
	pr_debug("[ISL] has been opened\n");
	return 0;
}

static int isl_ls_close(struct inode *inode, struct file *file)
{
	pr_debug("[ISL] has been closed\n");
	return 0;
}

static int isl_ls_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	int err = 0;
	int isl_cmd = 0;
	int val = 0;

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	/* check cmd */
	if (_IOC_TYPE(cmd) != ISL29018_IOC_MAGIC) {
		pr_err("[ISL] cmd magic type error\n");
		return -ENOTTY;
	}
	isl_cmd = _IOC_NR(cmd);
	if (isl_cmd >= ISL29018_IOC_MAXNR) {
		pr_err("[ISL] cmd number error\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	}
	if (err) {
		pr_err("[ISL] cmd access_ok error\n");
		return -EFAULT;
	}
	if (client == NULL) {
		pr_err("[ISL] I2C driver not install (isl_ioctl)\n");
		return -EFAULT;
	}

	/* cmd mapping */
	switch (cmd) {
		case LIGHT_SENSOR_IOCTL_ENABLE:
			if (get_user(val, (unsigned long __user *)arg))
				return -EFAULT;
			if (val) {
				cdata->ls_enabled = 1;
				cdata->info.resolution = BIT_12_RESOLUTION;
				cdata->info.range = RANGE2;
				pr_err("LIGHT_SENSOR_IOCTL_ENABLE\n");
				return isl_enable();
			}
			else {
				cdata->ls_enabled = 0;
				pr_err("LIGHT_SENSOR_IOCTL_DISABLE\n");
				return isl_disable();
			}
			break;
		case LIGHT_SENSOR_IOCTL_GET_ENABLE:
			return put_user(cdata->ls_enabled, (unsigned long __user *)arg);
			break;
		default:
			pr_err("[ISL] ioctl cmd not found\n");
			return -EFAULT;
	}
	return 0;
}

static int isl_ps_open(struct inode *inode, struct file *file)
{
	struct i2c_client *client = private_isl29018_client;
	if (client == NULL){
		pr_err("[ISL] I2C driver not install (isl_open)\n");
		return -1;
	}
	pr_debug("[ISL] has been opened\n");
	return 0;
}

static int isl_ps_close(struct inode *inode, struct file *file)
{
	pr_debug("[ISL] has been closed\n");
	return 0;
}

static int isl_ps_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	int err = 0;
	int isl_cmd = 0;
	int val = 0;

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	/* check cmd */
	if (_IOC_TYPE(cmd) != ISL29018_IOC_MAGIC) {
		pr_err("[ISL] cmd magic type error\n");
		return -ENOTTY;
	}
	isl_cmd = _IOC_NR(cmd);
	if (isl_cmd >= ISL29018_IOC_MAXNR) {
		pr_err("[ISL] cmd number error\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	}
	if (err) {
		pr_err("[ISL] cmd access_ok error\n");
		return -EFAULT;
	}
	if (client == NULL) {
		pr_err("[ISL] I2C driver not install (isl_ioctl)\n");
		return -EFAULT;
	}

	/* cmd mapping */
	switch (cmd) {
		case PROXIMITY_SENSOR_IOCTL_ENABLE:
			if (get_user(val, (unsigned long __user *)arg))
				return -EFAULT;
			if (val) {
				cdata->ps_enabled=1;
				pr_err("PROXIMITY_SENSOR_IOCTL_ENABLE\n");
				return isl_enable();
			}
			else {
				cdata->ps_enabled=0;
				pr_err("PROXIMITY_SENSOR_IOCTL_DISABLE\n");
				return isl_disable();
			}
			break;
		case PROXIMITY_SENSOR_IOCTL_GET_ENABLE:
			return put_user(cdata->ps_enabled, (unsigned long __user *)arg);
			break;
		default:
			pr_err("[ISL] ioctl cmd not found\n");
			return -EFAULT;
	}
	return 0;
}

/* File operation of ISL device file for proximity sensor */
static const struct file_operations isl_ps_fops = {
	.owner		= THIS_MODULE,
	.open		= isl_ps_open,
	.release	= isl_ps_close,
	.ioctl		= isl_ps_ioctl,
};

static struct miscdevice isl_ps_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "proximity",
	.fops = &isl_ps_fops,
};

/* File operation of ISL device file for light sensor */
static const struct file_operations isl_ls_fops = {
	.owner		= THIS_MODULE,
	.open		= isl_ls_open,
	.release	= isl_ls_close,
	.ioctl		= isl_ls_ioctl,
};

static struct miscdevice isl_ls_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsensor",
	.fops = &isl_ls_fops,
};

int read_ls_adc(uint16_t *adc_value)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	unsigned char reg_buf[2] = {0};
	int rs=0;

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	reg_buf[0] = 0x01;  //Addr: 0x01
	reg_buf[1] = 0xB5; //because time, we use 12-bit rs
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_write reg 0x01 fail in %s\n",__func__);
	}

	reg_buf[0] = 0x00;  //Addr: 0x00
	reg_buf[1] = 0x20;
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_write reg 0x00 fail in %s\n",__func__);
	}
	msleep(10);

	reg_buf[0] = 0x02;  // Addr: 0x02, 0x03
	rs = i2c_read(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_read fail in isl_adc()\n");
		return 0;
	} else {
		*adc_value = (reg_buf[1] * 256) + reg_buf[0];
		return 1;
	}
}

int read_ps_adc(uint16_t *adc_value)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	unsigned char reg_buf[2] = {0};
	int rs=0;

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	reg_buf[0] = 0x01;  //Addr: 0x01
	reg_buf[1] = 0xB5; //bit 12 , range2:4000
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_write reg 0x08 fail in isl_complete_reset()\n");
		return 0;
	}
	reg_buf[0] = 0x00;  //Addr: 0x00
	reg_buf[1] = 0x60;
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_write reg 0x08 fail in isl_complete_reset()\n");
	}
	msleep(10);
	reg_buf[0] = 0x02;  // Addr: 0x02, 0x03
	rs = i2c_read(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_read fail in isl_adc()\n");
		return 0;
	} else {
		if ((reg_buf[1] >> 7) == 0) {
			*adc_value = (reg_buf[1] * 256) + reg_buf[0];
		}
		return 1 ;
	}
}

static ssize_t isl_ps_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	uint16_t adc_value = 0;
	int ret;
	read_ps_adc(&adc_value);
	ret = sprintf(buf, "Raw ps ADC[%d] == ADC[%d]\n", adc_value , adc_value*16);
	return ret;
}

static ssize_t isl_ls_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	uint16_t adc_value = 0;
	int ret = 0;
	read_ls_adc(&adc_value);
	ret = sprintf(buf, "Raw ls ADC[%d]\n", adc_value);
	return ret;
}

static int isl_complete_reset(void)
{
	struct i2c_client *client;
	int rs = 0;
	unsigned char reg_buf[2] = {0};

	client = private_isl29018_client;

	reg_buf[0] = 0x08;  //Addr: 0x08
	reg_buf[1] = 0x00;
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_write reg 0x08 fail in isl_complete_reset()\n");
		goto err_i2c_write;
	}

	reg_buf[0] = 0x00;  //Addr: 0x00
	reg_buf[1] = 0x00;
	rs = i2c_write(client, reg_buf, 2);
	if (rs == -1) {
		pr_err("[ISL]i2c_write reg 0x00 fail in isl_complete_reset()\n");
		goto err_i2c_write;
	}

err_i2c_write:
	return rs;
}

void report_ps_adc(void)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	int ret=0;
	uint16_t adc_value = 0;
#if defined(CONFIG_MACH_ACER_A5)
	static int com = 0;
#endif
	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);
	ret = read_ps_adc(&adc_value);
	if (ret) {
#if defined(CONFIG_MACH_ACER_A5)
		if (abs(adc_value - com) > FILTER_PS_TH) {
			com = adc_value;
			input_report_abs(cdata->ps_input_dev, ABS_DISTANCE, adc_value*16);
			input_sync(cdata->ps_input_dev);
		}
#else
		input_report_abs(cdata->ps_input_dev, ABS_DISTANCE, adc_value*16);
		input_sync(cdata->ps_input_dev);
#endif
	}
}

void report_ls_adc(void)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	int ret=0;
	uint16_t adc_value = 0;
	static int prev = 0;

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);
	ret = read_ls_adc(&adc_value);
	if (ret) {
		if (adc_value <= DARK_LS_ADC_LEVEL) {
#if defined(CONFIG_MACH_ACER_A5)
			adc_value = adc_value/2;
			prev = adc_value;
#else
			adc_value = DARK_LS_ADC;
#endif
			prev = adc_value;
		} else {
			if (abs(adc_value - prev) >= LS_ADC_INTERVAL) {
				prev = adc_value;
			} else {
				adc_value = prev;
			}
		}
		input_report_abs(cdata->ls_input_dev, ABS_MISC, adc_value);
		input_sync(cdata->ls_input_dev);
	}
}

static void sensor_work_func(struct work_struct *work)
{
	struct i2c_client *client;
	struct isl_data *cdata;

	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	if (cdata->ps_enabled) {
		report_ps_adc();
	}
	if (cdata->ls_enabled) {
		report_ls_adc();
	}
}

static ssize_t poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client;
	struct isl_data *cdata;
	client = private_isl29018_client;
	cdata = i2c_get_clientdata(client);

	return sprintf(buf, "delay_time = %d\n", cdata->delay_time);
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, NULL);

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       isl_ps_adc_show, NULL);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       isl_ls_adc_show, NULL);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static int isl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	pr_debug("%s ++ entering\n", __FUNCTION__);

	isl_data = kzalloc(sizeof(struct isl_data),GFP_KERNEL);
	if (NULL==isl_data) {
		res = -ENOMEM;
		goto out;
	}
	isl_data->client = client;
	isl_data->info.operation_mode = POWER_DOWN;
	private_isl29018_client = client;

	/* check i2c functionality is workable */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[ISL] i2c_check_functionality error!\n");
		res = -ENOTSUPP;
		goto out_free_mem;
	}
	strlcpy(client->name, ISL29018_DRIVER_NAME, I2C_NAME_SIZE);
	i2c_set_clientdata(client, isl_data);

	/* reset isl29011 chip */
	res = isl_complete_reset();
	if (res == -1) {
		pr_err("[ISL] reset isl29011 chip error!\n");
		goto out_free_mem;
	}
	/* Proximity sensor related */
	/* allocate input device */
	isl_data -> ps_input_dev = input_allocate_device();
	if (!isl_data -> ps_input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		res = -ENOMEM;
		goto out;
	}
	isl_data -> ps_input_dev -> name = "proximity";
	set_bit(EV_ABS, isl_data -> ps_input_dev -> evbit);
	input_set_abs_params(isl_data -> ps_input_dev, ABS_DISTANCE , 0, 65536 , 0, 0);

	/* register input device */
	res = input_register_device(isl_data -> ps_input_dev);
	if (res < 0) {
		pr_err("%s: could not register input device\n", __func__);
		goto out;
	}
	res = sysfs_create_group(&isl_data -> ps_input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (res) {
		pr_err("could not create ps sysfs group\n");
	}
	/* register misc device */
	res = misc_register(&isl_ps_dev);
	if (res < 0) {
		pr_err("[ISL]isl_dev register failed! error code:[%d]\n", res);
		goto out;
	}
	/* Proximity sensor related - end */

	/* Light sensor related */
	/* allocate input device */
	isl_data -> ls_input_dev = input_allocate_device();
	if (!isl_data -> ls_input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		res = -ENOMEM;
		goto out;
	}
	isl_data -> ls_input_dev -> name = "lightsensor-level";
	set_bit(EV_ABS, isl_data -> ls_input_dev -> evbit);
	input_set_abs_params(isl_data -> ls_input_dev, ABS_MISC, 0, 65535, 0, 0);

	/* register input device */
	res = input_register_device(isl_data -> ls_input_dev);
	if (res < 0) {
		pr_err("%s: could not register input device\n", __func__);
		goto out;
	}
	res = sysfs_create_group(&isl_data -> ls_input_dev ->dev.kobj,
				 &light_attribute_group);
	if (res) {
		pr_err("could not create ls sysfs group\n");
	}
	/* register misc device */
	res = misc_register(&isl_ls_dev);
	if (res < 0) {
		pr_err("[ISL]isl_dev register failed! error code:[%d]\n", res);
		goto out;
	}
	/* Light sensor related - end */

	INIT_WORK(&isl_data->sensor_work, sensor_work_func);
	init_waitqueue_head(&isl_data->sensor_wait);

	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;

out_free_mem:
	kfree(isl_data);
out:
	pr_err("[ISL] probe error\n");
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return res;
}

static int isl_remove(struct i2c_client *client)
{
	struct isl_data *tp = i2c_get_clientdata(client);

	misc_deregister(&isl_ps_dev);
	misc_deregister(&isl_ls_dev);
	kfree(tp);

	return 0;
}

static const struct i2c_device_id isl_id[] = {
	{ISL29018_DRIVER_NAME, 0},
	{ }
};

static int isl_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_debug("%s ++ entering\n", __FUNCTION__);
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;
}

static int isl_resume(struct i2c_client *client)
{
	pr_debug("%s ++ entering\n", __FUNCTION__);
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return 0;
}

/* new style I2C driver struct */
static struct i2c_driver isl_driver = {
	.probe = isl_probe,
	.remove = __devexit_p(isl_remove),
	.id_table = isl_id,
	.suspend = isl_suspend,
	.resume = isl_resume,
	.driver = {
		.name = ISL29018_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init isl_init(void)
{
	int res;
	pr_debug("%s ++ entering\n", __FUNCTION__);
	res = i2c_add_driver(&isl_driver);
	if (res) {
		pr_err("[ISL]%s: Driver Initialisation failed\n", __FILE__);
	}
	pr_debug("%s -- leaving\n", __FUNCTION__);
	return res;
}

static void __exit isl_exit(void)
{
	pr_debug("%s ++ entering\n", __FUNCTION__);
	i2c_del_driver(&isl_driver);
	pr_debug("%s -- leaving\n", __FUNCTION__);
}

module_init(isl_init);
module_exit(isl_exit);

MODULE_AUTHOR("Wei Liu <Wei_Liu@acer.com.tw>");
MODULE_DESCRIPTION("Intersil isl29011/18 P/L sensor driver");

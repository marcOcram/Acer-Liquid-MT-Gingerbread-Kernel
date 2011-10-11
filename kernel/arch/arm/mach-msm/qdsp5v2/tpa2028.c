/* arch/arm/mach-msm/qdsp5v2/tpa2028.c
 *
 * Copyright (C) 2010 acer Corporation.
 */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/earlysuspend.h>
#include <linux/mfd/msm-adie-codec.h>
#include <asm/uaccess.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/qdsp5v2/tpa2028.h>

#if 1
#define ACER_DBG(fmt, arg...) pr_debug(KERN_INFO "[TPA2028]: %s: " fmt "\n", __FUNCTION__, ## arg)
#else
#define ACER_DBG(fmt, arg...) do {} while (0)
#endif

#define TPA2028_DRIVER_NAME		"tpa2028"

/* Stream Type definition */
#define STREAM_VOICE_CALL		0
#define STREAM_SYSTEM			1
#define STREAM_RING				2
#define STREAM_MUSIC			3
#define STREAM_ALARM			4
#define STREAM_NOTIFICATION		5
#define STREAM_BLUETOOTH_SCO	6

static int __init tpa2028_init(void);
static int tpa2028_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpa2028_remove(struct i2c_client *client);
static int tpa2028_open(struct inode *inode, struct file *file);
static int tpa2028_close(struct inode *inode, struct file *file);
static int i2c_write(struct i2c_client *client, char *buf, int count);
static int i2c_read(struct i2c_client *client, char *buf, int count);
static void tpa2028_arg_init(void);
static int tpa2028_check_gpio_and_regvalue(void);
static int tpa2028_set_limitor(int type);
static int tpa2028_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int tpa2028_suspend(struct i2c_client *client, pm_message_t mesg);
static int tpa2028_resume(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void tpa2028_early_suspend(struct early_suspend *h);
static void tpa2028_early_resume(struct early_suspend *h);
#endif

static bool tpa_act_flag;

static const struct i2c_device_id tpa2028_id[] = {
	{TPA2028_DRIVER_NAME, 0},
	{}
};

static struct tpa2028_data {
	struct i2c_client *client;
	wait_queue_head_t wait;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
} tpa2028_data;

static const struct file_operations tpa2028_fops = {
	.owner		= THIS_MODULE,
	.open		= tpa2028_open,
	.release	= tpa2028_close,
	.ioctl		= tpa2028_ioctl,
};

static struct i2c_driver tpa2028_driver = {
	.probe		= tpa2028_probe,
	.remove		= tpa2028_remove,
	.id_table	= tpa2028_id,
	.suspend	= tpa2028_suspend,
	.resume		= tpa2028_resume,
	.driver	= {
	.name 	= TPA2028_DRIVER_NAME,
	},
};

static struct miscdevice tpa2028_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= TPA2028_DRIVER_NAME,
	.fops	= &tpa2028_fops,
};

static int i2c_read(struct i2c_client *client, char *buf, int count)
{
	if (1 != i2c_master_send(client, buf, 1)) {
		pr_debug("[TPA2028] i2c_read --> Send reg. info error\n");
		return -1;
	}

	if (count != i2c_master_recv(client, buf, count)) {
		pr_debug("[TPA2028] i2c_read --> get response error\n");
		return -1;
	}

	return 0;
}

static int i2c_write(struct i2c_client *client, char *buf, int count)
{
	if (count != i2c_master_send(client, buf, count)) {
		pr_debug("[TPA2028] i2c_write --> Send reg. info error\n");
		return -1;
	}

	return 0;
}

static void tpa2028_arg_init(void)
{
	int count;
	uint8_t tpa_rBuf[7] = {0};
	struct i2c_client *client = tpa2028_data.client;

	msleep(10);
	tpa2028_set_control(1, 1, 226);	//0xE2
	tpa2028_set_control(1, 2, 1);	//0x01
	tpa2028_set_control(1, 3, 11);	//0x0B
	tpa2028_set_control(1, 4, 0);	//0x00
	tpa2028_set_control(1, 5, 26);	//0x1A
	tpa2028_set_control(1, 6, 25);	//0x19
	tpa2028_set_control(1, 7, 128);	//0x80

	tpa_rBuf[0] = 1;
	i2c_read(client, tpa_rBuf, 7);

	for (count = 0; count < 7; count++) {
		pr_debug("init - reg[%d] = %d\n", count, tpa_rBuf[count]);
	}

	tpa2028_check_gpio_and_regvalue();
}

static int tpa2028_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;

	tpa2028_data.client = client;

	pr_debug("[TPA2028] tpa2028_probe start...\r\n");

	/* spk_amp_en - speaker amplifier enable*/
	res = gpio_request(GPIO_SPK_AMP, "TPA2028 AMP");
	if (res) {
		pr_err("GPIO request for SPK AMP EN failed!\n");
		goto gpio_err;
	}

	gpio_set_value(GPIO_SPK_AMP, 1);

	pr_debug("[TPA2028] Probe!!\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[TPA2028] i2c_check_functionality error!\n");
		return -ENOTSUPP;
	}

	strlcpy(client->name, TPA2028_DRIVER_NAME, I2C_NAME_SIZE);
	i2c_set_clientdata(client, &tpa2028_data);

#ifdef CONFIG_HAS_EARLYSUSPEND
	tpa2028_data.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	tpa2028_data.early_suspend.suspend = tpa2028_early_suspend;
	tpa2028_data.early_suspend.resume = tpa2028_early_resume;
	register_early_suspend(&tpa2028_data.early_suspend);
#endif

	res = misc_register(&tpa2028_dev);
	if (res) {
		pr_err("tpa2028_probe: tpa2028_dev register failed\n");
		goto error_tpa2028_dev;
	}

	tpa2028_arg_init();

	pr_debug("[TPA2028] probe done\n");

	return 0;

gpio_err:
	gpio_free(GPIO_SPK_AMP);

error_tpa2028_dev:
	pr_err("[TPA2028] probe: tpa2028_dev error\n");

	return res;
}

static int tpa2028_remove(struct i2c_client *client)
{
	pr_debug("remove tpa2028\n");
	gpio_free(GPIO_SPK_AMP);

	return 0;
}

static int tpa2028_open(struct inode *inode, struct file *file)
{
	pr_debug("[TPA2028] has been opened\n");

	return 0;
}

static int tpa2028_close(struct inode *inode, struct file *file)
{
	pr_debug("[TPA2028] has been closed\n");

	return 0;
}

static int tpa2028_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_debug("[TPA2028] low power suspend init done.\n");

	return 0;
}

static int tpa2028_resume(struct i2c_client *client)
{
	pr_debug("[TPA2028] normal resume init done.\n");

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tpa2028_early_suspend(struct early_suspend *h)
{
	pr_debug("tpa2028_early_suspend +++\n");
	pr_debug("[TPA2028] %s ++ entering\n", __FUNCTION__);
	pr_debug("[TPA2028] %s -- leaving\n", __FUNCTION__);
	pr_debug("tpa2028_early_suspend --- \n");
}

static void tpa2028_early_resume(struct early_suspend *h)
{
	pr_debug("tpa2028_early_resume +++ \n");
	pr_debug("[TPA2028] %s ++ entering\n", __FUNCTION__);
	pr_debug("[TPA2028] %s -- leaving\n", __FUNCTION__);
	pr_debug("tpa2028_early_resume --- \n");
}
#endif

static int tpa2028_check_gpio_and_regvalue(void)
{
	int count;
	uint8_t tpa_rBuf[7] = {0};
	struct i2c_client *client = tpa2028_data.client;

	tpa_rBuf[0] = 1;
	i2c_read(client, tpa_rBuf, 7);

	for (count = 0; count < 7; count++) {
		pr_debug("check - reg[%d] = %d\n", count, tpa_rBuf[count]);
	}

	if (gpio_get_value(GPIO_SPK_AMP) == 0) {
		pr_debug("GPIO_SPK_AMP isn't enable...\r\n");
		gpio_set_value(GPIO_SPK_AMP, 1);
		tpa2028_arg_init();
		return 0;
	}

	return 0;
}

static int tpa2028_set_limitor(int type)
{
	switch (type) {
	case STREAM_VOICE_CALL:
		return 0;

	case STREAM_SYSTEM:
		return 0;

	case STREAM_RING:
		return 0;

	case STREAM_MUSIC:
		return 0;

	case STREAM_ALARM:
		return 0;

	case STREAM_NOTIFICATION:
		return 0;

	default:
		return 0;
	}
}

int tpa2028_software_shutdown(int command)
{
	tpa2028_check_gpio_and_regvalue();

	if (command == 1) {
		pr_debug("tpa2028 software shutdown = true \n");
		tpa2028_set_control(1, 1, 226);	//0xE2
	} else {
		pr_debug("tpa2028 software shutdown = false \n");
		tpa2028_set_control(1, 2, 5);	//0x05
		tpa2028_set_control(1, 5, 6);	//0x06
		tpa2028_set_control(1, 6, 27);	//0x1B
		tpa2028_set_control(1, 7, 192);	//0xC0
		tpa2028_set_control(1, 1, 194);	//0xC2
	}

	return 0;
}

int tpa2028_speaker_dolby_switch(int command)
{
	if (command == 1) {
		pr_debug("tpa2051 speaker dolby switch = true \n");
		tpa2028_set_control(1, 2, 1);	//0x01
		tpa2028_set_control(1, 5, 26);	//0x1A
		tpa2028_set_control(1, 6, 25);	//0x19
		tpa2028_set_control(1, 7, 128);	//0x80
		tpa2028_set_control(1, 1, 194);	//0xC2
	} else {
		pr_debug("tpa2051 speaker dolby switch = false \n");
		tpa2028_set_control(1, 1, 226);	//0xE2
	}

	return 0;
}

int tpa2028_speaker_phone_switch(int command)
{
	if (command == 1) {
		pr_debug("tpa2051 speaker phone switch = true \n");
		tpa2028_set_control(1, 2, 5);	//0x05
		tpa2028_set_control(1, 5, 6);	//0x06
		tpa2028_set_control(1, 6, 58);	//0x3A
		tpa2028_set_control(1, 7, 128);	//0x80
		tpa2028_set_control(1, 1, 194);	//0xC2
	} else {
		pr_debug("tpa2051 speaker phone switch = false \n");
		tpa2028_set_control(1, 1, 226);	//0xE2
	}

	return 0;
}

int tpa2028_set_control(int commad, int regiter, int value)
{
	uint8_t tpa_wBuf[2];
	uint8_t tpa_rBuf[2];
	struct i2c_client *client = tpa2028_data.client;

	switch (commad) {
	case 1:
		tpa_wBuf[0] = regiter;
		tpa_wBuf[1] = value;
		i2c_write(client, tpa_wBuf, 2);
		pr_debug("[TPA2028] WRITE GAIN CONTROL \n");
		msleep(1);
		return 0;

	case 2:
		tpa_rBuf[0] = regiter;
		tpa_rBuf[1] = value;
		i2c_read(client, tpa_rBuf, 1);
		pr_debug("[TPA2028] READ GAIN CONTROL \n");
		msleep(1);
		return tpa_rBuf[0];

	default:
		pr_err("[TPA2028]: Command not found!\n");
		return -1;
	}
}

static int tpa2028_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	u32 uparam;

	struct i2c_client *client = tpa2028_data.client;

	pr_debug("[TPA2028] tpa2028 ioctl \n");
	if (_IOC_TYPE(cmd) != TPA2028_IOCTL_MAGIC) {
		pr_err("[TPA2028] IOCTL: cmd magic type error\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > IOC_MAXNR) {
		pr_err("[TPA2028] IOCTL: cmd number error\n");
		return -ENOTTY;
	}
	if (_IOC_DIR(cmd) & _IOC_NONE) {
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	}
	if (err) {
		pr_err("[TPA2028] IOCTL: cmd access right error\n");
		return -EFAULT;
	}
	if (client == NULL) {
		pr_err("[TPA2028] IOCTL: I2C driver not install (tpa2028_ioctl)\n");
		return -EFAULT;
	}

	switch(cmd) {
	case TPA2028_SET_FIXED_GAIN:
		if (copy_from_user(&uparam, (void *)arg, sizeof(uparam)))
			return -1;
		pr_debug("uparam = %d.\n", uparam);
		uparam *= 26;
		uparam /= 100;
		pr_debug("uparam/=100 = %d.\n", uparam);
		return 0;

	case TPA2028_SET_STREAM_TYPE:
		if (copy_from_user(&uparam, (void *)arg, sizeof(uparam)))
			return -1;
		pr_debug("Stream Type = %d.\n", uparam);
		tpa2028_set_limitor(uparam);
		return 0;

	case TPA2028_OPEN:
		tpa2028_software_shutdown(0);
		tpa_act_flag = true;
		return 0;

	case TPA2028_CLOSE:
		if (tpa_act_flag)
			tpa2028_software_shutdown(1);
		tpa_act_flag = false;
		return 0;

	case TPA2028_SET_ADIE_CODEC: {
		u8 reg;
		u8 value;
		struct tpa2028_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *)arg, sizeof(struct tpa2028_codec_info)))
			return -1;

		reg = 0xff & codec_info.reg;
		value = 0xff & codec_info.val;

		if (0 != acer_adie_codec_write(reg, 0xff, value))
			return -1;

		return 0;
	}

	case TPA2028_GET_ADIE_CODEC: {
		u8 reg;
		u8 value;
		struct tpa2028_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *)arg, sizeof(struct tpa2028_codec_info)))
			return -1;

		reg = 0xff & codec_info.reg;

		if (0 != acer_adie_codec_read(reg, &value))
			return -1;

		codec_info.reg = reg;
		codec_info.val = value;

		pr_debug("AUDIO_GET_AUDIO_CODEC : reg=%x, val=%x...\r\n", codec_info.reg, codec_info.val);

		if (copy_to_user((void __user *) arg, &codec_info, sizeof(struct tpa2028_codec_info)))
			return -1;

		return 0;
	}

	case TPA2028_SET_AMP_CODEC: {
		struct tpa2028_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *) arg, sizeof(struct tpa2028_codec_info)))
			return -1;

		tpa2028_set_control(1, codec_info.reg, codec_info.val);

		return 0;
	}

	case TPA2028_GET_AMP_CODEC: {
		u8 value;
		struct tpa2028_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *) arg, sizeof(struct tpa2028_codec_info)))
			return -1;

		value = tpa2028_set_control(2, codec_info.reg, codec_info.val);

		codec_info.val = value;

		pr_debug("TPA2028_GET_AMP_CODEC : reg=%x, val=%x...\r\n", codec_info.reg, codec_info.val);

		if (copy_to_user((void __user *) arg, &codec_info, sizeof(struct tpa2028_codec_info)))
			return -1;

		return 0;
	}

	default:
		pr_err("[TPA2028] IOCTL: Command not found!\n");
		return -1;
	}
}

static void __exit tpa2028_exit(void)
{
	i2c_del_driver(&tpa2028_driver);
	pr_info("[TPA2028] tpa2028 device exit ok!\n");
}

static int __init tpa2028_init(void)
{
	int res = 0;

	res = i2c_add_driver(&tpa2028_driver);
	if (res) {
		pr_err("[TPA2028]i2c_add_driver failed! \n");
		return res;
	}

	pr_info("[TPA2028] tpa2028 device init ok!\n");
	return 0;
}

module_init(tpa2028_init);
module_exit(tpa2028_exit);

MODULE_AUTHOR("Eric Cheng <Eric_Cheng@acer.com.tw>");
MODULE_DESCRIPTION("TPA2028 driver");

/* arch/arm/mach-msm/qdsp5v2/tpa2051.c
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
#include <mach/qdsp5v2/tpa2051.h>

#if 1
#define ACER_DBG(fmt, arg...) pr_debug(KERN_INFO "[TPA2051]: %s: " fmt "\n", __FUNCTION__, ## arg)
#else
#define ACER_DBG(fmt, arg...) do {} while (0)
#endif

#define TPA2051_DRIVER_NAME "tpa2051"

/* Stream Type definition */
#define STREAM_VOICE_CALL		0
#define STREAM_SYSTEM			1
#define STREAM_RING				2
#define STREAM_MUSIC			3
#define STREAM_ALARM			4
#define STREAM_NOTIFICATION	5
#define STREAM_BLUETOOTH_SCO	6

static int __init tpa2051_init(void);
static int tpa2051_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpa2051_remove(struct i2c_client *client);
static int tpa2051_open(struct inode *inode, struct file *file);
static int tpa2051_close(struct inode *inode, struct file *file);
static int i2c_write(struct i2c_client *client, char *buf, int count);
static int i2c_read(struct i2c_client *client, char *buf, int count);
static void tpa2051_arg_init(void);
static int tpa2051_check_gpio_and_regvalue(void);
static int tpa2051_set_limitor(int type);
static int tpa2051_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int tpa2051_suspend(struct i2c_client *client, pm_message_t mesg);
static int tpa2051_resume(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void tpa2051_early_suspend(struct early_suspend *h);
static void tpa2051_early_resume(struct early_suspend *h);
#endif

// Fix the pop noise of system open sound.
static void enable_speaker_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(en_speaker_wq, enable_speaker_work);

static bool tpa_act_flag;

// Fix the pop noise of system open sound.
static bool bootsound;

static const struct i2c_device_id tpa2051_id[] = {
	{TPA2051_DRIVER_NAME, 0},
	{}
};

static struct tpa2051_data {
	struct i2c_client *client;
	wait_queue_head_t wait;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
} tpa2051_data;

static const struct file_operations tpa2051_fops = {
	.owner		= THIS_MODULE,
	.open		= tpa2051_open,
	.release	= tpa2051_close,
	.ioctl		= tpa2051_ioctl,
};

static struct i2c_driver tpa2051_driver = {
	.probe		= tpa2051_probe,
	.remove		= tpa2051_remove,
	.id_table	= tpa2051_id,
	.suspend	= tpa2051_suspend,
	.resume		= tpa2051_resume,
	.driver	= {
	.name 	= TPA2051_DRIVER_NAME,
	},
};

static struct miscdevice tpa2051_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= TPA2051_DRIVER_NAME,
	.fops	= &tpa2051_fops,
};

static int i2c_read(struct i2c_client *client, char *buf, int count)
{
	if (1 != i2c_master_send(client, buf, 1)) {
		pr_debug("[TPA2051] i2c_read --> Send reg. info error\n");
		return -1;
	}

	if (count != i2c_master_recv(client, buf, count)) {
		pr_debug("[TPA2051] i2c_read --> get response error\n");
		return -1;
	}

	return 0;
}

static int i2c_write(struct i2c_client *client, char *buf, int count)
{
	if (count != i2c_master_send(client, buf, count)) {
		pr_debug("[TPA2051] i2c_write --> Send reg. info error\n");
		return -1;
	}

	return 0;
}

static void tpa2051_arg_init(void)
{
	int count;
	uint8_t tpa_rBuf[7] = {0};
	struct i2c_client *client = tpa2051_data.client;

	msleep(10);
	tpa2051_set_control(1, 0, 0);
	tpa2051_set_control(1, 1, 16);//0x10
	tpa2051_set_control(1, 2, 33);//0x21
	tpa2051_set_control(1, 3, 84);//0x54
	tpa2051_set_control(1, 4, 10);//0x0A
	tpa2051_set_control(1, 5, 141);//0x8D
	tpa2051_set_control(1, 6, 173);//0xAD

	tpa_rBuf[0] = 1;
	i2c_read(client, tpa_rBuf, 7);

	for (count = 0; count < 7; count++) {
		pr_debug("init - reg[%d] = %d\n", count, tpa_rBuf[count]);
	}

	tpa2051_check_gpio_and_regvalue();
}

static int tpa2051_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;

	tpa2051_data.client = client;

	pr_debug("[TPA2051] tpa2051_probe start...\r\n");

	/* spk_amp_en - speaker amplifier enable*/
	res = gpio_request(GPIO_SPK_AMP, "TPA2051 AMP");
	if (res) {
		pr_err("GPIO request for SPK AMP EN failed!\n");
		goto gpio_err;
	}

	gpio_set_value(GPIO_SPK_AMP, 1);

	pr_debug("[TPA2051] Probe!!\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[TPA2051] i2c_check_functionality error!\n");
		return -ENOTSUPP;
	}

	strlcpy(client->name, TPA2051_DRIVER_NAME, I2C_NAME_SIZE);
	i2c_set_clientdata(client, &tpa2051_data);


#ifdef CONFIG_HAS_EARLYSUSPEND
	tpa2051_data.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	tpa2051_data.early_suspend.suspend = tpa2051_early_suspend;
	tpa2051_data.early_suspend.resume = tpa2051_early_resume;
	register_early_suspend(&tpa2051_data.early_suspend);
#endif

	res = misc_register(&tpa2051_dev);
	if (res) {
		pr_err("tpa2051_probe: tpa2051_dev register failed\n");
		goto error_tpa2051_dev;
	}

	tpa2051_arg_init();

	// Fix the pop noise of system open sound.
	bootsound = true;

	pr_debug("[TPA2051] probe done\n");

	return 0;

gpio_err:
	gpio_free(GPIO_SPK_AMP);

error_tpa2051_dev:
	pr_err("[TPA2051] probe: tpa2051_dev error\n");

	return res;
}

static int tpa2051_remove(struct i2c_client *client)
{
	pr_debug("remove tpa2051\n");
	gpio_free(GPIO_SPK_AMP);

	return 0;
}

static int tpa2051_open(struct inode *inode, struct file *file)
{
	pr_debug("[TPA2051] has been opened\n");

	return 0;
}

static int tpa2051_close(struct inode *inode, struct file *file)
{
	pr_debug("[TPA2051] has been closed\n");

	return 0;
}

static int tpa2051_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_debug("[TPA2051] low power suspend init done.\n");

	return 0;
}

static int tpa2051_resume(struct i2c_client *client)
{
	pr_debug("[TPA2051] normal resume init done.\n");

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tpa2051_early_suspend(struct early_suspend *h)
{
	pr_debug("tpa2051_early_suspend +++\n");
	pr_debug("[TPA2051] %s ++ entering\n", __FUNCTION__);
	pr_debug("[TPA2051] %s -- leaving\n", __FUNCTION__);
	pr_debug("tpa2051_early_suspend --- \n");
}

static void tpa2051_early_resume(struct early_suspend *h)
{
	pr_debug("tpa2051_early_resume +++ \n");
	pr_debug("[TPA2051] %s ++ entering\n", __FUNCTION__);
	pr_debug("[TPA2051] %s -- leaving\n", __FUNCTION__);
	pr_debug("tpa2051_early_resume --- \n");
}
#endif

static int tpa2051_check_gpio_and_regvalue(void)
{
	int count;
	uint8_t tpa_rBuf[7] = {0};
	struct i2c_client *client = tpa2051_data.client;

	tpa_rBuf[0] = 1;
	i2c_read(client, tpa_rBuf, 7);

	for (count = 0; count < 7; count++) {
		pr_debug("check - reg[%d] = %d\n", count, tpa_rBuf[count]);
	}

	if (gpio_get_value(GPIO_SPK_AMP) == 0) {
		pr_debug("GPIO_SPK_AMP isn't enable...\r\n");
		gpio_set_value(GPIO_SPK_AMP, 1);
		tpa2051_arg_init();
		return 0;
	}

	return 0;
}

static int tpa2051_set_limitor(int type)
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

// Fix the pop noise of system open sound.
static void enable_speaker_work(struct work_struct *work)
{
	tpa2051_set_control(1, 5, 205);//0xCD
	bootsound = false;
}

int tpa2051_software_shutdown(int command)
{
	tpa2051_check_gpio_and_regvalue();

	if (command == 1) {
		pr_debug("tpa2051 software shutdown = true \n");
		tpa2051_set_control(1, 1, 16);//0x10
	} else {
		pr_debug("tpa2051 software shutdown = false \n");
		tpa2051_set_control(1, 1, 194);//0xC2
		tpa2051_set_control(1, 2, 33);//0x21
		tpa2051_set_control(1, 4, 21);//0x15
	}

	return 0;
}

int tpa2051_speaker_dolby_switch(int command)
{
	if (command == 1) {
		pr_debug("tpa2051 speaker dolby switch = true \n");
		tpa2051_set_control(1, 1, 194);//0xC2
		tpa2051_set_control(1, 2, 33);//0x21
		tpa2051_set_control(1, 4, 18);//0x12
	} else {
		pr_debug("tpa2051 speaker dolby switch = false \n");
		tpa2051_set_control(1, 1, 16);//0x10
	}

	return 0;
}

int tpa2051_speaker_phone_switch(int command)
{
	if (command == 1) {
		pr_debug("tpa2051 speaker phone switch = true \n");
		tpa2051_set_control(1, 1, 194);//0xC2
		tpa2051_set_control(1, 2, 33);//0x21
		tpa2051_set_control(1, 4, 10);//0x0A
	} else {
		pr_debug("tpa2051 speaker phone switch = false \n");
		tpa2051_set_control(1, 1, 16);//0x10
	}

	return 0;
}

int tpa2051_headset_switch(int command)
{
	if (command == 1) {
		pr_debug("tpa2051_headset_switch = true \n");
		tpa2051_set_control(1, 1, 12);//0xC
		tpa2051_set_control(1, 2, 33);//0x21
		tpa2051_set_control(1, 4, 109);//0x6D
		tpa2051_set_control(1, 5, 83);//0x53
	} else {
		pr_debug("tpa2051_headset_switch = false \n");
		tpa2051_set_control(1, 1, 16);//0x10
		tpa2051_set_control(1, 4, 10);//0x0A
		tpa2051_set_control(1, 5, 141);//0x8D
	}

	return 0;
}

int tpa2051_headset_speaker_switch(int command)
{
	if (command == 1) {
		pr_debug("tpa2051_headset_speaker_switch = true \n");
		if (bootsound) {
			// Fix the pop noise of system open sound.
			tpa2051_set_control(1, 5, 13);//0x0D
			tpa2051_set_control(1, 1, 206);//0xCE
			tpa2051_set_control(1, 2, 33);//0x21
			tpa2051_set_control(1, 4, 109);//0x6D
			schedule_delayed_work(&en_speaker_wq, 10);
		} else {
			tpa2051_set_control(1, 1, 206);//0xCE
			tpa2051_set_control(1, 2, 33);//0x21
			tpa2051_set_control(1, 4, 109);//0x6D
			tpa2051_set_control(1, 5, 205);//0xCD
		}
	} else {
       pr_debug("tpa2051_headset_speaker_switch = false \n");
		tpa2051_set_control(1, 1, 16);//0x10
		tpa2051_set_control(1, 4, 10);//0x0A
		tpa2051_set_control(1, 5, 141);//0x8D
	}

	return 0;
}

int tpa2051_set_control(int commad, int regiter, int value)
{
	uint8_t tpa_wBuf[2];
	uint8_t tpa_rBuf[2];

	struct i2c_client *client = tpa2051_data.client;

	switch (commad) {
	case 1:
		tpa_wBuf[0] = regiter;
		tpa_wBuf[1] = value;
		i2c_write(client, tpa_wBuf, 2);
		pr_debug("[TPA2051] WRITE GAIN CONTROL \n");
		msleep(1);
		return 0;

	case 2:
		tpa_rBuf[0] = regiter;
		tpa_rBuf[1] = value;
		i2c_read(client, tpa_rBuf, 1);
		pr_debug("[TPA2051] READ GAIN CONTROL \n");
		msleep(1);
		return tpa_rBuf[0];

	default:
		pr_err("[TPA2051]: Command not found!\n");
		return -1;
	}
}

static int tpa2051_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	u32 uparam;

	struct i2c_client *client = tpa2051_data.client;

	pr_debug("[TPA2051] tpa2051 ioctl \n");
	if (_IOC_TYPE(cmd) != TPA2051_IOCTL_MAGIC) {
		pr_err("[TPA2051] IOCTL: cmd magic type error\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > IOC_MAXNR) {
		pr_err("[TPA2051] IOCTL: cmd number error\n");
		return -ENOTTY;
	}
	if (_IOC_DIR(cmd) & _IOC_NONE) {
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	}
	if (err) {
		pr_err("[TPA2051] IOCTL: cmd access right error\n");
		return -EFAULT;
	}
	if (client == NULL) {
		pr_err("[TPA2051] IOCTL: I2C driver not install (tpa2051_ioctl)\n");
		return -EFAULT;
	}

	switch(cmd) {
	case TPA2051_SET_FIXED_GAIN:
		if (copy_from_user(&uparam, (void *)arg, sizeof(uparam)))
			return -1;
		pr_debug("uparam = %d.\n", uparam);
		uparam *= 26;
		uparam /= 100;
		pr_debug("uparam/=100 = %d.\n", uparam);
		return 0;

	case TPA2051_SET_STREAM_TYPE:
		if (copy_from_user(&uparam, (void *)arg, sizeof(uparam)))
			return -1;
		pr_debug("Stream Type = %d.\n", uparam);
		tpa2051_set_limitor(uparam);
		return 0;

	case TPA2051_OPEN:
		tpa2051_software_shutdown(0);
		tpa_act_flag = true;
		return 0;

	case TPA2051_CLOSE:
		if (tpa_act_flag)
			tpa2051_software_shutdown(1);
		tpa_act_flag = false;
		return 0;

	case TPA2051_SET_ADIE_CODEC: {
		u8 reg;
		u8 value;
		struct tpa2051_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *)arg, sizeof(struct tpa2051_codec_info)))
			return -1;

		reg = 0xff & codec_info.reg;
		value = 0xff & codec_info.val;

		if (0 != acer_adie_codec_write(reg, 0xff, value))
			return -1;

		return 0;
	}

	case TPA2051_GET_ADIE_CODEC: {
		u8 reg;
		u8 value;
		struct tpa2051_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *)arg, sizeof(struct tpa2051_codec_info)))
			return -1;

		reg = 0xff & codec_info.reg;

		if (0 != acer_adie_codec_read(reg, &value))
			return -1;

		codec_info.reg = reg;
		codec_info.val = value;

		pr_debug("AUDIO_GET_AUDIO_CODEC : reg=%x, val=%x...\r\n", codec_info.reg, codec_info.val);

		if (copy_to_user((void __user *) arg, &codec_info, sizeof(struct tpa2051_codec_info)))
			return -1;

		return 0;
	}

	case TPA2051_SET_AMP_CODEC: {
		struct tpa2051_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *) arg, sizeof(struct tpa2051_codec_info)))
			return -1;

		tpa2051_set_control(1, codec_info.reg, codec_info.val);

		return 0;
	}

	case TPA2051_GET_AMP_CODEC: {
		u8 value;
		struct tpa2051_codec_info codec_info;

		if (copy_from_user(&codec_info, (void __user *) arg, sizeof(struct tpa2051_codec_info)))
			return -1;

		value = tpa2051_set_control(2, codec_info.reg, codec_info.val);

		codec_info.val = value;

		pr_debug("TPA2051_GET_AMP_CODEC : reg=%x, val=%x...\r\n", codec_info.reg, codec_info.val);

		if (copy_to_user((void __user *) arg, &codec_info, sizeof(struct tpa2051_codec_info)))
			return -1;

		return 0;
	}

	default:
		pr_err("[TPA2051] IOCTL: Command not found!\n");
		return -1;
	}
}

static void __exit tpa2051_exit(void)
{
	i2c_del_driver(&tpa2051_driver);
	pr_info("[TPA2051] tpa2051 device exit ok!\n");
}

static int __init tpa2051_init(void)
{
	int res = 0;

	res = i2c_add_driver(&tpa2051_driver);
	if (res) {
		pr_err("[TPA2051]i2c_add_driver failed! \n");
		return res;
	}

	pr_info("[TPA2051] tpa2051 device init ok!\n");
	return 0;
}

module_init(tpa2051_init);
module_exit(tpa2051_exit);

MODULE_AUTHOR("Eric Cheng <Eric_Cheng@acer.com.tw>");
MODULE_DESCRIPTION("TPA2051 driver");

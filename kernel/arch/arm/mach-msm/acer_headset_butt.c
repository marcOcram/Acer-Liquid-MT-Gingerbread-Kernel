/*
 * Acer Headset device button driver.
 *
 *
 * Copyright (C) 2010 acer Corporation.
 *
 * Authors:
 *    Eric Cheng <Eric_Cheng@acer.com.tw>
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
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <mach/gpio.h>
#include <mach/acer_headset_butt.h>
#include <mach/acer_headset.h>

#define HS_BUTT_DRIVER_NAME		"acer-hs-butt"
#if defined(CONFIG_MACH_ACER_A5)
	#define DEBOUNCE_TIME			50000000 /* 50 ms */
#elif defined(CONFIG_MACH_ACER_A4)
	#define DEBOUNCE_TIME			300000000 /* 300 ms */
#endif

#define DEV_IOCTLID				0x30
#define IOC_MAXNR				2
#define IOCTL_GET_ADC_ON		_IOW(DEV_IOCTLID, 1, int)

static int __init hs_butt_init(void);
static int hs_butt_probe(struct platform_device *pdev);
static int hs_butt_remove(struct platform_device *pdev);
static int hs_butt_open(struct inode *inode, struct file *file);
static int hs_butt_close(struct inode *inode, struct file *file);
static int hs_butt_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static irqreturn_t hs_butt_interrupt(int irq, void *dev_id);
static int __init hs_butt_register_input(struct input_dev *input);
/* Add a work queue !! */
static struct work_struct hsmic_wq;
static int rpc_call_func(void);

/* Add hs state */
static bool hs_state;
static bool hs_type_state;

struct hs_butt_data {
	struct switch_dev sdev;
	struct input_dev *input;
	struct hrtimer btn_timer;
	ktime_t btn_debounce_time;
	unsigned int butt;
	unsigned int dett;
	unsigned int mic;
	unsigned int irq;
};

static struct hs_butt_data *hr;
struct work_struct work;
static struct msm_rpc_endpoint *rpc_endpoint = NULL;

static struct platform_driver hs_butt_driver = {
	.probe		= hs_butt_probe,
	.remove		= hs_butt_remove,
	.driver		= {
		.name	= HS_BUTT_DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

/*File operation of HS-BUTT device file */
static const struct file_operations hs_butt_fops = {
	.owner	= THIS_MODULE,
	.open	= hs_butt_open,
	.release	= hs_butt_close,
	.ioctl		= hs_butt_ioctl,
};

static struct miscdevice hs_butt_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= HS_BUTT_DRIVER_NAME,
	.fops	= &hs_butt_fops,
};

static enum hrtimer_restart button_event_timer_func(struct hrtimer *data)
{
	/* Prevent to trigger the Music_AP after hanging up*/
	if (gpio_get_value(hr->dett) == 1)
		return HRTIMER_NORESTART;

	schedule_work(&work);

	return HRTIMER_NORESTART;
}

/* For Headset type detect  +++*/
void set_hs_type_state(bool state)
{
	hs_type_state = state;
}

bool get_hs_type_state(void)
{
	return hs_type_state;
}

void set_hs_state(bool state)
{
	hs_state = state;
}

static void acer_update_mic_work(struct work_struct *work)
{
	hs_type_state = true;
}
/* For Headset type detect ---*/

static void rpc_call_work(struct work_struct *work)
{
#if defined(CONFIG_MACH_ACER_A5)
	int adc_reply = -1;
	int count = 0;
	int key_code = 0;
#endif

	/* Prevent to trigger the Music_AP after hanging up*/
	if (hs_state) {
		if (gpio_get_value(hr->butt)) {
			pr_debug("ACER Headset Button is press...\n");
#if defined(CONFIG_MACH_ACER_A4)
			input_report_key(hr->input, KEY_MEDIA, 1);
			input_sync(hr->input);
			input_report_key(hr->input, KEY_MEDIA, 0);
			input_sync(hr->input);
#elif defined(CONFIG_MACH_ACER_A5)
			while (count < 3) {
				adc_reply = rpc_call_func();
				pr_debug("ACER Headset Button : rpc = %d...\n", adc_reply);
				if (adc_reply > 19 && adc_reply < 34) {
					// ADC Range: 20 ~ 33
					key_code = KEY_MEDIA;
					break;
				} else if (adc_reply > 203 && adc_reply < 218) {
					// ADC Range: 204 ~ 217
					key_code = KEY_VOLUMEUP;
					break;
				} else if (adc_reply > 403 && adc_reply < 418) {
					// ADC Range: 404 ~ 417
					key_code = KEY_VOLUMEDOWN;
					break;
				} else {
					key_code = 0;
				}

				mdelay(1);
				count++;
			}

			if (key_code == KEY_MEDIA) {
				input_report_key(hr->input, key_code, 1);
				input_sync(hr->input);
				input_report_key(hr->input, key_code, 0);
				input_sync(hr->input);
			} else {
				input_report_key(hr->input, key_code, 1);
				while(1) {
					mdelay(10);
					adc_reply = rpc_call_func();
					if (adc_reply > 420)
						break;
				}
				input_report_key(hr->input, key_code, 0);
				input_sync(hr->input);
			}
#endif
		}
	}
}

static int __init hs_butt_init(void)
{
	int ret;

	ret = platform_driver_register(&hs_butt_driver);
	if (ret)
		pr_err("[HS-BUTT] hs_butt_init failed! \n");

	pr_debug("[HS-BUTT] hs_butt_init done ...\n");

	return ret;
}

static int hs_butt_probe(struct platform_device *pdev)
{
	int err = 0;
	int ret = 0;

	struct hs_butt_gpio *pdata = pdev->dev.platform_data;

	hr = kzalloc(sizeof(struct hs_butt_data), GFP_KERNEL);
	if (!hr)
		return -ENOMEM;

	hr->btn_debounce_time = ktime_set(0, DEBOUNCE_TIME);

	/* init work queue*/
	INIT_WORK(&hsmic_wq, acer_update_mic_work);
	hr->sdev.name = HS_BUTT_DRIVER_NAME;
	ret = switch_dev_register(&hr->sdev);
	if (ret < 0) {
		pr_err("switch_dev fail!\n");
		goto err_switch_dev_register;
	} else {
		pr_debug("### hs_butt_switch_dev success register ###\n");
	}

	hr->butt = pdata->gpio_hs_butt;
	hr->dett = pdata->gpio_hs_dett;

	ret = gpio_request(hr->butt, "hs_butt");
	if (ret < 0) {
		pr_err("err_request_butt_gpio fail!\n");
		goto err_request_butt_gpio;
	}

	hr->irq = gpio_to_irq(hr->butt);
	if (hr->irq < 0) {
		ret = hr->irq;
		pr_err("err_get_hs_butt_irq_num fail!\n");
		goto err_get_hs_butt_irq_num_failed;
	}

	INIT_WORK(&work, rpc_call_work);
	hrtimer_init(&hr->btn_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr->btn_timer.function = button_event_timer_func;

	ret = request_irq(hr->irq, hs_butt_interrupt, IRQF_TRIGGER_RISING, "hs_butt", NULL);
	if (ret < 0) {
		pr_err("err_request_butt_irq fail!\n");
		goto err_request_butt_irq;
	} else {
		pr_debug("[HS-BUTT] IRQ_%d already request_butt_irq in use\n", hr->irq);
	}

	ret = set_irq_wake(hr->irq, 1);
	if (ret < 0) {
		pr_err("err_request_butt_irq fail!\n");
		goto err_request_butt_irq;
	}

	/* input register */
	hr->input = input_allocate_device();
	if (hr->input == NULL) {
		pr_err("[HS-BUTT] input_allocate_device error!\n");
		return -ENOMEM;
	}

	err = hs_butt_register_input(hr->input);
	if (err < 0) {
		pr_err("[HS-BUTT] register_input error\n");
		goto err_register_input_dev;
	}

	err = misc_register(&hs_butt_dev);
	if (err) {
		pr_err("[HS-BUTT] hs_butt_dev register failed\n");
		goto err_request_butt_gpio;
	}

#if defined(CONFIG_MACH_ACER_A5)
	// rpc endpoint initialize
	if (!rpc_endpoint) {
		rpc_endpoint = msm_rpc_connect(ADC_RPC_PROG, ADC_RPC_VERS, 0);
		if (IS_ERR(rpc_endpoint)) {
			pr_err("[HS-BUTT] RPC Server Connect fail!\n");
			rpc_endpoint = NULL;
			return -ENOMEM;
		}
	}
#endif

	pr_info("[HS-BUTT] Probe done\n");

	return 0;

err_register_input_dev:
	input_free_device(hr->input);
err_request_butt_irq:
	free_irq(hr->irq, 0);
err_get_hs_butt_irq_num_failed:
err_request_butt_gpio:
	gpio_free(hr->butt);
err_switch_dev_register:
	pr_err("[HS-BUTT] Probe error\n");

	return ret;
}

static int hs_butt_remove(struct platform_device *pdev)
{
	input_unregister_device(hr->input);
	free_irq(hr->irq, 0);
	gpio_free(hr->butt);
	switch_dev_unregister(&hr->sdev);

	return 0;
}

static irqreturn_t hs_butt_interrupt(int irq, void *dev_id)
{
	schedule_work(&hsmic_wq);

	if (hs_state) {
		pr_debug("hs_butt_interrupt hrtimer_start...\r\n");
		hrtimer_start(&hr->btn_timer, hr->btn_debounce_time, HRTIMER_MODE_REL);
	}

	return IRQ_HANDLED;
}

static int __init hs_butt_register_input(struct input_dev *input)
{
	input->name = HS_BUTT_DRIVER_NAME;
	input->evbit[0] = BIT_MASK(EV_SYN)|BIT_MASK(EV_KEY);
	input->keybit[BIT_WORD(KEY_MEDIA)] |= BIT_MASK(KEY_MEDIA);
#if defined(CONFIG_MACH_ACER_A5)
	input->keybit[BIT_WORD(KEY_VOLUMEUP)] |= BIT_MASK(KEY_VOLUMEUP);
	input->keybit[BIT_WORD(KEY_VOLUMEDOWN)] |= BIT_MASK(KEY_VOLUMEDOWN);
#endif
	return input_register_device(input);
}

/* open command for HS-BUTT device file	*/
static int hs_butt_open(struct inode *inode, struct file *file)
{
	return 0;
}

/* close command for HS-BUTT device file */
static int hs_butt_close(struct inode *inode, struct file *file)
{
	return 0;
}

static int hs_butt_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	/* check cmd */
	if (_IOC_TYPE(cmd) != DEV_IOCTLID) {
		pr_err("cmd magic type error\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > IOC_MAXNR) {
		pr_err("cmd number error\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if (err) {
		pr_err("cmd access_ok error\n");
		return -EFAULT;
	}

	/* cmd mapping */
	switch (cmd) {
	case IOCTL_GET_ADC_ON:
		if (!(gpio_get_value(hr->dett)))
			return rpc_call_func();
		else
			return -1;
		break;
	default:
		return -1;
	}

	return 0;
}

static int rpc_call_func(void)
{
	int rc;
	int adc_reply = 0;
	adc_req	req_packet;
	adc_rep	rep_packet;

	memset(&req_packet, ADC_RPC_HS_BUTT, sizeof(req_packet));

	rc = msm_rpc_call_reply(rpc_endpoint, ADC_PROC,
							&req_packet, sizeof(req_packet), &rep_packet, sizeof(rep_packet), -1);

	if (rc < 0) {
		pr_err("[HS-BUTT] rpc_call error! rc= %d, reply=%d\n", rc, be32_to_cpu(rep_packet.reply));
		return -1;
	}

	adc_reply = be32_to_cpu(rep_packet.reply);

	return adc_reply;
}

static void __exit hs_butt_exit(void)
{
	platform_driver_unregister(&hs_butt_driver);
}

module_init(hs_butt_init);
module_exit(hs_butt_exit);

MODULE_AUTHOR("Eric Cheng <Eric_Cheng@acer.com.tw>");
MODULE_DESCRIPTION("Acer Headset Button");
MODULE_LICENSE("GPL");

/* drivers/leds/leds-msp430.c - control leds through mcu
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/msp430.h>
#include <linux/leds-msp430.h>


static void led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	struct led_mcu_data *ldata;
	int on_off;

	ldata = container_of(led_cdev, struct led_mcu_data, ldev);
	if (brightness == LED_OFF)
		on_off = MCU_LED_OFF;
	else
		on_off = MCU_LED_ON;
	led_control(ldata->control_flag, on_off);
	ldata->brightness = brightness;
}

static enum led_brightness led_brightness_get(struct led_classdev *led_cdev)
{
	struct led_mcu_data *ldata;

	ldata = container_of(led_cdev, struct led_mcu_data, ldev);
	return ldata->brightness;
}

static int mcu_led_probe(struct platform_device *pdev)
{
	struct leds_msp430_platform_data *pdata = pdev->dev.platform_data;
	struct led_mcu_data *led;
	int i, ret;

	for (i = 0; i < pdata->num_leds; i++) {
		led = &pdata->leds[i];
		led->ldev.brightness_set = led_brightness_set;
		led->ldev.brightness_get = led_brightness_get;
		led->ldev.brightness	= 0;
		ret = led_classdev_register(&pdev->dev, &led->ldev);
		if (ret)
			pr_err("%s: led_classdev_register failed\n", __func__);
	}
	return 0;
}

static int __devexit mcu_led_remove(struct platform_device *pdev)
{
	struct leds_msp430_platform_data *pdata = pdev->dev.platform_data;
	struct led_mcu_data *led;
	int i;

	for (i = 0; i < pdata->num_leds; i++) {
		led = &pdata->leds[i];
		led_classdev_unregister(&led->ldev);
	}
	return 0;
}

static struct platform_driver mcu_led_driver = {
	.probe		= mcu_led_probe,
	.remove		= __devexit_p(mcu_led_remove),
	.driver		= {
		.name	= "msp430-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init mcu_led_init(void)
{
	return platform_driver_register(&mcu_led_driver);
}

static void __exit mcu_led_exit(void)
{
	platform_driver_unregister(&mcu_led_driver);
}

module_init(mcu_led_init);
module_exit(mcu_led_exit);

MODULE_AUTHOR("Jacob Lee <jacob_lee@acer.com.tw");
MODULE_DESCRIPTION("MCU LEDS driver");
MODULE_LICENSE("GPL");

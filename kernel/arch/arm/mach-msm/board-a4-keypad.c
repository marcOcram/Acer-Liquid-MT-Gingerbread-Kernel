/* arch/arm/mach-msm/board-a4-keypad.c
 *
 * Copyright (C) 2010 Acer Corporation.
 *
 * Author: Brad Chen <ChunHung_Chen@acer.com.tw>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/keyreset.h>
#include <linux/platform_device.h>
#include <mach/vreg.h>

#include <asm/mach-types.h>

#include "board-a4.h"

static struct gpio_event_direct_entry a4_keypad_key_map[] = {
	{
		.gpio	= A4_GPIO_KEY_VOL_UP,
		.code	= KEY_VOLUMEUP,
		.no_wake = 1,
	},
	{
		.gpio	= A4_GPIO_KEY_VOL_DOWN,
		.code	= KEY_VOLUMEDOWN,
		.no_wake = 1,
	},
	{
		.gpio	= A4_GPIO_CAM_BTN_STEP1,
		.code	= KEY_CAMERA - 1,
		.no_wake = 1,
	},
	{
		.gpio	= A4_GPIO_CAM_BTN_STEP2,
		.code	= KEY_CAMERA,
		.no_wake = 1,
	},
};

static struct gpio_event_input_info a4_keypad_key_info = {
	.info = {
		.func = gpio_event_input_func,
		.no_suspend = true,
	},
	.debounce_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags = 0,
	.type = EV_KEY,
	.keymap = a4_keypad_key_map,
	.keymap_size = ARRAY_SIZE(a4_keypad_key_map)
};

static struct gpio_event_info *a4_input_info[] = {
	&a4_keypad_key_info.info,
};

static struct gpio_event_platform_data a4_input_data = {
	.names = {
		"a4-gpio-keypad",
		NULL,
	},
	.info = a4_input_info,
	.info_count = ARRAY_SIZE(a4_input_info),
};

static struct platform_device a4_input_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &a4_input_data,
	},
};

static int __init a4_init_keypad(void)
{
	int ret;

	ret = platform_device_register(&a4_input_device);
	if (ret) {
		printk(KERN_ERR "%s: register platform device fail (%d)\n",
								__func__, ret);
		return ret;
	}
	return 0;
}

device_initcall(a4_init_keypad);

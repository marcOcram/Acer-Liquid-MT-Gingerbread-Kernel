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

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <mach/camera.h>

int adp1650_open_init(void);
int32_t adp1650_flash_mode_control(int ctrl);
int32_t adp1650_torch_mode_control(int ctrl);

#define ADP1650_REG_CURRENT_SET			0x03
#define ADP1650_REG_OUTPUT_MODE			0x04

/* Current Set Register (Register 0x03) */
/* Register Shifts */
#define ADP1650_I_FL_SHIFT			3
/* I_FL */
#define ADP1650_I_FL_300mA			0x00
#define ADP1650_I_FL_350mA			0x01
#define ADP1650_I_FL_400mA			0x02
#define ADP1650_I_FL_450mA			0x03
#define ADP1650_I_FL_500mA			0x04
#define ADP1650_I_FL_550mA			0x05
#define ADP1650_I_FL_600mA			0x06
#define ADP1650_I_FL_650mA			0x07
#define ADP1650_I_FL_700mA			0x08
#define ADP1650_I_FL_750mA			0x09
#define ADP1650_I_FL_800mA			0x0A
#define ADP1650_I_FL_850mA			0x0B
#define ADP1650_I_FL_900mA			0x0C
#define ADP1650_I_FL_950mA			0x0D
#define ADP1650_I_FL_1000mA			0x0E	/* default */
#define ADP1650_I_FL_1050mA			0x0F
#define ADP1650_I_FL_1100mA			0x10
#define ADP1650_I_FL_1150mA			0x11
#define ADP1650_I_FL_1200mA			0x12
#define ADP1650_I_FL_1250mA			0x13
#define ADP1650_I_FL_1300mA			0x14
#define ADP1650_I_FL_1350mA			0x15
#define ADP1650_I_FL_1400mA			0x16
#define ADP1650_I_FL_1450mA			0x17
#define ADP1650_I_FL_1500mA			0x18
/* I_TOR */
#define ADP1650_I_TOR_25mA			0x00
#define ADP1650_I_TOR_50mA			0x01
#define ADP1650_I_TOR_75mA			0x02
#define ADP1650_I_TOR_100mA			0x03	/* default */
#define ADP1650_I_TOR_125mA			0x04
#define ADP1650_I_TOR_150mA			0x05
#define ADP1650_I_TOR_175mA			0x06
#define ADP1650_I_TOR_200mA			0x07

/* Output Mode Register (Register 0x04) */
/* Register Shifts */
#define ADP1650_IL_PEAK_SHIFT			6
#define ADP1650_STR_LV_SHIFT			5
#define ADP1650_FREQ_FB_SHIFT			4
#define ADP1650_OUTPUT_EN_SHIFT			3
#define ADP1650_STR_MODE_SHIFT			2
/* IL_PEAK */
#define ADP1650_IL_PEAK_1P75A			0x00
#define ADP1650_IL_PEAK_1P25A			0x01
#define ADP1650_IL_PEAK_2P75A			0x02	/* default */
#define ADP1650_IL_PEAK_3A			0x03
/* STR_LV */
#define ADP1650_STR_LV_EDGE_SENSITIVE		0x00
#define ADP1650_STR_LV_LEVEL_SENSITIVE		0x01	/* defualt */
/* FREQ_FB */
#define ADP1650_FREQ_FB_1P5MHZ_NOT_ALLOWED	0x00	/* defualt */
#define ADP1650_FREQ_FB_1P5MHZ_ALLOWED		0x01
/* OUTPUT_EN */
#define ADP1650_OUTPUT_EN_OFF			0x00	/* defualt */
#define ADP1650_OUTPUT_EN_ON			0x01
/* STR_MODE */
#define ADP1650_STR_MODE_SW			0x00
#define ADP1650_STR_MODE_HW			0x01	/* defualt */
/* LED_MODE */
#define ADP1650_LED_MODE_STANDBY		0x00	/* defualt */
#define ADP1650_LED_MODE_VOUT			0x01
#define ADP1650_LED_MODE_ASSIST			0x02
#define ADP1650_LED_MODE_FLASH			0x03

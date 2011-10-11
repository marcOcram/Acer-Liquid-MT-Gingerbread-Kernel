/* include/linux/cy8c_tmg_ts.c
 *
 * Copyright (C) 2007-2008 HTC Corporation.
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

#ifndef CYPRESS_I2C_H
#define CYPRESS_I2C_H

#include <linux/types.h>

#define CYPRESS_TS_DRIVER_NAME "cypress-ts"

struct cypress_i2c_platform_data {
	int abs_x_min;
	int abs_x_max;
	int abs_y_min;
	int abs_y_max;
	int abs_pressure_min;
	int abs_pressure_max;
	int abs_id_min;
	int abs_id_max;
	int y_max;	/* max Y coordinate, include virtual keys if needed */
	int points_max;
	int irqflags;
	int (*hw_init)(int on);
	int (*power)(int ch);
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_ISSP
	bool (*enable_fw_update)(void);
#endif
};

enum {
	TS_VDD_POWER_OFF,
	TS_VDD_POWER_ON,
	TS_RESET,
};

enum {
	SUSPEND_STATE,
	INIT_STATE,
};

extern signed char download_firmware_main(char *);

#endif


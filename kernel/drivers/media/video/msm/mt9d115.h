/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2010, Acer Inc.
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

#ifndef MT9D115_H
#define MT9D115_H

#include <linux/types.h>
#include <mach/camera.h>

extern struct mt9d115_reg mt9d115_regs;

enum mt9d115_width {
	WORD_LEN,
	BYTE_LEN
};

struct mt9d115_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum mt9d115_width width;
	unsigned short mdelay_time;
};

struct mt9d115_reg {
	const struct register_address_value_pair *prev_snap_reg_settings;
	uint16_t prev_snap_reg_settings_size;
	const struct register_address_value_pair *noise_reduction_reg_settings;
	uint16_t noise_reduction_reg_settings_size;
	const struct mt9d115_i2c_reg_conf *plltbl;
	uint16_t plltbl_size;
	const struct mt9d115_i2c_reg_conf *stbl;
	uint16_t stbl_size;
	const struct mt9d115_i2c_reg_conf *rftbl;
	uint16_t rftbl_size;
	const struct mt9d115_i2c_reg_conf *otbl;
	uint16_t otbl_size;
};

#endif /* MT9D115_H */

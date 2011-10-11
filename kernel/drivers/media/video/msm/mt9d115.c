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

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include "mt9d115.h"

/* Micron MT9D115 Registers and their values */
/* Sensor Core Registers */
#define  REG_MT9D115_MODEL_ID 0x0000
#define  MT9D115_MODEL_ID     0x2580

/*  SOC Registers Page 1  */
#define  REG_MT9D115_SENSOR_RESET     0x301A
#define  REG_MT9D115_STANDBY_CONTROL  0x3202
#define  REG_MT9D115_MCU_BOOT         0x3386

#define SENSOR_DEBUG 0

struct mt9d115_work {
	struct work_struct work;
};

static struct  mt9d115_work *mt9d115_sensorw;
static struct  i2c_client *mt9d115_client;

struct mt9d115_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};


static struct mt9d115_ctrl *mt9d115_ctrl;
static struct vreg *vreg_gp2 = NULL;

static DECLARE_WAIT_QUEUE_HEAD(mt9d115_wait_queue);
DECLARE_MUTEX(mt9d115_sem);
static int16_t mt9d115_effect = CAMERA_EFFECT_OFF;
#if defined(CONFIG_MACH_ACER_A5)
static int16_t mt9d115_antibanding = CAMERA_ANTI_BANDING_OFF;
#endif

/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct mt9d115_reg mt9d115_regs;


/*=============================================================*/

static int mt9d115_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;

	CDBG("%s\n", __func__);

	rc = gpio_direction_output(dev->sensor_reset, 0);
	mdelay(1);
	rc = gpio_direction_output(dev->sensor_reset, 1);

	return rc;
}

static int32_t mt9d115_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

#if SENSOR_DEBUG
	if (length == 2)
		CDBG("msm_io_i2c_w: 0x%04x 0x%04x\n",
			*(u16 *) txdata, *(u16 *) (txdata + 2));
	else if (length == 4)
		CDBG("msm_io_i2c_w: 0x%04x\n", *(u16 *) txdata);
	else
		CDBG("msm_io_i2c_w: length = %d\n", length);
#endif
	if (i2c_transfer(mt9d115_client->adapter, msg, 1) < 0) {
		CDBG("mt9d115_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9d115_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum mt9d115_width width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF00)>>8;
		buf[3] = (wdata & 0x00FF);

		rc = mt9d115_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = waddr;
		buf[1] = wdata;
		rc = mt9d115_i2c_txdata(saddr, buf, 2);
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t mt9d115_i2c_write_table(
	struct mt9d115_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata,
			reg_conf_tbl->width);
		if (rc < 0)
			break;
		if (reg_conf_tbl->mdelay_time != 0)
			mdelay(reg_conf_tbl->mdelay_time);
		reg_conf_tbl++;
	}

	return rc;
}

static int mt9d115_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(mt9d115_client->adapter, msgs, 2) < 0) {
		CDBG("mt9d115_i2c_rxdata failed!\n");
		return -EIO;
	}

#if SENSOR_DEBUG
	if (length == 2)
		CDBG("msm_io_i2c_r: 0x%04x\n", *(u16 *) rxdata);
	else if (length == 4)
		CDBG("msm_io_i2c_r: 0x%04x 0x%04x\n",
			*(u16 *) rxdata, *(u16 *) (rxdata + 2));
	else
		CDBG("msm_io_i2c_r: length = %d\n", length);
#endif

	return 0;
}

static int32_t mt9d115_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum mt9d115_width width)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	switch (width) {
	case WORD_LEN: {
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);

		rc = mt9d115_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0] << 8 | buf[1];
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG("mt9d115_i2c_read failed!\n");

	return rc;
}

#if 0
static int32_t mt9d115_set_lens_roll_off(void)
{
	int32_t rc = 0;
	rc = mt9d115_i2c_write_table(&mt9d115_regs.rftbl[0],
								 mt9d115_regs.rftbl_size);
	return rc;
}
#endif

static long mt9d115_i2c_polling(unsigned short raddr,
	uint8_t pollingvalue, uint16_t delaytime, uint16_t timeout)
{
	uint16_t counter = 0, value = 0;
	long rc;

	CDBG("%s\n", __func__);
	timeout = ((timeout/delaytime)+1)*delaytime;
	CDBG("recalculate timeout = %d\n", timeout);
	do {
		mdelay(delaytime);
		rc = mt9d115_i2c_read(mt9d115_client->addr,
			0x0990, &value, WORD_LEN);
		counter++;
	} while (value != pollingvalue && ((counter*delaytime) < timeout));

	return rc;
}

static long mt9d115_reg_init(void)
{
	int32_t array_length;
	int32_t i;
	long rc;
	uint16_t value = 0;

	CDBG("%s\n", __func__);
	/* PLL Setup Start */
	rc = mt9d115_i2c_write_table(&mt9d115_regs.plltbl[0],
					mt9d115_regs.plltbl_size);
	if (rc < 0)
		return rc;

	/* OFIFO_CONTROL_STATUS */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x321C, 0x0003, WORD_LEN);
	if (rc < 0)
		return rc;

	/* Set powerup stop bit
	STANDBY_CONTROL
	(BITFIELD=0x0018, 0x0004, 1)=0x402D */
	rc = mt9d115_i2c_read(mt9d115_client->addr,
		0x0018, &value, WORD_LEN);
	if (rc < 0)
		return rc;
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0018, value|0x0004, WORD_LEN);
	if (rc < 0)
		return rc;

	/* Release MCU from standby
	STANDBY_CONTROL
	(BITFIELD=0x0018, 0x0001, 0)=0x402C*/
	rc = mt9d115_i2c_read(mt9d115_client->addr,
		0x0018, &value, WORD_LEN);
	if (rc < 0)
		return rc;
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0018, value&0xFFFE, WORD_LEN);
	if (rc < 0)
		return rc;

	/* Configure sensor for Preview mode and Snapshot mode */
	array_length = mt9d115_regs.prev_snap_reg_settings_size;
	for (i = 0; i < array_length; i++) {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
		  mt9d115_regs.prev_snap_reg_settings[i].register_address,
		  mt9d115_regs.prev_snap_reg_settings[i].register_value,
		  WORD_LEN);

		if (rc < 0)
			return rc;
	}

	/* POLL_FIELD=MON_PATCH_ID_0 */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x98C, 0xA024, WORD_LEN);
	if (rc < 0)
		return rc;
	mt9d115_i2c_polling(0x0990, 1, 40, 100);

	/* STANDBY_CONTROL
	(BITFIELD=0x0018, 0x0004, 0)=0x0028 */
	rc = mt9d115_i2c_read(mt9d115_client->addr,
		0x0018, &value, WORD_LEN);
	if (rc < 0)
		return rc;
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0018, value&0xFFFB, WORD_LEN);
	if (rc < 0)
		return rc;

	/* other value for FIH tuning */
	rc = mt9d115_i2c_write_table(&mt9d115_regs.otbl[0],
					mt9d115_regs.otbl_size);
	if (rc < 0)
		return rc;

	/* Refresh Mode */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x98C, 0xA103, WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x990, 0x06, WORD_LEN);
	if (rc < 0)
		return rc;
	mt9d115_i2c_polling(0x0990, 0, 40, 100);

	/* Refresh */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x98C, 0xA103, WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x990, 0x05, WORD_LEN);
	if (rc < 0)
		return rc;
	mt9d115_i2c_polling(0x0990, 0, 40, 100);

	/* FIELD_WR= PAD_SLEW */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x001E, 0x0505, WORD_LEN);
	if (rc < 0)
		return rc;
#if 0
	/* Configure for Noise Reduction, Saturation and Aperture Correction */
	array_length = mt9d115_regs.noise_reduction_reg_settings_size;

	for (i = 0; i < array_length; i++) {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			mt9d115_regs.noise_reduction_reg_settings[i].register_address,
			mt9d115_regs.noise_reduction_reg_settings[i].register_value,
			WORD_LEN);

		if (rc < 0)
			return rc;
	}

	/* Set Color Kill Saturation point to optimum value */
	rc =
	mt9d115_i2c_write(mt9d115_client->addr,
	0x35A4,
	0x0593,
	WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_write_table(&mt9d115_regs.stbl[0],
					mt9d115_regs.stbl_size);
	if (rc < 0)
		return rc;

	rc = mt9d115_set_lens_roll_off();
	if (rc < 0)
		return rc;
#endif

	return 0;
}

static long mt9d115_set_effect(int mode, int effect)
{
	long rc = 0;

	if (mt9d115_effect == effect) {
		CDBG("%s, The same as before not need set again.\n", __func__);
		return 0;
	}

	CDBG("%s, mode = %d, effect = %d\n", __func__, mode, effect);

	switch (effect) {
	case CAMERA_EFFECT_OFF: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x2759, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6440, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x275B, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6440, WORD_LEN);
		if (rc < 0)
			return rc;
	}
			break;

	case CAMERA_EFFECT_MONO: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x2759, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6441, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x275B, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6441, WORD_LEN);
		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_NEGATIVE: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x2759, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6443, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x275B, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6443, WORD_LEN);
		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_SOLARIZE: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x2759, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6444, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x275B, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6444, WORD_LEN);
		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_SEPIA: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x2759, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6442, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0x275B, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x6442, WORD_LEN);
		if (rc < 0)
			return rc;
	}
		break;

	default: {

		return -EINVAL;
	}
	}

	/* Refresh Sequencer */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x098C, 0xA103, WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0990, 0x0005, WORD_LEN);
	mt9d115_i2c_polling(0x0990, 0, 40, 100);

	if (rc >= 0)
		mt9d115_effect = effect;
	return rc;
}

#if defined(CONFIG_MACH_ACER_A5)
static long mt9d115_set_antibanding(int mode, int antibanding)
{
	long rc = 0;

	if (mt9d115_antibanding == antibanding) {
		CDBG("%s, The same as before not need set again.\n", __func__);
		return 0;
	}

	CDBG("%s, mode = %d, antibanding = %d\n", __func__, mode, antibanding);

	switch (antibanding) {
	case CAMERA_ANTI_BANDING_AUTO: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0xA11E, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x0001, WORD_LEN);
		if (rc < 0)
			return rc;
	}
			break;

	case CAMERA_ANTI_BANDING_60HZ: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0xA11E, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x0002, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0xA404, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x0080, WORD_LEN);
		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_ANTI_BANDING_50HZ: {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0xA11E, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x0002, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x098C, 0xA404, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
			0x0990, 0x00C0, WORD_LEN);
		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_ANTI_BANDING_OFF:
		break;

	default: {

		return -EINVAL;
	}
	}

	/* Refresh Sequencer */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x098C, 0xA103, WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0990, 0x0006, WORD_LEN);
	mt9d115_i2c_polling(0x0990, 0, 40, 100);

	if (rc >= 0)
		mt9d115_antibanding = antibanding;
	return rc;
}

static long mt9d115_set_rotation(int mode, uint16_t rotation)
{
	long rc = 0;
	uint8_t img_rotation = 0;
	uint16_t pre_value = 0, cur_value = 0;

	CDBG("%s, mode = %d, rotation = %d\n", __func__, mode, rotation);

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x098C, 0x272D, WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_read(mt9d115_client->addr,
		0x990, &pre_value, WORD_LEN);
	if (rc < 0)
		return rc;

	// Normal:0x0024, Horizontal:0x0025, Vertical: 0x0026, 180deg: 0x0027
	if (rotation == 0 || rotation == 180) { //rotation 0 and 180 degree
		img_rotation = CAMERA_ROTATION_VERTICAL;
		cur_value = 0x0024;
	} else if (rotation == 90 || rotation == 270) { //rotation 90 and 270 degree
		img_rotation = CAMERA_ROTATION_180DEG;
		cur_value = 0x0027;
	} else {
		return -EINVAL;
	}

	CDBG("%s, pre_value = 0x%x, cur_value = 0x%x\n", __func__, pre_value, cur_value);
	if (pre_value == cur_value) {
		CDBG("%s, The same as before not need set again.\n", __func__);
		return rc;
	}

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0990, cur_value, WORD_LEN);
	if (rc < 0)
		return rc;

	/* Refresh Sequencer */
	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x098C, 0xA103, WORD_LEN);
	if (rc < 0)
		return rc;

	rc = mt9d115_i2c_write(mt9d115_client->addr,
		0x0990, 0x0006, WORD_LEN);
	mt9d115_i2c_polling(0x0990, 0, 50, 200);

	return rc;
}
#endif

static long mt9d115_set_sensor_mode(int mode)
{
	long rc = 0;

	CDBG("%s, mode = %d\n", __func__, mode);
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0000, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA103, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0001, WORD_LEN);
		if (rc < 0)
			return rc;
		mdelay(5);

		break;

	case SENSOR_SNAPSHOT_MODE:
		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0002, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0012, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0032, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0072, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA103, WORD_LEN);
		if (rc < 0)
			return rc;

		CDBG("%s: line %d\n", __func__, __LINE__);
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0002, WORD_LEN);
		if (rc < 0)
			return rc;

		mdelay(5);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA115, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0070, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA116, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0003, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x098C, 0xA103, WORD_LEN);
		if (rc < 0)
			return rc;

		rc = mt9d115_i2c_write(mt9d115_client->addr,
				0x0990, 0x0002, WORD_LEN);
		if (rc < 0)
			return rc;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int mt9d115_probe_init_done(const struct msm_camera_sensor_info *data)
{
	CDBG("%s\n", __func__);

	// Pull up standby pin to enter hard standby first
	gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
	mdelay(200);
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);

	// Disable AVDD 2.8v
	if (vreg_gp2) {
		vreg_disable(vreg_gp2);
		vreg_gp2 = NULL;
	}
	return 0;
}

static int mt9d115_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id = 0;
	int rc = 0;

	CDBG("%s\n", __func__);
	CDBG("init entry \n");
	rc = mt9d115_reset(data);
	if (rc < 0) {
		CDBG("reset failed!\n");
		goto init_probe_fail;
	}

	// Enable AVDD 2.6v after reset pin high -> low -> high
	vreg_gp2 = vreg_get(NULL, "gp2");
	if (IS_ERR(vreg_gp2)) {
		pr_err("%s: Get vreg_gp2 failed %ld\n", __func__,
			PTR_ERR(vreg_gp2));
		vreg_gp2 = NULL;
		goto init_probe_fail;
	}

	if (vreg_set_level(vreg_gp2, 2600)) {
		pr_err("%s: Set vreg_gp2 failed\n", __func__);
		vreg_put(vreg_gp2);
		vreg_gp2 = NULL;
		goto init_probe_fail;
	}

	if (vreg_enable(vreg_gp2)) {
		pr_err("%s: Enable vreg_gp2 failed\n", __func__);
		vreg_put(vreg_gp2);
		vreg_gp2 = NULL;
		goto init_probe_fail;
	}

	mdelay(1);

	/* Read the Model ID of the sensor */
	rc = mt9d115_i2c_read(mt9d115_client->addr,
		REG_MT9D115_MODEL_ID, &model_id, WORD_LEN);
	if (rc < 0)
		goto init_probe_fail;

	CDBG("mt9d115 model_id = 0x%x\n", model_id);

	/* Check if it matches it with the value in Datasheet */
	if (model_id != MT9D115_MODEL_ID) {
		rc = -EINVAL;
		goto init_probe_fail;
	}

	rc = mt9d115_reg_init();
	if (rc < 0)
		goto init_probe_fail;

	return rc;

init_probe_fail:
	mt9d115_probe_init_done(data);
	return rc;
}

int mt9d115_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CDBG("%s\n", __func__);
	mt9d115_ctrl = kzalloc(sizeof(struct mt9d115_ctrl), GFP_KERNEL);
	if (!mt9d115_ctrl) {
		CDBG("mt9d115_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		mt9d115_ctrl->sensordata = data;

	rc = gpio_request(data->sensor_reset, "mt9d115");
	if (rc) {
		pr_err("Request 2M reset pin failed\n");
	}

	rc = gpio_request(data->sensor_pwd, "mt9d115");
	if (rc) {
		pr_err("Request 2M power down pin failed\n");
	}

	/* Input MCLK = 24MHz */
	msm_camio_clk_rate_set(24000000);

	msm_camio_camif_pad_reg_reset();

	rc = mt9d115_sensor_init_probe(data);
	if (rc < 0) {
		CDBG("mt9d115_sensor_init failed!\n");
		goto init_fail;
	}

init_done:
	return rc;

init_fail:
	mt9d115_probe_init_done(data);
	kfree(mt9d115_ctrl);
	return rc;
}

static int mt9d115_init_client(struct i2c_client *client)
{
	CDBG("%s\n", __func__);
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9d115_wait_queue);
	return 0;
}

int mt9d115_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&mt9d115_sem); */

	CDBG("mt9d115_ioctl, cfgtype = %d, mode = %d\n",
		cfg_data.cfgtype, cfg_data.mode);

		switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = mt9d115_set_sensor_mode(
						cfg_data.mode);
			break;

		case CFG_SET_EFFECT:
			rc = mt9d115_set_effect(cfg_data.mode,
						cfg_data.cfg.effect);
			break;

#if defined(CONFIG_MACH_ACER_A5)
		case CFG_SET_ANTIBANDING:
			rc = mt9d115_set_antibanding(cfg_data.mode,
						cfg_data.cfg.anti_banding);
			break;

		case CFG_SET_ROTATION:
			rc = mt9d115_set_rotation(cfg_data.mode,
						cfg_data.cfg.rotation);
			break;
#endif

		case CFG_GET_AF_MAX_STEPS:
		default:
			rc = -EINVAL;
			break;
		}

	/* up(&mt9d115_sem); */

	CDBG("%s: rc = %d\n", __func__, (int)rc);
	return rc;
}

int mt9d115_sensor_release(void)
{
	int rc = 0;

	CDBG("%s\n", __func__);
	/* down(&mt9d115_sem); */

	// Pull up standby pin to enter hard standby
	gpio_direction_output(mt9d115_ctrl->sensordata->sensor_pwd, 1);
	gpio_free(mt9d115_ctrl->sensordata->sensor_pwd);
	mdelay(200);

	// Pull down reset pin to avoid power consumption
	gpio_direction_output(mt9d115_ctrl->sensordata->sensor_reset, 0);
	gpio_free(mt9d115_ctrl->sensordata->sensor_reset);

	// Disable AVDD 2.8v
	if (vreg_gp2) {
		vreg_disable(vreg_gp2);
		vreg_gp2 = NULL;
	}

	kfree(mt9d115_ctrl);
	/* up(&mt9d115_sem); */

	return rc;
}

static int mt9d115_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9d115_sensorw =
		kzalloc(sizeof(struct mt9d115_work), GFP_KERNEL);

	if (!mt9d115_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9d115_sensorw);
	mt9d115_init_client(client);
	mt9d115_client = client;

	CDBG("mt9d115_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(mt9d115_sensorw);
	mt9d115_sensorw = NULL;
	CDBG("mt9d115_probe failed!\n");
	return rc;
}

static const struct i2c_device_id mt9d115_i2c_id[] = {
	{ "mt9d115", 0},
	{ },
};

static struct i2c_driver mt9d115_i2c_driver = {
	.id_table = mt9d115_i2c_id,
	.probe  = mt9d115_i2c_probe,
	.remove = __exit_p(mt9d115_i2c_remove),
	.driver = {
		.name = "mt9d115",
	},
};

static int mt9d115_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;

	CDBG("%s\n", __func__);
	rc = i2c_add_driver(&mt9d115_i2c_driver);
	if (rc < 0 || mt9d115_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	rc = gpio_request(info->sensor_reset, "mt9d115");
	if (rc) {
		pr_err("Request 2M reset pin failed\n");
	}

	rc = gpio_request(info->sensor_pwd, "mt9d115");
	if (rc) {
		pr_err("Request 2M power down pin failed\n");
	}

	/* Input MCLK = 24MHz */
	msm_camio_clk_rate_set(24000000);

	rc = mt9d115_sensor_init_probe(info);
	if (rc < 0)
		goto probe_done;

	s->s_init = mt9d115_sensor_init;
	s->s_release = mt9d115_sensor_release;
	s->s_config  = mt9d115_sensor_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = 180;
	mt9d115_probe_init_done(info);

probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
}

static int __mt9d115_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, mt9d115_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9d115_probe,
	.driver = {
		.name = "msm_camera_mt9d115",
		.owner = THIS_MODULE,
	},
};

static int __init mt9d115_init(void)
{
	CDBG("%s\n", __func__);
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9d115_init);

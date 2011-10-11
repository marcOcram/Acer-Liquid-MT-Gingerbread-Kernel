/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
 */

#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/adv7525.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/hdmi_cec.h>
#include "msm_fb.h"

#include "external_common.h"
#include <linux/wakelock.h>

/* #define PORT_DEBUG */

static struct external_common_state_type hdmi_common;

static struct i2c_client *hclient_main;
static struct i2c_client *hclient_cec;
static struct i2c_client *hclient_packet;

static bool chip_power_on = FALSE;	/* For chip power on/off */
static bool gpio_power_on = FALSE;	/* For dtv power on/off (I2C) */
static bool irq_en = FALSE;	/* For irq enable */

static u8 reg[256];	/* HDMI panel registers */

struct hdmi_data {
	struct msm_hdmi_platform_data *pd;
	struct work_struct work;
	struct input_dev *input;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};
static struct hdmi_data *dd;
static int adv7525_read_edid_block(int block, uint8 *edid_buf);

#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
static struct timer_list hdcp_link_timer;
static struct work_struct hdcp_link_work;
static int hdcp_activating;
static bool hdcp_on = FALSE;	/* For HDCP on/off */
static DEFINE_MUTEX(hdcp_state_mutex);
static DEFINE_MUTEX(hdcp_i2c_mutex);
static u8 bksv[75];
static u8 hdcp_bstatus[2];
#endif

static unsigned int monitor_plugin;
static unsigned int res;

static struct wake_lock idle_wlock;
static struct wake_lock suspend_wlock;
static struct timer_list hdmi_state_timer;

/* implement CEC */
static u8 adv7525_read_reg(struct i2c_client *client, u8 reg);
static int adv7525_write_reg(struct i2c_client *client, u8 reg, u8 val);

struct hdmi_i2c_type {
	u8 reg_addr;
	u8 reg_data;
};

/* CEC Clock Timing Register Settings for 1.2MHz */
static struct hdmi_i2c_type cec_clk_table_1_2_mhz[] = {
	{0x4e, 0x41},
	{0x51, 0x01},
	{0x52, 0x52},
	{0x53, 0x01},
	{0x54, 0x3b},
	{0x55, 0x01},
	{0x56, 0x68},
	{0x57, 0x01},
	{0x58, 0x16},
	{0x59, 0x00},
	{0x5a, 0xff},
	{0x5b, 0x01},
	{0x5c, 0x2c},
	{0x5d, 0x00},
	{0x5e, 0xb4},
	{0x5f, 0x00},
	{0x60, 0x92},
	{0x61, 0x00},
	{0x62, 0xd6},
	{0x63, 0x00},
	{0x64, 0x2d},
	{0x65, 0x00},
	{0x66, 0x71},
	{0x67, 0x00},
	{0x68, 0x87},
	{0x69, 0x00},
	{0x6a, 0x4f},
	{0x6b, 0x01},
	{0x6c, 0x0e},
	{0x6e, 0x00},
	{0x6f, 0x13},
	{0x71, 0x00},
	{0x72, 0x17},
	{0x73, 0x00},
	{0x74, 0x44},
	{0x75, 0x00},
	{0x76, 0x5a},
};

struct _cecdata {
    u8 logical_addr;
    u8 physical_addr[2];
    u8 hdr_directly;
    u8 hdr_broadcast;
};

static struct _cecdata cecdata;

int adv7525_i2c_read(struct i2c_client *client, u8 addr , u8 *buf , int length)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 1;
	msg[0].buf = &addr;
	msg[1].addr = client->addr;
	msg[1].flags = client->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = buf;

	if (i2c_transfer(adap, msg, 2) < 0) {
		DEV_INFO("%s: i2c read err\n", __func__);
		return -EBUSY;
	}
	return 0;
}

int adv7525_i2c_write(struct i2c_client *client, char *buf, int count)
{
	if (count != i2c_master_send(client, buf, count)) {
		DEV_INFO("%s: i2c write err\n", __func__);
		return -EBUSY;
	}
	return 0;
}

int hdmi_cec_get_logical_addr(void)
{
	u8 buf = 0;
	u8 playback_dev_addr[] = {4, 8, 11};
	u8 i = 0;

	for (i = 0 ; i < ARRAY_SIZE(playback_dev_addr) ; i++) {
		adv7525_write_reg(hclient_cec, 0x00, (playback_dev_addr[i] << 4 | playback_dev_addr[i]));
		adv7525_write_reg(hclient_cec, 0x11, 0x1);
		msleep(300);
		buf = adv7525_read_reg(hclient_cec, 0x14);
		if ((buf & 0x0f) != 0)
			return playback_dev_addr[i];
	}

	return 0x0f;
}

void adv7525_cec_init(void)
{
	int i;
	u16 phy_addr = hdmi_common_get_phy_addr();

	/* CEC Clock Timing Setting: 1.2MHz */
	for (i = 0; i < ARRAY_SIZE(cec_clk_table_1_2_mhz); i++)
		adv7525_write_reg(hclient_cec, cec_clk_table_1_2_mhz[i].reg_addr,
			cec_clk_table_1_2_mhz[i].reg_data);

	/* Initial cecdata structure */
	cecdata.logical_addr = hdmi_cec_get_logical_addr();
	cecdata.hdr_directly = cecdata.logical_addr << 4 | 0x0;
	cecdata.hdr_broadcast = cecdata.logical_addr << 4 | 0xf;
	cecdata.physical_addr[0] = (u8)(phy_addr >> 8);
	cecdata.physical_addr[1] = (u8)(phy_addr & 0x0f) ;
	DEV_INFO("logcal addr: 0x%x, physical addr: 0x%x\n", cecdata.logical_addr, phy_addr);

	/* CEC Rx Setting init */
	adv7525_write_reg(hclient_cec, 0x4a, 0);
	adv7525_write_reg(hclient_cec, 0x4c, (0xf << 4 | cecdata.logical_addr));
}

void adv7525_cec_rx_handle(u8 *rxbuf)
{
	u8 txbuf[16] = {0};
	int tx_length = 0;

	DEV_INFO("%s: Opcode=0x%x, Data1=0x%x\n", __func__, rxbuf[1], rxbuf[2]);
	switch (rxbuf[1]) {
	case CEC_OPCODE_USER_CONTROL_PRESSED:
		if (rxbuf[2])
			input_report_key(dd->input, rxbuf[2], 1);
		else
			input_report_key(dd->input, 0x2b, 1);	/* Select key */
	break;

	case CEC_OPCODE_USER_CONTROL_RELEASED:
		if (rxbuf[2])
			input_report_key(dd->input, rxbuf[2], 0);
		else
			input_report_key(dd->input, 0x2b, 0);
	break;

	case CEC_OPCODE_PLAY:
		switch (rxbuf[2]) {
		case 0x24:
		case 0x25:
			input_report_key(dd->input, 0x46, 1);
			input_report_key(dd->input, 0x46, 0);
		break;

		default:
		break;
		}
	break;

	case CEC_OPCODE_DECK_CONTROL:
		switch (rxbuf[2]) {
		case 0x1:
			input_report_key(dd->input, 0x49, 1);
			input_report_key(dd->input, 0x49, 0);
		break;
		case 0x2:
			input_report_key(dd->input, 0x48, 1);
			input_report_key(dd->input, 0x48, 0);
		break;
		case 0x3: /* stop */
			input_report_key(dd->input, 0x46, 1);
			input_report_key(dd->input, 0x46, 0);
		break;
		default:
		break;
		}
	break;

	case CEC_OPCODE_GIVE_OSD_NAME:
		txbuf[1] = cecdata.hdr_directly;
		txbuf[2] = CEC_OPCODE_SET_OSD_NAME;
		txbuf[3] = 'A';
		txbuf[4] = 'c';
		txbuf[5] = 'e';
		txbuf[6] = 'r';
		txbuf[7] = ' ';
		txbuf[8] = 'S';
		txbuf[9] = '3';
		txbuf[10] = '0';
		txbuf[11] = '0';
		tx_length = 11;
	break;

	case CEC_OPCODE_GIVE_DEVICE_VENDOR_ID:
		txbuf[1] = cecdata.hdr_broadcast;
		txbuf[2] = CEC_OPCODE_DEVICE_VENDOR_ID;
		txbuf[3] = 0x00;
		txbuf[4] = 0x00;
		txbuf[5] = 0xE2;
		tx_length = 5;
	break;

	case CEC_OPCODE_GIVE_DEVICE_POWER_STATUS:
		txbuf[1] = cecdata.hdr_directly;
		txbuf[2] = CEC_OPCODE_REPORT_POWER_STATUS;
		txbuf[3] = 0x00;
		tx_length = 3;
	break;

	case CEC_OPCODE_GET_CEC_VERSION:
		txbuf[1] = cecdata.hdr_directly;
		txbuf[2] = CEC_OPCODE_CEC_VERSION;
		txbuf[3] = 0x04;
		tx_length = 3;
	break;

	case CEC_OPCODE_SET_STREAM_PATH:
	case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
		txbuf[1] = cecdata.hdr_broadcast;
		txbuf[2] = CEC_OPCODE_ACTIVE_SOURCE;
		txbuf[3] = cecdata.physical_addr[0];
		txbuf[4] = cecdata.physical_addr[1];
		tx_length = 4;
	break;

	case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
		txbuf[1] = cecdata.hdr_broadcast;
		txbuf[2] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
		txbuf[3] = cecdata.physical_addr[0];
		txbuf[4] = cecdata.physical_addr[1];
		txbuf[5] = type_playback_device;
		tx_length = 5;
	break;

	case CEC_OPCODE_GIVE_DECK_STATUS:
		txbuf[1] = cecdata.hdr_directly;
		txbuf[2] = CEC_OPCODE_DECK_STATUS;
		txbuf[3] = 0x01;
		tx_length = 3;
	break;

	case CEC_OPCODE_MENU_REQUEST:
		txbuf[1] = cecdata.hdr_directly;
		txbuf[2] = CEC_OPCODE_MENU_STATUS;
		txbuf[3] = 0x01;
		tx_length = 3;
	break;

	default:
	break;
	}

	if (tx_length != 0) {
		adv7525_i2c_write(hclient_cec, txbuf, tx_length+1);
		adv7525_write_reg(hclient_cec, 0x10, tx_length);
		adv7525_write_reg(hclient_cec, 0x11, 0x1);
	}
}

static ssize_t hdmi_resolution_store(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	kobject_uevent(external_common_state->uevent_kobj, KOBJ_OFFLINE);
	res = simple_strtoul(buf, NULL, 0);

	switch (res)  {
		case 0:
			HDMI_SETUP_LUT(640x480p60_4_3);
			HDMI_SETUP_LUT(720x480p60_4_3);
			HDMI_SETUP_LUT(720x480p60_16_9);
			HDMI_SETUP_LUT(1280x720p60_16_9);
			break;
		case 1:
			HDMI_SETUP_LUT(640x480p60_4_3);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_1280x720p60_16_9);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_720x480p60_4_3);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_720x480p60_16_9);
			break;
		case 2:
			HDMI_SETUP_LUT(720x480p60_4_3);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_1280x720p60_16_9);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_640x480p60_4_3);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_720x480p60_16_9);
			break;
		case 3:
			HDMI_SETUP_LUT(720x480p60_16_9);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_1280x720p60_16_9);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_720x480p60_4_3);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_640x480p60_4_3);
			break;
		case 4:
			HDMI_SETUP_LUT(1280x720p60_16_9);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_720x480p60_16_9);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_720x480p60_4_3);
			HDMI_UNSETUP_LUT(HDMI_VFRMT_640x480p60_4_3);
			break;
		default:
			break;
	}

	msleep(1000);
	kobject_uevent(external_common_state->uevent_kobj, KOBJ_ONLINE);
	return count;
}

static struct device_attribute hdmi_resolution_attrs =
__ATTR(res, S_IRUGO | S_IWUSR | S_IWGRP, NULL, hdmi_resolution_store);

#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
static ssize_t hdmi_hdcp_enable_store(struct device *device,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (!hdcp_activating)
		hdcp_on = (bool)simple_strtoul(buf, NULL, 0);
	pr_info("%s: hdcp_on = %d\n", __func__, hdcp_on);

	return count;
}

static ssize_t hdmi_hdcp_enable_show(struct device *device,
		struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", hdcp_on);
}

static struct device_attribute hdmi_hdcp_enable_attrs =
__ATTR(hdcp, S_IRUGO | S_IWUSR | S_IWGRP, hdmi_hdcp_enable_show, hdmi_hdcp_enable_store);
#endif

/* Change HDMI state */
static void change_hdmi_state(int online)
{
	if (!external_common_state)
		return;

	mutex_lock(&external_common_state_hpd_mutex);
	external_common_state->hpd_state = online;
	mutex_unlock(&external_common_state_hpd_mutex);

	if (!external_common_state->uevent_kobj)
		return;

	if (online)
		kobject_uevent(external_common_state->uevent_kobj,
			KOBJ_ONLINE);
	else
		kobject_uevent(external_common_state->uevent_kobj,
			KOBJ_OFFLINE);
	DEV_DBG("adv7525_uevent: %d\n", online);
}


/*
 * Read a value from a register on ADV7525 device
 * If sucessfull returns value read , otherwise error.
 */
static u8 adv7525_read_reg(struct i2c_client *client, u8 reg)
{
	int err;
	struct i2c_msg msg[2];
	u8 reg_buf[] = { reg };
	u8 data_buf[] = { 0 };

	if (!client->adapter)
		return -ENODEV;
	if (!gpio_power_on) {
		DEV_WARN("%s: WARN: missing GPIO power\n", __func__);
		return -ENODEV;
	}
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = reg_buf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data_buf;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err < 0) {
		DEV_INFO("%s: I2C err: %d\n", __func__, err);
		return err;
	}

#ifdef PORT_DEBUG
	DEV_INFO("HDMI[%02x] [R] %02x\n", reg, data);
#endif
	return *data_buf;
}

/*
 * Write a value to a register on adv7525 device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int adv7525_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];

	if (!client->adapter)
		return -ENODEV;
	if (!gpio_power_on) {
		DEV_WARN("%s: WARN: missing GPIO power\n", __func__);
		return -ENODEV;
	}

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;
	data[0] = reg;
	data[1] = val;

	err = i2c_transfer(client->adapter, msg, 1);

	if (err >= 0)
		return 0;
#ifdef PORT_DEBUG
	DEV_INFO("HDMI[%02x] [W] %02x [%d]\n", reg, val, err);
#endif
	return err;
}

static void adv7525_write_reg_mask(struct i2c_client *client, u8 reg, u8 mask, u8 shift, u8 val)
{
	u8 data;

	data = adv7525_read_reg(client, reg);
	data = ( data & ~(mask << shift) ) | (u8)( ( val << shift ) & (mask << shift));
	adv7525_write_reg(client, reg, data);
}

#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
static void adv7525_hdcp_link_work(struct work_struct *work)
{
	u8 reg0xb8 = adv7525_read_reg(hclient_main, 0xb8);

	if (reg0xb8 & (1 << 6)) {
		DEV_INFO("%s: HDCP Link OK\n", __func__);
		mod_timer(&hdcp_link_timer, jiffies + msecs_to_jiffies(2000));
	} else {
		/* clear HDCP request */
		adv7525_write_reg_mask(hclient_main, 0xaf, 0x1, 7, 0);
		adv7525_write_reg_mask(hclient_main, 0xaf, 0x1, 7, 1);
		DEV_INFO("%s: clear HDCP reg[0xaf]=0x%02x\n",
					__func__, reg[0xaf]);
	}
}

static void adv7525_hdcp_link_timer_expired(unsigned long unused)
{
	schedule_work(&hdcp_link_work);
}
#endif

static void adv7525_state_timer_expired(unsigned long unused)
{
	change_hdmi_state(1);
}

static int adv7525_read_edid_block(int block, uint8 *edid_buf)
{
	u8 r = 0;
	int ret;
	struct i2c_msg msg[] = {
		{ .addr = reg[0x43] >> 1,
		  .flags = 0,
		  .len = 1,
		  .buf = &r },
		{ .addr = reg[0x43] >> 1,
		  .flags = I2C_M_RD,
		  .len = 0x100,
		  .buf = edid_buf } };

	if (block == 0)
		adv7525_write_reg(hclient_main, 0xc4, 0);
	if (block == 1)
		return 0;
	else if (block > 1) {
		adv7525_write_reg(hclient_main, 0xc4, 1);
		msleep(100);
	}

	ret = i2c_transfer(hclient_main->adapter, msg, 2);
	DEV_DBG("EDID block: addr=%02x, ret=%d\n", reg[0x43] >> 1, ret);
	return (ret < 2) ? -ENODEV : 0;
}

static void adv7525_read_edid(void)
{
	external_common_state->read_edid_block = adv7525_read_edid_block;
	hdmi_common_read_edid();
}

static void adv7525_chip_off(void)
{
	if (chip_power_on) {
		unsigned long reg0x41;

		DEV_INFO("%s: turn off chip power\n", __func__);

		/* Power down the whole chip,except I2C,HPD interrupt */
		reg0x41 = adv7525_read_reg(hclient_main, 0x41);
		set_bit(6, &reg0x41);
		adv7525_write_reg(hclient_main, 0x41, (u8)reg0x41);
		dd->pd->cec_power(0);
		chip_power_on = FALSE;
	} else
		DEV_INFO("%s: chip is already off\n", __func__);
}

static void adv7525_set_av_mute(bool on)
{
	if (on) {
		adv7525_write_reg_mask(hclient_main, 0x40, 0x01, 7, 0x01);
		adv7525_write_reg_mask(hclient_main, 0x4B, 0x03, 6, 0x01);
	} else {
		adv7525_write_reg_mask(hclient_main, 0x40, 0x01, 7, 0x01);
		adv7525_write_reg_mask(hclient_main, 0x4B, 0x03, 6, 0x02);
	}
}

void adv7525_tx_init(void)
{
	if (hdcp_on)
		adv7525_write_reg_mask(hclient_main, 0xe4, 0x01, 1, 0x01);
	else
		adv7525_write_reg_mask(hclient_main, 0xe4, 0x01, 1, 0x00);
}

void adv7525_set_spd_packet(void)
{
	u8 header[4] = {0, 0x83, 0x01, 0x19};
	u8 value = 0;
	u8 vendor_name[9] = {0x04, 'a', 'c', 'e', 'r'};
	u8 product_description[17] = {0x0c, 'S', '3', '0', '0'};

	/* Set SPD packet update */
	adv7525_write_reg_mask(hclient_packet, 0x1f, 0x01, 7, 0x01);

	adv7525_i2c_write(hclient_packet, header, 4);
	adv7525_i2c_write(hclient_packet, vendor_name, 9);
	adv7525_i2c_write(hclient_packet, product_description, 17);
	adv7525_i2c_write(hclient_packet, &value, 1);
	/* Set SPD packet enable */
	adv7525_write_reg_mask(hclient_main, 0x40, 0x01, 6, 0x01);

	adv7525_write_reg_mask(hclient_packet, 0x1f, 0x01, 7, 0x00);
}

void adv7525_set_audio_info(void)
{
	/* set audio coding type */
	adv7525_write_reg_mask(hclient_main, 0x73, 0x0f, 4, 0x00);

	/* sampling frequency */
	adv7525_write_reg_mask(hclient_main, 0x74, 0x07, 2, 0x00);
	/* sample size */
	adv7525_write_reg_mask(hclient_main, 0x74, 0x03, 0, 0x00);

	adv7525_write_reg_mask(hclient_main, 0x77, 0x01, 7, 0x00);
	adv7525_write_reg_mask(hclient_main, 0x77, 0x0f, 3, 0x00);
	adv7525_write_reg_mask(hclient_main, 0x77, 0x03, 0, 0x00);

	adv7525_write_reg_mask(hclient_main, 0x44, 0x01, 3, 0x01);
}

void adv7525_set_vs_info(void)
{
	u8 packet[12] = { 0xc0, 0x81, 0x01, 0x05, 0x87, 0x00, 0x0c, 0x03, 0x00, 0x00, 0x00, 0x00 };

	adv7525_write_reg_mask(hclient_packet, 0xdf, 0x01, 7, 0x01);
	adv7525_i2c_write(hclient_packet, packet, 12);
	adv7525_write_reg_mask(hclient_main, 0x40, 0x01, 0, 0x01);
	adv7525_write_reg_mask(hclient_packet, 0xdf, 0x01, 7, 0x00);
	adv7525_write_reg_mask(hclient_main, 0x40, 0x01, 0, 0x00);
}

static void setup_audio_video(void)
{
	/* set video mode */
	switch (external_common_state->video_resolution) {
	case HDMI_VFRMT_640x480p60_4_3:
	case HDMI_VFRMT_720x480p60_4_3:
		adv7525_write_reg_mask(hclient_main, 0x17, 1, 1, 0);
		adv7525_write_reg(hclient_main, 0x56, 0x18);
		break;
	case HDMI_VFRMT_720x480p60_16_9:
	case HDMI_VFRMT_1280x720p60_16_9:
		adv7525_write_reg_mask(hclient_main, 0x17, 1, 1, 1);
		adv7525_write_reg(hclient_main, 0x56, 0x28);
		break;
	default:
		break;
	}

	/* hdmi or dvi */
	if (hdmi_common_get_hdmi_ieee() == 0x000c03)
		adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 1, 0x01);
	else
		adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 1, 0x00);

	/* set audio mode: I2S */
	adv7525_write_reg_mask(hclient_main, 0x0a, 0x01, 4, 0x00);
	adv7525_write_reg_mask(hclient_main, 0x0c, 0x03, 0, 0x00);

	adv7525_set_spd_packet();
	adv7525_set_audio_info();
	adv7525_set_vs_info();
}

static void adv7525_handle_edid_intr(void)
{
	adv7525_read_edid();
	adv7525_set_av_mute(1);
	adv7525_tx_init();
	setup_audio_video();

	if (hdcp_on) {
		mutex_lock(&hdcp_state_mutex);
		hdcp_activating = TRUE;
		mutex_unlock(&hdcp_state_mutex);
		adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 7, 0x01);
		adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 4, 0x01);
	}
	adv7525_cec_init();
	adv7525_set_av_mute(0);
	del_timer(&hdmi_state_timer);
	change_hdmi_state(1);
}

static void adv7525_handle_rx_sense_intr(void)
{
	u8 reg0x42 = adv7525_read_reg(hclient_main, 0x42);

	DEV_INFO("%s: 0x42=%x\n", __func__, reg0x42);
	if ((reg0x42 & (1<< 5)) && (reg0x42 & (1<< 6))) {
		/* power up */
		adv7525_write_reg_mask(hclient_main, 0x41, 0x01, 6, 0x00);
		if (hdcp_on) {
			/* disable hdcp */
			adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 4, 0x00);
			adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 7, 0x00);
			adv7525_write_reg_mask(hclient_main, 0xd5, 0x01, 0, 0x00);

			/* set edid tries */
			adv7525_write_reg_mask(hclient_main, 0xc9, 0x0f, 0, 0x03);
		} else {
			/* set re-read bit */
			adv7525_write_reg_mask(hclient_main, 0xc9, 0x01, 4, 0x00);
			adv7525_write_reg_mask(hclient_main, 0xc9, 0x01, 4, 0x01);
		}
	}
}

static void adv7525_handle_hpd_intr(void)
{
	u8 reg0x42 = adv7525_read_reg(hclient_main, 0x42);

	DEV_INFO("%s: 0x42=%x\n", __func__, reg0x42);
	if (!(reg0x42 & (1<< 6))) {
		/* Power down oscillator */
		adv7525_write_reg_mask(hclient_main, 0xe6, 0x01, 7, 0x00);
		del_timer(&hdmi_state_timer);
		change_hdmi_state(0);
	} else {
		adv7525_write_reg_mask(hclient_main, 0x96, 0x01, 7, 0x01);/*?*/

		/* set interrupt bits */
		adv7525_write_reg_mask(hclient_main, 0x94, 0x01, 7, 0x01);
		adv7525_write_reg_mask(hclient_main, 0x94, 0x01, 6, 0x01);
		adv7525_write_reg_mask(hclient_main, 0x94, 0x01, 2, 0x01);
		adv7525_write_reg_mask(hclient_main, 0x95, 0x01, 7, 0x01);
		adv7525_write_reg_mask(hclient_main, 0x95, 0x01, 6, 0x01);

		/* power up Tx to read EDID w/o Rx Sense */
		adv7525_write_reg_mask(hclient_main, 0x41, 0x01, 6, 0x00);
		adv7525_write_reg_mask(hclient_main, 0xc9, 0x01, 4, 0x00);
		msleep(10);
		adv7525_write_reg_mask(hclient_main, 0xc9, 0x01, 4, 0x01);
		adv7525_write_reg_mask(hclient_main, 0xc9, 0x0f, 0, 0x05);

		reg0x42 = adv7525_read_reg(hclient_main, 0x42);
		if (reg0x42 & (1<< 5)) {
			adv7525_handle_rx_sense_intr();
			DEV_INFO("%s: Rx sense already high befor HPD\n", __func__);
		}

		dd->pd->cec_power(1);
		chip_power_on = TRUE;
		mod_timer(&hdmi_state_timer, jiffies + msecs_to_jiffies(1000));
	}
}

static void adv7525_handle_bksv_ready_intr(void)
{
	int bksv_count = adv7525_read_reg(hclient_main, 0xc7) & 0x7f;

	if (bksv_count == 0) {
		bksv[4] = adv7525_read_reg(hclient_main, 0xBF);
		bksv[3] = adv7525_read_reg(hclient_main, 0xC0);
		bksv[2] = adv7525_read_reg(hclient_main, 0xC1);
		bksv[1] = adv7525_read_reg(hclient_main, 0xC2);
		bksv[0] = adv7525_read_reg(hclient_main, 0xC3);

		DEV_INFO("BKSV={%02x,%02x,%02x,%02x,%02x}\n", bksv[4], bksv[3],
			bksv[2], bksv[1], bksv[0]);
		if (adv7525_read_reg(hclient_main, 0xaf) & (1 << 1))
			hdcp_bstatus[1] = 0x10;
	}
}

/* Power ON/OFF  ADV7525 chip */
static void adb7525_chip_init(void);
static irqreturn_t adv7525_interrupt(int irq, void *dev_id);
static void adv7525_isr(struct work_struct *work);

static int adv7525_power_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
	int rc;

	external_common_state->dev = &pdev->dev;
	if (mfd != NULL) {
		if (!external_common_state->uevent_kobj) {
			int ret = external_common_state_create(pdev);
			if (ret)
				return ret;
		}

		DEV_INFO("adv7525_power: ON (%dx%d %d)\n",
			mfd->var_xres, mfd->var_yres, mfd->var_pixclock);
		hdmi_common_get_video_format_from_drv_data(mfd);
	}

	wake_lock(&idle_wlock);
	gpio_power_on = TRUE;

	if (!chip_power_on) {
		adb7525_chip_init();
		adv7525_handle_hpd_intr();
	}

	if (!irq_en) {
		rc = request_irq(dd->pd->irq, &adv7525_interrupt,
				IRQF_TRIGGER_FALLING, "adv7525_cable", dd);
		if (rc) {
			pr_err("%s: fail to request irq\n", __func__);
			return rc;
		}
		DEV_INFO("%s: 'enable_irq'\n", __func__);
		irq_en = TRUE;
	}

	return 0;
}

static int adv7525_power_off(struct platform_device *pdev)
{
	DEV_INFO("%s: 'disable_irq', chip off, I2C off\n", __func__);
	adv7525_chip_off();
	monitor_plugin = 0;
	wake_unlock(&idle_wlock);
#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
	hdcp_activating = FALSE;
#endif
	return 0;
}

#if DEBUG
/* Read status registers for debugging */
static void adv7525_read_status(void)
{
	adv7525_read_reg(hclient_main, 0x94);
	adv7525_read_reg(hclient_main, 0x95);
	adv7525_read_reg(hclient_main, 0x97);
	adv7525_read_reg(hclient_main, 0xb8);
	adv7525_read_reg(hclient_main, 0xc8);
	adv7525_read_reg(hclient_main, 0x41);
	adv7525_read_reg(hclient_main, 0x42);
	adv7525_read_reg(hclient_main, 0xa1);
	adv7525_read_reg(hclient_main, 0xb4);
	adv7525_read_reg(hclient_main, 0xc5);
	adv7525_read_reg(hclient_main, 0x3e);
	adv7525_read_reg(hclient_main, 0x3d);
	adv7525_read_reg(hclient_main, 0xaf);
	adv7525_read_reg(hclient_main, 0xc6);

}
#else
#define adv7525_read_status() do {} while (0)
#endif


/* AV7525 chip specific initialization */
static void adb7525_chip_init(void)
{
	/* Initialize the variables used to read/write the ADV7525 chip. */
	memset(&reg, 0xff, sizeof(reg));

	/* Get the values from the "Fixed Registers That Must Be Set". */
	reg[0x98] = adv7525_read_reg(hclient_main, 0x98);
	reg[0x9c] = adv7525_read_reg(hclient_main, 0x9c);
	reg[0x9d] = adv7525_read_reg(hclient_main, 0x9d);
	reg[0xa2] = adv7525_read_reg(hclient_main, 0xa2);
	reg[0xa3] = adv7525_read_reg(hclient_main, 0xa3);
	reg[0xde] = adv7525_read_reg(hclient_main, 0xde);
	reg[0xe4] = adv7525_read_reg(hclient_main, 0xe4);
	reg[0xe5] = adv7525_read_reg(hclient_main, 0xe5);
	reg[0xe6] = adv7525_read_reg(hclient_main, 0xe6);
	reg[0xeb] = adv7525_read_reg(hclient_main, 0xeb);

	/* Get the "HDMI/DVI Selection" register. */
	reg[0xaf] = adv7525_read_reg(hclient_main, 0xaf);

	/* Hard coded values provided by ADV7525 data sheet. */
	reg[0x98] = 0x03;
	reg[0x99] = 0x02;
	reg[0x9A] = 0x00;
	reg[0x9B] = 0x18;
	reg[0x9c] = 0x38;
	reg[0x9d] = 0x61;
	reg[0xa2] = 0xA0;
	reg[0xa3] = 0xA0;
	reg[0xde] = 0x82;

	reg[0xe4] |= 0x44;
#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
	reg[0xe4] |= 0x02;
#endif
	reg[0xe5] |= 0x80;
	reg[0xe6] |= 0x1C;
	reg[0xe6] &= 0xFD;
	reg[0xeb] |= 0x02;

	/* Set the HDMI select bit. */
	reg[0xaf] |= 0x16;

	/* Set the audio related registers. */
	/* 0x01 to 0x03: Set N value to 6272 */
	reg[0x01] = 0x00;
	reg[0x02] = 0x18;
	reg[0x03] = 0x80;
	reg[0x0a] = 0x41;
	reg[0x0b] = 0x0e;
	reg[0x0c] = 0x84;
	reg[0x0d] = 0x18; /* Set I2S Bit Witdh to 24 bit */
	reg[0x12] = 0x00;
	reg[0x14] = 0x00;
	reg[0x15] = 0x20;
	reg[0x44] = 0x79;
	reg[0x73] = 0x01;
	reg[0x76] = 0x00;

	/* Set 720p display related registers */
	reg[0x16] = 0x00;
	reg[0x17] = 0x00;
	adv7525_write_reg(hclient_main, 0x17, reg[0x17]);

	reg[0x18] = 0x46;
	reg[0x55] = 0x02; /* Set PC mode */
	reg[0x3c] = 0x04;

	/* Set Interrupt Mask register for HPD/HDCP/EDID */
	reg[0x94] = 0xC4;
#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
	reg[0x95] = 0xC0;
#else
	reg[0x95] = 0x00;
#endif
	reg[0x95] |= 0x39;

	adv7525_write_reg(hclient_main, 0x94, reg[0x94]);
	adv7525_write_reg(hclient_main, 0x95, reg[0x95]);

	/* Set the values from the "Fixed Registers That Must Be Set". */
	adv7525_write_reg(hclient_main, 0x98, reg[0x98]);
	adv7525_write_reg(hclient_main, 0x99, reg[0x99]);
	adv7525_write_reg(hclient_main, 0x9a, reg[0x9a]);
	adv7525_write_reg(hclient_main, 0x9b, reg[0x9b]);
	adv7525_write_reg(hclient_main, 0x9c, reg[0x9c]);
	adv7525_write_reg(hclient_main, 0x9d, reg[0x9d]);
	adv7525_write_reg(hclient_main, 0xa2, reg[0xa2]);
	adv7525_write_reg(hclient_main, 0xa3, reg[0xa3]);
	adv7525_write_reg(hclient_main, 0xde, reg[0xde]);
	adv7525_write_reg(hclient_main, 0xe4, reg[0xe4]);
	adv7525_write_reg(hclient_main, 0xe5, reg[0xe5]);
	adv7525_write_reg(hclient_main, 0xe6, reg[0xe6]);
	adv7525_write_reg(hclient_main, 0xeb, reg[0xeb]);

	/* Set the "HDMI/DVI Selection" register. */
	adv7525_write_reg(hclient_main, 0xaf, reg[0xaf]);

	/* Set EDID Monitor address */
	reg[0x43] = 0x7E;
	adv7525_write_reg(hclient_main, 0x43, reg[0x43]);

	/* Enable the i2s audio input. */
	adv7525_write_reg(hclient_main, 0x01, reg[0x01]);
	adv7525_write_reg(hclient_main, 0x02, reg[0x02]);
	adv7525_write_reg(hclient_main, 0x03, reg[0x03]);
	adv7525_write_reg(hclient_main, 0x0a, reg[0x0a]);
	adv7525_write_reg(hclient_main, 0x0b, reg[0x0b]);
	adv7525_write_reg(hclient_main, 0x0c, reg[0x0c]);
	adv7525_write_reg(hclient_main, 0x0d, reg[0x0d]);
	adv7525_write_reg(hclient_main, 0x12, reg[0x12]);
	adv7525_write_reg(hclient_main, 0x14, reg[0x14]);
	adv7525_write_reg(hclient_main, 0x15, reg[0x15]);
	adv7525_write_reg(hclient_main, 0x44, reg[0x44]);
	adv7525_write_reg(hclient_main, 0x73, reg[0x73]);
	adv7525_write_reg(hclient_main, 0x76, reg[0x76]);

	/* Enable 720p display */
	adv7525_write_reg(hclient_main, 0x16, reg[0x16]);
	adv7525_write_reg(hclient_main, 0x18, reg[0x18]);
	adv7525_write_reg(hclient_main, 0x55, reg[0x55]);
	adv7525_write_reg(hclient_main, 0x3c, reg[0x3c]);
}

static void adv7525_isr(struct work_struct *work)
{
	u8 reg0x96, reg0x96_n;// = adv7525_read_reg(hclient_main, 0x96);
	u8 reg0x97, reg0x97_n;// = adv7525_read_reg(hclient_main, 0x97);
#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
	u8 reg0xc8;
#endif
	u8 rdata[17] = {0};
	u8 retry = 0;

	wake_lock(&suspend_wlock);
	reg0x96 = adv7525_read_reg(hclient_main, 0x96);
	reg0x97 = adv7525_read_reg(hclient_main, 0x97);
	DEV_INFO("%s: 0x96=%x, 0x97=%x\n",	__func__, reg0x96, reg0x97);

	do {
		/* Clearing the Interrupts */
		adv7525_write_reg(hclient_main, 0x96, 0xff);
		adv7525_write_reg(hclient_main, 0x97, 0xff);

		if (!chip_power_on)
			adb7525_chip_init();

		if(reg0x96 & 0x20)
		    adv7525_write_reg_mask(hclient_main, 0x94, 0x01, 5, 0x00);

		/* Rx sense intr */
		if (reg0x96 & 0x40)
			adv7525_handle_rx_sense_intr();

		/* HPD intr */
		if (reg0x96 & 0x80)
			adv7525_handle_hpd_intr();

		/* edid intr */
		if (reg0x96 & 0x04)
			adv7525_handle_edid_intr();

#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
		if (hdcp_activating) {
			/* HDCP controller error Interrupt */
			if (reg0x97 & 0x80) {
				u8 err = (adv7525_read_reg(hclient_main, 0xc8) &0xf0) >> 4;
				DEV_ERR("adv7525_irq: HDCP_ERROR, err=%x\n", err);
				adv7525_write_reg_mask(hclient_main, 0xd5, 0x01, 0, 0x01);
				adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 4, 0x00);
			/* BKSV Ready interrupts */
			} else if (reg0x97 & 0x40) {
				DEV_INFO("adv7525_irq: BKSV keys ready, Begin"
					" HDCP encryption\n");
				adv7525_handle_bksv_ready_intr();
			}

			/* HDCP_AUTH */
			if (reg0x96 & 0x02) {
				/* Restore video */
				adv7525_write_reg_mask(hclient_main, 0xd5, 0x01, 0, 0x00);
				adv7525_write_reg_mask(hclient_main, 0xaf, 0x01, 4, 0x01);
				mod_timer(&hdcp_link_timer, jiffies + msecs_to_jiffies(2000));
			}
			reg0xc8 = adv7525_read_reg(hclient_main, 0xc8);
			DEV_INFO("DDC controller reg[0xC8]=0x%02x\n", reg0xc8);
		} else
			DEV_INFO("adv7525_irq: final reg[0x96]=%02x reg[0x97]=%02x\n",
				reg0x96, reg0x97);
#endif

		/* CEC Rx Intr */
		if ((reg0x97 & 0x01)) {
			adv7525_i2c_read(hclient_cec, 0x15, rdata, 17);
			adv7525_cec_rx_handle(rdata);
			adv7525_write_reg(hclient_cec, 0x4a, 1);
			adv7525_write_reg(hclient_cec, 0x4a, 0);
		}

		reg0x96_n = adv7525_read_reg(hclient_main, 0x96);
		reg0x97_n = adv7525_read_reg(hclient_main, 0x97);
		reg0x96 = (reg0x96 & reg0x96_n) ^ reg0x96_n;
		reg0x97 = (reg0x97 & reg0x97_n) ^ reg0x97_n;
		if (!reg0x96 && !reg0x97) {
			if (dd->pd->intr_detect())
				break;
		}

		DEV_INFO("%s: clear interrupt pin, retry %d times\n",
			__func__, retry++);
	} while (retry < 5);

	wake_unlock(&suspend_wlock);
	enable_irq(dd->pd->irq);
}

static irqreturn_t adv7525_interrupt(int irq, void *dev_id)
{
	if (!gpio_power_on) {
		DEV_WARN("adv7525_irq: WARN: GPIO power off, skipping\n");
		return IRQ_HANDLED;
	}
	disable_irq_nosync(dd->pd->irq);
	schedule_work(&dd->work);
	return IRQ_HANDLED;
}

static const struct i2c_device_id adv7525_id[] = {
	{ ADV7525_DRV_NAME , 0},
	{}
};

static struct msm_fb_panel_data hdmi_panel_data = {
	.on  = adv7525_power_on,
	.off = adv7525_power_off,
};

static struct platform_device hdmi_device = {
	.name = ADV7525_DRV_NAME ,
	.id   = 2,
	.dev  = {
		.platform_data = &hdmi_panel_data,
		}
};

#ifdef CONFIG_HAS_EARLYSUSPEND
void adv7525_early_suspend(struct early_suspend *h)
{
	DEV_INFO("%s\n", __func__);

	disable_irq_nosync(dd->pd->irq);
	dd->pd->enable_5v(0);
	change_hdmi_state(0);
}

void adv7525_early_resume(struct early_suspend *h)
{
	DEV_INFO("%s\n", __func__);

	if (!gpio_power_on) {
		dd->pd->comm_power(1, 1);
		gpio_power_on = TRUE;
	}
	dd->pd->enable_5v(1);
	enable_irq(dd->pd->irq);

}
#endif

static int __devinit
adv7525_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc, i;
	struct platform_device *fb_dev;
	struct i2c_adapter *cec_adapter = i2c_get_adapter(0);
	struct i2c_adapter *packet_adapter = i2c_get_adapter(0);

	dd = kzalloc(sizeof *dd, GFP_KERNEL);
	if (!dd) {
		rc = -ENOMEM;
		goto probe_exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	external_common_state->dev = &client->dev;

	/* Init real i2c_client */
	hclient_main = client;
	hclient_cec = i2c_new_dummy(cec_adapter, 0x3C);
	hclient_packet = i2c_new_dummy(packet_adapter, 0x38);

	i2c_set_clientdata(client, dd);
	dd->pd = client->dev.platform_data;
	if (!dd->pd) {
		rc = -ENODEV;
		goto probe_free;
	}

	/* Input register */
	dd->input = input_allocate_device();
	if (!dd->input) {
		pr_err("%s: input_allocate_device failed!\n", __func__);
		rc = -ENOMEM;
		goto probe_free;
	}

	dd->input->name = ADV7525_DRV_NAME;
	set_bit(EV_KEY, dd->input->evbit);
	for (i = 0; i < MAX_KEYCODE; i++)
		input_set_capability(dd->input, EV_KEY, i);

	rc = input_register_device(dd->input);
	if (rc) {
		pr_err("%s input_register_device failed!\n", __func__);
		goto probe_free;
	}

	dd->pd->comm_power(1, 1);
	dd->pd->enable_5v(1);
	dd->pd->init_irq();

	INIT_WORK(&dd->work, adv7525_isr);
#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
	INIT_WORK(&hdcp_link_work, adv7525_hdcp_link_work);
	setup_timer(&hdcp_link_timer, adv7525_hdcp_link_timer_expired, 0);
#endif
	setup_timer(&hdmi_state_timer, adv7525_state_timer_expired, 0);

	fb_dev = msm_fb_add_device(&hdmi_device);
	if (!fb_dev)
		DEV_ERR("adv7525_probe: failed to add fb device\n");

#ifdef CONFIG_FB_MSM_HDMI_ADV7525_PANEL_HDCP_SUPPORT
	if (device_create_file(&client->dev, &hdmi_hdcp_enable_attrs))
		pr_err("%s: device_create_file hdmi_hdcp_enable_attrs error\n", __func__);
#endif
	if (device_create_file(&client->dev, &hdmi_resolution_attrs))
		pr_err("%s: device_create_file hdmi_hdcp_enable_attrs error\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	dd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 2;
	dd->early_suspend.suspend = adv7525_early_suspend;
	dd->early_suspend.resume = adv7525_early_resume;
	register_early_suspend(&dd->early_suspend);
#endif

	return 0;

probe_free:
	i2c_set_clientdata(client, NULL);
	kfree(dd);
probe_exit:
	return rc;

}

static int __devexit adv7525_remove(struct i2c_client *client)
{
	int err = 0;
	if (!client->adapter) {
		DEV_ERR("%s: No HDMI Device\n", __func__);
		return -ENODEV;
	}
	wake_lock_destroy(&idle_wlock);
	wake_lock_destroy(&suspend_wlock);
	return err;
}

#ifdef CONFIG_SUSPEND
static int adv7525_i2c_suspend(struct device *dev)
{
	DEV_INFO("%s\n", __func__);

	gpio_power_on = FALSE;
	dd->pd->comm_power(0, 0);

	return 0;
}

static int adv7525_i2c_resume(struct device *dev)
{
	DEV_INFO("%s\n", __func__);

	return 0;
}
#else
#define adv7525_i2c_suspend	NULL
#define adv7525_i2c_resume	NULL
#endif

static const struct dev_pm_ops adv7525_device_pm_ops = {
	.suspend = adv7525_i2c_suspend,
	.resume = adv7525_i2c_resume,
};

static struct i2c_driver hdmi_i2c_driver = {
	.driver		= {
		.name   = ADV7525_DRV_NAME,
		.pm     = &adv7525_device_pm_ops,
	},
	.probe		= adv7525_probe,
	.id_table	= adv7525_id,
	.remove		= __devexit_p(adv7525_remove),
};

static int __init adv7525_init(void)
{
	int rc;

	external_common_state = &hdmi_common;
	external_common_state->video_resolution = HDMI_VFRMT_1280x720p60_16_9;
	HDMI_SETUP_LUT(640x480p60_4_3);
	HDMI_SETUP_LUT(720x480p60_4_3);
	HDMI_SETUP_LUT(720x480p60_16_9);
	HDMI_SETUP_LUT(1280x720p60_16_9);

	hdmi_common_init_panel_info(&hdmi_panel_data.panel_info);

	rc = i2c_add_driver(&hdmi_i2c_driver);
	if (rc) {
		pr_err("hdmi_init FAILED: i2c_add_driver rc=%d\n", rc);
		goto init_exit;
	}

	wake_lock_init(&idle_wlock, WAKE_LOCK_IDLE, "hdmi_idle_wake_lock");
	wake_lock_init(&suspend_wlock, WAKE_LOCK_SUSPEND, "hdmi_suspend_wake_lock");

	return 0;

init_exit:
	return rc;
}

static void __exit adv7525_exit(void)
{
	i2c_del_driver(&hdmi_i2c_driver);
}

module_init(adv7525_init);
module_exit(adv7525_exit);
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_AUTHOR("Peng Chang");
MODULE_DESCRIPTION("ADV7525 HDMI driver for Acer Platform");

/* drivers/misc/msp430_update.c - mcu msp430 firmware update driver
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

#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>

static struct i2c_client *priv_client;

#define vaild_data(x) CharToHex(x)
static int CharToHex(char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	else if ((c >= 'A') && (c <= 'F'))
		return c - 'A' + 10;
	else if ((c >= 'a') && (c <= 'f'))
		return c - 'a' + 10;
	else
		return -1;
}

static int HexToChar(int hex)
{
	if ((hex >= 0) && (hex <= 9))
		return hex + '0';
	else if ((hex >= 0xa) && (hex <= 0xf))
		return hex + '7';
	else
		return -1;
}

static int check_img(unsigned char *data)
{
	if (!strncmp("@0200", data, 5))
		return 0;
	return -1;
}

static int write_to_mcu(unsigned char *data, unsigned int size)
{
	int count = size / 128;
	int remain = size % 128;
	int i = 0;

	while (i < count) {
		if (128 != i2c_master_send(priv_client, data+i*128, 128)) {
			pr_info("%s:i2c error.\n", __func__);
			return -1;
		}
		i++;
	}

	if (remain != i2c_master_send(priv_client, data+i*128, remain)) {
		pr_info("%s:i2c error.\n", __func__);
		return -1;
	}
	return 0;
}

static int flash_addr(unsigned char *data)
{
	int i = 0;
	int addr = 0;
	unsigned char buf[5];

	buf[0] = '@';
	for (i = 1; i < 5; i++)
		buf[i] = data[i];

	if (write_to_mcu(buf, 5) < 0)
		return -1;

	/*change addr from char to hex*/
	i = CharToHex(buf[4]);
	addr = i;
	i = CharToHex(buf[3]);
	addr |= i<<4;
	i = CharToHex(buf[2]);
	addr |= i<<8;
	i = CharToHex(buf[1]);
	addr |= i<<12;
	return addr;
}

static int flash_first_block(unsigned char *data)
{
	int i = 0;
	unsigned int cnt = 0;
	char *local_buf;

	if (data[cnt] == '@')
		flash_addr(data);
	else
		return -1;
	cnt += 5; /*addr cost 5 spaces*/

	local_buf = kmalloc(1024, GFP_KERNEL);
	if (local_buf == NULL) {
		pr_info("%s: kernel memory alloc error\n", __func__);
		return -1;
	}

	/* collect data to buf until met the next '@'*/
	while (data[cnt] != '@') {
		if (vaild_data(data[cnt]) >= 0) {
			local_buf[i] = data[cnt];
			i++;
			if (i >= 1024) {
				kfree(local_buf);
				return -1;
			}
		}
		cnt++;
	}

	if (write_to_mcu(local_buf, i) < 0) {
		kfree(local_buf);
		return -1;
	}

	kfree(local_buf);
	return cnt;
}

/*write data each 1024bytes*/
static int flash_block(unsigned char *data, unsigned int max_size)
{
	unsigned int cnt = 0;
	unsigned char cmd[5];
	unsigned int addr = 0;
	char *local_buf;
	int i = 0;

	if (data[cnt] == '@')
		addr = flash_addr(data);
	else
		return -1;
	cnt += 5;

	local_buf = kmalloc(1024, GFP_KERNEL);
	if (local_buf == NULL) {
		pr_info("%s: kernel memory alloc error\n", __func__);
		return -1;
	}

	while ((data[cnt] != 'q') && (data[cnt] != 'Q')) {
		if (data[cnt] == '@' || (i >= 1024)) {
			if (i) {
				if (write_to_mcu(local_buf, i) < 0)
					goto failed_out;

				cmd[0] = 'S';
				if (write_to_mcu(cmd, 1) < 0)
					goto failed_out;
				mdelay(100);
			}

			if (data[cnt] == '@') {
				addr = flash_addr(data + cnt);
				cnt += 5;
			} else {
				/*two char bytes cost 1 'real' byte in mcu*/
				addr += (i/2);

				cmd[0] = '@';
				cmd[4] = HexToChar(addr & 0xf);
				cmd[3] = HexToChar((addr & 0xf0) >> 4);
				cmd[2] = HexToChar((addr & 0xf00) >> 8);
				cmd[1] = HexToChar((addr & 0xf000) >> 12);

				if (write_to_mcu(cmd, 5) < 0)
					goto failed_out;
			}
			i = 0;
		}

		if (vaild_data(data[cnt]) >= 0) {
			local_buf[i] = data[cnt];
			i++;
		}

		cnt++;
		if (cnt >= max_size)
			goto failed_out;
	}

	if (i) {
		if (write_to_mcu(local_buf, i) < 0)
			goto failed_out;

		cmd[0] = 'S';
		if (write_to_mcu(cmd, 1) < 0)
			goto failed_out;

		cmd[0] = 'Q';
		if (write_to_mcu(cmd, 1) < 0)
			goto failed_out;
		mdelay(100);
	}

	kfree(local_buf);
	return 0;

failed_out:
	kfree(local_buf);
	return -1;
}

static int flash_mcu(unsigned char *data, unsigned int sz)
{
	unsigned int err_step = 0;
	int ptr = 0;
	unsigned char buf[5];

	if (check_img(data) < 0) {
		pr_info("Image format error!\n");
		return -1;
	}

	/*step1: flash firmware password: 0x7E, 0x55, 0xAA*/
	err_step = 1;
	buf[0] = 0x7e;
	buf[1] = 0x55;
	buf[2] = 0xaa;
	if (write_to_mcu(buf, 3) < 0)
		goto write_mcu_failed;

	/*step2: delay 500ms*/
	mdelay(550);

	/*step3: send 'G', start update flash write data*/
	err_step = 2;
	buf[0] = 'G';
	if (write_to_mcu(buf, 1) < 0)
		goto write_mcu_failed;

	/*step4: send data of @0x200*/
	err_step = 3;
	if ((ptr = flash_first_block(data)) < 0)
		goto write_mcu_failed;

	/*step5: delay 50ms*/
	mdelay(60);

	/*step6: send 'K'*/
	err_step = 4;
	buf[0] = 'K';
	if (write_to_mcu(buf, 1) < 0)
		goto write_mcu_failed;

	/*step7: delay 900ms*/
	mdelay(1000);

	/*step8: start to burn whole data*/
	err_step = 5;
	if (flash_block(data + ptr, sz - ptr) < 0)
		goto write_mcu_failed;

	mdelay(1000);
	return 0;

write_mcu_failed:
	pr_info("MCU firmware write error:%d!\n", err_step);
	return -1;
}

int is_mcu_need_update(struct i2c_client *client,
	const char *filename, char cur_version)
{
	struct file *filp;
	struct inode *inode = NULL;
	mm_segment_t oldfs;
	char firmware_ver[2];
	int length = 0, ret = 0;

	/*0xff, mcu firmware is broken, need update*/
	if (cur_version == 0xff)
		return 1;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(filename, O_RDONLY, S_IRUSR);
	if (IS_ERR(filp)) {
		pr_info("%s: file %s filp_open error!\n", __func__, filename);
		set_fs(oldfs);
		return -1;
	}

	if (!filp->f_op) {
		pr_info("%s: File Operation Method Error\n", __func__);
		ret = -1;
		goto out1;
	}

	inode = filp->f_path.dentry->d_inode;
	if (!inode) {
		pr_info("%s: Get inode from filp failed\n", __func__);
		ret = -1;
		goto out1;
	}

	length = i_size_read(inode->i_mapping->host);
	if (length == 0) {
		pr_info("%s: Try to get file size error\n", __func__);
		ret = -1;
		goto out1;
	}

	/* read image version*/
	if (generic_file_llseek(filp, length-16, SEEK_SET) < 0) {
		pr_info("%s: image seek set error\n", __func__);
		ret = -1;
		goto out1;
	}

	if (filp->f_op->read(filp, firmware_ver, 2, &filp->f_pos) != 2) {
		pr_info("%s: file read error\n", __func__);
		ret = -1;
		goto out1;
	}

	firmware_ver[0] = (char)CharToHex(firmware_ver[0])<<4;
	firmware_ver[0] |= (char)CharToHex(firmware_ver[1]);
	if (firmware_ver[0] > cur_version)
		ret = 1;
	else
		ret = 0;
out1:
	filp_close(filp, NULL);
	set_fs(oldfs);
	return ret;
}

int update_mcu_firwmare(struct i2c_client *client,
	const char *filename, char cur_version)
{
	struct file *filp;
	struct inode *inode = NULL;
	int length = 0, ret = 0;
	char *fw_buf = NULL;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(filename, O_RDONLY, S_IRUSR);
	if (IS_ERR(filp)) {
		pr_info("%s: file %s filp_open error!\n", __func__, filename);
		set_fs(oldfs);
		return -1;
	}

	if (!filp->f_op) {
		pr_info("%s: File Operation Method Error\n", __func__);
		ret = -1;
		goto out1;
	}

	inode = filp->f_path.dentry->d_inode;
	if (!inode) {
		pr_info("%s: Get inode from filp failed\n", __func__);
		ret = -1;
		goto out1;
	}

	length = i_size_read(inode->i_mapping->host);
	if (length == 0) {
		pr_info("%s: Try to get file size error\n", __func__);
		ret = -1;
		goto out1;
	}

	priv_client = client;

	fw_buf = kmalloc((length + 1), GFP_KERNEL);
	if (fw_buf == NULL) {
		pr_info("%s: kernel memory alloc error\n", __func__);
		ret = -1;
		goto out1;
	}

	if (generic_file_llseek(filp, 0, SEEK_SET) < 0) {
		pr_info("%s: image seek set head error\n", __func__);
		ret = -1;
		goto out;
	}

	if (filp->f_op->read(filp, fw_buf, length, &filp->f_pos) != length) {
		pr_info("%s: file read error\n", __func__);
		ret = -1;
		goto out;
	}

	pr_info("Start to update mcu firmware.....\n");
	if (flash_mcu(fw_buf, length) < 0) {
		pr_info("%s: flash_mcu error\n", __func__);
		ret = -1;
		goto out;
	}
	ret = 1;

out:
	kfree(fw_buf);
out1:
	filp_close(filp, NULL);
	set_fs(oldfs);
	if (ret < 0)
		pr_err("Update mcu firmware error!Skip...\n");
	return ret;
}


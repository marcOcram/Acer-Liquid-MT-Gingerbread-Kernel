/* drivers/i2c/chips/msp430.c - mcu sp430 driver
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
 *  mcu version history:
 *    0x14: change gague interrupt read/write order
 *    0x15: needs enable LED_LOW_BATTERY to flash low battery led
 *    0x16: add new event: battzero
 *          use to wakeup device when battery capacity zero
 * 	  0x18: adjust low battery flash cycle(6s -> 8s)
 *          added new mode: reserved mode
 *             reserved mode-> control charging led by MCU
 *             normal mode-> control charging led by driver
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/msp430.h>
#include <linux/time.h>
#include <linux/i2c/smb136.h>
#include <linux/acer_battery_framework.h>
#include "../../arch/arm/mach-msm/smd_private.h"

#include "msp430_update.c"

#define POLLING_AVG_CURRENT

#define RETRY_COUNT                 4	/* I2C retry count */

#define BATTERY_LOW_LEVEL           15
#define TEMP_WARN_HIGH              45
#define TEMP_WARN_HIGH_CANCEL       43
#define TEMP_WARN_LOW               0
#define TEMP_WARN_LOW_CANCEL        0
#define MCU_FIRMWARE_FILE           "/system/etc/a5_mcu_fw.txt"

#define POLLING_TIME                30000

#define IRQ_DEFAULT_ENABLE	(INT_QWKEY_EVENT | INT_GAUGE_EVENT | \
			INT_BATLOSS_EVENT | INT_BATCAP_EVENT)

/*If power off too early, the modem will crash. Hence report the battery
 *lost event at least after TIME_ENABLE_BATT_LOST_AFTER_BOOT seconds.*/
#define TIME_ENABLE_BATT_LOST_AFTER_BOOT	20 /*seconds*/

struct msp430_data {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct mutex mutex;
	struct mutex i2c_mutex;
	int keymap_size;
	char old_keyidx;
	char version;
	char led_status;
	int is_mcu_ready;
	int is_gauge_info;
	int is_battery_cap_need_retry;
	int is_first_read_cap_after_resume;
	int is_need_recharge_wakelock;
	int is_hw_key_wakeup;
	int temp_warn_led;
	int cap_zero_count;
	acer_hw_version_t hw_ver;

	int gauge_data_ready;
	wait_queue_head_t gauge_wait;
	struct mutex gauge_mutex;
	struct _batt_info batt_info;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct wake_lock cap_zero_wlock;
	struct wake_lock mcu_update_wlock;

	/* led_buf is used for mcu updating process
	 * 0:REG_LED_CTL 1:REG_CAHRGER_STATUS 2:REG_KEYPAD_PWM
	 */
	char led_buf[3];

	struct timer_list avg_timer;
	struct work_struct avg_work;

	void (*battery_isr_hander)(unsigned int flag);
	int (*get_charger_type)(void);
};

static const unsigned int msp430_keymap[] = {
	0,          /* shift base */
	229,        /* Menu */
	KEY_BACK,
	KEY_SEARCH,
	KEY_HOME,
};

static struct msp430_data *priv_data;

static int mcu_read(struct i2c_client *client, char *buf, unsigned int len)
{
	int retry = 1;
	char reg = buf[0];

	while (retry < RETRY_COUNT) {
		msleep(retry);
		if (1 == i2c_master_send(client, &reg, 1))
			break;
		pr_err("write reg:0x%x failed! retry it.\n", reg);
		retry++;
	}

	if (retry == RETRY_COUNT) {
		pr_err("%s: failed!! at addr = 0x%x\n", __func__, reg);
		return -EIO;
	}

	retry = 1;
	while (retry < RETRY_COUNT) {
		mdelay(2);
		if (len == i2c_master_recv(client, buf, len))
			break;
		pr_err("read reg:0x%x failed! retry it.\n", reg);
		retry++;
	}

	if (retry == RETRY_COUNT) {
		pr_err("%s: failed!! at addr = 0x%x\n", __func__, reg);
		return -EIO;
	}
	return 0;
}

static int mcu_write(struct i2c_client *client, char reg, char data)
{
	char i2c_buf[2] = {0};
	int retry = 1;

	while (retry < RETRY_COUNT) {
		i2c_buf[0] = reg;
		i2c_buf[1] = data;
		msleep(retry);
		if (2 == i2c_master_send(client, i2c_buf, 2))
			break;

		pr_err("mcu i2c failed!! at addr = 0x%x! retry it.\n", reg);
		retry++;
	}

	if (retry == RETRY_COUNT) {
		pr_err("%s: failed!! at addr = 0x%x\n", __func__, reg);
		return -EIO;
	}
	return 0;
}

static int mcu_access(struct i2c_client *client, char *buf,
	char len, int fn)
{
	int ret = 0;

	if (!priv_data) {
		pr_info("%s: mcu is not ready!\n", __func__);
		ret = -1;
		goto out;
	}

	if (!buf || (len < 0)) {
		pr_err("%s: wrong parameter!\n", __func__);
		ret = -1;
		goto out;
	}

	mutex_lock(&priv_data->i2c_mutex);
	if (fn == MCU_READ)
		ret = mcu_read(client, buf, len);
	else if (fn == MCU_WRITE)
		ret = mcu_write(client, buf[0], buf[1]);
out:
	mutex_unlock(&priv_data->i2c_mutex);
	return ret;
}

static inline int reg_read(struct i2c_client *client, char reg, char *data)
{
	if (!data)
		return -EINVAL;
	data[0] = reg;
	return mcu_access(client, data, 1, MCU_READ);
}

static inline int reg_write(struct i2c_client *client, char reg, char data)
{
	char buf[2];
	buf[0] = reg;
	buf[1] = data;
	return mcu_access(client, buf, 1, MCU_WRITE);
}

int led_control(int led_event, int on_off)
{
	char reg, status, buf;
	int ret;
	static int android_light_framework_ready;

	if (!priv_data) {
		pr_err("%s:priv_data Null pointer!!\n", __func__);
		return -EAGAIN;
	}

	if (on_off && (on_off != MCU_LED_ON))
		return -EINVAL;

	switch (led_event) {
	case LED_NEW_EVENT:
		reg = REG_LED_CTL;
		reg_read(priv_data->client, REG_LED_CTL, &buf);
		buf &= 0x30;
		if (priv_data->version >= 0x15)
			status = (LED_NEW_EVENT & on_off) | buf;
		else
			status = LED_NEW_EVENT & on_off;
		priv_data->led_buf[0] = status;
		break;
	case LED_BATTERY_CHARGE:
		reg = REG_CAHRGER_STATUS;
		status = LED_BATTERY_CHARGE & on_off;
		if (status == priv_data->led_buf[1])
			return 0;

		priv_data->led_buf[1] = status;
		break;
	case LED_BATTERY_CHARGE_COMPLETED:
		reg = REG_CAHRGER_STATUS;
		status = LED_BATTERY_CHARGE_COMPLETED & on_off;
		if (status == priv_data->led_buf[1])
			return 0;

		priv_data->led_buf[1] = status;
		break;
	case LED_KEYPAD:
		if ((priv_data->version > 0x18) && !android_light_framework_ready
			&& priv_data->is_mcu_ready) {
			/* After android light framework is ready, control charging led by i2c*/
			android_light_framework_ready = 1;

			reg_write(priv_data->client, REG_CAHRGER_STATUS, priv_data->led_buf[1]);

			ret = reg_write(priv_data->client, REG_LED_CTL,
				(priv_data->led_buf[0] & 0xcf) | SYS_MODE_NORMAL);
			if (ret < 0)
				pr_err("mcu: enter normal mode error!!!\n");
			else
				pr_info("mcu: enter normal mode!\n");
		}

		reg = REG_KEYPAD_PWM;
		status = LED_KEYPAD & on_off;
		if (status == priv_data->led_buf[2])
			return 0;

		priv_data->led_buf[2] = status;
		break;
	default:
		return -EINVAL;
	}

	if (!priv_data->is_mcu_ready) {
		pr_err("%s:mcu is not ready!!\n", __func__);
		return 0;
	}

	if (priv_data->temp_warn_led && (reg == REG_CAHRGER_STATUS))
		return 0;
	if (reg_write(priv_data->client, reg, status) < 0)
		return -EIO;
	return 0;
}
EXPORT_SYMBOL(led_control);

static int rw_gauge_reg(char rw, char addr, char data, char *buf)
{
	struct i2c_client *client;
	int ret = 0;
	int timeout;

	if (!priv_data->is_mcu_ready) {
		pr_err("read_gauge_reg:mcu is not ready!!\n");
		return -EIO;
	} else
		client = priv_data->client;

	if ((rw == GAUGE_READ) && !buf)
		return -EINVAL;

	mutex_lock(&priv_data->gauge_mutex);
	priv_data->gauge_data_ready = 0;

	if ((rw == GAUGE_WRITE) && (priv_data->version >= 0x14))
		if (reg_write(client, REG_BATT_GAUGE_DAT, data) < 0) {
			ret = -1;
			goto failed;
		}

	if (reg_write(client, REG_BATT_GAUGE_ADR, rw|addr) < 0) {
		ret = -1;
		goto failed;
	}

	timeout = msecs_to_jiffies(30);
	wait_event_timeout(priv_data->gauge_wait,
		priv_data->gauge_data_ready == 1, timeout);
	if (!timeout)
		ret = -1;
	else if (rw == GAUGE_WRITE)
		if (priv_data->version < 0x14)
			reg_write(client, REG_BATT_GAUGE_DAT, data);
		else
			ret = 0;
	else
		ret = reg_read(client, REG_BATT_GAUGE_DAT, buf);

failed:
	mutex_unlock(&priv_data->gauge_mutex);
	return ret;
}

static void read_gauge_info(void)
{
	char buf, buf1;

	rw_gauge_reg(GAUGE_WRITE, 0x00, 0x02, &buf);
	rw_gauge_reg(GAUGE_WRITE, 0x01, 0x00, &buf);
	rw_gauge_reg(GAUGE_READ, 0x01, 0x00, &buf);
	rw_gauge_reg(GAUGE_READ, 0x00, 0x00, &buf1);
	pr_info("gauge core version:%x.%x\n", buf, buf1);

	/* read gauge block A 0 */
	rw_gauge_reg(GAUGE_WRITE, 0x61, 0x00, &buf);
	rw_gauge_reg(GAUGE_WRITE, 0x3E, 0x39, &buf);
	rw_gauge_reg(GAUGE_WRITE, 0x3F, 0x00, &buf);
	rw_gauge_reg(GAUGE_READ, 0x40, 0x00, &buf);
	pr_info("gauge DFI version:0x%x\n", buf);

	/* read battery IT algorithm */
	rw_gauge_reg(GAUGE_WRITE, 0x00, 0x00, &buf);
	rw_gauge_reg(GAUGE_WRITE, 0x01, 0x00, &buf);
	rw_gauge_reg(GAUGE_READ, 0x00, 0x00, &buf);
	pr_info("gauge IT algorithm enabled:%d\n", buf & GAUGE_IT_EN_MASK);
}

static int _get_battery_info(struct _batt_info *batt_info)
{
	if (!priv_data->is_mcu_ready) {
		pr_err("read_gauge_reg:mcu is not ready!!\n");
		return -EIO;
	}

	if (!batt_info) {
		pr_err("get_battery_info get wrong parameter!!\n");
		return -EINVAL;
	}

	batt_info->cap_percent = priv_data->batt_info.cap_percent;
	batt_info->temperature = priv_data->batt_info.temperature;
	batt_info->voltage = priv_data->batt_info.voltage;

	/* charger is removed, disable high temperature warning led*/
	if (priv_data->get_charger_type &&
		(priv_data->temp_warn_led == 1) &&
		(priv_data->get_charger_type() == CHARGER_TYPE_NONE)) {
		reg_write(priv_data->client, REG_CAHRGER_STATUS, priv_data->led_buf[1]);
		priv_data->temp_warn_led = 0;
	}
	return 0;
}

static int is_capacity_make_sense(unsigned char cur_cap, unsigned char prev_cap)
{
	static int retry_count;

	if (cur_cap > 100)
		return 0;

	/* 0%, dobule check */
	if (!cur_cap) {
		priv_data->cap_zero_count++;
		if (priv_data->cap_zero_count >= 2)
			return 1;
		else {
			pr_info("BATT: 0%% count:%d\n", priv_data->cap_zero_count);
			return 0;
		}
	} else
		priv_data->cap_zero_count = 0;

	if (priv_data->is_first_read_cap_after_resume) {
		priv_data->is_first_read_cap_after_resume = 0;
	} else {
		if (((cur_cap - prev_cap) > 10) || ((prev_cap - cur_cap) > 10)) {
			retry_count++;
			pr_info("BATT: cur_cap:%d%% prev_cap:%d%%. Retry:%d.\n",
				cur_cap, prev_cap, retry_count);

			/* It should not get wrong capacity more than 5 times */
			if (retry_count > 5)
				return 1;
			else
				return 0;
		} else
			retry_count = 0;
	}
	return 1;
}

/* If the temperature is too high, device will auto shutdown. So we need double check.
 * (android framework default shutdown temperature: 68)
 */
static short temperature_filter(short temperature)
{
	static int high_temp_counter;
	static short old_temp;

	if (temperature >= TEMP_WARN_HIGH) {
		high_temp_counter++;
		pr_info("High temperature::%d count:%d\n", temperature, high_temp_counter);
		if (high_temp_counter >= 4)
			old_temp = temperature;
	} else {
		high_temp_counter = 0;
		old_temp = temperature;
	}
	return old_temp;
}

static int battery_info_change(void)
{
	struct i2c_client *client;
	struct _batt_info *batt_val = &priv_data->batt_info;
	unsigned char battery_capacity = 255;
	char buf[7];
	static int is_full_charger_region = 0;

	if (!priv_data->is_mcu_ready) {
		pr_err("get_battery_info:priv data is not ready!!\n");
		return -EIO;
	} else
		client = priv_data->client;

	buf[0] = REG_BATT_SOC;
	if (mcu_access(client, buf, 7, MCU_READ) >= 0)
		battery_capacity = buf[0];
	else {
		pr_err("get REG_BATT_SOC failed!!\n");
		priv_data->is_battery_cap_need_retry = 1;
		return 0;
	}

	if (!is_capacity_make_sense(battery_capacity, batt_val->cap_percent)) {
		pr_err("BATT: real capacity->%d but report previous.\n", battery_capacity);
		priv_data->is_battery_cap_need_retry = 1;
		return 0;
	}

	if (priv_data->get_charger_type) {
		if (priv_data->get_charger_type() == CHARGER_TYPE_NONE) {
			/* no charger, battery capacity avoid increase
			 * and the threshold is 3%*/
			if (((battery_capacity - batt_val->cap_percent) > 0) &&
				((battery_capacity - batt_val->cap_percent) < 4)) {
				pr_info("%s:real batt capacity is %d%% but report %d%%.\n",
					__func__, battery_capacity, batt_val->cap_percent);
				battery_capacity = batt_val->cap_percent;
			}

			priv_data->is_need_recharge_wakelock = 0;
			is_full_charger_region = 0;
		} else {
			/*charger in, if the battery capacity is 100%
			 * dont decrease capacity until the value is under 96 or
			 * no charger */
			if (is_full_charger_region) {
				if ((battery_capacity >= 96) && (battery_capacity != 100)) {
					pr_info("%s:real batt capacity is %d%% but report %d%%.\n",
						__func__, battery_capacity, batt_val->cap_percent);
					battery_capacity = batt_val->cap_percent;

					if (smb136_control(RD_FULL_CHARGING, 0)) {
						pr_info("Charging termination. re-charging enable.\n");
						smb136_recharge();
					}
				} else
					is_full_charger_region = 0;
			}
		}
	}

	batt_val->cap_percent = battery_capacity;
	batt_val->temperature = temperature_filter((buf[4]<<8) | buf[3]);
	batt_val->voltage = (buf[6]<<8) | buf[5];
	if (batt_val->cap_percent == 100)
		is_full_charger_region = 1;

	pr_info("battery cap:%d%% temperature:%d voltage:%d mv\n",
		batt_val->cap_percent, batt_val->temperature, batt_val->voltage);
	return 1;
}

static inline int temperature_warn_check(short temp, int is_warn)
{
	int ret;
	if (!is_warn) {
		ret = 0;
		if ((temp > TEMP_WARN_HIGH) || (temp < TEMP_WARN_LOW))
			ret = 1;
	} else {
		ret = 1;
		if ((temp < TEMP_WARN_HIGH_CANCEL) && (temp > TEMP_WARN_LOW_CANCEL))
			ret = 0;
	}
	return ret;
}

static void avg_timer_work(struct work_struct *work)
{
	char buf[2];

	if (!priv_data->is_mcu_ready) {
		pr_err("%s:mcu is not ready!!\n", __func__);
		mod_timer(&priv_data->avg_timer, jiffies + msecs_to_jiffies(POLLING_TIME));
		return;
	}

	if (!priv_data->is_gauge_info) {
		read_gauge_info();
		priv_data->is_gauge_info = 1;
	}

	if (priv_data->is_battery_cap_need_retry) {
		pr_info("BATT:polling battery capacity\n");
		priv_data->is_battery_cap_need_retry = 0;
		battery_info_change();
		if (priv_data->battery_isr_hander)
			priv_data->battery_isr_hander(INT_BATCAP_EVENT);
		else
			pr_err("Battery cap change but no handler registered.\n");
	}

#ifdef POLLING_AVG_CURRENT
	buf[0] = REG_BATT_AVG_CUR_L;
	if (mcu_access(priv_data->client, buf, 2, MCU_READ) >= 0)
		pr_info("battery avg_current:%dmA\n", (buf[1]<<8) | buf[0]);
	else
		pr_err("get REG_BATT_AVG_CUR_L failed!!\n");
#endif

	buf[0] = REG_BATT_TEMP_L;
	if (mcu_access(priv_data->client, buf, 2, MCU_READ) >= 0)
		priv_data->batt_info.temperature = temperature_filter((buf[1]<<8) | buf[0]);
	else
		pr_err("get REG_BATT_TEMP_L failed!!\n");

	if (priv_data->battery_isr_hander)
			priv_data->battery_isr_hander(FLAG_BATT_CAP_CHANGE);

	pr_debug("temperature report:%d\n", priv_data->batt_info.temperature);

	/*if temperature is abnormal, show warning led */
	if (priv_data->get_charger_type &&
		priv_data->get_charger_type() != CHARGER_TYPE_NONE) {
		if (temperature_warn_check(priv_data->batt_info.temperature, priv_data->temp_warn_led)) {
			pr_info("temperature warning:%d!!!\n", priv_data->batt_info.temperature);
			reg_write(priv_data->client, REG_CAHRGER_STATUS, 0x7);
			priv_data->temp_warn_led = 1;
		} else{
			reg_write(priv_data->client, REG_CAHRGER_STATUS, priv_data->led_buf[1]);
			priv_data->temp_warn_led = 0;
		}
	}

	mod_timer(&priv_data->avg_timer, jiffies + msecs_to_jiffies(POLLING_TIME));
}

static void avg_timer_expired(unsigned long unused)
{
	schedule_work(&priv_data->avg_work);
}

void report_msp430_key(struct msp430_data *data, char new_keyidx)
{
	int new_idx = new_keyidx;
	int old_idx = data->old_keyidx;

	if (new_idx > (data->keymap_size - 1)) {
		pr_err("%s: keyevent(%d) is over than"
				"keympa_size\n", __func__, new_idx);
		return;
	}

	if (new_idx != old_idx)
		input_report_key(data->input, msp430_keymap[old_idx], 0);

	if (new_idx)
		input_report_key(data->input, msp430_keymap[new_idx], 1);

	data->old_keyidx = new_keyidx;
}

static int event_double_check_thd(void *ptr)
{
	char event = *(char *)ptr;
	char rec_buf;

	kfree(ptr);
	if (!priv_data->is_mcu_ready) {
		pr_err("%s:mcu is not ready!!\n", __func__);
		return 0;
	}

	pr_info("%s: check event:%d\n", __func__, event);
	if (event == INT_BATLOSS_EVENT) {
		pr_info("double check battery lost event!\n");

		if (priv_data->get_charger_type &&
			(priv_data->get_charger_type() == CHARGER_TYPE_NONE)) {
			pr_info("No charger. Fake battery lost event?\n");
			return 0;
		}

		/* reg 0xa, bit3-> battery detect, 0 means battery lost */
		rw_gauge_reg(GAUGE_READ, 0x0A, 0x00, &rec_buf);
		if (rec_buf & 0x8) {
			pr_info("fake battery lost event?\n");
		} else {
			if (priv_data->battery_isr_hander) {
				reg_write(priv_data->client, REG_KEYPAD_PWM, 0);
				reg_write(priv_data->client, REG_CAHRGER_STATUS, 0);
				reg_write(priv_data->client, REG_LED_CTL, 0);
				priv_data->battery_isr_hander(FLAG_BATT_LOST);
			} else
				pr_err("Battery lost but no handler registered.\n");
		}
	}
	return 0;
}

static void msp430_work_func(struct work_struct *work)
{
	char int_status = 0, rec_buf = 0;
	int ret;
	char *ptr;
	struct task_struct *task;
	struct msp430_data *data;

	data = container_of(work, struct msp430_data, work);

	if (!data->is_mcu_ready) {
		pr_err("%s:mcu is not ready!!\n", __func__);
		return;
	}
	mutex_lock(&data->mutex);

	ret = reg_read(data->client, REG_INTERRUPT, &int_status);
	if (ret < 0)
		goto exit_read_failed;
	pr_debug("%s: Int status = %x\n", __func__, int_status);

	if (int_status & INT_QWKEY_EVENT) {
		ret = reg_read(data->client, REG_KEYBOARD_DATA, &rec_buf);
		if (ret < 0)
			goto exit_read_failed;

		report_msp430_key(data, rec_buf);
		pr_debug("%s: Key status = 0x%x\n", __func__, rec_buf);
	}

	if (int_status & INT_GAUGE_EVENT)
		data->gauge_data_ready = 1;

	if (int_status & INT_BATLOSS_EVENT) {
		ret = reg_read(data->client, REG_BAT_STATUS, &rec_buf);
		if (ret < 0)
			goto exit_read_failed;

		if (rec_buf & BAT_FLAG_LOST) {
			{
				struct timespec uptime;

				do_posix_clock_monotonic_gettime(&uptime);
				if (uptime.tv_sec > TIME_ENABLE_BATT_LOST_AFTER_BOOT) {
					ptr = kmalloc(sizeof(char), GFP_KERNEL);
					*ptr = INT_BATLOSS_EVENT;
					task = kthread_run(event_double_check_thd, ptr,
						"mcu-check_event");
					if (IS_ERR(task))
						pr_err("%s: create mcu thread failed!\n", __func__);
				}
			}
		} else
			pr_err("Fake battery lost interrupt?\n\n");
	}

	if ((int_status & INT_BATCAP_EVENT) ||
		(int_status & INT_BATZERO_EVENT) ||
		(int_status & INT_FULL_CHARGE_EVENT)) {
		/*battery zero event, give system 120secs to poweroff*/
		if (int_status & INT_BATZERO_EVENT)
			wake_lock_timeout(&priv_data->cap_zero_wlock, 120*HZ);

		/* When charging full but the capacity drops to 99%,
		 * we need to recharge manually. 5 seconds to do that.
		 */
		if ((priv_data->is_need_recharge_wakelock) &&
			(priv_data->get_charger_type() == CHARGER_TYPE_WALL))
			wake_lock_timeout(&priv_data->cap_zero_wlock, 5000);

		priv_data->is_need_recharge_wakelock = 0;
		if (int_status & INT_FULL_CHARGE_EVENT) {
			wake_lock_timeout(&priv_data->cap_zero_wlock, 100);
			ret = reg_read(priv_data->client, REG_MASK_INTERRUPT, &rec_buf);
			if (ret < 0) {
				pr_err("msp430: INT_FULL_CHARGE_EVENT read status error!\n");
			} else {
				rec_buf &= ~INT_FULL_CHARGE_EVENT;
				priv_data->is_need_recharge_wakelock = 1;
				reg_write(priv_data->client, REG_MASK_INTERRUPT, rec_buf);
				pr_info("%s:battery full charging event\n", __func__);
			}
		}

		battery_info_change();
		if (data->battery_isr_hander) {
			if (int_status & INT_BATCAP_EVENT) {
				data->battery_isr_hander(FLAG_BATT_CAP_CHANGE);
			} else {
				data->battery_isr_hander(FLAG_BATT_ZERO);
			}
		} else
			pr_err("Battery cap change but no handler registered.\n");
	}

exit_read_failed:
	mutex_unlock(&data->mutex);
}

static irqreturn_t msp430_interrupt(int irq, void *dev_id)
{
	struct msp430_data *data = (struct msp430_data *)dev_id;

	disable_irq_nosync(irq);
	schedule_work(&data->work);
	enable_irq(irq);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void msp430_early_suspend(struct early_suspend *h)
{
	char rec_buf;
	int ret;

	/* mcu enter suspend mode */
	rec_buf = priv_data->led_buf[0] & 0xcf;
	ret = reg_write(priv_data->client, REG_LED_CTL, rec_buf | SYS_MODE_SUSPEND);
	if (ret < 0)
		pr_err("mcu: enter suspend mode error!\n");

	if (!priv_data->is_hw_key_wakeup) {
		ret = reg_write(priv_data->client, REG_MASK_INTERRUPT,
			IRQ_DEFAULT_ENABLE & ~INT_QWKEY_EVENT);
		if (ret < 0)
			pr_err("mcu: early_suspend set irq fault!\n");
	}
}

void msp430_late_resume(struct early_suspend *h)
{
	char rec_buf;
	int ret;

	/* mcu enter normal mode */
	rec_buf = priv_data->led_buf[0] & 0xcf;
	ret = reg_write(priv_data->client, REG_LED_CTL, rec_buf | SYS_MODE_NORMAL);
	if (ret < 0)
		pr_err("mcu: enter normal mode error!\n");

	ret = reg_write(priv_data->client, REG_MASK_INTERRUPT, IRQ_DEFAULT_ENABLE);
	if (ret < 0)
		pr_err("mcu: resume set irq fault!\n");
}
#endif

static void mcu_hw_config(struct i2c_client *client, char version)
{
	char rec_buf = 0;
	int ret;

	ret = reg_write(priv_data->client, REG_CAHRGER_STATUS, 0);
	if (ret < 0)
		pr_err("mcu: REG_CAHRGER_STATUS fault!\n");

	/*enable low battery function*/
	ret = reg_write(client, REG_BATT_LOW_LEVEL, BATTERY_LOW_LEVEL);
	if (ret < 0)
		pr_err("mcu: set battery low level fault!\n");

	if (version >= 0x15 && version != 0xff) {
		if (version > 0x18)
			ret = reg_write(client, REG_LED_CTL, SYS_MODE_RESERVED);
		else
			ret = reg_write(client, REG_LED_CTL, SYS_MODE_NORMAL);
		if (ret < 0)
			pr_err("mcu: set battery low function fault!\n");
	}

	if (priv_data->hw_ver >= ACER_HW_VERSION_DVT2_2_AND_PVT) {
		if (reg_write(client, REG_WHITE_LED_PWM, PVT_WHITE_LED_LIGHT) < 0)
			pr_err("mcu: adjust white led brightness fault!\n");
		if (reg_write(client, REG_RED_LED_PWM, PVT_RED_LED_LIGHT) < 0)
			pr_err("mcu: adjust red led brightness fault!\n");
		if (reg_write(client, REG_GREEN_LED_PWM, PVT_GREEN_LED_LIGHT) < 0)
			pr_err("mcu: adjust green led brightness fault!\n");
	}

	/* Enable interrupt */
	reg_read(client, REG_INTERRUPT, &rec_buf); /* clear pending flag */
	ret = reg_write(client, REG_MASK_INTERRUPT, IRQ_DEFAULT_ENABLE);
	if (ret < 0)
		pr_err("mcu: enable irq fault!\n");
}

static int mcu_update_thread(void *ptr)
{
	unsigned int delay_ms = *(unsigned int *)ptr;
	char rec_buf;
	int ret;

	kfree(ptr);
	if (!priv_data) {
		pr_err("%s:null pointer!!!\n", __func__);
		return 0;
	}
	msleep(delay_ms);

	ret = is_mcu_need_update(priv_data->client,
			MCU_FIRMWARE_FILE, priv_data->version);
	if (!ret)
		return 0;
	else if (ret < 0) {
		pr_err("mcu update check error!\n");
		return 0;
	}

	wake_lock(&priv_data->mcu_update_wlock);
	priv_data->is_mcu_ready = 0;
	disable_irq(priv_data->client->irq);

	if (priv_data->version != 0xff)
		reg_write(priv_data->client, REG_MASK_INTERRUPT, 0);

	ret = update_mcu_firwmare(priv_data->client, MCU_FIRMWARE_FILE,
			priv_data->version);

	if (ret == 1) {
		msleep(1000);
		reg_read(priv_data->client, REG_SYSTEM_VERSION, &rec_buf);
		pr_info("mcu update successfully !! old Vers. %d "
					"new Vers. %d\n", priv_data->version, rec_buf);
		priv_data->version = rec_buf;
	} else
		pr_err("mcu update failed!!!!!\n");

	mcu_hw_config(priv_data->client, priv_data->version);
	priv_data->is_mcu_ready = 1;

	/*set led state*/
	ret = reg_write(priv_data->client, REG_LED_CTL,
		priv_data->led_buf[0] | SYS_MODE_NORMAL);
	if (ret < 0)
		pr_err("mcu: REG_LED_CTL fault!\n");
	ret = reg_write(priv_data->client, REG_CAHRGER_STATUS, priv_data->led_buf[1]);
	if (ret < 0)
		pr_err("mcu: REG_CAHRGER_STATUS fault!\n");
	ret = reg_write(priv_data->client, REG_KEYPAD_PWM, priv_data->led_buf[2]);
	if (ret < 0)
		pr_err("mcu: sREG_KEYPAD_PWM fault!\n");

	mod_timer(&priv_data->avg_timer, jiffies + msecs_to_jiffies(POLLING_TIME));
	enable_irq(priv_data->client->irq);
	wake_unlock(&priv_data->mcu_update_wlock);

	msleep(1000);
	priv_data->is_first_read_cap_after_resume = 1;
	battery_info_change();
	if (priv_data->battery_isr_hander)
		priv_data->battery_isr_hander(INT_BATCAP_EVENT);
	else
		pr_err("Battery cap change but no handler registered.\n");
	return 0;
}

static int update_mcu_delayed(unsigned int delay_ms)
{
	struct task_struct *task;
	unsigned int *delay_ptr;

	if (!priv_data) {
		pr_err("%s:null pointer!!!\n", __func__);
		return 0;
	}

	if (priv_data->hw_ver < ACER_HW_VERSION_DVT1) {
		mcu_hw_config(priv_data->client, priv_data->version);
		priv_data->is_mcu_ready = 1;
		return 0;
	}

	delay_ptr = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	*delay_ptr = delay_ms;

	task = kthread_run(mcu_update_thread, delay_ptr, "mcu-update");
	if (IS_ERR(task))
		pr_err("%s: create mcu thread failed!\n", __func__);
	return 1;
}

static ssize_t HWKey_wake_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	if ((count > 0) && priv_data) {
		if (buf[0] == '1')
			priv_data->is_hw_key_wakeup = 1;
		else if (buf[0] == '0')
			priv_data->is_hw_key_wakeup = 0;
		pr_info("MCU:HW key wakeup:%d\n", priv_data->is_hw_key_wakeup);
	}
	return count;
}
static ssize_t HWKey_wake_show(struct device *dev, struct device_attribute *devattr,
				char *buf)
{
	return sprintf(buf, "%d\n", priv_data?priv_data->is_hw_key_wakeup:-1);
}
static DEVICE_ATTR(HWKey_wake, S_IWUSR | S_IRUSR, HWKey_wake_show, HWKey_wake_store);

static int msp430_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int i;
	int ret = 0;
	char rec_buf = 0;
	struct msp430_data *data;
	struct _batt_func batt_func = {0};
	acer_smem_flag_t *acer_smem_flag;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c_check_functionality failed!\n", __func__);
		ret = -EIO;
		goto exit_i2c_check_failed;
	}

	data = kzalloc(sizeof(struct msp430_data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s: no memory for driver data\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc_failed;
	}
	i2c_set_clientdata(client, data);
	data->client = client;
	data->is_mcu_ready = 0;
	data->led_buf[0] = 0;
	data->led_buf[1] = 0;
	data->led_buf[2] = 0;
	data->batt_info.cap_percent = 255;
	data->is_first_read_cap_after_resume = 1;
	priv_data = data;

	acer_smem_flag = (acer_smem_flag_t *)(smem_alloc(SMEM_ID_VENDOR0,
		sizeof(acer_smem_flag_t)));
	if (acer_smem_flag == NULL) {
		pr_err("%s:alloc acer_smem_flag failed!\n", __func__);
		data->hw_ver = ACER_HW_VERSION_INVALID;
	} else
		data->hw_ver = acer_smem_flag->acer_hw_version;

	mutex_init(&data->gauge_mutex);
	mutex_init(&data->mutex);
	mutex_init(&data->i2c_mutex);
	INIT_WORK(&data->work, msp430_work_func);
	init_waitqueue_head(&data->gauge_wait);
	wake_lock_init(&data->cap_zero_wlock, WAKE_LOCK_SUSPEND, "batt_zero(full)_lock");
	wake_lock_init(&data->mcu_update_wlock, WAKE_LOCK_SUSPEND, "mcu_update_lock");

	setup_timer(&data->avg_timer, avg_timer_expired, 0);
	INIT_WORK(&data->avg_work, avg_timer_work);

	/* connect with battery driver */
	batt_func.get_battery_info = _get_battery_info;
	register_bat_func(&batt_func);
	if (data && batt_func.battery_isr_hander)
		data->battery_isr_hander = batt_func.battery_isr_hander;
	else
		pr_err("%s:register battery function(battery_isr_hander) error!\n", __func__);

	if (data && batt_func.get_charger_type)
		data->get_charger_type = batt_func.get_charger_type;
	else
		pr_err("%s:register battery function(get_charger_type) error!\n", __func__);


	/* Input register */
	data->input = input_allocate_device();
	if (!data->input) {
		pr_err("%s: input_allocate_device failed!\n", __func__);
		ret = -ENOMEM;
		goto exit_input_allocate_failed;
	}

	data->input->name = "a5-msp430-keypad";
	data->keymap_size = ARRAY_SIZE(msp430_keymap);
	for (i = 1; i < data->keymap_size; i++)
		input_set_capability(data->input, EV_KEY, msp430_keymap[i]);

	ret = input_register_device(data->input);
	if (ret) {
		pr_err("%s input_register_device failed!\n", __func__);
		goto exit_input_register_failed;
	}

	/*  Link interrupt routine with the irq */
	if (client->irq) {
		ret = request_irq(client->irq, msp430_interrupt,
				IRQF_TRIGGER_RISING, MSP430_DRIVER_NAME, data);
		if (ret < 0) {
			pr_err("%s: request_irq failed!\n", __func__);
			goto exit_irq_request_failed;
		} else
			enable_irq_wake(client->irq);
	}

	/* Read MCU Version */
	ret = reg_read(client, REG_SYSTEM_VERSION, &rec_buf);
	if (ret < 0) {
		ret = -EIO;
		goto exit_read_failed;
	}
	pr_info("MCU msp430 Version = %d\n", rec_buf);
	data->version = rec_buf;

	if (data->version > 0x18)
		data->led_buf[0] = SYS_MODE_RESERVED;
	else
		data->led_buf[0] = SYS_MODE_NORMAL;

	if (data->hw_ver != ACER_HW_VERSION_INVALID)
		update_mcu_delayed(14000);

	/*version=0xff means firmware is broken,
	 *it should be fixed by update_mcu_delayed*/
	if (data->version != 0xff) {
		mcu_hw_config(client, data->version);
		data->is_mcu_ready = 1;
		battery_info_change();
		mod_timer(&data->avg_timer, jiffies + msecs_to_jiffies(POLLING_TIME - 5000));
	}

	data->is_hw_key_wakeup = 1;
	if (device_create_file(&client->dev, &dev_attr_HWKey_wake))
		pr_err("MCU: create sysfs file error!\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	data->early_suspend.suspend = msp430_early_suspend;
	data->early_suspend.resume = msp430_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	return 0;

exit_read_failed:
	free_irq(client->irq, &data);

exit_irq_request_failed:
	input_unregister_device(data->input);

exit_input_register_failed:
	input_free_device(data->input);

exit_input_allocate_failed:
	kfree(data);

exit_kzalloc_failed:
exit_i2c_check_failed:

	return ret;
}

static void msp430_shutdown(struct i2c_client *client)
{
	/*disable led and irq before power down*/
	reg_write(priv_data->client, REG_MASK_INTERRUPT, 0);
	reg_write(priv_data->client, REG_KEYPAD_PWM, 0);
	reg_write(priv_data->client, REG_CAHRGER_STATUS, 0);
	reg_write(priv_data->client, REG_LED_CTL, 0);
}

static int msp430_pm_suspend(struct device *dev)
{
	int ret;
	char irq_flag = 0;

	if (priv_data && (priv_data->version >= 0x16))
		irq_flag = INT_BATZERO_EVENT | INT_BATLOSS_EVENT;
	else
		irq_flag = INT_BATLOSS_EVENT;

	del_timer_sync(&priv_data->avg_timer);

	if (priv_data->get_charger_type && (priv_data->batt_info.cap_percent != 100) &&
		(priv_data->get_charger_type() == CHARGER_TYPE_WALL)) {
		irq_flag |= INT_FULL_CHARGE_EVENT;
	}

	if (priv_data->is_need_recharge_wakelock)
		irq_flag |= INT_BATCAP_EVENT;

	if (priv_data->is_hw_key_wakeup)
		irq_flag |= INT_QWKEY_EVENT;

	ret = reg_write(priv_data->client, REG_MASK_INTERRUPT, irq_flag);
	if (ret < 0)
		pr_err("mcu: suspend set irq fault!\n");
	return 0;
}

static int msp430_pm_resume(struct device *dev)
{
	priv_data->is_first_read_cap_after_resume = 1;

	battery_info_change();
	if (priv_data->battery_isr_hander)
		priv_data->battery_isr_hander(FLAG_BATT_CAP_CHANGE);
	else
		pr_err("Battery cap change but no handler registered.\n");

	mod_timer(&priv_data->avg_timer, jiffies + msecs_to_jiffies(POLLING_TIME));
	return 0;
}

static int msp430_remove(struct i2c_client *client)
{
	struct msp430_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	del_timer_sync(&data->avg_timer);
	wake_lock_destroy(&data->cap_zero_wlock);
	wake_lock_destroy(&data->mcu_update_wlock);
	free_irq(client->irq, &data);
	input_unregister_device(data->input);
	input_free_device(data->input);
	kfree(data);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id msp430_id[] = {
	{ MSP430_DRIVER_NAME, 0 },
	{ }
};

static const struct dev_pm_ops msp430_pm_ops = {
	.suspend = msp430_pm_suspend,
	.resume = msp430_pm_resume,
};

static struct i2c_driver msp430_driver = {
	.probe     = msp430_probe,
	.remove    = msp430_remove,
	.id_table  = msp430_id,
	.shutdown  = msp430_shutdown,
	.driver    = {
		.name      = MSP430_DRIVER_NAME,
		.pm        = &msp430_pm_ops,
	},
};

static int __init msp430_init(void)
{
	int res = 0;

	res = i2c_add_driver(&msp430_driver);
	if (res) {
		pr_err("%s: i2c_add_driver failed!\n", __func__);
		return res;
	}

	return 0;
}

static void __exit msp430_exit(void)
{
	i2c_del_driver(&msp430_driver);
}

module_init(msp430_init);
module_exit(msp430_exit);

MODULE_AUTHOR("Brad Chen <ChunHung_Chen@acer.com.tw");
MODULE_DESCRIPTION("MCU msp430 driver");
MODULE_LICENSE("GPL");

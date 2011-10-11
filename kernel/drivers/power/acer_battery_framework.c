/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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

/*
 * this needs to be before <linux/kernel.h> is loaded,
 * and <linux/sched.h> loads <linux/kernel.h>
 */

#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/acer_battery_framework.h>
#include <linux/i2c/smb136.h>
#include <linux/wakelock.h>

#include <asm/atomic.h>

#include <mach/msm_battery.h>
#include <mach/msm_hsusb.h>

/* If the battery physical layer dont have interrupt of
 * capacity changing, the timer needs to be enabled.
 * */
#ifdef CONFIG_BATTERY_MSM_A4
#define BATT_POLLING_TIMER
#define AC_WAKE_LOCK
#endif

#ifdef BATT_POLLING_TIMER
#define BAT_POLLING_TIME       30000 /* 30 seconds*/
static struct timer_list polling_timer;
#endif

struct msm_battery_info {
	u32 voltage_max_design;
	u32 voltage_min_design;
	u32 batt_technology;

	u32 avail_chg_sources;
	u32 current_chg_source;

	u32 batt_status;
	u32 batt_health;
	u32 batt_capacity; /* in percentage */
	u32 batt_valid;

	u32 charger_type;
	u32 battery_voltage; /* in millie volts */
	s32 battery_temp;  /* in celsius */
	u32 ready;
	u32 chg_type;

	/* Some phone experiment need to disable the feature
	 * to avoid phone power-off.
	 */
	u32 no_zero_battery;

	struct power_supply *msm_psy_ac;
	struct power_supply *msm_psy_usb;
	struct power_supply *msm_psy_batt;

	struct early_suspend early_suspend;
	struct mutex mutex;
	struct work_struct bat_work;
	struct workqueue_struct *wq;

	int (*get_battery_info)(struct _batt_info *);
	void (*phy_early_suspend)(void);
	void (*phy_late_resume)(void);
};

static struct msm_battery_info msm_batt_info = {
	.charger_type = CHARGER_TYPE_NONE,
	.battery_voltage = BATTERY_HIGH,
	.batt_capacity = 100,
	.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING,
	.batt_health = POWER_SUPPLY_HEALTH_GOOD,
	.batt_valid  = 1,
	.battery_temp = 23,
	.ready = 0,
	.chg_type = 0xf,
	.get_battery_info = 0,
	.phy_early_suspend = 0,
	.phy_late_resume = 0,
	.no_zero_battery = 0,
};

/* work around: detec usb charger type by usb driver */
static unsigned int g_chg_type = USB_CHG_TYPE__INVALID;

#ifdef AC_WAKE_LOCK
static struct wake_lock ac_wlock;
#endif

static enum power_supply_property msm_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *msm_power_supplied_to[] = {
	"battery",
};

static int msm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
			val->intval = msm_batt_info.current_chg_source & AC_CHG
			    ? 1 : 0;
		}
		if (psy->type == POWER_SUPPLY_TYPE_USB) {
			val->intval = msm_batt_info.current_chg_source & USB_CHG
			    ? 1 : 0;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property msm_batt_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP
};

static int msm_batt_power_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = msm_batt_info.batt_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = msm_batt_info.batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = msm_batt_info.batt_valid;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = msm_batt_info.batt_technology;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = msm_batt_info.voltage_max_design;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = msm_batt_info.voltage_min_design;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = msm_batt_info.battery_voltage * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = msm_batt_info.batt_capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = msm_batt_info.battery_temp * 10;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct power_supply msm_psy_ac = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = msm_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(msm_power_supplied_to),
	.properties = msm_power_props,
	.num_properties = ARRAY_SIZE(msm_power_props),
	.get_property = msm_power_get_property,
};

static struct power_supply msm_psy_usb = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.supplied_to = msm_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(msm_power_supplied_to),
	.properties = msm_power_props,
	.num_properties = ARRAY_SIZE(msm_power_props),
	.get_property = msm_power_get_property,
};

static struct power_supply msm_psy_batt = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = msm_batt_power_props,
	.num_properties = ARRAY_SIZE(msm_batt_power_props),
	.get_property = msm_batt_power_get_property,
};

int report_batt_info(struct _batt_info *battery_info)
{
	if (!battery_info)
		return -EINVAL;
	battery_info->cap_percent = msm_batt_info.batt_capacity;
	battery_info->voltage = msm_batt_info.battery_voltage;
	battery_info->temperature = msm_batt_info.battery_temp;
	return 0;
}
EXPORT_SYMBOL(report_batt_info);

int fast_charging_enable(bool enable)
{
	if (g_chg_type != USB_CHG_TYPE__SDP) {
		pr_err("call fast charging function while no USB_SDP!!\n");
		return 0;
	}

	if (enable) {
		if (smb136_control(USBIN_MODE, USB_HC_MODE|USB_IN_500MA) < 0)
			pr_err("charging ic:USB_HC_MODE error!!\n");
	} else {
		if (smb136_control(USBIN_MODE, USB_IN_500MA) < 0)
			pr_err("charging ic:USB_IN_500MA error!!\n");
	}

	pr_info("Fast charging: %s.\n", enable ? "enable" : "disable");
	return 0;
}
EXPORT_SYMBOL(fast_charging_enable);

static int battery_get_charger_type(void)
{
	if (g_chg_type == USB_CHG_TYPE__INVALID)
		return CHARGER_TYPE_NONE;
	if (g_chg_type == USB_CHG_TYPE__WALLCHARGER)
		return CHARGER_TYPE_WALL;
	if (g_chg_type == USB_CHG_TYPE__SDP)
		return CHARGER_TYPE_USB_PC;
	return CHARGER_TYPE_INVALID;
}

static int msm_batt_get_batt_chg_status(unsigned int *charger_type, int force_update)
{
	int rs = 0;

	if (!force_update && (g_chg_type == msm_batt_info.chg_type)) {
		*charger_type = msm_batt_info.charger_type;
		return rs;
	}

	if (g_chg_type == USB_CHG_TYPE__INVALID) {
		*charger_type = CHARGER_TYPE_NONE;

		if (msm_batt_info.chg_type == USB_CHG_TYPE__WALLCHARGER)
			pr_info("remove AC Charger.\n");
		else
			pr_info("remove USB Charger.\n");

		if (smb136_control(PIN_CONTROl, PIN_CTL) < 0) {
			pr_err("charging ic:PIN_CTL error!!\n");
			rs = -1;
		}
#ifdef AC_WAKE_LOCK
		wake_unlock(&ac_wlock);
#endif
	} else {
		if ((g_chg_type == USB_CHG_TYPE__WALLCHARGER) ||
			(g_chg_type == USB_CHG_TYPE__CARKIT)) {
#ifdef AC_WAKE_LOCK
			wake_lock(&ac_wlock);
#endif
			*charger_type = CHARGER_TYPE_WALL;

			pr_info("charger type:AC!\n");
			if (smb136_control(USBIN_MODE, USB_HC_MODE|USB_IN_500MA) < 0) {
				pr_err("charging ic:USB_HC_MODE error!!\n");
				rs = -1;
			}

		} else if (g_chg_type == USB_CHG_TYPE__SDP) {
			*charger_type = CHARGER_TYPE_USB_PC;

			pr_info("charger type:USB!\n");
			if (smb136_control(USBIN_MODE, USB_IN_500MA) < 0) {
				pr_err("charging ic:USB_IN_500MA error!!\n");
				rs = -1;
			}
		} else
			*charger_type = CHARGER_TYPE_INVALID;

		if (smb136_control(PIN_CONTROl, I2C_CTL) < 0) {
			pr_err("charging ic:I2C_CTL error!!\n");
			rs = -1;
		}
	}
	msm_batt_info.chg_type = g_chg_type;

	return rs;
}

static inline int batt_info_check(struct _batt_info *info)
{
	u32 ret = 0;
	ret = (info->temperature > 100);
	ret |= (info->voltage > BATTERY_HIGH) || (info->voltage < BATTERY_LOW);
	ret |= (info->cap_percent < 0) || (info->cap_percent > 100);
	return ret;
}

static void msm_batt_update_psy_status(void)
{
	u32	charger_type = 0;
	u32 battery_voltage = 0;
	u32	battery_temp = 0;
	u32 battery_capacity = 0;
	int ret = 0;
	struct	power_supply *supp;
	struct _batt_info batt_info;
	static int force_update;

	if (!msm_batt_info.ready)
		return;

#ifdef BATT_POLLING_TIMER
	del_timer_sync(&polling_timer);
#endif
	mutex_lock(&msm_batt_info.mutex);

	if (!msm_batt_info.batt_valid) {
		msm_batt_info.batt_status =
			POWER_SUPPLY_STATUS_NOT_CHARGING;
		msm_batt_info.charger_type = CHARGER_TYPE_NONE;
		msm_batt_info.current_chg_source = 0;
		msm_batt_info.battery_voltage = 0;
		msm_batt_info.batt_capacity = 0;
		supp = &msm_psy_batt;
		power_supply_changed(supp);
		goto out;
	}

	if (msm_batt_get_batt_chg_status(&charger_type, force_update)) {
		pr_err("get wrong charger type!!\n");
		ret = -1;
		goto out;
	}

	if (msm_batt_info.get_battery_info) {
		if (msm_batt_info.get_battery_info(&batt_info) < 0) {
			pr_err("get_battery_info function error!\n");
			ret = -1;
			goto out;
		}
	} else {
		pr_err("battery function is not registered!!\n");
		ret = -1;
		goto out;
	}

	if (!batt_info_check(&batt_info)) {
		battery_temp = batt_info.temperature;
		battery_voltage = batt_info.voltage;
		battery_capacity = batt_info.cap_percent;
	} else {
		pr_err("wrong battery info! use previous data..\n");
		ret = -1;
		battery_temp = msm_batt_info.battery_temp;
		battery_voltage = msm_batt_info.battery_voltage;
		battery_capacity = msm_batt_info.batt_capacity;
	}

	if (charger_type == msm_batt_info.charger_type &&
	    battery_capacity == msm_batt_info.batt_capacity &&
	    battery_voltage == msm_batt_info.battery_voltage &&
	    battery_temp == msm_batt_info.battery_temp)
		goto out;

	if (charger_type == CHARGER_TYPE_USB_WALL ||
		charger_type == CHARGER_TYPE_USB_PC ||
		charger_type == CHARGER_TYPE_USB_CARKIT) {
		supp = &msm_psy_usb;
		msm_batt_info.current_chg_source = USB_CHG;
		msm_batt_info.batt_status =
			POWER_SUPPLY_STATUS_CHARGING;

	} else if (charger_type == CHARGER_TYPE_WALL) {
		supp = &msm_psy_ac;
		msm_batt_info.current_chg_source = AC_CHG;
		msm_batt_info.batt_status =
			POWER_SUPPLY_STATUS_CHARGING;
	} else if (charger_type == CHARGER_TYPE_NONE) {
		supp = &msm_psy_batt;
		msm_batt_info.current_chg_source = 0;
		msm_batt_info.batt_status =
			POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else { /*should not happen*/
		ret = -1;
		goto out;
	}

	msm_batt_info.charger_type = charger_type;
	msm_batt_info.battery_temp = battery_temp;
	msm_batt_info.battery_voltage = battery_voltage;
	if (!battery_capacity && msm_batt_info.no_zero_battery) {
		pr_info("battery capacity is 0%% but report 1%%!!\n");
		msm_batt_info.batt_capacity = 1;
	} else
		msm_batt_info.batt_capacity = battery_capacity;

	if ((msm_batt_info.batt_capacity == 100) &&
		(charger_type != CHARGER_TYPE_NONE))
		msm_batt_info.batt_status = POWER_SUPPLY_STATUS_FULL;

	if (supp)
		power_supply_changed(supp);

out:
	if (ret < 0) {
		pr_err("msm_batt_update_psy_status: update failed!!\n");
		force_update = 1;
	} else
		force_update = 0;

	mutex_unlock(&msm_batt_info.mutex);
#ifdef BATT_POLLING_TIMER
	mod_timer(&polling_timer, jiffies + msecs_to_jiffies(BAT_POLLING_TIME));
#endif
}

static void bat_work_func(struct work_struct *work)
{
	msm_batt_update_psy_status();
}

void batt_chg_type_notify_callback(uint8_t type)
{
	g_chg_type = type;
	if (msm_batt_info.ready)
		if (!queue_work(msm_batt_info.wq, &msm_batt_info.bat_work)) {
			pr_info("BATT:flush work queue\n");
			flush_workqueue(msm_batt_info.wq);
			queue_work(msm_batt_info.wq, &msm_batt_info.bat_work);
		}
}
EXPORT_SYMBOL(batt_chg_type_notify_callback);

#ifdef CONFIG_HAS_EARLYSUSPEND
void msm_batt_early_suspend(struct early_suspend *h)
{
	if (msm_batt_info.phy_early_suspend)
		msm_batt_info.phy_early_suspend();
}

void msm_batt_late_resume(struct early_suspend *h)
{
	if (msm_batt_info.phy_late_resume)
		msm_batt_info.phy_late_resume();
#ifdef BATT_POLLING_TIMER
	if (msm_batt_info.ready)
		if (!queue_work(msm_batt_info.wq, &msm_batt_info.bat_work)) {
			pr_info("BATT:flush work queue\n");
			flush_workqueue(msm_batt_info.wq);
			queue_work(msm_batt_info.wq, &msm_batt_info.bat_work);
		}
#endif
}
#endif

static void battery_isr_hander(unsigned int flag)
{
	if (!msm_batt_info.ready)
		return;

	if (flag & FLAG_BATT_LOST) {
		pr_info("battery lost evnet.\n");
		msm_batt_info.batt_valid = 0;
		msm_batt_update_psy_status();
	}

	if (flag & FLAG_BATT_CAP_CHANGE)
		if (!queue_work(msm_batt_info.wq, &msm_batt_info.bat_work)) {
			pr_info("BATT:flush work queue\n");
			flush_workqueue(msm_batt_info.wq);
			queue_work(msm_batt_info.wq, &msm_batt_info.bat_work);
		}

	if (flag & FLAG_BATT_ZERO)
		msm_batt_update_psy_status();
}

static ssize_t no_zero_batt_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count > 0) {
		if (buf[0] == '1') {
			msm_batt_info.no_zero_battery = 1;
			pr_info("BATT: no 0%% battery is enabled!\n");
		} else if (buf[0] == '0') {
			msm_batt_info.no_zero_battery = 0;
			pr_info("BATT: no 0%% battery is disabled!\n");
		}
	}
	return count;
}

static ssize_t module_enable_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#ifdef CONFIG_BATTERY_MSM_A4
	if (!batt_module_enable)
		return count;

	switch (buf[0]) {
	case 'w':
		batt_module_enable(MODULE_WIFI, true);
		break;
	case 'c':
		batt_module_enable(MODULE_CAMERA, true);
		break;
	case 'p':
		batt_module_enable(MODULE_PHONE_CALL, true);
		break;
	}
#endif
	return count;
}

static ssize_t module_disable_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#ifdef CONFIG_BATTERY_MSM_A4
	if (!batt_module_enable)
		return count;

	switch (buf[0]) {
	case 'w':
		batt_module_enable(MODULE_WIFI, false);
		break;
	case 'c':
		batt_module_enable(MODULE_CAMERA, false);
		break;
	case 'p':
		batt_module_enable(MODULE_PHONE_CALL, false);
		break;
	}
#endif
	return count;
}

static DEVICE_ATTR(no_zero_batt, S_IWUSR, NULL, no_zero_batt_store);
static DEVICE_ATTR(module_enable, S_IWUSR, NULL, module_enable_store);
static DEVICE_ATTR(module_disable, S_IWUSR, NULL, module_disable_store);

/* create entry under /sys/devices/platform/msm-battery/power_supply/battery/ */
static void create_batt_sys_entry(struct power_supply *psy)
{
	int rc = 0;
	rc = device_create_file(psy->dev, &dev_attr_no_zero_batt);
	rc = device_create_file(psy->dev, &dev_attr_module_enable);
	rc = device_create_file(psy->dev, &dev_attr_module_disable);
	if (rc)
		pr_err(" module_use_sys_create error!\n");
}

static void del_batt_sys_entry(struct power_supply *psy)
{
	device_remove_file(psy->dev, &dev_attr_no_zero_batt);
	device_remove_file(psy->dev, &dev_attr_module_enable);
	device_remove_file(psy->dev, &dev_attr_module_disable);
}

static int msm_battery_cleanup(void)
{
	msm_batt_info.ready = 0;
	if (msm_batt_info.msm_psy_ac)
		power_supply_unregister(msm_batt_info.msm_psy_ac);
	if (msm_batt_info.msm_psy_usb)
		power_supply_unregister(msm_batt_info.msm_psy_usb);
	if (msm_batt_info.msm_psy_batt)
		power_supply_unregister(msm_batt_info.msm_psy_batt);
	if (msm_batt_info.wq)
		destroy_workqueue(msm_batt_info.wq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (msm_batt_info.early_suspend.suspend == msm_batt_early_suspend)
		unregister_early_suspend(&msm_batt_info.early_suspend);
#endif
	return 0;
}

int register_bat_func(struct _batt_func *batt_func)
{
	batt_func->battery_isr_hander = battery_isr_hander;
	batt_func->get_charger_type = battery_get_charger_type;
	batt_func->batt_supply = &msm_psy_batt;
	if (batt_func->get_battery_info)
		msm_batt_info.get_battery_info = batt_func->get_battery_info;
	else
		pr_err("register_bat_func failed!!!\n");

	if (batt_func->late_resume)
		msm_batt_info.phy_late_resume = batt_func->late_resume;
	if (batt_func->early_suspend)
		msm_batt_info.phy_early_suspend = batt_func->early_suspend;
	return 0;
}

#ifdef BATT_POLLING_TIMER
static void polling_timer_expired(unsigned long unused)
{
	if (msm_batt_info.ready) {
		if (!queue_work(msm_batt_info.wq, &msm_batt_info.bat_work)) {
			pr_info("BATT:flush work queue\n");
			flush_workqueue(msm_batt_info.wq);
			queue_work(msm_batt_info.wq, &msm_batt_info.bat_work);
		}
	} else
		pr_info("start battery timer failed!!!!!\n");
}
#endif

static int __devinit msm_batt_probe(struct platform_device *pdev)
{
	int rc;
	struct msm_psy_batt_pdata *pdata = pdev->dev.platform_data;

	if (pdev->id != -1) {
		dev_err(&pdev->dev,
			"%s: MSM chipsets Can only support one"
			" battery ", __func__);
		return -EINVAL;
	}

	if (pdata->avail_chg_sources & AC_CHG) {
		rc = power_supply_register(&pdev->dev, &msm_psy_ac);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"%s: power_supply_register failed"
				" rc = %d\n", __func__, rc);
			msm_battery_cleanup();
			return rc;
		}
		msm_batt_info.msm_psy_ac = &msm_psy_ac;
		msm_batt_info.avail_chg_sources |= AC_CHG;
	}

	if (pdata->avail_chg_sources & USB_CHG) {
		rc = power_supply_register(&pdev->dev, &msm_psy_usb);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"%s: power_supply_register failed"
				" rc = %d\n", __func__, rc);
			msm_battery_cleanup();
			return rc;
		}
		msm_batt_info.msm_psy_usb = &msm_psy_usb;
		msm_batt_info.avail_chg_sources |= USB_CHG;
	}

	if (!msm_batt_info.msm_psy_ac && !msm_batt_info.msm_psy_usb) {

		dev_err(&pdev->dev,
			"%s: No external Power supply(AC or USB)"
			"is avilable\n", __func__);
		msm_battery_cleanup();
		return -ENODEV;
	}

	msm_batt_info.voltage_max_design = pdata->voltage_max_design;
	msm_batt_info.voltage_min_design = pdata->voltage_min_design;
	msm_batt_info.batt_technology = pdata->batt_technology;

	if (!msm_batt_info.voltage_min_design)
		msm_batt_info.voltage_min_design = BATTERY_LOW;
	if (!msm_batt_info.voltage_max_design)
		msm_batt_info.voltage_max_design = BATTERY_HIGH;

	if (msm_batt_info.batt_technology == POWER_SUPPLY_TECHNOLOGY_UNKNOWN)
		msm_batt_info.batt_technology = POWER_SUPPLY_TECHNOLOGY_LION;

	rc = power_supply_register(&pdev->dev, &msm_psy_batt);
	if (rc < 0) {
		dev_err(&pdev->dev, "%s: power_supply_register failed"
			" rc=%d\n", __func__, rc);
		msm_battery_cleanup();
		return rc;
	}
	msm_batt_info.msm_psy_batt = &msm_psy_batt;

	/* register battery voltage getting function here */
#ifdef CONFIG_HAS_EARLYSUSPEND
	msm_batt_info.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	msm_batt_info.early_suspend.suspend = msm_batt_early_suspend;
	msm_batt_info.early_suspend.resume = msm_batt_late_resume;
	register_early_suspend(&msm_batt_info.early_suspend);
#endif

#ifdef AC_WAKE_LOCK
	wake_lock_init(&ac_wlock, WAKE_LOCK_SUSPEND, "acer_battery");
#endif

	INIT_WORK(&msm_batt_info.bat_work, bat_work_func);
	msm_batt_info.wq = create_workqueue("battery workqueue");
	if (!msm_batt_info.wq) {
		pr_err("%s: Create work queue error!!!\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&msm_batt_info.mutex);

	msm_batt_info.ready = 1;
	create_batt_sys_entry(msm_batt_info.msm_psy_batt);

#ifdef BATT_POLLING_TIMER
	setup_timer(&polling_timer, polling_timer_expired, 0);
	mod_timer(&polling_timer, jiffies + msecs_to_jiffies(BAT_POLLING_TIME));
#endif

	msm_batt_update_psy_status();
	return 0;
}

static int __devexit msm_batt_remove(struct platform_device *pdev)
{
	return msm_battery_cleanup();
}

static void msm_batt_shutdown(struct platform_device *pdev)
{
#ifdef BATT_POLLING_TIMER
	del_timer_sync(&polling_timer);
#endif
	if (smb136_control(POWER_DETECTION, POWER_DET_EN) < 0)
		pr_err("charging ic:POWER_DETECTION error!!\n");
	if (smb136_control(PIN_CONTROl, PIN_CTL) < 0)
		pr_err("charging ic:PIN_CTL error!!\n");
}

static struct platform_driver msm_batt_driver = {
	.probe = msm_batt_probe,
	.shutdown  = msm_batt_shutdown,
	.remove = __devexit_p(msm_batt_remove),
	.driver = {
		   .name = "msm-battery",
		   .owner = THIS_MODULE,
		   },
};

static int __init msm_batt_init(void)
{
	int rc;

	rc = platform_driver_register(&msm_batt_driver);
	if (rc < 0)
		pr_err("%s: FAIL: platform_driver_register. rc = %d\n",
		       __func__, rc);
	return 0;
}

static void __exit msm_batt_exit(void)
{
#ifdef BATT_POLLING_TIMER
	del_timer_sync(&polling_timer);
#endif
	del_batt_sys_entry(msm_batt_info.msm_psy_batt);
	platform_driver_unregister(&msm_batt_driver);
}

module_init(msm_batt_init);
module_exit(msm_batt_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jacob Lee");
MODULE_DESCRIPTION("Battery driver for ACER platform.");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:msm_battery");

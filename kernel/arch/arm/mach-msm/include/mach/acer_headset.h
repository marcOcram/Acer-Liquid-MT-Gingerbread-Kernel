/*
 * Acer Headset device detection driver
 *
 *
 * Copyright (C) 2010 acer Corporation.
 *
 * Authors:
 *    Eric Chang <Eric_Cheng@acer.com.tw>
 */

#ifndef __ACER_HEADSET_H
#define __ACER_HEADSET_H

#include <linux/switch.h>
#include <asm/uaccess.h>

#if defined(CONFIG_MACH_ACER_A4)
	#define GPIO_HS_DET							26
	#define GPIO_HS_BUTT						48
	#define GPIO_HS_SWITCH						55
	#define HEADSET_MIC_BIAS_WORK_DELAY_TIME	10
#elif defined(CONFIG_MACH_ACER_A5)
	#define GPIO_HS_DET							120
	#define GPIO_HS_BUTT						39
	#define GPIO_HS_SWITCH						55
	#define PMIC_GPIO_HS_AMP					11  /* PMIC GPIO Number 12 */
	#define HEADSET_MIC_BIAS_WORK_DELAY_TIME	100
#endif

#define ACER_HS_IOCTL_MAGIC 	'g'
#define ACER_HS_IOC_MAXNR		3

#define EXPIRES					3

#define ACER_HS_CHANGE_CONTROL	_IO(ACER_HS_IOCTL_MAGIC, 1)
#define ACER_HS_ENABLE_AMP		_IO(ACER_HS_IOCTL_MAGIC, 2)
#define ACER_HS_GET_STATE		_IO(ACER_HS_IOCTL_MAGIC, 3)

struct hs_res {
	struct switch_dev sdev;
	unsigned int det;
	unsigned int irq;
	unsigned int mic_bias_en;
	unsigned int hph_amp_en;
	bool headsetOn;
	struct hrtimer timer;
	ktime_t debounce_time;
};

extern void acer_headset_update(void);
extern void hs_amp(bool enable);
extern bool control;
#endif

/* arch/arm/mach-msm/include/mach/tpa2028.h
 *
 * Copyright (C) 2010 acer Corporation.
 *
 */

#ifndef __LINUX_TPA2028_H
#define __LINUX_TPA2028_H
#endif

#include <linux/ioctl.h>

#define GPIO_SPK_AMP			143

#define TPA2028_IOCTL_MAGIC		'f'
#define IOC_MAXNR				8

#define TPA2028_SET_FIXED_GAIN	_IO(TPA2028_IOCTL_MAGIC, 1)
#define TPA2028_SET_STREAM_TYPE	_IO(TPA2028_IOCTL_MAGIC, 2)
#define TPA2028_OPEN				_IO(TPA2028_IOCTL_MAGIC, 3)
#define TPA2028_CLOSE				_IO(TPA2028_IOCTL_MAGIC, 4)
#define TPA2028_SET_ADIE_CODEC	_IO(TPA2028_IOCTL_MAGIC, 5)
#define TPA2028_GET_ADIE_CODEC	_IO(TPA2028_IOCTL_MAGIC, 6)
#define TPA2028_SET_AMP_CODEC		_IO(TPA2028_IOCTL_MAGIC, 7)
#define TPA2028_GET_AMP_CODEC		_IO(TPA2028_IOCTL_MAGIC, 8)

struct tpa2028_codec_info {
	uint32_t reg;
	uint32_t val;
};

extern int tpa2028_set_control(int commad, int regiter, int value);
extern int tpa2028_software_shutdown(int command);
extern int tpa2028_speaker_dolby_switch(int command);
extern int tpa2028_speaker_phone_switch(int command);

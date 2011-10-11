/*
 * Copyright (c) 2009 ACER, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/gpio.h>
#include "msm_fb.h"
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <linux/pwm.h>
#include <linux/moduleparam.h>
#ifdef CONFIG_PMIC8058_PWM
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwm.h>
#endif
/* get hardware version */
#include "../../../arch/arm/mach-msm/smd_private.h"
acer_hw_version_t hw_ver = ACER_HW_VERSION_DVT1;
static int cabc_min_value = 120;
static int cabc_mode = 3;

/* LCD GPIOs */
#define GPIO_LCD_RST	180

/* SPI GPIOs */
#define GPIO_SPI_CLK	45
#define GPIO_SPI_DI		47
#define GPIO_SPI_CS		44

/* LCD IF */
#define GPIO_PCLK		90
#define GPIO_VSYNC		92
#define GPIO_HSYNC		93
#define GPIO_VDEN		91

/* SPI cmd define */
#define SPI_DELAY		100

#define SPI_READ	0x01
#define SPI_DATA	0x02
#define SPI_W_CMD	(0x70)
#define SPI_W_DAT	(0x70|SPI_DATA)
#define SPI_R_CMD	(0x70|SPI_READ)
#define SPI_R_DAT	(0x70|SPI_READ|SPI_DATA)
#define CMD_DELAY	0xf5

/* Code from fastboot */
#define LCD_RST_HI			gpio_set_value(GPIO_LCD_RST, 1)
#define LCD_RST_LO			gpio_set_value(GPIO_LCD_RST, 0)
#define LCD_SPI_CS_HI		gpio_set_value(GPIO_SPI_CS, 1)
#define LCD_SPI_CS_LO		gpio_set_value(GPIO_SPI_CS, 0)
#define LCD_SPI_CLK_HI		gpio_set_value(GPIO_SPI_CLK, 1)
#define LCD_SPI_CLK_LO		gpio_set_value(GPIO_SPI_CLK, 0)
#define LCD_SPI_SET_DI(x)	gpio_set_value(GPIO_SPI_DI, x)

#ifdef CONFIG_PMIC8058_PWM
static struct pwm_device *bl_pwm;
#define PWM_PERIOD 32768	/* ns, period of 37Khz */
#endif

#ifdef CONFIG_PMIC8058_PWM
void set_pwm(int bl_level)
{
	int duty_level;
	if (bl_pwm) {
		duty_level = (PWM_PERIOD / 256);
		pwm_config(bl_pwm, duty_level * bl_level, PWM_PERIOD);
		pwm_enable(bl_pwm);
	}
}
#endif

unsigned char LCD_ON_Flag = 0;

void send_spi(unsigned char spi_data)
{
	int bit;
	unsigned char mask;

	LCD_SPI_SET_DI(1);
	for (bit = 7; bit >= 0; bit--) {
		mask = (unsigned char)1 << bit;
		LCD_SPI_CLK_LO;
		ndelay(SPI_DELAY);

		if (spi_data & mask)
			LCD_SPI_SET_DI(1);
		else
			LCD_SPI_SET_DI(0);
		ndelay(SPI_DELAY);

		LCD_SPI_CLK_HI;
		ndelay(SPI_DELAY);
	}
	LCD_SPI_SET_DI(1);
}

void spi_gen(unsigned char spi_cmd, unsigned char spi_data)
{
	if (spi_cmd == SPI_W_CMD || spi_cmd == SPI_W_DAT) {
		LCD_SPI_CS_LO;
		ndelay(SPI_DELAY);
		send_spi(spi_cmd);
		send_spi(spi_data);
		LCD_SPI_CS_HI;
		ndelay(SPI_DELAY);
		LCD_SPI_CS_LO;
	} else if (spi_cmd == CMD_DELAY) {
		ndelay(SPI_DELAY);
		LCD_SPI_CS_HI;
		msleep(spi_data);
	} else {
		pr_err("[LG_LCDC] SPI cmd = 0x%x not supported\n", spi_cmd);
	}
}

static void send_poweron_sequence(void)
{
	/* display inversion off */
	spi_gen(SPI_W_CMD, 0x20);

	/* Set address mode, no flip */
	spi_gen(SPI_W_CMD, 0x36);
	spi_gen(SPI_W_DAT, 0x00);

	/* Pixel format, 24bits */
	spi_gen(SPI_W_CMD, 0x3A);
	spi_gen(SPI_W_DAT, 0x77);

	/* panel character setting */
	spi_gen(SPI_W_CMD, 0xB2);
	spi_gen(SPI_W_DAT, 0x00);
	spi_gen(SPI_W_DAT, 0x00);		/* 1024 line */

	/* panel driving setting */
	spi_gen(SPI_W_CMD, 0xB3);
	spi_gen(SPI_W_DAT, 0x00);		/* column inversion */

	/* display mode control */
	spi_gen(SPI_W_CMD, 0xB4);
	spi_gen(SPI_W_DAT, 0x04);		/* dither enable */

	/* display control 1 */
	spi_gen(SPI_W_CMD, 0xB5);
	spi_gen(SPI_W_DAT, 0x42);
	spi_gen(SPI_W_DAT, 0x10);
	spi_gen(SPI_W_DAT, 0x10);
	spi_gen(SPI_W_DAT, 0x00);
	spi_gen(SPI_W_DAT, 0x00);

	spi_gen(SPI_W_CMD, 0xB6);
	if (hw_ver <= ACER_HW_VERSION_DVT1) {
		spi_gen(SPI_W_DAT, 0x03);
	} else {
		spi_gen(SPI_W_DAT, 0x1B);
	}
	spi_gen(SPI_W_DAT, 0x48);
	spi_gen(SPI_W_DAT, 0x3c);
	spi_gen(SPI_W_DAT, 0x0f);
	spi_gen(SPI_W_DAT, 0x0f);
	spi_gen(SPI_W_DAT, 0x0f);

	spi_gen(SPI_W_CMD, 0xC0);
	spi_gen(SPI_W_DAT, 0x00);
	spi_gen(SPI_W_DAT, 0x1C);

	spi_gen(SPI_W_CMD, 0xC3);
	spi_gen(SPI_W_DAT, 0x07);
	spi_gen(SPI_W_DAT, 0x0c);
	spi_gen(SPI_W_DAT, 0x0b);
	spi_gen(SPI_W_DAT, 0x0b);
	spi_gen(SPI_W_DAT, 0x04);


	spi_gen(SPI_W_CMD, 0xC4);
	spi_gen(SPI_W_DAT, 0x12);
	spi_gen(SPI_W_DAT, 0x24);
	spi_gen(SPI_W_DAT, 0x18);
	spi_gen(SPI_W_DAT, 0x18);
	spi_gen(SPI_W_DAT, 0x02);
	spi_gen(SPI_W_DAT, 0x7b);

	spi_gen(SPI_W_CMD, 0xC5);
	if (hw_ver <= ACER_HW_VERSION_DVT1) {
		spi_gen(SPI_W_DAT, 0x62);
	} else {
		spi_gen(SPI_W_DAT, 0x65);
	}

	spi_gen(SPI_W_CMD, 0xC6);
	spi_gen(SPI_W_DAT, 0x42);
	spi_gen(SPI_W_DAT, 0x63);

	/* Setting Gamma Table */
	if (hw_ver <= ACER_HW_VERSION_DVT1) {
		spi_gen(SPI_W_CMD, 0xD0);
		spi_gen(SPI_W_DAT, 0x10);
		spi_gen(SPI_W_DAT, 0x05);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x01);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD1);
		spi_gen(SPI_W_DAT, 0x10);
		spi_gen(SPI_W_DAT, 0x05);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x01);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD2);
		spi_gen(SPI_W_DAT, 0x10);
		spi_gen(SPI_W_DAT, 0x05);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x01);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD3);
		spi_gen(SPI_W_DAT, 0x10);
		spi_gen(SPI_W_DAT, 0x05);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x01);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD4);
		spi_gen(SPI_W_DAT, 0x10);
		spi_gen(SPI_W_DAT, 0x05);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x01);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD5);
		spi_gen(SPI_W_DAT, 0x10);
		spi_gen(SPI_W_DAT, 0x05);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x01);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);
	} else {
		spi_gen(SPI_W_CMD, 0xD0);
		spi_gen(SPI_W_DAT, 0x30);
		spi_gen(SPI_W_DAT, 0x06);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD1);
		spi_gen(SPI_W_DAT, 0x30);
		spi_gen(SPI_W_DAT, 0x06);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD2);
		spi_gen(SPI_W_DAT, 0x30);
		spi_gen(SPI_W_DAT, 0x06);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD3);
		spi_gen(SPI_W_DAT, 0x30);
		spi_gen(SPI_W_DAT, 0x06);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD4);
		spi_gen(SPI_W_DAT, 0x30);
		spi_gen(SPI_W_DAT, 0x06);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);

		spi_gen(SPI_W_CMD, 0xD5);
		spi_gen(SPI_W_DAT, 0x30);
		spi_gen(SPI_W_DAT, 0x06);
		spi_gen(SPI_W_DAT, 0x77);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x20);
		spi_gen(SPI_W_DAT, 0x00);
		spi_gen(SPI_W_DAT, 0x02);
	}

	/* sleep out */
	spi_gen(SPI_W_CMD, 0x11);

	/* Display on */
	spi_gen(SPI_W_CMD, 0x29);

	if (hw_ver <= ACER_HW_VERSION_DVT1) {
		/* Disable PWM output */
		spi_gen(SPI_W_CMD, 0x53);
		spi_gen(SPI_W_DAT, 0x00);
	} else {
		/* CABC min brightness */
		spi_gen(SPI_W_CMD, 0x5e);
		spi_gen(SPI_W_DAT, cabc_min_value);

		/* CABC control */
		spi_gen(SPI_W_CMD, 0x55);
		spi_gen(SPI_W_DAT, cabc_mode);	/* 0x02:still/0x03:moving/0x00:CABC off */

		/* Dimming control */
		spi_gen(SPI_W_CMD, 0x53);
		spi_gen(SPI_W_DAT, 0x24);   /* 0x2c:dimming on/0x24:dimming off */
	}

	/* Set PWM polarity and freq.*/
	spi_gen(SPI_W_CMD, 0xC8);
	spi_gen(SPI_W_DAT, 0x82);   /*  dimming step, default 0x82 */
	spi_gen(SPI_W_DAT, 0x03);
}


static void send_poweroff_sequence(void)
{
	/* display off */
	spi_gen(SPI_W_CMD, 0x28);

	if (hw_ver > ACER_HW_VERSION_DVT1) {
		/* disable CABC */
		spi_gen(SPI_W_CMD, 0x53);
		spi_gen(SPI_W_DAT, 0x00);
	}

	/* enter sleep mode */
	spi_gen(SPI_W_CMD, 0x10);
}

/* LG LCM power on/off SPI commands */
void send_lcd_spi_cmd(int bOnOff)
{
	if (bOnOff == 1) {
		LCD_SPI_CS_HI;
		LCD_RST_HI;
		mdelay(1);
		LCD_RST_LO;
		mdelay(1);
		LCD_RST_HI;
		mdelay(10);

		/* send power on sequence */
		send_poweron_sequence();
	} else {
		/* send power off sequence */
		send_poweroff_sequence();
		LCD_SPI_CS_HI;
		LCD_RST_LO;
	}
}

static int lcdc_lg_panel_on(struct platform_device *pdev)
{
	pr_err("[LG_LCDC] %s ++ entering no delay\n", __func__);

	send_lcd_spi_cmd(1);			/* Send Power ON data to LCD */
	LCD_ON_Flag = 1;

	pr_err("[LG_LCDC] %s -- leaving\n", __func__);
	return 0;
}

static int lcdc_lg_panel_off(struct platform_device *pdev)
{
	pr_err("[LG_LCDC] %s ++ entering no delay\n", __func__);

	send_lcd_spi_cmd(0);			/* Send Power OFF data to LCD */
	LCD_ON_Flag = 0;

	pr_err("[LG_LCDC] %s -- leaving\n", __func__);
	return 0;
}

static void lcdc_lg_lcd_set_backlight(struct msm_fb_data_type *mfd)
{
	/* backlight function */
	if (hw_ver <= ACER_HW_VERSION_DVT1)
		if (mfd->bl_level > 0 && mfd->bl_level < 30 )
			mfd->bl_level = 30;
	pr_err("[LG_LCDC] bl_val = %d\n", mfd->bl_level);

	/* send backlight command */
	if (hw_ver <= ACER_HW_VERSION_DVT1) {
		set_pwm(mfd->bl_level);
	} else {
		if (LCD_ON_Flag == 1) {
			spi_gen(SPI_W_CMD, 0x51);
			spi_gen(SPI_W_DAT, mfd->bl_level);
			/* Turn off PWM output if brightness equals to zero */
			spi_gen(SPI_W_CMD, 0x53);
			if (mfd->bl_level == 0) {
				spi_gen(SPI_W_DAT, 0x00);
			} else {
				spi_gen(SPI_W_DAT, 0x24);
			}
		}
	}
}

void get_hw_version(void)
{
	acer_smem_flag_t *acer_smem_flag;

	acer_smem_flag = (acer_smem_flag_t *)(smem_alloc(SMEM_ID_VENDOR0,
		sizeof(acer_smem_flag_t)));
	if (acer_smem_flag == NULL) {
		pr_err("[LG_LCDC] alloc acer_smem_flag failed!\n");
		hw_ver = ACER_HW_VERSION_INVALID;
	} else {
		hw_ver = acer_smem_flag->acer_hw_version;
		pr_err("[LG_LCDC] hw_ver = %d\n", hw_ver);
	}
}

static struct msm_fb_panel_data lcdc_lg_panel_data = {
	.on = lcdc_lg_panel_on,
	.off = lcdc_lg_panel_off,
	.set_backlight = lcdc_lg_lcd_set_backlight,
};

static struct msm_panel_info pinfo;

static int __init lcdc_lg_init(void)
{
	int ret;

	pr_err("[LG_LCDC] lcdc_lg_init\n");

	pinfo.xres = 480;
	pinfo.yres = 1024;
	pinfo.type = LCDC_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 36864000;
	pinfo.bl_max = 255;
	pinfo.bl_min = 0;
	pinfo.width = 52;		/* Actual size, in mm */
	pinfo.height = 111;	/* Actual size, in mm */
	pinfo.lcdc.h_back_porch = 60;	/* 59 */
	pinfo.lcdc.h_front_porch = 10;	/* 10 */
	pinfo.lcdc.h_pulse_width = 10;	/* 10 */
	pinfo.lcdc.v_back_porch =  16;	/* 15 */
	pinfo.lcdc.v_front_porch = 16;	/* 15 */
	pinfo.lcdc.v_pulse_width = 16;	/* 15 */
	pinfo.lcdc.border_clr = 0;	/* black */
	pinfo.lcdc.underflow_clr = 0x00;	/* blue */
	pinfo.lcdc.hsync_skew = 0;

#ifdef CONFIG_PMIC8058_PWM
	bl_pwm = pwm_request(2, "backlight");
	if (bl_pwm == NULL || IS_ERR(bl_pwm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_pwm = NULL;
	}
#endif

	set_pwm(0);		/* turn off backlight */
	get_hw_version();

	ret = lcdc_device_register(&pinfo, &lcdc_lg_panel_data);
	if (ret)
		pr_err("[LG_LCDC] %s: failed to register device!\n", __func__);

	return ret;
}

module_param(cabc_min_value,int,S_IRWXU);
module_param(cabc_mode,int,S_IRWXU);
module_init(lcdc_lg_init);

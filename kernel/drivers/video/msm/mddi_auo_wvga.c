/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, and instead of the terms immediately above, this
 * software may be relicensed by the recipient at their option under the
 * terms of the GNU General Public License version 2 ("GPL") and only
 * version 2.  If the recipient chooses to relicense the software under
 * the GPL, then the recipient shall replace all of the text immediately
 * above and including this paragraph with the text immediately below
 * and between the words START OF ALTERNATE GPL TERMS and END OF
 * ALTERNATE GPL TERMS and such notices and license terms shall apply
 * INSTEAD OF the notices and licensing terms given above.
 *
 * START OF ALTERNATE GPL TERMS
 *
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This software was originally licensed under the Code Aurora Forum
 * Inc. Dual BSD/GPL License version 1.1 and relicensed as permitted
 * under the terms thereof by a recipient under the General Public
 * License Version 2.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * END OF ALTERNATE GPL TERMS
 *
 */

/*  This code is based on mddi_orise.c
 *  For Acer A4 Project
 *  LCM Model : AUO H361VL01 WVGA 24 bits 800x480 MDDI interface
 *  LCD Driver IC : Novatek NT35582
 */

#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <mach/vreg.h>
#include <linux/wait.h>

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"

#define A4_BL_TEST_CABC
#define MDDI_AUO_WVGA_ID	1
#define GPIO_LCD_RST		180
#define MDDI_AUO_WVGA_MFR_NAME		0xB9F6
#define MDDI_AUO_WVGA_PRODUCT_CODE	0x5582

#define write_client_reg(__X, __Y)  do {\
	mddi_queue_register_write(__X, __Y, TRUE, 0);\
} while (0)

#define gpio_output_enable(gpio, en) do {\
	gpio_configure(gpio, en == 0 ? GPIOF_INPUT : GPIOF_DRIVE_OUTPUT);\
} while (0)

#define LCD_RST_HI	gpio_set_value(GPIO_LCD_RST,  1)
#define LCD_RST_LO	gpio_set_value(GPIO_LCD_RST,  0)

static int backlight_enable = TRUE;
static int mddi_auo_lcd_on(struct platform_device *pdev);
static int mddi_auo_lcd_off(struct platform_device *pdev);
static int mddi_auo_probe(struct platform_device *pdev);
static int __init mddi_auo_init(void);
wait_queue_head_t wait;
#ifdef CONFIG_ENABLE_LCD_80HZ
/*
 * marcOcram // TechnoLover
 * 
 * Lower T2 means a higher LCD-Frequency
 * 
 */
static int T2 = 250; //default one is 340
#endif

void send_mddi_auo_poweron_sequence(void)
{
	/* exit sleep mode */
	write_client_reg(0x1100, 0x00);
	wait_event_timeout(wait, 0, 12);	// wait 120ms

	/* setting DC/DC */
	write_client_reg(0xC000, 0x86);
	write_client_reg(0xC001, 0x00);
	write_client_reg(0xC002, 0x86);
	write_client_reg(0xC003, 0x00);
	write_client_reg(0xC100, 0x40);
	write_client_reg(0xC200, 0x21);
	write_client_reg(0xC202, 0x02);

	/* setting gamma table */
	write_client_reg(0xE000, 0x0E);
	write_client_reg(0xE200, 0x0E);
	write_client_reg(0xE400, 0x0E);
	write_client_reg(0xE001, 0x14);
	write_client_reg(0xE201, 0x14);
	write_client_reg(0xE401, 0x14);
	write_client_reg(0xE002, 0x29);
	write_client_reg(0xE202, 0x29);
	write_client_reg(0xE402, 0x29);
	write_client_reg(0xE003, 0x3A);
	write_client_reg(0xE203, 0x3A);
	write_client_reg(0xE403, 0x3A);
	write_client_reg(0xE004, 0x1D);
	write_client_reg(0xE204, 0x1D);
	write_client_reg(0xE404, 0x1D);
	write_client_reg(0xE005, 0x30);
	write_client_reg(0xE205, 0x30);
	write_client_reg(0xE405, 0x30);
	write_client_reg(0xE006, 0x61);
	write_client_reg(0xE206, 0x61);
	write_client_reg(0xE406, 0x61);
	write_client_reg(0xE007, 0x3D);
	write_client_reg(0xE207, 0x3D);
	write_client_reg(0xE407, 0x3D);
	write_client_reg(0xE008, 0x22);
	write_client_reg(0xE208, 0x22);
	write_client_reg(0xE408, 0x22);
	write_client_reg(0xE009, 0x2A);
	write_client_reg(0xE209, 0x2A);
	write_client_reg(0xE409, 0x2A);
	write_client_reg(0xE00A, 0x87);
	write_client_reg(0xE20A, 0x87);
	write_client_reg(0xE40A, 0x87);
	write_client_reg(0xE00B, 0x16);
	write_client_reg(0xE20B, 0x16);
	write_client_reg(0xE40B, 0x16);
	write_client_reg(0xE00C, 0x3B);
	write_client_reg(0xE20C, 0x3B);
	write_client_reg(0xE40C, 0x3B);
	write_client_reg(0xE00D, 0x4C);
	write_client_reg(0xE20D, 0x4C);
	write_client_reg(0xE40D, 0x4C);
	write_client_reg(0xE00E, 0x78);
	write_client_reg(0xE20E, 0x78);
	write_client_reg(0xE40E, 0x78);
	write_client_reg(0xE00F, 0x96);
	write_client_reg(0xE20F, 0x96);
	write_client_reg(0xE40F, 0x96);
	write_client_reg(0xE010, 0x4A);
	write_client_reg(0xE210, 0x4A);
	write_client_reg(0xE410, 0x4A);
	write_client_reg(0xE011, 0x4D);
	write_client_reg(0xE211, 0x4D);
	write_client_reg(0xE411, 0x4D);
	write_client_reg(0xE100, 0x0E);
	write_client_reg(0xE300, 0x0E);
	write_client_reg(0xE500, 0x0E);
	write_client_reg(0xE101, 0x14);
	write_client_reg(0xE301, 0x14);
	write_client_reg(0xE501, 0x14);
	write_client_reg(0xE102, 0x29);
	write_client_reg(0xE302, 0x29);
	write_client_reg(0xE502, 0x29);
	write_client_reg(0xE103, 0x3A);
	write_client_reg(0xE303, 0x3A);
	write_client_reg(0xE503, 0x3A);
	write_client_reg(0xE104, 0x1D);
	write_client_reg(0xE304, 0x1D);
	write_client_reg(0xE504, 0x1D);
	write_client_reg(0xE105, 0x30);
	write_client_reg(0xE305, 0x30);
	write_client_reg(0xE505, 0x30);
	write_client_reg(0xE106, 0x61);
	write_client_reg(0xE306, 0x61);
	write_client_reg(0xE506, 0x61);
	write_client_reg(0xE107, 0x3D);
	write_client_reg(0xE307, 0x3D);
	write_client_reg(0xE507, 0x3D);
	write_client_reg(0xE108, 0x22);
	write_client_reg(0xE308, 0x22);
	write_client_reg(0xE508, 0x22);
	write_client_reg(0xE109, 0x2A);
	write_client_reg(0xE309, 0x2A);
	write_client_reg(0xE509, 0x2A);
	write_client_reg(0xE10A, 0x87);
	write_client_reg(0xE30A, 0x87);
	write_client_reg(0xE50A, 0x87);
	write_client_reg(0xE10B, 0x16);
	write_client_reg(0xE30B, 0x16);
	write_client_reg(0xE50B, 0x16);
	write_client_reg(0xE10C, 0x3B);
	write_client_reg(0xE30C, 0x3B);
	write_client_reg(0xE50C, 0x3B);
	write_client_reg(0xE10D, 0x4C);
	write_client_reg(0xE30D, 0x4C);
	write_client_reg(0xE50D, 0x4C);
	write_client_reg(0xE10E, 0x78);
	write_client_reg(0xE30E, 0x78);
	write_client_reg(0xE50E, 0x78);
	write_client_reg(0xE10F, 0x96);
	write_client_reg(0xE30F, 0x96);
	write_client_reg(0xE50F, 0x96);
	write_client_reg(0xE110, 0x4A);
	write_client_reg(0xE310, 0x4A);
	write_client_reg(0xE510, 0x4A);
	write_client_reg(0xE111, 0x4D);
	write_client_reg(0xE311, 0x4D);
	write_client_reg(0xE511, 0x4D);

	/* SET RGB I/F 24 bits */
	write_client_reg(0x3A00, 0x77);

	/* PWM CLK = 5500KHz/(256*Div), Setting Div = 0x01 ==> 21KHz */
	write_client_reg(0x6A02, 0x01);

	/* PWM backlight control, turn on */
	write_client_reg(0x5300, 0x2C);

	/* Dimming Step, 8 step */
	write_client_reg(0x5305, 0x02);

#ifdef A4_BL_TEST_CABC
	/* Enable LABC */
	write_client_reg(0x5302, 0x01);

	/* Disable Force PWM ctrl */
	write_client_reg(0x6A17, 0x00);

	/* setting CABC mode, still picture mode */
	write_client_reg(0x5500, 0x02);
#else
	/* Enable Force PWM ctrl */
	write_client_reg(0x6A17, 0x01);
#endif

	/* FTE, enable vsync */
	write_client_reg(0x3500, 0x10);

#ifdef CONFIG_ENABLE_LCD_80HZ
	write_client_reg(0xB101, (0xFF00 & T2) >> 8);
	write_client_reg(0xB102, 0x00FF & T2);
#endif

	/* display on */
	write_client_reg(0x2900, 0x00);
};

void send_mddi_auo_poweroff_sequence(void)
{
	/* FTE, disable vsync */
	write_client_reg(0x3400, 0x00);

	/* PWM backlight control, turn off */
	write_client_reg(0x5300, 0x0C);

	/* display off */
	write_client_reg(0x2800, 0x00);

	/* enter sleep mode */
	write_client_reg(0x1000, 0x00);
}

void send_lcd_mddi_cmd(int bOnOff)
{
	if (bOnOff == 1) {
		LCD_RST_HI;
		wait_event_timeout(wait, 0, 1);
		LCD_RST_LO;
		wait_event_timeout(wait, 0, 1);
		LCD_RST_HI;
		wait_event_timeout(wait, 0, 2);		// delay 20ms

		/* send power on sequence */
		send_mddi_auo_poweron_sequence();
	} else {
		/* send power off sequence */
		send_mddi_auo_poweroff_sequence();

		LCD_RST_LO;
		mdelay(1);
	}
}

void auo_gpio_init(void)
{
	pr_debug("[LCD_MDDI] %s (%d) ++ entering\n", __func__, __LINE__);

	gpio_tlmm_config(GPIO_CFG(GPIO_LCD_RST,  0, GPIO_CFG_OUTPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(GPIO_LCD_RST,     1);

	pr_debug("[LCD_MDDI] %s (%d) -- leaving\n", __func__, __LINE__);
}

static int mddi_auo_lcd_on(struct platform_device *pdev)
{
	pr_emerg("[LCD_MDDI] %s (%d) ++ enter\n", __func__, __LINE__);

	send_lcd_mddi_cmd(1);	/* send initial command to LCD driver IC */
	backlight_enable = TRUE;

	pr_emerg("[LCD_MDDI] %s (%d) -- leave\n", __func__, __LINE__);
	return 0;
}

static int mddi_auo_lcd_off(struct platform_device *pdev)
{
	pr_emerg("[LCD_MDDI] %s (%d) ++ enter\n", __func__, __LINE__);

	backlight_enable = FALSE;
	send_lcd_mddi_cmd(0);	/* send power off command to LCD driver IC */

	pr_emerg("[LCD_MDDI] %s (%d) -- leave\n", __func__, __LINE__);
	return 0;
}

static void mddi_auo_set_backlight(struct msm_fb_data_type *mfd)
{
	if (mfd) {
		pr_emerg("[LCD_MDDI] bl_val=%d, bl_en=%d\n", mfd->bl_level, backlight_enable);

		/* Check MDDI power status before using it for sending command */
#ifdef A4_BL_TEST_CABC
		if (backlight_enable == TRUE)
			write_client_reg(0x5100, mfd->bl_level);
#else
		if (backlight_enable == TRUE)
			write_client_reg(0x6A18, mfd->bl_level);
#endif
	}
}

static struct platform_driver this_driver = {
	.probe  = mddi_auo_probe,
	.driver = {
		.name   = "mddi_auo_wvga",
	},
};

static struct msm_fb_panel_data mddi_auo_panel_data = {
	.on  = mddi_auo_lcd_on,
	.off = mddi_auo_lcd_off,
	.set_backlight = mddi_auo_set_backlight,
};

static struct platform_device this_device = {
	.name = "mddi_auo_wvga",
	.id   = MDDI_AUO_WVGA_ID,
	.dev  = {
		.platform_data = &mddi_auo_panel_data,
	}
};

static int mddi_auo_probe(struct platform_device *pdev)
{
	pr_debug("[LCD_MDDI] %s (%d) ++ enter\n", __func__, __LINE__);

	msm_fb_add_device(pdev);

	pr_debug("[LCD_MDDI] %s (%d) -- leave\n", __func__, __LINE__);
	return 0;
}

static int __init mddi_auo_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	pr_debug("[LCD_MDDI] %s (%d) ++ enter\n", __func__, __LINE__);

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
	u32 id;

	pr_debug("start detect MDDI panel automatically (%d)\n", __LINE__);

	ret = msm_fb_detect_client("mddi_auo_wvga");
	if (ret == -ENODEV) {
		pr_err("detect mddi_auo_wvga error (%d)\n", __LINE__);
		return 0;
	}

	pr_debug("ret = %lx of msm_fb_detect_client\n", ret);
	if (ret) {
		/* it will return MFR Name and Product Code, Novatek NT35582 */
		id = mddi_get_client_id();
		pr_debug("client id = 0x%x\n", id);
		if (((id >> 16) != MDDI_AUO_WVGA_MFR_NAME)
		|| ((id & 0xffff) != MDDI_AUO_WVGA_PRODUCT_CODE))
			return 0;
	}
#endif

	ret = platform_driver_register(&this_driver);

	if (!ret) {
		pr_debug("[LCD_MDDI] platform driver register OK\n");

		pinfo = &mddi_auo_panel_data.panel_info;
		pinfo->xres = 480;		/* resolution of x */
		pinfo->yres = 800;		/* resolution of y */
		pinfo->bpp = 16;		/* color depth */
		pinfo->type = MDDI_PANEL;	/* panel type */
		pinfo->pdest = DISPLAY_1;	/* assign MDDI panel ID */
		pinfo->wait_cycle = 0;		/* write clock, for EBI2 only */
		pinfo->fb_num = 2;		/* allocate for dubble buffer */
		pinfo->bl_max = 255;		/* backlight MAX value */
		pinfo->bl_min = 0;		/* backlight MIN value */
#ifdef CONFIG_ENABLE_VSYNC
		pinfo->clk_rate = 184320000;	/* clock setting, MAX 200MHz */
		pinfo->clk_min =  184320000;
		pinfo->clk_max =  184320000;
#else
		pinfo->clk_rate = 200000000;	/* clock setting, MAX 200MHz */
		pinfo->clk_min =  200000000;
		pinfo->clk_max =  200000000;
#endif
		pinfo->width = 47;		/* Actual size, in mm */
		pinfo->height = 79;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
#ifdef CONFIG_ENABLE_VSYNC
		pinfo->lcd.vsync_enable = TRUE;
		pinfo->lcd.refx100 = 5800;
		pinfo->lcd.v_back_porch = 2;
		pinfo->lcd.v_front_porch = 2;
		pinfo->lcd.v_pulse_width = 2;
		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = 0;
#else
		pinfo->lcd.vsync_enable = FALSE;
		pinfo->lcd.hw_vsync_mode = FALSE;
#endif

		/* Init GPIOs*/
		auo_gpio_init();

		ret = platform_device_register(&this_device);
		if (ret) {
			pr_err("[LCD_MDDI] platform device register Failed\n");
			platform_driver_unregister(&this_driver);
		} else {
			pr_debug("[LCD_MDDI] platform device register OK\n");
		}

	}
	init_waitqueue_head(&wait);
	pr_debug("[LCD_MDDI] %s (%d) -- leave\n", __func__, __LINE__);
	return ret;
}

module_init(mddi_auo_init);

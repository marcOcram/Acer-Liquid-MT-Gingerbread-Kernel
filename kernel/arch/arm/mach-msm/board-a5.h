/* arch/arm/mach-msm/board-a5.h
 *
 * Copyright (C) 2010 Acer.
 * Author: Haley Teng <Anthony_Chang@acer.com.tw>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __ARCH_ARM_MACH_MSM_BOARD_A5_H
#define __ARCH_ARM_MACH_MSM_BOARD_A5_H

#include <mach/board.h>

#define A5_GPIO_KEY_VOL_UP              26
#define A5_GPIO_KEY_VOL_DOWN            48
#define A5_GPIO_CAM_BTN_STEP1           34
#define A5_GPIO_CAM_BTN_STEP2           42

#define A5_GPIO_CYP_TP_IRQ              148
#define A5_GPIO_CYP_TP_RST              181
#define A5_GPIO_CYP_TP_ISSP_SCLK        150
#define A5_GPIO_CYP_TP_ISSP_SDATA       151

/* HDMI GPIO Pin Definition */
#define A5_N_HDMI_INT                   40
#define A5_GPIO_VOUT_5V_EN              49

#define A5_MCU_IRQ                      142

#endif /* __ARCH_ARM_MACH_MSM_BOARD_A5_H */

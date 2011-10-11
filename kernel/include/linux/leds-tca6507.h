/*
 * tca6507.h - platform data structure for tca6507 led controller
 *
 * Copyright (C) 2010 Brad Chen <ChunHung Chen@acer.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 */

#ifndef __LINUX_TCA6507_H
#define __LINUX_TCA6507_H

#define TCA6507_NAME                       "tca6507"

/* The max support is 7 */
#define TCA6507_MAX_OUTPUT                  7

/* TCA 6507 register definition */
#define TCA6507_REG_SELECT0                 0x00
#define TCA6507_REG_SELECT1                 0x01
#define TCA6507_REG_SELECT2                 0x02
#define TCA6507_REG_FADE_ON                 0x03
#define TCA6507_REG_FULLY_ON                0x04
#define TCA6507_REG_FADE_OFF                0x05
#define TCA6507_REG_FIRST_FULLY_OFF         0x06
#define TCA6507_REG_SECOND_FULLY_OFF        0x07
#define TCA6507_REG_MAX_INTENSITY           0x08
#define TCA6507_REG_ONE_SHOT_OR_MASTER      0x09
#define TCA6507_REG_INITIAL                 0x0A

#define TCA6507_AUTO_INCREMENT              0x10

/* TCA 6507 port setting */
#define TCA6507_PORT_LED_OFF                0x00    /* LED off (high impedance) */
#define TCA6507_PORT_LED_ON_STEADILY_PWM0   0x02    /* LED on steadily with maximum intensity value of PWM0 (ALD value or BRIGHT_F0 value) */
#define TCA6507_PORT_LED_ON_STEADILY_PWM1   0x03    /* LED on steadily with maximum intensity value of PWM1 (ALD value or BRIGHT_F1 value) */
#define TCA6507_PORT_LED_ON_FULLY           0x04    /* LED fully on (output low). Can be used as general-purpose output */
#define TCA6507_PORT_LED_ON_ONE_SHOT        0x05    /* LED on at brightness set by One Shot / Master Intensity register */
#define TCA6507_PORT_LED_BLINK_PWM0         0x06    /* LED blinking with intensity characteristics of BANK0 (PWM0) */
#define TCA6507_PORT_LED_BLINK_PWM1         0x07    /* LED blinking with intensity characteristics of BANK1 (PWM1) */

/* TCA 6507 time setting */
#define TCA6507_TIME_0                      0x00    /* 0 ms */
#define TCA6507_TIME_1                      0x01    /* 64 ms */
#define TCA6507_TIME_2                      0x02    /* 128 ms */
#define TCA6507_TIME_3                      0x03    /* 192 ms */
#define TCA6507_TIME_4                      0x04    /* 256 ms */
#define TCA6507_TIME_5                      0x05    /* 384 ms */
#define TCA6507_TIME_6                      0x06    /* 512 ms */
#define TCA6507_TIME_7                      0x07    /* 768 ms */
#define TCA6507_TIME_8                      0x08    /* 1024 ms */
#define TCA6507_TIME_9                      0x09    /* 1536 ms */
#define TCA6507_TIME_10                     0x0a    /* 2048 ms */
#define TCA6507_TIME_11                     0x0b    /* 3072 ms */
#define TCA6507_TIME_12                     0x0c    /* 4096 ms */
#define TCA6507_TIME_13                     0x0d    /* 5760 ms */
#define TCA6507_TIME_14                     0x0e    /* 8128 ms */
#define TCA6507_TIME_15                     0x0f    /* 16320 ms */

#define TCA6507_MAX_INTENSITY               0x0F

#define TCA6507_MAX_ALD_VALUE               0x0F

/**
 * If these definitions are modified,
 * lights_a4.c in hardware is also need to be modified.
 */
enum tca6507_state {
	TCA6507_OFF        = 0,
	TCA6507_ON         = 1,
	TCA6507_FAST_BLINK = 2,
	TCA6507_SLOW_BLINK = 3,
};

struct tca6507_pwm_config {
	uint8_t fade_on;
	uint8_t fully_on;
	uint8_t fade_off;
	uint8_t fir_fully_off;
	uint8_t sec_fully_off;
	uint8_t max_intensity;
};

struct tca6507_pin_config {
	const char *name;
	uint8_t	output_pin;
};

struct tca6507_platform_data {
	struct tca6507_pwm_config pwm0;
	struct tca6507_pwm_config pwm1;
	struct tca6507_pin_config *pin_config;
	uint8_t num_output_pins;
	uint8_t	gpio_enable;
};

#endif

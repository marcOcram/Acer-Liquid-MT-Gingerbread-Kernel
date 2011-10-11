#ifndef __LEDS_MSP430_H__
#define __LEDS_MSP430_H__

struct led_mcu_data {
	struct led_classdev ldev;
	int		id;
	enum   led_brightness brightness;
	char   control_flag;
};

struct leds_msp430_platform_data {
	int	num_leds;
	struct led_mcu_data *leds;
};

enum mcu_leds {
	LED_NEW_EVENT_ID = 0,
	LED_BATTERY_CHARGE_ID,
	LED_BATTERY_FULL_ID,
	LED_KEYPAD_ID,
};


#endif

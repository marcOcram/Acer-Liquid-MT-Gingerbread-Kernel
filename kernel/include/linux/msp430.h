#ifndef __LINUX_MSP430_H
#define __LINUX_MSP430_H

#define MSP430_DRIVER_NAME          "msp430"

/* Command Register list */
#define REG_SYSTEM_VERSION          0x00
#define REG_WHITE_LED_PWM           0x02
#define REG_RED_LED_PWM             0x03
#define REG_GREEN_LED_PWM           0x04
#define REG_BATT_LOW_LEVEL          0x14
#define REG_MASK_INTERRUPT          0x22
#define REG_INTERRUPT               0x23
#define REG_BATT_SOC                0x25  /* capacity percentage */
#define REG_BATT_CAP_L              0x26
#define REG_BATT_CAP_H              0x27
#define REG_BATT_TEMP_L             0x28
#define REG_BATT_TEMP_H             0x29
#define REG_BATT_VOL_L              0x2A
#define REG_BATT_VOL_H              0x2B
#define REG_BATT_AVG_CUR_L          0x2C
#define REG_BATT_AVG_CUR_H          0x2D
#define REG_BATT_GAUGE_ADR          0x2F
#define REG_BATT_GAUGE_DAT          0x30
#define REG_KEYBOARD_DATA           0x31
#define REG_KEYPAD_PWM              0x33
#define REG_LED_CTL                 0x34
#define REG_BAT_STATUS              0x35
#define REG_IR_DATA                 0x36
#define REG_CAHRGER_STATUS          0x37

/* Interrupt Event Register*/
#define INT_FULL_CHARGE_EVENT       (1<<0)
#define INT_BATCAP_EVENT            (1<<1)
#define INT_BATLOW_EVENT            (1<<2)
#define INT_BATZERO_EVENT           (1<<3)
#define INT_BATLOSS_EVENT           (1<<4)
#define INT_GAUGE_EVENT             (1<<5)
#define INT_QWKEY_EVENT             (1<<6)
#define INT_TEMP_EVENT              (1<<7)

/* LED status control */
#define LED_NEW_EVENT                0x08
#define LED_LOW_BATTERY              0x10
#define LED_BATTERY_CHARGE           0x01
#define LED_BATTERY_CHARGE_COMPLETED 0x03
#define LED_BATTERY_OVER_TEMPERATURE 0x05
#define LED_BATTERY_FAULT            0x07
#define LED_KEYPAD                   0x06

#define SYS_MODE_DARK                0x00
#define SYS_MODE_SUSPEND             0x10
#define SYS_MODE_RESERVED            0x20
#define SYS_MODE_NORMAL              0x30

#define BAT_FLAG_LOST                0x10

#define MCU_LED_OFF                  0
#define MCU_LED_ON                   0xff

#define GAUGE_READ                  (0<<7)
#define GAUGE_WRITE                 (1<<7)
#define GAUGE_IT_EN_MASK  0x01

#define MCU_READ	1
#define MCU_WRITE	2

/* The led brightness should be the same as bootloader */
#define PVT_WHITE_LED_LIGHT	0x2
#define PVT_RED_LED_LIGHT	0x3
#define PVT_GREEN_LED_LIGHT	0x1

extern int led_control(int, int);
#endif

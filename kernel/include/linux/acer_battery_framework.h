#ifndef ACER_BATTERY_FRAME_H
#define ACER_BATTERY_FRAME_H

/* Battery information */
struct _batt_info {
	unsigned char  cap_percent;
	short temperature;
	unsigned short voltage;
};

/*
 *This enum contains defintions of the charger hardware type
 */
#define CHARGER_TYPE_NONE	0/*The charger is removed*/
#define CHARGER_TYPE_WALL	1/*AC charger*/
#define CHARGER_TYPE_USB_PC	2/*PC USB charger*/
#define CHARGER_TYPE_USB_WALL	3
#define CHARGER_TYPE_USB_CARKIT	4
#define CHARGER_TYPE_INVALID	5

struct _batt_func{
	/*physical layer --> acer battery frame */
	int (*get_battery_info)(struct _batt_info *);
	void (*early_suspend)(void);
	void (*late_resume)(void);

	/*acer battery frame --> physical layer */
	void (*battery_isr_hander)(unsigned int flag);
	int (*get_charger_type)(void);
	struct power_supply *batt_supply;
};

int register_bat_func(struct _batt_func *);

#define FLAG_BATT_LOST	(1<<0)
#define FLAG_BATT_ZERO	(1<<1)
#define FLAG_BATT_CAP_CHANGE	(1<<2)

#define BATTERY_LOW            	2800
#define BATTERY_HIGH           	4300

#ifdef CONFIG_BATTERY_MSM_A4
/* batt module use */
#define MODULE_INIT	1<<0
#define MODULE_WIFI	1<<1
#define MODULE_CAMERA	1<<2
#define MODULE_PHONE_CALL	1<<3
/*batt_module_enable(MODULE_WIFI, true) means wifi is enabled*/
extern void batt_module_enable(int module, bool state) __attribute__((weak));
#endif

/* report battery information to other driver */
extern int report_batt_info(struct _batt_info *) __attribute__((weak));
#endif

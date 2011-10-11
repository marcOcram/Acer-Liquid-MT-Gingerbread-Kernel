#ifndef __SMB136_A5_H_
#define __SMB136_A5_H_

/*control commands*/
enum smb_commands{
	ALLOW_VOLATILE_WRITE,
	INPUT_CURRENT_LIMIT,
	COMPLETE_CHARGER_TIMEOUT,
	AICL_DETECTION_THRESHOLD,
	POWER_DETECTION,
	RD_FULL_CHARGING,
	USBIN_MODE,
	PIN_CONTROl,
	RD_IS_CHARGING,
	RD_CHG_TYPE,
	STAT_OUT_DISABLE,
};

#define REG00        0x00
#define REG01        0x01
#define REG03        0x03
#define REG05        0x05
#define REG06        0x06
#define REG09        0x09
#define REG31        0x31
#define REG34        0x34
#define REG36        0x36

//reg 0x01
#define MA_900       0x40
#define MA_1000      0x60
#define MA_1100      0x80
#define MA_1200      0xA0
#define VOL_425      0x00
#define VOL_450      0x01
#define VOL_475      0x02
#define VOL_500      0x03

//reg 0x05
#define PIN_CTL      0x10
#define I2C_CTL      0x00

//reg 0x06
#define POWER_DET_EN   0x0
#define POWER_DET_DIS  0x20

//reg 0x09
#define MIN_382      0x00
#define MIN_764      0x04
#define MIN_1527     0x08
#define MIN_DISABLE  0xc0

//reg 0x31
#define VOLATILE_WRITE  0x80
#define BAT_CHARGE_EN   0x00
#define BAT_CHARGE_DIS  0x10
#define USB_IN_100MA    0x00
#define USB_IN_500MA    0x08
#define USB_5_1_MODE    0x00
#define USB_HC_MODE     0x04
#define STAT_DISABLE    0x01

//reg 0x34
#define CHG_TYPE_USB    0x00
#define CHG_TYPE_AC     0x01

//reg 0x36
#define CHARGER_TERMINATION   0x40
#define IS_CHARGE             0x06

extern char smb136_control(enum smb_commands command, char data);
extern int smb136_recharge(void);
#endif

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/kernel.h>
#include "adxl346.h"
#include "adxl346_config.h"

#define I2C_WRITE_LEN 2

int adxl346_i2c_write(unsigned char reg_addr, unsigned char *data , struct adxl346_data* adxl346_data)
{
	int nSetRealLen=0;
	unsigned char buf[I2C_WRITE_LEN] = {0};
	if (NULL == adxl346_data->client) {
		return -1;
	}
	buf[0] = reg_addr;
	buf[1] = data[0];
	nSetRealLen = i2c_master_send(adxl346_data->client, buf, I2C_WRITE_LEN);
	if (I2C_WRITE_LEN != nSetRealLen) {
		pr_err("[adxl346] i2c_write --> Send reg. info error\n");
	}
	return nSetRealLen - 1;
}

int adxl346_i2c_read(unsigned char reg_addr, unsigned char *data, unsigned char len,
	struct adxl346_data* adxl346_data)
{
	int nGetRealLen=0;
	unsigned char buf[1] = {0};
	if (NULL == adxl346_data->client) {
		return -1;
	}
	//Send target reg. info.
	buf[0] = reg_addr;
	if(1 != i2c_master_send(adxl346_data->client, buf, 1)) {
		pr_err("[adxl346] i2c_read --> Send reg. info error\n");
		return -1;
	}
	//Get response data and set to buf
	nGetRealLen = i2c_master_recv(adxl346_data->client, data, len);
	if (len != nGetRealLen) {
		pr_err("[adxl] i2c_read --> get response error\n");
		return nGetRealLen;
	}
	return len;
}
int adxl346_Initial(struct adxl346_data* adxl346_data)
{
	unsigned char buf[1];
	buf[0] = 0x08;
	adxl346_i2c_write(0x2D,buf,adxl346_data);
	buf[0] = 0x01;
	adxl346_i2c_write(0x31,buf,adxl346_data);
	buf[0] = XL345_RATE_50;
	adxl346_i2c_write( XL345_BW_RATE,buf, adxl346_data);
	return 1;
}
int adxl346_read_accel_xyz(adxl346_acc_t* data, struct adxl346_data* adxl346_data)
{
	int i2c_r_status = 0;
	unsigned char buf[6];
	unsigned char DataX_High, DataX_Low;
	unsigned char DataY_High, DataY_Low;
	unsigned char DataZ_High, DataZ_Low;
	i2c_r_status = adxl346_i2c_read(XL345_DATAX0, buf, 6, adxl346_data);
	DataX_Low = buf[0];
	DataX_High = buf[1];
	DataX_High &= 0x03;
	DataY_Low = buf[2];
	DataY_High = buf[3];
	DataX_High &= 0x03;
	DataZ_Low = buf[4];
	DataZ_High = buf[5];
	DataX_High &= 0x03;
	data->x = DataX_High;
	data->x = (data->x << 8) | DataX_Low;
	data->y = DataY_High;
	data->y = (data->y << 8) | DataY_Low;
	data->z = DataZ_High;
	data->z = (data->z << 8) | DataZ_Low;
	return i2c_r_status;
}
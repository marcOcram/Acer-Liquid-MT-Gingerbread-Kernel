/*----------------------------------------------------------------------
  File Name          :
  Author             : MPD Application Team
  Version            : V0.0.1
  Date               : 11/06/2008
  Description        :
  File ID            : $Id: xl345_io.h,v 1.1.1.1 2008/11/10 19:45:46 jlee11 Exp $

  Analog Devices ADXL 345 digital output accellerometer
  with advanced digital features.

  (c) 2008 Analog Devices application support team.
  xxx@analog.com

  ----------------------------------------------------------------------

  The present firmware which is for guidance only aims at providing
  customers with coding information regarding their products in order
  for them to save time.  As a result, Analog Devices shall not be
  held liable for any direct, indirect or consequential damages with
  respect to any claims arising from the content of such firmware and/or
  the use made by customers of the coding information contained herein
  in connection with their products.

----------------------------------------------------------------------*/
#ifndef __adxl346_H
#define __adxl346_H

/* Data for I2C driver */
struct adxl346_data {
	struct i2c_client *client;
	struct work_struct work;
	wait_queue_head_t wait;
};
typedef struct  {
	short x,
	y,
	z;
} adxl346_acc_t;
int adxl346_i2c_write(unsigned char reg_addr, unsigned char *data, struct adxl346_data* adxl346_data);
int adxl346_i2c_read(unsigned char reg_addr, unsigned char *data, unsigned char len,
	struct adxl346_data* adxl346_data);
int adxl346_Initial(struct adxl346_data* adxl346_data);
int adxl346_read_accel_xyz(adxl346_acc_t* data, struct adxl346_data* adxl346_data);

#endif

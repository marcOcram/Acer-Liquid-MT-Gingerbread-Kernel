/* filename: ISSP_Driver_Routines.c */
#include "issp_revision.h"
#ifdef PROJECT_REV_1
/* Copyright 2006-2009, Cypress Semiconductor Corporation.
//
// This software is owned by Cypress Semiconductor Corporation (Cypress)
// and is protected by and subject to worldwide patent protection (United
// States and foreign), United States copyright laws and international
// treaty provisions. Cypress hereby grants to licensee a personal,
// non-exclusive, non-transferable license to copy, use, modify, create
// derivative works of, and compile the Cypress Source Code and derivative
// works for the sole purpose of creating custom software in support of
// licensee product to be used only in conjunction with a Cypress integrated
// circuit as specified in the applicable agreement. Any reproduction,
// modification, translation, compilation, or representation of this
// software except as specified above is prohibited without the express
// written permission of Cypress.
//
// Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND,EXPRESS OR IMPLIED,
// WITH REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Cypress reserves the right to make changes without further notice to the
// materials described herein. Cypress does not assume any liability arising
// out of the application or use of any product or circuit described herein.
// Cypress does not authorize its products for use as critical components in
// life-support systems where a malfunction or failure may reasonably be
// expected to result in significant injury to the user. The inclusion of
// Cypress� product in a life-support systems application implies that the
// manufacturer assumes all risk of such use and in doing so indemnifies
// Cypress against all charges.
//
// Use may be limited by and subject to the applicable Cypress software
// license agreement.
//
--------------------------------------------------------------------------*/

#include "issp_defs.h"
#include "issp_errors.h"
#include "issp_directives.h"
#include <mach/gpio.h>
#include <linux/delay.h>
/* xch - used for debug purposes */
#define SECURITY_DATA	0xFF

extern    unsigned char    bTargetDataPtr;
extern    unsigned char    abTargetDataOUT[TARGET_DATABUFF_LEN];

/****************************** PORT BIT MASKS ******************************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
 ****************************************************************************/
#define SDATA_PIN   0x01
#define SCLK_PIN    0x02
#define XRES_PIN    0x01
#define TARGET_VDD  0x02
#define TP_PIN		0x80




/* ((((((((((((((((((((((( DEMO ISSP SUBROUTINE SECTION )))))))))))))))))))))))
// ((((( Demo Routines can be deleted in final ISSP project if not used   )))))
// ((((((((((((((((((((((((((((((((((((()))))))))))))))))))))))))))))))))))))))

// ============================================================================
// InitTargetTestData()
// !!!!!!!!!!!!!!!!!!FOR TEST!!!!!!!!!!!!!!!!!!!!!!!!!!
// PROCESSOR_SPECIFIC
// Loads a 64-Byte array to use as test data to program target. Ultimately,
// this data should be fed to the Host by some other means, ie: I2C, RS232,
// etc. Data should be derived from hex file.
//  Global variables affected:
//    bTargetDataPtr
//    abTargetDataOUT
 ============================================================================*/
unsigned char get_hex(char digit)
{
	if (digit >= '0' && digit <= '9')
		return digit - '0';
	else if (digit >= 'a' && digit <= 'f')
		return digit - 'a' + 10;
	else if (digit >= 'A' && digit <= 'F')
		return digit - 'A' + 10;
	else
		return -1;
}

void InitTargetTestData(unsigned char bBlockNum, unsigned char bBankNum, const char *buffer, unsigned long count)
{
	for (bTargetDataPtr = 0; bTargetDataPtr < TARGET_DATABUFF_LEN; bTargetDataPtr++)
		abTargetDataOUT[bTargetDataPtr] = 0;

	/* create unique data for each block */
	for (bTargetDataPtr = 0; bTargetDataPtr < TARGET_DATABUFF_LEN; bTargetDataPtr++) {
		if ((bTargetDataPtr % 2) == 0)
			abTargetDataOUT[bTargetDataPtr/2] |= (get_hex(buffer[(bBankNum*2*141+9)+bTargetDataPtr])) << 4;
		/* bTargetDataPtr + bBlockNum + bBankNum; */
		else
			abTargetDataOUT[bTargetDataPtr/2] |= (get_hex(buffer[(bBankNum*2*141+9)+bTargetDataPtr]));
		/* bTargetDataPtr + bBlockNum + bBankNum; */
	}

	for (bTargetDataPtr = 0; bTargetDataPtr < TARGET_DATABUFF_LEN; bTargetDataPtr++) {
		if ((bTargetDataPtr % 2) == 0)
			/* bTargetDataPtr + bBlockNum + bBankNum; */
			abTargetDataOUT[bTargetDataPtr/2 + 64] |= (get_hex(buffer[((bBankNum*2+1)*141+9)+bTargetDataPtr])) << 4;
		else
			/* bTargetDataPtr + bBlockNum + bBankNum; */
			abTargetDataOUT[bTargetDataPtr/2 + 64] |= (get_hex(buffer[((bBankNum*2+1)*141+9)+bTargetDataPtr]));
	}
}



/* ============================================================================
// LoadArrayWithSecurityData()
// !!!!!!!!!!!!!!!!!!FOR TEST!!!!!!!!!!!!!!!!!!!!!!!!!!
// PROCESSOR_SPECIFIC
// Most likely this data will be fed to the Host by some other means, ie: I2C,
// RS232, etc., or will be fixed in the host. The security data should come
// from the hex file.
//   bStart  - the starting byte in the array for loading data
//   bLength - the number of byte to write into the array
//   bType   - the security data to write over the range defined by bStart and
//             bLength
 ============================================================================*/
void LoadArrayWithSecurityData(unsigned char bStart, unsigned char bLength, unsigned char bType)
{
	/* Now, write the desired security-bytes for the range specified */
	/*  pr_info("wly: %s\n", __FUNCTION__); */

/*
	int i = 141*BLOCKS_PER_BANK*2+17+9;

	for (bTargetDataPtr = bStart; bTargetDataPtr < bLength; bTargetDataPtr++) {
		abTargetDataOUT[bTargetDataPtr] = 0;

	for (bTargetDataPtr = bStart; bTargetDataPtr < bLength*2; bTargetDataPtr++) {
		abTargetDataOUT[bTargetDataPtr] = bType;
			if((bTargetDataPtr % 2 ) == 0)
				abTargetDataOUT[bTargetDataPtr/2] |= get_hex(buffer[i+bTargetDataPtr]) << 4;
			else
				abTargetDataOUT[bTargetDataPtr/2] |= get_hex(buffer[i+bTargetDataPtr]);
	}
*/
}


/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// Delay()
// This delay uses a simple "nop" loop. With the CPU running at 24MHz, each
// pass of the loop is about 1 usec plus an overhead of about 3 usec.
//      total delay = (n + 3) * 1 usec
// To adjust delays and to adapt delays when porting this application, see the
// ISSP_Delays.h file.
 ****************************************************************************/
void Delay(unsigned char n)
{
	while (n) {
		asm("nop");
		n -= 1;
	}
}


/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// LoadProgramData()
// The final application should load program data from HEX file generated by
// PSoC Designer into a 64 byte host ram buffer.
//    1. Read data from next line in hex file into ram buffer. One record
//      (line) is 64 bytes of data.
//    2. Check host ram buffer + record data (Address, # of bytes) against hex
//       record checksum at end of record line
//    3. If error reread data from file or abort
//    4. Exit this Function and Program block or verify the block.
// This demo program will, instead, load predetermined data into each block.
// The demo does it this way because there is no comm link to get data.
 ****************************************************************************/
void LoadProgramData(unsigned char bBlockNum, unsigned char bBankNum, const char *buffer, unsigned long count)
{
	/* >>> The following call is for demo use only. <<<
	// Function InitTargetTestData fills buffer for demo
	 */
	InitTargetTestData(bBlockNum, bBankNum, buffer, count);

	/* Note:
	// Error checking should be added for the final version as noted above.
	// For demo use this function just returns VOID.
	 */
}


/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// fLoadSecurityData()
// Load security data from hex file into 64 byte host ram buffer. In a fully
// functional program (not a demo) this routine should do the following:
//    1. Read data from security record in hex file into ram buffer.
//    2. Check host ram buffer + record data (Address, # of bytes) against hex
//       record checksum at end of record line
//    3. If error reread security data from file or abort
//    4. Exit this Function and Program block
// In this demo routine, all of the security data is set to unprotected (0x00)
// and it returns.
// This function always returns PASS. The flag return is reserving
// functionality for non-demo versions.
 ****************************************************************************/
signed char fLoadSecurityData(unsigned char bBankNum, const char *buffer)
{
	/* >>> The following call is for demo use only. <<<
	// Function LoadArrayWithSecurityData fills buffer for demo
	//LoadArrayWithSecurityData(0,SECURITY_BYTES_PER_BANK, SECURITY_DATA);
	//     LoadArrayWithSecurityData(0,SECURITY_BYTES_PER_BANK, 0x00);
	//     LoadArrayWithSecurityData(0,SECURITY_BYTES_PER_BANK, 0xFF);
	// Note:
	// Error checking should be added for the final version as noted above.
	// For demo use this function just returns PASS.
	 */
	int i = 141*BLOCKS_PER_BANK*2+17+9;

	for (bTargetDataPtr = 0; bTargetDataPtr < SECURITY_BYTES_PER_BANK; bTargetDataPtr++)
		abTargetDataOUT[bTargetDataPtr] = 0;

	for (bTargetDataPtr = 0; bTargetDataPtr < SECURITY_BYTES_PER_BANK*2; bTargetDataPtr++) {
		if ((bTargetDataPtr % 2) == 0)
			abTargetDataOUT[bTargetDataPtr/2] |= get_hex(buffer[i+bTargetDataPtr]) << 4;
		else
			abTargetDataOUT[bTargetDataPtr/2] |= get_hex(buffer[i+bTargetDataPtr]);
	}

	return PASS;
}


/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// fSDATACheck()
// Check SDATA pin for high or low logic level and return value to calling
// routine.
// Returns:
//     0 if the pin was low.
//     1 if the pin was high.
 ****************************************************************************/
unsigned char fSDATACheck(void)
{
	if (gpio_get_value(SDATA_GPIO))
		return 1;
	else
		return 0;
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ***************************************************************************
// ****                       PROCESSOR SPECIFIC                          ****
// ***************************************************************************
// ****                     USER ATTENTION REQUIRED                       ****
// ***************************************************************************
// SCLKHigh()
// Set the SCLK pin High
****************************************************************************/
void SCLKHigh(void)
{
	gpio_direction_output(SCLK_GPIO, 1);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SCLKLow()
// Make Clock pin Low
 ****************************************************************************/
void SCLKLow(void)
{
	gpio_direction_output(SCLK_GPIO, 0);
	udelay(1);
}

#ifndef RESET_MODE  /* Only needed for power cycle mode */
/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSCLKHiZ()
// Set SCLK pin to HighZ drive mode.
 ****************************************************************************/
void SetSCLKHiZ(void)
{
	gpio_direction_input(SCLK_GPIO);
	udelay(1);
}
#endif

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSCLKStrong()
// Set SCLK to an output (Strong drive mode)
 ****************************************************************************/
void SetSCLKStrong(void)
{
	gpio_direction_output(SCLK_GPIO, 1);
	udelay(1);
}


/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATAHigh()
// Make SDATA pin High
 ****************************************************************************/
void SetSDATAHigh(void)
{
	gpio_direction_output(SDATA_GPIO, 1);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATALow()
// Make SDATA pin Low
 ****************************************************************************/
void SetSDATALow(void)
{
	gpio_direction_output(SDATA_GPIO, 0);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATAHiZ()
// Set SDATA pin to an input (HighZ drive mode).
 ****************************************************************************/
void SetSDATAHiZ(void)
{
	gpio_direction_input(SDATA_GPIO);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetSDATAStrong()
// Set SDATA for transmission (Strong drive mode) -- as opposed to being set to
// High Z for receiving data.
****************************************************************************/
void SetSDATAStrong(void)
{
	gpio_direction_output(SDATA_GPIO, 1);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetXRESStrong()
// Set external reset (XRES) to an output (Strong drive mode).
 ****************************************************************************/
void SetXRESStrong(void)
{
	gpio_direction_output(XRES_GPIO, 1);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// AssertXRES()
// Set XRES pin High
 ****************************************************************************/
void AssertXRES(void)
{
	gpio_direction_output(XRES_GPIO, 1);
	udelay(1);
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// DeassertXRES()
// Set XRES pin low.
****************************************************************************/
void DeassertXRES(void)
{
	gpio_direction_output(XRES_GPIO, 0);
	udelay(1);
}


/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// SetTargetVDDStrong()
// Set VDD pin (PWR) to an output (Strong drive mode).
 ****************************************************************************/
void SetTargetVDDStrong(void)
{
	return;
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// ApplyTargetVDD()
// Provide power to the target PSoC's Vdd pin through a GPIO.
****************************************************************************/
void ApplyTargetVDD(void)
{
	return;
}

/********************* LOW-LEVEL ISSP SUBROUTINE SECTION ********************
// ****************************************************************************
// ****                        PROCESSOR SPECIFIC                          ****
// ****************************************************************************
// ****                      USER ATTENTION REQUIRED                       ****
// ****************************************************************************
// RemoveTargetVDD()
// Remove power from the target PSoC's Vdd pin.
****************************************************************************/
void RemoveTargetVDD(void)
{
	return;
}


#endif  /*(PROJECT_REV_) */
/* end of file ISSP_Drive_Routines.c */

/*
Copyright (c) 2010, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef CAMSENSOR_S5K4E1GX
#define CAMSENSOR_S5K4E1GX

#include <mach/board.h>

/* Mode select register */
#define S5K4E1GX_REG_MODE_SELECT             0x0100
#define S5K4E1GX_MODE_SELECT_STREAM          0x01    /* start streaming */
#define S5K4E1GX_MODE_SELECT_SW_STANDBY      0x00    /* software standby */

#define S5K4E1GX_REG_SOFTWARE_RESET          0x0103
#define S5K4E1GX_SOFTWARE_RESET              0x01

/* CDS timing settings */
/* Reserved registers */
#define REG_LD_START                         0x3000
#define REG_SL_START                         0x3001
#define REG_RX_START                         0x3002
#define REG_CDS_START                        0x3003
#define REG_SMP_WIDTH                        0x3004
#define REG_AZ_WIDTH                         0x3005
#define REG_S1R_WIDTH                        0x3006
#define REG_TX_START                         0x3007
#define REG_TX_WIDTH                         0x3008
#define REG_STX_WIDTH                        0x3009
#define REG_DTX_WIDTH                        0x300A
#define REG_RMP_RST_START                    0x300B
#define REG_RMP_SIG_START                    0x300C
#define REG_RMP_LAT                          0x300D

#define REG_300F                             0x300F

#define REG_SMP_EN                           0x3010
#define REG_RST_MX                           0x3011
#define REG_SIG_OFFSET1                      0x3012
#define REG_RST_OFFSET1                      0x3013
#define REG_SIG_OFFSET2                      0x3014
#define REG_RST_OFFSET2                      0x3015
#define REG_ADC_SAT                          0x3016
#define REG_RMP_INIT                         0x3017
#define REG_RMP_OPTION                       0x3018

#define REG_301B                             0x301B

#define REG_CLP_LEVEL                        0x301D
#define REG_INRUSH_CTRL                      0x3021
#define REG_PUMP_RING_OSC                    0x3022
#define REG_PIX_VOLTAGE                      0x3024
#define REG_NTG_VOLTAGE                      0x3027

#define REG_3029                             0x3029
#define REG_302B                             0x302B

/* Pixel option setting */
#define REG_PIXEL_BIAS                       0x301C
#define REG_ALL_TX_OFF                       0x30D8

/* ADLC SETTING */
#define REG_L_ADLC_BPR                       0x3070
#define REG_F_L_ADLC_MAX                     0x3071
#define REG_F_ADLC_FILTER_A                  0x3080
#define REG_F_ADLC_FILTER_B                  0x3081

#define REG_SEL_CCP                          0x30BD
#define REG_SYNC_MODE                        0x3084
#define REG_M_PCLK_DIV                       0x30BE
#define REG_PACK_VIDEO_ENABLE                0x30C1
#define REG_DPHY_ENABLE                      0x30EE
#define REG_EMBEDDED_DATA_OFF                0x3111
#define REG_OUTIF_NUM_OF_LANES               0x30E2
#define REG_DPHY_BAND_CTRL                   0x30F1

#define REG_COARSE_INTEGRATION_TIME          0x0202
#define REG_COARSE_INTEGRATION_TIME_LSB      0x0203
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_MSB    0x0204
#define REG_ANALOGUE_GAIN_CODE_GLOBAL_LSB    0x0205

/* Frame Fotmat */
#define REG_FRAME_LENGTH_LINES_MSB           0x0340
#define REG_FRAME_LENGTH_LINES_LSB           0x0341
#define REG_LINE_LENGTH_PCK_MSB              0x0342
#define REG_LINE_LENGTH_PCK_LSB              0x0343

/* PLL Registers */
#define REG_PRE_PLL_CLK_DIV                  0x0305
#define REG_PLL_MULTIPLIER_MSB               0x0306
#define REG_PLL_MULTIPLIER_LSB               0x0307
#define REG_VT_SYS_CLK_DIV                   0x30B5

/* Reserved register */
#define REG_H_BINNING                        0x30A9
#define REG_V_BINNING                        0x300E

/* Binning */
#define REG_X_EVEN_INC_MSB                   0x0380
#define REG_X_EVEN_INC_LSB                   0x0381
#define REG_X_ODD_INC_MSB                    0x0382
#define REG_X_ODD_INC_LSB                    0x0383
#define REG_Y_EVEN_INC_MSB                   0x0384
#define REG_Y_EVEN_INC_LSB                   0x0385
#define REG_Y_ODD_INC_MSB                    0x0386
#define REG_Y_ODD_INC_LSB                    0x0387

/*MIPI Size Setting for preview*/
#define REG_X_ADDR_START_MSB                 0x0344
#define REG_X_ADDR_START_LSB                 0x0345
#define REG_X_ADDR_END_MSB                   0x0348
#define REG_X_ADDR_END_LSB                   0x0349
#define REG_Y_ADDR_START_MSB                 0x0346
#define REG_Y_ADDR_START_LSB                 0x0347
#define REG_Y_ADDR_END_MSB                   0x034A
#define REG_Y_ADDR_END_LSB                   0x034B

/* Output Size */
#define REG_X_OUTPUT_SIZE_MSB                0x034C
#define REG_X_OUTPUT_SIZE_LSB                0x034D
#define REG_Y_OUTPUT_SIZE_MSB                0x034E
#define REG_Y_OUTPUT_SIZE_LSB                0x034F
#define REG_OUTIF_VIDEO_DATA_TYPE1           0x30BF
#define REG_OUTIF_OFFSET_VIDEO               0x30C0
#define REG_VIDEO_DATA_LENGTH_MSB            0x30C8
#define REG_VIDEO_DATA_LENGTH_LSB            0x30C9

#define REG_30BC                             0x30BC

struct reg_struct {
	/* CDS timing setting ... */
	uint8_t ld_start;                          /* 0x3000 */
	uint8_t sl_start;                          /* 0x3001 */
	uint8_t rx_start;                          /* 0x3002 */
	uint8_t cds_start;                         /* 0x3003 */
	uint8_t smp_width;                         /* 0x3004 */
	uint8_t az_width;                          /* 0x3005 */
	uint8_t s1r_width;                         /* 0x3006 */
	uint8_t tx_start;                          /* 0x3007 */
	uint8_t tx_width;                          /* 0x3008 */
	uint8_t stx_width;                         /* 0x3009 */
	uint8_t dtx_width;                         /* 0x300A */
	uint8_t rmp_rst_start;                     /* 0x300B */
	uint8_t rmp_sig_start;                     /* 0x300C */
	uint8_t rmp_lat;                           /* 0x300D */
	uint8_t v_binning_1;                       /* 0x300E */
	uint8_t reg_300F;                          /* 0x300F */
	uint8_t reg_301B;                          /* 0x301B */

	/* CDS option setting ... */
	uint8_t smp_en;                            /* 0x3010 */
	uint8_t rst_mx;                            /* 0x3011 */
	uint8_t reg_3029;                          /* 0x3029 */
	uint8_t sig_offset1;                       /* 0x3012 */
	uint8_t rst_offset1;                       /* 0x3013 */
	uint8_t sig_offset2;                       /* 0x3014 */
	uint8_t rst_offset2;                       /* 0x3015 */
	uint8_t adc_sat;                           /* 0x3016 */
	uint8_t rmp_init;                          /* 0x3017 */
	uint8_t rmp_option;                        /* 0x3018 */
	uint8_t clp_level;                         /* 0x301D */
	uint8_t inrush_ctrl;                       /* 0x3021 */
	uint8_t pump_ring_osc;                     /* 0x3022 */
	uint8_t pix_voltage;                       /* 0x3024 */
	uint8_t ntg_voltage;                       /* 0x3027 */
	uint8_t reg_30BC;                          /* 0x30BC */

	/* Pixel option setting ... */
	uint8_t pixel_bias;                        /* 0x301C */
	uint8_t all_tx_off;                        /* 0x30D8 */
	uint8_t reg_302B;                          /* 0x302B */

	/* ADLC setting ... */
	uint8_t l_adlc_bpr;                        /* 0x3070 */
	uint8_t f_l_adlc_max;                      /* 0x3071 */
	uint8_t f_adlc_filter_a;                   /* 0x3080 */
	uint8_t f_adlc_filter_b;                   /* 0x3081 */

	/* MIPI setting */
	uint8_t sel_ccp;                           /* 0x30BD */
	uint8_t sync_mode;                         /* 0x3084 */
	uint8_t m_pclk_div;                        /* 0x30BE */
	uint8_t pack_video_enable;                 /* 0x30C1 */
	uint8_t dphy_enable;                       /* 0x30EE */
	uint8_t embedded_data_off;                 /* 0x3111 */

	/* Integration setting ... */
	uint8_t coarse_integration_time;           /* 0x0202 */
	uint8_t coarse_integration_time_lsb;       /* 0x0203 */
	uint8_t analogue_gain_code_global_msb;     /* 0x0204 */
	uint8_t analogue_gain_code_global_lsb;     /* 0x0205 */

	/* Frame Length */
	uint8_t frame_length_lines_msb;            /* 0x0340 */
	uint8_t frame_length_lines_lsb;            /* 0x0341 */

	/* Line Length */
	uint8_t line_length_pck_msb;               /* 0x0342 */
	uint8_t line_length_pck_lsb;               /* 0x0343 */

	/* PLL setting ... */
	uint8_t pre_pll_clk_div;                   /* 0x0305 */
	uint8_t pll_multiplier_msb;                /* 0x0306 */
	uint8_t pll_multiplier_lsb;                /* 0x0307 */
	uint8_t vt_sys_clk_div;                    /* 0x30B5 */
	uint8_t outif_num_of_lanes;                /* 0x30E2 */
	uint8_t dphy_band_ctrl;                    /* 0x30F1 */

	/* MIPI Size Setting */
	uint8_t h_binning;                         /* 0x30A9 */
	uint8_t v_binning_2;                       /* 0x300E */
	uint8_t y_odd_inc_lsb_1;                   /* 0x0387 */

	/* MIPI Size Setting for preview */
	uint8_t x_addr_start_msb;                  /* 0x0344 */
	uint8_t x_addr_start_lsb;                  /* 0x0345 */
	uint8_t x_addr_end_msb;                    /* 0x0348 */
	uint8_t x_addr_end_lsb;                    /* 0x0349 */
	uint8_t y_addr_start_msb;                  /* 0x0346 */
	uint8_t y_addr_start_lsb;                  /* 0x0347 */
	uint8_t y_addr_end_msb;                    /* 0x034A */
	uint8_t y_addr_end_lsb;                    /* 0x034B */
	uint8_t x_even_inc_msb;                    /* 0x0380 */
	uint8_t x_even_inc_lsb;                    /* 0x0381 */
	uint8_t x_odd_inc_msb;                     /* 0x0382 */
	uint8_t x_odd_inc_lsb;                     /* 0x0383 */
	uint8_t y_even_inc_msb;                    /* 0x0384 */
	uint8_t y_even_inc_lsb;                    /* 0x0385 */
	uint8_t y_odd_inc_msb;                     /* 0x0386 */
	uint8_t y_odd_inc_lsb_2;                   /* 0x0387 */

	uint8_t x_output_size_msb;                 /* 0x034C */
	uint8_t x_output_size_lsb;                 /* 0x034D */
	uint8_t y_output_size_msb;                 /* 0x034E */
	uint8_t y_output_size_lsb;                 /* 0x034F */
	uint8_t outif_video_data_typel;            /* 0x30BF */
	uint8_t outif_offset_video;                /* 0x30C0 */
	uint8_t video_data_length_msb;             /* 0x30C8 */
	uint8_t video_data_length_lsb;             /* 0x30C9 */

	uint32_t size_h;
	uint32_t blk_l;
	uint32_t size_w;
	uint32_t blk_p;
};

struct reg_struct s5k4e1gx_reg_pat[2] = {
	{ /* Preview */
		// CDS timing setting ...
		0x05,    /* ld_start;                        REG=0x3000 */
		0x03,    /* sl_start;                        REG=0x3001 */
		0x08,    /* rx_start;                        REG=0x3002 */
		0x09,    /* cds_start;                       REG=0x3003 */
		0x2E,    /* smp_width;                       REG=0x3004 */
		0x06,    /* az_width;                        REG=0x3005 */
		0x34,    /* s1r_width;                       REG=0x3006 */
		0x00,    /* tx_start;                        REG=0x3007 */
		0x3C,    /* tx_width;                        REG=0x3008 */
		0x3C,    /* stx_width;                       REG=0x3009 */
		0x28,    /* dtx_width;                       REG=0x300A */
		0x04,    /* rmp_rst_start;                   REG=0x300B */
		0x0A,    /* rmp_sig_start;                   REG=0x300C */
		0x02,    /* rmp_lat;                         REG=0x300D */
		0xEB,    /* v_binning                        REG=0x300E */
		0x82,    /* reg_300F;                        REG=0x300F */
		0x83,    /* reg_301B;                        REG=0x301B */

		//// CDS option setting ...
		0x00,    /* smp_en;                          REG=0x3010 */
		0x4C,    /* rst_mx;                          REG=0x3011 */
		0xC6,    /* reg_3029;                        REG=0x3029 */
		0x30,    /* sig_offset1;                     REG=0x3012 */
		0xC0,    /* rst_offset1;                     REG=0x3013 */
		0x00,    /* sig_offset2;                     REG=0x3014 */
		0x00,    /* rst_offset2;                     REG=0x3015 */
		0x2C,    /* adc_sat;                         REG=0x3016 */
		0x94,    /* rmp_init;                        REG=0x3017 */
		0x78,    /* rmp_option;                      REG=0x3018 */
		0xD4,    /* clp_level;                       REG=0x301D */
		0x02,    /* inrush_ctrl;                     REG=0x3021 */
		0x24,    /* pump_ring_osc;                   REG=0x3022 */
		0x40,    /* pix_voltage;                     REG=0x3024 */
		0x08,    /* ntg_voltage;                     REG=0x3027 */
		0x98,    /* reg_30BC;                        REG=0x30BC */

		// Pixel option setting ...
		0x04,    /* pixel_bias;                      REG=0x301C */
		0x3F,    /* all_tx_off;                      REG=0x30D8 */
		0x01,    /* reg_302B;                        REG=0x302B */

		// ADLC setting ..
		0x5F,    /* l_adlc_bpr;                      REG=0x3070 */
		0x00,    /* f_l_adlc_max;                    REG=0x3071 */
		0x04,    /* f_adlc_filter_a;                 REG=0x3080 */
		0x38,    /* f_adlc_filter_b;                 REG=0x3081 */

		// MIPI setting
		0x00,    /* sel_cpp;                         REG=0x30BD */
		0x15,    /* sync_mode;                       REG=0x3084 */
		0x1A,    /* m_pclk_div                       REG=0x30BE */
		0x01,    /* pack_video_enable                REG=0x30C1 */
		0x02,    /* dphy_enable                      REG=0x30EE */
		0x86,    /* embedded_data_off                REG=0x3111 */

		// Integration setting ...
		0x01,    /* coarse_integration_time          REG=0x0202 */
		0xFD,    /* coarse_integration_time_lsb      REG=0x0203 */
		0x00,    /* analogue_gain_code_global_msb    REG=0x0204 */
		0x80,    /* analogue_gain_code_global_lsb    REG=0x0205 */

		//Frame Length
		0x03,    /* frame_length_lines_msb           REG=0x0340 */
		0xE0,    /* frame_length_lines_lsb           REG=0x0341 */

		// Line Length
		0x0A,    /* line_length_pck_msb              REG=0x0342 */
		0xB2,    /* line_length_pck_lsb              REG=0x0343 */

		// PLL setting ...
		0x04,    /* pre_pll_clk_div                  REG=0x0305 */
		0x00,    /* pll_multiplier_msb               REG=0x0306 */
		0x56,    /* pll_multiplier_lsb               REG=0x0307 */
		0x01,    /* vt_sys_clk_div                   REG=0x30B5 */
		0x02,    /* outif_num_of_lanes               REG=0x30E2 */
		0x90,    /* dphy_band_ctrl                   REG=0x30F1 */

		// MIPI Size Setting ...
		// 1304 x 980
		0x02,    /* h_binning                        REG=0x30A9 */
		0xEB,    /* v_binning                        REG=0x300E */
		0x03,    /* y_odd_inc_lsb                    REG=0x0387 */
		0x00,    /* x_addr_start_msb                 REG=0x0344 */
		0x00,    /* x_addr_start_lsb                 REG=0x0345 */
		0x0A,    /* x_addr_end_msb                   REG=0x0348 */
		0x2F,    /* x_addr_end_lsb                   REG=0x0349 */
		0x00,    /* y_addr_start_msb                 REG=0x0346 */
		0x00,    /* y_addr_start_lsb                 REG=0x0347 */
		0x07,    /* y_addr_end_msb                   REG=0x034A */
		0xA7,    /* y_addr_end_msb                   REG=0x034B */
		0x00,    /* x_even_inc_msb                   REG=0x0380 */
		0x01,    /* x_even_inc_lsb                   REG=0x0381 */
		0x00,    /* x_odd_inc_msb                    REG=0x0382 */
		0x01,    /* x_odd_inc_lsb                    REG=0x0383 */
		0x00,    /* y_even_inc_msb                   REG=0x0384 */
		0x01,    /* y_even_inc_lsb                   REG=0x0385 */
		0x00,    /* y_odd_inc_msb                    REG=0x0386 */
		0x03,    /* y_odd_inc_lsb                    REG=0x0387 */
		0x05,    /* x_output_size_msb                REG=0x034C */
		0x18,    /* x_output_size_lsb                REG=0x034D */
		0x03,    /* y_output_size_msb                REG=0x034E */
		0xD4,    /* y_output_size_lsb                REG=0x034F */
		0xAB,    /* outif_video_data_typel           REG=0x30BF */
		0xA0,    /* outif_offset_video               REG=0x30C0 */
		0x06,    /* video_data_length_msb            REG=0x30C8 */
		0x5E,    /* video_data_length_lsb            REG=0x30C9 */

		980,     /* y_output_size */
		12,      /* y_blank = frame_length_lines - y_output_size */
		1304,    /* x_output_size */
		1434     /* x_blank = line_length_pck - x_output_size */
	},
	{ /* Snapshot */
		// CDS timing setting ...
		0x05,    /* ld_start;                        REG=0x3000 */
		0x03,    /* sl_start;                        REG=0x3001 */
		0x08,    /* rx_start;                        REG=0x3002 */
		0x09,    /* cds_start;                       REG=0x3003 */
		0x2E,    /* smp_width;                       REG=0x3004 */
		0x06,    /* az_width;                        REG=0x3005 */
		0x34,    /* s1r_width;                       REG=0x3006 */
		0x00,    /* tx_start;                        REG=0x3007 */
		0x3C,    /* tx_width;                        REG=0x3008 */
		0x3C,    /* stx_width;                       REG=0x3009 */
		0x28,    /* dtx_width;                       REG=0x300A */
		0x04,    /* rmp_rst_start;                   REG=0x300B */
		0x0A,    /* rmp_sig_start;                   REG=0x300C */
		0x02,    /* rmp_lat;                         REG=0x300D */
		0xE8,    /* v_binning                        REG=0x300E */
		0x82,    /* reg_300F;                        REG=0x300F */
		0x75,    /* reg_301B;                        REG=0x301B */

		//// CDS option setting ...
		0x00,    /* smp_en;                          REG=0x3010 */
		0x4C,    /* rst_mx;                          REG=0x3011 */
		0xC6,    /* reg_3029;                        REG=0x3029 */
		0x30,    /* sig_offset1;                     REG=0x3012 */
		0xC0,    /* rst_offset1;                     REG=0x3013 */
		0x00,    /* sig_offset2;                     REG=0x3014 */
		0x00,    /* rst_offset2;                     REG=0x3015 */
		0x2C,    /* adc_sat;                         REG=0x3016 */
		0x94,    /* rmp_init;                        REG=0x3017 */
		0x78,    /* rmp_option;                      REG=0x3018 */
		0xD4,    /* clp_level;                       REG=0x301D */
		0x02,    /* inrush_ctrl;                     REG=0x3021 */
		0x24,    /* pump_ring_osc;                   REG=0x3022 */
		0x40,    /* pix_voltage;                     REG=0x3024 */
		0x08,    /* ntg_voltage;                     REG=0x3027 */
		0x98,    /* reg_30BC                         REG=0x30BC */

		// Pixel option setting ...
		0x04,    /* pixel_bias;                      REG=0x301C */
		0x3F,    /* all_tx_off;                      REG=0x30D8 */
		0x01,    /* reg_302B;                        REG=0x302B */

		// ADLC setting ..
		0x5F,    /* l_adlc_bpr;                      REG=0x3070 */
		0x00,    /* f_l_adlc_max;                    REG=0x3071 */
		0x04,    /* f_adlc_filter_a;                 REG=0x3080 */
		0x38,    /* f_adlc_filter_b;                 REG=0x3081 */

		// MIPI setting
		0x00,    /* sel_cpp;                         REG=0x30BD */
		0x15,    /* sync_mode;                       REG=0x3084 */
		0x1A,    /* m_pclk_div                       REG=0x30BE */
		0x01,    /* pack_video_enable                REG=0x30C1 */
		0x02,    /* dphy_enable                      REG=0x30EE */
		0x86,    /* embedded_data_off                REG=0x3111 */

		// Integration setting ...
		0x04,    /* coarse_integration_time          REG=0x0202 */
		0x12,    /* coarse_integration_time_lsb      REG=0x0203 */
		0x00,    /* analogue_gain_code_global_msb    REG=0x0204 */
		0x80,    /* analogue_gain_code_global_lsb    REG=0x0205 */

		//Frame Length
		0x07,    /* frame_length_lines_msb           REG=0x0340 */
		0xB4,    /* frame_length_lines_lsb           REG=0x0341 */

		// Line Length
		0x0A,    /* line_length_pck_msb              REG=0x0342 */
		0xB2,    /* line_length_pck_lsb              REG=0x0343 */

		// PLL setting ...
		0x04,    /* pre_pll_clk_div                  REG=0x0305 */
		0x00,    /* pll_multiplier_msb               REG=0x0306 */
		0x56,    /* pll_multiplier_lsb               REG=0x0307 */
		0x01,    /* vt_sys_clk_div                   REG=0x30B5 */
		0x02,    /* outif_num_of_lanes               REG=0x30E2 */
		0x90,    /* dphy_band_ctrl                   REG=0x30F1 */

		// MIPI Size Setting ...
		// 2608 x 1960
		0x03,    /* h_binning                        REG=0x30A9 */
		0xE8,    /* v_binning                        REG=0x300E */
		0x01,    /* y_odd_inc_lsb                    REG=0x0387 */

		//Not used for snapshot
		0x00,    /* x_addr_start_msb                 REG=0x0344 */
		0x00,    /* x_addr_start_lsb                 REG=0x0345 */
		0x0A,    /* x_addr_end_msb                   REG=0x0348 */
		0x2F,    /* x_addr_end_lsb                   REG=0x0349 */
		0x00,    /* y_addr_start_msb                 REG=0x0346 */
		0x00,    /* y_addr_start_lsb                 REG=0x0347 */
		0x07,    /* y_addr_end_msb                   REG=0x034A */
		0xA7,    /* y_addr_end_msb                   REG=0x034B */
		0x00,    /* x_even_inc_msb                   REG=0x0380 */
		0x01,    /* x_even_inc_lsb                   REG=0x0381 */
		0x00,    /* x_odd_inc_msb                    REG=0x0382 */
		0x01,    /* x_odd_inc_lsb                    REG=0x0383 */
		0x00,    /* y_even_inc_msb                   REG=0x0384 */
		0x01,    /* y_even_inc_lsb                   REG=0x0385 */
		0x00,    /* y_odd_inc_msb                    REG=0x0386 */
		//Not used for snapshot

		0x01,    /* y_odd_inc_lsb                    REG=0x0387 */
		0x0A,    /* x_output_size_msb                REG=0x034C */
		0x30,    /* x_output_size_lsb                REG=0x034D */
		0x07,    /* y_output_size_msb                REG=0x034E */
		0xA8,    /* y_output_size_lsb                REG=0x034F */

		0xAB,    /* outif_video_data_typel           REG=0x30BF */
		0x80,    /* outif_offset_video               REG=0x30C0 */
		0x0C,    /* video_data_length_msb            REG=0x30C8 */
		0xBC,    /* video_data_length_lsb            REG=0x30C9 */

		1960,    /* y_output_size */
		12,      /* y_blank = frame_length_lines - y_output_size */
		2608,    /* x_output_size */
		130      /* x_blank = line_length_pck - x_output_size */
	}
};

struct s5k4e1gx_i2c_reg_conf {
	unsigned short waddr;
	unsigned char  bdata;
};

#if defined(A4_APPLY_SENSOR_SHADING)
struct s5k4e1gx_i2c_reg_conf shading_setting[] = {
	{0x3096, 0x40},

	{0x3097, 0x52},    /* sh4ch_blk_width = 82 */
	{0x3098, 0x7b},    /* sh4ch_blk_height = 123 */
	{0x3099, 0x03},    /* sh4ch_step_x msb (sh4ch_step_x = 799) */
	{0x309a, 0x1f},    /* sh4ch_step_x lsb */
	{0x309b, 0x02},    /* sh4ch_step_y msb (sh4ch_step_y = 533) */
	{0x309c, 0x15},    /* sh4ch_step_y lsb */
	{0x309d, 0x00},    /* sh4ch_start_blk_cnt_x = 0 */
	{0x309e, 0x00},    /* sh4ch_start_int_cnt_x = 0 */
	{0x309f, 0x00},    /* sh4ch_start_frac_cnt_x msb
					(sh4ch_start_frac_cnt_x = 0) */
	{0x30a0, 0x00},    /* sh4ch_start_frac_cnt_x lsb */
	{0x30a1, 0x00},    /* sh4ch_start_blk_cnt_y = 0 */
	{0x30a2, 0x00},    /* sh4ch_start_int_cnt_y = 0 */
	{0x30a3, 0x00},    /* sh4ch_start_frac_cnt_y msb
					(sh4ch_start_frac_cnt_x = 0) */
	{0x30a4, 0x00},    /* sh4ch_start_frac_cnt_y lsb */

	{0x30a5, 0x01},
	{0x30a6, 0x00},    /* gs_pedestal	= 64 */

	{0x3200, 0x00},
	{0x3201, 0xac},
	{0x3202, 0x69},
	{0x3203, 0x0f},
	{0x3204, 0xb6},
	{0x3205, 0xab},
	{0x3206, 0x00},
	{0x3207, 0x41},
	{0x3208, 0x27},
	{0x3209, 0x0f},
	{0x320a, 0xe1},
	{0x320b, 0x16},
	{0x320c, 0x0f},
	{0x320d, 0xf4},
	{0x320e, 0x95},
	{0x320f, 0x00},
	{0x3210, 0x2d},
	{0x3211, 0xd4},
	{0x3212, 0x0f},
	{0x3213, 0x9c},
	{0x3214, 0x18},
	{0x3215, 0x00},
	{0x3216, 0x56},
	{0x3217, 0xe7},
	{0x3218, 0x0f},
	{0x3219, 0xae},
	{0x321a, 0xeb},
	{0x321b, 0x00},
	{0x321c, 0x27},
	{0x321d, 0xe8},
	{0x321e, 0x00},
	{0x321f, 0x18},
	{0x3220, 0x39},
	{0x3221, 0x0f},
	{0x3222, 0xb3},
	{0x3223, 0xe2},
	{0x3224, 0x00},
	{0x3225, 0x67},
	{0x3226, 0x0e},
	{0x3227, 0x0f},
	{0x3228, 0x7a},
	{0x3229, 0x0a},
	{0x322a, 0x00},
	{0x322b, 0x68},
	{0x322c, 0x62},
	{0x322d, 0x0f},
	{0x322e, 0xd8},
	{0x322f, 0x51},
	{0x3230, 0x0f},
	{0x3231, 0xee},
	{0x3232, 0x42},
	{0x3233, 0x00},
	{0x3234, 0x50},
	{0x3235, 0xbe},
	{0x3236, 0x0f},
	{0x3237, 0xba},
	{0x3238, 0x09},
	{0x3239, 0x00},
	{0x323a, 0x83},
	{0x323b, 0x90},
	{0x323c, 0x0f},
	{0x323d, 0xb4},
	{0x323e, 0x29},
	{0x323f, 0x00},
	{0x3240, 0x05},
	{0x3241, 0x18},
	{0x3242, 0x00},
	{0x3243, 0x10},
	{0x3244, 0x87},
	{0x3245, 0x0f},
	{0x3246, 0xbe},
	{0x3247, 0x03},
	{0x3248, 0x00},
	{0x3249, 0x04},
	{0x324a, 0x8b},
	{0x324b, 0x0f},
	{0x324c, 0xd6},
	{0x324d, 0xbe},
	{0x324e, 0x0f},
	{0x324f, 0xf8},
	{0x3250, 0xdf},
	{0x3251, 0x00},
	{0x3252, 0x1f},
	{0x3253, 0x85},
	{0x3254, 0x00},
	{0x3255, 0x08},
	{0x3256, 0x68},
	{0x3257, 0x00},
	{0x3258, 0x07},
	{0x3259, 0x7f},
	{0x325a, 0x00},
	{0x325b, 0x2e},
	{0x325c, 0x83},
	{0x325d, 0x0f},
	{0x325e, 0xd0},
	{0x325f, 0x1f},
	{0x3260, 0x00},
	{0x3261, 0x41},
	{0x3262, 0x04},
	{0x3263, 0x0f},
	{0x3264, 0xda},
	{0x3265, 0x31},
	{0x3266, 0x0f},
	{0x3267, 0xdd},
	{0x3268, 0xbf},
	{0x3269, 0x00},
	{0x326a, 0x32},
	{0x326b, 0xb2},
	{0x326c, 0x00},
	{0x326d, 0xb6},
	{0x326e, 0x71},
	{0x326f, 0x0f},
	{0x3270, 0xb5},
	{0x3271, 0x8c},
	{0x3272, 0x00},
	{0x3273, 0x3f},
	{0x3274, 0x28},
	{0x3275, 0x0f},
	{0x3276, 0xe4},
	{0x3277, 0xce},
	{0x3278, 0x0f},
	{0x3279, 0xf5},
	{0x327a, 0x4b},
	{0x327b, 0x00},
	{0x327c, 0x2b},
	{0x327d, 0xa7},
	{0x327e, 0x0f},
	{0x327f, 0x9a},
	{0x3280, 0x7f},
	{0x3281, 0x00},
	{0x3282, 0x57},
	{0x3283, 0xbd},
	{0x3284, 0x0f},
	{0x3285, 0xad},
	{0x3286, 0x8a},
	{0x3287, 0x00},
	{0x3288, 0x25},
	{0x3289, 0xec},
	{0x328a, 0x00},
	{0x328b, 0x1d},
	{0x328c, 0xb5},
	{0x328d, 0x0f},
	{0x328e, 0xac},
	{0x328f, 0xf8},
	{0x3290, 0x00},
	{0x3291, 0x63},
	{0x3292, 0x86},
	{0x3293, 0x0f},
	{0x3294, 0x79},
	{0x3295, 0xd7},
	{0x3296, 0x00},
	{0x3297, 0x64},
	{0x3298, 0xce},
	{0x3299, 0x0f},
	{0x329a, 0xe2},
	{0x329b, 0x9c},
	{0x329c, 0x0f},
	{0x329d, 0xe6},
	{0x329e, 0xcc},
	{0x329f, 0x00},
	{0x32a0, 0x55},
	{0x32a1, 0xe2},
	{0x32a2, 0x0f},
	{0x32a3, 0xc3},
	{0x32a4, 0xae},
	{0x32a5, 0x00},
	{0x32a6, 0x7a},
	{0x32a7, 0x35},
	{0x32a8, 0x0f},
	{0x32a9, 0xb8},
	{0x32aa, 0x29},
	{0x32ab, 0x00},
	{0x32ac, 0x06},
	{0x32ad, 0x1d},
	{0x32ae, 0x00},
	{0x32af, 0x03},
	{0x32b0, 0xca},
	{0x32b1, 0x0f},
	{0x32b2, 0xc5},
	{0x32b3, 0xf7},
	{0x32b4, 0x0f},
	{0x32b5, 0xf9},
	{0x32b6, 0xc5},
	{0x32b7, 0x0f},
	{0x32b8, 0xf1},
	{0x32b9, 0xa2},
	{0x32ba, 0x0f},
	{0x32bb, 0xf5},
	{0x32bc, 0x22},
	{0x32bd, 0x00},
	{0x32be, 0x0d},
	{0x32bf, 0x89},
	{0x32c0, 0x00},
	{0x32c1, 0x23},
	{0x32c2, 0xc3},
	{0x32c3, 0x0f},
	{0x32c4, 0xfb},
	{0x32c5, 0x6c},
	{0x32c6, 0x00},
	{0x32c7, 0x36},
	{0x32c8, 0x21},
	{0x32c9, 0x0f},
	{0x32ca, 0xb4},
	{0x32cb, 0x7d},
	{0x32cc, 0x00},
	{0x32cd, 0x47},
	{0x32ce, 0x86},
	{0x32cf, 0x0f},
	{0x32d0, 0xe5},
	{0x32d1, 0x6d},
	{0x32d2, 0x0f},
	{0x32d3, 0xd2},
	{0x32d4, 0xd2},
	{0x32d5, 0x00},
	{0x32d6, 0x33},
	{0x32d7, 0x76},
	{0x32d8, 0x00},
	{0x32d9, 0x94},
	{0x32da, 0xc0},
	{0x32db, 0x0f},
	{0x32dc, 0xbe},
	{0x32dd, 0x12},
	{0x32de, 0x00},
	{0x32df, 0x3f},
	{0x32e0, 0xc7},
	{0x32e1, 0x0f},
	{0x32e2, 0xdb},
	{0x32e3, 0x0d},
	{0x32e4, 0x00},
	{0x32e5, 0x00},
	{0x32e6, 0xf3},
	{0x32e7, 0x00},
	{0x32e8, 0x21},
	{0x32e9, 0x6b},
	{0x32ea, 0x0f},
	{0x32eb, 0xad},
	{0x32ec, 0xc0},
	{0x32ed, 0x00},
	{0x32ee, 0x4f},
	{0x32ef, 0x72},
	{0x32f0, 0x0f},
	{0x32f1, 0xb4},
	{0x32f2, 0x53},
	{0x32f3, 0x00},
	{0x32f4, 0x22},
	{0x32f5, 0xfc},
	{0x32f6, 0x00},
	{0x32f7, 0x1f},
	{0x32f8, 0x96},
	{0x32f9, 0x0f},
	{0x32fa, 0xb0},
	{0x32fb, 0xe0},
	{0x32fc, 0x00},
	{0x32fd, 0x56},
	{0x32fe, 0xb0},
	{0x32ff, 0x0f},
	{0x3300, 0x8b},
	{0x3301, 0x34},
	{0x3302, 0x00},
	{0x3303, 0x5b},
	{0x3304, 0x10},
	{0x3305, 0x0f},
	{0x3306, 0xe3},
	{0x3307, 0xcc},
	{0x3308, 0x0f},
	{0x3309, 0xdb},
	{0x330a, 0xca},
	{0x330b, 0x00},
	{0x330c, 0x5c},
	{0x330d, 0x65},
	{0x330e, 0x0f},
	{0x330f, 0xcb},
	{0x3310, 0x3c},
	{0x3311, 0x00},
	{0x3312, 0x6e},
	{0x3313, 0x28},
	{0x3314, 0x0f},
	{0x3315, 0xbd},
	{0x3316, 0x3e},
	{0x3317, 0x00},
	{0x3318, 0x0c},
	{0x3319, 0xf9},
	{0x331a, 0x00},
	{0x331b, 0x03},
	{0x331c, 0xd5},
	{0x331d, 0x0f},
	{0x331e, 0xc8},
	{0x331f, 0x0c},
	{0x3320, 0x0f},
	{0x3321, 0xf3},
	{0x3322, 0xc9},
	{0x3323, 0x0f},
	{0x3324, 0xe3},
	{0x3325, 0xf9},
	{0x3326, 0x0f},
	{0x3327, 0xf4},
	{0x3328, 0xe1},
	{0x3329, 0x00},
	{0x332a, 0x0f},
	{0x332b, 0x17},
	{0x332c, 0x00},
	{0x332d, 0x2b},
	{0x332e, 0x33},
	{0x332f, 0x0f},
	{0x3330, 0xeb},
	{0x3331, 0xeb},
	{0x3332, 0x00},
	{0x3333, 0x36},
	{0x3334, 0x4c},
	{0x3335, 0x0f},
	{0x3336, 0xcd},
	{0x3337, 0xeb},
	{0x3338, 0x00},
	{0x3339, 0x46},
	{0x333a, 0x9a},
	{0x333b, 0x0f},
	{0x333c, 0xd6},
	{0x333d, 0xad},
	{0x333e, 0x0f},
	{0x333f, 0xd7},
	{0x3340, 0xa0},
	{0x3341, 0x00},
	{0x3342, 0x39},
	{0x3343, 0x7d},
	{0x3344, 0x00},
	{0x3345, 0xab},
	{0x3346, 0x4f},
	{0x3347, 0x0f},
	{0x3348, 0xb3},
	{0x3349, 0xdb},
	{0x334a, 0x00},
	{0x334b, 0x4c},
	{0x334c, 0xf3},
	{0x334d, 0x0f},
	{0x334e, 0xc8},
	{0x334f, 0xb1},
	{0x3350, 0x00},
	{0x3351, 0x1a},
	{0x3352, 0x63},
	{0x3353, 0x00},
	{0x3354, 0x10},
	{0x3355, 0xe7},
	{0x3356, 0x0f},
	{0x3357, 0x9e},
	{0x3358, 0x85},
	{0x3359, 0x00},
	{0x335a, 0x4e},
	{0x335b, 0xf7},
	{0x335c, 0x0f},
	{0x335d, 0xbd},
	{0x335e, 0x09},
	{0x335f, 0x00},
	{0x3360, 0x25},
	{0x3361, 0xa9},
	{0x3362, 0x0f},
	{0x3363, 0xf9},
	{0x3364, 0xc7},
	{0x3365, 0x0f},
	{0x3366, 0xda},
	{0x3367, 0x90},
	{0x3368, 0x00},
	{0x3369, 0x5c},
	{0x336a, 0xfb},
	{0x336b, 0x0f},
	{0x336c, 0x94},
	{0x336d, 0xfc},
	{0x336e, 0x00},
	{0x336f, 0x34},
	{0x3370, 0xe2},
	{0x3371, 0x00},
	{0x3372, 0x01},
	{0x3373, 0x9d},
	{0x3374, 0x0f},
	{0x3375, 0xfb},
	{0x3376, 0x87},
	{0x3377, 0x00},
	{0x3378, 0x24},
	{0x3379, 0xf5},
	{0x337a, 0x0f},
	{0x337b, 0xcb},
	{0x337c, 0xc8},
	{0x337d, 0x00},
	{0x337e, 0x61},
	{0x337f, 0x8e},
	{0x3380, 0x0f},
	{0x3381, 0xea},
	{0x3382, 0x5b},
	{0x3383, 0x0f},
	{0x3384, 0xdc},
	{0x3385, 0x45},
	{0x3386, 0x00},
	{0x3387, 0x07},
	{0x3388, 0xf6},
	{0x3389, 0x0f},
	{0x338a, 0xe2},
	{0x338b, 0x47},
	{0x338c, 0x0f},
	{0x338d, 0xf7},
	{0x338e, 0xe6},
	{0x338f, 0x0f},
	{0x3390, 0xe4},
	{0x3391, 0xa6},
	{0x3392, 0x0f},
	{0x3393, 0xf5},
	{0x3394, 0x43},
	{0x3395, 0x00},
	{0x3396, 0x14},
	{0x3397, 0x8c},
	{0x3398, 0x00},
	{0x3399, 0x12},
	{0x339a, 0x60},
	{0x339b, 0x00},
	{0x339c, 0x05},
	{0x339d, 0xd6},
	{0x339e, 0x00},
	{0x339f, 0x30},
	{0x33a0, 0x7a},
	{0x33a1, 0x0f},
	{0x33a2, 0xd6},
	{0x33a3, 0xe7},
	{0x33a4, 0x00},
	{0x33a5, 0x21},
	{0x33a6, 0x60},
	{0x33a7, 0x00},
	{0x33a8, 0x01},
	{0x33a9, 0x39},
	{0x33aa, 0x0f},
	{0x33ab, 0xd9},
	{0x33ac, 0xd7},
	{0x33ad, 0x00},
	{0x33ae, 0x1a},
	{0x33af, 0xd2},

	{0x3096, 0x60},
	{0x3096, 0x40}
};

struct s5k4e1gx_i2c_reg_conf full_size_shading[] = {
	{0x3097, 0x52},
	{0x3098, 0x7b},
	{0x3099, 0x03},
	{0x309A, 0x1F},
	{0x309B, 0x02},
	{0x309C, 0x15}
};

struct s5k4e1gx_i2c_reg_conf sub_sampling_shading[] = {
	{0x3097, 0x52},
	{0x3098, 0x3E},
	{0x3099, 0x03},
	{0x309A, 0x1F},
	{0x309B, 0x04},
	{0x309C, 0x21}
};
#endif

#endif /* CAMSENSOR_S5K4E1GX */

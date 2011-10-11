/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _MACH_QDSP5_V2_AUDIO_ACDB_DEF_H
#define _MACH_QDSP5_V2_AUDIO_ACDB_DEF_H

/* Define ACDB device ID */
#define ACDB_ID_HANDSET_SPKR				1
#define ACDB_ID_HANDSET_MIC				2
#define ACDB_ID_HEADSET_MIC				3
#define ACDB_ID_HEADSET_SPKR_MONO			4
#define ACDB_ID_HEADSET_SPKR_STEREO			5
#define ACDB_ID_SPKR_PHONE_MIC				6
#define ACDB_ID_SPKR_PHONE_MONO				7
#define ACDB_ID_SPKR_PHONE_STEREO			8
#define ACDB_ID_BT_SCO_MIC				9
#define ACDB_ID_BT_SCO_SPKR				0x0A
#define ACDB_ID_BT_A2DP_SPKR				0x0B
#define ACDB_ID_BT_A2DP_TX				0x10
#define ACDB_ID_TTY_HEADSET_MIC				0x0C
#define ACDB_ID_TTY_HEADSET_SPKR			0x0D
#define ACDB_ID_HEADSET_MONO_PLUS_SPKR_MONO_RX		0x11
#define ACDB_ID_HEADSET_STEREO_PLUS_SPKR_STEREO_RX	0x14
#define ACDB_ID_FM_TX_LOOPBACK				0x17
#define ACDB_ID_FM_TX					0x18
#define ACDB_ID_LP_FM_SPKR_PHONE_STEREO_RX		0x19
#define ACDB_ID_LP_FM_HEADSET_SPKR_STEREO_RX		0x1A
#define ACDB_ID_I2S_RX					0x20
#define ACDB_ID_SPKR_PHONE_MIC_BROADSIDE		0x2B
#define ACDB_ID_HANDSET_MIC_BROADSIDE			0x2C
#define ACDB_ID_SPKR_PHONE_MIC_ENDFIRE			0x2D
#define ACDB_ID_HANDSET_MIC_ENDFIRE			0x2E
#define ACDB_ID_I2S_TX					0x30
#define ACDB_ID_HDMI					0x40
#if defined(CONFIG_MACH_ACER_A4) || defined(CONFIG_MACH_ACER_A5)
#define ACDB_ID_DOLBY_RX							0x12
#define ACDB_ID_HAC_TX								0x24
#define ACDB_ID_HAC_RX								0x25
#define ACDB_ID_BT_SCO_BT530_MIC					0x31
#define ACDB_ID_BT_SCO_BT530_SPKR					0x32
#define ACDB_ID_BT_SCO_BT2050_MIC					0x33
#define ACDB_ID_BT_SCO_BT2050_SPKR				0x34
#define ACDB_ID_BT_SCO_BT2070_MIC					0x35
#define ACDB_ID_BT_SCO_BT2070_SPKR				0x36
#define ACDB_ID_BT_SCO_BT103_MIC					0x37
#define ACDB_ID_BT_SCO_BT103_SPKR					0x38
#define ACDB_ID_BT_SCO_DS220_DS980_MIC			0x39
#define ACDB_ID_BT_SCO_DS220_DS980_SPKR			0x3A
#endif

/* ID used for virtual devices */
#define PSEUDO_ACDB_ID 					0xFFFF

#endif /* _MACH_QDSP5_V2_AUDIO_ACDB_DEF_H */

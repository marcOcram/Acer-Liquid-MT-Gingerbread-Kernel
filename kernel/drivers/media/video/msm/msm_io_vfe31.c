/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pm_qos_params.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <mach/clk.h>
#include <linux/gpio.h>
#if defined(CONFIG_MACH_ACER_A4)
#ifdef CONFIG_S5K4E1GX
#include "../../../arch/arm/mach-msm/smd_private.h"
#endif
#endif

#define CAMIF_CFG_RMSK             0x1fffff
#define CAM_SEL_BMSK               0x2
#define CAM_PCLK_SRC_SEL_BMSK      0x60000
#define CAM_PCLK_INVERT_BMSK       0x80000
#define CAM_PAD_REG_SW_RESET_BMSK  0x100000

#define EXT_CAM_HSYNC_POL_SEL_BMSK 0x10000
#define EXT_CAM_VSYNC_POL_SEL_BMSK 0x8000
#define MDDI_CLK_CHICKEN_BIT_BMSK  0x80

#define CAM_SEL_SHFT               0x1
#define CAM_PCLK_SRC_SEL_SHFT      0x11
#define CAM_PCLK_INVERT_SHFT       0x13
#define CAM_PAD_REG_SW_RESET_SHFT  0x14

#define EXT_CAM_HSYNC_POL_SEL_SHFT 0x10
#define EXT_CAM_VSYNC_POL_SEL_SHFT 0xF
#define MDDI_CLK_CHICKEN_BIT_SHFT  0x7

/* MIPI	CSI	controller registers */
#define	MIPI_PHY_CONTROL			0x00000000
#define	MIPI_PROTOCOL_CONTROL		0x00000004
#define	MIPI_INTERRUPT_STATUS		0x00000008
#define	MIPI_INTERRUPT_MASK			0x0000000C
#define	MIPI_CAMERA_CNTL			0x00000024
#define	MIPI_CALIBRATION_CONTROL	0x00000018
#define	MIPI_PHY_D0_CONTROL2		0x00000038
#define	MIPI_PHY_D1_CONTROL2		0x0000003C
#define	MIPI_PHY_D2_CONTROL2		0x00000040
#define	MIPI_PHY_D3_CONTROL2		0x00000044
#define	MIPI_PHY_CL_CONTROL			0x00000048
#define	MIPI_PHY_D0_CONTROL			0x00000034
#define	MIPI_PHY_D1_CONTROL			0x00000020
#define	MIPI_PHY_D2_CONTROL			0x0000002C
#define	MIPI_PHY_D3_CONTROL			0x00000030
#define	MIPI_PROTOCOL_CONTROL_SW_RST_BMSK			0x8000000
#define	MIPI_PROTOCOL_CONTROL_LONG_PACKET_HEADER_CAPTURE_BMSK	0x200000
#define	MIPI_PROTOCOL_CONTROL_DATA_FORMAT_BMSK			0x180000
#define	MIPI_PROTOCOL_CONTROL_DECODE_ID_BMSK			0x40000
#define	MIPI_PROTOCOL_CONTROL_ECC_EN_BMSK			0x20000
#define	MIPI_CALIBRATION_CONTROL_SWCAL_CAL_EN_SHFT		0x16
#define	MIPI_CALIBRATION_CONTROL_SWCAL_STRENGTH_OVERRIDE_EN_SHFT	0x15
#define	MIPI_CALIBRATION_CONTROL_CAL_SW_HW_MODE_SHFT		0x14
#define	MIPI_CALIBRATION_CONTROL_MANUAL_OVERRIDE_EN_SHFT	0x7
#define	MIPI_PROTOCOL_CONTROL_DATA_FORMAT_SHFT			0x13
#define	MIPI_PROTOCOL_CONTROL_DPCM_SCHEME_SHFT			0x1e
#define	MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_D1_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D1_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D1_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D1_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_D2_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D2_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D2_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D2_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_D3_CONTROL2_SETTLE_COUNT_SHFT			0x18
#define	MIPI_PHY_D3_CONTROL2_HS_TERM_IMP_SHFT			0x10
#define	MIPI_PHY_D3_CONTROL2_LP_REC_EN_SHFT				0x4
#define	MIPI_PHY_D3_CONTROL2_ERR_SOT_HS_EN_SHFT			0x3
#define	MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT			0x18
#define	MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT				0x2
#define	MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT				0x1c
#define	MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT		0x9
#define	MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT	0x8

#define	CAMIO_VFE_CLK_SNAP			122880000
#define	CAMIO_VFE_CLK_PREV			122880000

#ifdef CONFIG_MSM_NPA_SYSTEM_BUS
/* NPA Flow IDs */
#define MSM_AXI_QOS_PREVIEW     MSM_AXI_FLOW_CAMERA_PREVIEW_HIGH
#define MSM_AXI_QOS_SNAPSHOT    MSM_AXI_FLOW_CAMERA_SNAPSHOT_12MP
#define MSM_AXI_QOS_RECORDING   MSM_AXI_FLOW_CAMERA_RECORDING_720P
#else
/* AXI rates in KHz */
#define MSM_AXI_QOS_PREVIEW     192000
#define MSM_AXI_QOS_SNAPSHOT    192000
#define MSM_AXI_QOS_RECORDING   192000
#endif

static struct clk *camio_vfe_mdc_clk;
static struct clk *camio_mdc_clk;
static struct clk *camio_vfe_clk;
static struct clk *camio_vfe_camif_clk;
static struct clk *camio_vfe_pbdg_clk;
static struct clk *camio_cam_m_clk;
static struct clk *camio_camif_pad_pbdg_clk;
static struct clk *camio_csi_clk;
static struct clk *camio_csi_pclk;
static struct clk *camio_csi_vfe_clk;
static struct clk *camio_jpeg_clk;
static struct clk *camio_jpeg_pclk;
static struct clk *camio_vpe_clk;
#if defined(CONFIG_MACH_ACER_A5)
#ifdef CONFIG_MT9D115
static struct vreg *vreg_wlan2;
#endif
#ifdef CONFIG_MT9E013
static struct vreg *vreg_gp4;
static struct vreg *vreg_gp13;
static struct vreg *vreg_gp9;
#endif
#elif defined(CONFIG_MACH_ACER_A4)
#ifdef CONFIG_S5K4E1GX
static int acer_hw_version = -1;
static struct vreg *vreg_gp2 = NULL;
static struct vreg *vreg_gp10 = NULL;
static struct vreg *vreg_vddio_2p8 = NULL;
#endif
#else
static struct vreg *vreg_gp2;
static struct vreg *vreg_lvsw1;
#endif
static struct msm_camera_io_ext camio_ext;
static struct msm_camera_io_clk camio_clk;
static struct resource *camifpadio, *csiio;
void __iomem *camifpadbase, *csibase;

void msm_io_w(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	writel((data), (addr));
}

void msm_io_w_mb(u32 data, void __iomem *addr)
{
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	wmb();
	writel((data), (addr));
	wmb();
}

u32 msm_io_r(void __iomem *addr)
{
	uint32_t data = readl(addr);
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

u32 msm_io_r_mb(void __iomem *addr)
{
	uint32_t data;
	rmb();
	data = readl(addr);
	rmb();
	CDBG("%s: %08x %08x\n", __func__, (int) (addr), (data));
	return data;
}

void msm_io_memcpy_toio(void __iomem *dest_addr,
	void __iomem *src_addr, u32 len)
{
	int i;
	u32 *d = (u32 *) dest_addr;
	u32 *s = (u32 *) src_addr;
	/* memcpy_toio does not work. Use writel for now */
	for (i = 0; i < len; i++)
		writel(*s++, d++);
}

void msm_io_dump(void __iomem *addr, int size)
{
	char line_str[128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	CDBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			sprintf(p_str, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl(p++);
		sprintf(p_str, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			CDBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		CDBG("%s\n", line_str);
}

void msm_io_memcpy(void __iomem *dest_addr, void __iomem *src_addr, u32 len)
{
	CDBG("%s: %p %p %d\n", __func__, dest_addr, src_addr, len);
	msm_io_memcpy_toio(dest_addr, src_addr, len / 4);
	msm_io_dump(dest_addr, len);
}

#if defined(CONFIG_MACH_ACER_A5)
#ifdef CONFIG_MT9E013
static void msm_camera_mt9e013_vreg_enable(const struct msm_camera_sensor_info *data)
{
	/* DVDD: 1.8V (L20) */
	vreg_gp13 = vreg_get(NULL, "gp13");
	if (IS_ERR(vreg_gp13)) {
		pr_err("%s: VREG GP13 get failed %ld\n", __func__,
			PTR_ERR(vreg_gp13));
		vreg_gp13 = NULL;
		return;
	}
	if (vreg_set_level(vreg_gp13, 1800)) {
		pr_err("%s: VREG GP13 set failed\n", __func__);
		goto gp13_put;
	}
	if (vreg_enable(vreg_gp13)) {
		pr_err("%s: VREG GP13 enable failed\n", __func__);
		goto gp13_put;
	}

	/* AVDD: 2.6V (L10) */
	vreg_gp4 = vreg_get(NULL, "gp4");
	if (IS_ERR(vreg_gp4)) {
		pr_err("%s: VREG GP4 get failed %ld\n", __func__,
			PTR_ERR(vreg_gp4));
		vreg_gp4 = NULL;
		goto gp13_disable;
	}
	if (vreg_set_level(vreg_gp4, 2600)) {
		pr_err("%s: VREG GP4 set failed\n", __func__);
		goto gp4_put;
	}
	if (vreg_enable(vreg_gp4)) {
		pr_err("%s: VREG GP4 enable failed\n", __func__);
		goto gp4_put;
	}

	/* VCM: 2.6V (L12)*/
	vreg_gp9 = vreg_get(NULL, "gp9");
	if (IS_ERR(vreg_gp9)) {
		pr_err("%s: VREG GP9 get failed %ld\n", __func__,
			PTR_ERR(vreg_gp9));
		vreg_gp9 = NULL;
		goto gp4_disable;
	}
	if (vreg_set_level(vreg_gp9, 2600)) {
		pr_err("%s: VREG GP9 set failed\n", __func__);
		goto gp9_put;
	}
	if (vreg_enable(vreg_gp9)) {
		pr_err("%s: VREG GP9 enable failed\n", __func__);
		goto gp9_put;
	}
	return;

gp9_put:
	vreg_put(vreg_gp9);
gp4_disable:
	vreg_disable(vreg_gp4);
gp4_put:
	vreg_put(vreg_gp4);
gp13_disable:
	vreg_disable(vreg_gp13);
gp13_put:
	vreg_put(vreg_gp13);
}
#endif
#ifdef CONFIG_MT9D115
static void msm_camera_mt9d115_vreg_enable(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	// Pull down standby pin
	rc = gpio_request(data->sensor_pwd, "mt9d115");
	if (!rc) {
		gpio_direction_output(data->sensor_pwd, 0);
		gpio_free(data->sensor_pwd);
	} else {
		pr_err("%s: request power down pin failed\n", __func__);
	}

	// Pull up reset pin
	rc = gpio_request(data->sensor_reset, "mt9d115");
	if (!rc) {
		gpio_direction_output(data->sensor_reset, 1);
		gpio_free(data->sensor_reset);
	} else {
		pr_err("%s: request reset pin failed\n", __func__);
	}

	// Enable VDD 1.8V
	vreg_wlan2 = vreg_get(NULL, "wlan2");
	if (IS_ERR(vreg_wlan2)) {
		pr_err("%s: VREG WLAN2 get failed %ld\n", __func__,
			PTR_ERR(vreg_wlan2));
		return;
	}
	if (vreg_set_level(vreg_wlan2, 1800)) {
		pr_err("%s: VREG WLAN2 set failed\n", __func__);
		goto wlan2_put;
	}
	if (vreg_enable(vreg_wlan2)) {
		pr_err("%s: VREG WLAN2 enable failed\n", __func__);
		goto wlan2_put;
	}

	// Per HW's request, add 5ms delay before MCLK being enabled
	mdelay(5);
	return;

wlan2_put:
	vreg_put(vreg_wlan2);
}
#endif
#endif

static void msm_camera_vreg_enable(const struct platform_device *pdev)
{
#if defined(CONFIG_MACH_ACER_A5)
#ifdef CONFIG_MT9E013
	if (!strcmp(pdev->name, "msm_camera_mt9e013")) {
		msm_camera_mt9e013_vreg_enable(pdev->dev.platform_data);
	}
#endif
#ifdef CONFIG_MT9D115
	if (!strcmp(pdev->name, "msm_camera_mt9d115")) {
		msm_camera_mt9d115_vreg_enable(pdev->dev.platform_data);;
	}
#endif
#elif defined(CONFIG_MACH_ACER_A4)
#ifdef CONFIG_S5K4E1GX
	int rc = 0;

	/* Open VDIG 1.8V */
	vreg_gp10 = vreg_get(NULL, "gp10");
	if (IS_ERR(vreg_gp10)) {
		CDBG("%s: vreg_get(%s) failed (%ld)\n",
			__func__, "gp10", PTR_ERR(vreg_gp10));
		vreg_gp10 = NULL;
		return;
	}

	rc = vreg_set_level(vreg_gp10, 1800);
	if (rc) {
		CDBG("%s: vreg_set level failed (%d)\n",
			__func__, rc);
		goto gp10_put;
	}

	rc = vreg_enable(vreg_gp10);
	if (rc) {
		CDBG("%s: vreg_enable() = %d \n",
			__func__, rc);
		goto gp10_put;
	}

	mdelay(1);

	/* Open VADD 2.85V */
	vreg_gp2 = vreg_get(NULL, "gp2");
	if (IS_ERR(vreg_gp2)) {
		CDBG("%s: vreg_get(%s) failed (%ld)\n",
			__func__, "gp2", PTR_ERR(vreg_gp2));
		goto gp10_disable;
	}

	rc = vreg_set_level(vreg_gp2, 2850);
	if (rc) {
		CDBG("%s: vreg_set level failed (%d)\n",
			__func__, rc);
		goto gp2_put;
	}

	rc = vreg_enable(vreg_gp2);
	if (rc) {
		CDBG("%s: vreg_enable() = %d \n",
			__func__, rc);
		goto gp2_put;
	}

	mdelay(1);

	/* Open VDDIO 2.8V */
	if (acer_hw_version != -1) {
		if (acer_hw_version <= ACER_HW_VERSION_DVT1)
			vreg_vddio_2p8 = vreg_get(NULL, "gp7");
		else
			vreg_vddio_2p8 = vreg_get(NULL, "gp9");
		if (IS_ERR(vreg_vddio_2p8)) {
			CDBG("%s: vreg_get(%s) failed (%ld)\n",
				__func__, "gp9", PTR_ERR(vreg_vddio_2p8));
			vreg_vddio_2p8 = NULL;
			goto gp2_disable;
		}

		rc = vreg_set_level(vreg_vddio_2p8, 2800);
		if (rc) {
			CDBG("%s: vreg_set level failed (%d)\n",
				__func__, rc);
			goto vddio_2p8_put;
		}

		rc = vreg_enable(vreg_vddio_2p8);
		if (rc) {
			CDBG("%s: vreg_enable() = %d \n",
				__func__, rc);
			goto vddio_2p8_put;
		}
	}
	return;

vddio_2p8_put:
	vreg_put(vreg_vddio_2p8);
	vreg_vddio_2p8 = NULL;
gp2_disable:
	vreg_disable(vreg_gp2);
gp2_put:
	vreg_put(vreg_gp2);
	vreg_gp2 = NULL;
gp10_disable:
	vreg_disable(vreg_gp10);
gp10_put:
	vreg_put(vreg_gp10);
	vreg_gp10 = NULL;
#endif
#else
	vreg_gp2 = vreg_get(NULL, "gp2");
	if (IS_ERR(vreg_gp2)) {
		pr_err("%s: VREG GP2 get failed %ld\n", __func__,
			PTR_ERR(vreg_gp2));
		vreg_gp2 = NULL;
		return;
	}

	if (vreg_set_level(vreg_gp2, 2600)) {
		pr_err("%s: VREG GP2 set failed\n", __func__);
		goto gp2_put;
	}

	if (vreg_enable(vreg_gp2)) {
		pr_err("%s: VREG GP2 enable failed\n", __func__);
		goto gp2_put;
		}

	vreg_lvsw1 = vreg_get(NULL, "lvsw1");
	if (IS_ERR(vreg_lvsw1)) {
		pr_err("%s: VREG LVSW1 get failed %ld\n", __func__,
			PTR_ERR(vreg_lvsw1));
		vreg_lvsw1 = NULL;
		goto gp2_disable;
		}
	if (vreg_set_level(vreg_lvsw1, 1800)) {
		pr_err("%s: VREG LVSW1 set failed\n", __func__);
		goto lvsw1_put;
	}
	if (vreg_enable(vreg_lvsw1))
		pr_err("%s: VREG LVSW1 enable failed\n", __func__);

	return;

lvsw1_put:
	vreg_put(vreg_lvsw1);
gp2_disable:
	vreg_disable(vreg_gp2);
gp2_put:
	vreg_put(vreg_gp2);
#endif
}

#if defined(CONFIG_MACH_ACER_A5)
#ifdef CONFIG_MT9E013
static void msm_camera_mt9e013_vreg_disable(const struct msm_camera_sensor_info *data)
{
	if (vreg_gp4) {
		vreg_disable(vreg_gp4);
		vreg_put(vreg_gp4);
	}
	if (vreg_gp13) {
		vreg_disable(vreg_gp13);
		vreg_put(vreg_gp13);
	}
	if (vreg_gp9) {
		vreg_disable(vreg_gp9);
		vreg_put(vreg_gp9);
	}
}
#endif
#ifdef CONFIG_MT9D115
static void msm_camera_mt9d115_vreg_disable(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	if (vreg_wlan2) {
		vreg_disable(vreg_wlan2);
		vreg_put(vreg_wlan2);
	}

	// Pull down standby pin
	rc = gpio_request(data->sensor_pwd, "mt9d115");
	if (!rc) {
		gpio_direction_output(data->sensor_pwd, 0);
		gpio_free(data->sensor_pwd);
	} else {
		pr_err("%s: request power down pin failed\n", __func__);
	}
}
#endif
#endif

static void msm_camera_vreg_disable(const struct platform_device *pdev)
{
#if defined(CONFIG_MACH_ACER_A5)
#ifdef CONFIG_MT9E013
	if (!strcmp(pdev->name, "msm_camera_mt9e013")) {
		msm_camera_mt9e013_vreg_disable(pdev->dev.platform_data);
	}
#endif
#ifdef CONFIG_MT9D115
	if (!strcmp(pdev->name, "msm_camera_mt9d115")) {
		msm_camera_mt9d115_vreg_disable(pdev->dev.platform_data);
	}
#endif
#elif defined(CONFIG_MACH_ACER_A4)
#ifdef CONFIG_S5K4E1GX
	if (vreg_gp2) {
		vreg_disable(vreg_gp2);
		vreg_put(vreg_gp2);
		vreg_gp2 = NULL;
	}
	if (vreg_gp10) {
		vreg_disable(vreg_gp10);
		vreg_put(vreg_gp10);
		vreg_gp10 = NULL;
	}
	if (vreg_vddio_2p8) {
		vreg_disable(vreg_vddio_2p8);
		vreg_put(vreg_vddio_2p8);
		vreg_vddio_2p8 = NULL;
	}
#endif
#else
	if (vreg_gp2) {
		vreg_disable(vreg_gp2);
		vreg_put(vreg_gp2);
	}
	if (vreg_lvsw1) {
		vreg_disable(vreg_lvsw1);
		vreg_put(vreg_lvsw1);
	}
#endif
}

int msm_camio_clk_enable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_VFE_MDC_CLK:
		camio_vfe_mdc_clk =
		clk = clk_get(NULL, "vfe_mdc_clk");
		break;

	case CAMIO_MDC_CLK:
		camio_mdc_clk =
		clk = clk_get(NULL, "mdc_clk");
		break;

	case CAMIO_VFE_CLK:
		camio_vfe_clk =
		clk = clk_get(NULL, "vfe_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.vfe_clk_rate);
		break;

	case CAMIO_VFE_CAMIF_CLK:
		camio_vfe_camif_clk =
		clk = clk_get(NULL, "vfe_camif_clk");
		break;

	case CAMIO_VFE_PBDG_CLK:
		camio_vfe_pbdg_clk =
		clk = clk_get(NULL, "vfe_pclk");
		break;

	case CAMIO_CAM_MCLK_CLK:
		camio_cam_m_clk =
		clk = clk_get(NULL, "cam_m_clk");
		msm_camio_clk_rate_set_2(clk, camio_clk.mclk_clk_rate);
		break;

	case CAMIO_CAMIF_PAD_PBDG_CLK:
		camio_camif_pad_pbdg_clk =
		clk = clk_get(NULL, "camif_pad_pclk");
		break;

	case CAMIO_CSI0_CLK:
		camio_csi_clk =
		clk = clk_get(NULL, "csi_clk");
		msm_camio_clk_rate_set_2(clk, 153600000);
		break;
	case CAMIO_CSI0_VFE_CLK:
		camio_csi_vfe_clk =
		clk = clk_get(NULL, "csi_vfe_clk");
		break;
	case CAMIO_CSI0_PCLK:
		camio_csi_pclk =
		clk = clk_get(NULL, "csi_pclk");
		break;

	case CAMIO_JPEG_CLK:
		camio_jpeg_clk =
		clk = clk_get(NULL, "jpeg_clk");
		clk_set_min_rate(clk, 144000000);
		break;
	case CAMIO_JPEG_PCLK:
		camio_jpeg_pclk =
		clk = clk_get(NULL, "jpeg_pclk");
		break;
	case CAMIO_VPE_CLK:
		camio_vpe_clk =
		clk = clk_get(NULL, "vpe_clk");
		msm_camio_clk_set_min_rate(clk, 150000000);
		break;
	default:
		break;
	}

	if (!IS_ERR(clk))
		clk_enable(clk);
	else
		rc = -1;
	return rc;
}

int msm_camio_clk_disable(enum msm_camio_clk_type clktype)
{
	int rc = 0;
	struct clk *clk = NULL;

	switch (clktype) {
	case CAMIO_VFE_MDC_CLK:
		clk = camio_vfe_mdc_clk;
		break;

	case CAMIO_MDC_CLK:
		clk = camio_mdc_clk;
		break;

	case CAMIO_VFE_CLK:
		clk = camio_vfe_clk;
		break;

	case CAMIO_VFE_CAMIF_CLK:
		clk = camio_vfe_camif_clk;
		break;

	case CAMIO_VFE_PBDG_CLK:
		clk = camio_vfe_pbdg_clk;
		break;

	case CAMIO_CAM_MCLK_CLK:
		clk = camio_cam_m_clk;
		break;

	case CAMIO_CAMIF_PAD_PBDG_CLK:
		clk = camio_camif_pad_pbdg_clk;
		break;
	case CAMIO_CSI0_CLK:
		clk = camio_csi_clk;
		break;
	case CAMIO_CSI0_VFE_CLK:
		clk = camio_csi_vfe_clk;
		break;
	case CAMIO_CSI0_PCLK:
		clk = camio_csi_pclk;
		break;
	case CAMIO_JPEG_CLK:
		clk = camio_jpeg_clk;
		break;
	case CAMIO_JPEG_PCLK:
		clk = camio_jpeg_pclk;
		break;
	case CAMIO_VPE_CLK:
		clk = camio_vpe_clk;
		break;
	default:
		break;
	}

	if (!IS_ERR(clk)) {
		clk_disable(clk);
		clk_put(clk);
	} else
		rc = -1;

	return rc;
}

void msm_camio_clk_rate_set(int rate)
{
	struct clk *clk = camio_cam_m_clk;
	clk_set_rate(clk, rate);
}

void msm_camio_vfe_clk_rate_set(int rate)
{
	struct clk *clk = camio_vfe_clk;
	clk_set_rate(clk, rate);
}

void msm_camio_clk_rate_set_2(struct clk *clk, int rate)
{
	clk_set_rate(clk, rate);
}

void msm_camio_clk_set_min_rate(struct clk *clk, int rate)
{
	clk_set_min_rate(clk, rate);
}

static irqreturn_t msm_io_csi_irq(int irq_num, void *data)
{
	uint32_t irq;
	irq = msm_io_r(csibase + MIPI_INTERRUPT_STATUS);
	CDBG("%s MIPI_INTERRUPT_STATUS = 0x%x\n", __func__, irq);
	msm_io_w(irq, csibase + MIPI_INTERRUPT_STATUS);
	return IRQ_HANDLED;
}

int msm_camio_jpeg_clk_disable(void)
{
	msm_camio_clk_disable(CAMIO_JPEG_CLK);
	msm_camio_clk_disable(CAMIO_JPEG_PCLK);
	/* Need to add the code for remove PM QOS requirement */
	return 0;
}


int msm_camio_jpeg_clk_enable(void)
{
	msm_camio_clk_enable(CAMIO_JPEG_CLK);
	msm_camio_clk_enable(CAMIO_JPEG_PCLK);
	return 0;
}

int msm_camio_vpe_clk_disable(void)
{
	msm_camio_clk_disable(CAMIO_VPE_CLK);
	return 0;
}

int msm_camio_vpe_clk_enable(void)
{
	msm_camio_clk_enable(CAMIO_VPE_CLK);
	return 0;
}

int msm_camio_enable(struct platform_device *pdev)
{
	int rc = 0;
	uint32_t val;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	msm_camio_clk_enable(CAMIO_VFE_PBDG_CLK);
	if (!sinfo->csi_if)
		msm_camio_clk_enable(CAMIO_VFE_CAMIF_CLK);
	else {
		msm_camio_clk_enable(CAMIO_VFE_CLK);
		csiio = request_mem_region(camio_ext.csiphy,
			camio_ext.csisz, pdev->name);
		if (!csiio) {
			rc = -EBUSY;
			goto common_fail;
		}
		csibase = ioremap(camio_ext.csiphy,
			camio_ext.csisz);
		if (!csibase) {
			rc = -ENOMEM;
			goto csi_busy;
		}
		rc = request_irq(camio_ext.csiirq, msm_io_csi_irq,
			IRQF_TRIGGER_RISING, "csi", 0);
		if (rc < 0)
			goto csi_irq_fail;
		/* enable required clocks for CSI */
		msm_camio_clk_enable(CAMIO_CSI0_PCLK);
		msm_camio_clk_enable(CAMIO_CSI0_VFE_CLK);
		msm_camio_clk_enable(CAMIO_CSI0_CLK);

		msleep(10);
		val = (20 <<
			MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT) |
			(0x0F << MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT) |
			(0x0 << MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT) |
			(0x1 << MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT);
		CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D2_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D3_CONTROL2);

		val = (0x0F << MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT) |
			(0x0 << MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT);
		CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_CL_CONTROL);
	}
	return 0;
csi_irq_fail:
	iounmap(csibase);
csi_busy:
	release_mem_region(camio_ext.csiphy, camio_ext.csisz);
common_fail:
	msm_camio_clk_disable(CAMIO_VFE_PBDG_CLK);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
	return rc;
}

void msm_camio_disable(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	uint32_t val;
	if (!sinfo->csi_if) {
		msm_camio_clk_disable(CAMIO_VFE_CAMIF_CLK);
	} else {
		val = (20 <<
			MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT) |
			(0x0F << MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT) |
			(0x0 << MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT) |
			(0x1 << MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT);
		CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D2_CONTROL2);
		msm_io_w(val, csibase + MIPI_PHY_D3_CONTROL2);
		val = (0x0F << MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT) |
			(0x0 << MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT);
		CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
		msm_io_w(val, csibase + MIPI_PHY_CL_CONTROL);
		msleep(10);
		free_irq(camio_ext.csiirq, 0);
		msm_camio_clk_disable(CAMIO_CSI0_PCLK);
		msm_camio_clk_disable(CAMIO_CSI0_VFE_CLK);
		msm_camio_clk_disable(CAMIO_CSI0_CLK);
		msm_camio_clk_disable(CAMIO_VFE_CLK);
		iounmap(csibase);
		release_mem_region(camio_ext.csiphy, camio_ext.csisz);
	}
	msm_camio_clk_disable(CAMIO_VFE_PBDG_CLK);
}

void msm_camio_camif_pad_reg_reset(void)
{
	uint32_t reg;

	msm_camio_clk_sel(MSM_CAMIO_CLK_SRC_INTERNAL);
	msleep(10);

	reg = (msm_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	reg |= 0x3;
	msm_io_w(reg, camifpadbase);
	msleep(10);

	reg = (msm_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	reg |= 0x10;
	msm_io_w(reg, camifpadbase);
	msleep(10);

	reg = (msm_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	/* Need to be uninverted*/
	reg &= 0x03;
	msm_io_w(reg, camifpadbase);
	msleep(10);
}

void msm_camio_vfe_blk_reset(void)
{
	return;


}

void msm_camio_camif_pad_reg_reset_2(void)
{
	uint32_t reg;
	uint32_t mask, value;
	reg = (msm_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 1 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_io_w((reg & (~mask)) | (value & mask), camifpadbase);
	mdelay(10);
	reg = (msm_io_r(camifpadbase)) & CAMIF_CFG_RMSK;
	mask = CAM_PAD_REG_SW_RESET_BMSK;
	value = 0 << CAM_PAD_REG_SW_RESET_SHFT;
	msm_io_w((reg & (~mask)) | (value & mask), camifpadbase);
	mdelay(10);
}

void msm_camio_clk_sel(enum msm_camio_clk_src_type srctype)
{
	struct clk *clk = NULL;

	clk = camio_vfe_clk;

	if (clk != NULL) {
		switch (srctype) {
		case MSM_CAMIO_CLK_SRC_INTERNAL:
			clk_set_flags(clk, 0x00000100 << 1);
			break;

		case MSM_CAMIO_CLK_SRC_EXTERNAL:
			clk_set_flags(clk, 0x00000100);
			break;

		default:
			break;
		}
	}
}
int msm_camio_probe_on(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
#if defined(CONFIG_MACH_ACER_A4)
#ifdef CONFIG_S5K4E1GX
	acer_smem_flag_t *acer_smem_flag;
	// smem_alloc acer_smem_flag_t
	acer_smem_flag = (acer_smem_flag_t *)(smem_alloc(SMEM_ID_VENDOR0, sizeof(acer_smem_flag_t)));
	if (acer_smem_flag == NULL) {
		pr_err("msm_camio_probe_on: smem_alloc SMEM_ID_VENDOR0 error\n");
		return -1;
	}

	// Get HW version.
	acer_hw_version = acer_smem_flag->acer_hw_version;
#endif
#endif
	camio_clk = camdev->ioclk;
	camio_ext = camdev->ioext;
	camdev->camera_gpio_on();
	msm_camera_vreg_enable(pdev);
	return msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
}

int msm_camio_probe_off(struct platform_device *pdev)
{
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	msm_camera_vreg_disable(pdev);
	camdev->camera_gpio_off();
	return msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
}

int msm_camio_sensor_clk_on(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camio_clk = camdev->ioclk;
	camio_ext = camdev->ioext;
	camdev->camera_gpio_on();
	msm_camera_vreg_enable(pdev);
	msm_camio_clk_enable(CAMIO_CAM_MCLK_CLK);
	msm_camio_clk_enable(CAMIO_CAMIF_PAD_PBDG_CLK);
	if (!sinfo->csi_if) {
		camifpadio = request_mem_region(camio_ext.camifpadphy,
			camio_ext.camifpadsz, pdev->name);
		msm_camio_clk_enable(CAMIO_VFE_CLK);
		if (!camifpadio) {
			rc = -EBUSY;
			goto common_fail;
		}
		camifpadbase = ioremap(camio_ext.camifpadphy,
			camio_ext.camifpadsz);
		if (!camifpadbase) {
			CDBG("msm_camio_sensor_clk_on fail\n");
			rc = -ENOMEM;
			goto parallel_busy;
		}
	}
	return rc;
parallel_busy:
	release_mem_region(camio_ext.camifpadphy, camio_ext.camifpadsz);
	goto common_fail;
common_fail:
	msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
	msm_camio_clk_disable(CAMIO_VFE_CLK);
	msm_camio_clk_disable(CAMIO_CAMIF_PAD_PBDG_CLK);
	msm_camera_vreg_disable(pdev);
	camdev->camera_gpio_off();
	return rc;
}

int msm_camio_sensor_clk_off(struct platform_device *pdev)
{
	uint32_t rc = 0;
	struct msm_camera_sensor_info *sinfo = pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	camdev->camera_gpio_off();
	msm_camera_vreg_disable(pdev);
	rc = msm_camio_clk_disable(CAMIO_CAM_MCLK_CLK);
	rc = msm_camio_clk_disable(CAMIO_CAMIF_PAD_PBDG_CLK);
	if (!sinfo->csi_if) {
		iounmap(camifpadbase);
		release_mem_region(camio_ext.camifpadphy, camio_ext.camifpadsz);
		rc = msm_camio_clk_disable(CAMIO_VFE_CLK);
	}
	return rc;
}

int msm_camio_csi_config(struct msm_camera_csi_params *csi_params)
{
	int rc = 0;
	uint32_t val = 0;

	CDBG("msm_camio_csi_config \n");

	/* SOT_ECC_EN enable error correction for SYNC (data-lane) */
	msm_io_w(0x4, csibase + MIPI_PHY_CONTROL);

	/* SW_RST to the CSI core */
	msm_io_w(MIPI_PROTOCOL_CONTROL_SW_RST_BMSK,
		csibase + MIPI_PROTOCOL_CONTROL);

	/* PROTOCOL CONTROL */
	val = MIPI_PROTOCOL_CONTROL_LONG_PACKET_HEADER_CAPTURE_BMSK |
		MIPI_PROTOCOL_CONTROL_DECODE_ID_BMSK |
		MIPI_PROTOCOL_CONTROL_ECC_EN_BMSK;
	val |= (uint32_t)(csi_params->data_format) <<
		MIPI_PROTOCOL_CONTROL_DATA_FORMAT_SHFT;
	val |= csi_params->dpcm_scheme <<
		MIPI_PROTOCOL_CONTROL_DPCM_SCHEME_SHFT;
	CDBG("%s MIPI_PROTOCOL_CONTROL val=0x%x\n", __func__, val);
	msm_io_w(val, csibase + MIPI_PROTOCOL_CONTROL);

	/* SW CAL EN */
	val = (0x1 << MIPI_CALIBRATION_CONTROL_SWCAL_CAL_EN_SHFT) |
		(0x1 <<
		MIPI_CALIBRATION_CONTROL_SWCAL_STRENGTH_OVERRIDE_EN_SHFT) |
		(0x1 << MIPI_CALIBRATION_CONTROL_CAL_SW_HW_MODE_SHFT) |
		(0x1 << MIPI_CALIBRATION_CONTROL_MANUAL_OVERRIDE_EN_SHFT);
	CDBG("%s MIPI_CALIBRATION_CONTROL val=0x%x\n", __func__, val);
	msm_io_w(val, csibase + MIPI_CALIBRATION_CONTROL);

	/* settle_cnt is very sensitive to speed!
	increase this value to run at higher speeds */
	val = (csi_params->settle_cnt <<
			MIPI_PHY_D0_CONTROL2_SETTLE_COUNT_SHFT) |
		(0x0F << MIPI_PHY_D0_CONTROL2_HS_TERM_IMP_SHFT) |
		(0x1 << MIPI_PHY_D0_CONTROL2_LP_REC_EN_SHFT) |
		(0x1 << MIPI_PHY_D0_CONTROL2_ERR_SOT_HS_EN_SHFT);
	CDBG("%s MIPI_PHY_D0_CONTROL2 val=0x%x\n", __func__, val);
	msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL2);
	msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL2);
	msm_io_w(val, csibase + MIPI_PHY_D2_CONTROL2);
	msm_io_w(val, csibase + MIPI_PHY_D3_CONTROL2);


	val = (0x0F << MIPI_PHY_CL_CONTROL_HS_TERM_IMP_SHFT) |
		(0x1 << MIPI_PHY_CL_CONTROL_LP_REC_EN_SHFT);
	CDBG("%s MIPI_PHY_CL_CONTROL val=0x%x\n", __func__, val);
	msm_io_w(val, csibase + MIPI_PHY_CL_CONTROL);

	val = 0 << MIPI_PHY_D0_CONTROL_HS_REC_EQ_SHFT;
	msm_io_w(val, csibase + MIPI_PHY_D0_CONTROL);

	val = (0x1 << MIPI_PHY_D1_CONTROL_MIPI_CLK_PHY_SHUTDOWNB_SHFT) |
		(0x1 << MIPI_PHY_D1_CONTROL_MIPI_DATA_PHY_SHUTDOWNB_SHFT);
	CDBG("%s MIPI_PHY_D1_CONTROL val=0x%x\n", __func__, val);
	msm_io_w(val, csibase + MIPI_PHY_D1_CONTROL);

	msm_io_w(0x00000000, csibase + MIPI_PHY_D2_CONTROL);
	msm_io_w(0x00000000, csibase + MIPI_PHY_D3_CONTROL);

	/* halcyon only supports 1 or 2 lane */
	switch (csi_params->lane_cnt) {
	case 1:
		msm_io_w(csi_params->lane_assign << 8 | 0x4,
			csibase + MIPI_CAMERA_CNTL);
		break;
	case 2:
		msm_io_w(csi_params->lane_assign << 8 | 0x5,
			csibase + MIPI_CAMERA_CNTL);
		break;
	case 3:
		msm_io_w(csi_params->lane_assign << 8 | 0x6,
			csibase + MIPI_CAMERA_CNTL);
		break;
	case 4:
		msm_io_w(csi_params->lane_assign << 8 | 0x7,
			csibase + MIPI_CAMERA_CNTL);
		break;
	}

	/* mask out ID_ERROR[19], DATA_CMM_ERR[11]
	and CLK_CMM_ERR[10] - de-featured */
	msm_io_w(0xFFF7F3FF, csibase + MIPI_INTERRUPT_MASK);
	/*clear IRQ bits*/
	msm_io_w(0xFFF7F3FF, csibase + MIPI_INTERRUPT_STATUS);

	return rc;
}
void msm_camio_set_perf_lvl(enum msm_bus_perf_setting perf_setting)
{
	switch (perf_setting) {
	case S_INIT:
		add_axi_qos();
		break;
	case S_PREVIEW:
		update_axi_qos(MSM_AXI_QOS_PREVIEW);
		break;
	case S_VIDEO:
		update_axi_qos(MSM_AXI_QOS_RECORDING);
		break;
	case S_CAPTURE:
		update_axi_qos(MSM_AXI_QOS_SNAPSHOT);
		break;
	case S_DEFAULT:
		update_axi_qos(PM_QOS_DEFAULT_VALUE);
		break;
	case S_EXIT:
		release_axi_qos();
		break;
	default:
		CDBG("%s: INVALID CASE\n", __func__);
	}
}

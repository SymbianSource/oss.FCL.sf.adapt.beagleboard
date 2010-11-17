// This component and the accompanying materials are made available
// under the terms of the License "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// lukasz.forynski@gmail.com
//
// Contributors:
//
// Description:
// omap3530/assp/inc/omap3530_scm.h
//
// Contains definitions of SCM pad control registers and
// helper functions to manipulate PAD configuration
// (i.e. to change pin functions / modes)
//

#ifndef __OMAP3530_SCM_H__
#define __OMAP3530_SCM_H__

#include <assp/omap3530_assp/omap3530_hardware_base.h>
#include <assp.h>

class SCM
	{
public:

	enum TPadConfigFlags
		{
		EMode0             = 0x0,
		EMode1             = 0x1,
		EMode2             = 0x2,
		EMode3             = 0x3,
		EMode4             = 0x4,
		EMode5             = 0x5,
		EMode6             = 0x6,
		ETestMode          = 0x7,
		EPullUdEnable      = 0x8,
		EPullTypeSelect    = 0x10,
		EInputEnable       = 0x100,
		EOffEnable         = 0x200,
		EOffOutEnable      = 0x400,
		EOffOutValue       = 0x800,
		EOffPullUdEnable   = 0x1000,
		EOffPullTypeSelect = 0x2000,
		EWakeUpEnable      = 0x4000,
		EWakeUpEvent       = 0x8000,
		};

	enum TLowerHigherWord
		{
		ELsw = 0,
		EMsw
		};

	/**
	 Set pad configuration
	 @param aPadAddr - address of padconf register (one of defined below)
	 @param aAtMsw   - one of TLowerHigherWords (flag) to specify which 16bit part
	                    of the register should be updated (i.e. there is one register
	                    for two pins)
	 @param aConfig  - a bitmask of TPadConfigFlags (note to only useone of EModeXx values)
	 */
	inline static void SetPadConfig(TUint32 aPadAddr, TLowerHigherWord aAtMsw, TUint aConfig)
		{
		TUint clear_mask = aAtMsw ? (0xffffu ^ aConfig) << 16 : 0xffffu ^ aConfig;
		TUint set_mask   = aAtMsw ? (0xffffu & aConfig) << 16 : 0xffffu & aConfig;
		AsspRegister::Modify32(aPadAddr, clear_mask, set_mask);
		}

	/**
	 Get pad configuration
	 */
	inline static TUint GetPadConfig(TUint32 aPadAddr, TLowerHigherWord aAtMsw)
		{
		TUint val = AsspRegister::Read32(aPadAddr);
		return aAtMsw ? val >> 16 : val & 0xffffu;
		}
	};

const TUint32 CONTROL_PADCONF_SDRC_D0 = Omap3530HwBase::TVirtual<0x48002030>::Value; // sdrc_d0
const TUint32 CONTROL_PADCONF_SDRC_D2 = Omap3530HwBase::TVirtual<0x48002034>::Value; // sdrc_d2
const TUint32 CONTROL_PADCONF_SDRC_D4 = Omap3530HwBase::TVirtual<0x48002038>::Value; // sdrc_d4
const TUint32 CONTROL_PADCONF_SDRC_D6 = Omap3530HwBase::TVirtual<0x4800203C>::Value; // sdrc_d6
const TUint32 CONTROL_PADCONF_SDRC_D8 = Omap3530HwBase::TVirtual<0x48002040>::Value; // sdrc_d8
const TUint32 CONTROL_PADCONF_SDRC_D10 = Omap3530HwBase::TVirtual<0x48002044>::Value; // sdrc_d10
const TUint32 CONTROL_PADCONF_SDRC_D12 = Omap3530HwBase::TVirtual<0x48002048>::Value; // sdrc_d12
const TUint32 CONTROL_PADCONF_SDRC_D14 = Omap3530HwBase::TVirtual<0x4800204C>::Value; // sdrc_d14
const TUint32 CONTROL_PADCONF_SDRC_D16 = Omap3530HwBase::TVirtual<0x48002050>::Value; // sdrc_d16
const TUint32 CONTROL_PADCONF_SDRC_D18 = Omap3530HwBase::TVirtual<0x48002054>::Value; // sdrc_d18
const TUint32 CONTROL_PADCONF_SDRC_D20 = Omap3530HwBase::TVirtual<0x48002058>::Value; // sdrc_d20
const TUint32 CONTROL_PADCONF_SDRC_D22 = Omap3530HwBase::TVirtual<0x4800205C>::Value; // sdrc_d22
const TUint32 CONTROL_PADCONF_SDRC_D24 = Omap3530HwBase::TVirtual<0x48002060>::Value; // sdrc_d24
const TUint32 CONTROL_PADCONF_SDRC_D26 = Omap3530HwBase::TVirtual<0x48002064>::Value; // sdrc_d26
const TUint32 CONTROL_PADCONF_SDRC_D28 = Omap3530HwBase::TVirtual<0x48002068>::Value; // sdrc_d28
const TUint32 CONTROL_PADCONF_SDRC_D30 = Omap3530HwBase::TVirtual<0x4800206C>::Value; // sdrc_d30
const TUint32 CONTROL_PADCONF_SDRC_CLK = Omap3530HwBase::TVirtual<0x48002070>::Value; // sdrc_clk
const TUint32 CONTROL_PADCONF_SDRC_CKE1 = Omap3530HwBase::TVirtual<0x48002264>::Value; // sdrc_cke1 safe_mode_
const TUint32 CONTROL_PADCONF_SDRC_DQS1 = Omap3530HwBase::TVirtual<0x48002074>::Value; // sdrc_dqs1
const TUint32 CONTROL_PADCONF_SDRC_DQS3 = Omap3530HwBase::TVirtual<0x48002078>::Value; // sdrc_dqs3
const TUint32 CONTROL_PADCONF_GPMC_A2 = Omap3530HwBase::TVirtual<0x4800207C>::Value; // gpmc_a2 gpio_35 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_A4 = Omap3530HwBase::TVirtual<0x48002080>::Value; // gpmc_a4 gpio_37 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_A6 = Omap3530HwBase::TVirtual<0x48002084>::Value; // gpmc_a6 gpio_39 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_A8 = Omap3530HwBase::TVirtual<0x48002088>::Value; // gpmc_a8 gpio_41 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_A10 = Omap3530HwBase::TVirtual<0x4800208C>::Value; // gpmc_a10 sys_ndmareq3 gpio_43 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_D1 = Omap3530HwBase::TVirtual<0x48002090>::Value; // gpmc_d1
const TUint32 CONTROL_PADCONF_GPMC_D3 = Omap3530HwBase::TVirtual<0x48002094>::Value; // gpmc_d3
const TUint32 CONTROL_PADCONF_GPMC_D5 = Omap3530HwBase::TVirtual<0x48002098>::Value; // gpmc_d5
const TUint32 CONTROL_PADCONF_GPMC_D7 = Omap3530HwBase::TVirtual<0x4800209C>::Value; // gpmc_d7
const TUint32 CONTROL_PADCONF_GPMC_D9 = Omap3530HwBase::TVirtual<0x480020A0>::Value; // gpmc_d9 gpio_45 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_D11 = Omap3530HwBase::TVirtual<0x480020A4>::Value; // gpmc_d11 gpio_47 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_D13 = Omap3530HwBase::TVirtual<0x480020A8>::Value; // gpmc_d13 gpio_49 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_D15 = Omap3530HwBase::TVirtual<0x480020AC>::Value; // gpmc_d15 gpio_51 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_NCS1 = Omap3530HwBase::TVirtual<0x480020B0>::Value; // gpmc_ncs1 gpio_52 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_NCS3 = Omap3530HwBase::TVirtual<0x480020B4>::Value; // gpmc_ncs3 sys_ndmareq0 gpio_54 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_NCS5 = Omap3530HwBase::TVirtual<0x480020B8>::Value; // gpmc_ncs5 sys_ndmareq2 mcbsp4_dr gpt10_pwm_evt gpio_56 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_NCS7 = Omap3530HwBase::TVirtual<0x480020BC>::Value; // gpmc_ncs7 gpmc_io_dir mcbsp4_fsx gpt8_pwm_evt gpio_58 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_NADV_ALE = Omap3530HwBase::TVirtual<0x480020C0>::Value; // gpmc_nadv_ale
const TUint32 CONTROL_PADCONF_GPMC_NWE = Omap3530HwBase::TVirtual<0x480020C4>::Value; // gpmc_nwe
const TUint32 CONTROL_PADCONF_GPMC_NBE1 = Omap3530HwBase::TVirtual<0x480020C8>::Value; // gpmc_nbe1 gpio_61 safe_mode
const TUint32 CONTROL_PADCONF_GPMC_WAIT0 = Omap3530HwBase::TVirtual<0x480020CC>::Value; // gpmc_wait0
const TUint32 CONTROL_PADCONF_GPMC_WAIT2 = Omap3530HwBase::TVirtual<0x480020D0>::Value; // gpmc_wait2 gpio_64 safe_mode
const TUint32 CONTROL_PADCONF_DSS_PCLK = Omap3530HwBase::TVirtual<0x480020D4>::Value; // dss_pclk gpio_66 hw_dbg12 safe_mode
const TUint32 CONTROL_PADCONF_DSS_VSYNC = Omap3530HwBase::TVirtual<0x480020D8>::Value; // dss_vsync gpio_68 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA0 = Omap3530HwBase::TVirtual<0x480020DC>::Value; // dss_data0 dsi_dx0 uart1_cts dssvenc656_data gpio_70 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA2 = Omap3530HwBase::TVirtual<0x480020E0>::Value; // dss_data2 dsi_dx1 dssvenc656_data gpio_72 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA4 = Omap3530HwBase::TVirtual<0x480020E4>::Value; // dss_data4 dsi_dx2 uart3_rx_irrx dssvenc656_data gpio_74 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA6 = Omap3530HwBase::TVirtual<0x480020E8>::Value; // dss_data6 uart1_tx dssvenc656_data gpio_76 hw_dbg14 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA8 = Omap3530HwBase::TVirtual<0x480020EC>::Value; // dss_data8 gpio_78 hw_dbg16 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA10 = Omap3530HwBase::TVirtual<0x480020F0>::Value; // dss_data10 sdi_dat1n gpio_80 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA12 = Omap3530HwBase::TVirtual<0x480020F4>::Value; // dss_data12 sdi_dat2n gpio_82 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA14 = Omap3530HwBase::TVirtual<0x480020F8>::Value; // dss_data14 sdi_dat3n gpio_84 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA16 = Omap3530HwBase::TVirtual<0x480020FC>::Value; // dss_data16 gpio_86 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA18 = Omap3530HwBase::TVirtual<0x48002100>::Value; // dss_data18 sdi_vsync mcspi3_clk dss_data0 gpio_88 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA20 = Omap3530HwBase::TVirtual<0x48002104>::Value; // dss_data20 sdi_den mcspi3_somi dss_data2 gpio_90 safe_mode
const TUint32 CONTROL_PADCONF_DSS_DATA22 = Omap3530HwBase::TVirtual<0x48002108>::Value; // dss_data22 sdi_clkp mcspi3_cs1 dss_data4 gpio_92 safe_mode
const TUint32 CONTROL_PADCONF_CAM_HS = Omap3530HwBase::TVirtual<0x4800210C>::Value; // cam_hs gpio_94 hw_dbg0 safe_mode
const TUint32 CONTROL_PADCONF_CAM_XCLKA = Omap3530HwBase::TVirtual<0x48002110>::Value; // cam_xclka gpio_96 safe_mode
const TUint32 CONTROL_PADCONF_CAM_FLD = Omap3530HwBase::TVirtual<0x48002114>::Value; // cam_fld cam_global_rese gpio_98 hw_dbg3 safe_mode
const TUint32 CONTROL_PADCONF_CAM_D1 = Omap3530HwBase::TVirtual<0x48002118>::Value; // cam_d1 csi2_dy2 gpio_100 safe_mode
const TUint32 CONTROL_PADCONF_CAM_D3 = Omap3530HwBase::TVirtual<0x4800211C>::Value; // cam_d3 gpio_102 hw_dbg5 safe_mode
const TUint32 CONTROL_PADCONF_CAM_D5 = Omap3530HwBase::TVirtual<0x48002120>::Value; // cam_d5 gpio_104 hw_dbg7 safe_mode
const TUint32 CONTROL_PADCONF_CAM_D7 = Omap3530HwBase::TVirtual<0x48002124>::Value; // cam_d7 gpio_106 safe_mode
const TUint32 CONTROL_PADCONF_CAM_D9 = Omap3530HwBase::TVirtual<0x48002128>::Value; // cam_d9 gpio_108 safe_mode
const TUint32 CONTROL_PADCONF_CAM_D11 = Omap3530HwBase::TVirtual<0x4800212C>::Value; // cam_d11 gpio_110 hw_dbg9 safe_mode
const TUint32 CONTROL_PADCONF_CAM_WEN = Omap3530HwBase::TVirtual<0x48002130>::Value; // cam_wen cam_shutter gpio_167 hw_dbg10 safe_mode
const TUint32 CONTROL_PADCONF_CSI2_DX0 = Omap3530HwBase::TVirtual<0x48002134>::Value; // csi2_dx0 gpio_112 safe_mode
const TUint32 CONTROL_PADCONF_CSI2_DX1 = Omap3530HwBase::TVirtual<0x48002138>::Value; // csi2_dx1 gpio_114 safe_mode
const TUint32 CONTROL_PADCONF_MCBSP2_FSX = Omap3530HwBase::TVirtual<0x4800213C>::Value; // mcbsp2_fsx gpio_116 safe_mode
const TUint32 CONTROL_PADCONF_MCBSP2_DR = Omap3530HwBase::TVirtual<0x48002140>::Value; // mcbsp2_dr gpio_118 safe_mode
const TUint32 CONTROL_PADCONF_MMC1_CLK = Omap3530HwBase::TVirtual<0x48002144>::Value; // mmc1_clk ms_clk gpio_120 safe_mode
const TUint32 CONTROL_PADCONF_MMC1_DAT0 = Omap3530HwBase::TVirtual<0x48002148>::Value; // mmc1_dat0 ms_dat0 gpio_122 safe_mode
const TUint32 CONTROL_PADCONF_MMC1_DAT2 = Omap3530HwBase::TVirtual<0x4800214C>::Value; // mmc1_dat2 ms_dat2 gpio_124 safe_mode
const TUint32 CONTROL_PADCONF_MMC1_DAT4 = Omap3530HwBase::TVirtual<0x48002150>::Value; // mmc1_dat4 sim_io gpio_126 safe_mode
const TUint32 CONTROL_PADCONF_MMC1_DAT6 = Omap3530HwBase::TVirtual<0x48002154>::Value; // mmc1_dat6 sim_pwrctrl gpio_128 safe_mode
const TUint32 CONTROL_PADCONF_MMC2_CLK = Omap3530HwBase::TVirtual<0x48002158>::Value; // mmc2_clk mcspi3_clk gpio_130 safe_mode
const TUint32 CONTROL_PADCONF_MMC2_DAT0 = Omap3530HwBase::TVirtual<0x4800215C>::Value; // mmc2_dat0 mcspi3_somi gpio_132 safe_mode
const TUint32 CONTROL_PADCONF_MMC2_DAT2 = Omap3530HwBase::TVirtual<0x48002160>::Value; // mmc2_dat2 mcspi3_cs1 gpio_134 safe_mode
const TUint32 CONTROL_PADCONF_MMC2_DAT4 = Omap3530HwBase::TVirtual<0x48002164>::Value; // mmc2_dat4 mmc2_dir_dat0 mmc3_dat0 gpio_136 safe_mode
const TUint32 CONTROL_PADCONF_MMC2_DAT6 = Omap3530HwBase::TVirtual<0x48002168>::Value; // mmc2_dat6 mmc2_dir_cmd cam_shutter mmc3_dat2 gpio_138 hsusb3_tll_dir safe_mode
const TUint32 CONTROL_PADCONF_MCBSP3_DX = Omap3530HwBase::TVirtual<0x4800216C>::Value; // mcbsp3_dx uart2_cts gpio_140 hsusb3_tll_data4 safe_mode
const TUint32 CONTROL_PADCONF_MCBSP3_CLKX = Omap3530HwBase::TVirtual<0x48002170>::Value; // mcbsp3_clkx uart2_tx gpio_142 hsusb3_tll_data6 safe_mode
const TUint32 CONTROL_PADCONF_UART2_CTS = Omap3530HwBase::TVirtual<0x48002174>::Value; // uart2_cts mcbsp3_dx gpt9_pwm_evt gpio_144 safe_mode
const TUint32 CONTROL_PADCONF_UART2_TX = Omap3530HwBase::TVirtual<0x48002178>::Value; // uart2_tx mcbsp3_clkx gpt11_pwm_evt gpio_146 safe_mode
const TUint32 CONTROL_PADCONF_UART1_TX = Omap3530HwBase::TVirtual<0x4800217C>::Value; // uart1_tx gpio_148 safe_mode
const TUint32 CONTROL_PADCONF_UART1_CTS = Omap3530HwBase::TVirtual<0x48002180>::Value; // uart1_cts gpio_150 hsusb3_tll_clk safe_mode
const TUint32 CONTROL_PADCONF_MCBSP4_CLKX = Omap3530HwBase::TVirtual<0x48002184>::Value; // mcbsp4_clkx gpio_152 hsusb3_tll_data1 mm3_txse0 safe_mode
const TUint32 CONTROL_PADCONF_MCBSP4_DX = Omap3530HwBase::TVirtual<0x48002188>::Value; // mcbsp4_dx gpio_154 hsusb3_tll_data2 mm3_txdat safe_mode
const TUint32 CONTROL_PADCONF_MCBSP1_CLKR = Omap3530HwBase::TVirtual<0x4800218C>::Value; // mcbsp1_clkr mcspi4_clk sim_cd gpio_156 safe_mode
const TUint32 CONTROL_PADCONF_MCBSP1_DX = Omap3530HwBase::TVirtual<0x48002190>::Value; // mcbsp1_dx mcspi4_simo mcbsp3_dx gpio_158 safe_mode
const TUint32 CONTROL_PADCONF_MCBSP_CLKS = Omap3530HwBase::TVirtual<0x48002194>::Value; // mcbsp_clks cam_shutter gpio_160 uart1_cts safe_mode
const TUint32 CONTROL_PADCONF_MCBSP1_CLKX = Omap3530HwBase::TVirtual<0x48002198>::Value; // mcbsp1_clkx mcbsp3_clkx gpio_162 safe_mode
const TUint32 CONTROL_PADCONF_UART3_RTS_SD = Omap3530HwBase::TVirtual<0x4800219C>::Value; // uart3_rts_sd gpio_164 safe_mode
const TUint32 CONTROL_PADCONF_UART3_TX_IRTX = Omap3530HwBase::TVirtual<0x480021A0>::Value; // uart3_tx_irtx gpio_166 safe_mode
const TUint32 CONTROL_PADCONF_HSUSB0_STP = Omap3530HwBase::TVirtual<0x480021A4>::Value; // hsusb0_stp gpio_121 safe_mode
const TUint32 CONTROL_PADCONF_HSUSB0_NXT = Omap3530HwBase::TVirtual<0x480021A8>::Value; // hsusb0_nxt gpio_124 safe_mode
const TUint32 CONTROL_PADCONF_HSUSB0_DATA1 = Omap3530HwBase::TVirtual<0x480021AC>::Value; // hsusb0_data1 uart3_rx_irrx gpio_130 safe_mode
const TUint32 CONTROL_PADCONF_HSUSB0_DATA3 = Omap3530HwBase::TVirtual<0x480021B0>::Value; // hsusb0_data3 uart3_cts_rctx gpio_169 safe_mode
const TUint32 CONTROL_PADCONF_HSUSB0_DATA5 = Omap3530HwBase::TVirtual<0x480021B4>::Value; // hsusb0_data5 gpio_189 safe_mode
const TUint32 CONTROL_PADCONF_HSUSB0_DATA7 = Omap3530HwBase::TVirtual<0x480021B8>::Value; // hsusb0_data7 gpio_191 safe_mode
const TUint32 CONTROL_PADCONF_I2C1_SDA = Omap3530HwBase::TVirtual<0x480021BC>::Value; // i2c1_sda
const TUint32 CONTROL_PADCONF_I2C2_SDA = Omap3530HwBase::TVirtual<0x480021C0>::Value; // i2c2_sda gpio_183 safe_mode
const TUint32 CONTROL_PADCONF_I2C3_SDA = Omap3530HwBase::TVirtual<0x480021C4>::Value; // i2c3_sda gpio_185 safe_mode
const TUint32 CONTROL_PADCONF_MCSPI1_CLK = Omap3530HwBase::TVirtual<0x480021C8>::Value; // mcspi1_clk mmc2_dat4 gpio_171 safe_mode
const TUint32 CONTROL_PADCONF_MCSPI1_SOMI = Omap3530HwBase::TVirtual<0x480021CC>::Value; // mcspi1_somi mmc2_dat6 gpio_173 safe_mode
const TUint32 CONTROL_PADCONF_MCSPI1_CS1 = Omap3530HwBase::TVirtual<0x480021D0>::Value; // mcspi1_cs1 adpllv2d_dither mmc3_cmd gpio_175 safe_mode
const TUint32 CONTROL_PADCONF_MCSPI1_CS3 = Omap3530HwBase::TVirtual<0x480021D4>::Value; // mcspi1_cs3 hsusb2_tll_data2 hsusb2_data2 gpio_177 mm2_txdat safe_mode
const TUint32 CONTROL_PADCONF_MCSPI2_SIMO = Omap3530HwBase::TVirtual<0x480021D8>::Value; // mcspi2_simo gpt9_pwm_evt hsusb2_tll_data4 hsusb2_data4 gpio_179 safe_mode
const TUint32 CONTROL_PADCONF_MCSPI2_CS0 = Omap3530HwBase::TVirtual<0x480021DC>::Value; // mcspi2_cs0 gpt11_pwm_evt hsusb2_tll_data6 hsusb2_data6 gpio_181 safe_mode
const TUint32 CONTROL_PADCONF_SYS_NIRQ = Omap3530HwBase::TVirtual<0x480021E0>::Value; // sys_nirq gpio_0 safe_mode
const TUint32 CONTROL_PADCONF_ETK_CLK = Omap3530HwBase::TVirtual<0x480025D8>::Value; // etk_clk mcbsp5_clkx mmc3_clk hsusb1_stp gpio_12 mm1_rxdp hsusb1_tll_stp hw_dbg0
const TUint32 CONTROL_PADCONF_ETK_D0 = Omap3530HwBase::TVirtual<0x480025DC>::Value; // etk_d0 mcspi3_simo mmc3_dat4 hsusb1_data0 gpio_14 mm1_rxrcv hsusb1_tll_data0 hw_dbg2
const TUint32 CONTROL_PADCONF_ETK_D2 = Omap3530HwBase::TVirtual<0x480025E0>::Value; // etk_d2 mcspi3_cs0 hsusb1_data2 gpio_16 mm1_txdat hsusb1_tll_data2 hw_dbg4
const TUint32 CONTROL_PADCONF_ETK_D4 = Omap3530HwBase::TVirtual<0x480025E4>::Value; // etk_d4 mcbsp5_dr mmc3_dat0 hsusb1_data4 gpio_18 hsusb1_tll_data4 hw_dbg6
const TUint32 CONTROL_PADCONF_ETK_D6 = Omap3530HwBase::TVirtual<0x480025E8>::Value; // etk_d6 mcbsp5_dx mmc3_dat2 hsusb1_data6 gpio_20 hsusb1_tll_data6 hw_dbg8
const TUint32 CONTROL_PADCONF_ETK_D8 = Omap3530HwBase::TVirtual<0x480025EC>::Value; // etk_d8 Reserved for mmc3_dat6 hsusb1_dir gpio_22 hsusb1_tll_dir hw_dbg10
const TUint32 CONTROL_PADCONF_ETK_D10 = Omap3530HwBase::TVirtual<0x480025F0>::Value; // etk_d10 uart1_rx hsusb2_clk gpio_24 hsusb2_tll_clk hw_dbg12
const TUint32 CONTROL_PADCONF_ETK_D12 = Omap3530HwBase::TVirtual<0x480025F4>::Value; // etk_d12 hsusb2_dir gpio_26 hsusb2_tll_dir hw_dbg14
const TUint32 CONTROL_PADCONF_ETK_D14 = Omap3530HwBase::TVirtual<0x480025F8>::Value; // etk_d14 hsusb2_data0 gpio_28 mm2_rxrcv hsusb2_tll_data0 hw_dbg16
const TUint32 CONTROL_PADCONF_SAD2D_MCAD0 = Omap3530HwBase::TVirtual<0x480021E4>::Value; // sad2d_mcad0 mad2d_mcad0
const TUint32 CONTROL_PADCONF_SAD2D_MCAD2 = Omap3530HwBase::TVirtual<0x480021E8>::Value; // sad2d_mcad2 mad2d_mcad2
const TUint32 CONTROL_PADCONF_SAD2D_MCAD4 = Omap3530HwBase::TVirtual<0x480021EC>::Value; // sad2d_mcad4 mad2d_mcad4
const TUint32 CONTROL_PADCONF_SAD2D_MCAD6 = Omap3530HwBase::TVirtual<0x480021F0>::Value; // sad2d_mcad6 mad2d_mcad6
const TUint32 CONTROL_PADCONF_SAD2D_MCAD8 = Omap3530HwBase::TVirtual<0x480021F4>::Value; // sad2d_mcad8 mad2d_mcad8
const TUint32 CONTROL_PADCONF_SAD2D_MCAD10 = Omap3530HwBase::TVirtual<0x480021F8>::Value; // sad2d_mcad10 mad2d_mcad10
const TUint32 CONTROL_PADCONF_SAD2D_MCAD12 = Omap3530HwBase::TVirtual<0x480021FC>::Value; // sad2d_mcad12 mad2d_mcad12
const TUint32 CONTROL_PADCONF_SAD2D_MCAD14 = Omap3530HwBase::TVirtual<0x48002200>::Value; // sad2d_mcad14 mad2d_mcad14
const TUint32 CONTROL_PADCONF_SAD2D_MCAD16 = Omap3530HwBase::TVirtual<0x48002204>::Value; // sad2d_mcad16 mad2d_mcad16
const TUint32 CONTROL_PADCONF_SAD2D_MCAD18 = Omap3530HwBase::TVirtual<0x48002208>::Value; // sad2d_mcad18 mad2d_mcad18
const TUint32 CONTROL_PADCONF_SAD2D_MCAD20 = Omap3530HwBase::TVirtual<0x4800220C>::Value; // sad2d_mcad20 mad2d_mcad20
const TUint32 CONTROL_PADCONF_SAD2D_MCAD22 = Omap3530HwBase::TVirtual<0x48002210>::Value; // sad2d_mcad22 mad2d_mcad22
const TUint32 CONTROL_PADCONF_SAD2D_MCAD24 = Omap3530HwBase::TVirtual<0x48002214>::Value; // sad2d_mcad24 mad2d_mcad24
const TUint32 CONTROL_PADCONF_SAD2D_MCAD26 = Omap3530HwBase::TVirtual<0x48002218>::Value; // sad2d_mcad26 mad2d_mcad26
const TUint32 CONTROL_PADCONF_SAD2D_MCAD28 = Omap3530HwBase::TVirtual<0x4800221C>::Value; // sad2d_mcad28 mad2d_mcad28
const TUint32 CONTROL_PADCONF_SAD2D_MCAD30 = Omap3530HwBase::TVirtual<0x48002220>::Value; // sad2d_mcad30 mad2d_mcad30
const TUint32 CONTROL_PADCONF_SAD2D_MCAD32 = Omap3530HwBase::TVirtual<0x48002224>::Value; // sad2d_mcad32 mad2d_mcad32
const TUint32 CONTROL_PADCONF_SAD2D_MCAD34 = Omap3530HwBase::TVirtual<0x48002228>::Value; // sad2d_mcad34 mad2d_mcad34
const TUint32 CONTROL_PADCONF_SAD2D_MCAD36 = Omap3530HwBase::TVirtual<0x4800222C>::Value; // sad2d_mcad36 mad2d_mcad36
const TUint32 CONTROL_PADCONF_SAD2D_NRESPWRON = Omap3530HwBase::TVirtual<0x48002230>::Value; // sad2d_nrespwron
const TUint32 CONTROL_PADCONF_SAD2D_ARMNIRQ = Omap3530HwBase::TVirtual<0x48002234>::Value; // sad2d_armnirq
const TUint32 CONTROL_PADCONF_SAD2D_SPINT = Omap3530HwBase::TVirtual<0x48002238>::Value; // sad2d_spint gpio_187
const TUint32 CONTROL_PADCONF_SAD2D_DMAREQ0 = Omap3530HwBase::TVirtual<0x4800223C>::Value; // sad2d_dmareq0 uart2_dma_tx mmc1_dma_tx
const TUint32 CONTROL_PADCONF_SAD2D_DMAREQ2 = Omap3530HwBase::TVirtual<0x48002240>::Value; // sad2d_dmareq2 uart1_dma_tx uart3_dma_tx
const TUint32 CONTROL_PADCONF_SAD2D_NTRST = Omap3530HwBase::TVirtual<0x48002244>::Value; // sad2d_ntrst
const TUint32 CONTROL_PADCONF_SAD2D_TDO = Omap3530HwBase::TVirtual<0x48002248>::Value; // sad2d_tdo
const TUint32 CONTROL_PADCONF_SAD2D_TCK = Omap3530HwBase::TVirtual<0x4800224C>::Value; // sad2d_tck
const TUint32 CONTROL_PADCONF_SAD2D_MSTDBY = Omap3530HwBase::TVirtual<0x48002250>::Value; // sad2d_mstdby
const TUint32 CONTROL_PADCONF_SAD2D_IDLEACK = Omap3530HwBase::TVirtual<0x48002254>::Value; // sad2d_idleack
const TUint32 CONTROL_PADCONF_SAD2D_SWRITE = Omap3530HwBase::TVirtual<0x48002258>::Value; // sad2d_swrite mad2d_mwrite
const TUint32 CONTROL_PADCONF_SAD2D_SREAD = Omap3530HwBase::TVirtual<0x4800225C>::Value; // sad2d_sread mad2d_mread
const TUint32 CONTROL_PADCONF_SAD2D_SBUSFLAG = Omap3530HwBase::TVirtual<0x48002260>::Value; // sad2d_sbusflag mad2d_mbusflag
const TUint32 CONTROL_PADCONF_I2C4_SCL = Omap3530HwBase::TVirtual<0x48002A00>::Value; // i2c4_scl sys_nvmode1 safe_mode
const TUint32 CONTROL_PADCONF_SYS_32K = Omap3530HwBase::TVirtual<0x48002A04>::Value; // sys_32k
const TUint32 CONTROL_PADCONF_SYS_NRESWARM = Omap3530HwBase::TVirtual<0x48002A08>::Value; // sys_nreswarm gpio_30 safe_mode
const TUint32 CONTROL_PADCONF_SYS_BOOT1 = Omap3530HwBase::TVirtual<0x48002A0C>::Value; // sys_boot1 gpio_3 safe_mode
const TUint32 CONTROL_PADCONF_SYS_BOOT3 = Omap3530HwBase::TVirtual<0x48002A10>::Value; // sys_boot3 gpio_5 safe_mode
const TUint32 CONTROL_PADCONF_SYS_BOOT5 = Omap3530HwBase::TVirtual<0x48002A14>::Value; // sys_boot5 mmc2_dir_dat3 gpio_7 safe_mode
const TUint32 CONTROL_PADCONF_SYS_OFF_MODE = Omap3530HwBase::TVirtual<0x48002A18>::Value; // sys_off_mode gpio_9 safe_mode
const TUint32 CONTROL_PADCONF_JTAG_NTRST = Omap3530HwBase::TVirtual<0x48002A1C>::Value; // jtag_ntrst
const TUint32 CONTROL_PADCONF_JTAG_TMS_TMSC = Omap3530HwBase::TVirtual<0x48002A20>::Value; // jtag_tms_tmsc
const TUint32 CONTROL_PADCONF_JTAG_EMU0 = Omap3530HwBase::TVirtual<0x48002A24>::Value; // jtag_emu0 gpio_11 safe_mode
const TUint32 CONTROL_PADCONF_SAD2D_SWAKEUP = Omap3530HwBase::TVirtual<0x48002A4C>::Value; // sad2d_swakeup
const TUint32 CONTROL_PADCONF_JTAG_TDO = Omap3530HwBase::TVirtual<0x48002A50>::Value; // jtag_tdo

#endif /*__OMAP3530_SCM_H__*/

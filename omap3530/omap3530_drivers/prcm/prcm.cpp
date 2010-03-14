// Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the License "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// \omap3530\omap3530_assp\prcm.cpp
// Access to PRCM. And implimentation of device driver's power and clock control API
// This file is part of the Beagle Base port
//

#include <e32cmn.h>
#include <assp/omap3530_assp/omap3530_prcm.h>
#include <assp/omap3530_assp/omap3530_ktrace.h>
#include <assp/omap3530_assp/omap3530_irqmap.h>
#include <assp/omap3530_assp/omap3530_hardware_base.h>
#include <assp/omap3530_assp/omap3530_assp_priv.h>
#include <nkern.h>

#include "prcm_regs.h"

// Dummy location for redirecting writes which have no effect on a particular clock
// More efficient than having to test for it in code
TUint32 __dummypoke;
#define	KDummy	(TUint32)&__dummypoke

namespace
{

// PLL modes
const TUint32 KPllModeStop	= 0x1;
const TUint32 KPllModeBypass = 0x5;
const TUint32 KPllModeFastRelock = 0x6;
const TUint32 KPllModeLock = 0x7;
const TUint32 KPllModeMask = 0x7;
const TUint32 KPllAutoOff = 0x0;
const TUint32 KPllAutoOn = 0x1;
const TUint32 KPllAutoMask = 0x7;

#ifdef _DEBUG	// to stop warings about unused definitions
const TUint	KPllMaximumDivider		= 127;
const TUint	KPllMaximumMultiplier	= 2047;
#endif
const TUint	KPllDividerMask		= 127;
const TUint	KPllMultiplierMask	= 2047;
const TUint	KPllFreqRangeMask	= 15;
const TUint	KPllRampMask		= 3;

const TUint KPllLpModeMaximumFrequency = 600000000;

// TPll to TClock lookup table
static const Prcm::TClock KPllToClock [] =
	{
	Prcm::EClkMpu,
	Prcm::EClkIva2Pll,
	Prcm::EClkCore,
	Prcm::EClkPeriph,
	Prcm::EClkPeriph2
	};

// struct of info on how to configure each PLL
// this doesn't include settings which are the same for all PLLs
struct TPllControlInfo
	{
	TUint32	iConfigRegister;		// register containing configuration settings
	TUint32	iMulDivRegister;		// register containing multiplier and divider setting
	TUint32	iStatusRegister;		// register containing PLL status
	TUint	iMultShift;				// shift to move multiplier into position
	TUint	iDivShift;				// shift to move divider into position
	TUint	iFreqSelShift;			// shift to move frequency range selection into position
	TUint	iRampShift;				// shift to move ramp bits into position
	TUint	iDriftShift;			// shift to move driftsel into position
	TUint	iLpShift;				// shift to move LP bit into position
	TUint	iLockBit;				// bit number of lock flag in iStatusRegister
	};

static const TPllControlInfo KPllControlInfo[ Prcm::KSupportedPllCount ] =
	{
	//	ConfReg				MulDivReg			StatusReg				MulShift	DivShift	FreqShift	RampShift	DriftShift	LpShift	LockBit
		{ KCM_CLKEN_PLL_MPU,  KCM_CLKSEL1_PLL_MPU,	KCM_IDLEST_PLL_MPU,		8,		0,			4,			8,			3,			10,		0 },		// DPLL1 (mpu)
		{ KCM_CLKEN_PLL_IVA2, KCM_CLKSEL1_PLL_IVA2,	KCM_IDLEST_PLL_IVA2,	8,		0,			4,			8,			3,			10,		0 },		// DPLL2 (iva2)
		{ KCM_CLKEN_PLL,	KCM_CLKSEL1_PLL,		KCM_IDLEST_CKGEN,		16,		8,			4,			8,			3,			10,		0 },		// DPLL3 (core)
		{ KCM_CLKEN_PLL,	KCM_CLKSEL2_PLL,		KCM_IDLEST_CKGEN,		8,		0,			20,			24,			19,			26,		1 },		// DPLL4 (periph)
		{ KCM_CLKEN2_PLL,	KCM_CLKSEL4_PLL,		KCM_IDLEST2_CKGEN,		8,		0,			4,			8,			3,			10,		0 }		// DPLL5 (periph2)
	};
__ASSERT_COMPILE( (sizeof(KPllControlInfo) / sizeof( KPllControlInfo[0] )) == Prcm::KSupportedPllCount );

struct TPllModeInfo
	{
	TUint32		iModeRegister;
	TUint32		iAutoRegister;
	TUint8		iModeShift;
	TUint8		iAutoShift;
	TUint8		_spare[2];
	};

static const TPllModeInfo KPllMode[] =
	{
		// iModeRegister		iAutoRegister			iModeShift	iAutoShift
		{ KCM_CLKEN_PLL_MPU,	KCM_AUTOIDLE_PLL_MPU,	0,			0 },
		{ KCM_CLKEN_PLL_IVA2,	KCM_AUTOIDLE_PLL_IVA2,	0,			0 },
		{ KCM_CLKEN_PLL,		KCM_AUTOIDLE_PLL,		0,			0 },
		{ KCM_CLKEN_PLL,		KCM_AUTOIDLE_PLL,		16,			3 },
		{ KCM_CLKEN2_PLL,		KCM_AUTOIDLE2_PLL,		0,			3 }
	};
__ASSERT_COMPILE( (sizeof(KPllMode) / sizeof( KPllMode[0] )) == Prcm::KSupportedPllCount );


// All dividers in the PRCM fall into one of these classes
// Some are unique to a particular peripheral but some
// are used by multiple peripherals so we can share that implementation
enum TDivType
	{
	EDivNotSupported,
	EDiv_1_2,
	EDivCore_1_2_4,
	EDivCore_3_4_6_96M,
	EDivPll_1_To_16,
	EDivPll_1_To_31,
	EDivUsimClk,
	EDivClkOut_1_2_4_8_16,
	};

struct TDividerInfo
	{
	TUint32		iRegister;
	TUint32		iMask;			// mask of bits to modify in register
	TDivType	iDivType : 8;
	TUint8		iShift;			// number of bits to shift to move divide value into position
	};

static const TDividerInfo KDividerInfo[] =
	{
		{ KCM_CLKSEL2_PLL_MPU,		0x1F,							EDivPll_1_To_16,	0 },	// EClkMpu,		///< DPLL1
		{ KCM_CLKSEL2_PLL_IVA2,		0x1F,							EDivPll_1_To_16,	0 },
		{ KCM_CLKSEL1_PLL,			0x1FU << 27,					EDivPll_1_To_31,	27 },	// EClkCore,		///< DPLL3
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkPeriph,		///< DPLL4
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkPeriph2,	///< DPLL5

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkPrcmInterface,

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkEmu,		///< Emulation clock
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkNeon,

		{ KCM_CLKSEL_CORE,			KBit0 | KBit1,					EDiv_1_2,			0 },	// EClkL3Domain,
		{ KCM_CLKSEL_CORE,			KBit2 | KBit3,					EDiv_1_2,			2 },	// EClkL4Domain,

		{ KCM_CLKSEL1_PLL_MPU,		KBit19 | KBit20 | KBit21,		EDivCore_1_2_4,		19 },	// EClkMpuPll_Bypass,	///< DPLL1 bypass frequency
		{ KCM_CLKSEL1_PLL_IVA2,		KBit19 | KBit20 | KBit21,		EDivCore_1_2_4,		19 },	// EClkIva2Pll_Bypass,	///< DPLL2 bypass frequency
		{ KCM_CLKSEL_WKUP,			KBit1 | KBit2,					EDiv_1_2,			1 },	// EClkRM_F,	///< Reset manager functional clock
		{ KCM_CLKSEL3_PLL,			0x1F,							EDivPll_1_To_16,	0 },	// EClk96M		///< 96MHz clock
		{ KCM_CLKSEL5_PLL,			0x1F,							EDivPll_1_To_16,	0 },	// EClk120M		///< 120MHz clock
		{ KCM_CLKOUT_CTRL,			KBit3 | KBit4 | KBit5,			EDivClkOut_1_2_4_8_16,	3 },	// EClkSysOut

		// Functional clocks
		{ KCM_CLKSEL_DSS,			0x1FU << 8,						EDivPll_1_To_16,	8 },	// EClkTv_F,
		{ KCM_CLKSEL_DSS,			0x1F,							EDivPll_1_To_16,	0 },	// EClkDss1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkDss2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkCsi2_F,
		{ KCM_CLKSEL_CAM,			0x1F,							EDivPll_1_To_16,	0 },	// EClkCam_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkIva2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMmc1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMmc2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMmc3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMsPro_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkHdq_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp4_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp5_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi4_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkI2c1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkI2c2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkI2c3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUart1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUart2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUart3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt4_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt5_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt6_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt7_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt8_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt9_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt10_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt11_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUsbTll_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkTs_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkCpeFuse_F,

		{ KCM_CLKSEL_SGX,	KBit0 | KBit1 | KBit2,					EDivCore_3_4_6_96M, 0 },	// EClkSgx_F,

		{ KCM_CLKSEL_WKUP,	KBit3 | KBit4 | KBit5 | KBit6,			EDivUsimClk,		3 },	// EClkUsim_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSmartReflex2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSmartReflex1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkWdt2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkWdt3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio1_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio2_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio3_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio4_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio5_F,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio6_F,

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUsb120_F,		///< USB host 120MHz functional clock
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUsb48_F,		///< USB host 48MHz functional clock


	// Interface clocks
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkDss_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkCam_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkIcr_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMmc1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMmc2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMmc3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMsPro_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkHdq_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkAes1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkAes2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSha11_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSha12_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkDes1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkDes2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp4_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcBsp5_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkI2c1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkI2c2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkI2c3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUart1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUart2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUart3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMcSpi4_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt4_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt5_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt6_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt7_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt8_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt9_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt10_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt11_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpt12_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkMailboxes_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkOmapSCM_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkHsUsbOtg_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSdrc_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkPka_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkRng_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUsbTll_I,

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSgx_I,

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUsim_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkWdt1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkWdt2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkWdt3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio1_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio2_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio3_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio4_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio5_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkGpio6_I,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClk32Sync_I,

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkUsb_I,			///< USB host interface clock

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClk48M
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClk12M

		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSysClk,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkAltClk,
		{ KDummy,					0,								EDivNotSupported,	0 },	// EClkSysClk32k,
	};
__ASSERT_COMPILE( (sizeof(KDividerInfo) / sizeof( KDividerInfo[0] )) == Prcm::KSupportedClockCount );

// Special case divider and mux info for USIM
struct TUsimDivMuxInfo
	{
	Prcm::TClock	iClock : 8;		// source clock
	TUint8			iDivider;		// divider factor
	};
static const TUsimDivMuxInfo UsimDivMuxInfo[16] =
	{
		{ Prcm::EClkSysClk,		1 },	// 0x0
		{ Prcm::EClkSysClk,		1 },	// 0x1
		{ Prcm::EClkSysClk,		2 },	// 0x2
		{ Prcm::EClk96M,		2 },	// 0x3
		{ Prcm::EClk96M,		4 },	// 0x4
		{ Prcm::EClk96M,		8 },	// 0x5
		{ Prcm::EClk96M,		10 },	// 0x6
		{ Prcm::EClk120M,		4 },	// 0x7
		{ Prcm::EClk120M,		8 },	// 0x8
		{ Prcm::EClk120M,		16 },	// 0x9
		{ Prcm::EClk120M,		20 },	// 0xA
		{ Prcm::EClkSysClk,		1 },	// 0xB
		{ Prcm::EClkSysClk,		1 },	// 0xC
		{ Prcm::EClkSysClk,		1 },	// 0xD
		{ Prcm::EClkSysClk,		1 },	// 0xE
		{ Prcm::EClkSysClk,		1 }		// 0xF
	};

// Structure representing a register, mask and enable/disable values
struct TRegisterBitDef
	{
	TUint32	iRegister;
	TUint32	iMask;
	TUint32	iEnablePattern;
	TUint32	iDisablePattern;
	};

// Structure for holding information on clock enable and auto mode
struct TClockEnableAutoInfo
	{
	TRegisterBitDef	iGate;
	TRegisterBitDef	iAuto;
	};

const TUint32 KDummyReadAsDisabled = 1;
const TUint32 KDummyReadAsEnabled = 0;
const TUint32 KBit012	= KBit0 | KBit1 | KBit2;
const TUint32 KBit345	= KBit3 | KBit4 | KBit5;
const TUint32 KBit16_17_18 = KBit16 | KBit17 | KBit18;

// Table of bits to set to enable each clock
// Note where a function doesn't exist, use { KDummy, 0, V, 0 } which will cause a write to harmlessly write
// to __dummypoke and a read to find that the item is disabled if V==KDummyReadAsDisabled and enabled if V=KDummyReadAsEnabled
static const TClockEnableAutoInfo KClockControlTable[] =
	{
		{ { KDummy, 0, 0, 0 },						{ KCM_AUTOIDLE_PLL_MPU, KBit012, 1, 0 } },					// EClkMpu,
		{ { KCM_CLKEN_PLL_IVA2, KBit012, 7, 1 },	{ KCM_AUTOIDLE_PLL_IVA2, KBit0, 1, 0 } },					// EClkIva2Pll,
		{ { KCM_CLKEN_PLL, KBit012, 0x7, 0x5 },						{ KCM_AUTOIDLE_PLL, KBit012, 1, 0 } },		// EClkCore,		///< DPLL3
		{ { KCM_CLKEN_PLL, KBit16_17_18, KBit16_17_18, KBit16 },	{ KCM_AUTOIDLE_PLL, KBit345, KBit3, 0 } },	// EClkPeriph,		///< DPLL4
		{ { KCM_CLKEN2_PLL, KBit012, 0x7, 0x1 },					{ KCM_AUTOIDLE2_PLL, KBit012, 1, 0 } },		// EClkPeriph2,	///< DPLL5

		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkPrcmInterface,
		{ { KDummy, 0, 0, 0 },							{ KCM_CLKSTCTRL_EMU, KBit0 | KBit1, 3, 2 } },		// EClkEmu,		///< Emulation clock
		{ { KCM_IDLEST_NEON, KBit0, 0, 1 },				{ KCM_CLKSTCTRL_NEON, KBit0 | KBit1, 3, 2 } },		// EClkNeon,

		{ { KDummy, 0, 0, 0 },							{ KCM_CLKSTCTRL_CORE, KBit0 | KBit1, KBit0 | KBit1, 0 } },		// EClkL3Domain,
		{ { KDummy, 0, 0, 0 },							{ KCM_CLKSTCTRL_CORE, KBit2 | KBit3, KBit2 | KBit3, 0 } },	// EClkL4Domain,

		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkMpuPll_Bypass,	///< DPLL1 bypass frequency
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkIva2Pll_Bypass,	///< DPLL2 bypass frequency
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkRM_F,			///< Reset manager functional clock
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClk96M,			///< 96MHz clock
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClk120M,			///< 120MHz clock
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkSysOut,

	// Functional clocks
		{ { KCM_FCLKEN_DSS, KBit2, KBit2, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkTv_F,
		{ { KCM_FCLKEN_DSS, KBit0, KBit0, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkDss1_F,
		{ { KCM_FCLKEN_DSS, KBit1, KBit1, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkDss2_F,
		{ { KCM_FCLKEN_CAM, KBit1, KBit1, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkCsi2_F,
		{ { KCM_FCLKEN_CAM, KBit0, KBit0, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkCam_F,
		{ { KCM_FCLKEN_IVA2, KBit0, KBit0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkIva2_F,
		{ { KCM_FCLKEN1_CORE, KBit24, KBit24, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMmc1_F,
		{ { KCM_FCLKEN1_CORE, KBit25, KBit25, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMmc2_F,
		{ { KCM_FCLKEN1_CORE, KBit30, KBit30, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMmc3_F,
		{ { KCM_FCLKEN1_CORE, KBit23, KBit23, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMsPro_F,
		{ { KCM_FCLKEN1_CORE, KBit22, KBit22, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkHdq_F,
		{ { KCM_FCLKEN1_CORE, KBit9, KBit9, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcBSP1_F,
		{ { KCM_FCLKEN_PER, KBit0, KBit0, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcBSP2_F,
		{ { KCM_FCLKEN_PER, KBit1, KBit1, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcBSP3_F,
		{ { KCM_FCLKEN_PER, KBit2, KBit2, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcBSP4_F,
		{ { KCM_FCLKEN1_CORE, KBit10, KBit10, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcBSP5_F,
		{ { KCM_FCLKEN1_CORE, KBit18, KBit18, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcSpi1_F,
		{ { KCM_FCLKEN1_CORE, KBit19, KBit19, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkMcSpi2_F,
		{ { KCM_FCLKEN1_CORE, KBit20, KBit20, 0 },	{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkMcSpi3_F,
		{ { KCM_FCLKEN1_CORE, KBit21, KBit21, 0 },	{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkMcSpi4_F,
		{ { KCM_FCLKEN1_CORE, KBit15, KBit15, 0},	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkI2c1_F,
		{ { KCM_FCLKEN1_CORE, KBit16, KBit16, 0},	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkI2c2_F,
		{ { KCM_FCLKEN1_CORE, KBit17, KBit17, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkI2c3_F,
		{ { KCM_FCLKEN1_CORE, KBit13, KBit13, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUart1_F,
		{ { KCM_FCLKEN1_CORE, KBit14, KBit14, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUart2_F,
		{ { KCM_FCLKEN_PER, KBit11, KBit11, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUart3_F,
		{ { KCM_FCLKEN_WKUP, KBit0, KBit0, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt1_F,
		{ { KCM_FCLKEN_PER, KBit3, KBit3, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt2_F,
		{ { KCM_FCLKEN_PER, KBit4, KBit4, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt3_F,
		{ { KCM_FCLKEN_PER, KBit5, KBit5, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt4_F,
		{ { KCM_FCLKEN_PER, KBit6, KBit6, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt5_F,
		{ { KCM_FCLKEN_PER, KBit7, KBit7, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt6_F,
		{ { KCM_FCLKEN_PER, KBit8, KBit8, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt7_F,
		{ { KCM_FCLKEN_PER, KBit9, KBit9, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt8_F,
		{ { KCM_FCLKEN_PER, KBit10, KBit10, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt9_F,
		{ { KCM_FCLKEN1_CORE, KBit11, KBit11, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt10_F,
		{ { KCM_FCLKEN1_CORE, KBit12, KBit12, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpt11_F,
		{ { KCM_FCLKEN3_CORE, KBit2, KBit2, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUsbTll_F,
		{ { KCM_FCLKEN3_CORE, KBit1, KBit1, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkTs_F,
		{ { KCM_FCLKEN3_CORE, KBit0, KBit0, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkCpeFuse_F,

		{ { KCM_FCLKEN_SGX, KBit1, KBit1, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkSgx_F,

		{ { KCM_FCLKEN_WKUP, KBit9, KBit9, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUsim_F,
		{ { KCM_FCLKEN_WKUP, KBit7, KBit7, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkSmartReflex2_F,
		{ { KCM_FCLKEN_WKUP, KBit6, KBit6, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkSmartReflex1_F,
		{ { KCM_FCLKEN_WKUP, KBit5, KBit5, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkWdt2_F,
		{ { KCM_FCLKEN_PER, KBit12, KBit12, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkWdt3_F,
		{ { KCM_FCLKEN_WKUP, KBit3, KBit3, 0 },		{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpio1_F,
		{ { KCM_FCLKEN_PER, KBit13, KBit13, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpio2_F,
		{ { KCM_FCLKEN_PER, KBit14, KBit14, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpio3_F,
		{ { KCM_FCLKEN_PER, KBit15, KBit15, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpio4_F,
		{ { KCM_FCLKEN_PER, KBit16, KBit16, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpio5_F,
		{ { KCM_FCLKEN_PER, KBit17, KBit17, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkGpio6_F,

		{ { KCM_FCLKEN_USBHOST, KBit1, KBit1, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUsb120_F,
		{ { KCM_FCLKEN_USBHOST, KBit0, KBit0, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },		// EClkUsb48_F,


	// Interface clocks
		{ { KCM_ICLKEN_DSS, KBit0, KBit0, 0 },		{ KCM_AUTOIDLE_DSS, KBit0, KBit0, 0 } },		// EClkDss_I,
		{ { KCM_ICLKEN_CAM, KBit0,KBit0, 0 },		{ KCM_AUTOIDLE_CAM, KBit0, KBit0, 0 } },		// EClkCam_I,
		{ { KCM_ICLKEN1_CORE, KBit29, KBit29, 0 },	{ KCM_AUTOIDLE1_CORE, KBit29, KBit29, 0 } },	// EClkIcr_I,
		{ { KCM_ICLKEN1_CORE, KBit24, KBit24, 0 },	{ KCM_AUTOIDLE1_CORE, KBit24, KBit24, 0 } },	// EClkMmc1_I,
		{ { KCM_ICLKEN1_CORE, KBit25, KBit25, 0 },	{ KCM_AUTOIDLE1_CORE, KBit25, KBit25, 0 } },	// EClkMmc2_I,
		{ { KCM_ICLKEN1_CORE, KBit30, KBit30, 0 },	{ KCM_AUTOIDLE1_CORE, KBit30, KBit30, 0 } },	// EClkMmc3_I,
		{ { KCM_ICLKEN1_CORE, KBit23, KBit23, 0 },	{ KCM_AUTOIDLE1_CORE, KBit23, KBit23, 0 } },	// EClkMsPro_I,
		{ { KCM_ICLKEN1_CORE, KBit22, KBit22, 0 },	{ KCM_AUTOIDLE1_CORE, KBit22, KBit22, 0 } },	// EClkHdq_I,
		{ { KCM_ICLKEN2_CORE, KBit3, KBit3, 0 },	{ KCM_AUTOIDLE2_CORE, KBit3, KBit3, 0 } },		// EClkAes1_I,
		{ { KCM_ICLKEN1_CORE, KBit28, KBit28, 0 },	{ KCM_AUTOIDLE1_CORE, KBit28, KBit28, 0 } },	// EClkAes2_I,
		{ { KCM_ICLKEN2_CORE, KBit1, KBit1, 0 },	{ KCM_AUTOIDLE2_CORE, KBit1, KBit1, 0 } },		// EClkSha11_I,
		{ { KCM_ICLKEN1_CORE, KBit28, KBit27, 0 },	{ KCM_AUTOIDLE1_CORE, KBit27, KBit27, 0 } },	// EClkSha12_I,
		{ { KCM_ICLKEN2_CORE, KBit0, KBit0, 0 },	{ KCM_AUTOIDLE2_CORE, KBit0, KBit0, 0 } },		// EClkDes1_I,
		{ { KCM_ICLKEN1_CORE, KBit26, KBit26, 0 },	{ KCM_AUTOIDLE1_CORE, KBit26, KBit26, 0 } },	// EClkDes2_I,
		{ { KCM_ICLKEN1_CORE, KBit9, KBit9, 0 },	{ KCM_AUTOIDLE1_CORE, KBit9, KBit9, 0 } },		// EClkMcBSP1_I,
		{ { KCM_ICLKEN_PER, KBit0, KBit0, 0},		{ KCM_AUTOIDLE_PER, KBit0, KBit0, 0 } },		// EClkMcBSP2_I,
		{ { KCM_ICLKEN_PER, KBit1, KBit1, 0 },		{ KCM_AUTOIDLE_PER, KBit1, KBit1, 0 } },		// EClkMcBSP3_I,
		{ { KCM_ICLKEN_PER, KBit2, KBit2, 0 },		{ KCM_AUTOIDLE_PER, KBit2, KBit2, 0 } },		// EClkMcBSP4_I,
		{ { KCM_ICLKEN1_CORE, KBit10, KBit10, 0 },	{ KCM_AUTOIDLE1_CORE, KBit10, KBit10, 0 } },	// EClkMcBSP5_I,
		{ { KCM_ICLKEN1_CORE, KBit15, KBit15, 0 },	{ KCM_AUTOIDLE1_CORE, KBit15, KBit15, 0 } },	// EClkI2c1_I,
		{ { KCM_ICLKEN1_CORE, KBit16, KBit16, 0 },	{ KCM_AUTOIDLE1_CORE, KBit16, KBit16, 0 } },	// EClkI2c2_I,
		{ { KCM_ICLKEN1_CORE, KBit17, KBit17, 0 },	{ KCM_AUTOIDLE1_CORE, KBit17, KBit17, 0 } },	// EClkI2c3_I,
		{ { KCM_ICLKEN1_CORE, KBit13, KBit13, 0 },	{ KCM_AUTOIDLE1_CORE, KBit13, KBit13, 0 } },	// EClkUart1_I,
		{ { KCM_ICLKEN1_CORE, KBit14, KBit14, 0 },	{ KCM_AUTOIDLE1_CORE, KBit14, KBit14, 0 } },	// EClkUart2_I,
		{ { KCM_ICLKEN_PER, KBit11, KBit11, 0 },	{ KCM_AUTOIDLE_PER, KBit11, KBit11, 0 } },		// EClkUart3_I,
		{ { KCM_ICLKEN1_CORE, KBit18, KBit18, 0 },	{ KCM_AUTOIDLE1_CORE, KBit18, KBit18, 0 } },	// EClkMcSpi1_I,
		{ { KCM_ICLKEN1_CORE, KBit19, KBit19, 0 },	{ KCM_AUTOIDLE1_CORE, KBit19, KBit19, 0 } },	// EClkMcSpi2_I,
		{ { KCM_ICLKEN1_CORE, KBit20, KBit20, 0 },	{ KCM_AUTOIDLE1_CORE, KBit20, KBit20, 0 } },	// EClkMcSpi3_I,
		{ { KCM_ICLKEN1_CORE, KBit21, KBit21, 0 },	{ KCM_AUTOIDLE1_CORE, KBit21, KBit21, 0 } },	// EClkMcSpi4_I,
		{ { KCM_ICLKEN_WKUP, KBit0, KBit0, 0 },		{ KCM_AUTOIDLE_WKUP, KBit0, KBit0, 0 } },		// EClkGpt1_I,
		{ { KCM_ICLKEN_PER, KBit3, KBit3, 0 },		{ KCM_AUTOIDLE_PER, KBit3, KBit3, 0 } },		// EClkGpt2_I,
		{ { KCM_ICLKEN_PER, KBit4, KBit4, 0 },		{ KCM_AUTOIDLE_PER, KBit4, KBit4, 0 } },		// EClkGpt3_I,
		{ { KCM_ICLKEN_PER, KBit5, KBit5, 0 },		{ KCM_AUTOIDLE_PER, KBit5, KBit5, 0 } },		// EClkGpt4_I,
		{ { KCM_ICLKEN_PER, KBit6, KBit6, 0 },		{ KCM_AUTOIDLE_PER, KBit6, KBit6, 0 } },		// EClkGpt5_I,
		{ { KCM_ICLKEN_PER, KBit7, KBit7, 0 },		{ KCM_AUTOIDLE_PER, KBit7, KBit7, 0 } },		// EClkGpt6_I,
		{ { KCM_ICLKEN_PER, KBit8, KBit8, 0 },		{ KCM_AUTOIDLE_PER, KBit8, KBit8, 0 } },		// EClkGpt7_I,
		{ { KCM_ICLKEN_PER, KBit9, KBit9, 0 },		{ KCM_AUTOIDLE_PER, KBit9, KBit9, 0 } },		// EClkGpt8_I,
		{ { KCM_ICLKEN_PER, KBit10, KBit10, 0 },	{ KCM_AUTOIDLE_PER, KBit10, KBit10, 0 } },		// EClkGpt9_I,
		{ { KCM_ICLKEN1_CORE, KBit11, KBit11, 0 },	{ KCM_AUTOIDLE1_CORE, KBit11, KBit11, 0 } },	// EClkGpt10_I,
		{ { KCM_ICLKEN1_CORE, KBit12, KBit12, 0 },	{ KCM_AUTOIDLE1_CORE, KBit12, KBit12, 0 } },	// EClkGpt11_I,
		{ { KDummy, 0, 0, 0 },						{ KDummy, 0, KDummyReadAsDisabled, 0 } },							// EClkGpt12_I,
		{ { KCM_ICLKEN1_CORE, KBit7, KBit7, 0 },	{ KCM_AUTOIDLE1_CORE, KBit7, KBit7, 0 } },		// EClkMailboxes_I,
		{ { KCM_ICLKEN1_CORE, KBit6, KBit6, 0 },	{ KCM_AUTOIDLE1_CORE, KBit6, KBit6, 0 } },		// EClkOmapSCM_I,
		{ { KCM_ICLKEN1_CORE, KBit4, KBit4, 0 },	{ KCM_AUTOIDLE1_CORE, KBit4, KBit4, 0 } },		// EClkHsUsbOtg_I,
		{ { KCM_ICLKEN1_CORE, KBit1, KBit1, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSdrc_I,
		{ { KCM_ICLKEN2_CORE, KBit4, KBit4, 0 },	{ KCM_AUTOIDLE2_CORE, KBit4, KBit4, 0 } },		// EClkPka_I,
		{ { KCM_ICLKEN2_CORE, KBit2, KBit2, 0 },	{ KCM_AUTOIDLE2_CORE, KBit2, KBit2, 0 } },		// EClkRng_I,
		{ { KCM_ICLKEN3_CORE, KBit2, KBit2, 0 },	{ KCM_AUTOIDLE3_CORE, KBit2, KBit2, 0 } },		// EClkUsbTll_I,

		{ { KCM_ICLKEN_SGX, KBit0, KBit0, 0 },		{ KCM_CLKSTCTRL_SGX, KBit0 | KBit1, 0x3, 0x0 } },	// EClkSgx_I,

		{ { KCM_ICLKEN_WKUP, KBit9, KBit9, 0 },		{ KCM_AUTOIDLE_WKUP, KBit9, KBit9, 0 } },		// EClkUsim_I,
		{ { KCM_ICLKEN_WKUP, KBit4, KBit4, 0 },		{ KCM_AUTOIDLE_WKUP, KBit4, KBit4, 0 } },		// EClkWdt1_I,
		{ { KCM_ICLKEN_WKUP, KBit5, KBit5, 0 },		{ KCM_AUTOIDLE_WKUP, KBit5, KBit5, 0 } },		// EClkWdt2_I,
		{ { KCM_ICLKEN_PER, KBit12, KBit12, 0 },	{ KCM_AUTOIDLE_PER, KBit12, KBit12, 0 } },		// EClkWdt3_I,
		{ { KCM_ICLKEN_WKUP, KBit3, KBit3, 0 },		{ KCM_AUTOIDLE_WKUP, KBit3, KBit3, 0 } },		// EClkGpio1_I,
		{ { KCM_ICLKEN_PER, KBit13, KBit13, 0 },	{ KCM_AUTOIDLE_PER, KBit13, KBit13, 0 } },		// EClkGpio2_I,
		{ { KCM_ICLKEN_PER, KBit14, KBit14, 0 },	{ KCM_AUTOIDLE_PER, KBit14, KBit14, 0 } },		// EClkGpio3_I,
		{ { KCM_ICLKEN_PER, KBit15, KBit15, 0 },	{ KCM_AUTOIDLE_PER, KBit15, KBit15, 0 } },		// EClkGpio4_I,
		{ { KCM_ICLKEN_PER, KBit16, KBit16, 0 },	{ KCM_AUTOIDLE_PER, KBit16, KBit16, 0 } },		// EClkGpio5_I,
		{ { KCM_ICLKEN_PER, KBit17, KBit17, 0 },	{ KCM_AUTOIDLE_PER, KBit17, KBit17, 0 } },		// EClkGpio6_I,
		{ { KCM_ICLKEN_WKUP, KBit2, KBit2, 0 },		{ KCM_AUTOIDLE_WKUP, KBit2, KBit2, 0 } },		// EClk32Sync_I,

		{ { KCM_ICLKEN_USBHOST, KBit0, KBit0, 0 }, { KCM_AUTOIDLE_USBHOST, KBit0, KBit0, 0 } },		// EClkUsb_I,			///< USB host interface clock

		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClk48M
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClk12M

		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkSysClk
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkAltClk
		{ { KDummy, 0, 0, 0 },							{ KDummy, 0, KDummyReadAsEnabled, 0 } },		// EClkSysClk32k
	};
__ASSERT_COMPILE( (sizeof(KClockControlTable) / sizeof( KClockControlTable[0] )) == Prcm::KSupportedClockCount );

static const TRegisterBitDef KClockWakeupTable[] =
	{
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMpu,		///< DPLL1
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkIva2Pll,	///< DPLL2
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkCore,		///< DPLL3
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkPeriph,		///< DPLL4
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkPeriph2,	///< DPLL5

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkPrcmInterface,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkEmu,		///< Emulation clock
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkNeon,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkL3Domain,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkL4Domain,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMpuPll_Bypass,	///< DPLL1 bypass frequency
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkIva2Pll_Bypass,	///< DPLL2 bypass frequency
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkRM_F,			///< Reset manager functional clock
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClk96M,			///< 96MHz clock
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClk120M,			///< 120MHz clock
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSysOut,

	// Functional clocks
	// NOTE - functional clocks aren't mapped to a wakeup event, these just clock the internals
	// Use the interface clocks to register a wakeup
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkTv_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkDss1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkDss2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkCsi2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkCam_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkIva2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMmc1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMmc2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMmc3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMsPro_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkHdq_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcBSP1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcBSP2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcBSP3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcBSP4_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcBSP5_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcSpi1_F
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcSpi2_F
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcSpi3_F
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMcSpi4_F
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkI2c1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkI2c2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkI2c3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUart1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUart2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUart3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt4_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt5_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt6_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt7_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt8_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt9_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt10_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpt11_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUsbTll_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkTs_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkCpeFuse_F,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSgx_F,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUsim_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSmartReflex2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSmartReflex1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkWdt2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkWdt3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpio1_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpio2_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpio3_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpio4_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpio5_F,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkGpio6_F,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUsb120_F,		///< USB host 120MHz functional clock
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkUsb48_F,		///< USB host 48MHz functional clock


	// Interface clocks
		{ KPM_WKEN_DSS, KBit0, KBit0, 0 },	// EClkDss_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkCam_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkIcr_I,
		{ KPM_WKEN1_CORE, KBit24, KBit24, 0 },	// EClkMmc1_I,
		{ KPM_WKEN1_CORE, KBit25, KBit25, 0 },	// EClkMmc2_I,
		{ KPM_WKEN1_CORE, KBit30, KBit30, 0 },	// EClkMmc3_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMsPro_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkHdq_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkAes1_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkAes2_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSha11_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSha12_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkDes1_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkDes2_I,
		{ KPM_WKEN1_CORE, KBit9, KBit9, 0 },	// EClkMcBSP1_I,
		{ KPM_WKEN_PER, KBit0, KBit0, 0 },	// EClkMcBSP2_I,
		{ KPM_WKEN_PER, KBit1, KBit1, 0 },	// EClkMcBSP3_I,
		{ KPM_WKEN_PER, KBit2, KBit2, 0 },	// EClkMcBSP4_I,
		{ KPM_WKEN1_CORE, KBit10, KBit10, 0 },	// EClkMcBSP5_I,
		{ KPM_WKEN1_CORE, KBit15, KBit15, 0 },	// EClkI2c1_I,
		{ KPM_WKEN1_CORE, KBit16, KBit16, 0 },	// EClkI2c2_I,
		{ KPM_WKEN1_CORE, KBit17, KBit17, 0 },	// EClkI2c3_I,
		{ KPM_WKEN1_CORE, KBit13, KBit13, 0 },	// EClkUart1_I,
		{ KPM_WKEN1_CORE, KBit14, KBit14, 0 },	// EClkUart2_I,
		{ KPM_WKEN_PER, KBit11, KBit11, 0 },	// EClkUart3_I,
		{ KPM_WKEN1_CORE, KBit18, KBit18, 0 },	// EClkMcSpi1_I
		{ KPM_WKEN1_CORE, KBit19, KBit19, 0 },	// EClkMcSpi2_I
		{ KPM_WKEN1_CORE, KBit20, KBit20, 0 },	// EClkMcSpi3_I
		{ KPM_WKEN1_CORE, KBit21, KBit21, 0 },	// EClkMcSpi4_I
		{ KPM_WKEN_WKUP, KBit0, KBit0, 0 },	// EClkGpt1_I,
		{ KPM_WKEN_PER, KBit3, KBit3, 0 },	// EClkGpt2_I,
		{ KPM_WKEN_PER, KBit4, KBit4, 0 },	// EClkGpt3_I,
		{ KPM_WKEN_PER, KBit5, KBit5, 0 },	// EClkGpt4_I,
		{ KPM_WKEN_PER, KBit6, KBit6, 0 },	// EClkGpt5_I,
		{ KPM_WKEN_PER, KBit7, KBit7, 0 },	// EClkGpt6_I,
		{ KPM_WKEN_PER, KBit8, KBit8, 0 },	// EClkGpt7_I,
		{ KPM_WKEN_PER, KBit9, KBit9, 0 },	// EClkGpt8_I,
		{ KPM_WKEN_PER, KBit10, KBit10, 0 },	// EClkGpt9_I,
		{ KPM_WKEN1_CORE, KBit11, KBit11, 0 },	// EClkGpt10_I,
		{ KPM_WKEN1_CORE, KBit12, KBit12, 0 },	// EClkGpt11_I,
		{ KPM_WKEN_WKUP, KBit1, KBit1, 0 },	// EClkGpt12_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkMailboxes_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkOmapSCM_I,
		{ KPM_WKEN1_CORE, KBit4, KBit4, 0 },	// EClkHsUsbOtg_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSdrc_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkPka_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkRng_I,
		{ KPM_WKEN3_CORE, KBit2, KBit2, 0 },	// EClkUsbTll_I,

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSgx_I,

		{ KPM_WKEN_WKUP, KBit9, KBit9, 0 },	// EClkUsim_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkWdt1_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkWdt2_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkWdt3_I,
		{ KPM_WKEN_WKUP, KBit3, KBit3, 0 },	// EClkGpio1_I,
		{ KPM_WKEN_PER, KBit13, KBit13, 0 },	// EClkGpio2_I,
		{ KPM_WKEN_PER, KBit14, KBit14, 0 },	// EClkGpio3_I,
		{ KPM_WKEN_PER, KBit15, KBit15, 0 },	// EClkGpio4_I,
		{ KPM_WKEN_PER, KBit16, KBit16, 0 },	// EClkGpio5_I,
		{ KPM_WKEN_PER, KBit17, KBit17, 0 },	// EClkGpio6_I,
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClk32Sync_I,

		{ KPM_WKEN_USBHOST, KBit0, KBit0, 0 },	// EClkUsb_I,			///< USB host interface clock

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClk48M
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClk12M

		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkSysClk
		{ KDummy, 0, KDummyReadAsDisabled, 0 },	// EClkAltClk
		{ KDummy, 0, KDummyReadAsDisabled, 0 }	// EClkSysClk32k

	};
__ASSERT_COMPILE( (sizeof(KClockWakeupTable) / sizeof( KClockWakeupTable[0] )) == Prcm::KSupportedClockCount );


__ASSERT_COMPILE( Prcm::EWakeGroupMpu == 0 );
__ASSERT_COMPILE( Prcm::EWakeGroupIva2 == 1 );
static const TRegisterBitDef KClockWakeupGroupTable[ Prcm::KSupportedClockCount ][ Prcm::KSupportedWakeupGroupCount ] =
	{
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMpu,		///< DPLL1
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkIva2Pll,	///< DPLL2
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkCore,		///< DPLL3
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkPeriph,		///< DPLL4
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkPeriph2,	///< DPLL5

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkPrcmInterface,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkEmu,		///< Emulation clock
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkNeon,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkL3Domain,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkL4Domain,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMpuPll_Bypass,	///< DPLL1 bypass frequency
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkIva2Pll_Bypass,	///< DPLL2 bypass frequency
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkRM_F,			///< Reset manager functional clock
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClk96M,			///< 96MHz clock
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClk120M,			///< 120MHz clock
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSysOut,

	// Functional clocks
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkTv_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkDss1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkDss2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkCsi2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkCam_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkIva2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMmc1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMmc2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMmc3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMsPro_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkHdq_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcBsp1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcBsp2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcBsp3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcBsp4_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcBsp5_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcSpi1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcSpi2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcSpi3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMcSpi4_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkI2c1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkI2c2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkI2c3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUart1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUart2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUart3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt4_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt5_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt6_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt7_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt8_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt9_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt10_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpt11_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUsbTll_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkTs_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkCpeFuse_F,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSgx_F,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUsim_F,
		{ { KPM_MPUGRPSEL_WKUP, KBit7, KBit7, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSmartReflex2_F,
		{ { KPM_MPUGRPSEL_WKUP, KBit6, KBit6, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSmartReflex1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkWdt2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkWdt3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpio1_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpio2_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpio3_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpio4_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpio5_F,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkGpio6_F,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUsb120_F,		///< USB host 120MHz functional clock
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkUsb48_F,		///< USB host 48MHz functional clock


	// Interface clocks
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkDss_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkCam_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkIcr_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit24, KBit24, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit24, KBit24, 0 } },			// EClkMmc1_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit25, KBit25, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit25, KBit25, 0 } },			// EClkMmc2_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit30, KBit30, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit30, KBit30, 0 } },			// EClkMmc3_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMsPro_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkHdq_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkAes1_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkAes2_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSha11_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSha12_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkDes1_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkDes2_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit9, KBit9, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit9, KBit9, 0 } },			// EClkMcBsp1_I,
		{ { KPM_MPUGRPSEL_PER, KBit0, KBit0, 0 },	{ KPM_IVA2GRPSEL_PER, KBit0, KBit0, 0 } },			// EClkMcBsp2_I,
		{ { KPM_MPUGRPSEL_PER, KBit1, KBit1, 0 },	{ KPM_IVA2GRPSEL_PER, KBit1, KBit1, 0 } },			// EClkMcBsp3_I,
		{ { KPM_MPUGRPSEL_PER, KBit2, KBit2, 0 },	{ KPM_IVA2GRPSEL_PER, KBit2, KBit2, 0 } },			// EClkMcBsp4_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit10, KBit10, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit10, KBit10, 0 } },			// EClkMcBsp5_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit15, KBit15, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit15, KBit15, 0 } },			// EClkI2c1_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit16, KBit16, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit16, KBit16, 0 } },			// EClkI2c2_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit17, KBit17, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit17, KBit17, 0 } },			// EClkI2c3_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit13, KBit13, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit13, KBit13, 0 } },			// EClkUart1_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit14, KBit14, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit14, KBit14, 0 } },			// EClkUart2_I,
		{ { KPM_MPUGRPSEL_PER, KBit11, KBit11, 0 },	{ KPM_IVA2GRPSEL_PER, KBit11, KBit11, 0 } },			// EClkUart3_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit18, KBit18, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit18, KBit18, 0 } },			// EClkMcSpi1_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit19, KBit19, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit19, KBit19, 0 } },			// EClkMcSpi2_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit20, KBit20, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit20, KBit20, 0 } },			// EClkMcSpi3_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit21, KBit21, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit21, KBit21, 0 } },			// EClkMcSpi4_I,
		{ { KPM_MPUGRPSEL_WKUP, KBit0, KBit0, 0 },	{ KPM_IVA2GRPSEL_WKUP, KBit0, KBit0, 0 } },			// EClkGpt1_I,
		{ { KPM_MPUGRPSEL_PER, KBit3, KBit3, 0 },	{ KPM_IVA2GRPSEL_PER, KBit3, KBit3, 0 } },			// EClkGpt2_I,
		{ { KPM_MPUGRPSEL_PER, KBit4, KBit4, 0 },	{ KPM_IVA2GRPSEL_PER, KBit4, KBit4, 0 } },			// EClkGpt3_I,
		{ { KPM_MPUGRPSEL_PER, KBit5, KBit5, 0 },	{ KPM_IVA2GRPSEL_PER, KBit5, KBit5, 0 } },			// EClkGpt4_I,
		{ { KPM_MPUGRPSEL_PER, KBit6, KBit6, 0 },	{ KPM_IVA2GRPSEL_PER, KBit6, KBit6, 0 } },			// EClkGpt5_I,
		{ { KPM_MPUGRPSEL_PER, KBit7, KBit7, 0 },	{ KPM_IVA2GRPSEL_PER, KBit7, KBit7, 0 } },			// EClkGpt6_I,
		{ { KPM_MPUGRPSEL_PER, KBit8, KBit9, 0 },	{ KPM_IVA2GRPSEL_PER, KBit8, KBit8, 0 } },			// EClkGpt7_I,
		{ { KPM_MPUGRPSEL_PER, KBit9, KBit9, 0 },	{ KPM_IVA2GRPSEL_PER, KBit9, KBit9, 0 } },			// EClkGpt8_I,
		{ { KPM_MPUGRPSEL_PER, KBit10, KBit10, 0 },	{ KPM_IVA2GRPSEL_PER, KBit10, KBit10, 0 } },			// EClkGpt9_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit11, KBit11, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit11, KBit11, 0 } },			// EClkGpt10_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit12, KBit12, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit12, KBit12, 0 } },			// EClkGpt11_I,
		{ { KPM_MPUGRPSEL_WKUP, KBit1, KBit1, 0 },	{ KPM_IVA2GRPSEL_WKUP, KBit1, KBit1, 0 } },			// EClkGpt12_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkMailboxes_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkOmapSCM_I,
		{ { KPM_MPUGRPSEL1_CORE, KBit4, KBit4, 0 },	{ KPM_IVA2GRPSEL1_CORE, KBit4, KBit4, 0 } },			// EClkHsUsbOtg_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSdrc_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkPka_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkRng_I,
		{ { KPM_MPUGRPSEL3_CORE, KBit2, KBit2, 0 },	{ KPM_IVA2GRPSEL3_CORE, KBit2, KBit2, 0 } },			// EClkUsbTll_I,

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSgx_I,

		{ { KPM_MPUGRPSEL_WKUP, KBit9, KBit9, 0 },	{ KPM_IVA2GRPSEL_WKUP, KBit9, KBit9, 0 } },			// EClkUsim_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkWdt1_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkWdt2_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkWdt3_I,
		{ { KPM_MPUGRPSEL_WKUP, KBit3, KBit3, 0 },	{ KPM_IVA2GRPSEL_WKUP, KBit3, KBit3, 0 } },			// EClkGpio1_I,
		{ { KPM_MPUGRPSEL_PER, KBit13, KBit13, 0 },	{ KPM_IVA2GRPSEL_PER, KBit13, KBit13, 0 } },			// EClkGpio2_I,
		{ { KPM_MPUGRPSEL_PER, KBit14, KBit14, 0 },	{ KPM_IVA2GRPSEL_PER, KBit14, KBit14, 0 } },			// EClkGpio3_I,
		{ { KPM_MPUGRPSEL_PER, KBit15, KBit15, 0 },	{ KPM_IVA2GRPSEL_PER, KBit15, KBit15, 0 } },			// EClkGpio4_I,
		{ { KPM_MPUGRPSEL_PER, KBit16, KBit16, 0 },	{ KPM_IVA2GRPSEL_PER, KBit16, KBit16, 0 } },			// EClkGpio5_I,
		{ { KPM_MPUGRPSEL_PER, KBit17, KBit17, 0 },	{ KPM_IVA2GRPSEL_PER, KBit17, KBit17, 0 } },			// EClkGpio6_I,
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClk32Sync_I,

		{ { KPM_MPUGRPSEL_USBHOST, KBit0, KBit0, 0 },	{ KPM_IVA2GRPSEL_USBHOST, KBit0, KBit0, 0 } },			// EClkUsb_I,			///< USB host interface clock

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClk48M
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClk12M

		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkSysClk
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } },			// EClkAltClk
		{ { KDummy, 0, KDummyReadAsDisabled, 0 },	{ KDummy, 0, KDummyReadAsDisabled, 0 } }			// EClkSysClk32k
	};

	__ASSERT_COMPILE( Prcm::EWakeDomainMpu == 0 );
	__ASSERT_COMPILE( Prcm::EWakeDomainCore == 1 );
	__ASSERT_COMPILE( Prcm::EWakeDomainIva2 == 2 );
	__ASSERT_COMPILE( Prcm::EWakeDomainPeripheral == 3 );
	__ASSERT_COMPILE( Prcm::EWakeDomainDss == 4 );
	__ASSERT_COMPILE( Prcm::EWakeDomainWakeup == 5 );
	__ASSERT_COMPILE( Prcm::KSupportedWakeupDomainCount == 6 );

struct TWakeupDomainInfo
	{
	// To save space, there's an assumption here that all domain dependency configuration for
	// a single clock is in one register, and a single bit defines the dependency,
	// 1 = dependant, 0 = independant
	// The bits are defined here by bit number rather than by mask
	TUint32		iRegister;
	TInt8		iBitNumber[ Prcm::KSupportedWakeupDomainCount ];	///< bit number to modify, -1 if not supported
	};

static const TWakeupDomainInfo KClockWakeupDomainTable[ Prcm::KSupportedClockCount ] =
	{
		// REGISTER			MPU		CORE	IVA2	PER		DSS		WAKE
		{ KPM_WKDEP_MPU,	{-1,		0,		2,		7,		5,		-1 } },		// EClkMpu,		///< DPLL1
		{ KPM_WKDEP_IVA2,	{1,			0,		-1,		7,		5,		4 } },		// EClkIva2Pll,	///< DPLL2
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkCore,		///< DPLL3
		{ KPM_WKDEP_PER,	{1,			0,		2,		-1,		-1,		4 } },		// EClkPeriph,		///< DPLL4
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkPeriph2,	///< DPLL5

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkPrcmInterface,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkEmu,		///< Emulation clock
		{ KPM_WKDEP_NEON,	{1,			-1,		-1,		-1,		-1,		-1 } },		// EClkNeon,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkL3Domain,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkL4Domain,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMpuPll_Bypass,	///< DPLL1 bypass frequency
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkIva2Pll_Bypass,	///< DPLL2 bypass frequency
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkRM_F,			///< Reset manager functional clock
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClk96M,			///< 96MHz clock
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClk120M,			///< 120MHz clock
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSysOut,

	// Functional clocks
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkTv_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkDss1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkDss2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkCsi2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkCam_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkIva2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMmc1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMmc2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMmc3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMsPro_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkHdq_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp4_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp5_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi4_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkI2c1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkI2c2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkI2c3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUart1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUart2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUart3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt4_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt5_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt6_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt7_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt8_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt9_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt10_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt11_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUsbTll_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkTs_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkCpeFuse_F,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSgx_F,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUsim_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSmartReflex2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSmartReflex1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkWdt2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkWdt3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio1_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio2_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio3_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio4_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio5_F,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio6_F,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUsb120_F,		///< USB host 120MHz functional clock
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUsb48_F,		///< USB host 48MHz functional clock


	// Interface clocks
		{ KPM_WKDEP_DSS,	{1,		-1,		2,		-1,		-1,		4 } },		// EClkDss_I,
		{ KPM_WKDEP_CAM,	{1,		-1,		2,		-1,		-1,		4 } },		// EClkCam_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkIcr_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMmc1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMmc2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMmc3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMsPro_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkHdq_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkAes1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkAes2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSha11_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSha12_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkDes1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkDes2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp4_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcBsp5_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkI2c1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkI2c2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkI2c3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUart1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUart2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUart3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMcSpi4_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt4_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt5_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt6_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt7_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt8_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt9_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt10_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt11_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpt12_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkMailboxes_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkOmapSCM_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkHsUsbOtg_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSdrc_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkPka_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkRng_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUsbTll_I,

		{ KPM_WKDEP_SGX,	{1,		-1,		2,		-1,		-1,		4 } },		// EClkSgx_I,

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkUsim_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkWdt1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkWdt2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkWdt3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio1_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio2_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio3_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio4_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio5_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkGpio6_I,
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClk32Sync_I,

		{ KPM_WKDEP_USBHOST,	{1,	0,		2,		-1,		-1,		4	} },		// EClkUsb_I,			///< USB host interface clock

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClk48M
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClk12M

		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSysClk
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkAltClk
		{ KDummy,		{-1,		-1,		-1,		-1,		-1,		-1 } },		// EClkSysClk32k
		// REGISTER			MPU		CORE	IVA2	PER		DSS		WAKE
	};

struct TPowerDomainControl
	{
	TUint32		iRegister;
	TUint8		iShift;			///< shift to move bits into position
	TUint8		iAllowedMask;	///< mask of which modes are supported
	TUint8		__spare[2];
	};

const TUint8	KPowerAllowedOff		= 1 << Prcm::EPowerOff;
const TUint8	KPowerAllowedOn			= 1 << Prcm::EPowerOn;
const TUint8	KPowerAllowedRetention	= 1 << Prcm::EPowerRetention;
const TUint8	KPowerAllowedOnOffRetention	=	(KPowerAllowedOff bitor KPowerAllowedOn bitor KPowerAllowedRetention);
const TUint8	KPowerModeMask			= 0x3;

static const TPowerDomainControl KPowerDomainControl[] =
	{
		// iRegister			iShift	iAllowedMask
		{ KPM_PWSTCTRL_MPU,		0,		KPowerAllowedOnOffRetention	},	// EPowerDomainMpu,
		{ KPM_PWSTCTRL_IVA2,	0,		KPowerAllowedOnOffRetention	},	// EPowerDomainIva2,
		{ KPM_PWSTCTRL_NEON,	0,		KPowerAllowedOnOffRetention	},	// EPowerDomainNeon,
		{ KPM_PWSTCTRL_CORE,	0,		KPowerAllowedOnOffRetention },	// EPowerDomainCore,
		{ KPM_PWSTCTRL_SGX,		0,		KPowerAllowedOnOffRetention },	// EPowerDomainSgx,
		{ KPM_PWSTCTRL_DSS,		0,		KPowerAllowedOnOffRetention	},	// EPowerDomainDss,
		{ KPM_PWSTCTRL_CAM,		0,		KPowerAllowedOnOffRetention	},	// EPowerDomainCamera,
		{ KPM_PWSTCTRL_USBHOST,	0,		KPowerAllowedOnOffRetention	},	// EPowerDomainUsb,
		{ KPM_PWSTCTRL_PER,		0,		KPowerAllowedOnOffRetention	}	// EPowerDomainPer,
	};
__ASSERT_COMPILE( (sizeof(KPowerDomainControl) / sizeof( KPowerDomainControl[0] )) == Prcm::KSupportedPowerDomainCount );

struct TGptClkSelInfo
	{
	TUint32	iRegister;
	TUint32	iMask;
	};

static const TGptClkSelInfo KGptClockSourceInfo[ Prcm::KSupportedGptCount ] =
	{
		{ KCM_CLKSEL_WKUP, KBit0 },	//	EGpt1,
		{ KCM_CLKSEL_PER, KBit0 },	//	EGpt2,
		{ KCM_CLKSEL_PER, KBit1 },	//	EGpt3,
		{ KCM_CLKSEL_PER, KBit2 },	//	EGpt4,
		{ KCM_CLKSEL_PER, KBit3 },	//	EGpt5,
		{ KCM_CLKSEL_PER, KBit4 },	//	EGpt6,
		{ KCM_CLKSEL_PER, KBit5 },	//	EGpt7,
		{ KCM_CLKSEL_PER, KBit6 },	//	EGpt8,
		{ KCM_CLKSEL_PER, KBit7 },	//	EGpt9,
		{ KCM_CLKSEL_CORE, KBit6 },	//	EGpt10,
		{ KCM_CLKSEL_CORE, KBit7 },	//	EGpt11,
		{ KDummy, 0 },			//	EGpt12	- clocked from security block
	};

// This table is used to find the source clock for a given clock. That is, by looking up a
// specific clock in this table, you can find out which DPLL/divider it was derived from.
// Following the chain backwards to SYSCLK allows building of the total multiply and
// divide applied to SYSCLK to get the given clock
enum TClockSourceType
	{
	EIgnore,	// not implemented yet...
	EDpll,		// this clock is derived from a PLL
	EDivider,	// this clock is divied from a given clock
	EDivMux,	// divider fed by mux-selectable input clock
	EMux,		// fed by mux-selectable input clock
	EDuplicate,	// this clock is a duplicate of another clock
	E96MMux,	// 96MHz mux-selected clock source
	E54MMux,	// 54MHz mux-selected clock source
	E48MMux,	// 48MHz mux-selected clock source
	EDiv4,		// specified clock source divided by 4
	};

struct TClockSourceInfo
	{
	TClockSourceType	iType : 8;	// type of the source for this clock
	union	{
		Prcm::TClock	iClock : 8;		// the clock that feeds this divider, or which this is a duplicate of
		Prcm::TPll		iPll : 8;		// the PLL that generates this clock
		Prcm::TGpt		iGpt : 8;		// conversion to TGpt type for the clock we are interested in
		};
	};

static const TClockSourceInfo KClockSourceInfo[] =
	{
		{ EDpll,		(Prcm::TClock)Prcm::EDpll1 },			// EClkMpu,
		{ EDpll,		(Prcm::TClock)Prcm::EDpll2 },			// EClkIva2Pll,
		{ EDpll,		(Prcm::TClock)Prcm::EDpll3 },			// EClkCore,
		{ EDpll,		(Prcm::TClock)Prcm::EDpll4 },			// EClkPeriph,
		{ EDpll,		(Prcm::TClock)Prcm::EDpll5 },			// EClkPeriph2,
		{ EDuplicate,	Prcm::EClkSysClk },		// EClkPrcmInterface,
		{ EIgnore,		(Prcm::TClock)0 },		// EClkEmu,
		{ EDuplicate,	Prcm::EClkMpu },		// EClkNeon,
		{ EDivider,		Prcm::EClkCore },		// EClkL3Domain,
		{ EDivider,		Prcm::EClkL3Domain },	// EClkL4Domain,
		{ EDivider,		Prcm::EClkCore },		// EClkMpuPll_Bypass,
		{ EDivider,		Prcm::EClkCore },		// EClkIva2Pll_Bypass,
		{ EDivider,		Prcm::EClkL4Domain },	// EClkRM_F,
		{ E96MMux,		Prcm::EClkPeriph },		// EClk96M,
		{ EDivider,		Prcm::EClkPeriph2 },	// EClk120M,
		{ EDivMux,		(Prcm::TClock)0 },		// EClkSysOut,

	// Functional clocks
		{ E54MMux,		Prcm::EClkPeriph },
		{ EDivider,		Prcm::EClkPeriph },		// EClkDss1_F,
		{ EDuplicate,	Prcm::EClkSysClk },		// EClkDss2_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkCsi2_F,
		{ EDivider,		Prcm::EClkPeriph },		// EClkCam_F,
		{ EDuplicate,	Prcm::EClkIva2Pll },	// EClkIva2_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMmc1_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMmc2_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMmc3_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMsPro_F,
		{ EDuplicate,	Prcm::EClk12M },		// EClkHdq_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMcBsp1_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMcBsp2_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMcBsp3_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMcBsp4_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkMcBsp5_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkMcSpi1_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkMcSpi2_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkMcSpi3_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkMcSpi4_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkI2c1_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkI2c2_F,
		{ EDuplicate,	Prcm::EClk96M },		// EClkI2c3_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkUart1_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkUart2_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkUart3_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt1 },			// EClkGpt1_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt2 },			// EClkGpt2_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt3 },			// EClkGpt3_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt4 },			// EClkGpt4_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt5 },			// EClkGpt5_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt6 },			// EClkGpt6_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt7 },			// EClkGpt7_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt8 },			// EClkGpt8_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt9 },			// EClkGpt9_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt10 },			// EClkGpt10_F,
		{ EMux,			(Prcm::TClock)Prcm::EGpt11 },			// EClkGpt11_F,
		{ EDuplicate,	Prcm::EClk120M },		// EClkUsbTll_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },	// EClkTs_F,
		{ EDuplicate,	Prcm::EClkSysClk },		// EClkCpeFuse_F,
		{ EDivMux,		(Prcm::TClock)0 },					// EClkSgx_F,
		{ EDivMux,		Prcm::EClkSysClk },		// EClkUsim_F,
		{ EDuplicate,	Prcm::EClkSysClk },		// EClkSmartReflex2_F,
		{ EDuplicate,	Prcm::EClkSysClk },		// EClkSmartReflex1_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkWdt2_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkWdt3_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkGpio1_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkGpio2_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkGpio3_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkGpio4_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkGpio5_F,
		{ EDuplicate,	Prcm::EClkSysClk32k },					// EClkGpio6_F,
		{ EDuplicate,	Prcm::EClk120M },		// EClkUsb120_F,
		{ EDuplicate,	Prcm::EClk48M },		// EClkUsb48_F,

	// Interface clocks
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkDss_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkCam_I,
		{ },					// EClkIcr_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMmc1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMmc2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMmc3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMsPro_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkHdq_I,
		{ EDuplicate,	Prcm::EClkL4Domain},		// EClkAes1_I,
		{ EDuplicate,	Prcm::EClkL4Domain},		// EClkAes2_I,
		{ EDuplicate,	Prcm::EClkL4Domain},		// EClkSha11_I,
		{ EDuplicate,	Prcm::EClkL4Domain},		// EClkSha12_I,
		{ EDuplicate,	Prcm::EClkL4Domain},		// EClkDes1_I,
		{ EDuplicate,	Prcm::EClkL4Domain},		// EClkDes2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcBsp1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcBsp2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcBsp3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcBsp4_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcBsp5_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkI2c1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkI2c2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkI2c3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkUart1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkUart2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkUart3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcSpi1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcSpi2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcSpi3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMcSpi4_I,
		{ EDuplicate,	Prcm::EClkSysClk },			// EClkGpt1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt4_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt5_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt6_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt7_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt8_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt9_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt10_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt11_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpt12_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkMailboxes_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkOmapSCM_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkHsUsbOtg_I,
		{ EDuplicate,	Prcm::EClkL3Domain },		// EClkSdrc_I,
		{ EDuplicate,	Prcm::EClkL3Domain },		// EClkPka_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkRng_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkUsbTll_I,
		{ EDuplicate,	Prcm::EClkL3Domain },		// EClkSgx_I,
		{ EDuplicate,	Prcm::EClkSysClk },			// EClkUsim_I,
		{ EDuplicate,	Prcm::EClkSysClk },			// EClkWdt1_I,
		{ EDuplicate,	Prcm::EClkSysClk },			// EClkWdt2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkWdt3_I,
		{ EDuplicate,	Prcm::EClkSysClk },			// EClkGpio1_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpio2_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpio3_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpio4_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpio5_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkGpio6_I,
		{ EDuplicate,	Prcm::EClkSysClk },			// EClk32Sync_I,
		{ EDuplicate,	Prcm::EClkL4Domain },		// EClkUsb_I,

		{ E48MMux,		Prcm::EClk96M },		// EClk48M,
		{ EDiv4,		Prcm::EClk48M },		// EClk12M,

		{ EDuplicate,	Prcm::EClkSysClk },		// EClkSysClk
		{ EDuplicate,	Prcm::EClkAltClk },		// EClkAltClk
		{ EDuplicate,	Prcm::EClkSysClk32k },	// EClkSysClk32k

	};

__ASSERT_COMPILE( sizeof( KClockSourceInfo ) / sizeof( KClockSourceInfo[0] ) == Prcm::KSupportedClockCount );


// Bit of hackery to enable creation of a const table of pointer to _LITs.
// Taking the address of a _LIT will cause the compiler to invoke its operator&()
// function, which forces the compiler to generate the table in code. But hiding
// it inside a dummy struct allows taking of the address of the struct instead,
// avoiding the operator&() problem.

template< TInt S >
struct THiddenLit8
	{
	TLitC8<S>	iLit;
	};

#define __PLIT8(name,s) const static THiddenLit8<sizeof(s)> name={{sizeof(s)-1,s}};

// List of identifer strings for each clock source - used for PRM
__PLIT8(KClkMpu,			"a.MPU" );
__PLIT8(KClkIva2Pll,		"a.IVA" );
__PLIT8(KClkCore,			"a.CORE" );
__PLIT8(KClkPeriph,			"a.PER" );
__PLIT8(KClkPeriph2,		"a.PER2" );
__PLIT8(KClkPrcmInterface,	"a.PRCM" );
__PLIT8(KClkEmu,			"a.EMU" );
__PLIT8(KClkNeon,			"a.NEON" );
__PLIT8(KClkL3Domain,		"a.L3" );
__PLIT8(KClkL4Domain,		"a.L4" );
__PLIT8(KClkMpuPll_Bypass,	"a.MPUB" );
__PLIT8(KClkIva2Pll_Bypass,	"a.IVAB" );
__PLIT8(KClkRM_F,			"a.RMf" );
__PLIT8(KClk96M,			"a.96" );
__PLIT8(KClk120M,			"a.120" );
__PLIT8(KClkSysOut,			"a.OUT" );
__PLIT8(KClkTv_F,			"a.TVf" );
__PLIT8(KClkDss1_F,			"a.DSS1f" );
__PLIT8(KClkDss2_F,			"a.DSS2f" );
__PLIT8(KClkCsi2_F,			"a.CSI2f" );
__PLIT8(KClkCam_F,			"a.CAMf" );
__PLIT8(KClkIva2_F,			"a.IVA2f" );
__PLIT8(KClkMmc1_F,			"a.MMC1f" );
__PLIT8(KClkMmc2_F,			"a.MMC2f" );
__PLIT8(KClkMmc3_F,			"a.MMC3f" );
__PLIT8(KClkMsPro_F,		"a.MSPf" );
__PLIT8(KClkHdq_F,			"a.HDQf" );
__PLIT8(KClkMcBsp1_F,		"a.BSP1f" );
__PLIT8(KClkMcBsp2_F,		"a.BSP2f" );
__PLIT8(KClkMcBsp3_F,		"a.BSP3f" );
__PLIT8(KClkMcBsp4_F,		"a.BSP4f" );
__PLIT8(KClkMcBsp5_F,		"a.BSP5f" );
__PLIT8(KClkMcSpi1_F,		"a.SPI1f" );
__PLIT8(KClkMcSpi2_F,		"a.SPI2f" );
__PLIT8(KClkMcSpi3_F,		"a.SPI3f" );
__PLIT8(KClkMcSpi4_F,		"a.SPI4f" );
__PLIT8(KClkI2c1_F,			"a.I2C1f" );
__PLIT8(KClkI2c2_F,			"a.I2C2f" );
__PLIT8(KClkI2c3_F,			"a.I2C3f" );
__PLIT8(KClkUart1_F,		"a.UART1f" );
__PLIT8(KClkUart2_F,		"a.UART2f" );
__PLIT8(KClkUart3_F,		"a.UART3f" );
__PLIT8(KClkGpt1_F,			"a.GPT1f" );
__PLIT8(KClkGpt2_F,			"a.GPT2f" );
__PLIT8(KClkGpt3_F,			"a.GPT3f" );
__PLIT8(KClkGpt4_F,			"a.GPT4f" );
__PLIT8(KClkGpt5_F,			"a.GPT5f" );
__PLIT8(KClkGpt6_F,			"a.GPT6f" );
__PLIT8(KClkGpt7_F,			"a.GPT7f" );
__PLIT8(KClkGpt8_F,			"a.GPT8f" );
__PLIT8(KClkGpt9_F,			"a.GPT9f" );
__PLIT8(KClkGpt10_F,		"a.GPTAf" );
__PLIT8(KClkGpt11_F,		"a.GPTBf" );
__PLIT8(KClkUsbTll_F,		"a.UTLLf" );
__PLIT8(KClkTs_F,			"a.TSf" );
__PLIT8(KClkCpeFuse_F,		"a.FUSEf" );
__PLIT8(KClkSgx_F,			"a.SGXf" );
__PLIT8(KClkUsim_F,			"a.USIMf" );
__PLIT8(KClkSmartReflex2_F,	"a.SMRF2f" );
__PLIT8(KClkSmartReflex1_F,	"a.SMRF1f" );
__PLIT8(KClkWdt2_F,			"a.WDT2f" );
__PLIT8(KClkWdt3_F,			"a.WDT3f" );
__PLIT8(KClkGpio1_F,		"a.GPIO1f" );
__PLIT8(KClkGpio2_F,		"a.GPIO2f" );
__PLIT8(KClkGpio3_F,		"a.GPIO3f" );
__PLIT8(KClkGpio4_F,		"a.GPIO4f" );
__PLIT8(KClkGpio5_F,		"a.GPIO5f" );
__PLIT8(KClkGpio6_F,		"a.GPIO6f" );
__PLIT8(KClkUsb120_F,		"a.U120f" );
__PLIT8(KClkUsb48_F,		"a.U48f" );
__PLIT8(KClkDss_I,			"a.DSSi" );
__PLIT8(KClkCam_I,			"a.CAMi" );
__PLIT8(KClkIcr_I,			"a.ICRi" );
__PLIT8(KClkMmc1_I,			"a.MMC1i" );
__PLIT8(KClkMmc2_I,			"a.MMC2i" );
__PLIT8(KClkMmc3_I,			"a.MMC3i" );
__PLIT8(KClkMsPro_I,		"a.MSi" );
__PLIT8(KClkHdq_I,			"a.HDQi" );
__PLIT8(KClkAes1_I,			"a.AES1i" );
__PLIT8(KClkAes2_I,			"a.AES2i" );
__PLIT8(KClkSha11_I,		"a.SHA1i" );
__PLIT8(KClkSha12_I,		"a.SHA2i" );
__PLIT8(KClkDes1_I,			"a.DES1i" );
__PLIT8(KClkDes2_I,			"a.DES2i" );
__PLIT8(KClkMcBsp1_I,		"a.BSP1i" );
__PLIT8(KClkMcBsp2_I,		"a.BSP2i" );
__PLIT8(KClkMcBsp3_I,		"a.BSP3i" );
__PLIT8(KClkMcBsp4_I,		"a.BSP4i" );
__PLIT8(KClkMcBsp5_I,		"a.BSP5i" );
__PLIT8(KClkI2c1_I,			"a.I2C1i" );
__PLIT8(KClkI2c2_I,			"a.I2C2i" );
__PLIT8(KClkI2c3_I,			"a.I2C3i" );
__PLIT8(KClkUart1_I,		"a.UART1i" );
__PLIT8(KClkUart2_I,		"a.UART2i" );
__PLIT8(KClkUart3_I,		"a.UART3i" );
__PLIT8(KClkMcSpi1_I,		"a.SPI1i" );
__PLIT8(KClkMcSpi2_I,		"a.SPI2i" );
__PLIT8(KClkMcSpi3_I,		"a.SPI3i" );
__PLIT8(KClkMcSpi4_I,		"a.SPI4i" );
__PLIT8(KClkGpt1_I,			"a.GPT1i" );
__PLIT8(KClkGpt2_I,			"a.GPT2i" );
__PLIT8(KClkGpt3_I,			"a.GPT3i" );
__PLIT8(KClkGpt4_I,			"a.GPT4i" );
__PLIT8(KClkGpt5_I,			"a.GPT5i" );
__PLIT8(KClkGpt6_I,			"a.GPT6i" );
__PLIT8(KClkGpt7_I,			"a.GPT7i" );
__PLIT8(KClkGpt8_I,			"a.GPT8i" );
__PLIT8(KClkGpt9_I,			"a.GPT9i" );
__PLIT8(KClkGpt10_I,		"a.GPTAi" );
__PLIT8(KClkGpt11_I,		"a.GPTBi" );
__PLIT8(KClkGpt12_I,		"a.GPTCi" );
__PLIT8(KClkMailboxes_I,	"a.MBi" );
__PLIT8(KClkOmapSCM_I,		"a.SCMi" );
__PLIT8(KClkHsUsbOtg_I,		"a.OTGi" );
__PLIT8(KClkSdrc_I,			"a.SDRCi" );
__PLIT8(KClkPka_I,			"a.PKAi" );
__PLIT8(KClkRng_I,			"a.RNGi" );
__PLIT8(KClkUsbTll_I,		"a.TLLi" );
__PLIT8(KClkSgx_I,			"a.SGXi" );
__PLIT8(KClkUsim_I,			"a.USIMi" );
__PLIT8(KClkWdt1_I,			"a.WDT1i" );
__PLIT8(KClkWdt2_I,			"a.WDT2i" );
__PLIT8(KClkWdt3_I,			"a.WDT3i" );
__PLIT8(KClkGpio1_I,		"a.GPIO1i" );
__PLIT8(KClkGpio2_I,		"a.GPIO2i" );
__PLIT8(KClkGpio3_I,		"a.GPIO3i" );
__PLIT8(KClkGpio4_I,		"a.GPIO4i" );
__PLIT8(KClkGpio5_I,		"a.GPIO5i" );
__PLIT8(KClkGpio6_I,		"a.GPIO6i" );
__PLIT8(KClk32Sync_I,		"a.32SYNi" );
__PLIT8(KClkUsb_I,			"a.USBi" );
__PLIT8(KClk48M,			"a.48" );
__PLIT8(KClk12M,			"a.12" );
__PLIT8(KClkSysClk,			"a.SYSCLK" );
__PLIT8(KClkAltClk,			"a.ALTCLK" );
__PLIT8(KClkSysClk32k,		"a.SYS32K" );


// Table converting clock sources to string identifiers for PRM
static const TDesC8* const KNames[] =
	{
	(const TDesC8*)( &KClkMpu ),				// EClkMpu
	(const TDesC8*)( &KClkIva2Pll ),			// EClkIva2Pll
	(const TDesC8*)( &KClkCore ),				// EClkCore
	(const TDesC8*)( &KClkPeriph ),			// EClkPeriph
	(const TDesC8*)( &KClkPeriph2 ),			// EClkPeriph2
	(const TDesC8*)( &KClkPrcmInterface ),		// EClkPrcmInterface
	(const TDesC8*)( &KClkEmu ),				// EClkEmu
	(const TDesC8*)( &KClkNeon ),				// EClkNeon
	(const TDesC8*)( &KClkL3Domain ),			// EClkL3Domain
	(const TDesC8*)( &KClkL4Domain ),			// EClkL4Domain
	(const TDesC8*)( &KClkMpuPll_Bypass ),		// EClkMpuPll_Bypass
	(const TDesC8*)( &KClkIva2Pll_Bypass ),	// EClkIva2Pll_Bypass
	(const TDesC8*)( &KClkRM_F ),				// EClkRM_F
	(const TDesC8*)( &KClk96M ),				// EClk96M
	(const TDesC8*)( &KClk120M ),				// EClk120M
	(const TDesC8*)( &KClkSysOut ),			// EClkSysOut
	(const TDesC8*)( &KClkTv_F ),				// EClkTv_F
	(const TDesC8*)( &KClkDss1_F ),			// EClkDss1_F
	(const TDesC8*)( &KClkDss2_F ),			// EClkDss2_F
	(const TDesC8*)( &KClkCsi2_F ),			// EClkCsi2_F
	(const TDesC8*)( &KClkCam_F ),				// EClkCam_F
	(const TDesC8*)( &KClkIva2_F ),			// EClkIva2_F
	(const TDesC8*)( &KClkMmc1_F ),			// EClkMmc1_F
	(const TDesC8*)( &KClkMmc2_F ),			// EClkMmc2_F
	(const TDesC8*)( &KClkMmc3_F ),			// EClkMmc3_F
	(const TDesC8*)( &KClkMsPro_F ),			// EClkMsPro_F
	(const TDesC8*)( &KClkHdq_F ),				// EClkHdq_F
	(const TDesC8*)( &KClkMcBsp1_F ),			// EClkMcBsp1_F
	(const TDesC8*)( &KClkMcBsp2_F ),			// EClkMcBsp2_F
	(const TDesC8*)( &KClkMcBsp3_F ),			// EClkMcBsp3_F
	(const TDesC8*)( &KClkMcBsp4_F ),			// EClkMcBsp4_F
	(const TDesC8*)( &KClkMcBsp5_F ),			// EClkMcBsp5_F
	(const TDesC8*)( &KClkMcSpi1_F ),			// EClkMcSpi1_F
	(const TDesC8*)( &KClkMcSpi2_F ),			// EClkMcSpi2_F
	(const TDesC8*)( &KClkMcSpi3_F ),			// EClkMcSpi3_F
	(const TDesC8*)( &KClkMcSpi4_F ),			// EClkMcSpi4_F
	(const TDesC8*)( &KClkI2c1_F ),			// EClkI2c1_F
	(const TDesC8*)( &KClkI2c2_F ),			// EClkI2c2_F
	(const TDesC8*)( &KClkI2c3_F ),			// EClkI2c3_F
	(const TDesC8*)( &KClkUart1_F ),			// EClkUart1_F
	(const TDesC8*)( &KClkUart2_F ),			// EClkUart2_F
	(const TDesC8*)( &KClkUart3_F ),			// EClkUart3_F
	(const TDesC8*)( &KClkGpt1_F ),			// EClkGpt1_F
	(const TDesC8*)( &KClkGpt2_F ),			// EClkGpt2_F
	(const TDesC8*)( &KClkGpt3_F ),			// EClkGpt3_F
	(const TDesC8*)( &KClkGpt4_F ),			// EClkGpt4_F
	(const TDesC8*)( &KClkGpt5_F ),			// EClkGpt5_F
	(const TDesC8*)( &KClkGpt6_F ),			// EClkGpt6_F
	(const TDesC8*)( &KClkGpt7_F ),			// EClkGpt7_F
	(const TDesC8*)( &KClkGpt8_F ),			// EClkGpt8_F
	(const TDesC8*)( &KClkGpt9_F ),			// EClkGpt9_F
	(const TDesC8*)( &KClkGpt10_F ),			// EClkGpt10_F
	(const TDesC8*)( &KClkGpt11_F ),			// EClkGpt11_F
	(const TDesC8*)( &KClkUsbTll_F ),			// EClkUsbTll_F
	(const TDesC8*)( &KClkTs_F ),				// EClkTs_F
	(const TDesC8*)( &KClkCpeFuse_F ),			// EClkCpeFuse_F
	(const TDesC8*)( &KClkSgx_F ),				// EClkSgx_F
	(const TDesC8*)( &KClkUsim_F ),			// EClkUsim_F
	(const TDesC8*)( &KClkSmartReflex2_F ),	// EClkSmartReflex2_F
	(const TDesC8*)( &KClkSmartReflex1_F ),	// EClkSmartReflex1_F
	(const TDesC8*)( &KClkWdt2_F ),			// EClkWdt2_F
	(const TDesC8*)( &KClkWdt3_F ),			// EClkWdt3_F
	(const TDesC8*)( &KClkGpio1_F ),			// EClkGpio1_F
	(const TDesC8*)( &KClkGpio2_F ),			// EClkGpio2_F
	(const TDesC8*)( &KClkGpio3_F ),			// EClkGpio3_F
	(const TDesC8*)( &KClkGpio4_F ),			// EClkGpio4_F
	(const TDesC8*)( &KClkGpio5_F ),			// EClkGpio5_F
	(const TDesC8*)( &KClkGpio6_F ),			// EClkGpio6_F
	(const TDesC8*)( &KClkUsb120_F ),			// EClkUsb120_F
	(const TDesC8*)( &KClkUsb48_F ),			// EClkUsb48_F
	(const TDesC8*)( &KClkDss_I ),				// EClkDss_I
	(const TDesC8*)( &KClkCam_I ),				// EClkCam_I
	(const TDesC8*)( &KClkIcr_I ),				// EClkIcr_I
	(const TDesC8*)( &KClkMmc1_I ),			// EClkMmc1_I
	(const TDesC8*)( &KClkMmc2_I ),			// EClkMmc2_I
	(const TDesC8*)( &KClkMmc3_I ),			// EClkMmc3_I
	(const TDesC8*)( &KClkMsPro_I ),			// EClkMsPro_I
	(const TDesC8*)( &KClkHdq_I ),				// EClkHdq_I
	(const TDesC8*)( &KClkAes1_I ),			// EClkAes1_I
	(const TDesC8*)( &KClkAes2_I ),			// EClkAes2_I
	(const TDesC8*)( &KClkSha11_I ),			// EClkSha11_I
	(const TDesC8*)( &KClkSha12_I ),			// EClkSha12_I
	(const TDesC8*)( &KClkDes1_I ),			// EClkDes1_I
	(const TDesC8*)( &KClkDes2_I ),			// EClkDes2_I
	(const TDesC8*)( &KClkMcBsp1_I ),			// EClkMcBsp1_I
	(const TDesC8*)( &KClkMcBsp2_I ),			// EClkMcBsp2_I
	(const TDesC8*)( &KClkMcBsp3_I ),			// EClkMcBsp3_I
	(const TDesC8*)( &KClkMcBsp4_I ),			// EClkMcBsp4_I
	(const TDesC8*)( &KClkMcBsp5_I ),			// EClkMcBsp5_I
	(const TDesC8*)( &KClkI2c1_I ),			// EClkI2c1_I
	(const TDesC8*)( &KClkI2c2_I ),			// EClkI2c2_I
	(const TDesC8*)( &KClkI2c3_I ),			// EClkI2c3_I
	(const TDesC8*)( &KClkUart1_I ),			// EClkUart1_I
	(const TDesC8*)( &KClkUart2_I ),			// EClkUart2_I
	(const TDesC8*)( &KClkUart3_I ),			// EClkUart3_I
	(const TDesC8*)( &KClkMcSpi1_I ),			// EClkMcSpi1_I
	(const TDesC8*)( &KClkMcSpi2_I ),			// EClkMcSpi2_I
	(const TDesC8*)( &KClkMcSpi3_I ),			// EClkMcSpi3_I
	(const TDesC8*)( &KClkMcSpi4_I ),			// EClkMcSpi4_I
	(const TDesC8*)( &KClkGpt1_I ),			// EClkGpt1_I
	(const TDesC8*)( &KClkGpt2_I ),			// EClkGpt2_I
	(const TDesC8*)( &KClkGpt3_I ),			// EClkGpt3_I
	(const TDesC8*)( &KClkGpt4_I ),			// EClkGpt4_I
	(const TDesC8*)( &KClkGpt5_I ),			// EClkGpt5_I
	(const TDesC8*)( &KClkGpt6_I ),			// EClkGpt6_I
	(const TDesC8*)( &KClkGpt7_I ),			// EClkGpt7_I
	(const TDesC8*)( &KClkGpt8_I ),			// EClkGpt8_I
	(const TDesC8*)( &KClkGpt9_I ),			// EClkGpt9_I
	(const TDesC8*)( &KClkGpt10_I ),			// EClkGpt10_I
	(const TDesC8*)( &KClkGpt11_I ),			// EClkGpt11_I
	(const TDesC8*)( &KClkGpt12_I ),			// EClkGpt12_I
	(const TDesC8*)( &KClkMailboxes_I ),		// EClkMailboxes_I
	(const TDesC8*)( &KClkOmapSCM_I ),			// EClkOmapSCM_I
	(const TDesC8*)( &KClkHsUsbOtg_I ),		// EClkHsUsbOtg_I
	(const TDesC8*)( &KClkSdrc_I ),			// EClkSdrc_I
	(const TDesC8*)( &KClkPka_I ),				// EClkPka_I
	(const TDesC8*)( &KClkRng_I ),				// EClkRng_I
	(const TDesC8*)( &KClkUsbTll_I ),			// EClkUsbTll_I
	(const TDesC8*)( &KClkSgx_I ),				// EClkSgx_I
	(const TDesC8*)( &KClkUsim_I ),			// EClkUsim_I
	(const TDesC8*)( &KClkWdt1_I ),			// EClkWdt1_I
	(const TDesC8*)( &KClkWdt2_I ),			// EClkWdt2_I
	(const TDesC8*)( &KClkWdt3_I ),			// EClkWdt3_I
	(const TDesC8*)( &KClkGpio1_I ),			// EClkGpio1_I
	(const TDesC8*)( &KClkGpio2_I ),			// EClkGpio2_I
	(const TDesC8*)( &KClkGpio3_I ),			// EClkGpio3_I
	(const TDesC8*)( &KClkGpio4_I ),			// EClkGpio4_I
	(const TDesC8*)( &KClkGpio5_I ),			// EClkGpio5_I
	(const TDesC8*)( &KClkGpio6_I ),			// EClkGpio6_I
	(const TDesC8*)( &KClk32Sync_I ),			// EClk32Sync_I
	(const TDesC8*)( &KClkUsb_I ),				// EClkUsb_I
	(const TDesC8*)( &KClk48M ),				// EClk48M
	(const TDesC8*)( &KClk12M ),				// EClk12M
	(const TDesC8*)( &KClkSysClk ),				// EClkSysClk
	(const TDesC8*)( &KClkAltClk ),				// EClkAltClk
	(const TDesC8*)( &KClkSysClk32k ),			// EClkSysClk32k
	};

}
__ASSERT_COMPILE( (sizeof( KNames ) / sizeof( KNames[0] )) == Prcm::KSupportedClockCount );

namespace Prcm
{
TSpinLock iLock(/*TSpinLock::EOrderGenericIrqLow0*/); // prevents concurrent access to the prcm hardware registers

void Panic( TPanic aPanic )
	{
	Kern::Fault( "PRCM", aPanic );
	}

void InternalPanic( TInt aLine )
	{
	Kern::Fault( "PRCMINT", aLine );
	}

FORCE_INLINE void _BitClearSet( TUint32 aRegister, TUint32 aClearMask, TUint32 aSetMask )
	{
	volatile TUint32* pR = (volatile TUint32*)aRegister;
	*pR = (*pR & ~aClearMask) | aSetMask;
	}

FORCE_INLINE void _LockedBitClearSet( TUint32 aRegister, TUint32 aClearMask, TUint32 aSetMask )
	{
	volatile TUint32* pR = (volatile TUint32*)aRegister;
	TInt irq = __SPIN_LOCK_IRQSAVE(iLock);
	*pR = (*pR & ~aClearMask) | aSetMask;
	__SPIN_UNLOCK_IRQRESTORE(iLock, irq);
	}


EXPORT_C void SetPllConfig( TPll aPll, const TPllConfiguration& aConfig  )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetPllConfig(%x)", aPll ) );

	__ASSERT_DEBUG( (TUint)aPll < KSupportedPllCount, Panic( ESetPllConfigBadPll ) );

	const TPllControlInfo& inf = KPllControlInfo[ aPll ];

	__ASSERT_DEBUG( aConfig.iDivider <= KPllMaximumDivider,		Panic( ESetPllConfigBadDivider ) );
	__ASSERT_DEBUG( aConfig.iMultiplier <= KPllMaximumMultiplier,	Panic( ESetPllConfigBadMultiplier ) );
	__ASSERT_DEBUG( ((TUint)aConfig.iFreqRange <= EPllRange_1750_2100)
				&&  ((TUint)aConfig.iFreqRange >= EPllRange_075_100),	Panic( ESetPllConfigBadFreqRange ) );
	__ASSERT_DEBUG( ((TUint)aConfig.iRamp <= EPllRamp40us),	Panic( ESetPllConfigBadRamp ) );
	__ASSERT_DEBUG( (TUint)aConfig.iDrift <= EPllDriftGuardEnabled,	Panic( ESetPllConfigBadDrift ) );

	TUint	mult = (aConfig.iMultiplier bitand KPllMultiplierMask) << inf.iMultShift;
	TUint	div = ((aConfig.iDivider - 1) bitand KPllDividerMask) << inf.iDivShift;
	TUint	range = (aConfig.iFreqRange bitand KPllFreqRangeMask) << inf.iFreqSelShift;
	TUint	ramp = (aConfig.iRamp bitand KPllRampMask) << inf.iRampShift;
	TUint	drift = (aConfig.iDrift == EPllDriftGuardEnabled) ? (1 << inf.iDriftShift) : 0;

	TInt irq = __SPIN_LOCK_IRQSAVE(iLock);
	// We must apply frequency range setting before new multuplier and divider
	TUint clearMaskConfig =			(KPllFreqRangeMask << inf.iFreqSelShift)
							bitor	(KPllRampMask << inf.iRampShift)
							bitor	(1 << inf.iDriftShift);
	_BitClearSet( inf.iConfigRegister, clearMaskConfig, range | ramp | drift );

	TUint clearMaskMulDiv =	(KPllMultiplierMask << inf.iMultShift) bitor (KPllDividerMask << inf.iDivShift);
	_BitClearSet( inf.iMulDivRegister, clearMaskMulDiv, mult | div );
	__SPIN_UNLOCK_IRQRESTORE(iLock, irq);
	}

EXPORT_C void PllConfig( TPll aPll, TPllConfiguration& aConfigResult )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PllConfig(%x)", aPll ) );

	__ASSERT_DEBUG( (TUint)aPll < KSupportedPllCount, Panic( EGetPllConfigBadPll ) );

	const TPllControlInfo& inf = KPllControlInfo[ aPll ];

	TUint32 config = AsspRegister::Read32( inf.iConfigRegister );
	TUint32 muldiv = AsspRegister::Read32( inf.iMulDivRegister );

	aConfigResult.iMultiplier =	(muldiv >> inf.iMultShift) bitand KPllMultiplierMask;
	aConfigResult.iDivider =	1 + ((muldiv >> inf.iDivShift) bitand KPllDividerMask);
	aConfigResult.iFreqRange =	static_cast<TPllFrequencyRange>((config >> inf.iFreqSelShift) bitand KPllFreqRangeMask);
	aConfigResult.iRamp =		static_cast<TPllRamp>((config >> inf.iRampShift ) bitand KPllRampMask);
	aConfigResult.iDrift =		(config >> inf.iDriftShift ) bitand 1 ? EPllDriftGuardEnabled : EPllDriftGuardDisabled;

	__KTRACE_OPT( KPRCM, Kern::Printf( "DPLL%d: m=%d, d=%d, fr=%d, r=%d, dr=%d",
						aPll + 1,
						aConfigResult.iMultiplier,
						aConfigResult.iDivider,
						aConfigResult.iFreqRange,
						aConfigResult.iRamp,
						aConfigResult.iDrift ) );
	}

EXPORT_C void SetPllLp( TPll aPll, TLpMode aLpMode )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetPllLp(%x)", aPll ) );

	__ASSERT_DEBUG( (TUint)aPll < KSupportedPllCount, Panic( ESetPllLpBadPll ) );
	__ASSERT_DEBUG( (aLpMode == ENormalMode)
					|| (aLpMode == ELpMode), Panic( ESetPllLpBadMode ) );

	const TPllControlInfo& inf = KPllControlInfo[ aPll ];

	TUint32 clear = 1 << inf.iLpShift;
	TUint32 set = 0;

	if( ELpMode == aLpMode )
		{
		set = clear;
		clear = 0;
		}

	_LockedBitClearSet( inf.iConfigRegister, clear, set );
	}

EXPORT_C TLpMode PllLp( TPll aPll )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PllLp(%x)", aPll ) );

	__ASSERT_DEBUG( (TUint)aPll < KSupportedPllCount, Panic( EGetPllLpBadPll ) );

	const TPllControlInfo& inf = KPllControlInfo[ aPll ];

	TUint32 config = AsspRegister::Read32( inf.iConfigRegister );
	if( 0 == ((config >> inf.iLpShift) bitand 1) )
		{
		return ENormalMode;
		}
	else
		{
		return ELpMode;
		}
	}


EXPORT_C void SetPllMode( TPll aPll, TPllMode aPllMode )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetPllMode(%x;%x)", aPll, aPllMode ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( ESetPllModeBadClock ) );

	TUint32 newMode;
	TUint32 newAuto = KPllAutoOff;

	switch( aPllMode )
		{
		default:
			__DEBUG_ONLY( Panic( ESetPllModeBadMode ) );
			return;

		case EPllStop:
			newMode = KPllModeStop;
			break;

		case EPllBypass:
			newMode = KPllModeBypass;
			break;

		case EPllAuto:
			newAuto = KPllAutoOn;
			// fall through...

		case EPllRun:
			newMode = KPllModeLock;
			break;

		case EPllFastRelock:
			newMode = KPllModeFastRelock;
			break;
		}

	TInt irq = __SPIN_LOCK_IRQSAVE(iLock);

	_BitClearSet(	KPllMode[ aPll ].iModeRegister,
					KPllModeMask << KPllMode[ aPll ].iModeShift,
					newMode << KPllMode[ aPll ].iModeShift );

	_BitClearSet(	KPllMode[ aPll ].iAutoRegister,
					KPllAutoMask << KPllMode[ aPll ].iAutoShift,
					newAuto << KPllMode[ aPll ].iAutoShift );

	__SPIN_UNLOCK_IRQRESTORE(iLock, irq);
	}


EXPORT_C TPllMode PllMode( TPll aPll )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PllMode(%x)", aPll ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( ESetPllModeBadClock ) );

	TUint32 mode = (AsspRegister::Read32( KPllMode[ aPll ].iModeRegister ) >> KPllMode[ aPll ].iModeShift) bitand KPllModeMask;
	TUint32 autoSet = (AsspRegister::Read32( KPllMode[ aPll ].iAutoRegister ) >> KPllMode[ aPll ].iAutoShift) bitand KPllAutoMask;

	static const TPllMode modeTable[8][2] =
		{	// auto disabled	auto enabled
			{ EPllStop,			EPllStop },	// not possible
			{ EPllStop,			EPllStop },
			{ EPllStop,			EPllStop },	// not possible
			{ EPllStop,			EPllStop },	// not possible
			{ EPllStop,			EPllStop },	// not possible
			{ EPllBypass,		EPllBypass },
			{ EPllFastRelock,	EPllAuto },
			{ EPllRun,			EPllAuto },
		};
	return modeTable[ mode ][ (KPllAutoOff == autoSet) ? 0 : 1 ];
	}

EXPORT_C void CalcPllFrequencyRange( TPll aPll, TPllConfiguration& aConfig )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::CalcPllFrequencyRange(%x)", aPll ) );

	struct TFreqSelRange
		{
		TUint	iMin;
		TUint	iMax;
		TPllFrequencyRange	iSetting;
		};

	const TFreqSelRange KRanges[] =
		{
			{ 750000,	1000000,	EPllRange_075_100 },
			{ 1000001,	1250000,	EPllRange_100_125 },
			{ 1250001,	1500000,	EPllRange_125_150 },
			{ 1500001,	1750000,	EPllRange_150_175 },
			{ 1750001,	2100000,	EPllRange_175_210 },
			{ 7500000,	10000000,	EPllRange_750_1000 },
			{ 10000001,	12500000,	EPllRange_1000_1250 },
			{ 12500001,	15000000,	EPllRange_1250_1500 },
			{ 15000001,	17500000,	EPllRange_1500_1750 },
			{ 17500001,	21000000,	EPllRange_1750_2100 },
			{ 0,		0,			EPllRange_1750_2100	}
		};

	// We have to work out the internal frequency from the source clock frequency and the
	// divider factor N

	const TUint32 divider =	aConfig.iDivider;

	TInt found = -1;

	if( divider > 0 )
		{
		TUint fInternal = ClockFrequency( EClkSysClk ) / divider;

		// Find an appropriate range
		for( TInt i = 0; KRanges[i].iMax > 0; ++i )
			{
			if( fInternal < KRanges[i].iMin )
				{
				// We've passed all possible ranges, work out whether current or previous is nearest
				__DEBUG_ONLY( Panic( EPllInternalFrequencyOutOfRange ) );

				if( i > 0 )
					{
					// How near are we to minimum of current range?
					TUint currentDiff = KRanges[i].iMin - fInternal;

					// How near are we to maximum of previous range?
					TUint prevDiff = fInternal - KRanges[i - 1].iMax;

					found = (prevDiff < currentDiff) ? i - 1 : i;
					}
				else
					{
					// it's below minimum, so use minimum range
					found = 0;
					}
				break;
				}
			else if( (KRanges[i].iMin <= fInternal) && (KRanges[i].iMax >= fInternal) )
				{
				found = i;
				break;
				}
			}

		}
	// If we've fallen off end of list, use maximum setting
	__ASSERT_DEBUG( found >= 0, Panic( EPllInternalFrequencyOutOfRange ) );
	aConfig.iFreqRange = (found >= 0) ? KRanges[ found ].iSetting : EPllRange_1750_2100;
	}


EXPORT_C void AutoSetPllLpMode( TPll aPll )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PllMode(%x)", aPll ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( ESetPllModeBadClock ) );

	const TUint32 reg = KPllControlInfo[ aPll ].iConfigRegister;
	const TUint shift = KPllControlInfo[ aPll ].iLpShift;

	TUint freq = ClockFrequency( KPllToClock[ aPll ] );
	TUint32 clear = 1 << shift;
	TUint32 set = 0;
	if( freq <= KPllLpModeMaximumFrequency )
		{
		// LP mode can be enabled
		set = clear;
		clear = 0;
		}
	_LockedBitClearSet( reg, clear, set );
	}

EXPORT_C TBool PllIsLocked( TPll aPll )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PllIsLocked(%x)", aPll ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( EPllIsLockedBadPll ) );

	TUint32 reg = KPllControlInfo[ aPll ].iStatusRegister;
	TUint32 lockMask = 1 << KPllControlInfo[ aPll ].iLockBit;

	return ( 0 != (AsspRegister::Read32( reg ) bitand lockMask) );
	}

EXPORT_C void WaitForPllLock( TPll aPll )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::WaitForPllLock(%x)", aPll ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( EWaitForPllLockBadPll ) );

	TUint32 reg = KPllControlInfo[ aPll ].iStatusRegister;
	TUint32 lockMask = 1 << KPllControlInfo[ aPll ].iLockBit;

	while( 0 == (AsspRegister::Read32( reg ) bitand lockMask) );
	}

EXPORT_C void SetPllBypassDivider( TPll aPll, TBypassDivider aDivider )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetPllBypassDivider(%x;%x)", aPll, aDivider ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( ESetPllBypassDividerBadPll ) );
	__ASSERT_DEBUG( (TUint)aDivider <= EBypassDiv4, Panic( ESetPllBypassDividerBadDivider ) );

	static const TUint8 KLookupTable[] =
		{
		1,	// EBypassDiv1
		2,	// EBypassDiv2
		4.	// EBypassDiv4
		};

	TUint32 div = KLookupTable[ aDivider ];

	switch( aPll )
		{
		case EDpll1:
			_LockedBitClearSet( KCM_CLKSEL1_PLL_MPU, KBit19 | KBit20 | KBit21, div << 19 );
			break;

		case EDpll2:
			_LockedBitClearSet( KCM_CLKSEL1_PLL_IVA2, KBit19 | KBit20 | KBit21, div << 19 );
			break;

		default:
			break;
		}
	}

EXPORT_C TBypassDivider PllBypassDivider( TPll aPll )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PllBypassDivider(%x)", aPll ) );
	__ASSERT_DEBUG( (TUint)aPll <= EDpll5, Panic( EPllBypassDividerBadPll ) );

	TUint div = 1;

	switch( aPll )
		{
		case EDpll1:
			div = (AsspRegister::Read32( KCM_CLKSEL1_PLL_MPU ) >> 19) bitand 0x7;
			break;

		case EDpll2:
			div = (AsspRegister::Read32( KCM_CLKSEL1_PLL_IVA2 ) >> 19) bitand 0x7;
			break;

		default:
			break;
		}

	TBypassDivider result = EBypassDiv1;

	if( 2 == div )
		{
		result = EBypassDiv2;
		}
	else if( 4 == div )
		{
		result = EBypassDiv4;
		}

	return result;
	}

EXPORT_C void SetDivider( TClock aClock, TUint aDivide )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetDivider(%x;%x)", aClock, aDivide ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( ESetDividerBadClock ) );

	const TDividerInfo&	inf = KDividerInfo[ aClock ];

	TUint32 div = aDivide;	// most common case, special cases handled below

	switch( inf.iDivType )
		{
		case EDivUsimClk:
			// Special case, not suppored by this function - use SetUsimClockDivider()
			return;

		default:
		case EDivNotSupported:
			Panic( ESetDividerUnsupportedClock );
			return;

		case EDiv_1_2:
			if( (1 != aDivide ) && (2 != aDivide ) )
				{
				__DEBUG_ONLY( Panic( ESetDividerBadDivider ) );
				return;
				}
			break;

		case EDivCore_1_2_4:
			if( (1 != aDivide ) && (2 != aDivide ) && (3 != aDivide) )
				{
				__DEBUG_ONLY( Panic( ESetDividerBadDivider ) );
				return;
				}
			break;

		case EDivCore_3_4_6_96M:
			{
			switch( aDivide )
				{
				default:
					__DEBUG_ONLY( Panic( ESetDividerBadDivider ) );
					return;

				case 3:
					div = 0;
					break;

				case 4:
					div = 1;
					break;

				case 6:
					div = 2;
					break;

				case 0:
					// Special-case, use 96MHz clock
					div = 3;
					break;
				}
			break;
			}

		case EDivPll_1_To_16:
			if( (aDivide < 1) || (aDivide > 16) )
				{
				__DEBUG_ONLY( Panic( ESetDividerBadDivider ) );
				return;
				}
			break;

		case EDivPll_1_To_31:
			if( (aDivide < 1) || (aDivide > 16) )
				{
				__DEBUG_ONLY( Panic( ESetDividerBadDivider ) );
				return;
				}
			break;



		case EDivClkOut_1_2_4_8_16:
			{
			switch( aDivide )
				{
				default:
					__DEBUG_ONLY( Panic( ESetDividerBadDivider ) );
					return;

				case 1:
					div = 0;
					break;

				case 2:
					div = 1;
					break;

				case 4:
					div = 2;
					break;

				case 8:
					div = 3;
					break;

				case 16:
					div = 4;
					break;
				}
			break;
			}
		}

	// if we get here, we have a valid divider value

	_LockedBitClearSet( inf.iRegister, inf.iMask, div << inf.iShift );
	}

EXPORT_C TUint Divider( TClock aClock )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::Divider(%x)", aClock ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( EGetDividerBadClock ) );

	const TDividerInfo&	inf = KDividerInfo[ aClock ];

	TUint32 div = ( AsspRegister::Read32( inf.iRegister ) bitand inf.iMask ) >> inf.iShift;
	TUint result = div;	// most common case

	switch( inf.iDivType )
		{
		case EDivUsimClk:
			return UsimDivider();

		default:
		case EDivNotSupported:
			Panic( ESetDividerUnsupportedClock );
			return 0xFFFFFFFF;

		// These are all the standard case, where value in register is divide factor
		case EDiv_1_2:
		case EDivCore_1_2_4:
		case EDivPll_1_To_16:
		case EDivPll_1_To_31:
			break;

		case EDivCore_3_4_6_96M:
			{
			switch( div )
				{
				default:
					// hardware value has unknown meaning
					result = 0xFFFFFFFF;

				case 0:
					result = 3;
					break;

				case 1:
					result = 4;
					break;

				case 2:
					result = 6;
					break;

				case 3:
					result = 0;
					break;
				}
			break;
			}

		case EDivClkOut_1_2_4_8_16:
			{
			switch( div )
				{
				default:
					// hardware value has unknown meaning
					result = 0xFFFFFFFF;

				case 0:
					result = 1;
					break;

				case 1:
					result = 2;
					break;

				case 2:
					result = 4;
					break;

				case 3:
					result = 8;
					break;

				case 4:
					result = 16;
					break;
				}
			break;
			}
		}

	return result;
	}

EXPORT_C void SetPowerDomainMode( TPowerDomain aDomain, TPowerDomainMode aMode )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetPowerDomainMode(%x;%x)", aDomain, aMode ) );
	__ASSERT_DEBUG( (TUint)aDomain < KSupportedPowerDomainCount, Panic( ESetDomainModeBadDomain ) );
	__ASSERT_DEBUG( (TUint)aMode <= EPowerOn, Panic( ESetDomainModeBadMode ) );

	__ASSERT_DEBUG( 0 != (KPowerDomainControl[ aDomain ].iAllowedMask bitand (1 << aMode)), Panic( ESetDomainModeUnsupportedMode ) );

	TUint shift = KPowerDomainControl[ aDomain ].iShift;

	_LockedBitClearSet( KPowerDomainControl[ aDomain ].iRegister,
						KPowerModeMask << shift,
						aMode << shift );
	}

EXPORT_C TPowerDomainMode PowerDomainMode( TPowerDomain aDomain )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::PowerDomainMode(%x)", aDomain ) );
	__ASSERT_DEBUG( (TUint)aDomain < KSupportedPowerDomainCount, Panic( EGetDomainModeBadDomain ) );

	TUint32 m = (AsspRegister::Read32( KPowerDomainControl[ aDomain ].iRegister ) >> KPowerDomainControl[ aDomain ].iShift) bitand KPowerModeMask;
	return static_cast< TPowerDomainMode >( m );
	}

EXPORT_C void SetClockState( TClock aClock, TClockState aState )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetClockState(%x;%x)", aClock, aState ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( ESetStateBadClock ) );

	const TClockEnableAutoInfo& def = KClockControlTable[ aClock ];

	TUint32 reg = def.iGate.iRegister;
	TUint32 mask = def.iGate.iMask;
	TUint32 autoReg = def.iAuto.iRegister;
	TUint32 autoMask = def.iAuto.iMask;

	TInt irq = __SPIN_LOCK_IRQSAVE(iLock);

	if( EClkOn == aState )
		{
		_BitClearSet( reg, mask, def.iGate.iEnablePattern );
		_BitClearSet( autoReg, autoMask, def.iAuto.iDisablePattern );
		}
	else if( EClkOff == aState )
		{
		_BitClearSet( reg, mask, def.iGate.iDisablePattern );
		_BitClearSet( autoReg, autoMask, def.iAuto.iDisablePattern );
		}
	else if( EClkAuto == aState )
		{
		_BitClearSet( autoReg, autoMask, def.iAuto.iEnablePattern );
		_BitClearSet( reg, mask, def.iGate.iEnablePattern );
		}

	__SPIN_UNLOCK_IRQRESTORE(iLock, irq);
	}

EXPORT_C TClockState ClockState( TClock aClock )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "+Prcm::ClockState(%x)", aClock ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( EGetStateBadClock ) );

	const TClockEnableAutoInfo& def = KClockControlTable[ aClock ];

	TUint32 reg = def.iGate.iRegister;
	TUint32 mask = def.iGate.iMask;
	TUint32 autoReg = def.iAuto.iRegister;
	TUint32 autoMask = def.iAuto.iMask;

	TUint32 enable = AsspRegister::Read32( reg ) bitand mask;
	TUint32 autoClock = AsspRegister::Read32( autoReg ) bitand autoMask;

	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::ClockState(%x):e:%x a:%x", aClock, enable, autoClock ) );

	TClockState state = EClkAuto;

	// OFF = OFF
	// ON + AUTO = AUTO
	// ON + !AUTO = ON
	if( def.iGate.iEnablePattern != enable )
		{
		state = EClkOff;
		}
	else if( def.iAuto.iEnablePattern != autoClock )
		{
		state = EClkOn;
		}

	__KTRACE_OPT( KPRCM, Kern::Printf( "-Prcm::ClockState(%x):%d", aClock, state ) );

	return state;
	}

EXPORT_C void SetWakeupMode( TClock aClock, TWakeupMode aMode )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetWakeupMode(%x;%x)", aClock, aMode ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( ESetWakeupBadClock ) );

	const TRegisterBitDef& def = KClockWakeupTable[ aClock ];

	TUint32 reg = def.iRegister;
	TUint32 mask = def.iMask;

	TInt irq = __SPIN_LOCK_IRQSAVE(iLock);

	if( EWakeupEnabled == aMode )
		{
		_BitClearSet( reg, mask, def.iEnablePattern );
		}
	else
		{
		_BitClearSet( reg, mask, def.iDisablePattern );
		}

	__SPIN_UNLOCK_IRQRESTORE(iLock, irq);
	}

EXPORT_C TWakeupMode WakeupMode( TClock aClock )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::WakeupMode(%x)", aClock ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( EGetWakeupBadClock ) );

	const TRegisterBitDef& def = KClockWakeupTable[ aClock ];

	TUint32 reg = def.iRegister;
	TUint32 mask = def.iMask;

	if( def.iEnablePattern == (AsspRegister::Read32( reg ) bitand mask) )
		{
		return EWakeupEnabled;
		}
	else
		{
		return EWakeupDisabled;
		}
	}

EXPORT_C void AddToWakeupGroup( TClock aClock, TWakeupGroup aGroup )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::AddToWakeupGroup(%x;%x)", aClock, aGroup ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( EAddWakeupGroupBadClock ) );
	__ASSERT_DEBUG( (TUint)aGroup < KSupportedWakeupGroupCount, Panic( EAddWakeupGroupBadGroup ) );

	const TRegisterBitDef& def = KClockWakeupGroupTable[ aClock ][ aGroup ];

	TUint32 reg = def.iRegister;
	TUint32 mask = def.iMask;

	_LockedBitClearSet( reg, mask, def.iEnablePattern );
	}

EXPORT_C void RemoveFromWakeupGroup( TClock aClock, TWakeupGroup aGroup )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::RemoveFromWakeupGroup(%x;%x)", aClock, aGroup ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( ERemoveWakeupGroupBadClock ) );
	__ASSERT_DEBUG( (TUint)aGroup < KSupportedWakeupGroupCount, Panic( ERemoveWakeupGroupBadGroup ) );

	const TRegisterBitDef& def = KClockWakeupGroupTable[ aClock ][ aGroup ];

	TUint32 reg = def.iRegister;
	TUint32 mask = def.iMask;

	_LockedBitClearSet( reg, mask, def.iDisablePattern );
	}

EXPORT_C TBool IsInWakeupGroup( TClock aClock, TWakeupGroup aGroup )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::IsInWakeupGroup(%x)", aClock ) );

	__ASSERT_DEBUG( (TUint)aClock < KSupportedClockCount, Panic( EGetWakeupGroupBadClock ) );
	__ASSERT_DEBUG( (TUint)aGroup < KSupportedWakeupGroupCount, Panic( EGetWakeupGroupBadGroup ) );

	const TRegisterBitDef& def = KClockWakeupGroupTable[ aClock ][ aGroup ];

	TUint32 reg = def.iRegister;
	TUint32 mask = def.iMask;

	return( def.iEnablePattern == (AsspRegister::Read32( reg ) bitand mask) );
	}


EXPORT_C void AddToWakeupDomain( TClock aClock, TWakeupDomain aDomain )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::AddToWakeupDomain(%x;%x)", aClock, aDomain ) );

	__ASSERT_DEBUG( (TUint)aClock <= (TUint)KSupportedClockCount, Panic( EAddDomainBadClock ) );
	__ASSERT_DEBUG( (TUint)aDomain <= (TUint)KSupportedWakeupDomainCount, Panic( EAddDomainBadDomain ) );

	const TWakeupDomainInfo& inf = KClockWakeupDomainTable[ aClock ];
	TUint32 mask = 1 << (TUint)inf.iBitNumber[ aDomain ];	// unsupported bit numbers will result in a mask of 0x00000000

	_LockedBitClearSet( inf.iRegister, KClearNone, mask );
	}

EXPORT_C void RemoveFromWakeupDomain( TClock aClock, TWakeupDomain aDomain )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::RemoveFromWakeupDomain(%x;%x)", aClock, aDomain ) );

	__ASSERT_DEBUG( (TUint)aClock <= (TUint)KSupportedClockCount, Panic( ERemoveDomainBadClock ) );
	__ASSERT_DEBUG( (TUint)aDomain <= (TUint)KSupportedWakeupDomainCount, Panic( ERemoveDomainBadDomain ) );

	const TWakeupDomainInfo& inf = KClockWakeupDomainTable[ aClock ];
	TUint32 mask = 1 << (TUint)inf.iBitNumber[ aDomain ];	// unsupported bit numbers will result in a mask of 0x00000000

	_LockedBitClearSet( inf.iRegister, mask, KSetNone );
	}

EXPORT_C TBool IsInWakeupDomain( TClock aClock, TWakeupDomain aDomain )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::IsInWakeupDomain(%x;%x)", aClock, aDomain ) );

	__ASSERT_DEBUG( (TUint)aClock <= (TUint)KSupportedClockCount, Panic( ECheckDomainBadClock ) );
	__ASSERT_DEBUG( (TUint)aDomain <= (TUint)KSupportedWakeupDomainCount, Panic( ECheckDomainBadDomain ) );

	const TWakeupDomainInfo& inf = KClockWakeupDomainTable[ aClock ];
	TUint32 mask = 1 << (TUint)inf.iBitNumber[ aDomain ];	// unsupported bit numbers will result in a mask of 0x00000000

	return ( 0 != (AsspRegister::Read32( inf.iRegister ) bitand mask) );
	}

EXPORT_C void SetGptClockSource( TGpt aGpt, TGptClockSource aSource )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetGptClockSource(%x;%x)", aGpt, aSource ) );

	__ASSERT_DEBUG( (TUint)aGpt <= (TUint)EGpt12, Panic( ESetGptClockBadGpt ) );


	TUint32 reg = KGptClockSourceInfo[ aGpt ].iRegister;
	TUint32 mask = KGptClockSourceInfo[ aGpt ].iMask;
	TUint32 setPattern = (EGptClockSysClk == aSource ) ? mask : 0;

	_LockedBitClearSet( reg, mask, setPattern );
	}

EXPORT_C TGptClockSource GptClockSource( TGpt aGpt )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::GptClockSource(%x)", aGpt ) );

	__ASSERT_DEBUG( (TUint)aGpt <= (TUint)EGpt12, Panic( ESetGptClockBadGpt ) );

	TUint32 reg = KGptClockSourceInfo[ aGpt ].iRegister;
	TUint32 mask = KGptClockSourceInfo[ aGpt ].iMask;

	if( 0 == (AsspRegister::Read32( reg ) bitand mask) )
		{
		return EGptClock32k;
		}
	else
		{
		return EGptClockSysClk;
		}
	}

EXPORT_C TUint UsimDivider()
	{
	const TDividerInfo& info = KDividerInfo[ EClkUsim_F ];
	TUint divmux = (AsspRegister::Read32( info.iRegister ) bitand info.iMask ) >> info.iShift;
	return UsimDivMuxInfo[ divmux ].iDivider;
	}

EXPORT_C TClock UsimClockSource()
	{
	const TDividerInfo& info = KDividerInfo[ EClkUsim_F ];
	TUint divmux = (AsspRegister::Read32( info.iRegister ) bitand info.iMask ) >> info.iShift;
	return UsimDivMuxInfo[ divmux ].iClock;
	}

EXPORT_C void SetClockMux( TClock aClock, TClock aSource )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::SetClockMux(%x;%x)", aClock, aSource ) );

	switch( aClock )
		{
		case EClk96M:
			{
			TUint set = KBit6;
			TUint clear = 0;

			switch( aSource )
				{
				case EClkPeriph:
					clear = KBit6;
					set = 0;
					// fall through...

				case EClkSysClk:
					_LockedBitClearSet( KCM_CLKSEL1_PLL, clear, set );
					break;

				default:
					Panic( ESetClockMuxBadSource );
				}
			break;
			}

		case EClkSysOut:
			{
			TUint set;
			switch( aSource )
				{
				case EClkCore:
					set = 0;
					break;

				case EClkSysClk:
					set = 1;
					break;

				case EClkPeriph:
					set = 2;
					break;

				case EClkTv_F:
					set = 3;
					break;

				default:
					Panic( ESetClockMuxBadSource );
					return;
				}

			_LockedBitClearSet( KCM_CLKOUT_CTRL, KBit1 | KBit0, set );
			break;
			}

		case EClkTv_F:
			{
			TUint set = KBit5;
			TUint clear = 0;

			switch( aSource )
				{
				case EClkPeriph:
					clear = KBit5;
					set = 0;
					// fall through...

				case EClkAltClk:
					_LockedBitClearSet( KCM_CLKSEL1_PLL, clear, set );
					break;

				default:
					Panic( ESetClockMuxBadSource );
					return;
				}
			break;
			}

		case EClkGpt1_F:
		case EClkGpt2_F:
		case EClkGpt3_F:
		case EClkGpt4_F:
		case EClkGpt5_F:
		case EClkGpt6_F:
		case EClkGpt7_F:
		case EClkGpt8_F:
		case EClkGpt9_F:
			{
			TGptClockSource src = EGptClock32k;

			switch( aSource )
				{
				case EClkSysClk:
					src = EGptClockSysClk;
				case EClkSysClk32k:
					break;
				default:
					Panic( ESetClockMuxBadSource );
					return;
				}

			SetGptClockSource( KClockSourceInfo[ aClock ].iGpt, src );
			break;
			}

		case EClkSgx_F:
			switch( aSource )
				{
				case EClk96M:
					SetDivider( EClkSgx_F, 0 );
					break;

				case EClkCore:
					// Unfortunately the combined divider/mux means that switching from
					// CORE t 96M loses the old divider values
					if( 0 != Divider( EClkSgx_F ) )
						{
						// Not currently CORE, switch to default maximum divider
						SetDivider( EClkSgx_F, 6 );
						}
					break;

				default:
					Panic( ESetClockMuxBadSource );
					return;
				}
			break;


		case EClk48M:
			{
			TUint set = KBit3;
			TUint clear = 0;

			switch( aSource )
				{
				case EClkPeriph:
					clear = KBit3;
					set = 0;
					// fall through...

				case EClkAltClk:
					_LockedBitClearSet( KCM_CLKSEL1_PLL, clear, set );
					break;

				default:
					Panic( ESetClockMuxBadSource );
					return;
				}
			break;
			}

		default:
			Panic( ESetClockMuxBadClock );
			return;
		}
	}

EXPORT_C TClock ClockMux( TClock aClock )
	{
	__KTRACE_OPT( KPRCM, Kern::Printf( "Prcm::ClockMux(%x)", aClock ) );

	TClock result;

	switch( aClock )
		{
		case EClk96M:
			if( 0 == (AsspRegister::Read32( KCM_CLKSEL1_PLL ) bitand KBit6 ) )
				{
				result = EClkPeriph;
				}
			else
				{
				result = EClkSysClk;
				}
			break;

		case EClkSysOut:
			switch( AsspRegister::Read32( KCM_CLKOUT_CTRL ) bitand (KBit1 | KBit0) )
				{
				default:
				case 0:
					result = EClkCore;
					break;

				case 1:
					result = EClkSysClk;
					break;

				case 2:
					result = EClkPeriph;
					break;

				case 3:
					result = EClkTv_F;		// same as 54MHz clock
					break;
				}
			break;

		case EClkTv_F:
			if( 0 == (AsspRegister::Read32( KCM_CLKSEL1_PLL ) bitand KBit5 ) )
				{
				result = EClkPeriph;
				}
			else
				{
				result = EClkAltClk;
				}
			break;

		case EClkGpt1_F:
		case EClkGpt2_F:
		case EClkGpt3_F:
		case EClkGpt4_F:
		case EClkGpt5_F:
		case EClkGpt6_F:
		case EClkGpt7_F:
		case EClkGpt8_F:
		case EClkGpt9_F:
		case EClkGpt10_F:
		case EClkGpt11_F:
			// Redirect these to GptClockSource()
			if( EGptClockSysClk == GptClockSource( KClockSourceInfo[ aClock ].iGpt ) )
				{
				result = EClkSysClk;
				}
			else
				{
				result = EClkSysClk32k;
				}
			break;

		case EClkSgx_F:
			if( Divider( EClkSgx_F ) == 0 )
				{
				result = EClk96M;
				}
			else
				{
				result = EClkCore;
				}
			break;

		case EClkUsim_F:
			result = UsimClockSource();
			break;

		case EClk48M:
			if( 0 == (AsspRegister::Read32( KCM_CLKSEL1_PLL ) bitand KBit3 ) )
				{
				result = EClk96M;
				}
			else
				{
				result = EClkAltClk;
				}
			break;

		default:
			Panic( EGetClockMuxBadClock );
			return EClkAltClk;	// dumy to stop compiler warning
		}

	return result;
	}

EXPORT_C TUint ClockFrequency( TClock aClock )
	{
	// Works out the frequency by traversing backwards through the clock chain
	// assumulating a multply and divide factor until SYSCLK or SYSCLK32 is reached
	// Reaching a DPLL implicitly means SYSCLK has been reached

	TUint mul = 1;
	TUint div = 1;
	TClock currentClock = aClock;
	__ASSERT_ALWAYS( currentClock < Prcm::KSupportedClockCount, Panic( EClockFrequencyBadClock ) );

	// Ensure assumption that root clock range is >=EClkSysClk
	__ASSERT_COMPILE( EClkSysClk < EClkAltClk );
	__ASSERT_COMPILE( EClkAltClk < EClkSysClk32k );
	__ASSERT_COMPILE( (TUint)EClkSysClk32k == (TUint)KSupportedClockCount - 1 );

	while( currentClock < EClkSysClk )
		{
		// Get previous clock in chain
		TClock prevClock = KClockSourceInfo[ currentClock ].iClock;

		switch( KClockSourceInfo[ currentClock ].iType )
			{
			case EIgnore:
				return 0;	// unsupported clock

			case EDpll:
				{
				TPll pll = KClockSourceInfo[ currentClock ].iPll;

				if( PllMode( pll ) == EPllBypass )
					{
					if( EDpll1 == pll )
						{
						prevClock = Prcm::EClkMpuPll_Bypass;
						}
					else if( EDpll2 == pll )
						{
						prevClock = Prcm::EClkIva2Pll_Bypass;
						}
					else
						{
						// for all other DPLL1 the bypass clock is the input clock SYSCLK
						prevClock = EClkSysClk;
						}
					}
				else
					{
					TPllConfiguration pllCfg;
					PllConfig( pll, pllCfg );
					mul *= pllCfg.iMultiplier;
					div *= pllCfg.iDivider;
					if( EDpll4 == pll )
						{
						// Output is multiplied by 2 for DPLL4
						mul *= 2;
						}
					prevClock = EClkSysClk;
					}
				break;
				}

			case EMux:
				prevClock = ClockMux( currentClock );
				break;

			case EDivMux:
				// need to find what clock the divider is fed from
				prevClock = ClockMux( currentClock );
				// fall through to get divider..

			case EDivider:
				{
				TUint selectedDiv = Divider( currentClock );
				// Special case for SGX - ignore a return of 0
				if( 0 != selectedDiv )
					{
					div *= selectedDiv;
					}
				break;
				}

			case EDuplicate:
				// Nothing to do, we just follow to the next clock
				break;

			case E48MMux:
				prevClock = ClockMux( currentClock );
				if( prevClock != EClkAltClk )
					{
					div *= 2;
					}
				break;

			case E54MMux:
				prevClock = ClockMux( currentClock );
				if( prevClock != EClkAltClk )
					{
					div *= Divider( currentClock );
					}
				break;

			case E96MMux:
				prevClock = ClockMux( currentClock );
				if( prevClock != EClkSysClk )
					{
					div *= Divider( currentClock );
					}
				break;

			case EDiv4:
				div *= 4;
				break;
			}

		currentClock = prevClock;
		}	// end do

	// When we reach here we have worked back to the origin clock

	TUint64 fSrc;
	const Omap3530Assp* variant = (Omap3530Assp*)Arch::TheAsic();

	if( EClkSysClk == currentClock )
		{
		// input OSC_SYSCLK is always divided by 2 before being fed to SYS_CLK
		fSrc = variant->SysClkFrequency() / 2;
		}
	else if( EClkSysClk32k == currentClock )
		{
		fSrc = variant->SysClk32kFrequency();
		}
	else
		{
		fSrc = variant->AltClkFrequency();
		}

	if( div == 0 )
		{
		// to account for any registers set at illegal values
		return 0;
		}
	else
		{
		return (TUint)((fSrc * mul) / div);
		}
	}

EXPORT_C void SetSysClkFrequency( TSysClkFrequency aFrequency )
	{
	static const TUint8 KConfigValues[] =
		{
		0,	// ESysClk12MHz
		1,	// ESysClk13MHz
		5,	// ESysClk16_8MHz
		2,	// ESysClk19_2MHz
		3,	// ESysClk26MHz
		4	// ESysClk38_4MHz
		};

	_LockedBitClearSet( KPRM_CLKSEL, KBit0 | KBit1 | KBit2, KConfigValues[ aFrequency ] );
	}

/** Get the currently configured SysClk frequency */
EXPORT_C TSysClkFrequency SysClkFrequency()
	{

	switch( AsspRegister::Read32( KPRM_CLKSEL ) bitand (KBit0 | KBit1 | KBit2) )
		{
		case 0:
			return ESysClk12MHz;
		case 1:
			return ESysClk13MHz;
		case 2:
			return ESysClk19_2MHz;
		case 3:
			return ESysClk26MHz;
		case 4:
			return ESysClk38_4MHz;
		case 5:
			return ESysClk16_8MHz;
		default:
			__DEBUG_ONLY( InternalPanic( __LINE__ ) );
			return ESysClk13MHz;
		}
	}


EXPORT_C const TDesC& PrmName( TClock aClock )
	{
	__ASSERT_DEBUG( (TUint)aClock <= KSupportedClockCount, Panic( EGetNameBadClock ) );
	__ASSERT_DEBUG( KNames[ aClock ] != NULL, Kern::Fault( "PrmName", aClock ) );

	return *KNames[ aClock ];
	}

EXPORT_C void Init3()
	{
	// Enable LP mode if possible on MPU and CORE PLLs.
	// Don't enable on PERIPHERAL, IVA2 or USB because LP mode introduces jitter
	AutoSetPllLpMode( EDpll1 );
	AutoSetPllLpMode( EDpll3 );

	TInt irq = __SPIN_LOCK_IRQSAVE(iLock);
	TUint32 r;

	// IVA2
//	Not yet mapped! const TUint32 KPDCCMD = Omap3530HwBase::TVirtual<0x01810000>::Value;
//	r = AsspRegister::Read32(KPDCCMD);
//	AsspRegister::Modify32(KPDCCMD, 0, 1 << 16);
//	Set(KCM_FCLKEN_IVA2, 1 << 0, 0);
	// CAM
	const TUint32 KISP_CTRL = Omap3530HwBase::TVirtual<0x480BC040>::Value;
	r = AsspRegister::Read32(KISP_CTRL);
	_BitClearSet(KISP_CTRL, 0xf << 10, 0);
	_BitClearSet(KCM_FCLKEN_CAM, 1 << 0, 0);
	_BitClearSet(KCM_ICLKEN_CAM, 1 << 0, 0);

	// MMC
	r = AsspRegister::Read32(KMMCHS1_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMmc1_F, EClkOff );
	SetClockState( EClkMmc1_I, EClkOff );
	r = AsspRegister::Read32(KMMCHS2_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMmc2_F, EClkOff );
	SetClockState( EClkMmc2_I, EClkOff );
/* There is no MMC3 on the beagle board
	r = AsspRegister::Read32(KMMCHS3_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
*/
	SetClockState( EClkMmc3_F, EClkOff );
	SetClockState( EClkMmc3_I, EClkOff );

	// McBSP
	r = AsspRegister::Read32(KMCBSPLP1_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcBsp1_F, EClkOff );
	SetClockState( EClkMcBsp1_I, EClkOff );
	const TUint32 KMCBSPLP2_SPCR1 = Omap3530HwBase::TVirtual<0x49022014>::Value;
	_BitClearSet(KMCBSPLP2_SPCR1, 1 << 0, 0); // RRST := 0
	const TUint32 KMCBSPLP2_SPCR2 = Omap3530HwBase::TVirtual<0x49022010>::Value;
	_BitClearSet(KMCBSPLP2_SPCR2, 1 << 7 | 1 << 0, 0); // FRST, XRST := 0
	_BitClearSet(KMCBSPLP2_SYSCONFIG, 0x3 << 8 | 0x3 << 3, 0); // CLOCKACTIVITY := can be switched off, SIDLEMODE := force idle
	r = AsspRegister::Read32(KMCBSPLP2_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcBsp2_F, EClkOff );
	SetClockState( EClkMcBsp2_I, EClkOff );
	r = AsspRegister::Read32(KMCBSPLP3_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcBsp3_F, EClkOff );
	SetClockState( EClkMcBsp3_I, EClkOff );
	r = AsspRegister::Read32(KMCBSPLP4_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcBsp4_F, EClkOff );
	SetClockState( EClkMcBsp4_I, EClkOff );
	r = AsspRegister::Read32(KMCBSPLP5_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcBsp5_F, EClkOff );
	SetClockState( EClkMcBsp5_I, EClkOff );

	// McSPI
	r = AsspRegister::Read32(KMCSPI1_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcSpi1_F, EClkOff );
	SetClockState( EClkMcSpi1_I, EClkOff );
	r = AsspRegister::Read32(KMCSPI2_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkMcSpi2_F, EClkOff );
	SetClockState( EClkMcSpi2_I, EClkOff );
	r = AsspRegister::Read32(KMCSPI3_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	/* nxz enable SPI 3
	SetClockState( EClkMcSpi3_F, EClkOff );
	SetClockState( EClkMcSpi3_I, EClkOff );*/
	SetClockState( EClkMcSpi3_F, EClkOn );
	SetClockState( EClkMcSpi3_I, EClkOn );
	r = AsspRegister::Read32(KMCSPI4_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	/* nxz enable SPI 4
	SetClockState( EClkMcSpi4_F, EClkOff );
	SetClockState( EClkMcSpi4_I, EClkOff );*/
	SetClockState( EClkMcSpi4_F, EClkOn );
	SetClockState( EClkMcSpi4_I, EClkOn );

	// UART
	TInt debugport = Kern::SuperPage().iDebugPort;
	if( debugport != 0 )
		{
		r = AsspRegister::Read32(KUART1_SYSC);
		__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
		SetClockState( EClkUart1_F, EClkOff );
		SetClockState( EClkUart1_I, EClkOff );
		}
	if( debugport != 1 )
		{
		r = AsspRegister::Read32(KUART2_SYSC);
		__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
		SetClockState( EClkUart2_F, EClkOff );
		SetClockState( EClkUart2_I, EClkOff );
		}
	if( debugport != 2 )
		{
		r = AsspRegister::Read32(KUART3_SYSC);
		__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
		SetClockState( EClkUart3_F, EClkOff );
		SetClockState( EClkUart3_I, EClkOff );
		}

	// I2C KI2C1_SYSC
	r = AsspRegister::Read32(KI2C1_SYSC);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkI2c1_F, EClkOff );
	SetClockState( EClkI2c1_I, EClkOff );
	r = AsspRegister::Read32(KI2C2_SYSC);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkI2c2_F, EClkOff );
	SetClockState( EClkI2c2_I, EClkOff );
	r = AsspRegister::Read32(KI2C3_SYSC);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkI2c3_F, EClkOff );
	SetClockState( EClkI2c3_I, EClkOff );

	// GPT
	r = AsspRegister::Read32(KTI1OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt1_F, EClkOff );
	SetClockState( EClkGpt1_I, EClkOff );
	r = AsspRegister::Read32(KTI2OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt2_F, EClkOff );
	SetClockState( EClkGpt2_I, EClkOff );
	r = AsspRegister::Read32(KTI3OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt3_F, EClkOff );
	SetClockState( EClkGpt3_I, EClkOff );
	r = AsspRegister::Read32(KTI4OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt4_F, EClkOff );
	SetClockState( EClkGpt4_I, EClkOff );
	r = AsspRegister::Read32(KTI5OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt5_F, EClkOff );
	SetClockState( EClkGpt5_I, EClkOff );
	r = AsspRegister::Read32(KTI6OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt6_F, EClkOff );
	SetClockState( EClkGpt6_I, EClkOff );
	r = AsspRegister::Read32(KTI7OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt7_F, EClkOff );
	SetClockState( EClkGpt7_I, EClkOff );
	r = AsspRegister::Read32(KTI8OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt8_F, EClkOff );
	SetClockState( EClkGpt8_I, EClkOff );
	r = AsspRegister::Read32(KTI9OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt9_F, EClkOff );
	SetClockState( EClkGpt9_I, EClkOff );
	r = AsspRegister::Read32(KTI10OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt10_F, EClkOff );
	SetClockState( EClkGpt10_I, EClkOff );
	r = AsspRegister::Read32(KTI11OCP_CFG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkGpt11_F, EClkOff );
	SetClockState( EClkGpt11_I, EClkOff );

	// WDT
	r = AsspRegister::Read32(KWD2_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkWdt2_F, EClkOff );
	SetClockState( EClkWdt2_I, EClkOff );
	r = AsspRegister::Read32(KWD3_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	SetClockState( EClkWdt3_F, EClkOff );
	SetClockState( EClkWdt3_I, EClkOff );

	// GPIO
	/*
	r = AsspRegister::Read32(KGPIO1_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	_BitClearSet(KCM_FCLKEN_WKUP, 1 << 3, 0);
	_BitClearSet(KCM_ICLKEN_WKUP, 1 << 3, 0);

	//r = AsspRegister::Read32(KGPIO2_SYSCONFIG);
	//__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	//__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	//_BitClearSet(KCM_FCLKEN_PER, 1 << 13, 0);
	//_BitClearSet(KCM_ICLKEN_PER, 1 << 13, 0);
	r = AsspRegister::Read32(KGPIO3_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	_BitClearSet(KCM_FCLKEN_PER, 1 << 14, 0);
	_BitClearSet(KCM_ICLKEN_PER, 1 << 14, 0);
	r = AsspRegister::Read32(KGPIO4_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	_BitClearSet(KCM_FCLKEN_PER, 1 << 15, 0);
	_BitClearSet(KCM_ICLKEN_PER, 1 << 15, 0);
	r = AsspRegister::Read32(KGPIO5_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	_BitClearSet(KCM_FCLKEN_PER, 1 << 16, 0);
	_BitClearSet(KCM_ICLKEN_PER, 1 << 16, 0);
	r = AsspRegister::Read32(KGPIO6_SYSCONFIG);
	__NK_ASSERT_ALWAYS((r & 1 << 3) == 0);
	__NK_ASSERT_ALWAYS((r & 1 << 8) == 0);
	_BitClearSet(KCM_FCLKEN_PER, 1 << 17, 0);
	_BitClearSet(KCM_ICLKEN_PER, 1 << 17, 0);
	*/
	__SPIN_UNLOCK_IRQRESTORE(iLock, irq);
	}

} // end namespace Prcm


NONSHARABLE_CLASS( TPrcmInterruptDispatch ): public MInterruptDispatcher
	{
	public:
		TInt Init();
		virtual TInt Bind(TInt aId, TIsr anIsr, TAny* aPtr) ;
		virtual TInt Unbind(TInt aId);
		virtual TInt Enable(TInt aId);
		virtual TInt Disable(TInt aId);
		virtual TInt Clear(TInt aId);
		virtual TInt SetPriority(TInt aId, TInt aPriority);

	private:
		static void Spurious( TAny* aId );
		static void Dispatch( TAny* aParam );
	};

SInterruptHandler Handlers[ Prcm::KInterruptCount ];
TInt TheRootInterruptEnable = 0;
TSpinLock iIntLock(/*TSpinLock::EOrderGenericIrqLow0*/);
TPrcmInterruptDispatch TheIntDispatcher;

void TPrcmInterruptDispatch::Spurious( TAny* aId )
	{
	Kern::Fault( "PRCM:Spurious", (TInt)aId );
	}

void TPrcmInterruptDispatch::Dispatch( TAny* /*aParam*/ )
	{
	TUint32 status = AsspRegister::Read32( KPRM_IRQSTATUS_MPU )
					bitand AsspRegister::Read32( KPRM_IRQENABLE_MPU );

	for( TInt i = 0; (status) && (i < Prcm::KInterruptCount); ++i )
		{
		if( status bitand 1 )
			{
			(*Handlers[i].iIsr)( Handlers[i].iPtr );
			}
		status >>= 1;
		}
	}

TInt TPrcmInterruptDispatch::Init()
	{
	// Disable all interrupts
	AsspRegister::Write32( KPRM_IRQENABLE_MPU, 0 );
	AsspRegister::Write32( KPRM_IRQSTATUS_MPU, KSetAll );

	// Bind all to spurious handler
	for( TInt i = 0; i < Prcm::KInterruptCount; ++i )
		{
		Handlers[i].iIsr = TPrcmInterruptDispatch::Spurious;
		Handlers[i].iPtr = (TAny*)(i + (EIrqRangeBasePrcm << KIrqRangeIndexShift));
		}

	TInt r = Interrupt::Bind( EOmap3530_IRQ11_PRCM_MPU_IRQ, TPrcmInterruptDispatch::Dispatch, this );
	if( KErrNone == r )
		{
		Register( EIrqRangeBasePrcm );
		}
	return r;
	}

TInt TPrcmInterruptDispatch::Bind(TInt aId, TIsr aIsr, TAny* aPtr)
	{
	TUint id = aId bitand KIrqNumberMask;
	TInt r;

	if( id < Prcm::KInterruptCount )
		{
		if( Handlers[ id ].iIsr != TPrcmInterruptDispatch::Spurious )
			{
			r = KErrInUse;
			}
		else
			{
			Handlers[ id ].iIsr = aIsr;
			Handlers[ id ].iPtr = aPtr;
			r = KErrNone;
			}
		}
	else
		{
		r = KErrArgument;
		}
	return r;
	}

TInt TPrcmInterruptDispatch::Unbind(TInt aId)
	{
	TUint id = aId bitand KIrqNumberMask;
	TInt r;

	if( id < Prcm::KInterruptCount )
		{
		if( Handlers[ id ].iIsr == TPrcmInterruptDispatch::Spurious )
			{
			r = KErrGeneral;
			}
		else
			{
			Handlers[ id ].iIsr = TPrcmInterruptDispatch::Spurious;
			r = KErrNone;
			}
		}
	else
		{
		r = KErrArgument;
		}
	return r;
	}

TInt TPrcmInterruptDispatch::Enable(TInt aId)
	{
	TUint id = aId bitand KIrqNumberMask;

	if( id < Prcm::KInterruptCount )
		{
		TInt irq = __SPIN_LOCK_IRQSAVE(iIntLock);
		if( ++TheRootInterruptEnable == 1 )
			{
			Interrupt::Enable( EOmap3530_IRQ11_PRCM_MPU_IRQ );
			}
		Prcm::_BitClearSet( KPRM_IRQENABLE_MPU, KClearNone, 1 << id );
		__SPIN_UNLOCK_IRQRESTORE(iIntLock, irq);
		return KErrNone;
		}
	else
		{
		return KErrArgument;
		}
	}

TInt TPrcmInterruptDispatch::Disable(TInt aId)
	{
	TUint id = aId bitand KIrqNumberMask;

	if( id < Prcm::KInterruptCount )
		{
		TInt irq = __SPIN_LOCK_IRQSAVE(iIntLock);
		if( --TheRootInterruptEnable == 0 )
			{
			Interrupt::Disable( EOmap3530_IRQ11_PRCM_MPU_IRQ );
			}
		Prcm::_BitClearSet( KPRM_IRQENABLE_MPU, 1 << id, KSetNone );
		__SPIN_UNLOCK_IRQRESTORE(iIntLock, irq);
		return KErrNone;
		}
	else
		{
		return KErrArgument;
		}
	}

TInt TPrcmInterruptDispatch::Clear(TInt aId)
	{
	TUint id = aId bitand KIrqNumberMask;
	TInt r;

	if( id < Prcm::KInterruptCount )
		{
		AsspRegister::Write32( KPRM_IRQSTATUS_MPU, 1 << id );
		r = KErrNone;
		}
	else
		{
		r = KErrArgument;
		}
	return r;
	}

TInt TPrcmInterruptDispatch::SetPriority(TInt anId, TInt aPriority)
	{
	return KErrNotSupported;
	}



DECLARE_STANDARD_EXTENSION()
	{
	return TheIntDispatcher.Init();
	}






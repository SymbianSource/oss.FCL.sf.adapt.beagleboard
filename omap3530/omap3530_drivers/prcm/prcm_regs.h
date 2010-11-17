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
// \omap3530\omap3530_assp\prcm_regs.h.
// This file is part of the Beagle Base port
//

#ifndef __PRCM_REGS_H__
#define __PRCM_REGS_H__

#include "omap3530_hardware_base.h"

const TUint32 KCM_CLKSEL_CORE = Omap3530HwBase::TVirtual<0x48004A40>::Value;
const TUint32 KCM_CLKSEL_SGX = Omap3530HwBase::TVirtual<0x48004B40>::Value;
const TUint32 KCM_CLKSEL_WKUP = Omap3530HwBase::TVirtual<0x48004C40>::Value;
const TUint32 KCM_CLKSEL_DSS = Omap3530HwBase::TVirtual<0x48004E40>::Value;
const TUint32 KCM_CLKSEL_CAM = Omap3530HwBase::TVirtual<0x48004F40>::Value;
const TUint32 KCM_CLKSEL_PER = Omap3530HwBase::TVirtual<0x48005040>::Value;
const TUint32 KCM_CLKSEL1_EMU = Omap3530HwBase::TVirtual<0x48005140>::Value;
const TUint32 KCM_CLKSEL2_EMU = Omap3530HwBase::TVirtual<0x48005150>::Value;
const TUint32 KCM_CLKSEL3_EMU = Omap3530HwBase::TVirtual<0x48005154>::Value;
const TUint32 KCM_CLKSEL1_PLL = Omap3530HwBase::TVirtual<0x48004D40>::Value;
const TUint32 KCM_CLKSEL2_PLL = Omap3530HwBase::TVirtual<0x48004D44>::Value;
const TUint32 KCM_CLKSEL3_PLL = Omap3530HwBase::TVirtual<0x48004D48>::Value;
const TUint32 KCM_CLKSEL4_PLL = Omap3530HwBase::TVirtual<0x48004D4C>::Value;
const TUint32 KCM_CLKSEL5_PLL = Omap3530HwBase::TVirtual<0x48004D50>::Value;
const TUint32 KCM_CLKEN_PLL = Omap3530HwBase::TVirtual<0x48004D00>::Value;
const TUint32 KCM_FCLKEN_IVA2 = Omap3530HwBase::TVirtual<0x48004000>::Value;
const TUint32 KCM_FCLKEN_SGX = Omap3530HwBase::TVirtual<0x48004B00>::Value;
const TUint32 KCM_FCLKEN_WKUP = Omap3530HwBase::TVirtual<0x48004C00>::Value;
const TUint32 KCM_FCLKEN_DSS = Omap3530HwBase::TVirtual<0x48004E00>::Value;
const TUint32 KCM_FCLKEN_CAM = Omap3530HwBase::TVirtual<0x48004F00>::Value;
const TUint32 KCM_FCLKEN_PER = Omap3530HwBase::TVirtual<0x48005000>::Value;
const TUint32 KCM_FCLKEN_USBHOST = Omap3530HwBase::TVirtual<0x48005400>::Value;
const TUint32 KCM_FCLKEN1_CORE = Omap3530HwBase::TVirtual<0x48004A00>::Value;
const TUint32 KCM_FCLKEN3_CORE = Omap3530HwBase::TVirtual<0x48004A08>::Value;
const TUint32 KCM_ICLKEN1_CORE = Omap3530HwBase::TVirtual<0x48004A10>::Value;
const TUint32 KCM_ICLKEN2_CORE = Omap3530HwBase::TVirtual<0x48004A14>::Value;
const TUint32 KCM_ICLKEN3_CORE = Omap3530HwBase::TVirtual<0x48004A18>::Value;
const TUint32 KCM_ICLKEN_SGX = Omap3530HwBase::TVirtual<0x48004B10>::Value;
const TUint32 KCM_ICLKEN_WKUP = Omap3530HwBase::TVirtual<0x48004C10>::Value;
const TUint32 KCM_ICLKEN_DSS = Omap3530HwBase::TVirtual<0x48004E10>::Value;
const TUint32 KCM_ICLKEN_CAM = Omap3530HwBase::TVirtual<0x48004F10>::Value;
const TUint32 KCM_ICLKEN_PER = Omap3530HwBase::TVirtual<0x48005010>::Value;
const TUint32 KCM_ICLKEN_USBHOST = Omap3530HwBase::TVirtual<0x48005410>::Value;
const TUint32 KCM_AUTOIDLE1_CORE = Omap3530HwBase::TVirtual<0x48004A30>::Value;
const TUint32 KCM_AUTOIDLE2_CORE = Omap3530HwBase::TVirtual<0x48004A34>::Value;
const TUint32 KCM_AUTOIDLE3_CORE = Omap3530HwBase::TVirtual<0x48004A38>::Value;
const TUint32 KCM_AUTOIDLE_WKUP = Omap3530HwBase::TVirtual<0x48004C30>::Value;
const TUint32 KCM_AUTOIDLE_PLL = Omap3530HwBase::TVirtual<0x48004D30>::Value;
const TUint32 KCM_AUTOIDLE2_PLL = Omap3530HwBase::TVirtual<0x48004D34>::Value;
const TUint32 KCM_AUTOIDLE_DSS = Omap3530HwBase::TVirtual<0x48004E30>::Value;
const TUint32 KCM_AUTOIDLE_CAM = Omap3530HwBase::TVirtual<0x48004F30>::Value;
const TUint32 KCM_AUTOIDLE_PER = Omap3530HwBase::TVirtual<0x48005030>::Value;
const TUint32 KCM_AUTOIDLE_USBHOST = Omap3530HwBase::TVirtual<0x48005430>::Value;
const TUint32 KCM_IDLEST1_CORE = Omap3530HwBase::TVirtual<0x48004A20>::Value;
const TUint32 KCM_IDLEST2_CORE = Omap3530HwBase::TVirtual<0x48004A24>::Value;
const TUint32 KCM_IDLEST3_CORE = Omap3530HwBase::TVirtual<0x48004A28>::Value;
const TUint32 KCM_IDLEST_IVA2 = Omap3530HwBase::TVirtual<0x48004024>::Value;
const TUint32 KCM_IDLEST_MPU = Omap3530HwBase::TVirtual<0x48004920>::Value;
const TUint32 KCM_IDLEST_SGX = Omap3530HwBase::TVirtual<0x48004B20>::Value;
const TUint32 KCM_IDLEST_WKUP = Omap3530HwBase::TVirtual<0x48004C20>::Value;
const TUint32 KCM_IDLEST_CKGEN = Omap3530HwBase::TVirtual<0x48004D20>::Value;
const TUint32 KCM_IDLEST2_CKGEN = Omap3530HwBase::TVirtual<0x48004D24>::Value;
const TUint32 KCM_IDLEST_DSS = Omap3530HwBase::TVirtual<0x48004E20>::Value;
const TUint32 KCM_IDLEST_CAM = Omap3530HwBase::TVirtual<0x48004F20>::Value;
const TUint32 KCM_IDLEST_PER = Omap3530HwBase::TVirtual<0x48005020>::Value;
const TUint32 KCM_IDLEST_NEON = Omap3530HwBase::TVirtual<0x48005320>::Value;
const TUint32 KCM_IDLEST_USBHOST = Omap3530HwBase::TVirtual<0x48005420>::Value;
const TUint32 KCM_SLEEPDEP_SGX = Omap3530HwBase::TVirtual<0x48004B44>::Value;
const TUint32 KCM_SLEEPDEP_DSS = Omap3530HwBase::TVirtual<0x48004E44>::Value;
const TUint32 KCM_SLEEPDEP_CAM = Omap3530HwBase::TVirtual<0x48004F44>::Value;
const TUint32 KCM_SLEEPDEP_PER = Omap3530HwBase::TVirtual<0x48005044>::Value;
const TUint32 KCM_SLEEPDEP_USBHOST = Omap3530HwBase::TVirtual<0x48005444>::Value;
const TUint32 KCM_CLKSTCTRL_IVA2 = Omap3530HwBase::TVirtual<0x48004048>::Value;
const TUint32 KCM_CLKSTCTRL_MPU = Omap3530HwBase::TVirtual<0x48004948>::Value;
const TUint32 KCM_CLKSTCTRL_CORE = Omap3530HwBase::TVirtual<0x48004A48>::Value;
const TUint32 KCM_CLKSTCTRL_SGX = Omap3530HwBase::TVirtual<0x48004B48>::Value;
const TUint32 KCM_CLKSTCTRL_DSS = Omap3530HwBase::TVirtual<0x48004E48>::Value;
const TUint32 KCM_CLKSTCTRL_CAM = Omap3530HwBase::TVirtual<0x48004F48>::Value;
const TUint32 KCM_CLKSTCTRL_PER = Omap3530HwBase::TVirtual<0x48005048>::Value;
const TUint32 KCM_CLKSTCTRL_EMU = Omap3530HwBase::TVirtual<0x48005148>::Value;
const TUint32 KCM_CLKSTCTRL_NEON = Omap3530HwBase::TVirtual<0x48005348>::Value;
const TUint32 KCM_CLKSTCTRL_USBHOST = Omap3530HwBase::TVirtual<0x48005448>::Value;
const TUint32 KCM_CLKSTST_IVA2 = Omap3530HwBase::TVirtual<0x4800404C>::Value;
const TUint32 KCM_CLKSTST_MPU = Omap3530HwBase::TVirtual<0x4800494C>::Value;
const TUint32 KCM_CLKSTST_CORE = Omap3530HwBase::TVirtual<0x48004A4C>::Value;
const TUint32 KCM_CLKSTST_SGX = Omap3530HwBase::TVirtual<0x48004B4C>::Value;
const TUint32 KCM_CLKSTST_DSS = Omap3530HwBase::TVirtual<0x48004E4C>::Value;
const TUint32 KCM_CLKSTST_CAM = Omap3530HwBase::TVirtual<0x48004F4C>::Value;
const TUint32 KCM_CLKSTST_PER = Omap3530HwBase::TVirtual<0x4800504C>::Value;
const TUint32 KCM_CLKSTST_EMU = Omap3530HwBase::TVirtual<0x4800514C>::Value;
const TUint32 KCM_CLKSTST_USBHOST = Omap3530HwBase::TVirtual<0x4800544C>::Value;
const TUint32 KCM_CLKSEL1_PLL_IVA2 = Omap3530HwBase::TVirtual<0x48004040>::Value;
const TUint32 KCM_CLKSEL2_PLL_IVA2 = Omap3530HwBase::TVirtual<0x48004044>::Value;
const TUint32 KCM_CLKSEL1_PLL_MPU = Omap3530HwBase::TVirtual<0x48004940>::Value;
const TUint32 KCM_CLKSEL2_PLL_MPU = Omap3530HwBase::TVirtual<0x48004944>::Value;
const TUint32 KCM_CLKEN_PLL_IVA2 = Omap3530HwBase::TVirtual<0x48004004>::Value;
const TUint32 KCM_CLKEN_PLL_MPU = Omap3530HwBase::TVirtual<0x48004904>::Value;
const TUint32 KCM_CLKEN2_PLL = Omap3530HwBase::TVirtual<0x48004D04>::Value;
const TUint32 KCM_AUTOIDLE_PLL_IVA2 = Omap3530HwBase::TVirtual<0x48004034>::Value;
const TUint32 KCM_AUTOIDLE_PLL_MPU = Omap3530HwBase::TVirtual<0x48004934>::Value;
const TUint32 KCM_IDLEST_PLL_IVA2 = Omap3530HwBase::TVirtual<0x48004024>::Value;
const TUint32 KCM_IDLEST_PLL_MPU = Omap3530HwBase::TVirtual<0x48004924>::Value;
const TUint32 KCM_REVISION = Omap3530HwBase::TVirtual<0x48004800>::Value;
const TUint32 KCM_SYSCONFIG = Omap3530HwBase::TVirtual<0x48004810>::Value;
const TUint32 KCM_CLKOUT_CTRL = Omap3530HwBase::TVirtual<0x48004D70>::Value;
const TUint32 KCM_POLCTRL = Omap3530HwBase::TVirtual<0x4800529C>::Value;
const TUint32 KRM_RSTCTRL_IVA2 = Omap3530HwBase::TVirtual<0x48306050>::Value;
const TUint32 KRM_RSTST_IVA2 = Omap3530HwBase::TVirtual<0x48306058>::Value;
const TUint32 KPM_WKDEP_IVA2 = Omap3530HwBase::TVirtual<0x483060C8>::Value;
const TUint32 KPM_PWSTCTRL_IVA2 = Omap3530HwBase::TVirtual<0x483060E0>::Value;
const TUint32 KPM_PWSTST_IVA2 = Omap3530HwBase::TVirtual<0x483060E4>::Value;
const TUint32 KPM_PREPWSTST_IVA2 = Omap3530HwBase::TVirtual<0x483060E8>::Value;
const TUint32 KPRM_IRQSTATUS_IVA2 = Omap3530HwBase::TVirtual<0x483060F8>::Value;
const TUint32 KPRM_IRQENABLE_IVA2 = Omap3530HwBase::TVirtual<0x483060FC>::Value;
const TUint32 KPRM_REVISION = Omap3530HwBase::TVirtual<0x48306804>::Value;
const TUint32 KPRM_SYSCONFIG = Omap3530HwBase::TVirtual<0x48306814>::Value;
const TUint32 KPRM_IRQSTATUS_MPU = Omap3530HwBase::TVirtual<0x48306818>::Value;
const TUint32 KPRM_IRQENABLE_MPU = Omap3530HwBase::TVirtual<0x4830681C>::Value;
const TUint32 KRM_RSTST_MPU = Omap3530HwBase::TVirtual<0x48306958>::Value;
const TUint32 KPM_WKDEP_MPU = Omap3530HwBase::TVirtual<0x483069C8>::Value;
const TUint32 KPM_EVGENCTRL_MPU = Omap3530HwBase::TVirtual<0x483069D4>::Value;
const TUint32 KPM_EVGENONTIM_MPU = Omap3530HwBase::TVirtual<0x483069D8>::Value;
const TUint32 KPM_EVGENOFFTIM_MPU = Omap3530HwBase::TVirtual<0x483069DC>::Value;
const TUint32 KPM_PWSTCTRL_MPU = Omap3530HwBase::TVirtual<0x483069E0>::Value;
const TUint32 KPM_PWSTST_MPU = Omap3530HwBase::TVirtual<0x483069E4>::Value;
const TUint32 KPM_PREPWSTST_MPU = Omap3530HwBase::TVirtual<0x483069E8>::Value;
const TUint32 KRM_RSTST_CORE = Omap3530HwBase::TVirtual<0x48306A58>::Value;
const TUint32 KPM_WKEN1_CORE = Omap3530HwBase::TVirtual<0x48306AA0>::Value;
const TUint32 KPM_MPUGRPSEL1_CORE = Omap3530HwBase::TVirtual<0x48306AA4>::Value;
const TUint32 KPM_IVA2GRPSEL1_CORE = Omap3530HwBase::TVirtual<0x48306AA8>::Value;
const TUint32 KPM_WKST1_CORE = Omap3530HwBase::TVirtual<0x48306AB0>::Value;
const TUint32 KPM_WKST3_CORE = Omap3530HwBase::TVirtual<0x48306AB8>::Value;
const TUint32 KPM_PWSTCTRL_CORE = Omap3530HwBase::TVirtual<0x48306AE0>::Value;
const TUint32 KPM_PWSTST_CORE = Omap3530HwBase::TVirtual<0x48306AE4>::Value;
const TUint32 KPM_PREPWSTST_CORE = Omap3530HwBase::TVirtual<0x48306AE8>::Value;
const TUint32 KPM_WKEN3_CORE = Omap3530HwBase::TVirtual<0x48306AF0>::Value;
const TUint32 KPM_IVA2GRPSEL3_CORE = Omap3530HwBase::TVirtual<0x48306AF4>::Value;
const TUint32 KPM_MPUGRPSEL3_CORE = Omap3530HwBase::TVirtual<0x48306AF8>::Value;
const TUint32 KRM_RSTST_SGX = Omap3530HwBase::TVirtual<0x48306B58>::Value;
const TUint32 KPM_WKDEP_SGX = Omap3530HwBase::TVirtual<0x48306BC8>::Value;
const TUint32 KPM_PWSTCTRL_SGX = Omap3530HwBase::TVirtual<0x48306BE0>::Value;
const TUint32 KPM_PWSTST_SGX = Omap3530HwBase::TVirtual<0x48306BE4>::Value;
const TUint32 KPM_PREPWSTST_SGX = Omap3530HwBase::TVirtual<0x48306BE8>::Value;
const TUint32 KPM_WKEN_WKUP = Omap3530HwBase::TVirtual<0x48306CA0>::Value;
const TUint32 KPM_MPUGRPSEL_WKUP = Omap3530HwBase::TVirtual<0x48306CA4>::Value;
const TUint32 KPM_IVA2GRPSEL_WKUP = Omap3530HwBase::TVirtual<0x48306CA8>::Value;
const TUint32 KPM_WKST_WKUP = Omap3530HwBase::TVirtual<0x48306CB0>::Value;
const TUint32 KPRM_CLKSEL = Omap3530HwBase::TVirtual<0x48306D40>::Value;
const TUint32 KPRM_CLKOUT_CTRL = Omap3530HwBase::TVirtual<0x48306D70>::Value;
const TUint32 KRM_RSTST_DSS = Omap3530HwBase::TVirtual<0x48306E58>::Value;
const TUint32 KPM_WKEN_DSS = Omap3530HwBase::TVirtual<0x48306EA0>::Value;
const TUint32 KPM_WKDEP_DSS = Omap3530HwBase::TVirtual<0x48306EC8>::Value;
const TUint32 KPM_PWSTCTRL_DSS = Omap3530HwBase::TVirtual<0x48306EE0>::Value;
const TUint32 KPM_PWSTST_DSS = Omap3530HwBase::TVirtual<0x48306EE4>::Value;
const TUint32 KPM_PREPWSTST_DSS = Omap3530HwBase::TVirtual<0x48306EE8>::Value;
const TUint32 KRM_RSTST_CAM = Omap3530HwBase::TVirtual<0x48306F58>::Value;
const TUint32 KPM_WKDEP_CAM = Omap3530HwBase::TVirtual<0x48306FC8>::Value;
const TUint32 KPM_PWSTCTRL_CAM = Omap3530HwBase::TVirtual<0x48306FE0>::Value;
const TUint32 KPM_PWSTST_CAM = Omap3530HwBase::TVirtual<0x48306FE4>::Value;
const TUint32 KPM_PREPWSTST_CAM = Omap3530HwBase::TVirtual<0x48306FE8>::Value;
const TUint32 KRM_RSTST_PER = Omap3530HwBase::TVirtual<0x48307058>::Value;
const TUint32 KPM_WKEN_PER = Omap3530HwBase::TVirtual<0x483070A0>::Value;
const TUint32 KPM_MPUGRPSEL_PER = Omap3530HwBase::TVirtual<0x483070A4>::Value;
const TUint32 KPM_IVA2GRPSEL_PER = Omap3530HwBase::TVirtual<0x483070A8>::Value;
const TUint32 KPM_WKST_PER = Omap3530HwBase::TVirtual<0x483070B0>::Value;
const TUint32 KPM_WKDEP_PER = Omap3530HwBase::TVirtual<0x483070C8>::Value;
const TUint32 KPM_PWSTCTRL_PER = Omap3530HwBase::TVirtual<0x483070E0>::Value;
const TUint32 KPM_PWSTST_PER = Omap3530HwBase::TVirtual<0x483070E4>::Value;
const TUint32 KPM_PREPWSTST_PER = Omap3530HwBase::TVirtual<0x483070E8>::Value;
const TUint32 KRM_RSTST_EMU = Omap3530HwBase::TVirtual<0x48307158>::Value;
const TUint32 KPM_PWSTST_EMU = Omap3530HwBase::TVirtual<0x483071E4>::Value;
const TUint32 KPRM_VC_SMPS_SA = Omap3530HwBase::TVirtual<0x48307220>::Value;
const TUint32 KPRM_VC_SMPS_VOL_RA = Omap3530HwBase::TVirtual<0x48307224>::Value;
const TUint32 KPRM_VC_SMPS_CMD_RA = Omap3530HwBase::TVirtual<0x48307228>::Value;
const TUint32 KPRM_VC_CMD_VAL_0 = Omap3530HwBase::TVirtual<0x4830722C>::Value;
const TUint32 KPRM_VC_CMD_VAL_1 = Omap3530HwBase::TVirtual<0x48307230>::Value;
const TUint32 KPRM_VC_CH_CONF = Omap3530HwBase::TVirtual<0x48307234>::Value;
const TUint32 KPRM_VC_I2C_CFG = Omap3530HwBase::TVirtual<0x48307238>::Value;
const TUint32 KPRM_VC_BYPASS_VAL = Omap3530HwBase::TVirtual<0x4830723C>::Value;
const TUint32 KPRM_RSTCTRL = Omap3530HwBase::TVirtual<0x48307250>::Value;
const TUint32 KPRM_RSTTIME = Omap3530HwBase::TVirtual<0x48307254>::Value;
const TUint32 KPRM_RSTST = Omap3530HwBase::TVirtual<0x48307258>::Value;
const TUint32 KPRM_VOLTCTRL = Omap3530HwBase::TVirtual<0x48307260>::Value;
const TUint32 KPRM_SRAM_PCHARGE = Omap3530HwBase::TVirtual<0x48307264>::Value;
const TUint32 KPRM_CLKSRC_CTRL = Omap3530HwBase::TVirtual<0x48307270>::Value;
const TUint32 KPRM_OBS = Omap3530HwBase::TVirtual<0x48307280>::Value;
const TUint32 KPRM_VOLTSETUP1 = Omap3530HwBase::TVirtual<0x48307290>::Value;
const TUint32 KPRM_VOLTOFFSET = Omap3530HwBase::TVirtual<0x48307294>::Value;
const TUint32 KPRM_CLKSETUP = Omap3530HwBase::TVirtual<0x48307298>::Value;
const TUint32 KPRM_POLCTRL = Omap3530HwBase::TVirtual<0x4830729C>::Value;
const TUint32 KPRM_VOLTSETUP2 = Omap3530HwBase::TVirtual<0x483072A0>::Value;
const TUint32 KRM_RSTST_NEON = Omap3530HwBase::TVirtual<0x48307358>::Value;
const TUint32 KPM_WKDEP_NEON = Omap3530HwBase::TVirtual<0x483073C8>::Value;
const TUint32 KPM_PWSTCTRL_NEON = Omap3530HwBase::TVirtual<0x483073E0>::Value;
const TUint32 KPM_PWSTST_NEON = Omap3530HwBase::TVirtual<0x483073E4>::Value;
const TUint32 KPM_PREPWSTST_NEON = Omap3530HwBase::TVirtual<0x483073E8>::Value;
const TUint32 KRM_RSTST_USBHOST = Omap3530HwBase::TVirtual<0x48307458>::Value;
const TUint32 KPM_WKEN_USBHOST = Omap3530HwBase::TVirtual<0x483074A0>::Value;
const TUint32 KPM_MPUGRPSEL_USBHOST = Omap3530HwBase::TVirtual<0x483074A4>::Value;
const TUint32 KPM_IVA2GRPSEL_USBHOST = Omap3530HwBase::TVirtual<0x483074A8>::Value;
const TUint32 KPM_WKST_USBHOST = Omap3530HwBase::TVirtual<0x483074B0>::Value;
const TUint32 KPM_WKDEP_USBHOST = Omap3530HwBase::TVirtual<0x483074C8>::Value;
const TUint32 KPM_PWSTCTRL_USBHOST = Omap3530HwBase::TVirtual<0x483074E0>::Value;
const TUint32 KPM_PWSTST_USBHOST = Omap3530HwBase::TVirtual<0x483074E4>::Value;
const TUint32 KPM_PREPWSTST_USBHOST = Omap3530HwBase::TVirtual<0x483074E8>::Value;
const TUint32 KMAILBOX_SYSCONFIG = Omap3530HwBase::TVirtual<0x48094010>::Value;
const TUint32 KCONTROL_SYSCONFIG = Omap3530HwBase::TVirtual<0x48002010>::Value;
const TUint32 KMMU1_SYSCONFIG = Omap3530HwBase::TVirtual<0x480BD410>::Value;
const TUint32 KMMU2_SYSCONFIG = Omap3530HwBase::TVirtual<0x5D000010>::Value;
const TUint32 KDMA4_OCP_SYSCONFIG = Omap3530HwBase::TVirtual<0x4805602C>::Value;
const TUint32 KINTCPS_SYSCONFIG = Omap3530HwBase::TVirtual<0x48200010>::Value;
const TUint32 KGPMC_SYSCONFIG = Omap3530HwBase::TVirtual<0x6E000010>::Value;
const TUint32 KSMS_SYSCONFIG = Omap3530HwBase::TVirtual<0x6C000010>::Value;
const TUint32 KSDRC_SYSCONFIG = Omap3530HwBase::TVirtual<0x6D000010>::Value;
const TUint32 KISP_SYSCONFIG = Omap3530HwBase::TVirtual<0x480BC004>::Value;
const TUint32 KCBUFF_SYSCONFIG = Omap3530HwBase::TVirtual<0x480BC110>::Value;
const TUint32 KSYSC_SYSCONFIG = Omap3530HwBase::TVirtual<0x01C20008>::Value;
const TUint32 KWUGEN_SYSCONFIG = Omap3530HwBase::TVirtual<0x01C21008>::Value;
const TUint32 KDSS_SYSCONFIG = Omap3530HwBase::TVirtual<0x48050010>::Value;
const TUint32 KDISPC_SYSCONFIG = Omap3530HwBase::TVirtual<0x48050410>::Value;
const TUint32 KRFBI_SYSCONFIG = Omap3530HwBase::TVirtual<0x48050810>::Value;
const TUint32 KDSI_SYSCONFIG = Omap3530HwBase::TVirtual<0x4804FC10>::Value;
const TUint32 KWD1_SYSCONFIG = Omap3530HwBase::TVirtual<0x4830C010>::Value;
const TUint32 KWD2_SYSCONFIG = Omap3530HwBase::TVirtual<0x48314010>::Value;
const TUint32 KWD3_SYSCONFIG = Omap3530HwBase::TVirtual<0x49030010>::Value;
const TUint32 K32KSYNCNT_SYSCONFIG = Omap3530HwBase::TVirtual<0x48320004>::Value;
const TUint32 KI2C3_SYSC = Omap3530HwBase::TVirtual<0x48060020>::Value;
const TUint32 KI2C1_SYSC = Omap3530HwBase::TVirtual<0x48070020>::Value;
const TUint32 KI2C2_SYSC = Omap3530HwBase::TVirtual<0x48072020>::Value;
const TUint32 KMCSPI1_SYSCONFIG = Omap3530HwBase::TVirtual<0x48098010>::Value;
const TUint32 KMCSPI2_SYSCONFIG = Omap3530HwBase::TVirtual<0x4809A010>::Value;
const TUint32 KMCSPI3_SYSCONFIG = Omap3530HwBase::TVirtual<0x480B8010>::Value;
const TUint32 KMCSPI4_SYSCONFIG = Omap3530HwBase::TVirtual<0x480BA010>::Value;
const TUint32 KHDQ_SYSCONFIG = Omap3530HwBase::TVirtual<0x480B2014>::Value;
const TUint32 KMCBSPLP1_SYSCONFIG = Omap3530HwBase::TVirtual<0x4807408C>::Value;
const TUint32 KMCBSPLP5_SYSCONFIG = Omap3530HwBase::TVirtual<0x4809608C>::Value;
const TUint32 KMCBSPLP2_SYSCONFIG = Omap3530HwBase::TVirtual<0x4902208C>::Value;
const TUint32 KMCBSPLP3_SYSCONFIG = Omap3530HwBase::TVirtual<0x4902408C>::Value;
const TUint32 KMCBSPLP4_SYSCONFIG = Omap3530HwBase::TVirtual<0x4902608C>::Value;
const TUint32 KST2_SYSCONFIG = Omap3530HwBase::TVirtual<0x49028010>::Value;
const TUint32 KST3_SYSCONFIG = Omap3530HwBase::TVirtual<0x4902A010>::Value;
const TUint32 KMMCHS1_SYSCONFIG = Omap3530HwBase::TVirtual<0x4809C010>::Value;
const TUint32 KMMCHS3_SYSCONFIG = Omap3530HwBase::TVirtual<0x480AD010>::Value;
const TUint32 KMMCHS2_SYSCONFIG = Omap3530HwBase::TVirtual<0x480B4010>::Value;
const TUint32 KOTG_SYSCONFIG = Omap3530HwBase::TVirtual<0x480AB404>::Value;
const TUint32 KUSBTLL_SYSCONFIG = Omap3530HwBase::TVirtual<0x48062010>::Value;
const TUint32 KUHH_SYSCONFIG = Omap3530HwBase::TVirtual<0x48064010>::Value;
const TUint32 KGPIO1_SYSCONFIG = Omap3530HwBase::TVirtual<0x48310010>::Value;
const TUint32 KGPIO2_SYSCONFIG = Omap3530HwBase::TVirtual<0x49050010>::Value;
const TUint32 KGPIO3_SYSCONFIG = Omap3530HwBase::TVirtual<0x49052010>::Value;
const TUint32 KGPIO4_SYSCONFIG = Omap3530HwBase::TVirtual<0x49054010>::Value;
const TUint32 KGPIO5_SYSCONFIG = Omap3530HwBase::TVirtual<0x49056010>::Value;
const TUint32 KGPIO6_SYSCONFIG = Omap3530HwBase::TVirtual<0x49058010>::Value;
const TUint32 KUART1_SYSC = Omap3530HwBase::TVirtual<0x4806A050>::Value;
const TUint32 KUART2_SYSC = Omap3530HwBase::TVirtual<0x4806C050>::Value;
const TUint32 KUART3_SYSC = Omap3530HwBase::TVirtual<0x49020050>::Value;
const TUint32 KTI1OCP_CFG = Omap3530HwBase::TVirtual<0x48318010>::Value;
const TUint32 KTI2OCP_CFG = Omap3530HwBase::TVirtual<0x49032010>::Value;
const TUint32 KTI3OCP_CFG = Omap3530HwBase::TVirtual<0x49034010>::Value;
const TUint32 KTI4OCP_CFG = Omap3530HwBase::TVirtual<0x49036010>::Value;
const TUint32 KTI5OCP_CFG = Omap3530HwBase::TVirtual<0x49038010>::Value;
const TUint32 KTI6OCP_CFG = Omap3530HwBase::TVirtual<0x4903A010>::Value;
const TUint32 KTI7OCP_CFG = Omap3530HwBase::TVirtual<0x4903C010>::Value;
const TUint32 KTI8OCP_CFG = Omap3530HwBase::TVirtual<0x4903E010>::Value;
const TUint32 KTI9OCP_CFG = Omap3530HwBase::TVirtual<0x49040010>::Value;
const TUint32 KTI10OCP_CFG = Omap3530HwBase::TVirtual<0x48086010>::Value;
const TUint32 KTI11OCP_CFG = Omap3530HwBase::TVirtual<0x48088010>::Value;
const TUint32 KTI12OCP_CFG = Omap3530HwBase::TVirtual<0x48304010>::Value;


#endif

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
// omap3530/omap3530_drivers/spi/omap3530_spi.inl
//
// This file contains definitions to internal SPI implementation.
// It is not intended to be exported - SPI registers must not be modified from outside of
// the driver!
//


// This sets the CS line to inactive mode (Specify aActiveMode as appropriate for configuration)
// The CS pin will be put back to the opposite mode - using GPIO..
inline void SetCsInactive(TInt aModule, TInt aChannel, TSpiSsPinMode aActiveMode)
	{
	//__ASSERT_DEBUG(aModule < KMaxSpiChannelsPerModule, Kern::Fault("omap3530_spi.inl, line: ", __LINE__)); // aChannel > module channels
	// set the pin to the opposite to the currently active CS mode..
	TUint16 csPinOptions = aActiveMode == ESpiCSPinActiveLow ? KCsPinModeUp : KCsPinModeDown;
	const TPinConfig& csConf = ModulePinConfig[aModule].iCs[aChannel];
	__ASSERT_DEBUG(csConf.iAddress, Kern::Fault("omap3530_spi.inl, line: ", __LINE__)); // aChannel > module channels

	// now switch the pin mode..
	SCM::SetPadConfig(csConf.iAddress, csConf.iMswLsw, SCM::EMode4 | csPinOptions); // always go to mode 4 (gpio)
	}

void SetCsActive(TInt aModule, TInt aChannel, TSpiSsPinMode aActiveMode)
	{
	//__ASSERT_DEBUG(aModule < KMaxSpiChannelsPerModule, Kern::Fault("omap3530_spi.inl, line: ", __LINE__)); // aChannel > module channels
	TUint16 csPinOptions = aActiveMode == ESpiCSPinActiveLow ? KCsPinModeUp : KCsPinModeDown;
	const TPinConfig &csConf = ModulePinConfig[aModule].iCs[aChannel];
	__ASSERT_DEBUG(csConf.iAddress, Kern::Fault("omap3530_spi.inl, line: ", __LINE__)); // aChannel > module channels

	// now switch the pin mode back to the SPI
	SCM::SetPadConfig(csConf.iAddress, csConf.iMswLsw, csConf.iFlags | csPinOptions); // revert to intended mode
	}


// Setup pad function for SPI pins..
void SetupSpiPins(TUint aSpiModule)
	{
	//__ASSERT_DEBUG(aModule < KMaxSpiChannelsPerModule, Kern::Fault("omap3530_spi.inl, line: ", __LINE__)); // aChannel > module channels
	const TSpiPinConfig& pinCnf = ModulePinConfig[aSpiModule];
	SCM::SetPadConfig(pinCnf.iClk.iAddress, pinCnf.iClk.iMswLsw, pinCnf.iClk.iFlags);
	SCM::SetPadConfig(pinCnf.iSimo.iAddress, pinCnf.iSimo.iMswLsw, pinCnf.iSimo.iFlags);
	SCM::SetPadConfig(pinCnf.iSomi.iAddress, pinCnf.iSomi.iMswLsw, pinCnf.iSomi.iFlags);
	// Cs pins are set dynamically during operations.
	}

// helper function - returns appropriate value for the register for a given mode
inline TUint32 SpiClkMode(TSpiClkMode aClkMode)
	{
	// (POL) (PHA)
	//	0     0     Mode 0: spim_clk is active high and sampling occurs on the rising edge.
	//	0     1     Mode 1: spim_clk is active high and sampling occurs on the falling edge.
	//	1     0     Mode 2: spim_clk is active low and sampling occurs on the falling edge.
	//	1     1     Mode 3: spim_clk is active low and sampling occurs on the rising edge.

	TUint val = 0;
	switch(aClkMode)
		{
		//case ESpiPolarityLowRisingEdge:	// Active high, odd edges
			/*val |= MCSPI_CHxCONF_POL;*/ // 0 (not set)
			/*val |= MCSPI_CHxCONF_PHA;*/ // 0 (not set)
			//break; // commented out - it's only for  reference - there's nothing to change

		case ESpiPolarityLowFallingEdge: // Active high, even edges
			/*val |= MCSPI_CHxCONF_POL;*/ // 0 (not set)
			val |= MCSPI_CHxCONF_PHA;     // 1
			break;

		case ESpiPolarityHighFallingEdge: // Active low,  odd edges
			val |= MCSPI_CHxCONF_POL;     // 1
			/*val |= MCSPI_CHxCONF_PHA;*/ // 0 (not set)
			break;

		case ESpiPolarityHighRisingEdge: // Active low,  even edges
			val |= MCSPI_CHxCONF_POL; // 1
			val |= MCSPI_CHxCONF_PHA; // 1
			break;
		}
	return val;
	}

// helper function - returns appropriate value for the register for a given frequency (or error->if not found)
inline TInt SpiClkValue(TInt aClkSpeedHz)
	{
	for (TInt val = 0; val < 0xD; val++) // only loop through all possible values..
		{
		if(MCSPI_K48MHz >> val == aClkSpeedHz)
			{
			return (val << 2); // return value ready for the register
			}
		}
	return KErrNotFound;
	}

inline TInt SpiWordWidth(TSpiWordWidth aWidth)
	{
	TInt val = 0;
	switch(aWidth)
		{
		case ESpiWordWidth_8:  val |= MCSPI_CHxCONF_WL(8); break;
		case ESpiWordWidth_10: val |= MCSPI_CHxCONF_WL(10); break;
		case ESpiWordWidth_12: val |= MCSPI_CHxCONF_WL(12); break;
		case ESpiWordWidth_16: val |= MCSPI_CHxCONF_WL(16); break;
//		case ESpiWordWidth_32: val |= MCSPI_CHxCONF_WL(32); break; // TODO uncomment when fix for Bug 3665 is released
		}
	return val;
	}


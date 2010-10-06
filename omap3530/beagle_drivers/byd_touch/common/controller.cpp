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
// omap3530/beagle_drivers/byd_touch/common/controller.cpp
//
#include <drivers/iic.h>
#include <drivers/iic_channel.h>
#include <assp/omap3530_assp/omap3530_gpio.h>
#include "controller.h"

//#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
#define LOG_FUNCTION_CALL Kern::Printf("%s()", __FUNCTION__)
#else
#define LOG_FUNCTION_CALL
#endif

TTouchControllerInterface::TTouchControllerInterface() :
	iSpiTransactionHeader(KHeader),
	iSpiTxTransfer(TIicBusTransfer::EMasterWrite, KBufGranulatity, &iSpiWriteBuffer),
	iSpiRxTransfer(TIicBusTransfer::EMasterRead, KBufGranulatity, &iSpiReadBuffer),
	iSpiTransaction(&iSpiTransactionHeader, &iSpiTxTransfer)
	{
	// after all above - make the transaction full duplex using the Rx buffer
	iSpiTransaction.SetFullDuplexTrans(&iSpiRxTransfer);

	// set buffer length only once..
	iSpiWriteBuffer.SetLength(iSpiWriteBuffer.MaxLength());
	iSpiReadBuffer.SetLength(iSpiReadBuffer.MaxLength());

	// and set SPI addressing / bus configuration
	iSpiBusId = 0;
	SET_BUS_TYPE(iSpiBusId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(iSpiBusId, KSpiModule);
	SET_SLAVE_ADDR(iSpiBusId, KSpiSlaveAddr);
	}

TInt TTouchControllerInterface::Read(TUint8 aRegAddress, TUint8& aValue)
	{
	LOG_FUNCTION_CALL;
	iSpiWriteBuffer[0] = KReadCommand;
	iSpiWriteBuffer[1] = aRegAddress;
	iSpiWriteBuffer[2] = 0; // TODO - might not be needed..
#ifdef VERBOSE_DEBUG
	for (int i = 0; i <KSpiPacketLength; i++)
		Kern::Printf("R0[i]: %d", iSpiReadBuffer[1]);
#endif
	TInt r = IicBus::QueueTransaction(iSpiBusId, &iSpiTransaction);
	if(r == KErrNone)
		{
		aValue = iSpiReadBuffer[2];
		}
#ifdef VERBOSE_DEBUG
	for (int i = 0; i <KSpiPacketLength; i++)
		Kern::Printf("R1[i]: %d", iSpiReadBuffer[1]);
#endif
	return r;
	}

TInt TTouchControllerInterface::Write(TUint8 aRegAddress, TUint8 aValue)
	{
	LOG_FUNCTION_CALL;
	iSpiWriteBuffer[0] = KWriteCommand;
	iSpiWriteBuffer[1] = aRegAddress;
	iSpiWriteBuffer[2] = aValue;

#ifdef VERBOSE_DEBUG
	for (int i = 0; i < 3; i++)
		Kern::Printf("WR0[i]: %d", iSpiWriteBuffer[i]);

	for (int i = 0; i <KSpiPacketLength; i++)
		Kern::Printf("W0[i]: %d", iSpiReadBuffer[1]);
#endif
	TInt r = IicBus::QueueTransaction(iSpiBusId, &iSpiTransaction);

#ifdef VERBOSE_DEBUG
	for (int i = 0; i <KSpiPacketLength; i++)
		Kern::Printf("W1[i]: %d", iSpiReadBuffer[1]);
#endif
	return r;
	}

// Touch controller
TouchController::TouchController() :
	iCallback(NULL)
	{
	}

TouchController::TouchController(TVoidCallback aCallback) :
	iCallback(aCallback)
	{
	}

TInt TouchController::HardReset()
	{
	LOG_FUNCTION_CALL;
	TInt r = GPIO::SetPinMode(KResetPin, GPIO::EEnabled);
	if(r == KErrNone)
		{
		r = GPIO::SetPinDirection(KResetPin, GPIO::EOutput);
		if(r == KErrNone)
			{
//			GPIO::SetOutputState(KResetPin, GPIO::ELow);
			GPIO::SetOutputState(KResetPin, GPIO::EHigh);
//			GPIO::SetOutputState(KResetPin, GPIO::ELow);

			Kern::NanoWait(25000); // should be > 10us
//			GPIO::SetOutputState(KResetPin, GPIO::EHigh);
			GPIO::SetOutputState(KResetPin, GPIO::ELow);

			}
		}
//	Kern::NanoWait(1000000);
//	SoftReset();
//	Kern::NanoWait(1000000);

	iInterface.Write(KMasterReadStartAddr, X1_H);
	return r;
	}
TInt TouchController::SoftReset()
	{
	LOG_FUNCTION_CALL;
	for(TInt i = 0; i<4; i++)
		{
		iCtrlRegsCache[i] = 0;
		}
	return iInterface.Write(KControl_0, KControl_0_SWRST);
	}

TInt TouchController::SetTouchMode(TTouchMode aMode)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[0] &= ~KControl_0_MODE_MASK; // clear all mode bits
	switch(aMode)
		{
		case ESingle:
			iCtrlRegsCache[0] |= KControl_0_MODE_SINGLE;
			break;
		case EMulti:
			iCtrlRegsCache[0] |= KControl_0_MODE_MULTI;
			break;
		case EGesture:
			iCtrlRegsCache[0] |= KControl_0_MODE_GESTURE;
			break;
		}

	return iInterface.Write(KControl_0, iCtrlRegsCache[0]);
	}


TInt TouchController::SetResolution(TResolution aResolution)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[0] &= ~KControl_0_RM_12; // clear all mode bits
	if(aResolution == ERes12Bits)
		{
		iCtrlRegsCache[0] |= KControl_0_RM_12; // set bit
		}
	return iInterface.Write(KControl_0, iCtrlRegsCache[0]);
	}


TInt TouchController::SetLongerSamplingRate(TUint aRate)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[0] &= ~KControl_0_LST_MASK; // clear bits..
	iCtrlRegsCache[0] |= aRate & KControl_0_LST_MASK; // set new value..
	return iInterface.Write(KControl_0, iCtrlRegsCache[0]);
	}


TInt TouchController::SetIrqActiveTime(TUint aIrqActiveTime)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[1] &= ~KControl_1_PAT_MASK; // clear bits..
	iCtrlRegsCache[1] |= (aIrqActiveTime << KControl_1_PAT_SHIFT) & KControl_1_PAT_MASK; // set new value..
	return iInterface.Write(KControl_1, iCtrlRegsCache[1]);
	}


TInt TouchController::SetPanelVoltageStabTime(TUint aVoltageStabilizationTime)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[1] &= ~KControl_1_PVT_MASK; // clear bits..
	iCtrlRegsCache[1] |= (aVoltageStabilizationTime << KControl_1_PVT_SHIFT) & KControl_1_PVT_MASK; // set new value..
	return iInterface.Write(KControl_1, iCtrlRegsCache[1]);
	}


TInt TouchController::SetNumberOfColumns(TUint aNumberOfColumns)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[2] &= ~KControl_2_C_MASK; // clear bits..
	iCtrlRegsCache[2] |= (aNumberOfColumns << KControl_2_C_SHIFT) & KControl_2_C_MASK;
	return iInterface.Write(KControl_2, iCtrlRegsCache[2]);
	}

TInt TouchController::SetNumberOfRows(TUint aNumberOfRows)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[3] &= ~KControl_3_R_SHIFT; // clear bits..
	iCtrlRegsCache[3] |= (aNumberOfRows << KControl_3_R_SHIFT) & KControl_3_R_MASK; // set new value..
	return iInterface.Write(KControl_3, iCtrlRegsCache[3]);
	}

TInt TouchController::EnableWindowMode(TPoint aStart, TPoint aStop)
	{
	LOG_FUNCTION_CALL;

	TBool error_occured = EFalse;
	// setup window points
	TInt r = iInterface.Write(KWindowXStart_Msb, (TUint8)(aStart.iX >> 8));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowXStart_Lsb, (TUint8)(aStart.iX));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowYStart_Msb, (TUint8)(aStart.iY >> 8));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowYStart_Lsb, (TUint8)(aStart.iY));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowXStop_Msb, (TUint8)(aStop.iX >> 8));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowXStop_Lsb, (TUint8)(aStop.iX));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowYStop_Msb, (TUint8)(aStop.iY >> 8));
	if(r != KErrNone)
		error_occured = ETrue;

	r = iInterface.Write(KWindowYStop_Lsb, (TUint8)(aStop.iY));
	if(r != KErrNone)
		error_occured = ETrue;

	// enable mode
	if(!error_occured)
		{
		iCtrlRegsCache[1] |= ~KControl_1_WS; // set enable bit..
		r = iInterface.Write(KControl_1, iCtrlRegsCache[1]);
		}
	else
		{
		r = KErrGeneral;
		}
	return r;
	}

TInt TouchController::DisableWindowMode()
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[1] &= ~KControl_1_WS; // clear enable bit..
	return iInterface.Write(KControl_1, iCtrlRegsCache[1]);
	}

TInt TouchController::NumOfTouches()
	{
	TUint8 val = 0;
	return iInterface.Read(KTouchNumberAndType, val);
	}

TInt TouchController::GetMeasurements(TPoint* aPoints, TInt& aNumPoints)
	{
	LOG_FUNCTION_CALL;
	TInt r = KErrArgument;
	TInt num_points = 0;
	if(aPoints)
		{
		// check how many points is there to read..
		TUint8 val = 0;
		r = iInterface.Read(KTouchNumberAndType, val);

		//Kern::Printf("KTouchNumberAndType %x", val);
		// if in multi mode - read all received, but only up to one otherwise..
		num_points = val & (val & KTouchNumberAndType_MULTI ? KTouchNumberAndType_TouchNMask : 1);

		// setup the transaction:
		for (TInt i = 0; i < num_points; i++) // if anything was touched at all..
			{
			// get X coordinate
			r = iInterface.Read(X1_H + (i << 2), val);
			if (r != KErrNone)
				break;

			aPoints[i].iX = TInt(val << 8);

			r = iInterface.Read(X1_L + (i << 2) , val);
			if (r != KErrNone)
				break;

			aPoints[i].iX |= TInt(val);

			// get Y coordinate
			r = iInterface.Read(Y1_H + (i << 2), val);
			if (r != KErrNone)
				break;

			aPoints[i].iY = TInt(val << 8);

			r = iInterface.Read(Y1_L + (i << 2) , val);
			if (r != KErrNone)
				break;

			aPoints[i].iY |= TInt(val);
			}
		}

	// update number of points
	if (r != KErrNone)
		{
		aNumPoints = 0;
		}
	else
		{
		aNumPoints = num_points;
		}
	return r;
	}


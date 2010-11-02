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


// Writes number of bytes stored in the write buffer - to the controller.
// aNumOfBytes specifies number of bytes including first byte of address.
// The write buffer should contain the data from index=1 and it's length should be set
// to number of registers to update + 1 (one byte for the address / command)
TInt TTouchControllerInterface::WriteMultiple(TUint8 aStartAddress, TInt aNumBytes)
	{
	LOG_FUNCTION_CALL;
	if(aNumBytes != iSpiWriteBuffer.Length())
		{
#ifdef VERBOSE_DEBUG
		Kern::Printf("Transfer buffers are not properly set, line: %d", __LINE__);
#endif
		return KErrArgument;
		}
	iSpiReadBuffer.SetLength(aNumBytes);
	iSpiWriteBuffer[0] = KWriteCommand | (aStartAddress << 1);
	return IicBus::QueueTransaction(iSpiBusId, &iSpiTransaction);
	}


// Reads number of bytes and stores them into the read buffer
// aNumOfBytes specifies number of bytes (excluding first byte of address)
// The read buffer will contain the received data starting from index=1 (TODO: consider MidTPtr)
TInt TTouchControllerInterface::ReadMultiple(TUint8 aStartAddress, TInt aNumBytes)
	{
	LOG_FUNCTION_CALL;
	TInt r = KErrNone;

	// if trying to read from address different that currently stored - first update the read register..
	if(iCurrentReadStart != aStartAddress)
		{
		r = Write(KMasterReadStartAddr, aStartAddress);
		if(r != KErrNone)
			{
			return r;
			}
		iCurrentReadStart = aStartAddress;
		}

	// make sure buffers are set to the requested size
	iSpiReadBuffer.SetLength(aNumBytes + 1);
	iSpiWriteBuffer.SetLength(aNumBytes + 1);
	iSpiWriteBuffer[0] = KReadCommand;

	return IicBus::QueueTransaction(iSpiBusId, &iSpiTransaction);
	}

TInt TTouchControllerInterface::Write(TUint8 aRegAddress, TUint8 aValue)
	{
	LOG_FUNCTION_CALL;
	iSpiWriteBuffer.SetLength(2);
	iSpiReadBuffer.SetLength(2);
	iSpiWriteBuffer[0] = KWriteCommand | (aRegAddress << 1);
	iSpiWriteBuffer[1] = aValue;


#ifdef VERBOSE_DEBUG
	for (int i = 0; i < iSpiWriteBuffer.Length(); i++)
		Kern::Printf("WR0[i]: %d", iSpiWriteBuffer[i]);

	for (int i = 0; i < iSpiReadBuffer.Length(); i++)
		Kern::Printf("W0[i]: %d", iSpiReadBuffer[i]);
#endif
	TInt r = IicBus::QueueTransaction(iSpiBusId, &iSpiTransaction);

#ifdef VERBOSE_DEBUG
	for (int i = 0; i < iSpiReadBuffer.Length(); i++)
		Kern::Printf("W1[i]: %d", iSpiReadBuffer[i]);
#endif
	return r;
	}


TInt TTouchControllerInterface::Read(TUint8 aRegAddress, TUint8& aValue)
	{
	LOG_FUNCTION_CALL;
	TInt r = ReadMultiple(aRegAddress, 2);

	if(r == KErrNone)
		{
		aValue = iSpiReadBuffer[1];
		}
	return r;
	}


//(TODO: consider MidTPtr)
inline TDes& TTouchControllerInterface::ReadBuff()
	{
	return iSpiReadBuffer;
	}

//(TODO: consider MidTPtr)
inline TDes& TTouchControllerInterface::WriteBuff()
	{
	return iSpiWriteBuffer;
	}


// Touch controller implementation
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
#ifdef RESET_LOW
			GPIO::SetOutputState(KResetPin, GPIO::ELow);
#else
			GPIO::SetOutputState(KResetPin, GPIO::EHigh);
#endif
			Kern::NanoWait(25000); // should be > 10us
#ifdef RESET_LOW

			GPIO::SetOutputState(KResetPin, GPIO::EHigh);
#else
			GPIO::SetOutputState(KResetPin, GPIO::ELow);
#endif
			}
		}
	return r;
	}
TInt TouchController::SoftReset()
	{
	return iInterface.Write(KControl_0, KControl_0_SWRST);
	}

TInt TouchController::Configure(TTouchMode aMode)
	{
	LOG_FUNCTION_CALL;
	TDes& writeBuffer = iInterface.WriteBuff();
	writeBuffer.SetLength(14);

	// value for KControl_0 register in writeBuffer[1]
	switch(aMode)
		{
		case EModeSingle:
			writeBuffer[1] = KControl_0_MODE_SINGLE;
			break;
		case EModeMulti:
			writeBuffer[1] = KControl_0_MODE_MULTI;
			break;
		case EModeGesture:
			writeBuffer[1] = KControl_0_MODE_GESTURE;
			break;
		}

//	const TUint8 KWindowXStart_Msb = 0x4; // R/W
//	const TUint8 KWindowXStart_Lsb = 0x5; // R/W
//	const TUint8 KWindowXStop_Msb  = 0x6; // R/W
//	const TUint8 KWindowXStop_Lsb  = 0x7; // R/W
//	const TUint8 KWindowYStart_Msb = 0x8; // R/W
//	const TUint8 KWindowYStart_Lsb = 0x9; // R/W
//	const TUint8 KWindowYStop_Msb  = 0xa; // R/W
//	const TUint8 KWindowYStop_Lsb  = 0xb; // R/W
//	const TUint8 KMasterReadStartAddr = 0xc; // R/W

	writeBuffer[2] = (TInt8)(KControl_1_PAT_100 | KControl_1_PVT_200); // KControl_1 // TODO: update these values..
	writeBuffer[3] = (TInt8)(KNumColumns << KControl_2_C_SHIFT);       // KControl_2 // TODO: update these values..
	writeBuffer[4] = (TInt8)(KNumRows    << KControl_3_R_SHIFT);       // KControl_3 // TODO: update these values..

	writeBuffer[5] = 0; // KWindowXStart_Msb // TODO: update these values..
	writeBuffer[6] = 0; // KWindowXStart_Lsb // TODO: update these values..
	writeBuffer[7] = 0; // KWindowXStop_Msb // TODO: update these values..
	writeBuffer[8] = 0; // KWindowXStop_Lsb // TODO: update these values..

	writeBuffer[9] = 0; // KWindowYStart_Msb // TODO: update these values..
	writeBuffer[10] = 0; // KWindowYStart_Lsb // TODO: update these values..
	writeBuffer[11] = 0; // KWindowYStop_Msb // TODO: update these values..
	writeBuffer[12] = 0; // KWindowYStop_Lsb // TODO: update these values..
	writeBuffer[13] = 0; // KMasterReadStartAddr // TODO: update these values..

	return iInterface.WriteMultiple(KControl_0, 14);
	}

TInt TouchController::SetTouchMode(TTouchMode aMode)
	{
	LOG_FUNCTION_CALL;
	iCtrlRegsCache[0] &= ~KControl_0_MODE_MASK; // clear all mode bits
	switch(aMode)
		{
		case EModeSingle:
			iCtrlRegsCache[0] |= KControl_0_MODE_SINGLE;
			break;
		case EModeMulti:
			iCtrlRegsCache[0] |= KControl_0_MODE_MULTI;
			break;
		case EModeGesture:
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
	TDes& writeBuffer = iInterface.WriteBuff();
	writeBuffer.SetLength(9);

	// setup window points
	writeBuffer[0] = KWriteCommand | (KWindowXStart_Msb << 1); // address of first register to write..
	writeBuffer[1] = (TUint8)(aStart.iX >> 8); // KWindowXStart_Msb
	writeBuffer[2] = (TUint8)(aStart.iX);      // KWindowXStart_Lsb
	writeBuffer[3] = (TUint8)(aStop.iX >> 8);  // KWindowXStop_Msb
	writeBuffer[4] = (TUint8)(aStop.iX);       // KWindowXStop_Lsb
	writeBuffer[5] = (TUint8)(aStart.iY >> 8); // KWindowYStart_Msb
	writeBuffer[6] = (TUint8)(aStart.iY);      // KWindowYStart_Lsb
	writeBuffer[7] = (TUint8)(aStop.iY >> 8);  // KWindowYStop_Msb
	writeBuffer[8] = (TUint8)(aStop.iY);       // KWindowYStop_Lsb

	return iInterface.WriteMultiple(KWindowXStart_Msb, 9);
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
	TInt r = iInterface.Read(KTouchNumberAndType, val);
	if (r == KErrNone)
		{
		r = val;
		}
	return r;
	}

TInt TouchController::GetMeasurements(TPoint* aPoints, TInt& aNumPoints)
	{
	LOG_FUNCTION_CALL;
	TInt r = KErrArgument;
	TInt num_points = 0;

	if(aPoints)
		{
		r = iInterface.ReadMultiple(KTouchNumberAndType, 14);

		if(r == KErrNone)
			{
			TDes& readBuffer = iInterface.ReadBuff();

			// check how many points is there to read..
			TUint8 val = readBuffer[1]; // TODO: update this if MidTPtr could be used..
	//		Kern::Printf("KTouchNumberAndType %x", val);

			// if in multi mode - read all received, but only up to one otherwise..
			num_points = val & (val & KTouchNumberAndType_MULTI ? KTouchNumberAndType_TouchNMask : 1);

			// read the coordinates..
			for (TInt i = 0; i < num_points; i++) // if anything was touched at all..
				{
				// get X coordinate
				aPoints[i].iX |= TInt(readBuffer[(i << 2) + 1] << 8); // X_H
				aPoints[i].iX =  TInt(readBuffer[(i << 2) + 2]);      // X_L

				// get Y coordinate
				aPoints[i].iY |= TInt(readBuffer[(i << 2) + 3] << 8); // Yx_H
				aPoints[i].iY =  TInt(readBuffer[(i << 2) + 4]);      // Y_L
				}

			aNumPoints = num_points;
			}
		}
	return r;
	}


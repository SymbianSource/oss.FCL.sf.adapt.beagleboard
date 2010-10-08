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
// omap3530/beagle_drivers/byd_touch/common/controller.h
//

#ifndef TOUCH_KEY_CONTROLLER_H_
#define TOUCH_KEY_CONTROLLER_H_

#include <drivers/iic.h>
#include <e32def.h>


// controller specific constants..
const TUint KMaxNumPoints = 3;
typedef void (*TVoidCallback) (TAny*);


// spi specific constants..
const TUint KSpiPacketLength = 4;
const TUint KStartByte = 0x56 << 1;
const TUint KWrite     = 0;
const TUint KRead      = 1;
const TUint KReadCommand  = KStartByte | KRead;
const TUint KWriteCommand = KStartByte;
const TUint KBufGranulatity = 8;

// digitizer specific settings..
const TUint KResetPin = 93; // This setting does not change (it goes through J4/J5 connectors)
const TUint KDataReadyPin = 133; // DAV connected to the GPIO133 (Expansion header pin 15)

const TUint KSpiModule = 2; // McSPI3
const TUint KSpiSlaveAddr0 = 2; // for data18-data23 pin functions slave address 2=>CS0..
const TUint KSpiSlaveAddr1 = 3; // ..slave address 3=>CS1 (pref!)
const TUint KSpiSlaveAddr = KSpiSlaveAddr1;


const TConfigSpiV01 KHeader =
	{
	ESpiWordWidth_8, //iWordWidth
	375000,//,93750, //iClkSpeed
	ESpiPolarityHighFallingEdge, //iClkMode
	500, // iTimeoutPeriod
	EBigEndian, // iEndianness
	EMsbFirst, //iBitOrder
	2, //iTransactionWaitCycles
	ESpiCSPinActiveLow //iCsPinActiveMode
	};

// spi interface to the controller
class TTouchControllerInterface
	{
public:
	inline TTouchControllerInterface();
	inline TInt Read(TUint8 aRegAddress, TUint8& aValue);
	inline TInt Write(TUint8 aRegAddress, TUint8 aValue);
	inline TInt Write(TUint8 aRegAddress, TUint8* aValues, TInt aNumOfItems);


private:
	// SPI duplex transaction with two transfers for each direction
	TPckgBuf<TConfigSpiV01> iSpiTransactionHeader;
	TBuf8<KSpiPacketLength> iSpiWriteBuffer;
	TBuf8<KSpiPacketLength> iSpiReadBuffer;
	TIicBusTransfer         iSpiTxTransfer;
	TIicBusTransfer         iSpiRxTransfer;
	TIicBusTransaction      iSpiTransaction;
	TInt                    iSpiBusId;
	};

class TouchController
	{
public:
	enum TTouchMode
		{
		ESingle = 0,
		EMulti,
		EGesture
		};

	enum TResolution
		{
		ERes10Bits = 0,
		ERes12Bits
		};

	enum TGestureType
		{
		EClockwiseCircumvolve = 3,
		EDragLeft             = 4,
		EDragRight            = 5,
		EDragUp               = 6,
		EDragDown             = 7,
		EZoomOut              = 8,
		EZoomIn               = 9
		};

	TouchController();
	TouchController(TVoidCallback aCallback);


	TInt HardReset();
	TInt SoftReset();
	TInt SetTouchMode(TTouchMode aMode);
	TInt SetResolution(TResolution aResolution);
	TInt SetLongerSamplingRate(TUint aRate);
	TInt SetIrqActiveTime(TUint aIrqActiveTime);
	TInt SetPanelVoltageStabTime(TUint aVoltageStabilizationTime);
	TInt SetNumberOfColumns(TUint aNumberOfColumns);
	TInt SetNumberOfRows(TUint aNumberOfRows);
	TInt EnableWindowMode(TPoint aXpoint, TPoint aYPoint);
	TInt DisableWindowMode();
	TInt GetMeasurements(TPoint* aPoints, TInt& aNumPoints);
	TInt NumOfTouches();

private:
	TInt iCtrlRegsCache[4];
	TTouchControllerInterface iInterface;
	TVoidCallback iCallback;
	};


// BF6917 Register definitions
const TUint8 KControl_0  = 0x0; // R/W
const TUint8 KControl_0_SWRST        = 1 << 7;  // SW reset
const TUint8 KControl_0_RM_12        = 1 << 6; // Resolution mode 12 bits if set, 10bits otherwise
const TUint8 KControl_0_MODE_SINGLE  = 1 << 3; // single touch mode
const TUint8 KControl_0_MODE_MULTI   = 5 << 3; // multi-touch mode
const TUint8 KControl_0_MODE_GESTURE = 6 << 3; // gesture mode
const TUint8 KControl_0_MODE_MASK    = 7 << 3; // gesture mode

const TUint8 KControl_0_LST_MASK     = 0x7; // Longer sampling rate: from 5 to 120 ADC clocks sampling time
const TUint8 KControl_0_LST_5        = 0 << 0; // 5   ADC clocks sampling time
const TUint8 KControl_0_LST_10       = 1 << 0; // 10  ADC clocks sampling time
const TUint8 KControl_0_LST_20       = 2 << 0; // 20  ADC clocks sampling time
const TUint8 KControl_0_LST_40       = 3 << 0; // 40  ADC clocks sampling time
const TUint8 KControl_0_LST_60       = 4 << 0; // 60  ADC clocks sampling time
const TUint8 KControl_0_LST_80       = 5 << 0; // 80  ADC clocks sampling time
const TUint8 KControl_0_LST_100      = 6 << 0; // 100 ADC clocks sampling time
const TUint8 KControl_0_LST_120      = 7 << 0; // 120 ADC clocks sampling time


// Pen Irq Active time (sensiveness)
const TUint8 KControl_1  = 0x1; // R/W
const TUint8 KControl_1_PAT_SHIFT = 5;
const TUint8 KControl_1_PAT_MASK  = 7 << 5;
const TUint8 KControl_1_PAT_100   = 0 << 5;
const TUint8 KControl_1_PAT_87_5  = 1 << 5;
const TUint8 KControl_1_PAT_75    = 2 << 5;
const TUint8 KControl_1_PAT_62_5  = 3 << 5;
const TUint8 KControl_1_PAT_50    = 4 << 5;
const TUint8 KControl_1_PAT_37_5  = 5 << 5;
const TUint8 KControl_1_PAT_25    = 6 << 5;
const TUint8 KControl_1_PAT_12_5  = 7 << 5;

//  Panel Voltage stabilization Time(sensiveness)
const TUint8 KControl_1_PVT_SHIFT = 2;
const TUint8 KControl_1_PVT_MASK = 7 << 2;
const TUint8 KControl_1_PVT_200  = 0 << 2;
const TUint8 KControl_1_PVT_175  = 1 << 2;
const TUint8 KControl_1_PVT_150  = 2 << 2;
const TUint8 KControl_1_PVT_125  = 3 << 2;
const TUint8 KControl_1_PVT_100  = 4 << 2;
const TUint8 KControl_1_PVT_75   = 5 << 2;
const TUint8 KControl_1_PVT_50   = 6 << 2;
const TUint8 KControl_1_PVT_25   = 7 << 2;
const TUint8 KControl_1_WS       = 1 << 0; // Window mode enables


const TUint8 KControl_2  = 0x2; // R/W
const TUint8 KControl_2_C_SHIFT  = 3; // number of columns
const TUint8 KControl_2_C_MASK = 0xf8;
const TUint8 KControl_2_PR_SHIFT = 0; // pull-up resistance


const TUint8 KControl_3  = 0x3; // R/W
const TUint8 KControl_3_R_SHIFT  = 4; // number of rows
const TUint8 KControl_3_R_MASK   = 0xF0; // number of rows

const TUint8 KControl_2_R_TEST   = 1 << 3; // test mode select
const TUint8 KControl_2_PS_SHIFT = 0; // processing scale (0-7)

const TUint8 KWindowXStart_Msb = 0x4; // R/W
const TUint8 KWindowXStart_Lsb = 0x5; // R/W
const TUint8 KWindowXStop_Msb  = 0x6; // R/W
const TUint8 KWindowXStop_Lsb  = 0x7; // R/W
const TUint8 KWindowYStart_Msb = 0x8; // R/W
const TUint8 KWindowYStart_Lsb = 0x9; // R/W
const TUint8 KWindowYStop_Msb  = 0xa; // R/W
const TUint8 KWindowYStop_Lsb  = 0xb; // R/W
const TUint8 KMasterReadStartAddr = 0xc; // R/W


//  data registers
const TUint8 KTouchNumberAndType  = 0xd; //  R, Touch number and touch type
const TUint8 KTouchNumberAndType_SINGLE = 0 << 3;
const TUint8 KTouchNumberAndType_MULTI  = 1 << 3;
const TUint8 KTouchNumberAndType_TouchNMask = 7; // touch number (0 - 3)


const TUint8 X1_H   = 0xe; //  R, High byte of x1 measurement
const TUint8 X1_L   = 0xf; //  R, Low  byte of x1 measurement
const TUint8 Y1_H   = 0x10; // R, High byte of y1 measurement
const TUint8 Y1_L   = 0x11; // R, Low  byte of y1 measurement
const TUint8 X2_H   = 0x12; // R, High byte of x2 measurement
const TUint8 X2_L   = 0x13; // R, Low  byte of x2 measurement
const TUint8 Y2_H   = 0x14; // R, High byte of y2 measurement
const TUint8 Y2_L   = 0x15; // R, Low  byte of y2 measurement
const TUint8 X3_H   = 0x16; // R, High byte of x3 measurement
const TUint8 X3_L   = 0x17; // R, Low  byte of x3 measurement
const TUint8 Y3_H   = 0x18; // R, High byte of y3 measurement
const TUint8 Y3_L   = 0x19; // R, Low  byte of y3 measurement
const TUint8 Ges_H  = 0x1a; // R, High byte of gesture information
const TUint8 Ges_L  = 0x1b; // R, Low  byte of gesture information
const TUint8 TEST_H = 0x1c; // R, Low byte of the test result
const TUint8 TEST_L = 0x1d; // R, Low byte of the test result


#endif /* TOUCH_KEY_CONTROLLER_H_ */

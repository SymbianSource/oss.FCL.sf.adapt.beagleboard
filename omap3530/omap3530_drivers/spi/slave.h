// Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
// lukasz.forynski@gmail.com
//
// Description:
// omap3530/omap3530_drivers/spi/slave.h
//


#ifndef __OMAP3530_SPI_SLAVE_H__
#define __OMAP3530_SPI_SLAVE_H__

#include <drivers/iic_channel.h>

class DSpiSlaveBeagle: public DIicBusChannelSlave
	{

public:
	static DSpiSlaveBeagle* New(TInt aChannelNumber, const TBusType aBusType,
	                              const TChannelDuplex aChanDuplex);

	// Gateway function for PSL implementation
	virtual TInt DoRequest(TInt aOperation);

	DSpiSlaveBeagle(TInt aChannelNumber, const TBusType aBusType,
	                 const TChannelDuplex aChanDuplex);

private:
	// Overriders for base class pure-virtual methods
	virtual TInt DoCreate(); // second-stage construction,
	virtual TInt CheckHdr(TDes8* aHdrBuff);
	virtual void ProcessData(TInt aTrigger, TIicBusSlaveCallback* aCb);

	// Internal methods..
	TBool TransConfigDiffersFromPrev();
	TInt ConfigureInterface();
	TInt AsynchConfigureInterface();
	TInt InitTransfer();

	// ISR handler and other static methods..
	static void IicPslIsr(TAny* aPtr);
	static void TimeoutCallback(TAny* aPtr);
	static inline void NotifyClientEnd(DSpiSlaveBeagle* aPtr);

	// Register base for the Master channel
	TUint iSlaveChanBase;

	// Interrupt ID for the Master channel
	TInt iSlaveIntId;

	// Bit mask of the transfer triggers managed by the channel
	TUint8 iTrigger;

	// Granularity, expressed as the number of bytes in a word
	TInt8 iWordSize;

	// Flag indicating transmission activity - optional, may not be required for all bus types
	// In the template example it is used to indicate transitions on a chip-select line, such as for SPI.
	TInt8 iInProgress;

	// Dummy variable used merely to demonstrate the asynchronous channel capture mechanism
	// See method AsynchConfigureInterface
	TInt8 iAsyncConfig;

	// Pointers to buffers used for Rx and Tx transfers
	TInt8 *iTxData;
	TInt8 *iRxData;
	TInt8 *iTxDataEnd;
	TInt8 *iRxDataEnd;

	// Timer to guard 'while' loops..
	NTimer iHwGuardTimer;

	// status of the transaction
	volatile TInt iTransactionStatus;
	};

#endif /*__OMAP3530_SPI_SLAVE_H__*/

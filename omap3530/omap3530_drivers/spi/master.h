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
// omap3530/omap3530_drivers/spi/master.h
//


#ifndef __OMAP3530_SPI_MASTER_H__
#define __OMAP3530_SPI_MASTER_H__

#include <drivers/iic_channel.h>

_LIT(KIicPslThreadName,"SpiChannelThread_");
const TInt KIicPslDfcPriority = 0;
const TInt KIicPslThreadPriority = 24;

// class declaration for SPI master
class DSpiMasterBeagle : public DIicBusChannelMaster
	{
public:
	static DSpiMasterBeagle* New(TInt aChannelNumber, const TBusType aBusType, const TChannelDuplex aChanDuplex);
	virtual TInt DoRequest(TIicBusTransaction* aTransaction); // Gateway function for PSL implementation

private:
	DSpiMasterBeagle(TInt aChannelNumber, const TBusType aBusType, const TChannelDuplex aChanDuplex);

	// Override base-class pure virtual methods
	virtual TInt DoCreate();
	virtual TInt CheckHdr(TDes8* aHdr);
	virtual TInt HandleSlaveTimeout();

	// Internal methods
	TInt PrepareConfiguration();
	TInt ConfigureInterface();
	TInt ProcessNextTransfers();
	TInt StartTransfer(TIicBusTransfer* aTransferPtr, TUint8 aType);
	TInt DoTransfer(TUint8 aType);
	static void Isr(TAny* aPtr);
	static void TransferEndDfc(TAny* aPtr);
	void ExitComplete(TInt aErr, TBool aComplete = ETrue);

#ifdef _DEBUG
	void DumpCurrentStatus(const TInt8* aWhere = NULL);
#endif

private:
	TDfc iTransferEndDfc;
	TIicOperationType iOperation;
	TUint8 iWordSize;

	TInt8 iTxFifoThreshold;
	enum TMyState
		{
		EIdle,
		EBusy
		};
	TMyState iState;

	TInt iIrqId;
	TLinAddr iHwBase;

	// Pointers used to store current transfers information
	TIicBusTransfer* iHalfDTransfer;
	TIicBusTransfer* iFullDTransfer;

	// Pointer to the current transaction.
	TIicBusTransaction* iCurrTransaction;

	// Pointers to buffers used for Rx and Tx transfers
	TInt8 *iTxData;
	TInt8 *iRxData;
	TInt8 *iTxDataEnd;
	TInt8 *iRxDataEnd;

	// global status of the transaction
	volatile TInt iTransactionStatus;

	TConfigSpiV01 iCurrHeader;
	TInt iCurrSS;
	TInt iCurrSlavePinSet;
	};

#endif //__OMAP3530_SPI_MASTER_H__

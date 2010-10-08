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
// Implementation of IIC master channel for a SPI bus.
//

#define DBGPRINT(x)
#define DBG_ERR(x) x


#ifdef _DEBUG
#define DEBUG_ONLY(x) //x
#else
#define DEBUG_ONLY(x)
#endif


// DO NOT CHANGE THESE- trying to tune the driver (unless you really know what you're doing)
// as this this is only for development purpose to tune the driver. Fifo mode is not yet enabled, but this
// doesn't affect operation. After development has been finished - these macros and #ifdefs will be removed
// entirely. For now only SINGLE_MODE should ever be defined.
//#define USE_TX_FIFO
//#define USING_TX_COUNTER
//#define PER_TRANSFER_MODE
#define SINGLE_MODE

#include <assp/omap3530_assp/omap3530_assp_priv.h>
#include <assp/omap3530_assp/omap3530_prcm.h>
#include <drivers/iic.h>
#include "omap3530_spi.h"
#include "psl_init.h"
#include "master.h"

DSpiMasterBeagle::DSpiMasterBeagle(TInt aChannelNumber, TBusType aBusType, TChannelDuplex aChanDuplex) :
	DIicBusChannelMaster(aBusType, aChanDuplex),
	iTransferEndDfc(TransferEndDfc, this, KIicPslDfcPriority)
	{
	iChannelNumber = aChannelNumber;
	iIrqId  = KMcSpiIrqId[iChannelNumber];
	iHwBase = KMcSpiRegBase[iChannelNumber];
	iState  = EIdle;
	iCurrSS = -1; // make sure channel will be fully configured on the first use
	DBGPRINT(Kern::Printf("DSpiMasterBeagle::DSpiMasterBeagle: at 0x%x, iChannelNumber = %d", this, iChannelNumber));
	}

TInt DSpiMasterBeagle::DoCreate()
	{
	DBGPRINT(Kern::Printf("\nDSpiMasterBeagle::DoCreate() McSPI%d \n", iChannelNumber+1));
	DBGPRINT(Kern::Printf("HW revision is %x", AsspRegister::Read32(iHwBase + MCSPI_REVISION)));
	TInt r = KErrNone;

	// Create the DFCQ to be used by the channel
	if(!iDfcQ)
		{
		TBuf8<KMaxName> threadName (KIicPslThreadName);
		threadName.AppendNum(iChannelNumber);
		r = Kern::DfcQCreate(iDfcQ, KIicPslThreadPriority, &threadName);
		if(r != KErrNone)
			{
			DBG_ERR(Kern::Printf("DFC Queue creation failed, channel number: %d, r = %d\n", iChannelNumber, r));
			return r;
			}
		}

	// PIL Base class initialization - this must be called prior to SetDfcQ(iDfcQ)
	r = Init();
	if(r == KErrNone)
		{
		// Call base class function to set DFCQ pointers in the required objects
		// This also enables the channel to process transaction requests
		SetDfcQ(iDfcQ);

		// PSL DFCQ initialisation for local DFC
		iTransferEndDfc.SetDfcQ(iDfcQ);

		// Bind interrupts.
		r = Interrupt::Bind(iIrqId, Isr, this);
		if(r < KErrNone)
			{
			DBG_ERR(Kern::Printf("ERROR: InterruptBind error.. %d", r));
			return r;
			}
		}

	// Make sure clocks are enabled (TBD: this could go to 'PowerUp/PowerDown' if using PRM)
	Prcm::SetClockState( Prcm::EClkMcSpi3_F, Prcm::EClkOn );
	Prcm::SetClockState( Prcm::EClkMcSpi3_I, Prcm::EClkOn );
	// TODO:consider auto-idle for PRCM.CM_AUTOIDLE1_CORE

	// setup default spi pins. For channel 2 (McSPI3) it can be configured dynamically
	SetupSpiPins(iChannelNumber);

	return r;
	}

// A static method used to construct the DSpiMasterBeagle object.
DSpiMasterBeagle* DSpiMasterBeagle::New(TInt aChannelNumber, const TBusType aBusType, const TChannelDuplex aChanDuplex)
	{
	DBGPRINT(Kern::Printf("DSpiMasterBeagle::NewL(): ChannelNumber = %d, BusType =%d", aChannelNumber, aBusType));
	DSpiMasterBeagle *pChan = new DSpiMasterBeagle(aChannelNumber, aBusType, aChanDuplex);

	TInt r = KErrNoMemory;
	if(pChan)
		{
		r = pChan->DoCreate();
		}
	if(r != KErrNone)
		{
		delete pChan;
		pChan = NULL;
		}
	return pChan;
	}

// This method is called by the PIL to initiate the transaction. After finishing it's processing,
// the PSL calls the PIL function CompleteRequest to indicate the success (or otherwise) of the request
TInt DSpiMasterBeagle::DoRequest(TIicBusTransaction* aTransaction)
	{
	DBGPRINT(Kern::Printf("\n=>DSpiMasterBeagle::DoRequest (aTransaction=0x%x)\n", aTransaction));

	// If the pointer to the transaction passed in as a parameter, or its associated pointer to the
	// header information is NULL, return KErrArgument
	if(!aTransaction || !GetTransactionHeader(aTransaction))
		{
		return KErrArgument;
		}

	// The PSL operates a simple state machine to ensure that only one transaction is processed
	// at a time - if the channel is currently busy, reject the request (PIL should not try that!)
	if(iState != EIdle)
		{
		return KErrInUse;
		}

	// copy pointer to the transaction
	iCurrTransaction = aTransaction;

	// Configure the hardware to support the transaction
	TInt r = PrepareConfiguration();
	if(r == KErrNone)
		{
		r = ConfigureInterface();
		if(r == KErrNone)
			{
			// start processing transfers of this transaction.
			r = ProcessNextTransfers();
			}
		}
	return r;
	}

TInt DSpiMasterBeagle::PrepareConfiguration()
	{
	TConfigSpiV01 &newHeader = (*(TConfigSpiBufV01*) (GetTransactionHeader(iCurrTransaction)))();

	// get the slave address (i.e. known as a 'channel' for the current SPI module)
	TInt busId       = iCurrTransaction->GetBusId();
	TInt slaveAddr   = GET_SLAVE_ADDR(busId);
	TInt slavePinSet = 0;

	if(slaveAddr >= KMcSpiNumSupportedSlaves[iChannelNumber]) // address is 0-based
		{
		DBG_ERR(Kern::Printf("Slave address for McSPI%d should be < %, was: %d !",
				iChannelNumber + 1, KMcSpiNumSupportedSlaves[iChannelNumber], slaveAddr));
		return KErrArgument; // Slave address out of range
		}

	// Slave addresses > 1 for McSPI3 (iChannel2) really means alternative pin settings,
	// so adjust it in such case. *Pin set indexes are +1 to skip over the pin set for McSPI4
	// channel in the pin configuration table.
	if(iChannelNumber == 2 && slaveAddr > 1)
		{
		slavePinSet  =  slaveAddr > 3 ?  3 : 2; // slaveAddr: 2-3: pin set 2(1*); 4-5: pin set 3(2*)
		slaveAddr &= 1; // modulo 2 (i.e. 2 CS for McSPI3)
		}

	// reconfigure pins if needed..
	if(slavePinSet != iCurrSlavePinSet)
		{
		iCurrSlavePinSet = slavePinSet;
		SetupSpiPins(iChannelNumber, iCurrSlavePinSet);
		}

	// store configuration parameters
	iCurrSS          = slaveAddr;
	iCurrHeader      = newHeader; //copy the header..

	return KErrNone;
	}

// Init the hardware with the data provided in the transaction and slave-address field
// (these values are already stored in the iCurrHeader)
TInt DSpiMasterBeagle::ConfigureInterface()
	{
	DBGPRINT(Kern::Printf("ConfigureInterface()"));

	// soft reset the SPI..
	TUint val = AsspRegister::Read32(iHwBase + MCSPI_SYSCONFIG);
	val = MCSPI_SYSCONFIG_SOFTRESET;  // issue reset

	AsspRegister::Write32(iHwBase + MCSPI_SYSCONFIG, MCSPI_SYSCONFIG_SOFTRESET);

	val = 0; // TODO will add here this 'smart-wait' stuff that was proposed earlier..
	while (!(val & MCSPI_SYSSTATUS_RESETDONE))
		val = AsspRegister::Read32(iHwBase + MCSPI_SYSSTATUS);

	// disable and clear all interrupts..
	AsspRegister::Write32(iHwBase + MCSPI_IRQENABLE, 0);
	AsspRegister::Write32(iHwBase + MCSPI_IRQSTATUS,
	                      MCSPI_IRQ_RX_FULL(iCurrSS) |
	                      MCSPI_IRQ_RX_FULL(iCurrSS) |
	                      MCSPI_IRQ_TX_UNDERFLOW(iCurrSS) |
	                      MCSPI_IRQ_TX_EMPTY(iCurrSS) |
	                      MCSPI_IRQ_RX_OVERFLOW);

	// channel configuration
	//	Set the SPI1.MCSPI_CHxCONF[18] IS bit to 0 for the spi1_somi pin in receive mode.
	//	val = MCSPI_CHxCONF_IS; // pin selection (somi - simo)
	// TODO configuration of PINS could also be configurable on a 'per SPI module' basis..

	// Set the SPI1.MCSPI_CHxCONF[17] DPE1 bit to 0 and the SPI1.MCSPI_CHxCONF[16] DPE0 bit to 1 for the spi1.simo pin in transmit mode.
	val = MCSPI_CHxCONF_DPE0;

	// Set transmit & | receive mode for transmit only mode here. If needed - it will be changed dynamically.
	val |= MCSPI_CHxCONF_TRM_NO_RECEIVE;

	// set word length.
	val |= SpiWordWidth(iCurrHeader.iWordWidth);

	// use the appropriate word with (assuming the data is aligned to bytes).
	if(iCurrHeader.iWordWidth > ESpiWordWidth_16)
		{
		iWordSize = 4;
		}
	else if (iCurrHeader.iWordWidth > ESpiWordWidth_8)
		{
		iWordSize = 2;
		}
	else
		{
		iWordSize = 1;
		}

	// set Slave Select / Chip select signal mode
	val |= iCurrHeader.iSSPinActiveMode == ESpiCSPinActiveLow ? MCSPI_CHxCONF_EPOL_LOW : 0;

	// set the CLK POL and PHA (clock mode)
	val |= SpiClkMode(iCurrHeader.iClkMode);

	// Set clock. Note that CheckHdr() will be called prior to this function for this header,
	// so the value iClkSpeedHz is valid at this point, the KErrNotSupported is not possible
	// so the return value check can be ommited here
	val |= SpiClkValue(iCurrHeader.iClkSpeedHz);
	// __ASSERT_DEBUG(val >= KErrNone, Kern::Fault("spi/master.cpp, line: ", __LINE__));

#ifdef USE_TX_FIFO
	// enable fifo for transmission..
	// Update me: this can only set in a 'single' mode.. or for only one channel
	// but at the momment IIC SPI is used in 'single' mode onlny..
	val |= MCSPI_CHxCONF_FFEW;
//	val |= MCSPI_CHxCONF_FFER; // fifo enable for receive.. (TODO)
#endif

	val |= (iCurrHeader.iTransactionWaitCycles & 3) << MCSPI_CHxCONF_TCS_SHIFT;

	// update the register..
	AsspRegister::Write32(iHwBase + MCSPI_CHxCONF(iCurrSS), val);

	// set spim_somi pin direction to input
	val = MCSPI_SYST_SPIDATDIR0;

	// drive csx pin to inactive state
	if(iCurrHeader.iSSPinActiveMode == ESpiCSPinActiveLow)
		{
		AsspRegister::Modify32(iHwBase + MCSPI_SYST, 1u << iCurrSS, 0);
		}
	else
		{
		AsspRegister::Modify32(iHwBase + MCSPI_SYST, 0, (1u << iCurrSS));
		}

	// Set the MS bit to 0 to provide the clock (ie. to setup as master)
#ifndef SINGLE_MODE
	AsspRegister::Write32(iHwBase + MCSPI_MODULCTRL, MCSPI_MODULCTRL_MS_MASTER);
	// change the pad config - now the SPI drives the line appropriately..
	SetCsActive(iChannelNumber, iCurrSS, iCurrSlavePinSet);
#else
	AsspRegister::Write32(iHwBase + MCSPI_MODULCTRL, MCSPI_MODULCTRL_MS_MASTER | MCSPI_MODULCTRL_SINGLE);
#endif

	return KErrNone;
	}

TInt DSpiMasterBeagle::ProcessNextTransfers()
	{
	DBGPRINT(Kern::Printf("DSpiMasterBeagle::ProcessNextTransfers():%s", iState==EIdle ? "first" : "next"));

	// Since new transfers are strating, clear exisiting flags
	iOperation.iValue = TIicOperationType::ENop;

	// If this is the first transfer in the transaction the channel will be in state EIdle
	if(iState == EIdle)
		{
		// Get the pointer to half-duplex transfer object..
		iHalfDTransfer = GetTransHalfDuplexTferPtr(iCurrTransaction);

		// Get the pointer to full-duplex transfer object..
		iFullDTransfer = GetTransFullDuplexTferPtr(iCurrTransaction);

		// Update the channel state to EBusy and initialise the transaction status
		iState = EBusy;
		iTransactionStatus = KErrNone;

		// start timeout timer for this transaction
		StartSlaveTimeOutTimer(iCurrHeader.iTimeoutPeriod);
		}
	else
	// If not in state EIdle, get the next transfer in the linked-list held by the transaction
		{
		// Get the pointer the next half-duplex transfer object..
		iHalfDTransfer = GetTferNextTfer(iHalfDTransfer);

		// Get the pointer to the next half-duplex transfer object..
		if(iFullDTransfer)
			{
			iFullDTransfer = GetTferNextTfer(iFullDTransfer);
			}
		}

	TInt r = KErrNone;
	if(!iFullDTransfer && !iHalfDTransfer)
		{
		// There is no more to transfer - and all previous were were completed,
		DBGPRINT(Kern::Printf("All transfers completed successfully"));
		ExitComplete(KErrNone);
		}
	else
		{
		// Process next transfers
		TInt8 hDTrType = (TInt8) GetTferType(iHalfDTransfer);

		if(iFullDTransfer)
			{
			// For full-duplex transfer setup the read transfer first, as it doesn't
			// really start anything - SPI master starts operation when Tx (or clocks)starts..

			if(hDTrType == TIicBusTransfer::EMasterRead)
				{
				r = StartTransfer(iHalfDTransfer, TIicBusTransfer::EMasterRead);
				if(r != KErrNone)
					{
					return r;
					}
				r = StartTransfer(iFullDTransfer, TIicBusTransfer::EMasterWrite);
				}
			else // hDTrType == TIicBusTransfer::EMasterWrite)
				{
				r = StartTransfer(iFullDTransfer, TIicBusTransfer::EMasterRead);
				if(r != KErrNone)
					{
					return r;
					}
				r = StartTransfer(iHalfDTransfer, TIicBusTransfer::EMasterWrite);
				}
			}
		else
		// This is a HalfDuplex transfer - so just start it
			{
			r = StartTransfer(iHalfDTransfer, hDTrType);
			}
		}
	return r;
	}

TInt DSpiMasterBeagle::StartTransfer(TIicBusTransfer* aTransferPtr, TUint8 aType)
	{
	DBGPRINT(Kern::Printf("DSpiMasterBeagle::StartTransfer() @0x%x, aType: %s",
			               aTransferPtr, aType == TIicBusTransfer::EMasterWrite ? "write" : "read"));

	if(aTransferPtr == NULL)
		{
		DBG_ERR(Kern::Printf("DSpiMasterBeagle::StartTransfer - NULL pointer\n"));
		return KErrArgument;
		}

	TInt r = KErrNone;

	switch(aType)
		{
		case TIicBusTransfer::EMasterWrite:
			{
			DBGPRINT(Kern::Printf("Starting EMasterWrite, duplex=%x", iFullDTransfer));

			// Get a pointer to the transfer object's buffer, to facilitate passing arguments to DoTransfer
			const TDes8* desBufPtr = GetTferBuffer(aTransferPtr);

			DBGPRINT(Kern::Printf("Length %d, iWordSize %d", desBufPtr->Length(), iWordSize));

			// Store the current address and ending address for Transmission - they are required by the ISR and DFC
			iTxData    = (TInt8*)  desBufPtr->Ptr();
			iTxDataEnd = (TInt8*) (iTxData + desBufPtr->Length());
			if ((TInt)iTxDataEnd % iWordSize)
				{
				DBG_ERR(Kern::Printf("Wrong configuration - word size does not match buffer length"));
				return KErrArgument;
				}

			DBGPRINT(Kern::Printf("Tx: Start: %x, End %x, bytes %d", iTxData, iTxDataEnd, desBufPtr->Length()));

			// Set the flag to indicate that we'll be transmitting data
			iOperation.iOp.iIsTransmitting = ETrue;

			// initiate the transmission..
			r = DoTransfer(aType);
			if(r != KErrNone)
				{
				DBG_ERR(Kern::Printf("Starting Write failed, r = %d", r));
				}
			break;
			}

		case TIicBusTransfer::EMasterRead:
			{
			DBGPRINT(Kern::Printf("Starting EMasterRead, duplex=%x", iFullDTransfer));

			// Get a pointer to the transfer object's buffer, to facilitate passing arguments to DoTransfer
			const TDes8* aBufPtr = GetTferBuffer(aTransferPtr);

			// Store the current address and ending address for Reception - they are required by the ISR and DFC
			iRxData = (TInt8*) aBufPtr->Ptr();
			iRxDataEnd = (TInt8*) (iRxData + aBufPtr->Length());

			DBGPRINT(Kern::Printf("Rx: Start: %x, End %x, bytes %d", iRxData, iRxDataEnd, aBufPtr->Length()));

			// Set the flag to indicate that we'll be receiving data
			iOperation.iOp.iIsReceiving = ETrue;

			// initiate the reception
			r = DoTransfer(aType);
			if(r != KErrNone)
				{
				DBG_ERR(Kern::Printf("Starting Read failed, r = %d", r));
				}
			break;
			}

		default:
			{
			DBG_ERR(Kern::Printf("Unsupported TransactionType %x", aType));
			r = KErrArgument;
			break;
			}
		}

	return r;
	}

// Method called by StartTransfer to actually initiate the transfers.
TInt DSpiMasterBeagle::DoTransfer(TUint8 aType)
	{
	DBGPRINT(Kern::Printf("\nDSpiMasterBeagle::DoTransfer()"));
	TInt r = KErrNone;

	AsspRegister::Write32(iHwBase + MCSPI_IRQSTATUS, ~0);

	switch(aType)
		{
		case TIicBusTransfer::EMasterWrite:
			{
			// enable the channel here..
			AsspRegister::Write32(iHwBase + MCSPI_CHxCTRL(iCurrSS), MCSPI_CHxCTRL_EN);

			AsspRegister::Modify32(iHwBase + MCSPI_IRQSTATUS, 0,
			                       MCSPI_IRQ_TX_EMPTY(iCurrSS) /*| MCSPI_IRQ_TX_UNDERFLOW(iCurrSS)*/);

			AsspRegister::Modify32(iHwBase + MCSPI_IRQENABLE, 0,
			                       MCSPI_IRQ_TX_EMPTY(iCurrSS) /*| MCSPI_IRQ_TX_UNDERFLOW(iCurrSS)*/);

#ifdef SINGLE_MODE
			// in SINGLE mode needs to manually assert CS line for current
			AsspRegister::Modify32(iHwBase + MCSPI_CHxCONF(iCurrSS), 0, MCSPI_CHxCONF_FORCE);

			// change the pad config - now the SPI drives the line appropriately..
			SetCsActive(iChannelNumber, iCurrSS, iCurrSlavePinSet);
#endif /*SINGLE_MODE*/

#ifdef USE_TX_FIFO
			const TInt KTxFifoThreshold = 8;
			TUint numWordsToTransfer = (iTxDataEnd - iTxData);
			TUint wordsToWrite = Min(numWordsToTransfer/iWordSize, KTxFifoThreshold/iWordSize);


			TInt iAlmostFullLevel = 0;
			TInt iAlmostEmptyLevel = 1; //KTxFifoThreshold;

			// setup FIFOs
			AsspRegister::Write32(iHwBase + MCSPI_XFERLEVEL,
								  MCSPI_XFERLEVEL_WCNT(0) | // total num words
								  MCSPI_XFERLEVEL_AFL(iAlmostFullLevel)     | // Rx almost full
								  MCSPI_XFERLEVEL_AEL(iAlmostEmptyLevel) );   // Tx almost empty

			// copy data to fifo..
			for (TInt i = 0; i < wordsToWrite; i++)
				{
				iTxData += iWordSize;
				AsspRegister::Write32(iHwBase + MCSPI_TXx(iCurrSS), *(iTxData -iWordSize));
				}

#else /*USE_TX_FIFO*/

			TUint val = 0;
			for (TInt i = 0; i < iWordSize; i++)
				{
				val |= (*iTxData++) << i * 8;
				}

			DEBUG_ONLY(DumpCurrentStatus("DoTransfer(Write)"));
			AsspRegister::Write32(iHwBase + MCSPI_TXx(iCurrSS), val);
#endif /*USE_TX_FIFO*/

			// enable system interrupt
			Interrupt::Enable(iIrqId);
			break;
			}
		case TIicBusTransfer::EMasterRead:
			{
			// enable transmit and receive..
			AsspRegister::Modify32(iHwBase + MCSPI_CHxCONF(iCurrSS), MCSPI_CHxCONF_TRM_NO_RECEIVE, 0);

			// for single read (not duplex) one way to to allow clock generation is to enable Tx
			// and write '0' to Txregister (just like in duplex transaction). We also need to assert Cs line.
			if(!iFullDTransfer)
				{
				// enable the channel..
				AsspRegister::Write32(iHwBase + MCSPI_CHxCTRL(iCurrSS), MCSPI_CHxCTRL_EN);

				// enable TX and RX Empty interrupts
				AsspRegister::Modify32(iHwBase + MCSPI_IRQSTATUS, 0, MCSPI_IRQ_TX_EMPTY(iCurrSS) | MCSPI_IRQ_RX_FULL(iCurrSS));
				AsspRegister::Modify32(iHwBase + MCSPI_IRQENABLE, 0, MCSPI_IRQ_TX_EMPTY(iCurrSS) | MCSPI_IRQ_RX_FULL(iCurrSS));
#ifdef SINGLE_MODE
				// in SINGLE mode needs to manually assert CS line for current
				AsspRegister::Modify32(iHwBase + MCSPI_CHxCONF(iCurrSS), 0, MCSPI_CHxCONF_FORCE);

				// change the pad config - now the SPI drives the line appropriately..
				SetCsActive(iChannelNumber, iCurrSS, iCurrSlavePinSet);
#endif /*SINGLE_MODE*/
				}
			else
				{
				// enable only interrupts for RX here. Tx is handled in EMasterWrite case above.
				AsspRegister::Write32(iHwBase + MCSPI_IRQSTATUS, MCSPI_IRQ_RX_FULL(iCurrSS));
				AsspRegister::Write32(iHwBase + MCSPI_IRQENABLE, MCSPI_IRQ_RX_FULL(iCurrSS));
				}

			DEBUG_ONLY(DumpCurrentStatus("DoTransfer(Read)"));
			// and enable system interrupts
			if(!iFullDTransfer)
				Interrupt::Enable(iIrqId);
			break;
			}
		default:
			{
			DBG_ERR(Kern::Printf("Unsupported TransactionType %x", aType));
			r = KErrArgument;
			break;
			}
		}

	return r;
	}

#ifdef _DEBUG
static TInt IsrCnt = 0;
void DSpiMasterBeagle::DumpCurrentStatus(const TInt8* aWhere /*=NULL*/)
	{
	if(aWhere)
		Kern::Printf("------ Status (%s)--------", aWhere);
	else
		Kern::Printf("------ Status --------");
	Kern::Printf("\niTransactionStatus: %d", iTransactionStatus);
	Kern::Printf("iTransferEndDfc %s queued", iTransferEndDfc.Queued() ? "" : "NOT");

	if(iOperation.iOp.iIsTransmitting)
		{
		Kern::Printf("TX STATUS:");
		Kern::Printf("  iTxData    %x", iTxData);
		Kern::Printf("  iTxDataEnd %x", iTxDataEnd);
		Kern::Printf("  left to write: %x (words)", (iTxDataEnd - iTxData)/iWordSize);
		}

	if(iOperation.iOp.iIsReceiving)
		{
		Kern::Printf("RX STATUS:");
		Kern::Printf("  iRxData    %x", iRxData);
		Kern::Printf("  iRxDataEnd %x", iRxDataEnd);
		Kern::Printf("  left to read: %x (words)", (iRxDataEnd - iRxData)/iWordSize);
		}
	Kern::Printf("  iCurrSS %d",iCurrSS);

	Kern::Printf("IsrCnt %d", IsrCnt);
	TUint status = AsspRegister::Read32(iHwBase + MCSPI_IRQSTATUS);
	Kern::Printf("MCSPI_IRQSTATUS (0x%x):", status);
	if(status & MCSPI_IRQ_TX_EMPTY(iCurrSS))
		Kern::Printf("   MCSPI_IRQ_TX_EMPTY");
	if(status & MCSPI_IRQ_TX_UNDERFLOW(iCurrSS))
		Kern::Printf("   MCSPI_IRQ_TX_UNDERFLOW");
	if(!iCurrSS && status & MCSPI_IRQ_RX_OVERFLOW)
		Kern::Printf("   MCSPI_IRQ_RX_OVERFLOW");
	if(status & MCSPI_IRQ_RX_FULL(iCurrSS))
		Kern::Printf("   MCSPI_IRQ_RX_FULL");

	Kern::Printf("MCSPI_CHxSTAT(%d):", iCurrSS);
	status = AsspRegister::Read32(iHwBase + MCSPI_CHxSTAT(iCurrSS));
	if(status & MCSPI_CHxSTAT_RXFFF)
		Kern::Printf("   MCSPI_CHxSTAT_RXFFF");
	if(status & MCSPI_CHxSTAT_RXFFE)
		Kern::Printf("   MCSPI_CHxSTAT_RXFFE");
	if(status & MCSPI_CHxSTAT_TXFFF)
		Kern::Printf("   MCSPI_CHxSTAT_TXFFF");
	if(status & MCSPI_CHxSTAT_TXFFE)
		Kern::Printf("   MCSPI_CHxSTAT_TXFFE");
	if(status & MCSPI_CHxSTAT_EOT)
		Kern::Printf("   MCSPI_CHxSTAT_EOT");
	if(status & MCSPI_CHxSTAT_TXS)
		Kern::Printf("   MCSPI_CHxSTAT_TXS");
	if(status & MCSPI_CHxSTAT_RXS)
		Kern::Printf("   MCSPI_CHxSTAT_RXS");

	Kern::Printf("MCSPI_XFERLEVEL:");
	status = AsspRegister::Read32(iHwBase + MCSPI_XFERLEVEL);
	Kern::Printf("   MCSPI_XFERLEVEL_WCNT %d", status >> MCSPI_XFERLEVEL_WCNT_OFFSET);
	Kern::Printf("   MCSPI_XFERLEVEL_AFL %d", (status >> MCSPI_XFERLEVEL_AFL_OFFSET) & 0x3F);
	Kern::Printf("   MCSPI_XFERLEVEL_AEL %d\n", (status >> MCSPI_XFERLEVEL_AEL_OFFSET) & 0x1F);
	Kern::Printf("---------------------------------------/*\n\n\n");
	}
#endif

void DSpiMasterBeagle::Isr(TAny* aPtr)
	{
	DSpiMasterBeagle *a = (DSpiMasterBeagle*) aPtr;
	DEBUG_ONLY(IsrCnt++);
	DEBUG_ONLY(a->DumpCurrentStatus("Isr entry"));

	TUint32 status = AsspRegister::Read32(a->iHwBase + MCSPI_IRQSTATUS);
	AsspRegister::Write32(a->iHwBase + MCSPI_IRQSTATUS, status); // clear status bits..

	// TX_EMPTY - when an item (or number of items if FIFO is used) was transmitted..
	if(status & MCSPI_IRQ_TX_EMPTY(a->iCurrSS))
		{

		if(a->iOperation.iOp.iIsTransmitting)
			{
#ifdef USE_TX_FIFO
			// when FIFO is used - should write (at least) the MCSPI_XFERLEVEL_AFL + 1 words to this register..
			while(a->iTxData != a->iTxDataEnd)
				{
				AsspRegister::Write32(a->iHwBase + MCSPI_TXx(a->iCurrSS), *a->iTxData);
				a->iTxData += a->iWordSize;	// Then increment the pointer to the data.s

				if(AsspRegister::Read32(a->iHwBase + MCSPI_CHxSTAT(a->iCurrSS)) & MCSPI_CHxSTAT_TXFFF)
					{
					break;
					}
				}
#else
			// transfer next word..
			if(a->iTxData != a->iTxDataEnd)
				{
				TUint val = 0;
				for (TInt i = 0; i < a->iWordSize; i++)
					{
					val |= (*a->iTxData++) << i * 8;
					}
				AsspRegister::Write32(a->iHwBase + MCSPI_TXx(a->iCurrSS), val);
				}

			// check again - if this was the last one..and we're not waiting for rx - end transfer
			if(a->iTxData == a->iTxDataEnd && !a->iOperation.iOp.iIsReceiving)
				{
				Interrupt::Disable(a->iIrqId);
				a->iTransferEndDfc.Add();
				}
#endif
			}
		else
			{
			// writing a 'dummy' word (for read only transferss (writing 0 doesn't change line state)
			AsspRegister::Write32(a->iHwBase + MCSPI_TXx(a->iCurrSS), 0);
			}
		}

	if(status & MCSPI_IRQ_RX_FULL(a->iCurrSS))
		{
		if(a->iOperation.iOp.iIsReceiving)
			{
			if(a->iRxDataEnd != a->iRxData)
				{
				TUint8 nextRxValue = AsspRegister::Read32(a->iHwBase + MCSPI_RXx(a->iCurrSS));
				*a->iRxData = nextRxValue;
				a->iRxData += a->iWordSize;
				}

			// If the Rx buffer is now full, finish the transmission.
			if(a->iRxDataEnd == a->iRxData)
				{
				Interrupt::Disable(a->iIrqId);
				a->iTransferEndDfc.Add();
				}
			}
		}

#if 0 // TODO - probably master, as it creates CLK for slave - will never have to bother with this..
	if(status & MCSPI_IRQ_TX_UNDERFLOW(a->iCurrSS))
		{
		DBG_ERR(Kern::Printf("Underflow"));
		a->iTransactionStatus = KErrUnderflow;

		// disable the channel..
		AsspRegister::Modify32(a->iHwBase + MCSPI_CHxCTRL(0), MCSPI_CHxCTRL_EN, 0);
		Interrupt::Disable(a->iIrqId);
		DEBUG_ONLY(a->DumpCurrentStatus("TxUnderflow"));
		DBG_ERR(Kern::Fault("TxUnderflow", 0));
		}
#endif
#if defined(USE_TX_FIFO) && defined(USING_TX_COUNTER)
	if(status & MCSPI_IRQSTATUS_EOW)
		{
		Kern::Printf("EOW");
		// TODO: end of transfer..
		}
#endif

	// end of ISR processing
	DEBUG_ONLY(a->DumpCurrentStatus("Isr end"));
	}

void DSpiMasterBeagle::TransferEndDfc(TAny* aPtr)
	{
	DBGPRINT(Kern::Printf("DSpiMasterBeagle::TransferEndDfc"));
	DSpiMasterBeagle *a = (DSpiMasterBeagle*) aPtr;

	TUint chanStatus = AsspRegister::Read32(a->iHwBase + MCSPI_CHxSTAT(a->iCurrSS));
	if(a->iOperation.iOp.iIsTransmitting)
		{
		TUint expected = MCSPI_CHxSTAT_EOT | MCSPI_CHxSTAT_TXS;

#ifdef USE_TX_FIFO
		while(!AsspRegister::Read32(a->iHwBase + MCSPI_CHxSTAT(a->iCurrSS)) & MCSPI_CHxSTAT_TXFFE);
#endif
		while(chanStatus & expected != expected)
			{
			chanStatus = AsspRegister::Read32(a->iHwBase + MCSPI_CHxSTAT(a->iCurrSS));
			}
		}

	if(a->iOperation.iOp.iIsReceiving)
		{
		TUint expected = MCSPI_CHxSTAT_RXS;

		while(chanStatus & expected != expected)
			{
			chanStatus = AsspRegister::Read32(a->iHwBase + MCSPI_CHxSTAT(a->iCurrSS));
			}
		__ASSERT_DEBUG(a->iRxDataEnd == a->iRxData,
		               Kern::Fault("SPI master: exiting not having received all?", 12));
		}

#ifdef SINGLE_MODE
	// manually de-assert CS line for this channel
	AsspRegister::Modify32(a->iHwBase + MCSPI_CHxCONF(a->iCurrSS), MCSPI_CHxCONF_FORCE, 0);

	// put the CS signal to 'inactive' state (as on channel disable it would have a glitch)
	SetCsInactive(a->iChannelNumber, a->iCurrSS, a->iCurrHeader.iSSPinActiveMode, a->iCurrSlavePinSet);

#endif

	// disable the channel
	AsspRegister::Modify32(a->iHwBase + MCSPI_CHxCTRL(0), MCSPI_CHxCTRL_EN, 0);

	// Start the next transfer for this transaction, if any remain
	if(a->iState == EBusy)
		{
		TInt err = a->ProcessNextTransfers();
		if(err != KErrNone)
			{
			// If the next transfer could not be started, complete the transaction with
			// the returned error code
			a->ExitComplete(err);
			}
		}
	}

void DSpiMasterBeagle::ExitComplete(TInt aErr, TBool aComplete /*= ETrue*/)
	{
	DBGPRINT(Kern::Printf("DSpiMasterBeagle::ExitComplete, aErr %d, aComplete %d", aErr, aComplete));


	// in the case of error - make sure to reset the channel
	if(aErr != KErrNone)
		{
		// make sure CS is in inactive state (for the current / last transaction) on error
		// TODO: add extendable transaction support (..i.e. with no de-assertion of CS pin between such transactions)
		SetCsInactive(iChannelNumber, iCurrSS, iCurrHeader.iSSPinActiveMode, iCurrSlavePinSet);

		// disable this channel
		AsspRegister::Modify32(iHwBase + MCSPI_CHxCTRL(iCurrSS), MCSPI_CHxCTRL_EN, 0);

		AsspRegister::Write32(iHwBase + MCSPI_SYSCONFIG, MCSPI_SYSCONFIG_SOFTRESET);
		iCurrSS = -1; // make sure the interface will be re-configured at next transaction
		}

	// Disable interrupts for the channel
	Interrupt::Disable(iIrqId);

	// Cancel any timers and DFCs..
	CancelTimeOut();
	iTransferEndDfc.Cancel();

	// Change the channel state back to EIdle
	iState = EIdle;

	// Call the PIL method to complete the request
	if(aComplete)
		{
		CompleteRequest(aErr);
		}
	}

#ifdef _DEBUG
void DumpHeader(TConfigSpiV01& aHeader)
	{
	Kern::Printf("header:");
	Kern::Printf("iWordWidth %d (%d bits)", aHeader.iWordWidth, (SpiWordWidth(aHeader.iWordWidth)) >> MCSPI_CHxCONF_WL_OFFSET + 1);
	Kern::Printf("iClkSpeedHz %d", aHeader.iClkSpeedHz);
	Kern::Printf("iClkMode %d", aHeader.iClkMode);
	Kern::Printf("iTimeoutPeriod %d", aHeader.iTimeoutPeriod);
	Kern::Printf("iBitOrder %d", aHeader.iBitOrder);
	Kern::Printf("iTransactionWaitCycles %d", aHeader.iTransactionWaitCycles);
	Kern::Printf("iSSPinActiveMode %d", aHeader.iSSPinActiveMode);
	}
#endif

// virtual method called by the PIL when a transaction is queued (with QueueTransaction).
// This is done in the context of the Client's thread.
// The PSL is required to check that the transaction header is valid for this channel.
TInt DSpiMasterBeagle::CheckHdr(TDes8* aHdrBuff)
	{
	TInt r = KErrNone;
	if(!aHdrBuff)
		{
		r = KErrArgument;
		}
	else
		{
		TConfigSpiV01 &header = (*(TConfigSpiBufV01*) (aHdrBuff))();

		// check if word width and clock are supported
		if(SpiWordWidth(header.iWordWidth) < KMinSpiWordWidth ||
		   SpiClkValue(header.iClkSpeedHz) < 0 || // == KErrNotSupported
		   header.iBitOrder == ELsbFirst ||  // this SPI only transmits MSB fist
		   (TUint)header.iTransactionWaitCycles > KMaxTransactionWaitTime) // max 3(+.5) cycles between words
			{
#ifdef _DEBUG
			if(header.iBitOrder == ELsbFirst)
				DBG_ERR(Kern::Printf("iClkSpeedHz value (%d) is not supported", header.iClkSpeedHz));
			if(SpiClkValue(header.iClkSpeedHz) < 0)
				DBG_ERR(Kern::Printf("iClkSpeedHz: %d is not supported", header.iClkSpeedHz));
			if((SpiWordWidth(header.iWordWidth)+ 1) >> MCSPI_CHxCONF_WL_OFFSET < KMinSpiWordWidth)
				DBG_ERR(Kern::Printf("iWordWidth: %d is not supported, min value is: %d",
						             SpiWordWidth(header.iWordWidth), KMinSpiWordWidth));
			if((TUint)header.iTransactionWaitCycles > 3)
				DBG_ERR(Kern::Printf("iTransactionWaitCycles: %d is not supported, value should be from 0 to %d",
				                     header.iTransactionWaitCycles, KMaxTransactionWaitTime));

			DumpHeader(header);
#endif
			r = KErrNotSupported;
			DBG_ERR(Kern::Printf("DSpiMasterBeagle::CheckHdr()failed, r = %d", r));
			}
		}
	return r;
	}

// This method is called by the PIL in the case of expiry of a timer for a transaction.
// TODO: this name is confusing - it could be changed in the PIL to reflect it's real purpose(TBD)
// It has NOTHING to do with a Slave (i.e. slave might be completely silent for SPI-and master won't notice it!)
TInt DSpiMasterBeagle::HandleSlaveTimeout()
	{
	DBG_ERR(Kern::Printf("HandleSlaveTimeout"));

	// Stop the PSL's operation, and inform the PIL of the timeout
	ExitComplete(KErrTimedOut, EFalse);

	return KErrTimedOut;
	}


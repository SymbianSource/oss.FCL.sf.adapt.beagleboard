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
//
// Description:
// omap3530_drivers/spi/test/d_spi_client_m.cpp
//
// This test driver is a simple example IIC SPI client implementation - and a test to SPI implementation.
// It is an LDD but PDD or kernel extension can implement / use the IIC SPI bus exactly the same way.
//

// Note: IMPORTANT! -If you intend to make changes to the driver!
// Obviously the best test is to use the SPI with the real device (and check if it still works after changes).
// If this is not the case (e.g. as it was when this driver was being created) - to fully test the functionality
// this test can make use of duplex transactions with local loopback. In such case received data should match the
// data sent over the bus. (It might be possible to configure local loopback using pad config(not yet confirmed),
// but until this (and not to complicate the test too much)- here is simple description on how to do it on a
// beagleboard. If McSPI3 is configured with the default pin option, i.e. to route signals to the expansion
// hader (see spi driver for details):s
// following header pins have SPI3 functions:
// 21 - CLK
// 19 - SIMO
// 17 - SOMI
// 11 - CS0
// local loopback could be simply made by puttint the jumper between pin 17 and pin 19 of the extension header.
// This test will automatically detect this configuration and the jumper - so if you want to test it this way
// it is highly recommended (and it's simple).

#include <drivers/iic.h>
#include <drivers/iic_channel.h>
#include <kernel/kern_priv.h>
#include "d_spi_client_m.h"


_LIT(KTestDriverName,"d_spi_client_m");

// (un) comment it out for debugging
//#define LOG_FUNCTION_CALLS

#ifdef LOG_FUNCTION_CALLS
#define LOG_FUNCTION_ENTRY Kern::Printf("DSpiClientChannel::%s()", __FUNCTION__)
#define LOG_FUNCTION_RETURN Kern::Printf("DSpiClientChannel::%s() return: %d", __FUNCTION__, r)
#else
#define LOG_FUNCTION_ENTRY
#define LOG_FUNCTION_RETURN
#endif


// === generic (driver related) stuff ===

// this driver only implements one channel
const TInt KMaxNumChannels = 1;



// entry point
DECLARE_STANDARD_LDD()
	{
	return new DSpiClientTestFactory;
	}

DSpiClientTestFactory::DSpiClientTestFactory()
	{
	iParseMask = 0;
	iUnitsMask = 0;  // No info, no PDD, no Units
	iVersion = TVersion(KIntTestMajorVersionNumber,
	        KIntTestMinorVersionNumber, KIntTestBuildVersionNumber);
	}

DSpiClientTestFactory::~DSpiClientTestFactory()
	{
	}

// Install the device driver.
TInt DSpiClientTestFactory::Install()
	{
	return (SetName(&KLddFileNameRoot));
	}

void DSpiClientTestFactory::GetCaps(TDes8& aDes) const
	{
	TPckgBuf<TCapsProxyClient> b;
	b().version = TVersion(KIntTestMajorVersionNumber, KIntTestMinorVersionNumber, KIntTestBuildVersionNumber);
	Kern::InfoCopy(aDes, b);
	}

// Create a channel on the device.
TInt DSpiClientTestFactory::Create(DLogicalChannelBase*& aChannel)
	{
	if (iOpenChannels >= KMaxNumChannels)
		return KErrOverflow;

	aChannel = new DSpiClientChannel;
	return aChannel ? KErrNone : KErrNoMemory;
	}

DSpiClientChannel::DSpiClientChannel()
	{
	iClient = &Kern::CurrentThread();
	// Increase the DThread's ref count so that it does not close without us
	((DObject*)iClient)->Open();
	}

DSpiClientChannel::~DSpiClientChannel()
	{
	((TDynamicDfcQue*)iDfcQ)->Destroy();
	// decrement the DThread's reference count
	Kern::SafeClose((DObject*&) iClient, NULL);
	}

TInt DSpiClientChannel::DoCreate(TInt aUnit, const TDesC8* /*aInfo*/, const TVersion& /*aVer*/)
	{
	TDynamicDfcQue *dfcq = NULL;
	TInt r = Kern::DynamicDfcQCreate(dfcq, KIntTestThreadPriority, KTestDriverName);

	if(r == KErrNone)
		{
		SetDfcQ(dfcq);
		iMsgQ.Receive();
		}
	return r;
	}

void DSpiClientChannel::HandleMsg(TMessageBase* aMsg)
	{
	TThreadMessage& m = *(TThreadMessage*) aMsg;
	TInt id = m.iValue;

	if (id == ECloseMsg)
		{
		iMsgQ.iMessage->Complete(KErrNone, EFalse);
		return;
		}
	else if (id == KMaxTInt)
		{
		m.Complete(KErrNone, ETrue);
		return;
		}

	if (id < 0)
		{
		TRequestStatus* pS = (TRequestStatus*) m.Ptr0();
		TInt r = DoRequest(~id, pS, m.Ptr1(), m.Ptr2());
		if (r != KErrNone)
			{
			Kern::RequestComplete(iClient, pS, r);
			}
		m.Complete(KErrNone, ETrue);
		}
	else
		{
		TInt r = DoControl(id, m.Ptr0(), m.Ptr1());
		m.Complete(r, ETrue);
		}
	}

// to handle synchronous requests..
// TODO: There are unimplemented functions - returning KErrNotSupported - treat this as a 'sort of'
// of test-driven development.. Ideally - they should all be implemented..
TInt DSpiClientChannel::DoControl(TInt aId, TAny* a1, TAny* a2)
	{
	TInt r = KErrNone;
	switch (aId)
		{
		case RSpiClientTest::EHalfDuplexSingleWrite:
			{
			r = HalfDuplexSingleWrite();
			break;
			}
		case RSpiClientTest::EHalfDuplexMultipleWrite:
			{
			r = HalfDuplexMultipleWrite();
			break;
			}
		case RSpiClientTest::EHalfDuplexSingleRead:
			{
			r = HalfDuplexSingleRead();
			break;
			}
		case RSpiClientTest::EHalfDuplexMultipleRead:
			{
			r = HalfDuplexMultipleRead();
			break;
			}
		case RSpiClientTest::EHalfDuplexMultipleWriteRead:
			{
			r = HalfDuplexMultipleWriteRead();
			break;
			}
		case RSpiClientTest::EFullDuplexSingle:
			{
			r = FullDuplexSingle();
			break;
			}
		case RSpiClientTest::EFullDuplexMultiple:
			{
			r = FullDuplexMultiple();
			break;
			}
		case RSpiClientTest::EHalfDuplexExtendable:
			{
			r = HalfDuplexExtendable();
			break;
			}
		case RSpiClientTest::EFullDuplexExtendable:
			{
			r = FullDuplexExtendable();
			break;
			}

		default:
			{
			Kern::Printf("DSpiClientChannel::DoControl():Unrecognized value for aId=0x%x\n", aId);
			r = KErrArgument;
			break;
			}
		}
	return r;
	}

// to handle asynchronous requests from the client
TInt DSpiClientChannel::DoRequest(TInt aId, TRequestStatus* aStatus, TAny* a1, TAny* a2)
	{
	__KTRACE_OPT(KTESTFAST, Kern::Printf("DSpiClientChannel::DoRequest(aId=0x%x, aStatus=0x%x, a1=0x%x, a2=0x%x\n",
					aId, aStatus, a1, a2));

	// TODO: There are unimplemented functions - returning KErrNotSupported - treat this as a 'sort of'
	// of test-driven development.. Ideally - they should all be implemented..
	TInt r = KErrNone;
	switch (aId)
		{
		case RSpiClientTest::EAsyncHalfDuplexSingleWrite:
			{
			r = KErrNotSupported; // AsyncHalfDuplexSingleWrite(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncHalfDuplexMultipleWrite:
			{
			r = KErrNotSupported; // AsyncHalfDuplexMultipleWrite(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncHalfDuplexSingleRead:
			{
			r = KErrNotSupported; // AsyncHalfDuplexSingleRead(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncHalfDuplexMultipleRead:
			{
			r = KErrNotSupported; // AsyncHalfDuplexMultipleRead(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncHalfDuplexMultipleWriteRead:
			{
			r = KErrNotSupported; // AsyncHalfDuplexMultipleWriteRead(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncFullDuplexSingle:
			{
			r = KErrNotSupported; // AsyncFullDuplexSingle(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncFullDuplexMultiple:
			{
			r = KErrNotSupported; // AsyncFullDuplexMultiple(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncHalfDuplexExtendable:
			{
			r = KErrNotSupported; // AsyncHalfDuplexExtendable(aStatus);
			break;
			}
		case RSpiClientTest::EAsyncFullDuplexExtendable:
			{
			r = KErrNotSupported; // AsyncFullDuplexExtendable(aStatus);
			break;
			}
		default:
			{
			Kern::Printf("DSpiClientChannel::DoRequest(): unrecognized value for aId=0x%x\n", aId);
			r = KErrArgument;
			break;
			}
		}

	// complete request from here if there was an error creating it..
	if(r != KErrNone)
		{
		Kern::RequestComplete(iClient, aStatus, r);
		}

	return r;
	}
// ====== (end of driver related stuff) ===


// test half duplex write:
// - one transaction with one write transfer
// - synchronous use - all buffers / transfer objects / transactions - on the stack
// (Data on stack - recommended for small transfers).
// This could serve as the simplest example of a single write
TInt DSpiClientChannel::HalfDuplexSingleWrite()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		3000000, //iClkSpeed
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create transfer object
	const TInt KBuffLength = 64;
	TBuf8<KBuffLength> txTransferBuf; // txbuffer..

	// fill it with some data..(this will also set the length of the buffer)
	for (TInt i = 0; i < KBuffLength; ++i)
		{
		txTransferBuf.Append(i+1);
		}

	// create tranfer object
	TIicBusTransfer txTransfer(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf);

	// Create a transaction using header and list of transfers..
	TIicBusTransaction transaction(&header, &txTransfer);

	// queue the transaction synchronously
	r = IicBus::QueueTransaction(busId, &transaction);

	LOG_FUNCTION_RETURN;
	return r;
	}

// test half duplex write:
// - one transaction with more write transfers
// - synchronous use - all buffers / transfer objects / transactions - on the stack
// (Data on stack - recommended for small transfers).
// This could serve as the simplest example of a multiple writes
// Note, that ALWAYS (not only in this example) - each of the transfers is separated
// with SS signals assertion / deassertion
TInt DSpiClientChannel::HalfDuplexMultipleWrite()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		3000000, //iClkSpeed
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create transfer objects
	const TInt KBuffLength = 64;
	TBuf8<KBuffLength> txTransferBuf1;
	TBuf8<KBuffLength> txTransferBuf2;

	// put some data into these buffers..(this will also set their length)
	for (TInt i = 0; i < KBuffLength; ++i)
		{
		txTransferBuf1.Append(i+1);
		txTransferBuf2.Append(63-i);
		}

	// create two transfers and link them (transfer2 after transfer1)
	TIicBusTransfer txTransfer1(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf1);
	TIicBusTransfer txTransfer2(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf2);
	txTransfer1.LinkAfter(&txTransfer2); // will link txTransfer2 after txTransfer1..

	// Create a transaction using header and linked transfers (using first in one)
	TIicBusTransaction transaction(&header, &txTransfer1);

	r = IicBus::QueueTransaction(busId, &transaction);

	LOG_FUNCTION_RETURN;
	return r;
	}

// test half duplex read:
// - one transaction with one read transfer
// - synchronous use - all buffers / transfer objects / transactions - on the stack
// (Data on stack - recommended for small transfers).
// This could serve as the simplest example of a single read transfer.
// (e.g. for a potential use with a Slave device that is only capable of sending data)
TInt DSpiClientChannel::HalfDuplexSingleRead()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		3000000, //iClkSpeed 3MHz (could use SpiFreqHz(16))
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create transfer objects
	const TInt KBuffLength = 64;
	TBuf8<KBuffLength> rxTransferBuf;

	// put some data into the buffer..(this will also set its length)
	for (TInt i = 0; i < KBuffLength; ++i)
		{
		rxTransferBuf.Append(i+1);
		}

	// create transfer
	TIicBusTransfer rxTransfer(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf);

	// And a transaction
	TIicBusTransaction transaction(&header, &rxTransfer);

	// queue transaction synchronously
	r = IicBus::QueueTransaction(busId, &transaction);

	// now, we has non-zero data in the buffer, but we know, that there was nothing connected
	// so we should either receive 0x0 (or 0xff if line was driven high- unlikely)- but we'll check
	// if rx buffer has simply different data in it..
	for (int i = 0; i < KBuffLength; i++)
		{
		if(rxTransferBuf[i] == i+1)
			{
			r = KErrCorrupt;
			break;
			}
		}

	LOG_FUNCTION_RETURN;
	return r;
	}

// test half duplex multiple transfer read:
// - one transaction with two read transfers
// - synchronous use - all buffers / transfer objects / transactions - on the stack
// (Data on stack - recommended for small transfers).
// this is simmilar to write transactions with multiple transfers (e.g. HalfDuplexMultipleWrite)
TInt DSpiClientChannel::HalfDuplexMultipleRead()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		1500000, //iClkSpeed 3MHz (could use SpiFreqHz(32))
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create transfer objects
	const TInt KBuffLength1 = 32;
	const TInt KBuffLength2 = 15;

	TBuf8<KBuffLength1> rxTransferBuf1;
	TBuf8<KBuffLength2> rxTransferBuf2;

	// put some data into these buffers..(this will also set their lengths)
	for (TInt i = 0; i < KBuffLength1; ++i)
		{
		rxTransferBuf1.Append(i+1);
		}

	for (TInt i = 0; i < KBuffLength2; ++i)
		{
		rxTransferBuf2.Append(i+1);
		}

	// create two transfers and link them (transfer2 after transfer1)
	TIicBusTransfer rxTransfer1(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf1);
	TIicBusTransfer rxTransfer2(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf2);
	rxTransfer1.LinkAfter(&rxTransfer2);

	// Create a transaction using header and linked transfers (using first in one)
	TIicBusTransaction transaction(&header, &rxTransfer1);

	r = IicBus::QueueTransaction(busId, &transaction);

	// now, we has non-zero data in the buffer, but we know, that there was nothing connected
	// so we should either receive 0x0 (or 0xff if line was driven high- unlikely)- but we'll check
	// if rx buffer has simply different data in it..
	for (int i = 0; i < KBuffLength1; i++)
		{
		if(rxTransferBuf1[i] == i+1)
			{
			r = KErrCorrupt;
			break;
			}
		}

	if(r == KErrNone)
		{
		for (int i = 0; i < KBuffLength2; i++)
			{
			if(rxTransferBuf2[i] == i+1)
				{
				r = KErrCorrupt;
				break;
				}
			}
		}
	LOG_FUNCTION_RETURN;
	return r;
	}


// test half duplex read / write:
// - one transaction with more transfers - intermixed read / write
// - synchronous use - all buffers / transfer objects / transactions - on the stack
// (Data on stack - recommended for small transfers).
// This could serve as a simple example of a common spi usage for peripherals configuration, i.e.:
// write data - e.g. address of register and / or command(s) followed by read of their value(s)
TInt DSpiClientChannel::HalfDuplexMultipleWriteRead()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		3000000, //iClkSpeed
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create transfer objects
	// (they don't have to be of the same size-it's just for simplicity here!)
	const TInt KBuffLength = 32;
	TBuf8<KBuffLength> txTransferBuf1; // txbuffer1..
	TBuf8<KBuffLength> txTransferBuf2; // txbuffer2..
	TBuf8<KBuffLength> rxTransferBuf; // rxbuffer..

	for (TInt i = 0; i < KBuffLength; ++i)
		{
		txTransferBuf1.Append(i+1);
		txTransferBuf2.Append(63-i);
		rxTransferBuf.Append(0xa5); // append some non-zero data-will check, if it's overwritten by read..
		}
	// The above will also set size of buffers - and this is checked by the driver!

	// create two transfers and link them (transfer2 after transfer1)
	TIicBusTransfer txTransfer1(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf1);
	TIicBusTransfer txTransfer2(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf2);
	txTransfer1.LinkAfter(&txTransfer2); // will link txTransfer2 after txTransfer1..

	// create read transfer and link it after above two transfers
	TIicBusTransfer rxTransfer(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf);
	txTransfer2.LinkAfter(&rxTransfer); // will link rxTransfer after txTransfer2..

	// Create a transaction using header and first of linked transfers
	TIicBusTransaction transaction(&header, &txTransfer1);

	// and queue it synchronously..
	r = IicBus::QueueTransaction(busId, &transaction);

	LOG_FUNCTION_RETURN;
	return r;
	}

// test full duplex with single* transfer i,e. simultenous write and read.
// - one half duplex transaction with one write transfer (for first direction-like usually)
// - one read buffer - that will be used to 'setup' a full duplex transaction (for the other direction)
// (Data on stack - recommended for small transfers).
// This could serve as a simple example of simultenous write and read from a bus
TInt DSpiClientChannel::FullDuplexSingle()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		3000000, //iClkSpeed
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create transfer objects (they SHOULD be of the same size!)
	const TInt KBuffLength = 64;
	TBuf8<KBuffLength> txTransferBuf; // txbuffer..
	TBuf8<KBuffLength> rxTransferBuf; // rxbuffer..

	// fill TX buffer with some data to send..
	// and the RX buffer - with some other data - to check if it's overwritten by read
	// (this will also set buffers length's)
	for (TInt i = 0; i < KBuffLength; ++i)
		{
		txTransferBuf.Append(i+1);
		rxTransferBuf.Append(0x55);
		}

	// create transfer objects..
	TIicBusTransfer txTransfer(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf);
	TIicBusTransfer rxTransfer(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf);

	// Create transaction using header and list of transfers..
	TIicBusTransaction transaction(&header, &txTransfer);

	// setup a full duplex transaction - using Rx buffer
	transaction.SetFullDuplexTrans(&rxTransfer);

	// queue the transaction
	r = IicBus::QueueTransaction(busId, &transaction);

	// now confirm the reception..
	// BEAGLE BOARD only- if using local callback (ideally it should be used to fully test it)
	// data in rx buffer should match the data in the tx buffer.
	// otherwise (with no local loopback) - rx buffer should contain ZERO / (or different) data as it was
	// initially filled with. (i.e. as nothing was driving the SOMI line - rx will count that as 0 sent over the bus)
	// see top of this file and IsLoobackAvailable() function description for more details.
	TBool checkReceivedMatchesSent = IsLoopbackAvailable();

	if(!checkReceivedMatchesSent)
		Kern::Printf("!Warning: %s (%d): not using local-loop for duplex test", __FILE__, __LINE__);

	for (int i = 0; i < KBuffLength; i++)
		{
		if(checkReceivedMatchesSent)
			{
			if (rxTransferBuf[i] != txTransferBuf[i])
				{
				r = KErrCorrupt;
				break;
				}
			}
		else
			{
			if (rxTransferBuf[i] == 0x55) // this was the value used..
				{
				r = KErrCorrupt;
				break;
				}
			}
		}

	LOG_FUNCTION_RETURN;
	return r;
	}


// test full duplex with multiple transfers
// - one half duplex transaction with two transfers: write followed by read
// - two buffers to form the other direction: read followed by write.
// (Data on stack - recommended for small transfers only).
// This could serve as a simple example of multi-transfer full duplex transactions.
TInt DSpiClientChannel::FullDuplexMultiple()
	{
	LOG_FUNCTION_ENTRY;
	TInt r = KErrNone;

	TUint32 busId = 0;
	SET_BUS_TYPE(busId, DIicBusChannel::ESpi);
	SET_CHAN_NUM(busId, 2);   // THis is the ModuleNumber, i.e. McSPIx (minus one), e.g. 2 for McSPI3
	SET_SLAVE_ADDR(busId, 0); // THis is the ChannelNumber (Slave number of the above McSPIx)

	// create header
	const TConfigSpiV01 KHeader =
		{
		ESpiWordWidth_8, //iWordWidth
		3000000, //iClkSpeed
		ESpiPolarityLowRisingEdge, //iClkMode
		500, // iTimeoutPeriod
		EBigEndian, // iEndianness
		EMsbFirst, //iBitOrder
		0, //iTransactionWaitCycles
		ESpiCSPinActiveLow //iCsPinActiveMode
		};

	TPckgBuf<TConfigSpiV01> header(KHeader);

	// create buffers:
	// for first duplex transfer
	const TInt KBuffLength1 = 24;
	TBuf8<KBuffLength1> txTransferBuf1;
	TBuf8<KBuffLength1> rxTransferBuf1;

	// for second duplex transfer..
	// Note: Buffers for different transfers can have different size like below.
	// the only requirement is - that for each duplex transfer(tx and rx) they need to have the same.
	const TInt KBuffLength2 = 32;
	TBuf8<KBuffLength2> rxTransferBuf2;
	TBuf8<KBuffLength2> txTransferBuf2;

	// fill them with data..
	for (TInt i = 0; i < txTransferBuf1.MaxLength(); ++i)
		{
		txTransferBuf1.Append(i+1);
		rxTransferBuf1.Append(0x55);
		}

	for (TInt i = 0; i < txTransferBuf2.MaxLength(); ++i)
		{
		txTransferBuf2.Append(i+1);
		rxTransferBuf2.Append(0xaa);
		}

	// Now we want to chain transfers in the following way:
	// txTransfer1 -> rxTransfer2
	// rxTransfer1 -> txTransfer2
	// note, that there could always be a read and write at the same time (in the same column)

	// first direction
	TIicBusTransfer txTransfer1(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf1);
	TIicBusTransfer rxTransfer1(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf1);

	// second transfer objects (they will run in parallel after above have finished)
	TIicBusTransfer txTransfer2(TIicBusTransfer::EMasterWrite, 8, &txTransferBuf2);
	TIicBusTransfer rxTransfer2(TIicBusTransfer::EMasterRead, 8, &rxTransferBuf2);

	// chain them as described above
	txTransfer1.LinkAfter(&rxTransfer2); // rxTransfer2 after txTransfer1
	rxTransfer1.LinkAfter(&txTransfer2); // txTransfer2 after rxTransfer1

	// Create a transaction using header and list of transfers..
	TIicBusTransaction transaction(&header, &txTransfer1);
	transaction.SetFullDuplexTrans(&rxTransfer1);

	// and finally queue the transaction
	r = IicBus::QueueTransaction(busId, &transaction);

	// now confirm the reception..
	// BEAGLE BOARD only- if using local callback (ideally it should be used to fully test it)
	// data in rx buffer should match the data in the tx buffer.
	// otherwise (with no local loopback) - rx buffer should contain zero / (or different) data as it was
	// initially filled with. See top of this file and IsLoobackAvailable() function description for more details.
	TBool checkReceivedMatchesSent = IsLoopbackAvailable();

	if(!checkReceivedMatchesSent)
		Kern::Printf("!Warning: %s (%d): not using local-loop for duplex test", __FILE__, __LINE__);

	// check first transfer
	for (int i = 0; i < KBuffLength1; i++)
		{
		if(checkReceivedMatchesSent)
			{
			if (rxTransferBuf1[i] != txTransferBuf1[i])
				{
				r = KErrCorrupt;
				break;
				}
			}
		else
			{
			if (rxTransferBuf1[i] == 0x55) // this was the value used..
				{
				r = KErrCorrupt;
				break;
				}
			}
		}

	// check second transfer
	for (int i = 0; i < KBuffLength2; i++)
		{
		if(checkReceivedMatchesSent)
			{
			if (rxTransferBuf2[i] != txTransferBuf2[i])
				{
				r = KErrCorrupt;
				break;
				}
			}
		else
			{
			if (rxTransferBuf2[i] == 0xaa) // this was the value used..
				{
				r = KErrCorrupt;
				break;
				}
			}
		}

	LOG_FUNCTION_RETURN;
	return r;
	}

TInt DSpiClientChannel::HalfDuplexExtendable()
	{
	return KErrNotSupported; // TODO: not implemented yet..
	}

TInt DSpiClientChannel::FullDuplexExtendable()
	{
	return KErrNotSupported; // TODO: not implemented yet..
	}


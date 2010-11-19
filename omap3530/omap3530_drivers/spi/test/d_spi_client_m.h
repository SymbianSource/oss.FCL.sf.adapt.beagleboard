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
// omap3530_drivers/spi/test/d_spi_client_m.h
//

#ifndef __D_SPI_CLIENT_MASTER__
#define __D_SPI_CLIENT_MASTER__

#include <e32cmn.h>
#include <e32ver.h>


const TInt KIntTestThreadPriority = 24;

const TInt KIntTestMajorVersionNumber = 1;
const TInt KIntTestMinorVersionNumber = 0;
const TInt KIntTestBuildVersionNumber = KE32BuildVersionNumber;

_LIT(KLddFileName, "d_spi_client_m.ldd");
_LIT(KLddFileNameRoot, "d_spi_client_m");

#ifdef __KERNEL_MODE__
#include <kernel/kernel.h>

// For now - the driver has to know what frequencies are supported and has to specify them directly
// in the transaction header. This might be changes in the IIC PIL (kernelhwsrv)-in a way, that the driver
// returns supported frequencies with some 'GetCaps' method. For now - standard frequencies for CLK
// (that support duty cycle 50-50) are 48MHz divisions per power-of-two.
const TInt KSpiClkBaseFreqHz = 48000000;
const TInt KSpiClkMaxDivider = 4096;

inline TInt SpiFreqHz(TInt aDivider)
	{
	__ASSERT_DEBUG(aDivider > 0 && aDivider < KSpiClkMaxDivider, Kern::Fault("d_spi_client_master: divider out of range", 13));
	__ASSERT_DEBUG(!(aDivider &  (aDivider-1)), Kern::Fault("d_spi_client_master: divider not power of two", 14));
	return KSpiClkBaseFreqHz / aDivider;
	}
#endif

class RSpiClientTest: public RBusLogicalChannel
	{
public:
	enum TControl
		{
		EHalfDuplexSingleWrite,
		EHalfDuplexMultipleWrite,
		EHalfDuplexSingleRead,
		EHalfDuplexMultipleRead,
		EHalfDuplexMultipleWriteRead,
		EFullDuplexSingle,
		EFullDuplexMultiple,
		EHalfDuplexExtendable,
		EFullDuplexExtendable
		};

	enum TRequest
		{
		EAsyncHalfDuplexSingleWrite,
		EAsyncHalfDuplexMultipleWrite,
		EAsyncHalfDuplexSingleRead,
		EAsyncHalfDuplexMultipleRead,
		EAsyncHalfDuplexMultipleWriteRead,
		EAsyncFullDuplexSingle,
		EAsyncFullDuplexMultiple,
		EAsyncHalfDuplexExtendable,
		EAsyncFullDuplexExtendable
		};

#ifndef __KERNEL_MODE__
public:
	TInt Open()
		{
		return (DoCreate(KLddFileNameRoot,
		                 TVersion(KIntTestMajorVersionNumber,KIntTestMinorVersionNumber,KIntTestBuildVersionNumber),
		                 -1,
		                 NULL,
		                 NULL,
		                 EOwnerThread));
		}

	// Synchronous calls
	TInt HalfDuplexSingleWrite()
		{return DoControl(EHalfDuplexSingleWrite);}

	TInt HalfDuplexMultipleWrite()
		{return DoControl(EHalfDuplexMultipleWrite);}

	TInt HalfDuplexSingleRead()
		{return DoControl(EHalfDuplexSingleRead);}

	TInt HalfDuplexMultipleRead()
		{return DoControl(EHalfDuplexMultipleRead);}

	TInt HalfDuplexMultipleWriteRead()
		{return DoControl(EHalfDuplexMultipleWriteRead);}

	TInt FullDuplexSingle()
		{return DoControl(EFullDuplexSingle);}

	TInt FullDuplexMultiple()
		{return DoControl(EFullDuplexMultiple);}


	// Asynchronous calls..
	void HalfDuplexSingleWrite(TRequestStatus& aStatus)
		{DoRequest(EAsyncHalfDuplexSingleWrite, aStatus, NULL);}

	void HalfDuplexMultipleWrite(TRequestStatus& aStatus)
		{DoRequest(EAsyncHalfDuplexMultipleWrite, aStatus, NULL);}

	void HalfDuplexSingleRead(TRequestStatus& aStatus)
		{DoRequest(EAsyncHalfDuplexSingleRead, aStatus, NULL);}

	void HalfDuplexMultipleRead(TRequestStatus& aStatus)
		{DoRequest(EAsyncHalfDuplexMultipleRead, aStatus, NULL);}

	void HalfDuplexMultipleWriteRead(TRequestStatus& aStatus)
		{DoRequest(EAsyncHalfDuplexMultipleWriteRead, aStatus, NULL);}

	void FullDuplexSingle(TRequestStatus& aStatus)
		{DoRequest(EAsyncFullDuplexSingle, aStatus, NULL);}

	void FullDuplexMultiple(TRequestStatus& aStatus)
		{DoRequest(EAsyncFullDuplexMultiple, aStatus, NULL);}

#endif
	};

#ifdef __KERNEL_MODE__
struct TCapsProxyClient
	{
	TVersion version;
	};

class DSpiClientTestFactory: public DLogicalDevice
	{
public:
	DSpiClientTestFactory();
	~DSpiClientTestFactory();
	virtual TInt Install();
	virtual void GetCaps(TDes8 &aDes) const;
	virtual TInt Create(DLogicalChannelBase*& aChannel);
	};

// declaration for the client channel
class DSpiClientChannel: public DLogicalChannel
	{
public:
	DSpiClientChannel();
	~DSpiClientChannel();
	virtual TInt DoCreate(TInt aUnit, const TDesC8* aInfo, const TVersion& aVer);

protected:
	static void Handler(TAny *aParam);
	virtual void HandleMsg(TMessageBase* aMsg); // Note: this is a pure virtual in DLogicalChannel
	TInt DoControl(TInt aId, TAny* a1, TAny* a2);
	TInt DoRequest(TInt aId, TRequestStatus* aStatus, TAny* a1, TAny* a2);

	// set of example transfers with transactions queued synchronously
	TInt HalfDuplexSingleWrite();
	TInt HalfDuplexMultipleWrite();
	TInt HalfDuplexSingleRead();
	TInt HalfDuplexMultipleRead();
	TInt HalfDuplexMultipleWriteRead();
	TInt FullDuplexSingle();
	TInt FullDuplexMultiple();

	// set of example transfers with transactions queued asynchronously
	// callback for asynchronous transactions..
	static void SingleHalfDuplexTransCallback(TIicBusTransaction* aTransaction,
	                                          TInt aBusId,
	                                          TInt aResult,
	                                          TAny* aParam);
	// test functions..
	TInt AsyncHalfDuplexSingleWrite(TRequestStatus* aStatus);
	TInt AsyncHalfDuplexSingleRead(TRequestStatus* aStatus);
	TInt AsyncHalfDuplexMultipleWrite(TRequestStatus* aStatus);
	TInt AsyncHalfDuplexMultipleRead(TRequestStatus* aStatus);
	TInt AsyncHalfDuplexMultipleWriteRead(TRequestStatus* aStatus);
	TInt AsyncFullDuplexSingle(TRequestStatus* aStatus);
	TInt AsyncFullDuplexMultiple(TRequestStatus* aStatus);

private:
	DThread* iClient;
	};

// template for wrapper class used in asynchronous transactions in order to manage
// pointers to buffers and allocated objects (to further access the data/release memory)

template <int Size>
class TTransactionWrapper
	{
public:
	TTransactionWrapper(DThread* aClient, TRequestStatus* aReqStatus, TConfigSpiBufV01* aHeader) :
	  iHeader(aHeader),
	  iCallback(NULL),
	  iClient(aClient),
	  iReqStatus(aReqStatus)
	  {
	  }

	TTransactionWrapper() : iHeader(NULL), iCallback(NULL), iClient(NULL), iReqStatus(NULL)
		{
		for(TUint i = 0; i < Size; ++i)
			{
			iTxBuffers[i]   = NULL;
			iRxBuffers[i]   = NULL;
			iTxTransfers[i] = NULL;
			iRxTransfers[i] = NULL;
			}
		}

	inline HBuf8* GetRxBuffer(TInt index)
		{
		__ASSERT_DEBUG(index < Size, Kern::Fault("d_spi_client.h, line: %d", __LINE__));
		return iRxBuffers[index];
		}

	// destructor - to clean up all the objects..
	inline ~TTransactionWrapper()
		{
		// it is safe to call delete on a 'NULL' pointer
		delete iHeader;
		delete iCallback;
		for(TUint i = 0; i < Size; ++i)
			{
			delete iTxBuffers[i];
			delete iTxTransfers[i];
			delete iRxBuffers[i];
			delete iRxTransfers[i];
			}
		}

	// store all object used by transaction
	TConfigSpiBufV01* iHeader;
	TIicBusCallback *iCallback;
	HBuf8* iTxBuffers[Size];
	HBuf8* iRxBuffers[Size];
	TIicBusTransfer* iTxTransfers[Size];
	TIicBusTransfer* iRxTransfers[Size];

	// also store client and request information
	DThread* iClient;
	TRequestStatus* iReqStatus;
	};


// Below is additional stuff for testing with local loopback
// the IsLoopbackAvailable function checks if McSPI3 is configured to use pins from extension header
// (Beagleboard) and if these pins are physically connected (e.g. with jumper)- in order to determine
// if duplex transfers should receive the same data that was sent to the bus..
#include <assp/omap3530_assp/omap3530_scm.h>
#include <assp/omap3530_assp/omap3530_gpio.h>
const TInt KSpi3_SIMO_Pin = 131;
const TInt KSpi3_SOMI_Pin = 132;

inline TBool IsLoopbackAvailable()
	{
	// first check, if pad is configured to use SPI (this will confirm, if pins are used that way)
	// this is their configuration (EMode1)
	//	CONTROL_PADCONF_MMC2_CLK,  SCM::EMsw, SCM::EMode1, // mcspi3_simo
	//  CONTROL_PADCONF_MMC2_DAT0, SCM::ELsw, SCM::EMode1, // mcspi3_somi
	TUint mode_MMC2_CLK = SCM::GetPadConfig(CONTROL_PADCONF_MMC2_CLK, SCM::EMsw);
	TUint mode_MMC2_DAT0 = SCM::GetPadConfig(CONTROL_PADCONF_MMC2_DAT0, SCM::ELsw);

	if(!(mode_MMC2_CLK & SCM::EMode1) ||
	   !(mode_MMC2_DAT0 & SCM::EMode1))
		{
		return EFalse; // either other pins are used or SPI3 is not configured at all..
		}

	// swap pins to be GPIO (EMode4)
	SCM::SetPadConfig(CONTROL_PADCONF_MMC2_CLK, SCM::EMsw, SCM::EMode4 | SCM::EInputEnable);
	SCM::SetPadConfig(CONTROL_PADCONF_MMC2_DAT0, SCM::ELsw, SCM::EMode4 | SCM::EInputEnable);

	// set SIMO pin as output
	GPIO::SetPinDirection(KSpi3_SIMO_Pin, GPIO::EOutput);
	GPIO::SetPinMode(KSpi3_SIMO_Pin, GPIO::EEnabled);

	// and SOMI pin as input
	GPIO::SetPinDirection(KSpi3_SOMI_Pin, GPIO::EInput);
	GPIO::SetPinMode(KSpi3_SOMI_Pin, GPIO::EEnabled);

	TBool result = ETrue;
	GPIO::TGpioState pinState = GPIO::EHigh;

	// test 1: set SIMO to ELow and check if SOMI is the same
	GPIO::SetOutputState(KSpi3_SIMO_Pin, GPIO::ELow);
	GPIO::GetInputState(KSpi3_SOMI_Pin, pinState);
	if(pinState != GPIO::ELow)
		{
		result = EFalse;
		}
	else
		{
		// test 2: set SIMO to EHigh and check if SOMI is the same
		GPIO::SetOutputState(KSpi3_SIMO_Pin, GPIO::EHigh);
		GPIO::GetInputState(KSpi3_SOMI_Pin, pinState);
		if(pinState != GPIO::EHigh)
			{
			result = EFalse;
			}
		}

	// restore back oryginal pad configuration for these pins
	SCM::SetPadConfig(CONTROL_PADCONF_MMC2_CLK,  SCM::EMsw, mode_MMC2_CLK);
	SCM::SetPadConfig(CONTROL_PADCONF_MMC2_DAT0, SCM::ELsw, mode_MMC2_DAT0);

	return result;
	}

#endif /* __KERNEL_MODE__ */

#endif /* __D_SPI_CLIENT_MASTER__ */

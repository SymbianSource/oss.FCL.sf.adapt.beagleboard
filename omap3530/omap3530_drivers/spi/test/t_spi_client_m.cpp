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
// omap3530_drivers/spi/test/t_spi_client_master.cpp
//
// user-side application for the simple SPI client (master) test driver.
//

#include <e32test.h>
#include <e32cmn.h>
#include "d_spi_client_m.h"


// global data..
_LIT(testName,"T_SPI_CLIENT_M");
GLDEF_D RTest test(testName);

GLDEF_D RSpiClientTest testLdd; // the actual driver..

void PrepareDriver()
	{
	// Load the Test Driver
	TInt r = User::LoadLogicalDevice(KLddFileName);
	if(r != KErrNone && r != KErrAlreadyExists)
		{
		// fail..
		test.Printf(_L("\nFailed to load the driver, r=%d"), r);
		test(EFalse);
		}

	// Open the driver
	r = testLdd.Open();
	if(r != KErrNone)
		{
		test.Printf(_L("Failed to open the driver..\n\r"));
		test(EFalse);
		}
	}

void ReleaseDriver()
	{
	// Close the driver
	testLdd.Close();

	// unload the driver
	User::FreeLogicalDevice(KLddFileName);
	}

inline void TestError(TInt r)
	{
	if(r == KErrNotSupported) // this is to warn stuf not yet implemented (TDD)
		{
		test.Printf(_L("!! Warning: not implemented yet..\n\r"));
		}
	else
		{
		test(r == KErrNone);
		}
	}

void TestSynchronousOperation()
	{
	test.Next(_L("TestSynchronousOperation()"));

	test.Next(_L("HalfDuplexSingleWrite()"));
	TestError(testLdd.HalfDuplexSingleWrite());

	test.Next(_L("HalfDuplexMultipleWrite()"));
	TestError(testLdd.HalfDuplexMultipleWrite());

	test.Next(_L("HalfDuplexSingleRead()"));
	TestError(testLdd.HalfDuplexSingleRead());

	test.Next(_L("HalfDuplexMultipleRead()"));
	TestError(testLdd.HalfDuplexMultipleRead());

	test.Next(_L("HalfDuplexMultipleWriteRead()"));
	TestError(testLdd.HalfDuplexMultipleWriteRead());

	test.Next(_L("FullDuplexSingle()"));
	TestError(testLdd.FullDuplexSingle());

	test.Next(_L("FullDuplexMultiple()"));
	TestError(testLdd.FullDuplexMultiple());
	}


TInt E32Main()
	{
	test.Title();
	test.Start(_L("Testing SPI.."));

	PrepareDriver();

	TestSynchronousOperation();

	ReleaseDriver();

	test.End();

	return KErrNone;
	}


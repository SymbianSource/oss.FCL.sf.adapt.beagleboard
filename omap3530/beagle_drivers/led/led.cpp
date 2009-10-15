// Copyright (c) 1994-2009 Nokia Corporation and/or its subsidiary(-ies).
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
//
// Description:
// omap3530/beagle_drivers/led/led.cpp
//

#include <kern_priv.h>
#include <beagle/beagle_gpio.h>
#include <assp/omap3530_assp/omap3530_gpio.h>
#include <beagle/variant.h>
#include <assp/omap3530_assp/omap3530_assp_priv.h>
#include <assp/omap3530_assp/omap3530_irqmap.h> // GPIO interrupts

#include <assp.h> // Required for definition of TIsr

static NTimer * heartBeatTimer;



static void ledIsr(TAny* aPtr)
	{									
	//make sure the led is always in the sma estate when we crash
	
	GPIO::SetOutputState(KGPIO_LED1, GPIO::ELow);
	Kern::Fault("User invoked crash via keypad",KErrDied);
	}

static void beatLedHeartBeat(TAny * ptr)
	{
	GPIO::TGpioState ledState;
	GPIO::GetOutputState(KGPIO_LED1, ledState);
		
	if(GPIO::EHigh == ledState)
		{
		GPIO::SetOutputState(KGPIO_LED1, GPIO::ELow);
		}
	else
		{
		GPIO::SetOutputState(KGPIO_LED1, GPIO::EHigh);
		}
	
	heartBeatTimer->Again(Variant::GetMsTickPeriod());
	}


DECLARE_STANDARD_EXTENSION()
	{
		
	//Set up the button to proivde a panic button invoking Fault()
	if(KErrNone != GPIO::SetPinDirection(KGPIO_UserButton, GPIO::EInput))
		return KErrArgument;
		
	GPIO::SetPinMode(KGPIO_UserButton, GPIO::EEnabled);
	GPIO::SetDebounceTime(KGPIO_UserButton, 500);
	
	if(KErrNone !=GPIO::BindInterrupt(KGPIO_UserButton, ledIsr,NULL))
		return KErrArgument;
		
	if(KErrNone !=GPIO::SetInterruptTrigger(KGPIO_UserButton, GPIO::EEdgeRising))
		return KErrArgument;
		
	if(KErrNone !=GPIO::EnableInterrupt(KGPIO_UserButton))
		{
		GPIO::UnbindInterrupt(KGPIO_UserButton);
		return KErrInUse;
		}		

	//setup the Led to flash at the system tick rate ( heartbeat) 
	heartBeatTimer = new NTimer(beatLedHeartBeat,NULL);
	
	if(KErrNone != GPIO::SetPinDirection(KGPIO_LED1, GPIO::EOutput))
			return KErrArgument;
	
	if(KErrNone != GPIO::SetPinDirection(KGPIO_LED0, GPIO::EOutput))
				return KErrArgument;

	GPIO::SetPinMode(KGPIO_LED0, GPIO::EEnabled);
	GPIO::SetPinMode(KGPIO_LED1, GPIO::EEnabled);
	GPIO::SetOutputState(KGPIO_LED0, GPIO::ELow);	
	
	heartBeatTimer->OneShot(Variant::GetMsTickPeriod(),ETrue);
	return KErrNone;	
	}


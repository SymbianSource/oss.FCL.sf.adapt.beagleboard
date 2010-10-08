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
// omap3530/omap3530_drivers/spi/psl_init.h
//


#ifndef __OMAP3530_SPI_PSL_H__
#define __OMAP3530_SPI_PSL_H__

const TInt KIicPslNumOfChannels = 2; // Number of channels supported // TODO only two 3 and 4 for now..
// FIXME - there is a crash when using channels 1 and 2 - when accesing registers at e.g. 0xc609a000

struct TIicOperationType
    {
    enum TOperation
        {
        ENop             = 0x00,
        ETransmitOnly    = 0x01,
        EReceiveOnly     = 0x02,
        ETransmitReceive = 0x03
        };

    struct TOp
        {
        TUint8 iIsTransmitting : 1;
        TUint8 iIsReceiving    : 1;
        TUint8 iRest           : 6;
        };

    union
        {
        TOp iOp;
        TUint8 iValue;
        };
    };

#endif /*__OMAP3530_SPI_PSL_H__*/

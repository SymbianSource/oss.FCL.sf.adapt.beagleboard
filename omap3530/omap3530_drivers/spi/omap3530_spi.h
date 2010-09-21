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
// omap3530/omap3530_drivers/spi/omap3530_spi.h
//
// This file contains definitions to internal SPI implementation and register definitions
// It is not intended to be exported - SPI registers must not be modified from outside of
// the driver!
//

#ifndef __OMAP3530_SPI_H__
#define __OMAP3530_SPI_H__

#include <assp/omap3530_assp/omap3530_scm.h>


#define BIT_MASK(shift,len)       (((1u << (len)) - 1) << (shift))
#define GET_BITS(w,shift,len)     (((w) >> (shift)) & ((1 << (len)) - 1))
#define SET_BITS(w,set,shift,len) ((w) &= ~BIT_MASK(shift, len), (w) |= ((set) << (shift)))


// Device Instance Summary
const TUint MCSPI1_phys = 0x48098000; // 4Kbytes
const TUint MCSPI2_phys = 0x4809A000; // 4Kbytes
const TUint MCSPI3_phys = 0x480B8000; // 4Kbytes
const TUint MCSPI4_phys = 0x480BA000; // 4Kbytes

const TUint MCSPI1 = Omap3530HwBase::TVirtual<MCSPI1_phys>::Value;
const TUint MCSPI2 = Omap3530HwBase::TVirtual<MCSPI2_phys>::Value;
const TUint MCSPI3 = Omap3530HwBase::TVirtual<MCSPI3_phys>::Value;
const TUint MCSPI4 = Omap3530HwBase::TVirtual<MCSPI4_phys>::Value;

// map of SPI base addresses..
const TUint KMcSpiRegBase[] =
	{
	Omap3530HwBase::TVirtual<MCSPI1_phys>::Value, //McSPI module 1
	Omap3530HwBase::TVirtual<MCSPI2_phys>::Value, //McSPI module 2
	Omap3530HwBase::TVirtual<MCSPI3_phys>::Value, //McSPI module 3
	Omap3530HwBase::TVirtual<MCSPI4_phys>::Value  //McSPI module 4
	};


//.. and IRQ lines for SPI channels
const TUint KMcSpiIrqId[] =
	{
	EOmap3530_IRQ65_SPI1_IRQ, //McSPI module 1
	EOmap3530_IRQ66_SPI2_IRQ, //McSPI module 2
	EOmap3530_IRQ91_SPI3_IRQ, //McSPI module 3
	EOmap3530_IRQ48_SPI4_IRQ  //McSPI module 4
	};

// available channels per module (i.e. number of 'slave select'  inputs/outpus per module)
const TUint KMcSpiNumChannels[] =
	{
	4, // channels 0 - 3
	2, // channels 0 - 1
	2, // channels 0 - 1
	1  // channel 0  only
	};

//---------------------------------------------------------------
// MCSPI Registers offsets and bits definitions
//----------------------------------
// MCSPI_REVISION
//----------------------------------
const TUint MCSPI_REVISION = 0x00;


//----------------------------------
// MCSPI_SYSCONFIG
//----------------------------------
const TUint MCSPI_SYSCONFIG = 0x10;

// 9:8 CLOCKACTIVITY Clocks activity during wake up mode period
//0x0: Interface and Functional clocks may be switched off.
//0x1: Interface clock is maintained. Functional clock may be switched off.
//0x2: Functional clock is maintained. Interface clock may be switched off.
//0x3: Interface and Functional clocks are maintained.
const TUint MCSPI_SYSCONFIG_CLOCKACTIVITY_ALL_OFF        = 0 << 8;
const TUint MCSPI_SYSCONFIG_CLOCKACTIVITY_INT_ON_FUN_OFF = 1 << 8;
const TUint MCSPI_SYSCONFIG_CLOCKACTIVITY_INT_OFF_FUN_ON = 2 << 8;
const TUint MCSPI_SYSCONFIG_CLOCKACTIVITY_ALL_ON         = 3 << 8;

// 4:3 SIDLEMODE Power management
const TUint MCSPI_SYSCONFIG_SIDLEMODE_ALWAYS = 0 << 3;
const TUint MCSPI_SYSCONFIG_SIDLEMODE_IGNORE = 1 << 3;
const TUint MCSPI_SYSCONFIG_SIDLEMODE_COND   = 2 << 3;
const TUint MCSPI_SYSCONFIG_ENAWAKEUP = 1 << 2; // 0x1: Wake-up capability enabled
const TUint MCSPI_SYSCONFIG_SOFTRESET = 1 << 1; // Software reset. Read always returns 0.
const TUint MCSPI_SYSCONFIG_AUTOIDLE  = 1 << 0; // Internal interface Clock gating strategy


//----------------------------------
// MCSPI_SYSSTATUS
//----------------------------------
const TUint MCSPI_SYSSTATUS = 0x14;
const TUint MCSPI_SYSSTATUS_RESETDONE = 1 << 0;


//----------------------------------
// MCSPI_IRQSTATUS
//----------------------------------
const TUint MCSPI_IRQSTATUS = 0x18; // for each bit- write 0x1: reset status, Read 0x1: Event is pending.
const TUint MCSPI_IRQSTATUS_EOW           = 1 << 17; // End of word count event when a channel is enabled using
const TUint MCSPI_IRQSTATUS_WKS           = 1 << 16; // Wake-up event in slave mode when an active control signal is detected
const TUint MCSPI_IRQSTATUS_RX3_FULL      = 1 << 14; // MCSPI_RX3 register is full (only when channel 3 is enabled)
const TUint MCSPI_IRQSTATUS_TX3_UDERFLOW  = 1 << 13; // MCSPI_TX3 register underflow (only when channel 3 is enabled)(1)
const TUint MCSPI_IRQSTATUS_TX3_EMPTY     = 1 << 12; // MCSPI_TX3 register is empty (only when channel 3 is enabled)(2)
const TUint MCSPI_IRQSTATUS_RX2_FULL      = 1 << 10; // MCSPI_RX2 register full (only when channel 2 is enabled)
const TUint MCSPI_IRQSTATUS_TX2_UNDERFLOW = 1 <<  9; // MCSPI_TX2 register underflow (only when channel 2 is enabled)(1)
const TUint MCSPI_IRQSTATUS_TX2_EMPTY     = 1 <<  8; // MCSPI_TX2 register empty (only when channel 2 is enabled)(2)
const TUint MCSPI_IRQSTATUS_RX1_FULL      = 1 <<  6; // MCSPI_RX1 register full (only when channel 1 is enabled)
const TUint MCSPI_IRQSTATUS_TX1_UNDERFLOW = 1 <<  5; // MCSPI_TX1 register underflow (only when channel 1 is enabled)(1)
const TUint MCSPI_IRQSTATUS_TX1_EMPTY     = 1 <<  4; // MCSPI_TX1 register empty (only when channel 1 is enabled)(3)
const TUint MCSPI_IRQSTATUS_RX0_OVERFLOW  = 1 <<  3; // MCSPI_RX0 register overflow (only in slave mode)
const TUint MCSPI_IRQSTATUS_RX0_FULL      = 1 <<  2; // MCSPI_RX0 register full (only when channel 0 is enabled)
const TUint MCSPI_IRQSTATUS_TX0_UNDERFLOW = 1 <<  1; // MCSPI_TX0 register underflow (only when channel 0 is
const TUint MCSPI_IRQSTATUS_TX0_EMPTY     = 1 <<  0; // MCSPI_TX0 register empty (only when channel 0 is enabled)(3)

//----------------------------------
// MCSPI_IRQENABLE
//----------------------------------
//0x0: Interrupt disabled
//0x1: Interrupt enabled
const TUint MCSPI_IRQENABLE = 0x1C;
const TUint MCSPI_IRQENABLE_EOWKE         = 1 << 17; // End of Word count Interrupt Enable.
const TUint MCSPI_IRQENABLE_WKE           = 1 << 16; // Wake-up event interrupt enable in slave mode when an
const TUint MCSPI_IRQENABLE_RX3_FULL      = 1 << 14; // MCSPI_RX3 register full interrupt enable (channel 3)
const TUint MCSPI_IRQENABLE_TX3_UNDERFLOW = 1 << 13; // MCSPI_TX3 register underflow interrupt enable (channel 3)
const TUint MCSPI_IRQENABLE_TX3_EMPTY     = 1 << 12; // MCSPI_TX3 register empty interrupt enable (channel 3)
const TUint MCSPI_IRQENABLE_RX2_FULL      = 1 << 10; // MCSPI_RX2 register full interrupt enable (channel 2)
const TUint MCSPI_IRQENABLE_TX2_UNDERFLOW = 1 << 9;  // MCSPI_TX2 register underflow interrupt enable (channel 2)
const TUint MCSPI_IRQENABLE_TX2_EMPTY     = 1 << 8;  // MCSPI_TX2 register empty interrupt enable (channel 2)
const TUint MCSPI_IRQENABLE_RX1_FULL      = 1 << 6;  // MCSPI_RX1 register full interrupt enable (channel 1)
const TUint MCSPI_IRQENABLE_TX1_UNDERFLOW = 1 << 5;  // MCSPI_TX1 register underflow interrupt enable (channel 1)
const TUint MCSPI_IRQENABLE_TX1_EMPTY     = 1 << 4;  // MCSPI_TX1 register empty interrupt enable (channel 1)
const TUint MCSPI_IRQENABLE_RX0_OVERFLOW  = 1 << 3;  // MCSPI_RX0 register overflow interrupt enable (channel 0) (only Slave)
const TUint MCSPI_IRQENABLE_RX0_FULL      = 1 << 2;  // MCSPI_RX0 register full interrupt enable (channel 0)
const TUint MCSPI_IRQENABLE_TX0_UNDERFLOW = 1 << 1;  // MCSPI_TX0 register underflow interrupt enable (channel 0)
const TUint MCSPI_IRQENABLE_TX0_EMPTY     = 1 << 0;  // MCSPI_TX0 register empty interrupt enable (channel 0)

// macros to get these flags depending on the channel number..and ommited ENABLE
// - as they are the same for MCSPI_IRQSTATUS register
#define MCSPI_IRQ_RX_OVERFLOW         MCSPI_IRQENABLE_RX0_OVERFLOW  // Channel 0 only / slave mode only
#define MCSPI_IRQ_RX_FULL(chan)      (MCSPI_IRQENABLE_RX0_FULL      << ((chan)*4))
#define MCSPI_IRQ_TX_UNDERFLOW(chan) (MCSPI_IRQENABLE_TX0_UNDERFLOW << ((chan)*4))
#define MCSPI_IRQ_TX_EMPTY(chan)     (MCSPI_IRQENABLE_TX0_EMPTY     << ((chan)*4))


//----------------------------------
// MCSPI_WAKEUPENABL
//----------------------------------
const TUint MCSPI_WAKEUPENABL = 0x20;
const TUint MCSPI_WAKEUPENABL_WKEN = 1 << 0; //0x1: The event is allowed to wake-up the system


//----------------------------------
// MCSPI_SYST
//----------------------------------
const TUint MCSPI_SYST = 0x24;
const TUint MCSPI_SYST_SSB        = 1 << 11; // Set status bit: 0x1: Force to 1 all status bits of MCSPI_ IRQSTATUS
const TUint MCSPI_SYST_SPIENDIR   = 1 << 10; // spim_cs and spim_clk direction: 0x0: Output (as in master mode), 0x1: Input (as in slave mode)
const TUint MCSPI_SYST_SPIDATDIR1 = 1 << 9;  // SPIDAT[1] (spim_simo) direction- 0x0: Output, 0x1: Input
const TUint MCSPI_SYST_SPIDATDIR0 = 1 << 8;  // Set the direction of the SPIDAT[0] (spim_somi) RW 0
const TUint MCSPI_SYST_WAKD       = 1 << 7;  // SWAKEUP output 0x0: The pin is driven low, 0x1: The pin is driven high.
const TUint MCSPI_SYST_SPICLK     = 1 << 6;  // spim_clk line (signal data value) RW 0
const TUint MCSPI_SYST_SPIDAT_1   = 1 << 5;  // spim_somi line (signal data value) RW 0
const TUint MCSPI_SYST_SPIDAT_0   = 1 << 4;  // spim_simo line (signal data value)
const TUint MCSPI_SYST_SPIEN_3    = 1 << 3;  // spim_cs3 line (signal data value) RW 0
const TUint MCSPI_SYST_SPIEN_2    = 1 << 2;  // spim_cs2 line (signal data value) RW 0
const TUint MCSPI_SYST_SPIEN_1    = 1 << 1;  // spim_cs1 line (signal data value) RW 0
const TUint MCSPI_SYST_SPIEN_0    = 1 << 0;  // spim_cs0 line (signal data value) RW 0


//----------------------------------
// MCSPI_MODULCTRL
//----------------------------------
const TUint MCSPI_MODULCTRL = 0x28;
const TUint MCSPI_MODULCTRL_SYSTEM_TEST = 1 << 3; // 0x0: Functional mode, 0x1: System test mode (SYSTEST)
const TUint MCSPI_MODULCTRL_MS_SLAVE    = 1 << 2; // Master / Slave mode 0x0: Master, 0x1: Slave
const TUint MCSPI_MODULCTRL_MS_MASTER   = 0;      // this is spurious definition -> not to write '0' magic number -defined this..
const TUint MCSPI_MODULCTRL_SINGLE      = 1 << 0; // Single forced channel/multichannel (master mode only)
                                                  // MCSPI_CHxCONF_FORCE bit has to be set in this mode, TURBO cleared (recomended)


//----------------------------------
// MCSPI_CHxCONF - channel config
//----------------------------------
// x = 0 to 3 for MCSPI1.
// x = 0 to 1 for MCSPI2 and MCSPI3.
// x = 0 for MCSPI4.
#define MCSPI_CHxCONF(x) (0x2C + 0x14 * (x))
const TUint MCSPI_CHxCONF_CLKG = 1 << 29; //  Clock divider granularity.
const TUint MCSPI_CHxCONF_FFER = 1 << 28; // FIFO enabled for Receive.
const TUint MCSPI_CHxCONF_FFEW = 1 << 27; // FIFO enabled for Transmit. Only one channel can have this bit field set.

// [26:25] TCS Chip select time control
// Defines the number of interface clock cycles between CS toggling and first (or last) edge of SPI clock.
const TUint MCSPI_CHxCONF_TCS_0_5 = 0 << 25; // 0x0: 0.5 clock cycle
const TUint MCSPI_CHxCONF_TCS_1_5 = 1 << 25; // 0x1: 1.5 clock cycles
const TUint MCSPI_CHxCONF_TCS_2_5 = 2 << 25; // 0x2: 2.5 clock cycles
const TUint MCSPI_CHxCONF_TCS_3_5 = 3 << 25; // 0x3: 3.5 clock cycles

const TUint MCSPI_CHxCONF_SBPOL = 1 << 24; // Start bit polarity (0: spi word is command, 1: spi word is data)
const TUint MCSPI_CHxCONF_SBE   = 1 << 23; // Start bit enable - 0x1: Start bit D/CX added before transfer.
const TUint MCSPI_CHxCONF_FORCE = 1 << 20; // Manual spim_csx assertion to keep spim_csx active between SPI words.
                                           // (single channel master mode only)- MCSPI_MODULCTRL_SINGLE has to be set
const TUint MCSPI_CHxCONF_TURBO = 1 << 19; // Turbo mode
const TUint MCSPI_CHxCONF_IS    = 1 << 18; // Input select- 0x0: (spim_somi), 0x1: (spim_simo) selected for reception
const TUint MCSPI_CHxCONF_DPE1  = 1 << 17; // Transmission enable for data line 1 (spim_simo) RW 0x1
const TUint MCSPI_CHxCONF_DPE0  = 1 << 16; // Transmission enable for data line 0 (spim_somi)
const TUint MCSPI_CHxCONF_DMAR  = 1 << 15; // DMA Read request
const TUint MCSPI_CHxCONF_DMAW  = 1 << 14; // DMA Write request.

// 13:12 TRM Transmit/receive modes
const TUint MCSPI_CHxCONF_TRM_TRANSMIT_RECEIVE = 0 << 12;
const TUint MCSPI_CHxCONF_TRM_RECEIVE_ONLY     = 1 << 12;
const TUint MCSPI_CHxCONF_TRM_TRANSMIT_ONLY    = 2 << 12;
// these are to be cleared in the register
const TUint MCSPI_CHxCONF_TRM_NO_TRANSMIT      = 1 << 12;
const TUint MCSPI_CHxCONF_TRM_NO_RECEIVE       = 2 << 12;


// 11:7 WL SPI word length0
// values:<0-3> reserved, allowed:<4-31> => word_size = value + 1 (i.e. for 4: word size = 5)
const TInt KMinSpiWordWidth = 5;
const TUint MCSPI_CHxCONF_WL_OFFSET = 7;
#define MCSPI_CHxCONF_WL(x) ( (((x) - 1) & BIT_MASK(0, 5)) << MCSPI_CHxCONF_WL_OFFSET )

const TUint MCSPI_CHxCONF_EPOL_LOW = 1 << 6; // spim_csx polarity 0x0: active high, 0x1: active low

// A programmable clock divider divides the SPI reference clock
//5:2 CLKD Frequency divider for spim_clk (for master device only)
const TUint MCSPI_CHxCONF_CLKD_48M   = 0x0 << 2; //0x0: 1    = 48 MHz
const TUint MCSPI_CHxCONF_CLKD_24M   = 0x1 << 2; //0x1: 2    = 24 MHz
const TUint MCSPI_CHxCONF_CLKD_12M   = 0x2 << 2; //0x2: 4    = 12 MHz
const TUint MCSPI_CHxCONF_CLKD_6M    = 0x3 << 2; //0x3: 8    = 6 MHz
const TUint MCSPI_CHxCONF_CLKD_3M    = 0x4 << 2; //0x4: 16   = 3 MHz
const TUint MCSPI_CHxCONF_CLKD_1500k = 0x5 << 2; //0x5: 32   = 1.5 MHz
const TUint MCSPI_CHxCONF_CLKD_750k  = 0x6 << 2; //0x6: 64   = 750 kHz
const TUint MCSPI_CHxCONF_CLKD_375k  = 0x7 << 2; //0x7: 128  = 375 kHz
const TUint MCSPI_CHxCONF_CLKD_187k  = 0x8 << 2; //0x8: 256  = 187.5 kHz
const TUint MCSPI_CHxCONF_CLKD_93k   = 0x9 << 2; //0x9: 512  = 93.75 kHz
const TUint MCSPI_CHxCONF_CLKD_46k   = 0xA << 2; //0xA: 1024 = 46.875 kHz
const TUint MCSPI_CHxCONF_CLKD_23k   = 0xB << 2; //0xB: 2048 = 23.437,5 kHz
const TUint MCSPI_CHxCONF_CLKD_11k   = 0xC << 2; //0xC: 4096 = 11.718,75 kHz
const TUint MCSPI_K48MHz = 48000000;

const TUint MCSPI_CHxCONF_POL = 1 << 1; // spim_clk polarity 0x0: active high, 0x1: active low
const TUint MCSPI_CHxCONF_PHA = 1 << 0; // spim_clk phase
// 0x0: Data are latched on odd-numbered edges of spim_clk.
// 0x1: Data are latched on even-numbered edges of spim_clk.


//----------------------------------
// MCSPI_CHxSTAT
//----------------------------------
// x = 0 to 3 for MCSPI1.
// x = 0 to 1 for MCSPI2 and MCSPI3.
// x = 0 for MCSPI4.
#define MCSPI_CHxSTAT(x) (0x30 + 0x14 * (x))
const TUint MCSPI_CHxSTAT_RXFFF = 1 << 6; // Channel x FIFO Receive Buffer Full
const TUint MCSPI_CHxSTAT_RXFFE = 1 << 5; // Channel x FIFO Receive Buffer Empty
const TUint MCSPI_CHxSTAT_TXFFF = 1 << 4; // Channel x FIFO Transmit Buffer Full
const TUint MCSPI_CHxSTAT_TXFFE = 1 << 3; // Channel x FIFO Transmit Buffer Empty
const TUint MCSPI_CHxSTAT_EOT   = 1 << 2; // Channel x end-of-transfer status.
const TUint MCSPI_CHxSTAT_TXS   = 1 << 1; // Channel x MCSPI_TXx register status R 0x0
const TUint MCSPI_CHxSTAT_RXS   = 1 << 0; // Channel x MCSPI_RXx register status R 0x0


//----------------------------------
// MCSPI_CHxCTRL
//----------------------------------
// x = 0 to 3 for MCSPI1.
// x = 0 to 1 for MCSPI2 and MCSPI3.
// x = 0 for MCSPI4.
#define MCSPI_CHxCTRL(x) (0x34 + 0x14 * (x))
//15:8 EXTCLK Clock ratio extension: This register is used to concatenate with RW 0x00
const TUint MCSPI_CHxCTRL_EXTCLK_1      = 0x00 << 8; //0x0: Clock ratio is CLKD + 1
const TUint MCSPI_CHxCTRL_EXTCLK_1_16   = 0x01 << 8; //0x1: Clock ratio is CLKD + 1 + 16
const TUint MCSPI_CHxCTRL_EXTCLK_1_4080 = 0xff << 8; //0xFF: Clock ratio is CLKD + 1 + 4080
const TUint MCSPI_CHxCTRL_EN            = 0x01 << 0; // Channel enable


//----------------------------------
// MCSPI_TXx - Channel x Data to transmit
//----------------------------------
// x = 0 to 3 for MCSPI1.
// x = 0 to 1 for MCSPI2 and MCSPI3.
// x = 0 for MCSPI4.
#define MCSPI_TXx(x)     (0x38 + 0x14 * (x)) // Channel x Data to transmit


//----------------------------------
// MCSPI_RXx - Channel x Received Data
//----------------------------------
// x = 0 to 3 for MCSPI1.
// x = 0 to 1 for MCSPI2 and MCSPI3.
// x = 0 for MCSPI4.
#define MCSPI_RXx(x)     (0x3C + 0x14 * (x)) // Channel x Received Data


//----------------------------------
// MCSPI_XFERLEVEL
//----------------------------------
const TUint MCSPI_XFERLEVEL = 0x7C;
const TUint MCSPI_XFERLEVEL_WCNT_OFFSET = 16; // [31:16] WCNT Spi word counter -> how many bytes are transfered to FIFO before tx is enabled
const TUint MCSPI_XFERLEVEL_AFL_OFFSET  = 8;  // 13:8 AFL Buffer Almost Full. 0x0: One byte , 0x1: 2 bytes, x3E: 63 bytes.. etc
const TUint MCSPI_XFERLEVEL_AEL_OFFSET  = 0;  // 5:0 AEL Buffer Almost Empty (threshold?) 0x0: One byte. 0x1: 2 bytes..

#define MCSPI_XFERLEVEL_WCNT(x) ((x)  << MCSPI_XFERLEVEL_WCNT_OFFSET)
#define MCSPI_XFERLEVEL_AFL(x)  (((x) << MCSPI_XFERLEVEL_AFL_OFFSET))
#define MCSPI_XFERLEVEL_AEL(x)  (((x) << MCSPI_XFERLEVEL_AEL_OFFSET))


//----------------------------------
// PAD (PIN) configuration for SPI
//----------------------------------

//#define SPI_CHANNEL_3_PIN_OPTION_2 // TODO - move this to mmp file!
//#define SPI_CHANNEL_3_PIN_OPTION_3

// flags for CS signal pins - in order to keep them in certain state when SPI is inactive..
const TUint KCsPinUp = SCM::EPullUdEnable | SCM::EPullTypeSelect; //
const TUint KCsPinDown = SCM::EPullUdEnable; //

const TUint KCsPinOffHi =  SCM::EOffOutEnable | SCM::EOffOutValue; //
const TUint KCsPinOffLow = SCM::EOffOutEnable; //

const TUint KCsPinModeUp   = /*KCsPinUp   |*//* KCsPinOffHi |*/ SCM::EInputEnable;
const TUint KCsPinModeDown = KCsPinDown | KCsPinOffLow;

const TUint KMaxSpiChannelsPerModule = 4; // there are max 4 channels (McSPI 1)

struct TPinConfig
	{
	TLinAddr              iAddress;
	SCM::TLowerHigherWord iMswLsw;
	TUint16               iFlags;
	};

struct TSpiPinConfig
	{
	TPinConfig iClk;
	TPinConfig iSimo;
	TPinConfig iSomi;
	TPinConfig iCs[KMaxSpiChannelsPerModule];
	};

const TSpiPinConfig TSpiPinConfigMcSpi1 =
	{
	{CONTROL_PADCONF_MCSPI1_CLK,  SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi1_clk
	{CONTROL_PADCONF_MCSPI1_CLK,  SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi1_simo
	{CONTROL_PADCONF_MCSPI1_SOMI, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi1_somi
		{
		{CONTROL_PADCONF_MCSPI1_SOMI, SCM::EMsw, SCM::EMode1}, // mcspi1_cs0
		{CONTROL_PADCONF_MCSPI1_CS1,  SCM::ELsw, SCM::EMode1}, // mcspi1_cs1
		{CONTROL_PADCONF_MCSPI1_CS1,  SCM::EMsw, SCM::EMode1}, // mcspi1_cs2
		{CONTROL_PADCONF_MCSPI1_CS3,  SCM::ELsw, SCM::EMode1}, // mcspi1_cs3
		}
	};

const TSpiPinConfig TSpiPinConfigMcSpi2 =
	{
	{CONTROL_PADCONF_MCSPI1_CS3,  SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi2_clk
	{CONTROL_PADCONF_MCSPI2_SIMO, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi2_simo
	{CONTROL_PADCONF_MCSPI2_SIMO, SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi2_somi
		{
		{CONTROL_PADCONF_MCSPI2_CS0,  SCM::ELsw, SCM::EMode1}, // mcspi2_cs0
		{CONTROL_PADCONF_MCSPI2_CS0,  SCM::EMsw, SCM::EMode1}, // mcspi2_cs1
		{0, SCM::ELsw, 0}, // not supported
		{0, SCM::ELsw, 0}, // not supported
		}
	};


#if defined(SPI_CHANNEL_3_PIN_OPTION_2)
const TSpiPinConfig TSpiPinConfigMcSpi3 =
	{
	{CONTROL_PADCONF_DSS_DATA18, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_clk
	{CONTROL_PADCONF_DSS_DATA18, SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_simo
	{CONTROL_PADCONF_DSS_DATA20, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_somi
		{
		{CONTROL_PADCONF_DSS_DATA20, SCM::EMsw, SCM::EMode1}, // mcspi3_cs0
		{CONTROL_PADCONF_DSS_DATA20, SCM::ELsw, SCM::EMode1}, // mcspi3_cs1
		{0, SCM::ELsw, 0}, // not supported
		{0, SCM::ELsw, 0}, // not supported
		}
	};
#elif defined(SPI_CHANNEL_3_PIN_OPTION_3)
const TSpiPinConfig TSpiPinConfigMcSpi3 =
	{
	{CONTROL_PADCONF_ETK_D2, SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_clk
	{CONTROL_PADCONF_ETK_D0, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_simo
	{CONTROL_PADCONF_ETK_D0, SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_somi
		{
		{CONTROL_PADCONF_ETK_D2, SCM::ELsw, SCM::EMode1}, // mcspi3_cs0
		{CONTROL_PADCONF_ETK_D6, SCM::EMsw, SCM::EMode1}, // mcspi3_cs1
		{0, SCM::ELsw, 0}, // not supported
		{0, SCM::ELsw, 0}, // not supported
		}
	};
#else // default option (for beagle- these are pins on the extension header)
const TSpiPinConfig TSpiPinConfigMcSpi3 =
	{
	{CONTROL_PADCONF_MMC2_CLK,  SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_clk
	{CONTROL_PADCONF_MMC2_CLK,  SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_simo
	{CONTROL_PADCONF_MMC2_DAT0, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi3_somi
		{
		{CONTROL_PADCONF_MMC2_DAT2, SCM::EMsw, SCM::EMode1}, // mcspi3_cs0
		{CONTROL_PADCONF_MMC2_DAT2, SCM::ELsw, SCM::EMode1}, // mcspi3_cs1
		{0, SCM::ELsw, 0}, // not supported
		{0, SCM::ELsw, 0}, // not supported
		}
	};
#endif

const TSpiPinConfig TSpiPinConfigMcSpi4 =
	{
	{CONTROL_PADCONF_MCBSP1_CLKR, SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi4_clk
	{CONTROL_PADCONF_MCBSP1_DX,   SCM::ELsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi4_simo
	{CONTROL_PADCONF_MCBSP1_DX,   SCM::EMsw, SCM::EMode1 | SCM::EInputEnable}, // mcspi4_somi
		{
		{CONTROL_PADCONF_MCBSP_CLKS, SCM::EMsw, SCM::EMode1}, // mcspi3_cs0
		{0, SCM::ELsw, 0}, // not supported
		{0, SCM::ELsw, 0}, // not supported
		{0, SCM::ELsw, 0}, // not supported
		}
	};

const TSpiPinConfig ModulePinConfig[] =
	{
	TSpiPinConfigMcSpi1,
	TSpiPinConfigMcSpi2,
	TSpiPinConfigMcSpi3,
	TSpiPinConfigMcSpi4
	};

#include "omap3530_spi.inl"

#endif /* __OMAP3530_SPI_H__ */

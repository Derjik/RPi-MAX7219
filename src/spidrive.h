/*
 * Copyright (c) 2016 Julien "Derjik" Laurent <julien.laurent@engineer.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>	// printf()
#include <stdlib.h>	// exit()
#include <fcntl.h>	// open()
#include <unistd.h>	// close()
#include <sys/mman.h>	// mmap()
#include <stdint.h>	// uint32_t & uint8_t

// 6 SPI registers, 4 bytes each
#define MAP_LENGTH (4 * 6)

// Base address for BCM2708 peripherals
#define BCM2708_PERI_BASE 0x20000000
// Offset to SPI registers
#define SPI_OFFSET 0x00204000

// Offset to Control and Status register
#define CS_OFFSET 0x0
/*
	This structure 
 */
struct CS_Register
{
	volatile unsigned CS : 2;
	// Chip Select:	00 = CS0
	//		01 = CS1
	//		10 = CS2
	//		11 = Reserved

	volatile unsigned CPHA : 1;
	// Clock Phase:	0 = 1st transition at middle of data bit,
	//		1 = 1st transition at beginning of data bit

	volatile unsigned CPOL : 1;
	// Clock Polarity:	0 = Rest state of clock is low,
	//			1 = Rest state of clock is high

	volatile unsigned CLEAR : 2;
	// Clear FIFO:	00 = No action
	//		x1 = Clear TX FIFO (one shot!)
	//		1x = Clear RX FIFO (ont shot!)

	volatile unsigned CSPOL : 1;
	// Chip Select Polarity:	0 = CS lines are active low
	//				1 = CS lines are active high

	volatile unsigned TA : 1;	
	// Transfer Active:	0 = Transfer not active
	//			1 = Transfer active

	volatile unsigned DMAEN : 1;
	// DMA Enable:	0 = No DMA requests will be issued
	//		1 = Enable DMA operation

	volatile unsigned INTD : 1;
	// Interrupt on Done:	0 = Do not generate interrupt on transfer complete
	//			1 = Generate interrupt when DONE = 1

	volatile unsigned INTR : 1;
	// Interrupt on RXR:	0 = Do not generate interrupts on RX FIFO condition
	//			1 = Generate interrupt while RXR = 1

	volatile unsigned ADCS : 1;
	// Automatically Deassert Chip Select:	0 = Don't automatically deassert Chip
	//					Select at the end of a DMA transfer
	//					1 = Automatically deassert Chip Select
	//					at the end of a DMA transfer

	volatile unsigned REN : 1;
	// Read Enable (in bidirectionnal mode):	0 = Write to the SPI peripheral
	//						1 = Read from the SPI peripheral

	volatile unsigned LEN : 1;
	// LoSSI Enable:	0 = The serial interface will behave as an SPI master
	//			1 = The serial interface will behave as a LoSSI master

	volatile unsigned LMONO : 1;
	// Unused

	volatile unsigned TE_EN : 1;
	// Unused

	const volatile unsigned DONE : 1;
	// Transfer done:	0 = The transfer is in progress (or not active TA = 0)
	//			1 = The transfer is complete (cleared by wditing more data
	//			to the TX FIFO or setting TA to 0

	const volatile unsigned RXD : 1;
	// RX FIFO contains data:	0 = RX FIFO is empty
	//				1 = RX FIFO contains at least 1 byte

	const volatile unsigned TXD : 1;
	// TX FIFO can accept data:	0 = TX FIFO is full and cannot accept more data
	//				1 = TX FIFO has space for at least 1 more byte

	const volatile unsigned RXR : 1;
	// RX FIFO needs Reading (full):	0 = RX FIFO isn't full (or not active TA = 0)
	//					1 = RX FIFO is full (read it naow, you fool!)

	const volatile unsigned RXF : 1;
	// RX FIFO Full:	0 = RX FIFO is not full
	//			1 = RX FIFO is full. No further serial data will be sent or
	//			received until data is read from FIFO.

	volatile unsigned CSPOL0 : 1;
	// Chip Select 0 Polarity:	0 = CS0 is active low
	//				1 = CS0 is active high

	volatile unsigned CSPOL1 : 1;
	// Chip Select 1 Polarity:	0 = CS1 is active low
	//				1 = CS1 is active high

	volatile unsigned CSPOL2 : 1;
	// Chip Select 2 Polarity:	0 = CS2 is active low
	//				1 = CS2 is active high

	volatile unsigned DMA_LEN : 1;
	// Enable DMA mode in LoSSI mode:	0 = DMA inactive in LoSSI
	//					1 = DMA active in LoSSI

	volatile unsigned LEN_LONG : 1;
	// Enable long data word in LoSSI mode if DMA_LEN is set:
	//			0 = Writing to the FIFO will write a single byte
	//			1 = Writing to the FIFO will write a 32 bit word

	unsigned : 6;
	// Reserved. Can't touch this ♪.
};
typedef struct CS_Register CS_Register;
	
// Offset to TX & RX FIFO register
#define FIFO_OFFSET 0x1
// The SPI FIFO register is just a 32 bits-wide FIFO (no shit,
// Sherlock!) so we don't need any sexy bit-field-enabled
// data-structure.

// Offset to Clock Divider register
#define CLK_OFFSET 0x2
// Sexy structure to prevent useless read/write on reserved bits
struct CLK_Register
{
	volatile unsigned CDIV : 16;
	// Clock Divider:	SPI Clock = Core Clock / CDIV
	unsigned : 16;
	// Thou... shall not... TOUUUCH!
};
typedef struct CLK_Register CLK_Register;

// Offset to Data Length register
#define DLEN_OFFSET 0x3
// Drop da struct!
struct DLEN_Register
{
	volatile unsigned LEN : 16;
	// Data length:	The number of bytes to tx/rx (only valid in DMA mode ie. DMAEN = 1)
	unsigned : 16;
	// ACH RESERVIERT KEINE TOUCHEN NEIN NEIN NEIN
};
typedef struct DLEN_Register DLEN_Register;

// Offset to Lossi TOH mode register
#define LTOH_OFFSET 0x4
// Useful register is useful: here we are, marking 28 bits
// as "reserved" and only taking care of the 4 LSB.
struct LTOH_Register
{
	volatile unsigned TOH : 4;
	// Sets the Output Hold delay in APB clocks
	unsigned : 28;
	// Reserved bits are reserved.
};
typedef struct LTOH_Register LTOH_Register;

// Offset to SPI DMA DREQ Controls register
#define DC_OFFSET 0x5
struct DC_Register
{
	volatile unsigned TDREQ : 8;
	// Generate a DREQ signal to the TX DMA engine whenever the
	// TX FIFO level is less than or equal to this amount

	volatile unsigned TPANIC : 8;
	// Generate the Panic signal to the TX DMA engine whenever
	// the TX FIFO level is less than or equal to this amount

        volatile unsigned RDREQ : 8;
	// Generate a DREQ signal to the RX DMA engine whenever the
	// RX FIFO level is grater than this amount

	volatile unsigned RPANIC : 8;
	// Generate the Panic signal ro the RX DMA engine whenever
	// the RX FIFO
};
typedef struct DC_Register DC_Register;

volatile CS_Register* CS;
volatile uint32_t* FIFO;
volatile CLK_Register* CLK;
volatile DLEN_Register* DLEN;
volatile LTOH_Register* LTOH;
volatile DC_Register* DC;

int  mem_fd;		// RAM file descriptor
void *spi_map;		// SPI registers virtual memory map
volatile uint32_t *spi;	// Used to read/write directly to SPI registers


void init_spi();	// Initialize SPI map
void show_CS();
void showAll();

void sendBytes(uint8_t* bytes, unsigned length);
void send(char byte);

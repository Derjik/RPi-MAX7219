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

#include "spidrive.h"

void init_spi()
{
	if ( ( mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0)
	{
		printf("Error: failed to open /dev/mem!\n");
		exit(-1);
	}

	spi_map = mmap(

		NULL,				// We do not care about the map's virtual address
		4,
		PROT_READ|PROT_WRITE,		// Enable reading & writting to mapped memory
		MAP_SHARED,			// Shared with other processes
		mem_fd,				// File to map
		BCM2708_PERI_BASE + SPI_OFFSET	// Offset to SPI peripherals

	);

	close(mem_fd);

	if (spi_map == MAP_FAILED)
	{
		printf("Error: mmap failed to map the SPI registers into virtual memory ! (error %d)\n", (int)spi_map);
		exit(-1);
	}

	spi = (volatile uint32_t *) spi_map;

	// dest	= (cast)			(ptr + offset);
	CS	= (volatile CS_Register*)	(spi + CS_OFFSET);
	FIFO 	= (volatile uint32_t*)		(spi + FIFO_OFFSET);
	CLK	= (volatile CLK_Register*)	(spi + CLK_OFFSET);
	DLEN	= (volatile DLEN_Register*)	(spi + DLEN_OFFSET);
	LTOH	= (volatile LTOH_Register*)	(spi + LTOH_OFFSET);
	DC	= (volatile DC_Register*)	(spi + DC_OFFSET);
}

void show_CS()
{
	printf("CS REGISTER: %08X\n", *(spi));
	printf("________________________________\n");
	printf("Cable Select:\t\t%X\n", CS->CS);
	printf("Clock PHAse:\t\t%X\n", CS->CPHA);
	printf("Clock POLarity:\t\t%X\n", CS->CPOL);
	printf("CLEAR FIFO:\t\t%X\n", CS->CLEAR);
	printf("Chip Select POLarity:\t%X\n", CS->CSPOL);
	printf("Transfer Active:\t%X\n", CS->TA);
	printf("DMA ENabled:\t\t%X\n", CS->DMAEN);
	printf("INTerrupt on Done:\t%X\n", CS->INTD);
	printf("INTerrupt on RXR:\t%X\n", CS->INTR);
	printf("Auto. Deassert CS:\t%X\n", CS->ADCS);
	printf("Read ENable:\t\t%X\n", CS->REN);
	printf("LoSSI ENable:\t\t%X\n", CS->LEN);
	printf("DONE:\t\t\t%X\n", CS->DONE);
	printf("RX Data:\t\t%X\n", CS->RXD);
	printf("TX Data:\t\t%X\n", CS->TXD);
	printf("RX Read:\t\t%X\n", CS->RXR);
	printf("RX Full:\t\t%X\n", CS->RXF);
	printf("CSPOL0:\t\t\t%X\n", CS->CSPOL0);
	printf("CSPOL1:\t\t\t%X\n", CS->CSPOL1);
	printf("CSPOL2:\t\t\t%X\n", CS->CSPOL2);
	printf("\nCLK REGISTER: %08X\n", *(spi+CLK_OFFSET));

	return;
}

void sendBytes(uint8_t* bytes, unsigned length)
{
	unsigned i; 
	CS->TA = 1;

	while(!CS->TXD);

	for(i=0 ; i<length ; ++i)
	{
		*(FIFO) = bytes[i];
	}

	while(!CS->DONE);

	CS->TA = 0;
	CS->CLEAR = 3;

	return;
}

void send(char byte)
{
	printf("[%02X]\n",byte);

	CS->TA = 1;

	while(!CS->TXD);

	*FIFO = byte;

	while(!CS->DONE);

	CS->TA = 0;
	CS->CLEAR = 3;

	return;
}

void randLoop()
{
	int i=0;
	uint8_t msg[2];

	srand(time(NULL));
	
	while(42)
	{
		for(i=1 ; i<=8 ; ++i)
		{
			msg[1] = rand() % 0x100;
			msg[0] = i;
			sendBytes(msg,2);
			usleep(50000);
		}
	}

	return;
}


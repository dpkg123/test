/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include <spu_printf.h>
#include <cell/dma.h>

#include "UnrealTypes.h"
#include "PU_SPU.h"


#define TAG_READ 0
#define TAG_WRITE 1

#define BLOCK_SIZE (16 * 1024)


// the DMA Info to read into
volatile FDMAInfo DMAInfo;
volatile BYTE TempDMABuffer[BLOCK_SIZE]__attribute__((aligned(128)));


void ReadData(DWORD PUBuffer, INT DMAIndex)
{
//	printf("Getting data from %x", (DWORD)(PUBuffer + DMAIndex * 128)); // dmainfo is aligned to 128 bytes big
	// pull in the data via DMA
	DMARead(&DMAInfo, 0, PUBuffer + DMAIndex * 128, 128, TAG_READ);
//	printf("Waiting for dma to finish...");
	SyncDMA(TAG_READ);
}

inline void ReadWrite(DWORD& Offset, DWORD Size)
{
//	printf("DMAing %d bytes at offset %d", Size, Offset);
	// read from main ram
	DMARead(TempDMABuffer, 0, DMAInfo.RAMAddress.Low + Offset, Size, TAG_READ);
	// sync for completeion
	SyncDMA(TAG_READ);

	// write to GPU ram
	DMAWrite(0, DMAInfo.GPUAddress.Low + Offset, TempDMABuffer, Size, TAG_WRITE);
	// sync for completion
	SyncDMA(TAG_WRITE);

	// update the offset
	Offset += Size;
}

void ProcessData()
{
//	printf("DMA from %08x to %08x, %d bytes...", DMAInfo.RAMAddress.Low, DMAInfo.GPUAddress.Low, DMAInfo.Size);
	DWORD Offset = 0;
	// divide the transfer into 16k blocks
	for (INT Block=0; Block < DMAInfo.Size / (BLOCK_SIZE); Block++)
	{
		ReadWrite(Offset, BLOCK_SIZE);
	}

	// if we weren't a multiple of 16k, write the biggest multiple of 16 possible
	DWORD LeftOver = DMAInfo.Size & (BLOCK_SIZE - 1);
	if (!LeftOver)
	{
		return;
	}

	// first, we write the largest multiple of 16 block we can
	DWORD CurrentAmount = LeftOver & ~15;

	// if there's anything, process it
	if (CurrentAmount)
	{
		ReadWrite(Offset, CurrentAmount);
		LeftOver -= CurrentAmount;
	}

#if !FINAL_RELEASE
	if (LeftOver)
	{
		printf("Attempted to DMA a size that is not a multiple of 16 [%d, %d]! Not supported!", DMAInfo.Size, LeftOver);
	}
#endif

	/*
	// now we have to write 8, 4, 2, 1 bytes until done
	DWORD SizeMultiple = 8;
	for (DWORD SizeMultiple = 8; LeftOver != 0; SizeMultiple >>= 1)
	{
	// the 8, 4, 2, or 1 byte block that's left over
	CurrentAmount = LeftOver & SizeMultiple;

	// if there was a chunk of this multiple, process it
	if (CurrentAmount)
	{
	ReadWrite(Offset, CurrentAmount);
	LeftOver -= CurrentAmount;
	}
	}
	*/
}

int main(QWORD PUReadBuffer)
{
	printf("Starting up DMA SPU helper");

	DWORD SemaphoreValue = 1;
	while (1)
	{
//		printf("Waiting for signal");
		INT DMAIndex = WaitForSignal();

//		printf("Got signal! Reading data from %x[%d]", (DWORD)(PUReadBuffer & 0xFFFFFFFF), DMAIndex);
		ReadData(PUReadBuffer, DMAIndex);

//		printf("Proccessing data...");
		ProcessData();

		//		printf("Writing to semaphore %x", DMAInfo.Semaphore.Low);
		// write to GPU ram
//		DMAWrite(DMAInfo.Semaphore.High, DMAInfo.Semaphore.Low, &SemaphoreValue, sizeof(SemaphoreValue), TAG_WRITE);
		// sync for completion
		//		SyncDMA(TAG_WRITE);

//		printf("Sending event... [%d, %d, %d]", EVENT_PORT_KEY_DMA, MESSAGE_DMA_COMPLETE, DMAIndex);
		SendEvent(EVENT_PORT_KEY_DMA, MESSAGE_DMA_COMPLETE, DMAIndex);

		// write that the dma is complete
//		printf("Writing done flag to %x", (DWORD)(PUReadBuffer + DMAIndex * 128 + STRUCT_OFFSET(FDMAInfo, bIsDMAComplete)) & 0xFFFFFFFF);
//		DMAWrite(0, PUReadBuffer + DMAIndex * 128 + STRUCT_OFFSET(FDMAInfo, bIsDMAComplete), &SemaphoreValue, sizeof(SemaphoreValue), TAG_WRITE);
//		// sync for completion
//		SyncDMA(TAG_WRITE);
	}

	return 0;
}

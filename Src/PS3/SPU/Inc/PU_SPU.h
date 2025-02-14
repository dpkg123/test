/*=============================================================================
	PU_SPU.h: Definitions that need to be shared between the PU and SPU
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef PU_SPU_H
#define PU_SPU_H

// ====================================================
// Standard messages used by all PPU Event Queues (0+)
// ====================================================
#define MESSAGE_TERMINATE				0


// ====================================================
// The Keyboard Event Queue (10+)
// ====================================================
#define EVENT_QUEUE_KEY_KEYBOARD		10
#define EVENT_PORT_KEY_KEYBOARD			10 // spu port keys must be < 64

// message IDs for keyboard events/signals
#define MESSAGE_PROCESS_KEY_PRESSES		10
#define MESSAGE_KEY_EVENTS_READY		11


// ====================================================
// The Printf Event Queue (20+)
// ====================================================
#define EVENT_QUEUE_KEY_PRINTF			20
#define EVENT_PORT_KEY_PRINTF			EVENT_PRINTF_PORT // needs to be what the libraries use

// message IDs for printf events
#define MESSAGE_PRINTF_READY			20


// ====================================================
// The DMA Event Queue (30+)
// ====================================================
#define EVENT_QUEUE_KEY_DMA				30
#define EVENT_PORT_KEY_DMA				30 // spu port keys must be < 64

// message IDs for DMA events/signals
#define MESSAGE_DMA_POINTER_BUFFER		30
#define MESSAGE_DMA_COMPLETE			31


// ====================================================
// Add more queue types/ports/messages here. 
// NOTE: Never use any values higher than MAX_USER_EVENT_VALUE!
// ====================================================
#define MAX_USER_EVENT_VALUE			1000000


// ====================================================
// Shared structures
// ====================================================

#if SPU
struct PPUAddress
{
	unsigned int Low;
};
#else
typedef void* PPUAddress;
#endif

struct FDMAInfo
{
	PPUAddress RAMAddress;
	PPUAddress GPUAddress;
	unsigned int Size;
	unsigned int bIsDMAComplete;
	PPUAddress Semaphore;
}__attribute__((aligned(128)));

#define DMA_COMMAND_DEPTH	32


#endif // PU_SPU_H

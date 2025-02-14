/*=============================================================================
	SPU.h: Common definitions that are used by all SPU projects.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef SPU_H
#define SPU_H

#include <cell/dma.h>
#include <spu_printf.h>
#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_context.h>
#include <cell/spurs/common.h>
#include <cell/gcm_spu.h>

#include "UnrealTypes.h"
#include "PU_SPU.h"


#define ALIGN( X, Y )	(((X) + (Y) - 1) & ~((Y) - 1))

typedef CellSpursJob256 FSpursJobType;

struct FGlobals
{
	BYTE*	ScratchMemory;
	BYTE*	OutputMemory;
};

inline void InitGlobals( FGlobals& Globals, void* ScratchBase, void* OutputBase )
{
	Globals.ScratchMemory = (BYTE*) ALIGN(DWORD(ScratchBase), 128);
	Globals.OutputMemory = (BYTE*) ALIGN(DWORD(OutputBase), 128);
}

inline BYTE* AllocScratchMemory( FGlobals& Globals, DWORD Size )
{
	BYTE* StartAddress = Globals.ScratchMemory;
	Globals.ScratchMemory += ALIGN(Size, 128);
	return StartAddress;
}

inline BYTE* AllocOutputMemory( FGlobals& Globals, DWORD Size )
{
	BYTE* StartAddress = Globals.OutputMemory;
	Globals.OutputMemory += ALIGN(Size, 128);
	return StartAddress;
}

inline void SetupGcmContext( const void *DMABuffer, CellGcmContextData *Context, DWORD CommandBufferPtr, DWORD Size )
{
	DWORD Offset = CommandBufferPtr & 0xf;
	BYTE* Memory = (BYTE*)DMABuffer;

	// Make sure our LS command buffer start address has the same low 4 bits as the one in host memory.
	uint32_t* LSCommandBuffer	= (uint32_t*) (Memory + Offset);
	Context->begin			= LSCommandBuffer;
	Context->current		= LSCommandBuffer;
	Context->end			= (uint32_t*) (((BYTE*)LSCommandBuffer) + Size);
}

template<typename UserDataType>
inline const UserDataType& GetUserData(FSpursJobType* Job)
{
	// There's one user data QWORD reserved at the end for the fence info.
	static const UINT NumReservedUserDataQWORDs = 1;

	const UINT NumUserDataQWORDs = (sizeof(UserDataType) + sizeof(QWORD) - 1) / sizeof(QWORD);

	return *(UserDataType*)((void*)&Job->workArea.userData[ARRAY_COUNT(Job->workArea.userData) - NumUserDataQWORDs - NumReservedUserDataQWORDs]);
}

inline void WriteJobFence(CellSpursJobContext2* JobContext,FSpursJobType* Job, const void *FenceDMABuffer)
{
	// Do we need to update the fence?
	const QWORD& FenceUserData = Job->workArea.userData[ARRAY_COUNT(Job->workArea.userData) - 1];
	DWORD Fence = DWORD(FenceUserData & 0xffffffffull);
	if ( Fence )
	{
		// First copy the fence value into 128-byte aligned LS memory, then DMA it.
		DWORD FenceEa	= DWORD(FenceUserData >> 32);
		DWORD* FenceLS	= (DWORD*) FenceDMABuffer; // must be 128 bytes and 16-byte aligned
		*FenceLS		= Fence;
		//spu_printf( "SPU %d: Writing fence %d (LS=0x%05X, EA=0x%08X)!\n", cellSpursGetCurrentSpuId(), Fence, DWORD(FenceLS), FenceEa );
		cellDmaPutf( FenceLS, FenceEa, 128, JobContext->dmaTag, 0, 0 );
	}
}


#endif

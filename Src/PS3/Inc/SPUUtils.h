/*=============================================================================
	SPUUtils.h: Utility functions for SPU management from the PU side
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Josh Adams
=============================================================================*/

#include <sys/synchronization.h>

#ifndef _SPU_UTILS_H
#define _SPU_UTILS_H

/**
 * This union is used so a main memory address can easily be split into two 32 bit numbers
 */
struct PUAddress
{
	PUAddress()
	{}
	PUAddress(void* InPtr)
	{ PtrAddress = InPtr; }

	union
	{
#if !SPU
		sys_addr_t	IntAddress;
#endif
		void*		PtrAddress;
		struct
		{
			DWORD	High;
			DWORD	Low;
		};
	};
#if !SPU
	operator sys_addr_t()
	{
		return		IntAddress;
	}
#endif
};

// An SPU address is 32 bits
typedef DWORD SPUAddress;
typedef WORD DMATag;

struct FDMAManager
{
	
};

#endif

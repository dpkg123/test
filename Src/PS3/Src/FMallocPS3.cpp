/*=============================================================================
	FMallocPS3.cpp: PS3 memory allocator implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "FMallocPS3.h"
#include "FFileManagerPS3.h"
#include "EngineAudioDeviceClasses.h"
#include <cell/fs/cell_fs_file_api.h>

/** Size to leave for PRX modules */ 
// @todo: Figure out exactly how much we need
#if USE_FMALLOC_DL
// no need to reserve memory for PRX since dlmalloc only uses system allocs
INT GPS3PRXMemoryMegs = 0; 
#else
#if FINAL_RELEASE
INT GPS3PRXMemoryMegs = 5; 
#else
INT GPS3PRXMemoryMegs = 5;
#endif 
#endif 


/** Start address of main memory that is accessible by the RSX */
BYTE* GPS3GPUHostMemoryStart = NULL;

/** Size of all of main memory that is accessible by the RSX */
INT GPS3GPUHostMemorySize = 0;

/** RSX Command buffer */
void* GPS3CommandBuffer = NULL;

// the size of the system command buffer (must be smaller than RSXHOST_SIZE
INT GPS3CommandBufferSizeMegs = 3;

/* Remember the amount of free memory before the first malloc (this roughly accounts for executable, stack, static, and global object size) */
INT GPS3MemoryAvailableAtBoot = 0;

/* Remember the amount of memory available to malloc */
INT GPS3MemoryAvailableToMalloc = 0;

/** If this is TRUE, then failed allocations will return NULL, not assert */
UBOOL GPS3MallocNoCrashOnOOM = FALSE;

/*----------------------------------------------------------------------------
	Memory helpers
----------------------------------------------------------------------------*/

void* appPS3SystemMalloc(INT Size)
{
	sys_addr_t Addr = 0;
	INT Err = sys_memory_allocate(Align<INT>(Size, 64 * 1024), SYS_MEMORY_PAGE_SIZE_64K, &Addr);
	return (void*)Addr;
}

void appPS3SystemFree(void* Ptr)
{
	if (Ptr)
	{
		sys_memory_free((sys_addr_t)Ptr);
	}
}

extern "C" void* __wrap_malloc(size_t Size)
{
	return appMalloc(Size);
}

extern "C" void __wrap_free(void* Ptr)
{
	appFree(Ptr);
}

extern "C" void* __wrap_realloc(void* Ptr, size_t Size)
{
	return appRealloc(Ptr, Size);
}

extern "C" void* __wrap_reallocalign(void* Ptr, size_t Size, size_t Alignment)
{
	return appRealloc(Ptr, Size, Alignment);
}

extern "C" void* __wrap_memalign(size_t Alignment, size_t Size)
{
	return appMalloc(Size, Alignment);
}

extern "C" void* __wrap_calloc(size_t NumElems, size_t ElemSize)
{
	void* Ptr = appMalloc(NumElems * ElemSize);
	appMemzero(Ptr, NumElems * ElemSize);
	return Ptr;
}

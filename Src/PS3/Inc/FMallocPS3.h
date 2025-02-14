/*=============================================================================
	FMallocPS3.h: PS3 support for FPooledMalloc
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _F_MALLOC_PS3_H_
#define _F_MALLOC_PS3_H_

#include <sys/memory.h>

// toggle the allocator desired
#define USE_FMALLOC_DL		1	// Doug Lea allocator
#define USE_FMALLOC_POOLED	0	// Pooled allocator

// if this is defined to 1, then the GPU will not have access to all of GPU memory
// and will make a preallocated buffer RSXHOST_SIZE big
// Only use the old memory model when running any special builds
#if PS3_PASUITE
#define USE_STATIC_HOST_MODEL 1
#define USE_GROW_DYNAMIC_HOST_MODEL 0
#endif

/** pre-alloc static memory directly from OS and use with FMallocGcm for Host memory allocations */
#ifndef USE_STATIC_HOST_MODEL
#define USE_STATIC_HOST_MODEL 0
#endif

/** dynamically alloc memory directly from OS as needed using FMallocGcmHostGrowable for Host memory allocations */
#ifndef USE_GROW_DYNAMIC_HOST_MODEL
#define USE_GROW_DYNAMIC_HOST_MODEL 1
#endif

#if USE_FMALLOC_POOLED
#include "FMallocPS3Pooled.h"
#elif USE_FMALLOC_DL
#include "FMallocPS3DL.h"
#endif

// Globals

/** Start address of main memory that is accessible by the RSX */
extern BYTE* GPS3GPUHostMemoryStart;

/** Size of all of main memory that is accessible by the RSX */
extern INT GPS3GPUHostMemorySize;

/** RSX Command buffer */
extern void* GPS3CommandBuffer;

/** The size of the system command buffer (must be smaller than RSXHOST_SIZE) */
extern INT GPS3CommandBufferSizeMegs;

/* Remember the amount of free memory before the first malloc (this is roughly executable, static, and global object size, I believe) */
extern INT GPS3MemoryAvailableAtBoot;

/* Remember the amount of memory available to malloc */
extern INT GPS3MemoryAvailableToMalloc;

/** Size to leave for PRX modules */ 
extern INT GPS3PRXMemoryMegs;

/** If this is TRUE, then failed allocations will return NULL, not assert */
extern UBOOL GPS3MallocNoCrashOnOOM;

#endif // _F_MALLOC_PS3_H_


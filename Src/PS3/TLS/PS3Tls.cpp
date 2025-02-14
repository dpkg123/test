/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */




#if !PS3_NO_TLS

#include <cell/atomic.h>
#include <sys/ppu_thread.h>
#include "PS3Tls.h"

__thread void *		GThreadLocalStorage[TLS_MINIMUM_AVAILABLE_PS3];
__thread uint64_t	GTLS_ThreadID = 0;
__thread uint32_t	GTLS_StackBase;
__thread uint32_t	GTLS_StackSize;
__thread uint32_t	GTLS_IsInitialized = 0;		// 0 if the GTLS_* variables are uninitialized (0x01234567 otherwise)

/** Set to all zeros before any static initialization. */
volatile uint32_t	GTLSSlots[TLS_MINIMUM_AVAILABLE_PS3];


/**
 * Sets up the GTLS_* variables for the current thread.
 */
void appInitializeTls()
{
	// Setup the current thread id
	sys_ppu_thread_t ThreadID;
	sys_ppu_thread_get_id(&ThreadID);
	GTLS_ThreadID = ThreadID;

	// Setup the current thread stack info
	sys_ppu_thread_stack_t StackInfo;
	sys_ppu_thread_get_stack_information( &StackInfo );
	GTLS_StackBase = (uint32_t) StackInfo.pst_addr;
	GTLS_StackSize = (uint32_t) StackInfo.pst_size;

	// Mark as initialized
	GTLS_IsInitialized = 0x01234567;
}


#endif

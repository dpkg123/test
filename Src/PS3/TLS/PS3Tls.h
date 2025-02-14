/**
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */



#ifndef PS3TLS_HEADER
#define PS3TLS_HEADER


#if !PS3_NO_TLS

#define TLS_MINIMUM_AVAILABLE_PS3	64

extern __thread void *		GThreadLocalStorage[TLS_MINIMUM_AVAILABLE_PS3];
extern __thread uint64_t	GTLS_ThreadID;			// Current thread id
extern __thread uint32_t	GTLS_StackBase;			// Stack base address for the current thread
extern __thread uint32_t	GTLS_StackSize;			// Stack Size for the current thread
extern __thread uint32_t	GTLS_IsInitialized;		// 0 if the GTLS_* variables are uninitialized (0x01234567 otherwise)

/** Set to all zeros before any static initialization. */
extern volatile uint32_t	GTLSSlots[TLS_MINIMUM_AVAILABLE_PS3];

/**
 * Allocates a thread local store slot
 */
inline uint32_t appAllocTlsSlot(void)
{
	// Search for an available slot. Only updates the value if it is currently FALSE
	for (uint32_t SearchIndex = 0; SearchIndex < TLS_MINIMUM_AVAILABLE_PS3; ++SearchIndex)
	{
		if ( cellAtomicCompareAndSwap32((uint32_t*)&GTLSSlots[SearchIndex],0,1) == 0 )
		{
			// We have a slot so return it
			return SearchIndex;
		}
	}
	return 0;
}

/**
 * Frees a previously allocated TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 */
inline void appFreeTlsSlot(unsigned int SlotIndex)
{
	// Atomically make it available
	cellAtomicStore32((uint32_t*)&GTLSSlots[SlotIndex], 0);
}

/**
 * Sets a value in the specified TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 * @param Value the value to store in the slot
 */
inline void appSetTlsValue(uint32_t SlotIndex,void* Value)
{
	GThreadLocalStorage[SlotIndex] = Value;
}

/**
 * Reads the value stored at the specified TLS slot
 *
 * @return the value stored in the slot
 */
inline void* appGetTlsValue(uint32_t SlotIndex)
{
	return GThreadLocalStorage[SlotIndex];
}

/**
 * Sets up the GTLS_* variables for the current thread.
 */
extern void appInitializeTls();

/**
 * Initializes the GTLS_* variables for the current thread if necessary.
 */
inline void appTryInitializeTls()
{
	if ( GTLS_IsInitialized )
	{
		return;
	}

	appInitializeTls();
}


#else	//!PS3_NO_TLS

inline void appTryInitializeTls()
{
}

#endif	//!PS3_NO_TLS

#endif	//PS3TLS_HEADER

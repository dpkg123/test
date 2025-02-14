/*=============================================================================
	SPUUtils.h: Utility functions for SPU management from the PU side
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sys/ppu_thread.h>
#include <cell/atomic.h>
#include <sys/event.h>

#ifndef _PS3_CORE_THREADING_H
#define _PS3_CORE_THREADING_H

#if !PS3_NO_TLS
	#include "TLS/PS3Tls.h"
#endif	//!PS3_NO_TLS

/**
 * Interlocked style functions for threadsafe atomic operations
 */

/**
 * Atomically increments the value pointed to and returns that to the caller
 */
FORCEINLINE INT appInterlockedIncrement(volatile INT* Value)
{
	// since cellAtomicIncr returns the original value, we must add one to get the new value
	return cellAtomicIncr32((UINT*)Value) + 1;
}
/**
 * Atomically decrements the value pointed to and returns that to the caller
 */
FORCEINLINE INT appInterlockedDecrement(volatile INT* Value)
{
	// since cellAtomicDecr returns the original value, we must subtract one to get the new value
	return cellAtomicDecr32((UINT*)Value) - 1;
}
/**
 * Atomically adds the amount to the value pointed to and returns the old
 * value to the caller
 */
FORCEINLINE INT appInterlockedAdd(volatile INT* Value,INT Amount)
{
	return cellAtomicAdd32((UINT*)Value, Amount);
}
/**
 * Atomically swaps two values returning the original value to the caller
 */
FORCEINLINE INT appInterlockedExchange(volatile INT* Value,INT Exchange)
{
	return cellAtomicStore32((UINT*)Value, Exchange);
}
/**
 * Atomically compares the value to comperand and replaces with the exchange
 * value if they are equal and returns the original value
 */
FORCEINLINE INT appInterlockedCompareExchange(INT* Dest,INT Exchange,INT Comperand)
{
	return cellAtomicCompareAndSwap32((UINT*)Dest, Comperand, Exchange);
}
/**
 * Atomically compares the pointer to comperand and replaces with the exchange
 * pointer if they are equal and returns the original value
 */
FORCEINLINE void* appInterlockedCompareExchangePointer(void** Dest,void* Exchange,void* Comperand)
{
	return (void*)cellAtomicCompareAndSwap32((UINT*)Dest, (UINT)Comperand, (UINT)Exchange);
}

/**
 * Returns a pseudo-handle to the currently executing thread.
 */
FORCEINLINE HANDLE appGetCurrentThread(void)
{
	//@fixme josh
	return (HANDLE)-1;
}

/**
 * Returns the currently executing thread's id
 */
FORCEINLINE DWORD appGetCurrentThreadId(void)
{
#if PS3_NO_TLS
	sys_ppu_thread_t ThreadID;
	sys_ppu_thread_get_id(&ThreadID);
	// @todo: sys_ppu_thread_t is a 64bit number!
	return (DWORD)ThreadID;
#else
	appTryInitializeTls();
	// @todo: sys_ppu_thread_t is a 64bit number!
	return (DWORD)GTLS_ThreadID;	
#endif
}

/**
 * Sets the preferred processor for a thread.
 *
 * @param	ThreadHandle		handle for the thread to set affinity for
 * @param	PreferredProcessor	zero-based index of the processor that this thread prefers
 *
 * @return	the number of the processor previously preferred by the thread, MAXIMUM_PROCESSORS
 *			if the thread didn't have a preferred processor, or (DWORD)-1 if the call failed.
 */
FORCEINLINE DWORD appSetThreadAffinity( HANDLE ThreadHandle, DWORD PreferredProcessor )
{
	//@fixme josh
	return (DWORD)-1;
}

/**
 * This is the PU version of a critical section.
 */
class FCriticalSection :
	public FSynchronize
{
	/** The PU mutex structure */
	sys_lwmutex_t Mutex;

public:
	/**
	 * Constructor that initializes the aggregated critical section
	 */
	FCriticalSection(void)
	{
		sys_lwmutex_attribute_t MutexAttr;
		MutexAttr.attr_protocol = SYS_SYNC_PRIORITY;
		MutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
		// create the PU mutex
		INT Result = sys_lwmutex_create(&Mutex,&MutexAttr);
		checkSlow(Result == CELL_OK);
	}

	/**
	 * Destructor cleaning up the critical section
	 */
	~FCriticalSection(void)
	{
		INT Result;
		// we need to wait until the mutex unlocks to delete it
		do
		{
			// attempt to destroy it. will return EBUSY if the mutex is locked
			Result = sys_lwmutex_destroy(&Mutex);
		}
		while (Result == EBUSY);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
		// lock the resource
		INT Result = sys_lwmutex_lock(&Mutex,0);
		checkSlow(Result == CELL_OK);
	}

	/**
	 * Releases the lock on the critical section
	 */
	FORCEINLINE void Unlock(void)
	{
		// unlock the resource
		INT Result = sys_lwmutex_unlock(&Mutex);
		checkSlow(Result == CELL_OK);
	}
};


#if PS3_NO_TLS

/**
 * This class mimics TLS support similar to our other platforms
 */
class FTlsManager
{
	/**
	 * This class maintains a mapping of thread ids to internal slots
	 */
	class FTlsSlot
	{
		/** Structure holding the mapping of thread id to value */
		struct FTlsThreadMapping
		{
			/** The thread this value is associated with */
			volatile DWORD ThreadId;
			/** The value of the TLS index for this thread */
			void* Value;

			/** Inits members */
			FTlsThreadMapping(void) :
				ThreadId(0),
				Value(NULL)
			{
			}
		};
		/** Maximum number of threads we are storing data for */
		static const DWORD MaxThreads = 16;
		/** The per thread storage */
		FTlsThreadMapping PerThreadStorage[MaxThreads];

		/**
		 * Allocates a slot for a thread
		 *
		 * @param ThreadId the thread id to allocate for
		 *
		 * @return The new slot to use
		 */
		inline DWORD AllocateThreadSlot(DWORD ThreadId)
		{
			DWORD SearchIndex = 0;
			// Search for an available slot
			while (SearchIndex < MaxThreads &&
				// Only updates the value if it is currently InvalidId
				appInterlockedCompareExchange((INT*)&PerThreadStorage[SearchIndex].ThreadId,(INT)ThreadId,0) != 0)
			{
				SearchIndex++;
			}
			check(SearchIndex < MaxThreads && "No more slots. Increase MaxThreads");
			// We have a slot so return it
			return SearchIndex;
		}

		/**
		 * Finds the thread slot for this thread id
		 *
		 * @return the index into the per thread storage
		 */
		inline DWORD FindThreadSlot(DWORD ThreadId)
		{
			UBOOL bFound = FALSE;
			DWORD SearchIndex = 0;
			// Find the matching slot or the first available
			while (SearchIndex < MaxThreads &&
				PerThreadStorage[SearchIndex].ThreadId != 0)
			{
				if (PerThreadStorage[SearchIndex].ThreadId == ThreadId)
				{
					bFound = TRUE;
					break;
				}
				SearchIndex++;
			}
			// Check for not found
			if (bFound == FALSE)
			{
				// Allocate a new slot and use that
				SearchIndex = AllocateThreadSlot(ThreadId);
			}
			return SearchIndex;
		}

	public:
		/** Whether the slot is in use or not */
		volatile UBOOL bInUse;

		/** Simple constructor that marks the slot available */
		FTlsSlot(void) :
			bInUse(FALSE)
		{
		}

		/** 
		 * Searches the list of thread to value mappings for the thread id
		 * and replaces the value in that slot with the one specified
		 *
		 * @param Value the value to store in the thread specific slot
		 */
		inline void SetThreadValue(void* Value)
		{
			// Get the thread id for per thread storage
			DWORD ThreadId = appGetCurrentThreadId();
			// Find the slot (allocates if needed)
			DWORD SlotIndex = FindThreadSlot(ThreadId);
			// Now store the value
			PerThreadStorage[SlotIndex].Value = Value;
		}

		/**
		 * Searches the list of thread to value mappings for the thread id
		 * and returns the value in that slot
		 */
		inline void* GetThreadValue(void)
		{
			// Get the thread id for per thread storage
			DWORD ThreadId = appGetCurrentThreadId();
			// Find the slot (allocates if needed)
			DWORD SlotIndex = FindThreadSlot(ThreadId);
			// Now return the value
			return PerThreadStorage[SlotIndex].Value;
		}

		/**
		 * Frees all the threads associations since the slot is disabled
		 */
		void ClearThreadSlots(void)
		{
			appMemzero(PerThreadStorage,sizeof(PerThreadStorage));
		}
	};

	/** The maximum number of slots supported */
	static const DWORD MaxTlsSlots = 64;
	/**
	 * Holds the set of available slots. Each slot is allocated before it can be
	 * used. The allocation returns an index into this table. All subsequent
	 * access is done via that index
	 */
	FTlsSlot Slots[MaxTlsSlots];

public:
	/**
	 * Allocates a new TLS entry by searching the slot list for an available item
	 *
	 * @return The index of the new entry
	 */
	inline DWORD AllocTlsSlot(void)
	{
		DWORD SearchIndex = 0;
		// Search for an available slot
		while (SearchIndex < MaxTlsSlots &&
			// Only updates the value if it is currently FALSE
			appInterlockedCompareExchange((INT*)&Slots[SearchIndex].bInUse,TRUE,FALSE) == TRUE)
		{
			SearchIndex++;
		}
		check(SearchIndex < MaxTlsSlots && "No more slots. Increase max size");
		// We have a slot so return it
		return SearchIndex;
	}

	/**
	 * Frees a slot for later use
	 *
	 * @param SlotIndex the index to release
	 */
	FORCEINLINE void FreeTlsSlot(DWORD SlotIndex)
	{
		checkSlow(SlotIndex < MaxTlsSlots && "Out of bounds access");
		checkSlow(Slots[SlotIndex].bInUse && "Can't free something not in use");

		// Zero all mappings of threads to values
		Slots[SlotIndex].ClearThreadSlots();

		// Atomically make it available
		appInterlockedExchange((INT*)&Slots[SlotIndex].bInUse,FALSE);
	}

	/**
	 * Sets the value stored in the TLS slot
	 *
	 * @param SlotIndex the TLS slot to set on
	 * @param Value the new value to assign
	 */
	FORCEINLINE void SetTlsValue(DWORD SlotIndex,void* Value)
	{
		checkSlow(SlotIndex < MaxTlsSlots && "Out of bounds access");
		checkSlow(Slots[SlotIndex].bInUse && "Can't access something not in use");
		// Store the value in the per thread slot
		Slots[SlotIndex].SetThreadValue(Value);
	}

	/**
	 * Gets the value stored in the TLS slot
	 *
	 * @param SlotIndex the TLS slot to read
	 *
	 * @return the value assigned to the slot
	 */
	FORCEINLINE void* GetTlsValue(DWORD SlotIndex)
	{
		checkSlow(SlotIndex < MaxTlsSlots && "Out of bounds access");
		checkSlow(Slots[SlotIndex].bInUse && "Can't access something not in use");

		// Reads the value in the per thread slot
		return Slots[SlotIndex].GetThreadValue();
	}

	/**
	 * Singleton instance to get around order of operation problems with static
	 * initializers
	 *
	 * @return The global instance of the TLS manager
	 */
	static FTlsManager* GetInstance(void)
	{
		static FTlsManager* Instance = new FTlsManager();
		return Instance;
	}
};

/**
 * Allocates a thread local store slot
 */
FORCEINLINE DWORD appAllocTlsSlot(void)
{
	return FTlsManager::GetInstance()->AllocTlsSlot();
}

/**
 * Sets a value in the specified TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 * @param Value the value to store in the slot
 */
FORCEINLINE void appSetTlsValue(DWORD SlotIndex,void* Value)
{
	FTlsManager::GetInstance()->SetTlsValue(SlotIndex,Value);
}

/**
 * Reads the value stored at the specified TLS slot
 *
 * @return the value stored in the slot
 */
FORCEINLINE void* appGetTlsValue(DWORD SlotIndex)
{
	return FTlsManager::GetInstance()->GetTlsValue(SlotIndex);
}

/**
 * Frees a previously allocated TLS slot
 *
 * @param SlotIndex the TLS index to store it in
 */
FORCEINLINE void appFreeTlsSlot(DWORD SlotIndex)
{
	FTlsManager::GetInstance()->FreeTlsSlot(SlotIndex);
}


#endif	//PS3_NO_TLS


#endif

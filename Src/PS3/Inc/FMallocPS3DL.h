/*=============================================================================
	FMallocPS3DL.h: PS3 support for Doug Lea allocator
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _F_MALLOC_DL_H_
#define _F_MALLOC_DL_H_

/**
 * Doug Lea allocator for PS3 (http://g.oswego.edu/)
 */
class FMallocPS3DL : public FMalloc
{
public:
	FMallocPS3DL();
	~FMallocPS3DL();

	/** 
	 * Malloc
	 */
	virtual void* Malloc( DWORD Size, DWORD Alignment );

	/** 
	 * Realloc
	 */
	virtual void Free( void* Ptr );

	/** 
	 * Free
	 */
	virtual void*	Realloc( void* Ptr, DWORD NewSize, DWORD Alignment );

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual UBOOL IsInternallyThreadSafe() const
	{
		return FALSE;
	}

	/**
	 * Gathers memory allocations for both virtual and physical allocations.
	 *
	 * @param FMemoryAllocationStats	[out] structure containing information about the size of allocations
	 */
	virtual void GetAllocationInfo( FMemoryAllocationStats& MemStats );

	/**
	 * Dumps details about all allocations to an output device
	 *
	 * @param Ar	[in] Output device
	 */
	virtual void DumpAllocations( class FOutputDevice& Ar );

	/**
	 * Keeps trying to allocate memory until we fail
	 *
	 * @param Ar Device to send output to
	 */
	virtual void CheckMemoryFragmentationLevel( class FOutputDevice& Ar );

	/**
	 * If possible give memory back to the os from unused segments
	 *
	 * @param ReservePad - amount of space to reserve when trimming
	 * @param bShowStats - log stats about how much memory was actually trimmed. Disable this for perf
	 * @return TRUE if succeeded
	 */
	virtual UBOOL TrimMemory(SIZE_T ReservePad,UBOOL bShowStats=FALSE);

	/**
	 * Pre-initialize memory stuff before we call new on this (which calls 
	 * the constructor, so constructor is too late). 
	 */
	static void StaticInit();

	/** This is a test function which can be called right after StaticInit().  (e.g. for testing realloc) Also because various GLogs have not been set up so need to use printf **/
	void DoAfterStaticInit();

	/** Exec command handler for this allocator */
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );

private:
	FORCENOINLINE void	OutOfMemory( DWORD Size );

	/** global ptr for all Realloc(NULL,0) requests */
	static void* ReallocZeroPtr;

	/** types of mspace heaps */
	enum EMemSpaces
	{
		MemSpace_Heap0,		// for small allocs
		MemSpace_MAX		// max entries
	};

	/** mspace heap entry */
	struct FMemSpace
	{
		void*	MemSpacePtr;
		SIZE_T	MinAllocSize;
		SIZE_T	MaxAllocSize;
	};

	/** array of memory space heaps that can be used */
	static FMemSpace MemSpaces[MemSpace_MAX];

#if STATS
	DWORD				OsCurrent;
	DWORD				OsPeak;
	DWORD				CurrentAllocs;
	DWORD				TotalAllocs;
	QWORD				TotalWaste;
	DOUBLE				MemTime;
#endif
};

#endif // _F_MALLOC_DL_H_


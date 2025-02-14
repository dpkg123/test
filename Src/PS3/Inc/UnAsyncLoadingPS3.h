/*=============================================================================
	UnAsyncLoadingPS3.h: Definitions of classes used for content streaming on PS3.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNASYNCLOADINGPS3_H__
#define __UNASYNCLOADINGPS3_H__

#include "PS3Threading.h"
#include "PU_SPU.h"

/**
 * PS3 implementation of an async IO manager.	
 */
struct FAsyncIOSystemPS3 : public FAsyncIOSystemBase
{
	/**
	 * Constructor
	 */
	FAsyncIOSystemPS3();

	/**
	 * Destructor
	 */
	~FAsyncIOSystemPS3();

	/** 
	 * Reads passed in number of bytes from passed in file handle.
	 *
	 * @param	FileHandle	Handle of file to read from.
 	 * @param	Offset		Offset in bytes from start, INDEX_NONE if file pointer shouldn't be changed
	 * @param	Size		Size in bytes to read at current position from passed in file handle
	 * @param	Dest		Pointer to data to read into
	 *
	 * @return	TRUE if read was successful, FALSE otherwise
	 */
	virtual UBOOL PlatformReadDoNotCallDirectly(FAsyncIOHandle FileHandle, INT Offset, INT Size, void* Dest);

	/** 
	 * Creates a file handle for the passed in file name
	 *
	 * @param	Filename	Pathname to file
	 *
	 * @return	INVALID_HANDLE if failure, handle on success
	 */
	virtual FAsyncIOHandle PlatformCreateHandle(const TCHAR* Filename);

	/**
	 * Closes passed in file handle.
	 */
	virtual void PlatformDestroyHandle(FAsyncIOHandle FileHandle);

	/**
	 * Returns whether the passed in handle is valid or not.
	 *
	 * @param	FileHandle	File hande to check validity
	 *
	 * @return	TRUE if file handle is valid, FALSE otherwise
	 */
	virtual UBOOL PlatformIsHandleValid(FAsyncIOHandle FileHandle);

	/**
	 * Let the platform handle being done with the file
	 *
	 * @param Filename File that was being async loaded from, but no longer is
	 */
	virtual void PlatformHandleHintDoneWithFile(const FString& Filename);

protected:
	/** Last cache to read from.								*/
	class cell::fios::filehandle* BufferedReader;
	/** Last offset buffered data was read into from.			*/
	INT		BufferedOffset;
	/** Buffered data at BufferedOffset for BufferedHandle.		*/
	BYTE*	BufferedData;
	/** Current offset into BufferedHandle file.				*/
	INT		CurrentOffset;
};

#endif

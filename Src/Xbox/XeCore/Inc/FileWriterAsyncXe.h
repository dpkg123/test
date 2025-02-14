/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#if FINAL_RELEASE
#define ASYC_WRITE_BUFFER_SIZE (8 * 1024)
#else
// Up the async write buffer size for release as we write out a lot more stuff for profiling.
#define ASYC_WRITE_BUFFER_SIZE (128 * 1024)
#endif

/**
 * This file writer uses a double buffered writing with a thread pool worker
 * to allow for non-blocking file writes. NOTE: the thread synchronization is
 * only valid between the game thread and the worker thread
 */
class FArchiveFileWriterAsyncXe :
	public FArchive
{
	/**
	 * Writes the file data to disk on another thread
	 */
	class FQueuedWorkAsyncFileWrite : public FNonAbandonableTask
	{
		friend FAsyncTask<FQueuedWorkAsyncFileWrite>;
		/** The file handle to operate on */
		HANDLE Handle;
		/** Pointer to the buffer that is being written out */
		BYTE* WriteBuffer;
		/** The amount of data being written (zero if no pending write) */
		DWORD WriteSize;

		/**
		 * Performs the buffered write to disk on a thread pool thread
		 */
		void DoWork(void)
		{
			DWORD NumWritten = 0;
			// Do the write synchronously
			DWORD Result = WriteFile(Handle,WriteBuffer,WriteSize,&NumWritten,NULL);
			if (Result == 0 || NumWritten != WriteSize)
			{
				debugf(NAME_Error,TEXT("Failed to write %d bytes to file async Error:0x%X"), WriteSize, GetLastError());
			}
			WriteSize = 0;
			appMemoryBarrier();
		}

		/** Give the name for external event viewers
		* @return	the name to display in external event viewers
		*/
		static const TCHAR *Name()
		{
			return TEXT("FQueuedWorkAsyncFileWrite");
		}

		/**
		 * Copies the file handle that will be operated on
		 *
		 * @param InHandle the file that is going to be writen to
		 */
		FQueuedWorkAsyncFileWrite(HANDLE InHandle) :
			Handle(InHandle),
			WriteBuffer(NULL),
			WriteSize(0)
		{
		}

	public:

		/**
		 * Sets the data for an async write
		 *
		 * @param InBuffer the buffer to write to disk (must live throughout the write lifetime)
		 * @param InSize the size of the buffer to write out
		 */
		void SetBuffer(BYTE* InBuffer,DWORD InSize)
		{
			// Set up the values that will be used on the other thread
			WriteBuffer = InBuffer;
			WriteSize = InSize;
			appMemoryBarrier();
		}

	};

	/** The file handle to operate on */
	HANDLE Handle;
	/** Handle for stats tracking */
	INT StatsHandle;
	/** Filename for debugging purposes */
	FString FileName;
	/** Device to log errors to */
	FOutputDevice* Error;
	/** These are the two buffers used for writing the file asynchronously */
	BYTE Buffers[2][ASYC_WRITE_BUFFER_SIZE];
	/** The current index of the buffer that we are serializing to */
	INT SerializeBuffer;
	/** The current index of the streaming buffer for async writing from */
	INT StreamBuffer;
	/** Where we are in the serialize buffer */
	INT SerializePos;
	/** The thread pool worker that will write the data on another thread */
	FAsyncTask<FQueuedWorkAsyncFileWrite> AsyncFileWriter;
	/** The overall position in the file */
	INT OverallPos;

public:
	/**
	 * Constructor. Zeros values and copies parameters
	 *
	 * @param InHandle the file handle to use when writing
	 * @param InStatsHandle stats handle for tracking write stats
	 * @param InFilename the name of the file being written
	 * @param InError the output device to write errors to
	 * @param InPos the 
	 */
	FArchiveFileWriterAsyncXe(HANDLE InHandle,INT InStatsHandle,const TCHAR* InFilename,FOutputDevice* InError,INT InPos) :
		Handle(InHandle),
		StatsHandle(InStatsHandle),
		FileName(InFilename),
		Error(InError),
		SerializeBuffer(0),
		StreamBuffer(1),
		SerializePos(0),
		AsyncFileWriter(InHandle),
		OverallPos(0)
	{
		ArIsSaving = ArIsPersistent = 1;

		// Handle starting at a location other than zero
		if (InPos != 0)
		{
			Seek(InPos);
		}
	}

	/**
	 * Destructor. Cleans up any per file resources.
	 */
	virtual ~FArchiveFileWriterAsyncXe(void)
	{
		Close();
	}

	/**
	 * Seeks to a location in the file before the next write. Blocks if there is
	 * an outstanding async io operation in progress
	 *
	 * @param InPos the new location to start writing at
	 */
	virtual void Seek(INT InPos)
	{
		check(Handle);
		Flush();
		check(AsyncFileWriter.IsIdle());
		// Set the file pointer to match the overlapped
		if (SetFilePointer(Handle,InPos,NULL,FILE_BEGIN) != INVALID_SET_FILE_POINTER)
		{
			OverallPos = InPos;
		}
		else
		{
			ArIsError = 1;
			Error->Logf(TEXT("SetFilePointer() failed for Seek() with 0x%X"),GetLastError());
		}
	}

	/**
	 * @return Returns the current file position
	 */
	virtual INT Tell(void)
	{
		return OverallPos + SerializePos;
	}

	/**
	 * @return the file size on disk plus any pending data to be written
	 */
	virtual INT TotalSize(void)
	{
		AsyncFileWriter.EnsureCompletion();
		LARGE_INTEGER FileSize;
		FileSize.QuadPart = -1;
		if (GetFileSizeEx(Handle,&FileSize) == FALSE)
		{
			ArIsError = 1;
			Error->Logf(TEXT("GetFileSizeEx() failed for TotalSize() with 0x%X"),GetLastError());
		}
		return Max<INT>(FileSize.QuadPart, OverallPos + SerializePos);
	}

	/**
	 * Closes the file handle and potentially blocks while flushing pending data to disk
	 *
	 * @return always returns true
	 */
	virtual UBOOL Close(void)
	{
		if (Handle != NULL)
		{
			Flush();
			// Close the file handle
			CloseHandle(Handle);
			Handle = NULL;
		}
		return TRUE;
	}

	/** Blocks until all data is written to disk */
	virtual void Flush(void)
	{
		check(Handle);
		// Block until the write has completed
		AsyncFileWriter.EnsureCompletion();
		SCOPED_FILE_IO_WRITE_STATS(StatsHandle,SerializePos,0);
		// Now we need to swap the buffers. This toggles them between buffer indices 0 & 1
		StreamBuffer ^= 1;
		SerializeBuffer ^= 1;
		// If there is any buffered data, write that and then block
		if (SerializePos)
		{
			OverallPos += SerializePos;
			// Do an async write
			check(AsyncFileWriter.IsIdle());
			AsyncFileWriter.GetTask().SetBuffer(Buffers[StreamBuffer],SerializePos);
			// do the write now on this thread
			AsyncFileWriter.StartSynchronousTask();
			SerializePos = 0;
		}
	}

	/**
	 * Writes the amount of data into the buffer(s). If there isn't sufficient space, then
	 * the code blocks until all data can be copied into a buffer
	 *
	 * @param Source the location to copy the data from
	 * @param NumBytes the number of bytes to read
	 */
	virtual void Serialize(void* Source,INT NumBytes)
	{
		check(Handle);
		// While there is data to write
		while (NumBytes > 0)
		{
			// Figure out how many bytes we can write to the buffer
			INT NumToCopy = Min<INT>(NumBytes,ASYC_WRITE_BUFFER_SIZE - SerializePos);
			// See if we are at the end of the serialize buffer or not
			if (NumToCopy > 0)
			{
				// Copy into our buffer that will be written in bulk later
				appMemcpy(&Buffers[SerializeBuffer][SerializePos],Source,NumToCopy);
				// Update the internal positions
				SerializePos += NumToCopy;
				check(SerializePos <= ASYC_WRITE_BUFFER_SIZE);
				// Decrement the number of bytes we copied
				NumBytes -= NumToCopy;
				// Now offset the source pointer by the amount we copied
				Source = (BYTE*)Source + NumToCopy;
			}
			else
			{
				SCOPED_FILE_IO_WRITE_STATS(StatsHandle,ASYC_WRITE_BUFFER_SIZE,0);
				// Block until idle so subsequent serialize calls don't clobber data in 
				// process of being written to disk. Alternatively serializing more than
				// 2 * ASYC_WRITE_BUFFER_SIZE would have same effect.
				AsyncFileWriter.EnsureCompletion();
				// Now we need to swap the buffers. This toggles them between buffer indices 0 & 1
				StreamBuffer ^= 1;
				SerializeBuffer ^= 1;
				OverallPos += SerializePos;
				// The serialize buffer is full, so we need to start a new write using
				// that buffer but we need to make sure the previous write has completed
				check(AsyncFileWriter.IsIdle());
				AsyncFileWriter.GetTask().SetBuffer(Buffers[StreamBuffer],SerializePos);
				AsyncFileWriter.StartBackgroundTask();

				// We are now at the beginning of the serialize buffer
				SerializePos = 0;
			}
		}
	}
};

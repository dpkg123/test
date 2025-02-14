/*=============================================================================
    FFileManagerXenon.h: Unreal Xenon file manager.
    Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "FFileManagerXenon.h"
#include "FileWriterAsyncXe.h"
#include "FConfigCacheIni.h"
#include "Engine.h" // for UGuidCache, which we should move to core
#include "UnNet.h"

//for xbox debugging for creating e:\ partitions
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
#pragma pack(push,8)
#include <xbdm.h>
#pragma pack(pop)
#pragma comment( lib, "xbdm.lib" )
#endif

#include <sys/types.h>
#include <sys/stat.h>

/**
 * If we started with ..\.., skip over a ..\, since the TOC won't be expecting that 
 * 4 ifs so we are separator independent (/ or \)
 *
 * @param Filename [in/out] Filename to skip over the ..\ if ..\.. was passed in
 */ 
void FixupExtraDots(const TCHAR*& Filename)
{
	if (Filename[0] == '.' && Filename[1] == '.' && Filename[3] == '.' && Filename[4] == '.')
	{ 
		Filename += 3;
	}
}

/*-----------------------------------------------------------------------------
    File Manager.
-----------------------------------------------------------------------------*/

// File manager.
FArchiveFileReaderXenon::FArchiveFileReaderXenon( HANDLE InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize, INT InDebugTrackingNumber )
:   Handle          ( InHandle )
,	StatsHandle		( InStatsHandle )
#if !FINAL_RELEASE
,	Filename		( InFilename )
#endif
,   Error           ( InError )
,   Size            ( InSize )
,   Pos             ( 0 )
,   BufferBase      ( 0 )
,   BufferCount     ( 0 )
,	DebugTrackingNumber( InDebugTrackingNumber )
{
    ArIsLoading = ArIsPersistent = 1;
}

FArchiveFileReaderXenon::~FArchiveFileReaderXenon()
{
	if( Handle )
	{
		Close();
	}
}

UBOOL FArchiveFileReaderXenon::InternalPrecache( INT PrecacheOffset, INT PrecacheSize )
{
	UBOOL bSuccess = TRUE;

	// Only precache at current position and avoid work if precaching same offset twice.
	if( Pos == PrecacheOffset && (!BufferBase || !BufferCount || BufferBase != Pos) )
	{
		BufferBase = Pos;
		BufferCount = Min( Min( PrecacheSize, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		{
			// Read data from device via Win32 ReadFile API.
			SCOPED_FILE_IO_READ_STATS( StatsHandle, BufferCount, Pos );
			INT Count = 0;
			check(Handle != INVALID_HANDLE_VALUE);
			ReadFile( Handle, Buffer, BufferCount, (DWORD*)&Count, NULL );
			bSuccess = Count == BufferCount;
		}
		if( !bSuccess )
		{
			ArIsError = 1;
			TCHAR ErrorBuffer[1024];
			Error->Logf( TEXT("ReadFile failed: BufferCount=%i Error=%s"), BufferCount, appGetSystemErrorMessage(ErrorBuffer,1024) );
			appHandleIOFailure( NULL );
		}
	}
	return bSuccess;
}

void FArchiveFileReaderXenon::Seek( INT InPos )
{
    check(InPos>=0);
    check(InPos<=Size);
	check(Handle != INVALID_HANDLE_VALUE);
	UBOOL bSuccess = SetFilePointer( Handle, InPos, 0, FILE_BEGIN ) != INVALID_SET_FILE_POINTER;
    if ( !bSuccess )
    {
        ArIsError = 1;
		TCHAR ErrorBuffer[1024];
        Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, appGetSystemErrorMessage(ErrorBuffer,1024) );
		appHandleIOFailure( NULL );
    }
    Pos         = InPos;
    BufferBase  = Pos;
    BufferCount = 0;
}

INT FArchiveFileReaderXenon::Tell()
{
    return Pos;
}

INT FArchiveFileReaderXenon::TotalSize()
{
    return Size;
}

UBOOL FArchiveFileReaderXenon::Close()
{
    if( Handle )
	{
        CloseHandle( Handle );
	}
    Handle = NULL;
    return !ArIsError;
}

void FArchiveFileReaderXenon::Serialize( void* V, INT Length )
{
    while( Length>0 )
    {
        INT Copy = Min( Length, BufferBase+BufferCount-Pos );
        if( Copy==0 )
        {
            if( Length >= ARRAY_COUNT(Buffer) )
            {
				UBOOL bSuccess = FALSE;
				if( Handle )
				{
					check(Handle != INVALID_HANDLE_VALUE);
					// Read data from device via Win32 ReadFile API.
					SCOPED_FILE_IO_READ_STATS( StatsHandle, Length, Pos );
	                INT Count = 0;
	                ReadFile( Handle, V, Length, (DWORD*)&Count, NULL );
					bSuccess = Count == Length;
				}
                if ( !bSuccess )
                {
                    ArIsError = 1;
					TCHAR ErrorBuffer[1024];
                    Error->Logf( TEXT("ReadFile failed: Length=%i Error=%s"), Length, appGetSystemErrorMessage(ErrorBuffer,1024) );
					appHandleIOFailure( *Filename );
                }

                Pos += Length;
                BufferBase += Length;
                return;
            }
			InternalPrecache( Pos, MAXINT );
            Copy = Min( Length, BufferBase+BufferCount-Pos );
            if( Copy<=0 )
            {
                ArIsError = 1;
                Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i for file %s"), 
					Pos, Length, Size, *Filename );
            }
            if( ArIsError )
                return;
        }
        appMemcpy( V, Buffer+Pos-BufferBase, Copy );
        Pos       += Copy;
        Length    -= Copy;
        V          = (BYTE*)V + Copy;
    }
}

FArchiveFileWriterXenon::FArchiveFileWriterXenon( HANDLE InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InPos )
:   Handle      ( InHandle )
,	StatsHandle ( InStatsHandle )
#if !FINAL_RELEASE
,	Filename	( InFilename )
#endif
,   Error       ( InError )
,   Pos         ( InPos )
,   BufferCount ( 0 )
{
    ArIsSaving = ArIsPersistent = 1;
}

FArchiveFileWriterXenon::~FArchiveFileWriterXenon()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
    if( Handle )
	{
        Close();
	}
    Handle = NULL;
}

void FArchiveFileWriterXenon::Seek( INT InPos )
{
    Flush();
    if( SetFilePointer( Handle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
    {
        ArIsError = 1;
        Error->Logf( *LocalizeError("SeekFailed",TEXT("Core")) );
    }
    Pos = InPos;
}

INT FArchiveFileWriterXenon::Tell()
{
    return Pos;
}

UBOOL FArchiveFileWriterXenon::Close()
{
    Flush();
    if( Handle && !CloseHandle(Handle) )
    {
        ArIsError = 1;
        Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
    }
	Handle = NULL;
    return !ArIsError;
}

void FArchiveFileWriterXenon::Serialize( void* V, INT Length )
{
    Pos += Length;
    INT Copy;
    while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
    {
        appMemcpy( Buffer+BufferCount, V, Copy );
        BufferCount += Copy;
        Length      -= Copy;
        V            = (BYTE*)V + Copy;
        Flush();
    }
    if( Length )
    {
        appMemcpy( Buffer+BufferCount, V, Length );
        BufferCount += Length;
    }
}

void FArchiveFileWriterXenon::Flush()
{
    if( BufferCount )
    {
		SCOPED_FILE_IO_WRITE_STATS( StatsHandle, BufferCount, 0 );
        INT Result=0;
        if( !WriteFile( Handle, Buffer, BufferCount, (DWORD*)&Result, NULL ) )
        {
            ArIsError = 1;
            Error->Logf( *LocalizeError("WriteFailed",TEXT("Core")) );
        }
    }
    BufferCount = 0;
	// XFlushUtilityDrive();
}

/** 
 * Initialize the file manager/file system. 
 * @param Startup TRUE  if this the first call to Init at engine startup
 */
void FFileManagerXenon::Init( UBOOL Startup )
{
	// Read in the base TOC file with all the language agnostic files
	const TCHAR* TOCName;
	if (GUseCoderMode == FALSE)
	{
		TOCName = TEXT("Xbox360TOC.txt");
	}
	else
	{
		TOCName = TEXT("Xbox360TOC_CC.txt");
	}

	// Find any loc toc files
	TArray<FString> FoundFiles;
	FindFiles( FoundFiles, *(appGameDir() * TEXT("*.txt")), TRUE, FALSE );

	// Read in the main toc - this needs to after finding the loc tocs
	ReadTOC( TOC, TOCName, TRUE );

	// Read in the language specific ToCs
	for( INT FileIndex = 0; FileIndex < FoundFiles.Num(); FileIndex ++ )
	{
		const FString& File = FoundFiles( FileIndex );
	
		const TCHAR* Name = *File;
		// Check if the file is a table of contents file.  If so, read it.
		if( ( GUseCoderMode == FALSE && File.InStr( TEXT("Xbox360TOC_") ) != INDEX_NONE ) || 
			( GUseCoderMode == TRUE && File.InStr( TEXT("Xbox360TOC_CC_") ) != INDEX_NONE  ) )
		{
			ReadTOC( TOC, *File, FALSE );
		}
	}

	// look for patch files
	TArray<FString> Results;
	appFindFilesInDirectory(Results, TEXT("UPDATE:\\"), TRUE, TRUE);

	debugf(NAME_DevPatch, TEXT("Looking for UPDATE:\\ files, found %d files"), Results.Num());

	// remap game files to patch file
	for (INT FileIndex = 0; FileIndex < Results.Num(); FileIndex++)
	{
		// get the filename in the best xbox format
		FFilename PatchFilename = Results(FileIndex);
		// skip the helper files, they will be used manually below
		if (PatchFilename.GetExtension() == TEXT("bin_us"))
		{
			debugf(NAME_DevPatch, TEXT("Skipping %s"), *PatchFilename);
			continue;
		}

		// calculate what it would have been on the HDD normally
		FString OriginalFilename = PatchFilename.Replace(TEXT("UPDATE:\\"), TEXT("D:\\"));

		debugf(NAME_DevPatch, TEXT("Redirecting %s to %s..."), *OriginalFilename, *PatchFilename);
		PatchFiles.Set(*OriginalFilename, *PatchFilename);

		// get the filesize
		INT FileSize = InternalFileSize(*PatchFilename);

		// convert it to the relative paths the TOC wants
		FString Relative = OriginalFilename.Replace(TEXT("D:\\"), TEXT("..\\"));

		// look to see if it's compressed
		INT UncompressedSize = 0;

		FString SizeString;
		INT MetaFileSize = InternalFileSize(*(PatchFilename + TEXT("_us")));
		if (MetaFileSize > 0)
		{
			HANDLE MetaHandle = CreateFileA(
				TCHAR_TO_ANSI(*(PatchFilename + TEXT("_us"))), 
				GENERIC_READ, 
				FILE_SHARE_READ, 
				NULL, 
				OPEN_EXISTING, 
				FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, 
				NULL );

			// Dirty disk handling.
			if( MetaHandle == INVALID_HANDLE_VALUE )
			{
				appHandleIOFailure( NULL );
			}

			ANSICHAR* Buffer = ( ANSICHAR* )appMalloc( MetaFileSize + 1 );
			// terminate the buffer for below
			Buffer[MetaFileSize] = 0;
			
			DWORD BytesRead;
			if( ReadFile( MetaHandle, Buffer, MetaFileSize, &BytesRead, NULL ) == 0 )
			{
				appHandleIOFailure( NULL );
			}

			// make sure it succeeded
			check( BytesRead == MetaFileSize );

			// close the file
			CloseHandle( MetaHandle );

			UncompressedSize = atoi(Buffer);

			debugf(NAME_DevPatch, TEXT("  ... which is compressed from %d to %d bytes"), UncompressedSize, FileSize);

			appFree(Buffer);
		}

		// update the TOC
		TOC.AddEntry(*Relative, FileSize, UncompressedSize);
	}

	FFileManagerGeneric::Init(Startup);
}

FArchive* FFileManagerXenon::CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
{
	INT StatsHandle = FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_READ_OPEN_STATS( StatsHandle );

	HANDLE Handle = NULL;
	INT ExistingFileSize = 0;

	Handle = CreateFileA( 
					TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), 
					GENERIC_READ, 
					FILE_SHARE_READ, 
					NULL, 
					OPEN_EXISTING, 
					FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, 
					NULL );
	if( Handle==INVALID_HANDLE_VALUE )
	{
	    DWORD Error = GetLastError();
	    // Missing files do
	    if( Error != ERROR_PATH_NOT_FOUND && Error != ERROR_FILE_NOT_FOUND )
	    {
		    appHandleIOFailure( Filename );
	    }
		return NULL;
	}
	ExistingFileSize = GetFileSize( Handle, NULL );

	// Did the file not exist?
	if ( ExistingFileSize < 0 )
	{
		// if we were told NoFail, then abort
		if (Flags & FILEREAD_NoFail)
		{
			appErrorf(TEXT("Failed to open %s, which was marked as NoFail"), Filename);
		}
		return NULL;
	}

	FArchive* Archive = new FArchiveFileReaderXenon(Handle, StatsHandle, Filename, Error, ExistingFileSize, 0);

	return Archive;
}

FArchive* FFileManagerXenon::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	INT StatsHandle = FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_WRITE_OPEN_STATS( StatsHandle );

	// ensure the directory exists
	MakeDirectory(*FFilename(Filename).GetPath(), TRUE);

	if( (GFileManager->FileSize (Filename) >= 0) && (Flags & FILEWRITE_EvenIfReadOnly) )
	{
        SetFileAttributesA(TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), 0);
	}
    HANDLE Handle    = CreateFileA( 
							TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), 
							GENERIC_WRITE, 
							(Flags & FILEWRITE_AllowRead) ? FILE_SHARE_READ : 0,
							NULL, 
							(Flags & FILEWRITE_Append) ? OPEN_ALWAYS : (Flags & FILEWRITE_NoReplaceExisting) ? CREATE_NEW : CREATE_ALWAYS, 
							FILE_ATTRIBUTE_NORMAL, 
							NULL );
    INT Pos = 0;
    if( Handle==INVALID_HANDLE_VALUE )
    {
		FString ErrorStr = FString::Printf(TEXT("Failed to create file: %s Error: %d"), Filename, GetLastError());
		if( Flags & FILEWRITE_NoFail )
		{
			appErrorf(*ErrorStr);
		}
        return NULL;
    }
    if( Flags & FILEWRITE_Append )
	{
        Pos = SetFilePointer( Handle, 0, 0, FILE_END );
	}
	FArchive* RetArch = NULL;
	if (Flags & FILEWRITE_Async)
	{
		// Create a file writer that will use the thread pool to write to disk
		RetArch = new FArchiveFileWriterAsyncXe(Handle,StatsHandle,Filename,Error,Pos);
	}
	else
	{
		RetArch = new FArchiveFileWriterXenon(Handle,StatsHandle,Filename,Error,Pos);
	}
	return RetArch;
}

/**
 *	Returns the size of a file. (Thread-safe)
 *
 *	@param Filename		Platform-independent Unreal filename.
 *	@return				File size in bytes or -1 if the file didn't exist.
 **/
INT FFileManagerXenon::FileSize( const TCHAR* Filename )
{
	// skip extra ..\ if needed
	FixupExtraDots(Filename);

	// GetFileSize() automatically takes care of TOC synchronization for us
	INT FileSize = TOC.GetFileSize(Filename);
	if (FileSize == -1)
	{
		FileSize = InternalFileSize(Filename);
	}

	return FileSize;
}

/**
 *	Looks up the size of a file by opening a handle to the file.
 *
 *	@param	Filename	The path to the file.
 *	@return	The size of the file or -1 if it doesn't exist.
 */
INT FFileManagerXenon::InternalFileSize(const TCHAR* Filename)
{
	HANDLE Handle = CreateFileA(TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), 
							GENERIC_READ, 
							FILE_SHARE_READ,
							NULL, 
							OPEN_EXISTING, 
							FILE_ATTRIBUTE_NORMAL, 
							NULL );
								
	if(Handle == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	DWORD Result = GetFileSize(Handle, NULL);
	CloseHandle(Handle);

	return Result;
}

/**
 * If the given file is compressed, this will return the size of the uncompressed file,
 * if the platform supports it.
 * @param Filename Name of the file to get information about
 * @return Uncompressed size if known, otherwise -1
 */
INT FFileManagerXenon::UncompressedFileSize( const TCHAR* Filename )
{
	// skip extra ..\ if needed
	FixupExtraDots(Filename);

	return TOC.GetUncompressedFileSize(Filename);
}

DWORD FFileManagerXenon::Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress )
{
    if( EvenIfReadOnly )
	{
        SetFileAttributesA(TCHAR_TO_ANSI(*GetPlatformFilepath(DestFile)), 0);
	}
    DWORD Result;
    if( Progress )
	{
		Result = FFileManagerGeneric::Copy( DestFile, SrcFile, ReplaceExisting, EvenIfReadOnly, Attributes, Progress );
	}
	else
	{
		if( CopyFileA(TCHAR_TO_ANSI(*GetPlatformFilepath(SrcFile)), TCHAR_TO_ANSI(*GetPlatformFilepath(DestFile)), !ReplaceExisting) != 0)
		{
			Result = COPY_OK;
		}
		else
		{
			Result = COPY_MiscFail;
		}
	}
    if( Result==COPY_OK && !Attributes )
	{
        SetFileAttributesA(TCHAR_TO_ANSI(*GetPlatformFilepath(DestFile)), 0);
	}
    return Result;
}

UBOOL FFileManagerXenon::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
    if( EvenReadOnly )
	{
        SetFileAttributesA(TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)),FILE_ATTRIBUTE_NORMAL);
	}
    INT Result = DeleteFileA(TCHAR_TO_ANSI(*GetPlatformFilepath(Filename))) != 0;
    DWORD Error = GetLastError();
	Result = Result || (!RequireExists && (Error==ERROR_FILE_NOT_FOUND || Error==ERROR_PATH_NOT_FOUND));
    if( !Result )
    {
		debugf( NAME_Warning, TEXT("Error deleting file '%s' (%d)"), Filename, Error );
    }
    return Result!=0;
}

UBOOL FFileManagerXenon::IsReadOnly( const TCHAR* Filename )
{
    DWORD rc;
    if( FileSize( Filename ) < 0 )
	{
        return( 0 );
	}
    rc = GetFileAttributesA(TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)));
    if (rc != 0xFFFFFFFF)
	{
        return ((rc & FILE_ATTRIBUTE_READONLY) != 0);
	}
    else
    {
        debugf( NAME_Warning, TEXT("Error reading attributes for '%s'"), Filename );
        return (0);
    }
}

UBOOL FFileManagerXenon::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
    //warning: MoveFileEx is broken on Windows 95 (Microsoft bug).
    Delete( Dest, 0, EvenIfReadOnly );
    INT Result = MoveFileA(TCHAR_TO_ANSI(*GetPlatformFilepath(Src)),TCHAR_TO_ANSI(*GetPlatformFilepath(Dest)));
    if( !Result )
    {
        DWORD error = GetLastError();
        debugf( NAME_Warning, TEXT("Error moving file '%s' to '%s' (%d)"), Src, Dest, error );
    }
    return Result!=0;
}

SQWORD FFileManagerXenon::GetGlobalTime( const TCHAR* Filename )
{
    //return grenwich mean time as expressed in nanoseconds since the creation of the universe.
    //time is expressed in meters, so divide by the speed of light to obtain seconds.
    //assumes the speed of light in a vacuum is constant.
    //the file specified by Filename is assumed to be in your reference frame, otherwise you
    //must transform the result by the path integral of the minkowski metric tensor in order to
    //obtain the correct result.

	WIN32_FILE_ATTRIBUTE_DATA	FileAttributes;

	if(GetFileAttributesEx(TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)),GetFileExInfoStandard,&FileAttributes))
		return *(SQWORD*)&FileAttributes.ftLastWriteTime;
	else
	    return 0;
}

UBOOL FFileManagerXenon::SetGlobalTime( const TCHAR* Filename )
{
    return 0;
}

UBOOL FFileManagerXenon::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
    if( Tree )
		return FFileManagerGeneric::MakeDirectory( Path, Tree );
	return CreateDirectoryA(TCHAR_TO_ANSI(*GetPlatformFilepath(Path)),NULL) !=0 || GetLastError()==ERROR_ALREADY_EXISTS;
}

UBOOL FFileManagerXenon::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
    if( Tree )
        return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );
    return RemoveDirectoryA(TCHAR_TO_ANSI(*GetPlatformFilepath(Path))) !=0 || (!RequireExists && GetLastError()==ERROR_FILE_NOT_FOUND);
}

void FFileManagerXenon::FindFiles( TArray<FString>& Result, const TCHAR* Wildcard, UBOOL Files, UBOOL Directories )
{
	// cache a ffilename version
	FFilename			FullWildcard(Wildcard);

	// HDD Cache and downloadable content uses normal FindFiles method (touching HDD isn't so bad)
	if (!TOC.HasBeenInitialized() ||
		appStrnicmp(Wildcard, TEXT("cache:"), 6) == 0 ||
		appStrnicmp(Wildcard, TEXT("UPDATE:"), 7) == 0 ||
		appStrnicmp(Wildcard, TEXT("DLC"), 3) == 0)
	{
		// get the 
		FFilename			BasePath	= FullWildcard.GetPath() + PATH_SEPARATOR;
		WIN32_FIND_DATAA	Data;
		HANDLE				Handle		= FindFirstFileA(TCHAR_TO_ANSI(*GetPlatformFilepath(Wildcard)), &Data);

		if (Handle != INVALID_HANDLE_VALUE)
		{
			do
			{
				// skip ., .., and accept files if we want them, and directories if we want them
				if (stricmp(Data.cFileName, ".") != 0 && 
                    stricmp(Data.cFileName, "..") != 0 &&
                    ((Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? Directories : Files))
				{
					// get the filename
					FString Filename = FString(ANSI_TO_TCHAR(Data.cFileName));

					// add this to the results
					Result.AddItem(*Filename);

					// store this file in the TOC
					TOC.AddEntry(*(BasePath + Filename), Data.nFileSizeLow);
				}
			}
			// keep going until done
			while( FindNextFileA(Handle,&Data) );

			// close the find handle
			FindClose( Handle );
		}
	}
	else
	{
		// skip extra ..\ if needd
		FixupExtraDots(Wildcard);

		TOC.FindFiles(Result, Wildcard, Files, Directories);
	}
}

DOUBLE FFileManagerXenon::GetFileAgeSeconds( const TCHAR* Filename )
{
	FILETIME UTCFileTime;
	UTCFileTime.dwHighDateTime = 0;
	UTCFileTime.dwLowDateTime = 0;
	INT NewFileSize = INDEX_NONE;
	HANDLE WinHandle = CreateFileA( TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	UBOOL bSuccess = (WinHandle != INVALID_HANDLE_VALUE);
	if ( bSuccess )
	{
		bSuccess = GetFileTime( WinHandle, NULL, NULL, &UTCFileTime );
		CloseHandle( WinHandle );
	}
	if ( bSuccess )
	{
		// A 64-bit value the number of 100-nanosecond intervals since January 1, 1601.
		QWORD IntegerFileTime = (QWORD(UTCFileTime.dwHighDateTime) << 32) | QWORD(UTCFileTime.dwLowDateTime);

		SYSTEMTIME SystemUTCTime;
		FILETIME SystemFileTime;
		GetSystemTime( &SystemUTCTime );
		SystemTimeToFileTime( &SystemUTCTime, &SystemFileTime );
		QWORD IntegerSystemTime = (QWORD(SystemFileTime.dwHighDateTime) << 32) | QWORD(SystemFileTime.dwLowDateTime);

		QWORD Age = IntegerSystemTime - IntegerFileTime;
		return DOUBLE(Age) / 10000.0;
	}
	return -1.0;
}

DOUBLE FFileManagerXenon::GetFileTimestamp( const TCHAR* Filename )
{
	FILETIME UTCFileTime;
	UTCFileTime.dwHighDateTime = 0;
	UTCFileTime.dwLowDateTime = 0;
	INT NewFileSize = INDEX_NONE;
	HANDLE WinHandle = CreateFileA( TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	UBOOL bSuccess = (WinHandle != INVALID_HANDLE_VALUE);
	if ( bSuccess )
	{
		bSuccess = GetFileTime( WinHandle, NULL, NULL, &UTCFileTime );
		CloseHandle( WinHandle );
	}
	if ( bSuccess )
	{
		// A 64-bit value the number of 100-nanosecond intervals since January 1, 1601.
		QWORD IntegerTime = (QWORD(UTCFileTime.dwHighDateTime) << 32) | QWORD(UTCFileTime.dwLowDateTime);
		return DOUBLE(IntegerTime);
	}
	return -1.0;
}

UBOOL FFileManagerXenon::SetDefaultDirectory()
{
	return true;
}

UBOOL FFileManagerXenon::SetCurDirectory(const TCHAR* Directory)
{
	return true;
}

FString FFileManagerXenon::GetCurrentDirectory()
{
	return TEXT("D:\\");
}

/** 
 * Get the timestamp for a file
 *
 * @param Path		Path for file
 * @param Timestamp Output timestamp
 * @return success code
 */  
UBOOL FFileManagerXenon::GetTimestamp( const TCHAR* Filename, FTimeStamp& Timestamp )
{
	SYSTEMTIME UTCTime;
	appMemzero( &UTCTime, sizeof(UTCTime) );

	INT NewFileSize = INDEX_NONE;
	HANDLE WinHandle = CreateFileA( TCHAR_TO_ANSI(*GetPlatformFilepath(Filename)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	UBOOL bSuccess = (WinHandle != INVALID_HANDLE_VALUE);
	if ( bSuccess )
	{
		FILETIME UTCFileTime;
		bSuccess = GetFileTime( WinHandle, NULL, NULL, &UTCFileTime );
		bSuccess = bSuccess && FileTimeToSystemTime( &UTCFileTime, &UTCTime );
		CloseHandle( WinHandle );
	}
	Timestamp.Day       = UTCTime.wDay;
	Timestamp.Month     = UTCTime.wMonth - 1;
	Timestamp.DayOfWeek = UTCTime.wDayOfWeek;
	Timestamp.Hour      = UTCTime.wHour;
	Timestamp.Minute    = UTCTime.wMinute;
	Timestamp.Second    = UTCTime.wSecond;
	Timestamp.Year      = UTCTime.wYear;
	return bSuccess;
}

/**
 * Updates the modification time of the file on disk to right now, just like the unix touch command
 * @param Filename Path to the file to touch
 * @return TRUE if successful
 */
UBOOL FFileManagerXenon::TouchFile(const TCHAR* Filename)
{
	appErrorf(TEXT("Implement me!"));

	return FALSE;
}

/**
 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
 *
 *	@param Filename		Platform-independent Unreal filename
 *	@return				Platform-dependent full filepath
 **/
FString FFileManagerXenon::GetPlatformFilepath( const TCHAR* Filename )
{
	// store the Xbox version of the name
	FString XenonFilename;

	// any files on an "DLCnn:" drive are already in xbox format
	if (appStrnicmp(Filename, TEXT("DLC"), 3) == 0)
	{
		// Skip digits.
		const TCHAR* FilePathMatching = Filename + 3;
		while ( *FilePathMatching && appIsDigit(*FilePathMatching) )
		{
			FilePathMatching++;
		}

		// Check for ':' character.
		if ( *FilePathMatching == TEXT(':') )
		{
		XenonFilename = Filename;
	}
	}

	// XenonFilename not filled in yet?
	if ( XenonFilename.Len() == 0 )
	{
	// any files on an "cache:" drive are already in xbox format
		if (XenonFilename.Len() == 0 && appStrnicmp(Filename, TEXT("cache:"), 6) == 0)
		{
			XenonFilename = Filename;
		}
		// any files on an "update:" drive are already in xbox format
		else if (appStrnicmp(Filename, TEXT("update:"), 7) == 0)
		{
			XenonFilename = Filename;
		}
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
		// any files on an "update:" drive are already in xbox format
		else if (appStrnicmp(Filename, TEXT("DEVKIT:"), 7) == 0)
		{
			XenonFilename = Filename;
		}
#endif
		// any files on an "update:" drive are already in xbox format
		else if (appStrnicmp(Filename, TEXT("update:"), 7) == 0)
		{
			XenonFilename = Filename;
		}
		// any files on an "update:" drive are already in xbox format
		else if (appStrnicmp(Filename, TEXT("UPDATE:"), 7) == 0)
		{
			XenonFilename = Filename;
		}
		else
		{
			// Skip leading "..\".
			if( Filename[0] == L'.' )
			{
				Filename += 3;
			}

			// Skip an additional "..\".
			if( Filename[0] == L'.' )
			{
				Filename += 3;
			}

			if( Filename[0] == L'.' )
			{
				appErrorf(TEXT("No support for relative path names apart from leading \"..\\..\\\" [%s]."), Filename);
			}

			if( Filename[0] == L'\\')
			{
				Filename += 1;
			}

			// tack on the filename
			XenonFilename = TEXT("D:\\");
			XenonFilename += Filename;
		}
	}

	// Convert PATH_SEPARATOR
	XenonFilename = XenonFilename.Replace(TEXT("/"), TEXT("\\"));

	// look for a patched version of this file (this is needed for files that bypass the TOC)
	FString* PatchFile = PatchFiles.Find(*XenonFilename);
	if (PatchFile)
	{
		debugf(NAME_DevPatch, TEXT("Patching %s with %s (bypassed TOC)"), *XenonFilename, **PatchFile);
		XenonFilename = *PatchFile;
	}

	return XenonFilename;
}

/**Enables devkit drive addressing*/
void FFileManagerXenon::EnableLogging (void)
{
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	HRESULT Hr = DmMapDevkitDrive();
#endif
}


UBOOL FFileManagerXenon::IsDrive( const TCHAR* Path )
{
	if ( appStricmp(Path,TEXT("cache:")) == 0 )
		return TRUE;
	if ( appStricmp(Path,TEXT("devkit:")) == 0 )
		return TRUE;
	return FFileManagerGeneric::IsDrive( Path );
}



void appXenonPatchGame()
{
	// temporarily allow the GConfigCache to perform file operations if they were off
	UBOOL bWereFileOpsDisabled = GConfig->AreFileOperationsDisabled();
	GConfig->EnableFileOperations();

	// get a list of all known config files
	TArray<FFilename> ConfigFiles;
	GConfig->GetConfigFilenames(ConfigFiles);

	// look for files in the root (ie of the patch), which is where patched inis and loc files will be
	TArray<FString> NonPackageFiles;
	appFindFilesInDirectory(NonPackageFiles, TEXT("..\\"), FALSE, TRUE);

	for (INT FileIndex = 0; FileIndex < NonPackageFiles.Num(); FileIndex++)
	{
		FFilename ContentFile = FFilename(NonPackageFiles(FileIndex)).GetCleanFilename();
		//debugf(TEXT("----- Installing %s"), *ContentFile );

		// get filename extension
		FString Ext = ContentFile.GetExtension();

		// skip any non-ini/loc (for current language) files
		if (Ext != TEXT("ini") && Ext != UObject::GetLanguage())
		{
			continue;
		}

		// look for the optional special divider string
#define DividerString TEXT("__")
		INT Divider = ContentFile.InStr(DividerString);
		// if we found it, just use what's after the divider
		if (Divider != -1)
		{
			ContentFile = ContentFile.Right(ContentFile.Len() - (Divider + appStrlen(DividerString)));
		}

		FConfigFile* NewConfigFile = NULL;
		FString NewConfigFilename;

		// look for the filename in the config files
		UBOOL bWasCombined = FALSE;
		for (INT ConfigFileIndex = 0; ConfigFileIndex < ConfigFiles.Num() && !bWasCombined; ConfigFileIndex++)
		{
			// does the config file (without path) match the DLC file?
			if (ConfigFiles(ConfigFileIndex).GetCleanFilename() == ContentFile)
			{
				//debugf(TEXT("--------- Accepting %s"), *ConfigFiles(ConfigFileIndex) );
				// get the configfile object
				NewConfigFile = GConfig->FindConfigFile(*ConfigFiles(ConfigFileIndex));
				check(NewConfigFile);

				// merge our ini file into the existing one
				NewConfigFile->Combine(*NonPackageFiles(FileIndex));

				debugf(NAME_DevPatch, TEXT("Merged PATCH config file '%s' into existing config '%s'"), *NonPackageFiles(FileIndex), *ConfigFiles(ConfigFileIndex));

				// mark that we have combined
				bWasCombined = TRUE;

				// remember the filename
				NewConfigFilename = ConfigFiles(ConfigFileIndex);
			}
		}

		// if it wasn't combined, add a new file
		if (!bWasCombined)
		{
			// we need to create a usable pathname for the new ini/loc file
			if (Ext == TEXT("ini"))
			{
				NewConfigFilename = appGameConfigDir() + ContentFile;
			}
			else
			{
				// put this into any localization directory in the proper language sub-directory (..\ExampleGame\Localization\fra\DLCMap.fra)
				NewConfigFilename = GSys->LocalizationPaths(0) * Ext * ContentFile;
			}
			// first we set a value into the config for this filename (since we will be reading from a different
			// path than we want to store the config under, we can't use LoadFile)
			GConfig->SetBool(TEXT("DLCDummy"), TEXT("A"), FALSE, *NewConfigFilename);

			// now get the one we just made
			NewConfigFile = GConfig->FindConfigFile(*NewConfigFilename);

			// read in the file
			NewConfigFile->Combine(*NonPackageFiles(FileIndex));

			debugf(NAME_DevPatch, TEXT("Read new PATCH config file '%s' into the config cache"), *NonPackageFiles(FileIndex));
		}
	}


	// re-disable file ops if they were before
	if (bWereFileOpsDisabled)
	{
		GConfig->DisableFileOperations();
	}

	// find the one that was just loaded (there will be no others yet, DLC won't be installed yet)
	UGuidCache* GlobalGuidCache = NULL;
	for (TObjectIterator<UGuidCache> It; It; ++It)
	{
		GlobalGuidCache = *It;
		break;
	}
	check(GlobalGuidCache);

	FString Filename;
	if (GPackageFileCache->FindPackageFile(TEXT("PatchGuidCache"), NULL, Filename))
	{
		// load the patch guid pacakge
		UPackage* Package = UObject::LoadPackage(NULL, *Filename, LOAD_NoWarn);
		UGuidCache* PatchGuidCache = FindObject<UGuidCache>(Package, TEXT("GuidCache"));

		// look for patch file guids in the cache
		TArray<FString> Results;
		appFindFilesInDirectory(Results, TEXT("UPDATE:\\"), TRUE, FALSE);

		// remap game files to patch file
		for (INT FileIndex = 0; FileIndex < Results.Num(); FileIndex++)
		{
			FName PackageName = FName(*(FFilename(Results(FileIndex)).GetBaseFilename()));

			FGuid Guid;
			// look for this package in it
			if (PatchGuidCache->GetPackageGuid(PackageName, Guid))
			{
				debugf(NAME_DevPatch, TEXT("Setting package guid for %s to %s"), *PackageName.ToString(), *Guid.String());
				// if it's in this one, update the existing one
				GlobalGuidCache->SetPackageGuid(PackageName, Guid);
			}
		}
	}
}


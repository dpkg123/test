/*=============================================================================
    FFileManagerXenon.h: Unreal Xenon file manager.
    Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FFILEMANAGERXENON_H__
#define __FFILEMANAGERXENON_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
    File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderXenon : public FArchive
{
public:
    FArchiveFileReaderXenon( HANDLE InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize, INT InDebugTrackingNumber );
	~FArchiveFileReaderXenon();

	virtual void Seek( INT InPos );
	virtual INT Tell();
	virtual INT TotalSize();
	virtual UBOOL Close();
	virtual void Serialize( void* V, INT Length );

protected:
	UBOOL InternalPrecache( INT PrecacheOffset, INT PrecacheSize );

	HANDLE          Handle;
	/** Handle for stats tracking. */
	INT				StatsHandle;
	/** Filename for debugging purposes. */
	FString			Filename;
    FOutputDevice*  Error;
    INT             Size;
    INT             Pos;
	INT				RequestedPos;
    INT             BufferBase;
    INT             BufferCount;
    BYTE            Buffer[1024];

	/** Unique number for tracking file operations */
	INT				DebugTrackingNumber;
};

class FArchiveFileWriterXenon : public FArchive
{
public:
    FArchiveFileWriterXenon( HANDLE InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InPos );
    ~FArchiveFileWriterXenon();
    void Seek( INT InPos );
    INT Tell();
    UBOOL Close();
    void Serialize( void* V, INT Length );
    void Flush();

protected:
    HANDLE          Handle;
	/** Handle for stats tracking. */
	INT				StatsHandle;
	/** Filename for debugging purposes. */
	FString			Filename;
	FOutputDevice*  Error;
    INT             Pos;
    INT             BufferCount;
    BYTE            Buffer[4096];
};

class FFileManagerXenon : public FFileManagerGeneric
{
public:
	/** 
	 * Initialize the file manager/file system. 
	 * @param Startup TRUE  if this the first call to Init at engine startup
	 */
	virtual void Init(UBOOL Startup);

    virtual FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error );
    virtual FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize );
	/**
	 * If the given file is compressed, this will return the size of the uncompressed file,
	 * if the platform supports it.
	 * @param Filename Name of the file to get information about
	 * @return Uncompressed size if known, otherwise -1
	 */
    virtual INT UncompressedFileSize( const TCHAR* Filename );

	virtual DWORD Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, FCopyProgress* Progress );
    virtual UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 );
    virtual UBOOL IsReadOnly( const TCHAR* Filename );
    virtual UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 );
    virtual SQWORD GetGlobalTime( const TCHAR* Filename );
    virtual UBOOL SetGlobalTime( const TCHAR* Filename );
    virtual UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 );
    virtual UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 );
    virtual void FindFiles( TArray<FString>& Result, const TCHAR* Wildcard, UBOOL Files, UBOOL Directories );
	virtual DOUBLE GetFileAgeSeconds( const TCHAR* Filename );
	virtual DOUBLE GetFileTimestamp( const TCHAR* Filename );
	virtual UBOOL SetDefaultDirectory();
	virtual UBOOL SetCurDirectory(const TCHAR* Directory);
	virtual FString GetCurrentDirectory();

	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	virtual UBOOL TouchFile(const TCHAR* Filename);

	/** 
	 * Get the timestamp for a file
	 *
	 * @param Path		Path for file
	 * @param Timestamp Output timestamp
	 * @return success code
	 */  
	virtual UBOOL GetTimestamp( const TCHAR* /*Filename*/, FTimeStamp& /*Timestamp*/ );

	/**
	 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
	 *
	 *	@param Filename		Platform-independent Unreal filename
	 *	@return				Platform-dependent full filepath
	 **/
	virtual FString GetPlatformFilepath( const TCHAR* Filename );

	/**
	 *	Returns the size of a file. (Thread-safe)
	 *
	 *	@param Filename		Platform-independent Unreal filename.
	 *	@return				File size in bytes or INDEX_NONE if the file didn't exist.
	 **/
	virtual INT FileSize( const TCHAR* Filename );

	/**Enables file system drive addressing where appropriate (360)*/
	virtual void EnableLogging (void);

protected:

	/** Helper object to store the TOC */
	FTableOfContents TOC;

	/** Mapping of a filename the game will ask for, and where it is in the patch directory */
	TMap<FString, FString> PatchFiles;

	/**
	 *	Looks up the size of a file by opening a handle to the file.
	 *
	 *	@param	Filename	The path to the file.
	 *	@return	The size of the file or -1 if it doesn't exist.
	 */
	virtual INT InternalFileSize( const TCHAR* Filename );

	virtual UBOOL IsDrive( const TCHAR* Path );
};


#endif

/*=============================================================================
	FFileManagerPS3.h
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FFILEMANAGERPS3_H__
#define __FFILEMANAGERPS3_H__

#include "FTableOfContents.h"
#include "FFileManagerGeneric.h"
#include <sys/fs_external.h>
#include <cell/fios/compression/edge/edgezlib_decompressor.h>

typedef cell::fios::filehandle* PS3Fd;
typedef cell::fios::stat_t		PS3FsStat;
typedef	cell::fios::off_t		PS3Offset;


// The value of an unset FIOS file handle (this is different than the value of
// an invalid or unset FFileHandle, those still use INDEX_NONE).
#define UNSET_FILE_HANDLE	NULL

// this is the size of array that should be passed in
#define PS3_MAX_PLATFORM_FILENAME CELL_FS_MAX_FS_PATH_LENGTH

/** Predefined path to root of game on a BD */
#define DRIVE_BD	SYS_DEV_BDVD"/PS3_GAME/USRDIR"

/** Predefined path to host fs root */
#define DRIVE_APP	SYS_APP_HOME"/PS3_GAME/USRDIR"

/** Global to the PS3 file manager, for easy access and less type-casting of GFileManager */
extern class FFileManagerPS3* GFileManagerPS3;

/** This will make sure that the shutdown sysutil function isn't fully handled while we are saving, to avoid save corruption */
extern FCriticalSection GNoShutdownWhileSavingSection;

/** Scheduler object that performs almost all file operations */
extern cell::fios::scheduler* GScheduler;

/** Base directory to run from (overridable from the commandline) */
extern FString GBaseDirectory;

/** Possible stages of mod importing */
enum EModImportStage
{
	MIS_Inactive,		// not doing anything
	MIS_Importing,		// using sysutil to import from media device
	MIS_Canceled,		// user canceled in the import dialog
	MIS_ReadyToUnpack,	// ready to kick off install/unpack (for internal use)
	MIS_Unpacking,		// unpacking from imported file to HD
	MIS_Failed,			// failed to unpack
	MIS_Succeeded,		// succeeded unpacking 
	MIS_PackageName,	// there was a package name conflict
};


/**
 * Wrapper around cellFsOpen that will perform infinite retries on read errors
 *
 * @param Name Name of file
 * @param Flags Open flags
 */
PS3Fd PS3CheckedFsOpen(const ANSICHAR* Name, INT Flags);

/**
 * Wrapper around cellFsRead that will perform infinite retries on read errors
 *
 * @param FileDescriptor File handle
 * @param Buffer Output buffer to receive the read data
 * @param NumberOfBytes Number of bytes to read
 * @param BytesRead Output value to receive the number of bytes that were read
 */
void PS3CheckedFsRead(PS3Fd FileDescriptor, void* Buffer, PS3Offset NumberOfBytes, PS3Offset* BytesRead);

/**
 * Wrapper around cellFsStat that will perform infinite retries on read errors
 *
 * @param Path Path to file
 * @param Stat [out] Stat struct of file
 */
void PS3CheckedFsStat(const ANSICHAR* Path, PS3FsStat *Stat);

/**
 * Wrapper around cellFsLseek that will perform infinite retries on read errors
 *
 * @param FileDescriptor File handle
 * @param Offset How far to seek, depends on Type
 * @param Type Type of seek (SET, CUR, END)
 * @param CurrentPos [out] Current file position
 */
void PS3CheckedFsLseek(PS3Fd FileDescriptor, PS3Offset Offset, cell::fios::e_WHENCE Type, PS3Offset *CurrentPos);


/**
 * Converts an Unreal UNICODE Filename into one usable on PS3.
 *
 * @param	Filename		Filename to convert
 * @param	OutputFilename	Output buffer for converted filename (should be PS3_MAX_PLATFORM_FILENAME big)
 */
void GetPlatformFilename(const TCHAR* Filename, ANSICHAR* OutputFilename);
void GetPlatformFilename(const ANSICHAR* Filename, ANSICHAR* OutputFilename);

SceNpDrmKey appPS3GetHDDBootDrmKey();

/**
 * Wrapper class around FIOS Stacks
 */
class FFIOSStack
{
public:

	/**
	 * Perform one time initialization 
	 */
	static void StaticInit();

	/**
	 * Constructor
	 */
	FFIOSStack();

	/**
	 * Destructor
	 */
	~FFIOSStack()
	{
		DestroyCurrentStack(FALSE);
	}

	/**
	 * Create a FIOS stack that can read from source, and write to the temp game data directories
	 */
	void CreateGameDataInitializationStack(const ANSICHAR* TempContentDir, const ANSICHAR* TempDataDir);

	/**
	 * Create a FIOS stack that can read from source, cache to HDD1, write special files to HOSTFS, write other files to HDD0
	 */
	void CreateStandardStack(const ANSICHAR* DataDir, const ANSICHAR* TitleID);

	/**
	 * Update the FIOS stack after GSys creation, now that paths are set
	 */
	void PostGSysCreation();

	/**
	 * Teardown the stack and free any allocated resources
	 */
	void DestroyCurrentStack(UBOOL bShouldTerminateFIOS);

	/**
	 * Find patch files, and return their paths
	 */
	void HandlePatchFiles(TArray<FString>& PatchFiles);

	/**
	 * Look for DLC and redirect reads to there if they exist
	 * 
	 * @return TRUE if there were any DLC files installed
	 */
	UBOOL HandleDLC();

	/**
	 * Cleans up memory after FindFiles reads directories
	 */
	void CleanupFindFiles();

	/**
	 * Tosses HDD1 cached files
	 */
	void FlushCache();

private:

	/**
	 * @return if it's okay to read/write using HostFS (different then if we should load source files from Host)
	 */
	UBOOL ShouldLoadFromHostFS() const;


	/** The BluRay drive */
	cell::fios::ps3media* MediaBluRay;

	/** The HOSTFS drive */
	cell::fios::ps3media* MediaHost;

	/** GameData directory */
	cell::fios::ps3media* MediaHDD0;

	/** GameData's content directory */
	cell::fios::ps3media* MediaHDD0Content;

	/** SystemCache drive */
	cell::fios::ps3media* MediaHDD1;

	/** Redirection to some various files in different places (layers are added to here for redirection) */
	cell::fios::overlay* OverlayMiscFiles;

	/** Redirection to DLC and Patch files (layers are added to here for redirection) */
	cell::fios::overlay* OverlayDLCFiles;

	/** The system cache (to HDD1) mechanism */
	cell::fios::schedulercache* CacheHDD1;

	/** The scheduler used internally by the cache mechanism */
	cell::fios::scheduler* SchedulerHDD1;

	/** The main scheduler used to control the whole stack */
	cell::fios::scheduler* SchedulerMain;

	/** RAM cache object */
	cell::fios::ramcache* RAMCache;

	/** PSARC dearchiver object */
	cell::fios::dearchiver* Dearchiver; 

	/** Zlib decompressor used with the Dearchiver */
	cell::fios::Compression::EdgeDecompressor* DecompressorZlib;

	/** File handles to the .psarc files */
	TArray<PS3Fd> PSARCFileHandles;

	/** A temp LayerID that we use when we don't need to ID back for anything. In SDK 320, this can be replaced with NULL */
	cell::fios::overlay::id_t DummyID;

	/** Cached value of whether or not we should allow the use of hostfs */
	UBOOL bWriteToHostFS;
};


/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/


/**
 * Helper struct to be passed to a thread for installing
 */
struct FInstallProgress
{
	/** Progress through the file installer */
	INT InstallProgress;

	/** Total files to be installed */
	INT TotalFiles;

	/** Set to 1 when the game thread wants to cancel the installation */
	INT bWantsToCancel;

	/** Set to 1 when the install thread has finished the cancel */
	INT bHasCanceled;

	/** Set to 1 on install error */
	INT bHasError;
};


/**
 * Helper struct to be passed to a thread for importing
 */
struct FModImportData
{
	/** Progress through the file installer (INT for appInterlockedExchange) */
	INT ModImportProgress;
};

class FFileManagerPS3 : public FFileManagerGeneric
{
public:
	static void StaticInit();
	static void UnmountAll();

	virtual FArchive*	CreateFileReader(const TCHAR* Filename, DWORD Flags, FOutputDevice* Error);
	virtual FArchive*	CreateFileWriter(const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT FileSize);

	virtual void		Init(UBOOL Startup);
	virtual UBOOL		Delete(const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0);
    virtual UBOOL		IsReadOnly(const TCHAR* Filename);
	virtual UBOOL		MakeDirectory(const TCHAR* Path, UBOOL Tree=0);
	virtual UBOOL		DeleteDirectory(const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0);
	virtual void		FindFiles(TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories);
	virtual UBOOL		Move( const TCHAR* Dest, const TCHAR* Src, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes );
	virtual DOUBLE		GetFileAgeSeconds(const TCHAR* Filename);
	virtual DOUBLE		GetFileTimestamp(const TCHAR* Filename);
	virtual UBOOL		SetDefaultDirectory();
	virtual UBOOL		SetCurDirectory(const TCHAR* Directory);
	virtual FString		GetCurrentDirectory();
	virtual INT			FileSize( const TCHAR* Filename );

	/**
	 * If the given file is compressed, this will return the size of the uncompressed file,
	 * if the platform supports it.
	 * @param Filename Name of the file to get information about
	 * @return Uncompressed size if known, otherwise -1
	 */
    INT UncompressedFileSize( const TCHAR* Filename );

	/** 
	 * Get the timestamp for a file
	 * @param Path Path for file
	 * @Timestamp Output timestamp
	 * @return success code
	 */
	virtual UBOOL GetTimestamp(const TCHAR* Path, FTimeStamp& Timestamp);

	/**
	 * Updates the modification time of the file on disk to right now, just like the unix touch command
	 * @param Filename Path to the file to touch
	 * @return TRUE if successful
	 */
	virtual UBOOL TouchFile(const TCHAR* Filename);

	/**
	 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
	 *
	 *	@param Filename		Platform-independent Unreal filename
	 *	@param Root			Optional root path (don't include trailing path separator)
	 *	@return				Platform-dependent full filepath
	 **/
	virtual FString GetPlatformFilepath( const TCHAR* Filename );

	/**
	 * Destroy the contents of the file cache, all files will have to be read in again
	 */
	void FlushFileCache();

	/**
	 * Start to (asynchronously) import a mod from a memory device
	 */
	UBOOL BeginImportModFromMedia();

	/**
	 * Tick the mod importing, and return the status
	 */
	EModImportStage TickImportMod();

	/**
	 * Allow user to delete saves from a list so that a save will succeed
	 */
	void BeginDeleteFromList();

	/**
	 * Ticks the delete from list util
	 *
	 * @return TRUE when the utility is complete and game can continue
	 */
	UBOOL TickDeleteFromList();


	/**
	 * Update the FIOS stack after GSys creation, now that paths are set
	 */
	void PostGSysCreation();

	/**
	 * @return if this is a HDD booted game 
	 */
	static UBOOL IsHDDBoot()
	{
		return bIsHDDBoot;
	}

	/**
	 * @return if this is a HDD booted game 
	 */
	static UBOOL IsPatchBoot()
	{
		return bIsPatchBoot;
	}

	/**
	 * @return if data should be loaded from BD instead of HOSTFS (IsHDDBoot takes precedence)
	 */
	static UBOOL ShouldLoadFromBD()
	{
		return bShouldLoadfromBD;
	}

protected:
	virtual UBOOL	IsDrive( const TCHAR* Path );

	/** Progression through install process */
	FInstallProgress InstallProgress;

	/** Helper for thread communication */
	FModImportData ModImportData;

	/** Determines if the delete from list is complete */
	FThreadSafeCounter* DeleteFromListCompleteCounter;

	/** Helper struct for using the offline created TOC file */
	FTableOfContents TOC;

	/** The FIOS stack used for all file operations other than GameData creation */
	FFIOSStack MainStack;

	/** Is the game booted from the HDD vs a BD/patch? */
	static UBOOL bIsHDDBoot;

	/** Is the game booted from a patch of a BD disc? */
	static UBOOL bIsPatchBoot;

	/** Should we load data from BD (independent of how booted) */
	static UBOOL bShouldLoadfromBD;

	/**
	 *	Looks up the size of a file by opening a handle to the file.
	 *
	 *	@param	Filename	The path to the file.
	 *	@return	The size of the file or -1 if it doesn't exist.
	 */
	virtual INT InternalFileSize(const TCHAR* Filename);
};

#endif

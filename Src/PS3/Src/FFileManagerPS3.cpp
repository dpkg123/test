/*=============================================================================
	FFileManagerPS3.cpp
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sys/types.h>
#include <sys/sys_time.h>
#include <unistd.h>
#include <sys/process.h>
#include <cell/fs/cell_fs_file_api.h>
#include <cell/rtc.h>
#include <sys/paths.h>
#include <sys/memory.h>
#include <sysutil/sysutil_gamecontent.h>
#include <sysutil/sysutil_savedata.h>
#include <sysutil/sysutil_storagedata.h>
#include <sysutil/sysutil_syscache.h>
#include <sysutil/sysutil_msgdialog.h>
#include <sysutil/sysutil_oskdialog.h>
#include <np/drm.h>

#include "Engine.h"
#include "FFileManagerPS3.h"
#include "PS3Drv.h"
#include "PS3DownloadableContent.h"
#include "UnNet.h"
#include "PS3IDs.h"
#include "PS3NetworkPlatform.h"

//////////////////////////////////////////////////////////////////////////
// 
// Per-game settings. All games should set these!


/**
 * A slop amount that is always required, even if all of GGameDataRequiredKB is used with data.
 * If GGameDataRequiredKB - current cached data is less then this value, a warning will be displayed.
 */
static INT GGameDataRequiredSlopKB = 3 * 1024; // enough space for a save game with icons if it doesn't exist

/**
 * Max size of a save game "payload". This makes it simpler to deal with knowing we have enough space
 */
static const INT GSaveGameMaxPayloadSize = 4 * 1024;

/**
 * Don't allow HDD0 writes if the amount of free space is less than this amount. This works around the 
 * inaccuracy of the OS reporting free HD space. Note that it's possible this could be 0, but if someone 
 * is almost full HDD, then they'll understand.
 */
static const INT GReserveSpaceSlopMB = 3;

/**
 * Make a file that we force install into the game data directory. If you don't need this, simply delete the 
 * line, and then the code associated with it won't be compiled in. We use a UT file as a test.
 * NOTE: THIS FILE IS ONLY INSTALLED AT GAMEDATA CREATION TIME! It will not reinstall if the file is updated!
 */
// #define REQUIRED_FILE "/UDKGAME/COOKEDPS3/CHARTEXTURES.TFC"

// 
//////////////////////////////////////////////////////////////////////////

#if !FINAL_RELEASE
#include <sys/gpio.h>
/** allow for a command-line switch to enable read error injection */
UBOOL GHandleErrorInjection = FALSE;
#endif

// Everything in the FIOS API is in the "cell" namespace:
using namespace cell;

/** This will make sure that the shutdown sysutil function isn't fully handled while we are saving, to avoid save corruption */
FCriticalSection GNoShutdownWhileSavingSection;

/** This will make sure only one file system operation is happening at once */
FCriticalSection GCheckedFSCriticalSection;

/** Global to the PS3 file manager, for easy access and less type-casting of GFileManager */
FFileManagerPS3* GFileManagerPS3 = NULL;

/** Base directory to run from (overridable from the commandline) */
FString GBaseDirectory(TEXT("UnrealEngine3"));

// Title ID as retrieved fro the PARAM.SFO (or DEFAULT_PS3_TITLE_ID if no PARAM.SFO found)
char GSonyTitleID[10];

/** If this is true, we can not recover the file system, so don't start new file ops */
UBOOL GNoRecovery = FALSE;

/** Is the game booted from the HDD vs a BD/patch? */
UBOOL FFileManagerPS3::bIsHDDBoot;

/** Is the game booted from a patch of a BD disc? */
UBOOL FFileManagerPS3::bIsPatchBoot;

/** Should we load data from BD (independent of how booted) */
UBOOL FFileManagerPS3::bShouldLoadfromBD;


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

/**
 * Converts an Unreal UNICODE Filename into one usable on PS3.
 *
 * @param	Filename		Filename to convert
 * @param	OutputFilename	Output buffer for converted filename
 */
void GetPlatformFilename(const ANSICHAR* Filename, ANSICHAR* OutputFilename)
{
	// is the file a in the .. format (as the standard unreal files will be)
	if (strncmp(Filename, "..", 2) == 0)
	{
		const ANSICHAR* CleanFilename = Filename + 3;

		// Skip an additional "..\" if we need to
		if (strncmp(CleanFilename, "..", 2) == 0)
		{
			CleanFilename += 3;
		}

		strcpy(OutputFilename, "/");
		strcat(OutputFilename, CleanFilename);
	}
	// otherwise, it will be a full path
	else
	{
		strcpy(OutputFilename, Filename);
	}

	// Convert PATH_SEPARATOR
	ANSICHAR* Dummy	= OutputFilename;
	while( *Dummy )
	{
		if( *Dummy == '\\' )
		{
			*Dummy = '/';
		}
		else
		{
			// force uppercase for all filenames (to avoid any BD case sensitivity issues)
			*Dummy = appToUpper(*Dummy);
		}

		Dummy++;
	}
}

/**
 * Converts an Unreal UNICODE Filename into one usable on PS3.
 *
 * @param	Filename		Filename to convert
 * @param	OutputFilename	Output buffer for converted filename
 */
void GetPlatformFilename(const TCHAR* Filename, ANSICHAR* OutputFilename)
{
	GetPlatformFilename(TCHAR_TO_ANSI(Filename), OutputFilename);
}

/*-----------------------------------------------------------------------------
	FArchiveFileReaderPS3.
-----------------------------------------------------------------------------*/


// File manager.
class FArchiveFileReaderPS3 : public FArchive
{
public:
	FArchiveFileReaderPS3(PS3Fd Handle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize);
	~FArchiveFileReaderPS3();

	void Seek(INT InPos);
	INT Tell();
	INT TotalSize();
	UBOOL Close();
	void Serialize(void* V, INT Length);

protected:
	UBOOL InternalPrecache(INT PrecacheOffset, INT PrecacheSize);

	PS3Fd			Handle;
	/** Handle for stats tracking. */
	INT				StatsHandle;
	/** Filename for debugging purposes */
	FString         Filename;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	// @todo ps3 fios ask: Play with this size?
	BYTE			Buffer[1024]; // Reads smaller than this are buffered
};

/**
 * Constructor
 *
 * @param InHandle File handle of an open file
 * @param InError Device to send errors to
 * @param InSize Size of the file
 */
FArchiveFileReaderPS3::FArchiveFileReaderPS3(PS3Fd InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError, INT InSize)
: Handle		(InHandle)
, StatsHandle	(InStatsHandle)
, Filename		(InFilename)
, Error			(InError)
, Size			(InSize)
, Pos			(0)
, BufferBase	(0)
, BufferCount	(0)
{
	// mark FArchive flags.
	ArIsLoading = ArIsPersistent = TRUE;
}

/**
 * Destructor
 */
FArchiveFileReaderPS3::~FArchiveFileReaderPS3()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );
	Close();
}

UBOOL FArchiveFileReaderPS3::InternalPrecache( INT PrecacheOffset, INT PrecacheSize )
{ 
	// Only precache at current position and avoid work if precaching same offset twice.
	if( Pos == PrecacheOffset && (!BufferBase || !BufferCount || BufferBase != Pos) )
	{
		BufferBase = Pos;
		BufferCount = Min( Min( PrecacheSize, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		SCOPED_FILE_IO_READ_STATS( StatsHandle, BufferCount, Pos );

		PS3Offset NumRead;
		PS3CheckedFsRead(Handle, Buffer, BufferCount, &NumRead);
	}
	return TRUE;
}

void FArchiveFileReaderPS3::Seek( INT InPos )
{
	PS3Offset OutPos;
	PS3CheckedFsLseek(Handle, InPos, fios::kSEEK_SET, &OutPos);

	// Reset the buffer caching. We can't overflow Pos here since InPos is 32 bits.
	Pos = OutPos;
	BufferBase  = Pos;
	BufferCount = 0;
}

INT FArchiveFileReaderPS3::Tell()
{
	return Pos;
}

INT FArchiveFileReaderPS3::TotalSize()
{
	return Size;
}

UBOOL FArchiveFileReaderPS3::Close()
{
	CellFsErrno Err = GScheduler->closeFileSync(NULL, Handle);
	if (Err != fios::CELL_FIOS_NOERROR)
	{
		Error->Logf( TEXT("closeFileSync failed: error=%x file %s"), Err, *Filename );
		ArIsError = TRUE;
	}
	Handle = UNSET_FILE_HANDLE;
	return !ArIsError;
}

void FArchiveFileReaderPS3::Serialize( void* V, INT Length )
{
	// Reads larger than the sizeof(Buffer) are passed through, smaller reads
	// cause reads in sizeof(Buffer) size chunks via InternalPrecache(), or if
	// data has already been read, it's just copied out of Buffer.
	while (Length > 0)
	{
		INT Copy = Min(Length, BufferBase + BufferCount - Pos);
		if (Copy == 0)
		{
			if (Length >= ARRAY_COUNT(Buffer))
			{
				PS3Offset NumRead;
				{
					SCOPED_FILE_IO_READ_STATS( StatsHandle, Length, Pos );
					PS3CheckedFsRead(Handle, V, Length, &NumRead);
				}
				check ((Pos + Length) < MAXINT); // 32 bit overflow
				Pos += Length;
				// @todo ps3 fios: why is BufferBase updated here? This should invalidate
				// cached data, but does it? And why would we want to do that? 
				// I think this is broken on other platforms (iPhone, etc)
				BufferBase += Length;
				return;
			}
			InternalPrecache( Pos, MAXINT );
			Copy = Min(Length, BufferBase + BufferCount - Pos);
			if (Copy<=0)
			{
				ArIsError = 1;
				Error->Logf(TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size);
			}
			if (ArIsError)
				return;
		}

		BYTE* Dest = (BYTE*)V;
		BYTE* Src = (BYTE*)Buffer + Pos - BufferBase;
		// copy from buffer to destination
		appMemcpy(Dest, Src, Copy);
		Pos       += Copy;
		Length    -= Copy;
		V          = (BYTE*)V + Copy;
	}
}


/*-----------------------------------------------------------------------------
	FArchiveFileWriterPS3.
-----------------------------------------------------------------------------*/


class FArchiveFileWriterPS3 : public FArchive
{
public:
	FArchiveFileWriterPS3(PS3Fd Handle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError);
	~FArchiveFileWriterPS3();

	void Seek(INT InPos);
	INT Tell();
	UBOOL Close();
	void Serialize(void* V, INT Length);
	void FlushNoSync();
	void Flush();

protected:
	/** Internal file handle */
	PS3Fd			Handle;
	FString			Filename;
	/** Handle for stats tracking. */
	INT				StatsHandle;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	/** all writes are buffered, so use a value larger than that for reads */
	// @todo ps3 fios ask: This should be aligned, apparently, but to what?
	BYTE			Buffer[8192];
};

FArchiveFileWriterPS3::FArchiveFileWriterPS3(PS3Fd InHandle, INT InStatsHandle, const TCHAR* InFilename, FOutputDevice* InError)
: Handle(InHandle)
, Filename		(InFilename)
, StatsHandle	(InStatsHandle)
, Error			(InError)
, Pos			(0)
, BufferCount	(0)
{
	// set FArchive flags
	ArIsSaving = ArIsPersistent = TRUE;
}

FArchiveFileWriterPS3::~FArchiveFileWriterPS3()
{
	FILE_IO_STATS_CLOSE_HANDLE( StatsHandle );

	// close down the file if needed
	Close();
}

void FArchiveFileWriterPS3::Seek( INT InPos )
{
	// make sure buffer is flushed out
	FlushNoSync();
	check(InPos >= 0);
	Pos = InPos;
	// seek to the location specified
	CellFsErrno err = GScheduler->seekFileSync( NULL, Handle, (PS3Offset) InPos, fios::kSEEK_SET);
	if (err != fios::CELL_FIOS_NOERROR)
	{
		ArIsError = 1;
		Error->Logf( TEXT("seekFileSync failed: offset=%i error=%x file %s"), InPos, err, *Filename );
	}
}

INT FArchiveFileWriterPS3::Tell()
{
	return Pos;
}

UBOOL FArchiveFileWriterPS3::Close()
{
	FlushNoSync();
	CellFsErrno err = GScheduler->closeFileSync(NULL, Handle);
	Handle = UNSET_FILE_HANDLE;
	if (err != fios::CELL_FIOS_NOERROR)
	{
		Error->Logf( TEXT("closeFileSync failed: error=%x file %s"), err, *Filename );
		ArIsError = TRUE;
	}
	return !ArIsError;
}

void FArchiveFileWriterPS3::Serialize(void* V, INT Length)
{
	Pos += Length;
	INT Copy;
	while (Length > (Copy=ARRAY_COUNT(Buffer) - BufferCount))
	{
		appMemcpy(Buffer + BufferCount, V, Copy);
		BufferCount += Copy;
		Length      -= Copy;
		V            = (BYTE*)V + Copy;
		// @todo ps3 fios ask: For writes larger than sizeof(Buffer), flush and then directly write out the data.
		FlushNoSync();
	}

	if(Length)
	{
		appMemcpy(Buffer + BufferCount, V, Length);
		BufferCount += Length;
	}
}

void FArchiveFileWriterPS3::FlushNoSync()
{
	if (BufferCount)
	{
		SCOPED_FILE_IO_WRITE_STATS( StatsHandle, BufferCount, 0 );
		
		// @todo ps3 fios ask: This is synchronous?
		CellFsErrno Err = GScheduler->writeFileSync(NULL, Handle, Buffer, BufferCount);
		if (Err != fios::CELL_FIOS_NOERROR)
		{
			Error->Logf( TEXT("write failed: BufferCount=%i error=%x file %s"),	BufferCount, Err, *Filename );
		}
		BufferCount = 0;
	}
}

void FArchiveFileWriterPS3::Flush()
{
	// flushes and syncs, we probably don't require the sync, (AFAICT) users
	// can still directly call Flush().
	if (BufferCount)
	{
		FlushNoSync();
		CellFsErrno Err = GScheduler->syncFileSync(NULL, Handle);
		if (Err != fios::CELL_FIOS_NOERROR)
		{
			Error->Logf( TEXT("syncFileSync failed: error=%x file %s"), Err, *Filename );
		}
	}
}

/*-----------------------------------------------------------------------------
	FArchiveSaveDataReaderPS3.
-----------------------------------------------------------------------------*/

/**
 * Icon file enum type
 */
enum EPS3SaveStep
{
	SS_ICON0, // still icon
	SS_ICON1, // movie icon
	SS_PIC1, // background
	SS_SND0, // sound
	SS_PAYLOAD, // actual save data

	SS_MAX
};

/**
 * Icon file info
 */
static struct 
{
	/** Name of file */
	const ANSICHAR* Name;

	/** PS3 type */
	INT Type;

} GIconInfo[] = 
{
	{ "ICON0.PNG", CELL_SAVEDATA_FILETYPE_CONTENT_ICON0 },
	{ "ICON1.PAM", CELL_SAVEDATA_FILETYPE_CONTENT_ICON1 },
	{ "PIC1.PNG", CELL_SAVEDATA_FILETYPE_CONTENT_PIC1 },
	{ "SND0.AT3", CELL_SAVEDATA_FILETYPE_CONTENT_SND0 },
};


#define MEM_CONTAINER_SIZE (4 * 1024 * 1024)
#define SIZEKB(x) ((x + 1023) / 1024)

// @todo: Use better key?
static const char GSecureFileId[CELL_SAVEDATA_SECUREFILEID_SIZE] =
	{ 0xA, 0xB, 0x1, 0x7, 0xD, 0x6, 0x1, 0xC, 0x9, 0x5, 0x2, 0x6, 0x9, 0xC, 0xA, 0x1 };

/** If this is true, the next save will overwrite all files because there was some error while loading */
static UBOOL GForceNextSaveAsIfNew = FALSE;

// forward declaring friend functions (for gcc 4.1.1)
void LoadDataStatusFunc(CellSaveDataCBResult *Result, CellSaveDataStatGet *Get, CellSaveDataStatSet *Set);
void LoadFileOpFunc(CellSaveDataCBResult *Result, CellSaveDataFileGet *Get, CellSaveDataFileSet *Set);

class FArchiveSaveDataReaderPS3 : public FMemoryReader
{
public:
	FArchiveSaveDataReaderPS3(FOutputDevice* InError)
	: FMemoryReader(Storage)
	, Error(InError)
	{
	}

	// make sure the archive is closed before deleting
	~FArchiveSaveDataReaderPS3()
	{
		Close();
	}
	
	/**
	 * Close save reader, and return success
	 * @return TRUE if no errors occurred
	 */
	UBOOL Close()
	{
		UBOOL bWasSuccessful = !ArIsError;
		
		return bWasSuccessful;
	}

	/**
	 * Inits the save reader object by reading the data. Can fail, so can't use
	 * constructor.
	 *
	 * @param SaveDataName Which save to load
	 * 
	 * @return TRUE if successful
	 */
	UBOOL Init(const TCHAR* SaveDataName)
	{
		// make the path to the save data
		FFilename SaveDataPath = FFilename(SaveDataName).GetPath().ToUpper();
		
		// make sure there aren't any subdirectories
		check(SaveDataPath.GetPath() == TEXT(""));
		SaveDataPath = FString(GSonyTitleID) + TEXT("-") + SaveDataPath;

		debugf(TEXT("Loading save game [%s]!"), *SaveDataPath);

		// make sure GPU isn't going while we jam up the works
		extern void FlushRenderingCommands();
		FlushRenderingCommands();

		// fill out helper struct
		CellSaveDataSetBuf SaveSet;
		appMemzero(&SaveSet, sizeof(SaveSet));
		SaveSet.fileListMax = SS_MAX; // number of possible files
		SaveSet.dirListMax = 0;
		SaveSet.bufSize = SaveSet.fileListMax * sizeof(CellSaveDataFileStat) + SaveSet.dirListMax * sizeof(CellSaveDataDirList);
		SaveSet.buf = appMalloc(SaveSet.bufSize);

		// Enqueue command to suspend rendering during blocking load
		ENQUEUE_UNIQUE_RENDER_COMMAND( SuspendRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = TRUE; } );

		// The first time the game is run, there will be no save data.  A warning will be issued about this that requires user response.
		// So, we expose a command line argument for suppressing this warning for the sake of e.g. automated perf tests, etc.
		UINT ErrorSettings = CELL_SAVEDATA_ERRDIALOG_ALWAYS;

		INT FromCommandLine = 0;
		Parse( appCmdLine(), TEXT("AutomatedPerfTesting="), FromCommandLine );

		if ( GIsUnattended
			|| FromCommandLine != 0
			|| ParseParam(appCmdLine(),TEXT("ps3nosavedataerrors")) )
		{
			ErrorSettings = CELL_SAVEDATA_ERRDIALOG_NONE;
		}

		// do the load thing
		const INT Return = cellSaveDataAutoLoad2(CELL_SAVEDATA_VERSION_CURRENT, TCHAR_TO_ANSI(*SaveDataPath),
			ErrorSettings, &SaveSet, LoadDataStatusFunc, LoadFileOpFunc,
			SYS_MEMORY_CONTAINER_ID_INVALID, (void*)this);

		// if the data was corrupted, then next time we save, toss all data before saving
		if (Return == CELL_SAVEDATA_ERROR_BROKEN)
		{
			GForceNextSaveAsIfNew = TRUE;
		}

		// free up used memory
		appFree(SaveSet.buf);

		// Resume rendering again now that we're done loading.
		ENQUEUE_UNIQUE_RENDER_COMMAND( ResumeRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = FALSE; RHIResumeRendering(); } );

		// if user canceled or there was an error or there was no data read (didn't exist), return FALSE
		return Return == 0 && Storage.Num() > 0;
	}

	// allow callbacks into our struct
	friend void LoadDataStatusFunc(CellSaveDataCBResult *Result, CellSaveDataStatGet *Get, CellSaveDataStatSet *Set);
	friend void LoadFileOpFunc(CellSaveDataCBResult *Result, CellSaveDataFileGet *Get, CellSaveDataFileSet *Set);

protected:
	// array to store save data
	TArray<BYTE> Storage;

	// error sevice
	FOutputDevice* Error;

	/** Progress through the load */
	INT CurrentLoadStep;
};


/** 
 * Status callback function while loading data
 */
void LoadDataStatusFunc(CellSaveDataCBResult *Result, CellSaveDataStatGet *Get, CellSaveDataStatSet *Set)
{
	// get user data (the arhive object)
	FArchiveSaveDataReaderPS3* Archive = (FArchiveSaveDataReaderPS3*)Result->userdata;

	// make sure it exists
	if (Get->isNewData) 
	{
		printf("Data %s is not found.\n", Get->dir.dirName );
		// go to next step, but the loadstep is off the end, so the FileOp will do nothing and complete
		Archive->CurrentLoadStep = SS_MAX;
		Result->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
		return;
	}

	// set up load operation
	Set->reCreateMode = CELL_SAVEDATA_RECREATE_NO_NOBROKEN;
	Set->setParam = NULL;

	// show a default indicator
	static CellSaveDataAutoIndicator Indicator;
	appMemzero(&Indicator, sizeof(CellSaveDataAutoIndicator));
	Set->indicator = &Indicator;

	// find the file in the list
	for (UINT FileIndex = 0; FileIndex < Get->fileListNum; FileIndex++ )
	{
		if (strcmp(Get->fileList[FileIndex].fileName, "PAYLOAD") == 0)
		{
			// make room to read it in
			Archive->Storage.Add((INT)Get->fileList[FileIndex].st_size);
		}
	}

	// move on to next step (read payload data)
	Archive->CurrentLoadStep = SS_PAYLOAD;
	Result->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

#if !FINAL_RELEASE
	if (ParseParam(appCmdLine(), TEXT("ps3errorload")))
	{
		Result->result = CELL_SAVEDATA_CBRESULT_ERR_BROKEN;
	}
#endif
}

/** 
 * File save callback function while loading data
 */
void LoadFileOpFunc(CellSaveDataCBResult *Result, CellSaveDataFileGet *Get, CellSaveDataFileSet *Set)
{
	// get user data (the arhive object)
	FArchiveSaveDataReaderPS3* Archive = (FArchiveSaveDataReaderPS3*)Result->userdata;

	if (Archive->CurrentLoadStep == SS_PAYLOAD)
	{
		Set->fileOperation = CELL_SAVEDATA_FILEOP_READ;
		Set->fileName = "PAYLOAD";
		Set->fileBuf = Archive->Storage.GetData();
		Set->fileBufSize = Archive->Storage.Num();
		Set->fileSize = Archive->Storage.Num();
		Set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
		memcpy(Set->secureFileId, GSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE);

		// move on to next step
		Archive->CurrentLoadStep++;
		Result->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
	}
	else
	{
		Result->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
	}

}







/*-----------------------------------------------------------------------------
	FArchiveSaveDataWriterPS3.
-----------------------------------------------------------------------------*/


// forward declaring friend functions (for gcc 4.1.1)
void SaveDataStatusFunc(CellSaveDataCBResult *Result, CellSaveDataStatGet *Get, CellSaveDataStatSet *Set);
void SaveFileOpFunc(CellSaveDataCBResult *Result, CellSaveDataFileGet *Get, CellSaveDataFileSet *Set);
void SaveDataThreadProc(uint64_t Param);


			   
/**
 * Save game writing helped struct
 */
class FArchiveSaveDataWriterPS3 : public FMemoryWriter
{
public:
	FArchiveSaveDataWriterPS3(const TCHAR* InSaveDataName, UBOOL bInUseAsyncWrite, FOutputDevice* InError)
	: FMemoryWriter(Storage)
	, SaveDataName(InSaveDataName)
	, Error(InError)
	, bUseAsyncWrite(bInUseAsyncWrite)
	, bIsThreadActive(0)
	, bHasError(FALSE)
	{
		// uppercasify
		SaveDataName = FFilename(SaveDataName.ToUpper()).GetPath();
		
		// make sure there aren't any subdirectories
		check(SaveDataName.GetPath() == TEXT(""));

		SaveDataName = FString(GSonyTitleID) + TEXT("-") + SaveDataName;
	}

	// make sure the archive is closed before deleting
	~FArchiveSaveDataWriterPS3()
	{
		// can't do an async write if the object is about to be destroyed
		checkf(Storage.Num() == 0 || bUseAsyncWrite == FALSE, TEXT("Badness: StorageNum = %d, bUseAsyncWrite: %d"), Storage.Num(), bUseAsyncWrite);

		// close the file and write it out
		Close();
	}
	
	UBOOL Close()
	{
		// reset params
		bIsThreadActive = 0;
		bHasError = FALSE;

		// don't save if we ejected the disc with no recovery
		if (GNoRecovery || GIsRequestingExit)
		{
			Error->Logf(TEXT("NORECOVERY"));
			bHasError = TRUE;
			Storage.Empty();
			return FALSE;
		}
		
		if (Storage.Num() > 0)
		{
			if (bUseAsyncWrite)
			{
				// thread is now in use, don't do anything else here
				bIsThreadActive = 1;

				// kick off the save data in a thread
				sys_ppu_thread_t Thread;
				INT Err = sys_ppu_thread_create(&Thread, SaveDataThreadProc, (uint64_t)(PTRINT)this, 1000, 64 * 1024, 0, "SaveData");
				checkf(Err == CELL_OK, TEXT("Failed to create savedata thread, ret = %d"), Err);
			}
			else
			{
				// if not async, then just do the write right now
				PerformSaveDataWrite();
			}
		}

		return !bHasError;
	}

	/**
	 * Performs the actual save operation (can be in a separate thread or not
	 */
	void PerformSaveDataWrite()
	{
		// don't allow shutting down while this is happening
		FScopeLock ScopeLock(&GNoShutdownWhileSavingSection);

		// fill out helper struct
		CellSaveDataSetBuf SaveSet;
		appMemzero(&SaveSet, sizeof(SaveSet));
		SaveSet.fileListMax = SS_MAX; // number of possible files
		SaveSet.dirListMax = 0;
		SaveSet.bufSize = SaveSet.fileListMax * sizeof(CellSaveDataFileStat) + SaveSet.dirListMax * sizeof(CellSaveDataDirList);
		SaveSet.buf = appMalloc(SaveSet.bufSize);

		// do the save thing
		INT Return = cellSaveDataAutoSave2(CELL_SAVEDATA_VERSION_CURRENT, TCHAR_TO_ANSI(*SaveDataName),
			CELL_SAVEDATA_ERRDIALOG_NOREPEAT, &SaveSet, SaveDataStatusFunc, SaveFileOpFunc,
			SYS_MEMORY_CONTAINER_ID_INVALID, (void*)this);

		// negative numbers mean an error
		bHasError = Return < 0;
		if (Return == CELL_SAVEDATA_ERROR_CBRESULT)
		{
			// this means we were out of space, as it's the only callback result we set other than OK
			Error->Logf(TEXT("NOSPACE"));
		}
		else if (bHasError)
		{
			Error->Logf(TEXT("UNKNOWN"));
		}
		else
		{
			Error->Logf(TEXT("OK"));
		}

		// free up used memory
		appFree(SaveSet.buf);

		// don't close twice
		Storage.Empty();
	}

	/**
	 * Returns if an async close operation has finished or not, as well as if there was an error
	 *
	 * @param bHasError TRUE if there was an error
	 *
	 * @return TRUE if the close operation is complete (or if Close is not an async operation) 
	 */
	virtual UBOOL IsCloseComplete(UBOOL& bOutHasError)
	{
		if (bIsThreadActive)
		{
			bOutHasError = FALSE;
			return FALSE;
		}
		else
		{
			bOutHasError = bHasError;
			return TRUE;
		}
	}

	// allow callbacks into our struct
	friend void SaveDataStatusFunc(CellSaveDataCBResult *Result, CellSaveDataStatGet *Get, CellSaveDataStatSet *Set);
	friend void SaveFileOpFunc(CellSaveDataCBResult *Result, CellSaveDataFileGet *Get, CellSaveDataFileSet *Set);
	friend void SaveDataThreadProc(uint64_t Param);

protected:
	/** Array to store save data */
	TArray<BYTE> Storage;

	/** Name of save data to write on close */
	FFilename SaveDataName;

	/** Error device */
	FOutputDevice* Error;

	/** Which step into the save process we are in */
	INT CurrentSaveStep;

	/** Data holding the contents of an icon (icon0, icon1, etc) file. Static so the contents stay around while sysutil is using the contents */
	static TArray<BYTE> GIconFileContents;

	/** TRUE if the caller requested an async write operation */
	UBOOL bUseAsyncWrite;

	/** Is the saving thread active. While thread is active, no operations should take place on this object, don't even view properties! */
	INT bIsThreadActive;

	/** TRUE if the thread proc got an error */
	UBOOL bHasError;

};

TArray<BYTE> FArchiveSaveDataWriterPS3::GIconFileContents;


void SaveDataThreadProc(uint64_t Param)
{
	// get the writer object
	FArchiveSaveDataWriterPS3* Writer = (FArchiveSaveDataWriterPS3*)(PTRINT)Param;
	Writer->PerformSaveDataWrite();

	// mark the thread as complete
	appInterlockedExchange(&Writer->bIsThreadActive, 0);

	// exit the thread
	sys_ppu_thread_exit(0);
}


/**
 * Calculate how much space we need to save out all data + icons
 *
 * @param PayloadSize Size in KB of actul save game data
 * @param SystemSize Size in KB of system data
 * 
 * @return Size in KB of needed size for all files
 */
INT CalcNeededSizeKB(INT PayloadSizeKB, INT SystemSizeKB)
{
	// calculate needed size = payload size + icons + sys file size
	INT NeededSizeKB = PayloadSizeKB + SystemSizeKB;

	// get size of all the icon files
	for (INT FileIndex = 0; FileIndex < ARRAY_COUNT(GIconInfo); FileIndex++)
	{
		// look for source file
		INT FileSize = GFileManager->FileSize(*(FString(TEXT("..\\Binaries\\PS3\\Save\\")) + GIconInfo[FileIndex].Name));

		// if it exists, we will put it in the save 
		if (FileSize != -1)
		{
			NeededSizeKB += SIZEKB(FileSize);
		}
	}

	return NeededSizeKB;
}

/** 
 * Status callback function while saving data
 */
void SaveDataStatusFunc(CellSaveDataCBResult *Result, CellSaveDataStatGet *Get, CellSaveDataStatSet *Set)
{
	// get user data (the arhive object)
	FArchiveSaveDataWriterPS3* Archive = (FArchiveSaveDataWriterPS3*)Result->userdata;

	// reuse the get param for setting
	Set->reCreateMode = GForceNextSaveAsIfNew ? CELL_SAVEDATA_RECREATE_YES : CELL_SAVEDATA_RECREATE_NO_NOBROKEN;
	Set->setParam = &Get->getParam;

	// show a default indicator
	static CellSaveDataAutoIndicator Indicator;
	appMemzero(&Indicator, sizeof(CellSaveDataAutoIndicator));
	Set->indicator = &Indicator;

#if !FINAL_RELEASE
	// fake failure to write data
	if (ParseParam(appCmdLine(), TEXT("ps3errorsave")))
	{
		Result->result = CELL_SAVEDATA_CBRESULT_ERR_FAILURE;
		return;
	}
#endif
	// get free space on hard drive
	INT FreeHDSpace = Get->hddFreeSizeKB;

#if !FINAL_RELEASE
	// allow faking 0 kb free
	if (ParseParam(appCmdLine(), TEXT("ps3errorsavespace")))
	{
		FreeHDSpace = 0;
	}
#endif

	// make sure we are proper size
	checkf(Archive->Storage.Num() <= GSaveGameMaxPayloadSize, TEXT("Trying to save data bigger than max save data size. See FFileManagerPS3.cpp, GSaveGameMaxPayloadSize"));

	// set up new data
	if (Get->isNewData || GForceNextSaveAsIfNew)
	{
		// set save strings
		strcpy(Set->setParam->title, TCHAR_TO_UTF8(*LocalizeGeneral(TEXT("SaveGameTitle"), TEXT("PS3"))));
		strcpy(Set->setParam->detail,  TCHAR_TO_UTF8(*LocalizeGeneral(TEXT("SaveGameDetail"), TEXT("PS3"))));

		// look to see if a subtitle has been specified
		FString PremadeSubtitle = LocalizeGeneral(TEXT("SaveGameSubtitle"), TEXT("PS3"));
		if (PremadeSubtitle == TEXT(""))
		{
			// use the username if we don't have a hardcoded subtitle
			strcpy(Set->setParam->subTitle,	TCHAR_TO_UTF8(*appGetUserName()));
		}
		else
		{
			strcpy(Set->setParam->subTitle,	TCHAR_TO_UTF8(*PremadeSubtitle));
		}

		strcpy(Set->setParam->listParam, "UNKNOWN");

		// calc size needed
		INT NewSizeKB = CalcNeededSizeKB(SIZEKB(GSaveGameMaxPayloadSize), Get->sysSizeKB);

		// check if we need more than we have
		INT AvailSizeKB = FreeHDSpace - NewSizeKB;

		// handle not enough space
		if (AvailSizeKB < 0) 
		{
			Result->errNeedSizeKB = AvailSizeKB;
			Result->result = CELL_SAVEDATA_CBRESULT_ERR_NOSPACE;
			return;
		}

		// start by writing all the icons (ICON0 MUST exist or this will all fail)
		Archive->CurrentSaveStep = SS_ICON0;
	}
	// overwrite existing
	else
	{
		// update the subtitle, as it could change (if username)
		FString PremadeSubtitle = LocalizeGeneral(TEXT("SaveGameSubtitle"), TEXT("PS3"));
		if (PremadeSubtitle == TEXT(""))
		{
			// use the username if we don't have a hardcoded subtitle
			strcpy(Set->setParam->subTitle,	TCHAR_TO_UTF8(*appGetUserName()));
		}
		else
		{
			strcpy(Set->setParam->subTitle,	TCHAR_TO_UTF8(*PremadeSubtitle));
		}

		// calc size needed
		INT NewSizeKB = CalcNeededSizeKB(SIZEKB(GSaveGameMaxPayloadSize), Get->sysSizeKB);

		// calc how much the save data size has changed
		INT DiffSizeKB = NewSizeKB - Get->sizeKB;

		// only if we are growing do we need to check for avail HD space
		if (DiffSizeKB > 0)
		{
			// check if we need more than we have
			INT AvailSizeKB = FreeHDSpace - DiffSizeKB;

			// handle not enough space
			if (AvailSizeKB < 0)
			{
				Result->errNeedSizeKB = AvailSizeKB;
				Result->result = CELL_SAVEDATA_CBRESULT_ERR_NOSPACE;
				return;
			}
		}

		// if it exists, just update the save file, not all the icons files, etc
		Archive->CurrentSaveStep = SS_PAYLOAD;
	}

	// move on to next step
	Result->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;

	// reset the force overwrite flag
	GForceNextSaveAsIfNew = FALSE;

}

/**
 * Figure out what step to go to next
 */
INT GetNextStep(INT CurrentLoadStep)
{
	// special handling for the icon files, as some may not exist
	if (CurrentLoadStep < SS_PAYLOAD)
	{
		UBOOL bFoundNextFile = FALSE;

		while (bFoundNextFile == FALSE)
		{
			// move on to next file
			CurrentLoadStep++;

			// we always ahve the payload
			if (CurrentLoadStep == SS_PAYLOAD)
			{
				bFoundNextFile = TRUE;
			}
			else
			{
				// look for the file
				INT FileSize = GFileManager->FileSize(*(FString(TEXT("..\\Binaries\\PS3\\Save\\")) + GIconInfo[CurrentLoadStep].Name));
				// if it exists, we are done
				if (FileSize != -1)
				{
					bFoundNextFile = TRUE;
				}
			}
		}

		// this will be at most SS_PAYLOAD
		return CurrentLoadStep;
	}
	else
	{
		return CurrentLoadStep + 1;
	}
}

/** 
 * File save callback function while saving data
 */
void SaveFileOpFunc(CellSaveDataCBResult *Result, CellSaveDataFileGet *Get, CellSaveDataFileSet *Set)
{
	// get user data (the arhive object)
	FArchiveSaveDataWriterPS3* Archive = (FArchiveSaveDataWriterPS3*)Result->userdata;

	// handle each file
	if (Archive->CurrentSaveStep < SS_PAYLOAD)
	{
		// load source icon (assumed to be there from previous checks)
		FString Filename = FString(TEXT("..\\Binaries\\PS3\\Save\\")) + GIconInfo[Archive->CurrentSaveStep].Name;
		appLoadFileToArray(FArchiveSaveDataWriterPS3::GIconFileContents, *Filename);
		if (GIsRequestingExit)
		{
			Result->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
			return;
		}

		// set file info
		Set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
		Set->fileBuf = FArchiveSaveDataWriterPS3::GIconFileContents.GetData();
		Set->fileBufSize = FArchiveSaveDataWriterPS3::GIconFileContents.Num();
		Set->fileSize = FArchiveSaveDataWriterPS3::GIconFileContents.Num();
		Set->fileType = GIconInfo[Archive->CurrentSaveStep].Type;

		Result->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
	}
	else if (Archive->CurrentSaveStep == SS_MAX)
	{
		Result->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
	}
	else
	{
		// make sure the icon contents are emptied
		FArchiveSaveDataWriterPS3::GIconFileContents.Empty();

		// everything else should be handled above
		check(Archive->CurrentSaveStep == SS_PAYLOAD);

		Set->fileType = CELL_SAVEDATA_FILETYPE_SECUREFILE;
		Set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
		Set->fileBuf = Archive->Storage.GetData();
		Set->fileBufSize = Archive->Storage.Num();
		Set->fileSize = Archive->Storage.Num();
		// hardcode the filename
		Set->fileName = "PAYLOAD";
		appMemcpy(Set->secureFileId, GSecureFileId, CELL_SAVEDATA_SECUREFILEID_SIZE);

		Result->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
	}

	// move on to next step
	Archive->CurrentSaveStep = GetNextStep(Archive->CurrentSaveStep);
}


/*-----------------------------------------------------------------------------
	FFileManagerPS3.
-----------------------------------------------------------------------------*/

/**
 * Callback when a user hits a button in the less than desired game data space callback
 *
 * @param UserData Points to an INT that will be appInterlockedExchange'd to the button that was pressed when the callback is called
 */
void DialogCallback(INT ButtonType, void *UserData)
{
	// mark as us complete
	INT* Notify = (INT*)UserData;
	appInterlockedExchange(Notify, ButtonType);
}

/**
 * @return the size of the REQUIRED_FILE
 */
INT GetTotalRequiredFileSize()
{
#ifdef REQUIRED_FILE

	// cache the size of the required file
	static INT RequiredFileSize = -1;
	if (RequiredFileSize == -1)
	{
		// get a FIOS-usable name
		ANSICHAR RequiredHDDFile[PS3_MAX_PLATFORM_FILENAME];
		GetPlatformFilename(REQUIRED_FILE, RequiredHDDFile);

		PS3Offset FileSize;
		// get the size of the source file
		CellFsErrno Err = GScheduler->getFileSizeSync(NULL, RequiredHDDFile, &FileSize);
		if (Err == fios::CELL_FIOS_NOERROR)
		{
			RequiredFileSize = (INT)FileSize;
		}
	}

	// return size of the file
	return RequiredFileSize;
#else

	return 0;

#endif
}

void UpdateGameData()
{
	// look for game data
	CellGameContentSize ContentSize;
	INT GameCheck = cellGameDataCheck(FFileManagerPS3::IsHDDBoot() ? CELL_GAME_GAMETYPE_HDD : CELL_GAME_GAMETYPE_GAMEDATA, GSonyTitleID, &ContentSize);

	// verify that it was okay or missing (not corrupted)
	if (GameCheck != CELL_GAME_RET_NONE && GameCheck != CELL_GAME_RET_OK)
	{
		printf("GameData cellGameDataCheck, err = %x [%d]\n", GameCheck, GameCheck);
		cellGameContentErrorDialog(FFileManagerPS3::IsHDDBoot() ? CELL_GAME_ERRDIALOG_BROKEN_EXIT_HDDGAME : CELL_GAME_ERRDIALOG_BROKEN_EXIT_GAMEDATA, 0, NULL);

		// allow the user to shutdown
		while (1)
		{
			// @todo ps3 fios: This crashes (in debug at least) when shutting down
			cellSysutilCheckCallback();

			// handle shutdown if the user requested it
			if (GIsRequestingExit)
			{
				appPS3StopFlipPumperThread();
				appRequestExit(1);
			}
			appSleep(0.05);
		}
	}

	// create new data (this will only happen with an unpatched disc boot game or booting from host)
	if (GameCheck == CELL_GAME_RET_NONE)
	{
		// setup the initial PARAM.SFO information
		CellGameSetInitParams SetParams;
		appMemzero(&SetParams, sizeof(SetParams));
		strncpy(SetParams.title,   TCHAR_TO_UTF8(appPS3GetEarlyLocalizedString(ELST_GameDataDisplayName)), CELL_GAME_SYSP_TITLE_SIZE - 1) ;
		strncpy(SetParams.titleId, GSonyTitleID, CELL_GAME_SYSP_TITLEID_SIZE - 1) ;
		strncpy(SetParams.version, "01.00", CELL_GAME_SYSP_VERSION_SIZE - 1) ;
		//		for (INT LangIndex = 0; LangIndex < ARRAY_COUNT(SetParams.titleLang); LangIndex++)
		//		{
		//			strcpy( SetParams.titleLang[LangIndex], SetParams.title);
		//		}

		// create the gamedata on disk
		ANSICHAR TempContentInfoPath[CELL_GAME_PATH_MAX];
		ANSICHAR TempUserDirPath[CELL_GAME_PATH_MAX];
		cellGameCreateGameData(&SetParams, TempContentInfoPath, TempUserDirPath);

		// create a FIOS stack that can read from source and write to the temp game data directories
		FFIOSStack GameDataInitStack;
		GameDataInitStack.CreateGameDataInitializationStack(TempContentInfoPath, TempUserDirPath);

		// we need to know how much space we absolutely require
		INT AmountRequiredKB = ((GetTotalRequiredFileSize() + 1023) / 1024)  + GGameDataRequiredSlopKB;

		// make sure there is enough free space to even start
		if (ContentSize.hddFreeSizeKB < AmountRequiredKB)
		{
			cellGameContentErrorDialog(CELL_GAME_ERRDIALOG_NOSPACE_EXIT, AmountRequiredKB - ContentSize.hddFreeSizeKB, NULL);
			while (1)
			{
				cellSysutilCheckCallback();

				// handle shutdown if the user requested it
				if (GIsRequestingExit)
				{
					appPS3StopFlipPumperThread();
					appRequestExit(1);
				}
				appSleep(0.05);
			}
		}

		// now we know we have enough space for the required file and some extra (for ICON0.PNG, etc)
		ANSICHAR SourceFilename[PS3_MAX_PLATFORM_FILENAME];
		GetPlatformFilename("..\\Binaries\\PS3\\Save\\ICON0.PNG", SourceFilename);

		PS3Offset FileSize;
		CellFsErrno Err = GScheduler->getFileSizeSync(NULL, SourceFilename, &FileSize);
		checkf(Err == fios::CELL_FIOS_NOERROR, TEXT("Failed to find ICON0.PNG file"));

		// read in the contents of the file
		BYTE* FileBuffer = (BYTE*)appMalloc((DWORD)FileSize);
		Err = GScheduler->readFileSync(NULL, SourceFilename, FileBuffer, 0, FileSize);
		checkf(Err == fios::CELL_FIOS_NOERROR, TEXT("Failed to read ICON0.PNG file"));

		// write it out to HDD0Content (by using the /CONTENT mount point)
 		Err = GScheduler->writeFileSync(NULL, "/CONTENT/ICON0.PNG", FileBuffer, 0, FileSize);
		delete FileBuffer;

#ifdef REQUIRED_FILE
		// make sure the file was found, and then use async FIOS IO to install required file
		if (GetTotalRequiredFileSize())
		{
			// open up a progress bar dialog
			cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE |
				CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON | CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_NONE | CELL_MSGDIALOG_TYPE_PROGRESSBAR_SINGLE,
				TCHAR_TO_UTF8(appPS3GetEarlyLocalizedString(ELST_InstallingRequiredFile)), NULL, NULL, NULL);

			// @todo: This would be the meat of a user-install operation

			ANSICHAR RequiredFileSource[PS3_MAX_PLATFORM_FILENAME];
			GetPlatformFilename(REQUIRED_FILE, RequiredFileSource);

			// put the destination on HDD0 into the special INSTALL directory (will be called GBaseDirectory on disk)
			ANSICHAR RequiredFileDest[PS3_MAX_PLATFORM_FILENAME];
			strcpy(RequiredFileDest, "/INSTALL");
			strcat(RequiredFileDest, RequiredFileSource);

			// open the source file
			PS3Fd SourceFile = PS3CheckedFsOpen(RequiredFileSource, fios::kO_RDONLY);

			// create the directory where the file will reside
			GFileManagerPS3->MakeDirectory(*FFilename(RequiredFileDest).GetPath(), TRUE);

			// open the dest file
			PS3Fd DestFile = PS3CheckedFsOpen(RequiredFileDest, fios::kO_WRONLY | fios::kO_CREAT);

			// read and write, async and double-buffered for speed
			#define BUFFER_SIZE (512 * 1024)
			void* DoubleBuffer[2];

			DoubleBuffer[0] = appMalloc(BUFFER_SIZE);
			DoubleBuffer[1] = appMalloc(BUFFER_SIZE);

			// flip flop
			INT WhichBuffer = 0;
			fios::op* ReadOp = NULL;
			fios::op* WriteOp = NULL;
			INT AmountToRead;
			INT AmountToWrite;
			PS3Offset AmountRead;

			// how much to process
			INT AmountLeftToRead = GetTotalRequiredFileSize();
			INT NumChunks = (AmountLeftToRead + BUFFER_SIZE - 1) / BUFFER_SIZE;

			// prime the pump
			AmountToRead = Min(AmountLeftToRead, BUFFER_SIZE);
			ReadOp = GScheduler->readFile(NULL, SourceFile, DoubleBuffer[WhichBuffer], AmountToRead);
			WhichBuffer = 1 - WhichBuffer;
			AmountLeftToRead -= AmountToRead;

			INT LastPercent = 0;
			for (INT ChunkIndex = 0; ChunkIndex < NumChunks; ChunkIndex++)
			{
				// finish up outstanding ReadOp
				ReadOp->syncWait(&AmountRead); GScheduler->deleteOp(ReadOp);
				check((INT)AmountRead == AmountToRead);

				// finish up outstanding WriteOp
				if (WriteOp)
				{
					WriteOp->syncWait(); GScheduler->deleteOp(WriteOp);
				}

				// kick off the write
				WriteOp = GScheduler->writeFile(NULL, DestFile, DoubleBuffer[1 - WhichBuffer], AmountRead);

				if (AmountLeftToRead > 0)
				{
					// now read in to the other buffer
					AmountToRead = Min(AmountLeftToRead, BUFFER_SIZE);
					AmountLeftToRead -= AmountToRead;

					ReadOp = GScheduler->readFile(NULL, SourceFile, DoubleBuffer[WhichBuffer], AmountToRead);
					WhichBuffer = 1 - WhichBuffer;

					// calc percent now
					INT Percent = appTrunc(((FLOAT)(ChunkIndex + 1) / (FLOAT)NumChunks) * 100.f);

					// update progress bar
					cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, Percent - LastPercent);
					LastPercent = Percent;

					// allow a shutdown
					cellSysutilCheckCallback();
					if (GIsRequestingExit)
					{
						// we can just quite out and not worry about the fact that the file is only have done,
						// since we haven't called cellGameContentPermit yet, the temp game data will be tossed
						// on next boot
						appSleep(0.01f);
						appPS3StopFlipPumperThread();
						appRequestExit(1);
					}
				}
			}

			// finish outstanding WriteOp
			WriteOp->syncWait(); GScheduler->deleteOp(WriteOp);

			// update to final slot
			cellMsgDialogProgressBarInc(CELL_MSGDIALOG_PROGRESSBAR_INDEX_SINGLE, 100 - LastPercent);

			// close the box
			cellMsgDialogClose(0);

			// clean up
			GScheduler->closeFileSync(NULL, SourceFile);
			GScheduler->closeFileSync(NULL, DestFile);
			appFree(DoubleBuffer[0]);
			appFree(DoubleBuffer[1]);
		}
#endif

		// we are done with the temp game data FIOS stack now
		GameDataInitStack.DestroyCurrentStack(FALSE);
	}
}


/**
 * Super early initialization (finds what type of boot this is)
 */
void FFileManagerPS3::StaticInit()
{
	// get the attributes of the executable (disc, patch, hddboot, etc)
	UINT GameType, GameAttributes;
	CellGameContentSize ContentSize;
	cellGameBootCheck(&GameType, &GameAttributes, &ContentSize, NULL);

	// get the TitleID of the game
	INT Err;
	if ((Err = cellGameGetParamString(CELL_GAME_PARAMID_TITLE_ID, GSonyTitleID, ARRAY_COUNT(GSonyTitleID))) != CELL_GAME_RET_OK)
	{
		// if we couldn't get it, use the default one
		strcpy(GSonyTitleID, DEFAULT_PS3_TITLE_ID);
	}

	// now get the path to the boot file location (just to "close" the boot check operation)
	char BootInfoPath[CELL_GAME_PATH_MAX];
	char BootDataPath[CELL_GAME_PATH_MAX];
	cellGameContentPermit(BootInfoPath, BootDataPath);

	// check if the game was booted from the PC (either through debugger or via the /app_home option in the XMB)
	UBOOL bWasBootedFromPC = (GameAttributes & (CELL_GAME_ATTRIBUTE_APP_HOME | CELL_GAME_ATTRIBUTE_DEBUG)) != 0;

	// if we started up from the PC, then we don't want to change the loading behavior
	if (!bWasBootedFromPC)
	{
		// mark as an HDD boot game if it is
		if (GameType == CELL_GAME_GAMETYPE_HDD)
		{
			bIsHDDBoot = TRUE;
		}
		// otherwise, if we did not boot the game from a PC, we must have booted from either the BD or a patch of the BD
		else
		{
			bShouldLoadfromBD = TRUE;
		}
	}

	// are we booted from a patch?
	bIsPatchBoot = (GameAttributes & CELL_GAME_ATTRIBUTE_PATCH) != 0;

	// are we in lowgore mode?
	GForceLowGore = (strcmp(GSonyTitleID, FORCELOWGORE_PS3_TITLE_ID) == 0);

	printf("Boot Info: HDDBoot: %d, PatchBoot: %d, LoadFromBD: %d, TitleID: %s, Attrib: %x\n", bIsHDDBoot, bIsPatchBoot, bShouldLoadfromBD, GSonyTitleID, GameAttributes);
}

void FFileManagerPS3::Init(UBOOL Startup)
{
	// construct the global NP/trophy handler (file handling may need it to open EDAT files, so initialize it now)
	GPS3NetworkPlatform = new FPS3NetworkPlatform;
	if (!GPS3NetworkPlatform->Initialize())
	{
		checkf(0, TEXT("Failed to initialize PS3NetworkPlatform - HDD boot and trophies will fail"));
	}

	// init FIOS one time
	FFIOSStack::StaticInit();

	// save off a pointer this the file manager
	GFileManagerPS3 = this;

	// force BD file operations if bd is on the command line
	if (ParseParam(appCmdLine(), TEXT("bd")))
	{
		bShouldLoadfromBD = TRUE;
	}
	// use an already installed HDD game to load from, so it can be debugged easily
	else if (ParseParam(appCmdLine(), TEXT("hdd")))
	{
		bIsHDDBoot = TRUE;
	}

	// init vars
	DeleteFromListCompleteCounter = NULL;

	// create GameData if needed
	UpdateGameData();

	// now get the path to the boot file location (just to "close" the boot check operation)
	ANSICHAR GameInfoPath[CELL_GAME_PATH_MAX];
	ANSICHAR GameDataPath[CELL_GAME_PATH_MAX];
	cellGameContentPermit(GameInfoPath, GameDataPath);

	// create the stack for file IO going forward
	MainStack.CreateStandardStack(GameDataPath, GSonyTitleID);

#if !FINAL_RELEASE
	GHandleErrorInjection = ParseParam(appCmdLine(), TEXT("errorinjection"));
#endif

	debugf(TEXT("FFileManagerPS3::Initialized game directory %s"), ANSI_TO_TCHAR(GameDataPath));

	ModImportData.ModImportProgress = MIS_Inactive;

	// look for patch files if we are running as a patch
	TArray<FString> PatchFiles;
	if (bIsPatchBoot || ParseParam(appCmdLine(), TEXT("patch")))
	{
		MainStack.HandlePatchFiles(PatchFiles);
	}

	// use the TOC file if we are booting from disc. If HDD boot, just use the HDD
	if (!bIsHDDBoot)
	{
		// read in the TOC file
		TArray<BYTE> Buffer;
		TCHAR* TOCName;
		if (GUseCoderMode == FALSE)
		{
			TOCName = TEXT("PS3TOC.txt");
		}
		else
		{
			TOCName = TEXT("PS3TOC_CC.txt");
		}

		// merge in a per-language TOC file if it exists
		FString Lang = appGetLanguageExt();

		if (Lang != TEXT("int"))
		{
			FString LocTOCName = FString::Printf(TEXT("PS3TOC_%s%s.txt"), 
				GUseCoderMode ? TEXT("CC_") : TEXT(""),
				*Lang);

			ReadTOC( TOC, *LocTOCName, FALSE );
		}

		// read in the main TOC - this needs to be after the loc toc reading
		ReadTOC( TOC, TOCName, TRUE );

		// update the TOC with patch files
		for (INT FileIndex = 0; FileIndex < PatchFiles.Num(); FileIndex++)
		{
			PS3Offset FileSize;
			CellFsErrno Res = GScheduler->getFileSizeSync(NULL, TCHAR_TO_ANSI(*PatchFiles(FileIndex)), &FileSize);
			// this should definitely succeed, since we searched for and found the file above
			check(Res == fios::CELL_FIOS_NOERROR);

			// convert it to the relative paths the TOC wants
			FString Relative = FString(TEXT("..")) + PatchFiles(FileIndex);
			Relative = Relative.Replace(TEXT("/"), TEXT("\\"));

			// update the TOC
			TOC.AddEntry(*Relative, (INT)FileSize);
		}

		// precache save icons to HDD1, and keep them there (so disc eject during a save is safer)
		for (INT IconIndex = 0; IconIndex < ARRAY_COUNT(GIconInfo); IconIndex++)
		{
			// get the patch to each icon file
			ANSICHAR SourceFilename[PS3_MAX_PLATFORM_FILENAME];
			GetPlatformFilename(*(FString(TEXT("..\\Binaries\\PS3\\Save\\")) + GIconInfo[IconIndex].Name), SourceFilename);

			// prefetch the file
			fios::opattr_t OPAttr = FIOS_OPATTR_INITIALIZER;
			OPAttr.opflags = fios::kOPF_CACHEPERSIST;
			CellFsErrno Err = GScheduler->prefetchFileSync(&OPAttr, SourceFilename);

			// it's okay if the file doesn't exist
			checkf((Err == fios::CELL_FIOS_NOERROR) || (Err == fios::CELL_FIOS_ERROR_BADPATH), TEXT("Failed prefetching %s"), SourceFilename);
		}
	}

	// let the stack look for and redirect reads to DLC
	UBOOL bHasDLC = MainStack.HandleDLC();

#if FINAL_RELEASE
	// deal with the lock file to detect crashes
	ANSICHAR LockFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename("/INSTALL/___LOCK", LockFilename);

	// look to see if it exists
	if (FileSize(ANSI_TO_TCHAR(LockFilename)) >= 0)
	{
		// mark the autodownload cache to be fully cleared
		extern UBOOL GNeedsClearAutodownloadCache;
		GNeedsClearAutodownloadCache = TRUE;
		
		if (bHasDLC)
		{
			// the game crashed, so don't allow mods to be installed (but still can be deleted),
			// and alert the user to this
			INT ThreadSafeVar = CELL_MSGDIALOG_BUTTON_INVALID;

			// show the dialog
			cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON,
				TCHAR_TO_UTF8(appPS3GetEarlyLocalizedString(ELST_GameCrashed)),
				DialogCallback, &ThreadSafeVar, NULL);

			// wait until the callback is called (user hit Back)
			while (ThreadSafeVar == CELL_MSGDIALOG_BUTTON_INVALID)
			{
				cellSysutilCheckCallback();
				// handle shutdown if the user requested it
				if (GIsRequestingExit)
				{
					appPS3StopFlipPumperThread();
					appRequestExit(1);
				}
				appSleep(0.05);
			}

			// mark that we can't install mods, but we can still list them, if the user chose to disable
			extern UBOOL GNoInstallMods;
			GNoInstallMods = ThreadSafeVar == CELL_MSGDIALOG_BUTTON_YES;
		}
	}
	// otherwise create the lock file now to detect a crash (will be deleted on shutdown)
	else
	{
		printf("Creating lock file for crash detection...\n");
		// write a byte to the file
		CellFsErrno Ret = GScheduler->writeFileSync(NULL, LockFilename, "*", 0, 1);
		if (Ret != CELL_FS_SUCCEEDED)
		{
			printf("Failed to create the lock file, error = %d]!\n", Ret);
		}
	}
#endif

	// allow base class initialization
	FFileManagerGeneric::Init(Startup);

	// now that file sysutil stuff has started up, initialize trophies
	GPS3NetworkPlatform->InitializeTrophies();
}

/**
 * Update the FIOS stack after GSys creation, now that paths are set
 */
void FFileManagerPS3::PostGSysCreation()
{
	MainStack.PostGSysCreation();
}

/** 
 * Get the timestamp for a file
 * @param Path Path for file
 * @Timestamp Output timestamp
 * @return success code
 */
UBOOL FFileManagerPS3::GetTimestamp(const TCHAR* Path, FTimeStamp& Timestamp)
{
	// @todo ps3 streaming: fill this out
	return FALSE;
}

FArchive* FFileManagerPS3::CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
{
	INT StatsHandle = FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_READ_OPEN_STATS( StatsHandle );

	if (Flags & FILEREAD_SaveGame)
	{
		FArchiveSaveDataReaderPS3* SaveArchive = new FArchiveSaveDataReaderPS3(Error);
		if (SaveArchive->Init(Filename))
		{
			return SaveArchive;
		}
		delete SaveArchive;
		return NULL;
	}

	SHUTDOWN_IF_EXIT_REQUESTED;

	// if we are currently doing infinite retries, and we are in the rendering thread, then 
	// doing file ops will result in a deadlock with the critical sections in the SideBySideCache
	if (GNoRecovery && IsInRenderingThread())
	{
		return NULL;
	}

	// open flags
	DWORD FIOSFlags = fios::kO_RDONLY;
	
	ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformFilename);

	// see if the file exists as specified
	INT Size = FileSize(Filename);
	if (Size == -1)
	{
		// if not, try with .EDAT extension
		Size = FileSize(*(FString(Filename) + TEXT(".EDAT")));

		// if the .EDAT file exists, OR in the EDATA flag
		if (Size != -1)
		{
			FIOSFlags |= fios::kO_EDATA;
			strcat(PlatformFilename, ".EDAT");
		}
	}

	// create the wrapper if we have the file
	if (Size != -1)
	{
		PS3Fd Handle = PS3CheckedFsOpen(PlatformFilename, FIOSFlags);

		// the only way CheckedFsOpen will return NULL is if EDATA failed, and it won't throw up an error,
		// the caller determine what to do
		if (Handle)
		{
			return new FArchiveFileReaderPS3(Handle, StatsHandle, Filename, Error, Size);
		}
	}

	// if we were told NoFail, then abort
	if (Flags & FILEREAD_NoFail)
	{
		appErrorf(TEXT("File %s does not exist, but was marked as NoFail"), Filename);
	}

	return NULL;
}

FArchive* FFileManagerPS3::CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error, INT MaxFileSize )
{
	INT StatsHandle = FILE_IO_STATS_GET_HANDLE( Filename );
	SCOPED_FILE_IO_WRITE_OPEN_STATS( StatsHandle );

	if (Flags & FILEWRITE_SaveGame)
	{
		// operations are done on this at close time
		return new FArchiveSaveDataWriterPS3(Filename, (Flags & FILEWRITE_Async) != 0, Error);
	}

	// make sure we have enough HD space
	if (MaxFileSize > 0)
	{
		// get free space
		UINT SectorSize;
		QWORD UnusedSectors;
		cellFsGetFreeSize(SYS_DEV_HDD0, &SectorSize, &UnusedSectors);

		// make sure it fits, but don't eat into any reserved space
		if (MaxFileSize > (SectorSize * UnusedSectors - GReserveSpaceSlopMB * 1024 * 1024))
		{
			return NULL;
		}
	}

	// make sure the output directory exists
	MakeDirectory(*FFilename(Filename).GetPath(), TRUE);

	// create fios usable flags
	INT FsWriteFlags = 0;

	// Access Mode
	if (Flags & FILEWRITE_AllowRead)
	{
		FsWriteFlags |= cell::fios::kO_RDWR;
	}
	else
	{ 
		FsWriteFlags |= cell::fios::kO_WRONLY;
	}

	// Create mode
	if (Flags & FILEWRITE_Append)
	{
		FsWriteFlags |= cell::fios::kO_CREAT | cell::fios::kO_APPEND;
	}
	else if (Flags & FILEWRITE_NoReplaceExisting)
	{
		//FsWriteFlags |= cell::fios::kO_CREAT | cell::fios::kO_EXCL;
		appErrorf(TEXT("NoReplaceExisting not supported with FIOS."));
	}
	else
	{
		FsWriteFlags |= cell::fios::kO_CREAT | cell::fios::kO_TRUNC;
	}

	// convert the filename
	ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformFilename);

#if !FINAL_RELEASE
	// we only need this for read-only host files, anything created by the game won't be read-only (or is on BD and we can't delete anyway ;))
	if (Flags & FILEWRITE_EvenIfReadOnly)
	{
		// figure out the full pathname for cellFs function
		fios::filerange_t File;
		strcpy(File.path, PlatformFilename);
		File.offset = 0;
		File.length = FIOS_PATH_MAX;
		GScheduler->resolveSync(NULL, &File, NULL);

		// remove read-only flag if there
		CellFsErrno Ret = cellFsChmod(File.path, CELL_FS_DEFAULT_CREATE_MODE_2);
	}
#endif

	// open the file for writing
	PS3Fd Handle;
	CellFsErrno Err = GScheduler->openFileSync(NULL, PlatformFilename, FsWriteFlags, &Handle);

	// allow for failure (
	if (Err != fios::CELL_FIOS_NOERROR)
	{
		// crash if desired
		if (Flags & FILEWRITE_NoFail)
		{
			appErrorf(TEXT("  NoFail was specified, forcing a crash... %s [err = %x] Original Filename: %s"), Filename, Err, Filename);
		}
		return NULL;
	}

	// create a file writer at the location above
	return new FArchiveFileWriterPS3(Handle, StatsHandle, Filename, Error);
}

UBOOL FFileManagerPS3::Delete( const TCHAR* Filename, UBOOL RequireExists, UBOOL EvenReadOnly )
{
	// convert the filename
	ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformFilename);

	UBOOL bHadError = FALSE;

	// only require the HD file if we are writing to HD
	if (RequireExists)
	{
		bool bExists = false;
		CellFsErrno Err = GScheduler->itemExistsSync(NULL, PlatformFilename, &bExists);
		if (Err != fios::CELL_FIOS_NOERROR || !bExists)
		{
			return FALSE;
		}
	}

#if !FINAL_RELEASE
	// we only need this for read-only host files, anything created by the game won't be read-only (or is on BD and we can't delete anyway ;))
	if (EvenReadOnly)
	{
		// figure out the full pathname for cellFs function
		fios::filerange_t File;
		strcpy(File.path, PlatformFilename);
		File.offset = 0;
		File.length = FIOS_PATH_MAX;
		GScheduler->resolveSync(NULL, &File, NULL);

		// remove read-only flag if there
		CellFsErrno Ret = cellFsChmod(File.path, CELL_FS_DEFAULT_CREATE_MODE_2);
		if (Ret == CELL_FS_ENOTSUP)
		{
			debugf(TEXT("Unable to remove read-only flag on %s because the API is reported as not supported"), ANSI_TO_TCHAR(PlatformFilename));
		}
	}
#endif

	// delete the file
	CellFsErrno Err = GScheduler->unlinkSync(NULL, PlatformFilename);
	
	// allow for file to not be there
	bHadError |= (Err != fios::CELL_FIOS_NOERROR && Err != fios::CELL_FIOS_ERROR_BADPATH);

	// return success or failure
	return !bHadError;
}

UBOOL FFileManagerPS3::IsReadOnly( const TCHAR* Filename )
{
	// get the platform path
	ANSICHAR PlatformPath[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformPath);

	PS3FsStat Stat;
	// get file stat info
	GScheduler->statSync(NULL, PlatformPath, &Stat);

	// is it writable?
	return (Stat.statFlags & fios::kSTAT_WRITABLE) == 0;
}

UBOOL FFileManagerPS3::MakeDirectory( const TCHAR* Path, UBOOL Tree )
{
	if( Tree )
	{
		return FFileManagerGeneric::MakeDirectory( Path, Tree );
	}

	// never make the .. path, as GetPlatformFilename doesn't handle it well, and I don't want to slow
	// down that function with extra checks, better to slow this function with an extra check, and
	// it's never needed to be created
	if (appStrcmp(Path, TEXT("..")) == 0)
	{
		return TRUE;
	}

	// convert the filename
	ANSICHAR PlatformPath[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Path, PlatformPath);

	bool bExists = false;
	CellFsErrno Err = GScheduler->itemExistsSync(NULL, PlatformPath, &bExists);
	if ((Err == fios::CELL_FIOS_NOERROR) && bExists)
	{
		// if it's already there, don't need to make it again
		return TRUE;
	}

	// create the directory
	Err = GScheduler->createDirectorySync(NULL, PlatformPath);

	// make sure it succeeded
	return Err == fios::CELL_FIOS_NOERROR;
}

UBOOL FFileManagerPS3::DeleteDirectory( const TCHAR* Path, UBOOL RequireExists, UBOOL Tree )
{
	if (Tree)
	{
		return FFileManagerGeneric::DeleteDirectory(Path, RequireExists, Tree);
	}

	// convert the file name
	ANSICHAR PlatformPath[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Path, PlatformPath);

	// delete the directory
	return GScheduler->unlinkSync(NULL, PlatformPath) == fios::CELL_FIOS_NOERROR;
}

void FFileManagerPS3::FindFiles(TArray<FString>& Result, const TCHAR* Filename, UBOOL Files, UBOOL Directories)
{
	// determine where to look for files (TOC is critical when looking on BD)
	UBOOL bUseTableOfContents = TRUE;

	// HDD boot games always look on HD, and don't use TOC before it's ready
	if (bIsHDDBoot || !TOC.HasBeenInitialized())
	{
		bUseTableOfContents = FALSE;
	}
	// special root directories get special handling
	else if (appStrnicmp(Filename, TEXT("/PATCH"), 6) == 0 ||
		appStrnicmp(Filename, TEXT("/DLC"), 4) == 0 ||
		(GSys && appStrnicmp(Filename, *GSys->CachePath, GSys->CachePath.Len())) == 0
		)
	{
		bUseTableOfContents = FALSE;
	}

	// if we aren't forcing to look on the HD, then use the TOC to find the files
	if (bUseTableOfContents)
	{
		// skip extra ..\ if needed
		FixupExtraDots(Filename);

		TOC.FindFiles(Result, Filename, Files, Directories);
	}
	else
	{
		ANSICHAR Path[PS3_MAX_PLATFORM_FILENAME];
		GetPlatformFilename(Filename, Path);

		ANSICHAR Wildcard[1024];
		ANSICHAR* Filestart;

		// point to the file, removing all directories
		Filestart = strrchr(Path, '/');
		checkf(Filestart, TEXT("All FindFiles to the HD are expected to have a slash in them"));
		if (Filestart)
		{
			// chop off the trailing /
			*Filestart = 0;

			// wildcard starts after the /
			strcpy(Wildcard, Filestart + 1);
		}

		// cache the length of the wildcard string
		INT WildcardLen = strlen(Wildcard);
		UBOOL bIsLookingForEDAT = strcasecmp(Wildcard + WildcardLen - 5, ".EDAT") == 0;

		// Check each entry in the directory (files and directories)
		INT Index = 0;
		while (1)
		{
			// read an entry from the directory
			fios::direntry_t Dirent = FIOS_DIRENTRY_INITIALIZER;
			CellFsErrno Error = GScheduler->readDirectorySync(NULL, Path[0] ? Path : "/", Index++, &Dirent);
			// if we are at end of directory
			if (Error == fios::CELL_FIOS_ERROR_BADOFFSET)
			{
				break;
			}

			if (Error != fios::CELL_FIOS_NOERROR)
			{
				break;
			}

			UBOOL bIsDir = (Dirent.statFlags & fios::kSTAT_DIRECTORY);
			ANSICHAR* EntryName = Dirent.fullPath + Dirent.offsetToName;
			INT LastChar = Dirent.nameLength - 1;

			// by default, we haven't matched anything
			UBOOL Match = FALSE;

			// match the right types
			if (!((Files && !bIsDir) || (Directories && bIsDir)))
			{
				//debugf(TEXT("Not the right type: %d != %d, %d"), bIsDir, !Files, Directories);
				continue;
			}

			if (bIsDir && (EntryName[LastChar] == '/' || EntryName[LastChar] == '\\') )
			{
				// chop off the trailing slash
				EntryName[LastChar] = 0;
			}


			// ignore . and ..
			if (strcmp(EntryName, ".") == 0 || strcmp(EntryName, "..") == 0)
			{
				continue;
			}


			// cache the length of the filename
			INT EntryNameLen = strlen(EntryName);

			// is it an EDAT file, which we handle under the hood, so we chop off the .EDAT extension
			// (unless we are looking for an .edat file)
			// note that edat files aren't shipped on BD, so they should be in HDD boot games or DLC only,
			// which is why we can do this here outside of the TOC
			if (!bIsLookingForEDAT && strcasecmp(EntryName + EntryNameLen - 5, ".EDAT") == 0)
			{
				EntryName[EntryNameLen - 5] = 0;
			}

			// * matches any filename.
			if (strcmp(Wildcard, "*") == 0 || strcmp(Wildcard, "*.*") == 0)
			{
				Match = true;
			}
			// *extension matches files that end with everything after the *
			else if (Wildcard[0] == '*')
			{
				if (strcasecmp(Wildcard + 1, EntryName + (EntryNameLen - (WildcardLen - 1))) == 0)
				{
					Match = true;
				}
			}
			// "name.*" matches if the first part (including .) matches
			else if (Wildcard[WildcardLen - 1] == '*')
			{
				if (strncasecmp(EntryName, Wildcard, WildcardLen - 1) == 0)
				{
					Match = true;
				}
			}
			// "str.*.str case
			else if( strstr( Wildcard, "*" ) != NULL )
			{
				// single str.*.str match.
				char* Star = strstr(Wildcard, "*");
				INT FileLen = WildcardLen;
				INT StarLen = strlen(Star);
				INT StarPos = FileLen - (StarLen - 1);
				char Prefix[256];
				strncpy(Prefix, Wildcard, StarPos);
				Star++;
				// does the part before the * match?
				if (strncasecmp(EntryName, Prefix, StarPos - 1) == 0)
				{
					char* Postfix = EntryName + (EntryNameLen - StarLen) + 1;
					// does the part after the * match?
					if (strcasecmp(Postfix, Star) == 0)
					{
						Match = true;
					}
				}
			}
			// Literal filename, must match exactly
			else
			{
				if (strcasecmp(EntryName, Wildcard) == 0)
				{
					Match = true;
				}
			}

			// Does this entry match the Filename?
			if (Match)
			{
				// Yes, add the file name to Result.
				new(Result)FString(EntryName);
			}
		}

		MainStack.CleanupFindFiles();
	}
}

INT FFileManagerPS3::FileSize(const TCHAR* Filename)
{
	// determine where to look for files (TOC is critical when looking on BD)
	UBOOL bUseTableOfContents = TRUE;

	// HDD boot games always look on HD, and don't use TOC before it's ready
	if (bIsHDDBoot || !TOC.HasBeenInitialized())
	{
		bUseTableOfContents = FALSE;
	}

	// use the TOC to quickly fin filesize, without touching disk
	INT FileSize = -1;

	// don't use the TOC for hdd boot
	if (bUseTableOfContents)
	{
		// skip extra ..\ if needed
		FixupExtraDots(Filename);

		FileSize = TOC.GetFileSize(Filename);
	}

	if (FileSize == -1)
	{
		// only look on file system for non-BD files (anything on BD should be in TOC)
		fios::filerange_t FileResolver;
		FileResolver.offset = 0;
		FileResolver.length = FIOS_PATH_MAX;
		GetPlatformFilename(Filename, FileResolver.path);
		GScheduler->resolveSync(NULL, &FileResolver, NULL);

		// make sure the file won't be looked on BD (unless it's before the TOC is initialized!)
		if (!TOC.HasBeenInitialized() || strncasecmp(FileResolver.path, DRIVE_BD, strlen(DRIVE_BD)) != 0)
		{
			FileSize = InternalFileSize(Filename);
		}
	}

	// return whatever was found
	return FileSize;
}

/**
 * Looks up the size of a file by opening a handle to the file.
 *
 * @param	Filename	The path to the file.
 * @return	The size of the file or -1 if it doesn't exist.
 */
INT FFileManagerPS3::InternalFileSize(const TCHAR* Filename)
{
	INT FileSize = -1;

	// convert the filename and look on HD
	ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformFilename);

	fios::opattr_t FileSizeAttribute = FIOS_OPATTR_INITIALIZER;

	// handle getting proper filesize for edata files
	SceNpDrmKey Key;
	if (FString(Filename).EndsWith(TEXT(".EDAT")))
	{
		// geting file size with edat can be a little slow, so check to make sure it exists first,
		// and also we can check the IsInRenderingThread only if it exists
		fios::stat_t Status;
		bool bExists;
		if (GScheduler->fileExistsSync(&FileSizeAttribute, PlatformFilename, &bExists) == fios::CELL_FIOS_NOERROR && bExists)
		{
			// can't use rendering thread
			checkf(!IsInRenderingThread(), TEXT("EDAT files are not able to be read from the rendering thread because they need the sysUtilCheckCallback to be running in a different thread, and it's called from the rendering thread."));

			// get the key for EDATA
			Key = appPS3GetHDDBootDrmKey();

			// pass the key to the file open
			FileSizeAttribute.pLicense = &Key;
			FileSizeAttribute.opflags = fios::kOPF_EDATA;
		}
	}

	// get filesize
	PS3Offset Size;
	fios::err_t Err;
	if ((Err = GScheduler->getFileSizeSync(&FileSizeAttribute, PlatformFilename, &Size)) == fios::CELL_FIOS_NOERROR)
	{
		FileSize = Size;
	}

	// print out a warning that a file was looked for, but won't be performed on BD disc
	// except for files before TOC was loaded
	if (TOC.HasBeenInitialized() && !bIsHDDBoot)
	{
		appOutputDebugStringf(TEXT("Failed to find '%s' in TOC. Looked directly on filesystem, and it was %s. THIS WOULD NOT BE DONE ON BLURAY!\n"), Filename, FileSize != -1 ? TEXT("Found") : TEXT("Not Found"));
	}

	return FileSize;
}

/**
 * If the given file is compressed, this will return the size of the uncompressed file,
 * if the platform supports it.
 * @param Filename Name of the file to get information about
 * @return Uncompressed size if known, otherwise -1
 */
INT FFileManagerPS3::UncompressedFileSize( const TCHAR* Filename )
{
	// skip extra ..\ if needed
	FixupExtraDots(Filename);

	// we have no TOC with HDD boot, so look for the .uncompressed_size file
	if (bIsHDDBoot)
	{
		FString MetaFilename = FString(Filename) + TEXT(".uncompressed_size");
		// does it exist?
		if (FileSize(*MetaFilename) != -1)
		{
			FString SizeString;
			appLoadFileToString(SizeString, *MetaFilename);
			check(SizeString.Len());

			// get the uncompressed size from the file
			return appAtoi(*SizeString);
		}
		// if it doesn't exist, its not compressed
		return -1;
	}
	return TOC.GetUncompressedFileSize(Filename);
}

DOUBLE FFileManagerPS3::GetFileAgeSeconds( const TCHAR* Filename )
{
	// get current time
	CellRtcDateTime CurrentClock;
	cellRtcGetCurrentClockLocalTime(&CurrentClock);

	// get filetime (GetFileTimestamp just returns a DOUBLE, so convert back)
	time_t FileTime = (time_t)GetFileTimestamp(Filename);
	CellRtcDateTime FileClock;
	cellRtcSetTime_t(&FileClock, FileTime);

	// convert both to ticks
	CellRtcTick CurrentTick, FileTick;
	cellRtcGetTick(&CurrentClock, &CurrentTick);
	cellRtcGetTick(&FileClock, &FileTick);

	// subtract to get age in ticks
	uint64_t TickDiff = CurrentTick.tick - FileTick.tick;

	// convert to seconds (1 tick is 1 microsecond)
	return (DOUBLE)TickDiff / (1000000.0);
}

DOUBLE FFileManagerPS3::GetFileTimestamp( const TCHAR* Filename )
{
	// convert the filename
	ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
	GetPlatformFilename(Filename, PlatformFilename);

	fios::stat_t FileStat;
	if (GScheduler->statSync(NULL, PlatformFilename, &FileStat) == fios::CELL_FIOS_NOERROR)
	{
		fios::abstime_t Abstime;
		if (FIOSDateToAbstime(FileStat.modificationDate, &Abstime))
		{
			return FIOSAbstimeToSeconds(Abstime);
		}
	}

	// if all else fails, return error (file doesn't exist)
	return -1.0;
}

UBOOL FFileManagerPS3::Move( const TCHAR* Dest, const TCHAR* Src, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes )
{
	// get the resolved files and use cellFs to rename
	fios::filerange_t SourceFile, DestFile;
	SourceFile.offset = DestFile.offset = 0;
	SourceFile.length = DestFile.length = FIOS_PATH_MAX;

	// get the FIOS-happy names
	GetPlatformFilename(Src, SourceFile.path);
	GetPlatformFilename(Dest, DestFile.path);

	// get the underlying paths
	GScheduler->resolveSync(NULL, &SourceFile, NULL);
	GScheduler->resolveSync(NULL, &DestFile, NULL);

	// do the rename
	return cellFsRename(SourceFile.path, DestFile.path) == CELL_FS_SUCCEEDED;
}

UBOOL FFileManagerPS3::SetDefaultDirectory()
{
	return TRUE;
} 

UBOOL FFileManagerPS3::SetCurDirectory(const TCHAR* Directory)
{
	return TRUE;
} 

FString FFileManagerPS3::GetCurrentDirectory()
{
	return FString("");
}

void FFileManagerPS3::FlushFileCache()
{
	// Only called via the text command FLUSHFILECACHE, flush all of the FIOS cache.
	MainStack.FlushCache();
}

UBOOL FFileManagerPS3::IsDrive( const TCHAR* Path )
{
	// The "drive" path is set when creating the FIOS media, and a file path
	// can never point to the actual media (for example, if you mount a drive
	// *below* /media, and all references were relative to /media, you could
	// never directly reference the underlying / nor /media).
	return FALSE;
}


/**
 * Updates the modification time of the file on disk to right now, just like the unix touch command
 * @param Filename Path to the file to touch
 * @return TRUE if successful
 */
UBOOL FFileManagerPS3::TouchFile(const TCHAR* Filename)
{
	// get current time
	CellRtcDateTime Now;
	cellRtcGetCurrentClockLocalTime(&Now);

	// convert to time_t
	CellFsUtimbuf ModTime;
	cellRtcGetTime_t(&Now, &ModTime.modtime);
	ModTime.actime = ModTime.modtime;

	// set modtime on the file
	fios::filerange_t File;
	File.offset = 0;
	File.length = FIOS_PATH_MAX;
	GetPlatformFilename(Filename, File.path);
	cellFsUtime(File.path, &ModTime);

	return TRUE;
}

/**
 *	Threadsafely converts the platform-independent Unreal filename into platform-specific full path.
 *
 * NOTE: This returns the underlying cellFs-usable path, NOT fios
 *
 *	@param Filename		Platform-independent Unreal filename
 *	@return				Platform-dependent full filepath
 **/
FString FFileManagerPS3::GetPlatformFilepath( const TCHAR* Filename )
{
	// get the cellFs usable filepath
	fios::filerange_t File;
	File.offset = 0;
	File.length = FIOS_PATH_MAX;
	GetPlatformFilename(Filename, File.path);

	// resolve to underlying
	GScheduler->resolveSync(NULL, &File, NULL);
	return FString(File.path);
}

void ImportCallback(INT Result, void* Userdata)
{
	FModImportData* ModImportData = (FModImportData*)Userdata;

#if !FINAL_RELEASE
	if (ParseParam(appCmdLine(), TEXT("ps3errorimport")))
	{
		Result = CELL_STORAGEDATA_ERROR_ACCESS_ERROR;
	}
#endif

	// queue up a data extraction on success
	if (Result == CELL_STORAGEDATA_RET_OK)
	{
		// tell the main thread our new stage
		appInterlockedExchange(&ModImportData->ModImportProgress, (INT)MIS_ReadyToUnpack);
	}
	else if (Result == CELL_STORAGEDATA_RET_CANCEL)
	{
		// tell the main thread our new stage
		appInterlockedExchange(&ModImportData->ModImportProgress, (INT)MIS_Canceled);
	}
	else
	{
		// tell the main thread our new stage
		appInterlockedExchange(&ModImportData->ModImportProgress, (INT)MIS_Failed);
	}
}


struct FJamFileInfo
{
	FString Name;
	INT Size;
};

void UnpackModThreadProc(uint64_t Param)
{
	// Set the TLS thread ID so that debugfs can work :)
	appTryInitializeTls();

	INT ThreadingResult = MIS_Succeeded;

	FModImportData* ModImportData = (FModImportData*)(PTRINT)Param;
	FString ModName(TEXT(""));

	const FString ImportPath = TEXT("/DLC/IMPORT.BIN");
	FArchive* JamFile = GFileManager->CreateFileReader( *ImportPath );
	if (!JamFile)
	{
		debugf(TEXT("Failed to open Jam file!"));
		ThreadingResult = MIS_Failed;
	}
	else
	{
		// get free space
		UINT SectorSize;
		QWORD UnusedSectors;
		cellFsGetFreeSize(SYS_DEV_HDD0, &SectorSize, &UnusedSectors);
		
		// make sure it fits, but don't eat into any reserved space
		const UBOOL bHDSpaceAvailable = JamFile->TotalSize() < (SectorSize * UnusedSectors - GReserveSpaceSlopMB * 1024 * 1024);
		if ( !bHDSpaceAvailable )
		{
			debugf(TEXT("Failed to unpack Jam file: no HD space available!"));
			ThreadingResult = MIS_Failed;
			goto ExitThread;
		}

		// read offset to file table
		INT TableOffset;
		(*JamFile) << TableOffset;
		debugf(TEXT("Table offset = %d"), TableOffset);

		TArray<FJamFileInfo> JamFiles;
		ANSICHAR Name[MAX_FILEPATH_LENGTH];

		// go to file table and read it in
		JamFile->Seek(TableOffset);

		INT NumFiles;
		(*JamFile) << NumFiles;

		// read length of name
		INT StringLen;
		(*JamFile) << StringLen;

		// read name (jam maker should have already checked name length)
		JamFile->Serialize(Name, Min(MAX_FILEPATH_LENGTH - 1, StringLen));
		Name[StringLen] = 0;

		ModName = FString(Name);

		// Delete any existing mod content.
		if ( GPlatformDownloadableContent )
		{
			GPlatformDownloadableContent->DeleteDownloadableContent( *ModName );
		}

		// Put the DLC files where the DLC finding code looks for it
		// Make the directory now that we have a mod name.
		const FString DLCDir = FString(TEXT("/DLC/")) + ModName;
		if ( NumFiles > 0 )
		{
			GFileManager->MakeDirectory(*DLCDir, TRUE);
		}


		// get each file info
		for (INT FileIndex = 0; FileIndex < NumFiles; FileIndex++)
		{
			FJamFileInfo* Info = new(JamFiles) FJamFileInfo;

			// read file size
			(*JamFile) << Info->Size;

			// read length of name
			(*JamFile) << StringLen;

			// read name (jam maker should have already checked file name length)
			JamFile->Serialize(Name, Min(MAX_FILEPATH_LENGTH - 1, StringLen));
			Name[StringLen] = 0;

			Info->Name = FString(Name);
		}


		// cache the guid caches
		TArray<UGuidCache*> CachedGuidCaches;
		for (TObjectIterator<UGuidCache> It; It; ++It)
		{
			// don't look in a GuidCache we are about to overwite
			if (It->GetOuter()->GetName() != FString(TEXT("GuidCache_")) + ModName)
			{
				CachedGuidCaches.AddItem(*It);
			}
		}

		for (INT FileIndex = 0; FileIndex < NumFiles && ThreadingResult == MIS_Succeeded; FileIndex++)
		{
			FJamFileInfo& Info = JamFiles(FileIndex);

			FFilename ModFilename(Info.Name);
			// look for ambiguous packages
			if (ModFilename.GetExtension() == TEXT("xxx"))
			{
				UBOOL bWasPackageFound = FALSE;
				FString PackageName = ModFilename.GetBaseFilename();
				for (INT CacheIndex = 0; CacheIndex < CachedGuidCaches.Num() && !bWasPackageFound; CacheIndex++)
				{
					UGuidCache* Cache = CachedGuidCaches(CacheIndex);
debugf(TEXT("Checking %s for %s"), *Cache->GetFullName(), *PackageName);
					FGuid Guid;
					// look for this package in it
					if (Cache->GetPackageGuid(FName(*PackageName), Guid))
					{
						bWasPackageFound = true;
					}
				}

				if (bWasPackageFound)
				{
					ThreadingResult = MIS_PackageName;
					goto ExitThread;
				}
			}

		}


		// go back to start, reading ecah file, skippnig over table offset
		JamFile->Seek(4);

		// buffer for file copying
		BYTE* Buffer = (BYTE*)appMalloc(1024 * 1024);
		for (INT FileIndex = 0; FileIndex < NumFiles && ThreadingResult == MIS_Succeeded; FileIndex++)
		{
			FJamFileInfo& Info = JamFiles(FileIndex);

#if !FINAL_RELEASE
			if (ParseParam(appCmdLine(), TEXT("ps3errorunpack")))
			{
				debugf(TEXT("[FORCED] Failed to write file [%s] for unpacking"), *Info.Name);
				ThreadingResult = MIS_Failed;
				break;
			}
#endif

			// put the DLC files where the DLC finding code looks for it
			debugf(TEXT("Extracting %s"), *(DLCDir * Info.Name));
			FArchive* OutputFile = GFileManager->CreateFileWriter(*(DLCDir * Info.Name), 0, GNull, Info.Size);
			if (!OutputFile)
			{
				debugf(TEXT("Failed to open file or reserve space [%s] for unpacking"), *Info.Name);
				ThreadingResult = MIS_Failed;
			}
			else
			{
				// copy the file out of the jam file to it's final location
				INT AmountLeftToCopy = Info.Size;

				// copy in chunks until done
				while (AmountLeftToCopy > 0)
				{
					INT NumBytes = Min(1024 * 1024, AmountLeftToCopy);

					// copy data
					JamFile->Serialize(Buffer, NumBytes);
					OutputFile->Serialize(Buffer, NumBytes);

					// update how much we have left
					AmountLeftToCopy -= NumBytes;
				}

				// look for errors while writing
				if (OutputFile->Close() == FALSE)
				{
					debugf(TEXT("Failed to write file [%s] for unpacking"), *Info.Name);
					ThreadingResult = MIS_Failed;
				}

				// close output file
				delete OutputFile;
			}
		}

		// free temp memory
		appFree(Buffer);
	}

ExitThread:
	appInterlockedExchange(&ModImportData->ModImportProgress, ThreadingResult);

	// close jam file
	delete JamFile;
	GFileManager->Delete( *ImportPath );


	// if we errored, delete the directory
	if ( GPlatformDownloadableContent && (ThreadingResult == MIS_Failed || ThreadingResult == MIS_PackageName) && ModName != TEXT("") )
	{
		GPlatformDownloadableContent->DeleteDownloadableContent( *ModName );
	}

#if !FINAL_RELEASE
	// let the FIOS test clean up
	extern FString GFIOSTestModName;
	GFIOSTestModName = ModName;
#endif

	// exit the thread
	sys_ppu_thread_exit(0);
}

/**
 * Start to (asynchronously) import a mod from a memory device
 *
 * @return TRUE if successful
 */
UBOOL FFileManagerPS3::BeginImportModFromMedia()
{
	// Path for cellStorageDataImport, resolved to a cellFs name
	fios::filerange_t ResolvedFile;
	strcpy(ResolvedFile.path, "/DLC");
	ResolvedFile.offset = 0;
	ResolvedFile.length = FIOS_PATH_MAX;
	GScheduler->resolveSync(NULL, &ResolvedFile, NULL);

	// set params 
	CellStorageDataSetParam Params;
	Params.fileSizeMax = 1024 * 1024 * 1024; // 1GB is the max allowed
	ANSICHAR Title[CELL_STORAGEDATA_TITLE_MAX];
	strncpy(Title, TCHAR_TO_UTF8(*LocalizeGeneral(TEXT("ImportModCaption"), TEXT("PS3"))), CELL_STORAGEDATA_TITLE_MAX - 1);
	Params.title = Title;
	Params.reserved = NULL;

	// set current status
	ModImportData.ModImportProgress = MIS_Importing;

	// kick off the import
#define MediaFile "USERDATA.JAM"

	INT Ret = cellStorageDataImport(CELL_STORAGEDATA_VERSION_CURRENT, MediaFile, ResolvedFile.path, &Params, ImportCallback, SYS_MEMORY_CONTAINER_ID_INVALID, (void*)&ModImportData);

	if (Ret != CELL_STORAGEDATA_RET_OK)
	{
		ModImportData.ModImportProgress = MIS_Inactive;
	}

	return Ret = CELL_STORAGEDATA_RET_OK;
}


/**
 * Tick the mod importing, and return the status
 */
EModImportStage FFileManagerPS3::TickImportMod()
{
	// handle the state transition cases that need to be ticked
	if (ModImportData.ModImportProgress == MIS_ReadyToUnpack)
	{
		// make sure old DLC packages (guid cahce) are removed
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, TRUE );

		// go to next stage
		ModImportData.ModImportProgress = MIS_Unpacking;

		// start thread for background install
		sys_ppu_thread_t Thread;
		INT Err = sys_ppu_thread_create(&Thread, UnpackModThreadProc, (uint64_t)(PTRINT)&ModImportData, 3000, 8 * 1024, 0, "Install");
		checkf(Err == CELL_OK, TEXT("Failed to create import thread, ret = %d"), Err);
	}

	// store what we want to return, in case it changes below
	EModImportStage ReturnValue = (EModImportStage)ModImportData.ModImportProgress;
	
	// only return success or failed once, then return to inactive state
	if (ReturnValue == MIS_Succeeded || ReturnValue == MIS_Failed || ReturnValue == MIS_Canceled)
	{
		ModImportData.ModImportProgress = MIS_Inactive;
	}

	// return what we're up to
	return ReturnValue;
}


void DeleteFromListThreadProc(uint64_t Param)
{
	appTryInitializeTls();

	debugf(TEXT("Deleting from list"));

	debugf(TEXT("Showing list..."));
	// do the load thing (no useful return values here)
	cellSaveDataDelete2(SYS_MEMORY_CONTAINER_ID_INVALID);

	debugf(TEXT("Done deleting!"));

	// increment the value that is being waited on
	((FThreadSafeCounter*)(PTRINT)Param)->Increment();

	// bye!
	sys_ppu_thread_exit(0);
}


/**
 * Allow user to delete saves from a list so that a save will succeed
 */
void FFileManagerPS3::BeginDeleteFromList()
{
	// create a counter to track completion
	DeleteFromListCompleteCounter = new FThreadSafeCounter;

	// create a thread to start the deletion while game still rendering
	sys_ppu_thread_t Thread;
	INT Err = sys_ppu_thread_create(&Thread, DeleteFromListThreadProc, (uint64_t)(PTRINT)DeleteFromListCompleteCounter, 1000, 8 * 1024, 0, "DeleteFromList");
	checkf(Err == CELL_OK, TEXT("Failed to create delete from list thread thread, ret = %d"), Err);
}

/**
 * Ticks the delete from list util
 *
 * @return TRUE when the utility is complete and game can continue
 */
UBOOL FFileManagerPS3::TickDeleteFromList()
{
	// if no counter, then we aren't deleting
	if (DeleteFromListCompleteCounter == NULL)
	{
		return FALSE;
	}

	// if the value was incremented, then we are finished
	if (DeleteFromListCompleteCounter->GetValue() > 0)
	{
		delete DeleteFromListCompleteCounter;
		DeleteFromListCompleteCounter = NULL;
		return TRUE;
	}

	// otherwise, we are still deleting
	return FALSE;
}


#if !FINAL_RELEASE

/**
 * Return true if there is a forced (injected) error
 */
UBOOL CheckForErrorInjection(INT DipSwitchIndex)
{
	UBOOL bHadError = FALSE;
	// if we are looking for possible errors based on a given switch
	if (GHandleErrorInjection)
	{
		QWORD Value;
		sys_gpio_get(SYS_GPIO_DIP_SWITCH_DEVICE_ID, &Value);
		if (Value & (1 << DipSwitchIndex))
		{
			bHadError = TRUE;
		}
	}

	return bHadError;
}
#endif


/**
 * If there was an error, show the generic file read error dialog, and wait for a user quit
 *
 * @param Path Path to the file
 * @param Error Error code
 * @param DipSwitchIndex If allowing for error injection, which type is it?
 */
void ProcessFileError(const ANSICHAR* Path, INT Error, INT DipSwitchIndex)
{
	// block on failure with message
#if !FINAL_RELEASE
	if (CheckForErrorInjection(DipSwitchIndex))
	{
		Error = fios::CELL_FIOS_ERROR_BADPARAM;
	}
#endif
	
	// handle an error
	if (Error != fios::CELL_FIOS_NOERROR)
	{
		appHandleIOFailure(ANSI_TO_TCHAR(Path));
	}
}

/**
 * Wrapper around open that will perform infinite retries on read errors
 *
 * @param Name Name of file
 * @param Flags Open flags
 */
PS3Fd PS3CheckedFsOpen(const ANSICHAR* Name, INT Flags)
{
	FScopeLock ScopeLock(&GCheckedFSCriticalSection);

	PS3Fd FileDescriptor = NULL;
	INT Result = fios::CELL_FIOS_NOERROR;

	// try to open from source
	if (!GNoRecovery)
	{
		fios::opattr_t OpenAttribute = FIOS_OPATTR_INITIALIZER;

		// use the NPDRM key if the file is EDATA
		if (Flags & fios::kO_EDATA)
		{
			// get the key for EDATA
			SceNpDrmKey Key = appPS3GetHDDBootDrmKey();

			// pass the key to the file open
			OpenAttribute.pLicense = &Key;
		}

		Result = GScheduler->openFileSync(&OpenAttribute, Name, Flags, &FileDescriptor);

		// EDATA is a special case here, if the file fails to open, return NULL, because
		// it could be bad DRM, and the ProcessFileError won't be useful
		if (Result != fios::CELL_FIOS_NOERROR && (Flags & fios::kO_EDATA))
		{
			return NULL;
		}
	}

	// handle any errors
	ProcessFileError(Name, Result, 0);

	// return it when we've opened it
	return FileDescriptor;
}

/**
 * Wrapper around read that will perform infinite retries on read errors
 *
 * @param FileDescriptor File handle
 * @param Buffer Output buffer to receive the read data
 * @param NumberOfBytes Number of bytes to read
 * @param BytesRead Output value to receive the number of bytes that were read
 */
void PS3CheckedFsRead(PS3Fd FileDescriptor, void* Buffer, PS3Offset NumberOfBytes, PS3Offset* BytesRead)
{
	FScopeLock ScopeLock(&GCheckedFSCriticalSection);

	CellFsErrno Result = fios::CELL_FIOS_NOERROR;

	// do the actual read
	if (!GNoRecovery)
	{
		Result = GScheduler->readFile(NULL, FileDescriptor, Buffer, NumberOfBytes)->syncWait(BytesRead);

		// we allow reading past the end, since we return BytesRead, it's fine
		if (Result == fios::CELL_FIOS_ERROR_EOF)
		{
			Result = fios::CELL_FIOS_NOERROR;
		}
	}

	// handle any errors
	ProcessFileError(FileDescriptor->getPath(), Result, 1);
}

/**
 * Wrapper around cellFsStat that will perform infinite retries on read errors
 *
 * @param Path Path to file
 * @param Stat [out] Stat struct of file
 */
void PS3CheckedFsStat(const ANSICHAR* Path, PS3FsStat *Stat)
{
	FScopeLock ScopeLock(&GCheckedFSCriticalSection);

	CellFsErrno Result = fios::CELL_FIOS_NOERROR;

	// try to stat from source until it works
	if (!GNoRecovery)
	{
		Result = GScheduler->statSync( NULL, Path, Stat );
	}

	// handle any errors
	ProcessFileError(Path, Result, 2);
}


/**
 * Wrapper around seek that will perform infinite retries on read errors
 *
 * @param FileDescriptor File handle
 * @param Offset How far to seek, depends on Type
 * @param Type Type of seek (SET, CUR, END)
 * @param CurrentPos [out] Current file position
 */
void PS3CheckedFsLseek(PS3Fd FileDescriptor, PS3Offset Offset, cell::fios::e_WHENCE Type, PS3Offset *CurrentPos)
{
	FScopeLock ScopeLock(&GCheckedFSCriticalSection);

	CellFsErrno Result = fios::CELL_FIOS_NOERROR;

	// try to seek from source until it works
	if (!GNoRecovery)
	{
		Result = GScheduler->seekFileSync(NULL, FileDescriptor, Offset, Type, CurrentPos);
	}

	// handle any errors
	ProcessFileError(FileDescriptor->getPath(), Result, 3);
}



// make sure we see the appHandleIOFailure() printout even in Shipping
#undef printf

/**
 * Handles IO failure by ending gameplay.
 *
 * @param Filename	If not NULL, name of the file the I/O error occured with
 */
void appHandleIOFailure(const TCHAR* Filename)
{
	// don't do this if we;ve already started shutting down
	if (GIsRequestingExit)
	{
		return;
	}

	// disallow other file IOs
	GNoRecovery = TRUE;

	// which type of error to show
	UBOOL bShowBDError = FALSE;

	// figure out where it came from
	fios::filerange_t File;
	GetPlatformFilename(Filename, File.path);
	File.offset = 0;
	File.length = FIOS_PATH_MAX;
	GScheduler->resolveSync(NULL, &File, NULL);

	printf("IO Failure detected with file %s, callstack is:\n", File.path);
	PS3Callstack();

	if (strncmp(File.path, SYS_DEV_BDVD, strlen(SYS_DEV_BDVD)) == 0 ||
		strncmp(File.path, SYS_APP_HOME, strlen(SYS_APP_HOME)) == 0
		)
	{
		bShowBDError = TRUE;
	}

	// if we haven't start a rendering thread yet, use the manual flip pumper
	extern UBOOL GUseThreadedRendering;
	if (!GUseThreadedRendering)
	{
		appPS3StartFlipPumperThread();
	}
	// suspend rendering so we can put up error dialog
	else if (IsInRenderingThread())
	{
		RHISuspendRendering();
	}
	else
	{
		extern UBOOL GGameThreadWantsToSuspendRendering;
		GGameThreadWantsToSuspendRendering = TRUE;
	}

	if (bShowBDError)
	{
		cellOskDialogAbort();
		cellMsgDialogAbort();

		INT Ret = cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_NONE | CELL_MSGDIALOG_TYPE_DISABLE_CANCEL_ON,
			TCHAR_TO_UTF8(appPS3GetEarlyLocalizedString(ELST_BDEjected)), NULL, NULL, NULL);
	}
	else
	{
		// kill the game with a game data message (if from HDD, the game data could be corrupt)
		cellGameContentErrorDialog(FFileManagerPS3::IsHDDBoot() ? CELL_GAME_ERRDIALOG_BROKEN_EXIT_HDDGAME : CELL_GAME_ERRDIALOG_BROKEN_EXIT_GAMEDATA, 0, NULL);
	}

	// sleep until user tries to quit
	while (!GIsRequestingExit)
	{
		cellSysutilCheckCallback();
		appSleep(0.1f);

		if (IsInRenderingThread())
		{
			extern void PumpFlipPumper(FLOAT DeltaTime);
			PumpFlipPumper(0.1f);
		}

		extern DOUBLE GLastSwapTime;
		// start flipping if something is stuck
		if (appSeconds() - GLastSwapTime > 0.5f)
		{
			extern UBOOL GGameThreadWantsToSuspendRendering;
			GGameThreadWantsToSuspendRendering = TRUE;
		}
	}

	// give a little time to other threads
	appSleep(0.01);

	// and shutdown
	if (!GUseThreadedRendering)
	{
		appPS3StopFlipPumperThread();
	}
	else
	{
		if (IsInRenderingThread())
		{
			RHIResumeRendering();
		}
	}
	appRequestExit(TRUE);
}

void appPS3PatchGuidCache()
{
	// find the one that was just laoded (there will be no others yet, DLC won't be installed yet)
	UGuidCache* GlobalGuidCache = NULL;
	for (TObjectIterator<UGuidCache> It; It; ++It)
	{
		GlobalGuidCache = *It;
	}
	check(GlobalGuidCache);

	FString Filename;
	if (GPackageFileCache->FindPackageFile(TEXT("PatchGuidCache"), NULL, Filename))
	{
		// load the patch guid pacakge
		UPackage* Package = UObject::LoadPackage(NULL, *Filename, LOAD_NoWarn);
		UGuidCache* PatchGuidCache = FindObject<UGuidCache>(Package, TEXT("GuidCache"));

		// just update the global with everything in the patch
		for (TMap<FName, FGuid>::TIterator It(PatchGuidCache->PackageGuidMap); It; ++It)
		{
			GlobalGuidCache->SetPackageGuid(It.Key(), It.Value());
//			printf("Updating GlobalGuidCache for %ls to %ls\n", *It.Key().ToString(), *It.Value().String());
		}
	}
}

/**
 * @return the SceNpDrmKey for the installed HDD Game package
 */
SceNpDrmKey appPS3GetHDDBootDrmKey()
{
	SceNpDrmKey Key = { PS3_DRM_KEY };
	return Key;
}

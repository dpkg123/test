/*=============================================================================
	FIOSStack.cpp
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sysutil/sysutil_syscache.h>

#include "Engine.h"
#include "FFileManagerPS3.h"



/** Whether or not to support .psarc files */
#define USE_PSARC_FILES 1

/** Whether or not to support runtime-downloaded packages from servers(adds some overhead to file stat/open) */
#define SUPPORT_DOWNLOAD_CACHE 1

/** Whether or not to support importing mods into the DLC directory */
#define SUPPORT_MOD_IMPORT 1

// Everything in the FIOS API is in the "cell" namespace:
using namespace cell;

/**
 * FIOS Global scheduler, this is used for all normal IO operations:
 */
fios::scheduler* GScheduler = NULL;

/**
 * Allocator interface / class used by FIOS
 */
extern "C" void* __real_malloc(size_t);
extern "C" void __real_free(void*);
class FFIOSAllocatorWrapper : public cell::fios::allocator
{
public:
	void* Allocate(cell::fios::MemSize Size, cell::fios::MemFlags Flags, const char* /*File*/, int /*Line*/)
	{
		// when using the MallocProfiler, calling appMalloc from the FIOS threads
		// will cause a deadlock - MallocProfiler locks a CriticalSection, opens a file,
		// which makes FIOS allocate memory, which then would call MallocProfiler code,
		// which then locks on the CriticalSection
#if USE_MALLOC_PROFILER
		return __real_malloc(Size);
#else
		return appMalloc(Size, FIOS_ALIGNMENT_FROM_MEMFLAGS(Flags));
#endif
	}
	void Deallocate(void* Memory, cell::fios::MemFlags /*Flags*/, const char* /*File*/, int /*Line*/) 
	{
#if USE_MALLOC_PROFILER
		__real_free(Memory);
#else
		appFree(Memory);
#endif
	}
	void* Reallocate(void* /*Memory*/, uint32_t /*NewSize*/, uint32_t /*Flags*/, const char* /*File*/, int /*Line*/) 
	{
 		/* fios does not use Reallocate */
		check(0);
		return NULL;
	}
};


/**
 * Perform one time initialization 
 */
void FFIOSStack::StaticInit()
{
	// Initialize FIOS. All parameters except the allocator are optional;
	// The values set here are the same as the defaults.
	fios::fios_parameters Params = FIOS_PARAMETERS_INITIALIZER;
	Params.pAllocator = new FFIOSAllocatorWrapper;				// used for all memory allocations
	Params.pLargeMemcpy = memcpy;								// used when memcpy'ing 4KiB or larger amounts
	Params.pVprintf = vprintf;									// used for all TTY output
	Params.threadPriority[fios::platform::kComputeThread] = FIOS_COMPUTETHREAD_PRIORITY;
	Params.threadPriority[fios::platform::kIOThread] = FIOS_IOTHREAD_PRIORITY;
	Params.threadPriority[fios::platform::kSchedulerThread] = FIOS_SCHEDULERTHREAD_PRIORITY;

	// FIOS profiling - lots of output, a few lines for *each* IO request:
	// params.profiling = fios::kProfileAll;
	fios::FIOSInit(&Params);
}


/**
 * Constructor
 */
FFIOSStack::FFIOSStack()
	: MediaBluRay(NULL)
	, MediaHost(NULL)
	, MediaHDD0(NULL)
	, MediaHDD0Content(NULL)
	, MediaHDD1(NULL)
	, OverlayMiscFiles(NULL)
	, OverlayDLCFiles(NULL)
	, CacheHDD1(NULL)
	, SchedulerHDD1(NULL)
	, SchedulerMain(NULL)
	, RAMCache(NULL)
	, Dearchiver(NULL)
	, DecompressorZlib(NULL)
	, DummyID(fios::overlay::kINVALID_ID)
{

}


/**
 * Create a FIOS stack that can read from source, and write to the temp game data directories
 */
void FFIOSStack::CreateGameDataInitializationStack(const ANSICHAR* TempContentDir, const ANSICHAR* TempDataDir)
{
	// Track the most recently created entry in the stack
	fios::media *TopMostMedia;

	// the source media
	if (FFileManagerPS3::ShouldLoadFromBD())
	{
		MediaBluRay = new fios::ps3media(DRIVE_BD);
		TopMostMedia = MediaBluRay;
	}
	else
	{
		MediaHost = new fios::ps3media(DRIVE_APP);
		TopMostMedia = MediaHost;
	}

	// mount points for content and data, to write to
	MediaHDD0 = new fios::ps3media(TempDataDir);
	MediaHDD0Content = new fios::ps3media(TempContentDir);

	// add an overlay to direct files
	OverlayMiscFiles = new fios::overlay(TopMostMedia);
	TopMostMedia = OverlayMiscFiles;
	
	// redirect Content files to HDD0Content
	CellFsErrno Err = OverlayMiscFiles->addLayer("/CONTENT", NULL, MediaHDD0Content, "/", cell::fios::overlay::kOPAQUE, &DummyID);
	check(Err == fios::CELL_FIOS_NOERROR);

	// redirect data files to HDD0, in the _default_ GBaseDirectory (only uses the default since 
	// the REQUIRED_FILE is only copied once to GameData, no matter what the BaseDirectory is)
	FString FixedBaseDir = FString(TEXT("FIOS-")) + GBaseDirectory.ToUpper();
	OverlayMiscFiles->addLayer("/INSTALL", NULL, MediaHDD0, TCHAR_TO_ANSI(*FixedBaseDir), cell::fios::overlay::kOPAQUE, &DummyID);
	check(Err == fios::CELL_FIOS_NOERROR);

	// create the main scheduler - we can use this to write to the GameData partition before we cellGameContentPermit it
	fios::scheduler_attr Attr = FIOS_SCHEDULER_ATTR_INITIALIZER;
	GScheduler = fios::scheduler::createSchedulerForMedia(TopMostMedia, &Attr);
}

/**
 * Create a FIOS stack that can read from source, cache to HDD1, write special files to HOSTFS, write other files to HDD0
 */
void FFIOSStack::CreateStandardStack(const ANSICHAR* DataDir, const ANSICHAR* TitleID)
{

	// For HDD boot games, create a media stack that looks like this:
	//
	//                     main scheduler
	//                           |
	//                       dearchiver
	//                           |
	//                   HOSTFS/HDD0 overlay
	//                      |         |
	//                   HOSTFS      HDD0
	//

	// For HOSTFS and BD boot games, create a media stack that looks like this:
	//
	//                     main scheduler
	//                           |
	//                     DLC overlay -------  opaque overlay DLC to Gamedir/DLC
	//                           |            default overlay PATCH to Gamedir/Patch
	//                           |            default overlay INSTALL to Gamedir/<BaseDirectory>
	//                       dearchiver
	//                           |
	//                    debug overlay -------  send all logs/stats  to Hostfs or Gamedir
	//                           |
	//                   2GB schedulercache-----------+
	//                           |                    |
	//                   HOSTFS/BD overlay      HDD1 scheduler
	//                      |         |               |
	//                   HOSTFS     BluRay           HDD1
	//

	fios::media* TopMostMedia = NULL;

	// set up some loading flags
	UBOOL bLoadFromHostFS = ShouldLoadFromHostFS();
	UBOOL bIsHDDBoot = FFileManagerPS3::IsHDDBoot();
	UBOOL bShouldLoadFromBD = FFileManagerPS3::ShouldLoadFromBD();
	
#if FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE
	// Default to writing to console, but allow option to override
	bWriteToHostFS = FALSE;
	if( ParseParam(appCmdLine(), TEXT("writetohost")) )
	{
		bWriteToHostFS = TRUE;
	}
#else
	// Default to writing to host, but allow option to override.
	bWriteToHostFS = TRUE;
	if( ParseParam(appCmdLine(), TEXT("writetoconsole")) )
	{
		bWriteToHostFS = FALSE;
	}
#endif

	// Create the media layers at the bottom of the stack.
	// We need media layers for HDD0, HDD1, Blu-Ray, and HOSTFS.
	MediaHDD0 = new fios::ps3media(DataDir);
	if (!bIsHDDBoot)
	{
		// mount the system cache partition (HDD1)
		CellSysCacheParam CacheParams;
		if (ParseParam(appCmdLine(), TEXT("clearcache")))
		{
			strcpy(CacheParams.cacheId, "");
		}
		else
		{
			strcpy(CacheParams.cacheId, TitleID);
		}
		CacheParams.reserved = NULL;

		INT Error = cellSysCacheMount(&CacheParams);
		if (Error != CELL_SYSCACHE_RET_OK_CLEARED && Error != CELL_SYSCACHE_RET_OK_RELAYED)
		{
			// @todo ship: Handle error, including user error (can use the dialog sysutil to communicate to user)
			printf("System cache failed to mount. THis needs to be handled for ship\n");
		}

		MediaHDD1 = new fios::ps3media(CacheParams.getCachePath);
		MediaHDD0Content = new fios::ps3media(DataDir);
		MediaBluRay = new fios::ps3media(DRIVE_BD);
	}

	// create the hostfs media if we want to use it
	if (bLoadFromHostFS || bWriteToHostFS)
	{
		MediaHost = new fios::ps3media(DRIVE_APP);
	}

	// set up the media to read our source files from
	if (bIsHDDBoot)
	{
		TopMostMedia = MediaHDD0;
	}
	else if (bShouldLoadFromBD)
	{
		TopMostMedia = MediaBluRay;
	}
	else if (bLoadFromHostFS)
	{
		TopMostMedia = MediaHost;
	}
	else
	{
		appErrorf(TEXT("Failed to determine what to load from - none of bIsHDDBoot, bShouldLoadFromBD, or bLoadFromHostFS is set!"));
	}

	// storage to hold underlying directory names so we have the directories we mount to
	ANSICHAR UnderlyingDirectory[PS3_MAX_PLATFORM_FILENAME];

	// handle caching to HDD1
	if (!bIsHDDBoot)
	{
		// Create a scheduler for the sys cache on HDD1. Since we this is used
		// lightly, with most accesses going through the main scheduler, we
		// use smaller values than the defaults for the size of their op and
		// filehandle pools.
		fios::scheduler_attr HDDSchedulerAttr = FIOS_SCHEDULER_ATTR_INITIALIZER;
		HDDSchedulerAttr.numOps = 16;     // size of preallocated op pool
		HDDSchedulerAttr.numFiles = 4;    // size of preallocated filehandle pool
		SchedulerHDD1 = fios::scheduler::createSchedulerForMedia(MediaHDD1, &HDDSchedulerAttr);

		// Create a schedulercache that fills nearly all of HDD1:
		// 8173 cache blocks, each 256KiB in size. The odd number of blocks
		// leaves space for file system overhead.
		// If we aren't using any PSARC files, a smaller block size should be
		// used. You can also try:
		// 	8173 x 256KiB
		// 	4090 x 512KiB
		// 	2046 x 1MiB
		//
		// 	For developement, the cache *could* be created on HDD0, so that
		// 	crashes won't clear the cache (FIOS cache operations should keep
		// 	the cache sane), and then a much larger cache size could also be
		// 	used.
		CacheHDD1 = new fios::schedulercache(TopMostMedia,
			SchedulerHDD1,		// FasterScheduler - the cache's files location
			"/fios",			// CacheDirectory - subdirectory in which to store the cache files
			1,					// discID - arbitrary identifier, if this does not match the current FIOS cache, the cache is cleared
			TRUE,				// singleFile - data uses a single large file (true = faster)

// always check modification date when not using FINAL_RELEASE. Final release BD builds will need to format the HD between runs of 
// different builds (ie, QA process should include wiping HD when doing Bluray testing)
#if FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE
			!bShouldLoadFromBD,	// checkModification - check mod dates on open? set to false when only using Blu-Ray, true for typical HOSTFS use
#else
			TRUE,
#endif

			8173,				// numBlocks = number of blocks in the cache
			256*1024);			// blockSize = cache block size
		TopMostMedia = CacheHDD1;
	}

	// direct where debug files go (always needed for non-HDD boot games, or HDD boot games
	// that still want to write to HOSTFS)
	if (!bIsHDDBoot || bWriteToHostFS)
	{
		// where do we want to write debug files to?
		fios::ps3media* WriteTarget = bWriteToHostFS ? MediaHost : MediaHDD0;

		// creeate an overlay for debug files
		OverlayMiscFiles = new fios::overlay(TopMostMedia);
		TopMostMedia = OverlayMiscFiles;

		// Since these are opaque overlays, all files under the specified
		// directory will use the target media, and there is only a string
		// comparison needed (for each opaque layer).
		ANSICHAR WriteDir[PS3_MAX_PLATFORM_FILENAME];

		GetPlatformFilename(*appGameLogDir(), WriteDir);
		CellFsErrno Err = OverlayMiscFiles->addLayer(WriteDir, NULL, WriteTarget, WriteDir, fios::overlay::kOPAQUE, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);

		GetPlatformFilename(*appProfilingDir(), WriteDir);
		Err = OverlayMiscFiles->addLayer(WriteDir, NULL, WriteTarget, WriteDir, fios::overlay::kOPAQUE, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);

		// location where UnrealConsole puts Screen Captures
		GetPlatformFilename(TEXT("..\\Binaries\\PS3\\Screenshots"), WriteDir);
		Err = OverlayMiscFiles->addLayer(WriteDir, NULL, WriteTarget, WriteDir, fios::overlay::kOPAQUE, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);
	}


	// Notes related to the use of addLayer() calls (located elsewhere):
	//
	// For efficiency, we should really have one layer (and hence one
	// directory) for both patch and install files, since each default layer
	// means an extra stat to check for the existence of a file in that layer.
	// It might "just work" if we pre-install to the patch dir, and never
	// pre-install over files that are newer than the source ones (but then we
	// depend on the time stamps being right).

#if USE_RAM_CACHE
	// Create a small RAM cache to gather dearchiver reads into slightly
	// larger chunks. This helps most (and only a bit) if you have good
	// compression - better than 50%, and does not help at all with
	// uncompressed data. Since we are using unreals decompressor, nothing is
	// compressed, and this won't help any.
	RAMCache = new fios::ramcache(TopMostMedia,
		2,                  // 2 blocks
		64*1024);           // 64KiB for each block
	TopMostMedia = RAMCache;
#endif

#if USE_PSARC_FILES
	// Initialize the EDGE decompressors.
	DecompressorZlib = new fios::Compression::EdgeZlibDecompressor;
	DecompressorZlib->init(GSPURS2);

	// set up decompressors and extensions for the psarc reader
	fios::Compression::AsyncDecompressor* Decompressors[] = { DecompressorZlib };
	const char *Extensions[] = { ".PSARC", ".PSARC.SDAT", ".PSARC.EDAT" };

	// create the PSARC reader
	// @todo ps3 fios ask: For patching, should the FALSE be a TRUE? If we want to use a file in PATCH directory that is in the psarc...
	// or should we put this lower on the stack?
	Dearchiver = new fios::dearchiver(TopMostMedia,
		ARRAY_COUNT(Decompressors),						// number of decompressors
		Decompressors,									// array of decompressors to use
		fios::dearchiver::kSearchNewestToOldest,		// searchMethod when multiple archives are open
		FALSE,											// only use the archive files, don't look outside the archive for override files
		ARRAY_COUNT(Extensions),						// number of extensions
		Extensions);									// array of extensions to recognize
	TopMostMedia = Dearchiver;
#endif

	// allow the command line to override the base directory used on HD
	// NOTE: This is done AFTER UpdateGameData, because the REQUIRED_FILE is installed to there,
	// but it must always go to the default BaseDirectory because if the user switches BaseDirectories,
	// the required file won't get re-cached (only cached when creating GameData). So, if not using
	// the default BaseDirectory, then it won't use the one that was force installed. But that's okay
	// because the shipping game will always use the default BaseDirectory
	Parse(appCmdLine(), TEXT("BaseDir="), GBaseDirectory);

	// prepend FIOS- so that pre-FIOS PS3 HDD0 setups aren't used, now that we don't copy to HDD0 manually
	GBaseDirectory = FString(TEXT("FIOS-")) + GBaseDirectory.ToUpper();

	// handle looking on the HDD for DLC (from store or packages from servers)
	if (!bIsHDDBoot)
	{
		// Create an overlay for DLC, patches, installed files (these won't go through the cache)
		OverlayDLCFiles = new fios::overlay(TopMostMedia);
		TopMostMedia = OverlayDLCFiles;

		// redirect pre-installed files (in GBaseDirectory) to HDD0 
		CellFsErrno Err = OverlayDLCFiles->addLayer("/", NULL, MediaHDD0, TCHAR_TO_ANSI(*GBaseDirectory), cell::fios::overlay::kDEFAULT, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);

		// opaquely redirect /INSTALL to the same location, so we can write to /INSTALL, then read as normal
		Err = OverlayDLCFiles->addLayer("/INSTALL", NULL, MediaHDD0, TCHAR_TO_ANSI(*GBaseDirectory), cell::fios::overlay::kOPAQUE, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);

#if SUPPORT_DOWNLOAD_CACHE
		// redirect CacheDir files to HDD0
		ANSICHAR CacheDirMount[PS3_MAX_PLATFORM_FILENAME];
		ANSICHAR CacheDirActual[PS3_MAX_PLATFORM_FILENAME];
		GetPlatformFilename(*(appGameDir() + TEXT("PS3Cache")), CacheDirMount);
		GetPlatformFilename(*(GBaseDirectory * appGetGameName() + TEXT("Game\\PS3Cache")), CacheDirActual);
		Err = OverlayDLCFiles->addLayer(CacheDirMount, NULL, MediaHDD0, CacheDirActual, cell::fios::overlay::kOPAQUE, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);

		// make sure all directories needed above exist on HDD0
		strcpy(UnderlyingDirectory, DataDir);
		strcat(UnderlyingDirectory, "/");
		strcat(UnderlyingDirectory, TCHAR_TO_ANSI(*GBaseDirectory.ToUpper()));
		cellFsMkdir(UnderlyingDirectory, CELL_FS_DEFAULT_CREATE_MODE_1);

		strcat(UnderlyingDirectory, "/");
		strcat(UnderlyingDirectory, TCHAR_TO_ANSI(*(FString(appGetGameName()) + TEXT("Game")).ToUpper()));
		cellFsMkdir(UnderlyingDirectory, CELL_FS_DEFAULT_CREATE_MODE_1);

		strcat(UnderlyingDirectory, "/PS3CACHE");
		cellFsMkdir(UnderlyingDirectory, CELL_FS_DEFAULT_CREATE_MODE_1);
#endif
	}

	// Create a scheduler, the main object used for all I/O APIs.
	fios::scheduler_attr attr = FIOS_SCHEDULER_ATTR_INITIALIZER;
	SchedulerMain = fios::scheduler::createSchedulerForMedia(TopMostMedia, &attr);

	// keep a global scheduler for convenience in the rest of the code (and easier use in other files)
	GScheduler = SchedulerMain;

#if USE_PSARC_FILES
	// open all PSARC files that are found in the root
	TArray<FString> PSARCFiles;
	
	// note: this happens before the TOC has been read in, so it will look on the root drive, not the TOC
	GFileManagerPS3->FindFiles(PSARCFiles, TEXT("/*.PSARC"), TRUE, FALSE);
	for (INT PSARCIndex = 0; PSARCIndex < PSARCFiles.Num(); PSARCIndex++)
	{
		PS3Fd FileHandle = PS3CheckedFsOpen(TCHAR_TO_ANSI(*PSARCFiles(PSARCIndex)), fios::kO_RDONLY);
		PSARCFileHandles.AddItem(FileHandle);

		printf("Using unencrypted PSARC file %ls\n", *PSARCFiles(PSARCIndex));
	}
	PSARCFiles.Empty();

	// handle edata files
	GFileManagerPS3->FindFiles(PSARCFiles, TEXT("/*.PSARC.EDAT"), TRUE, FALSE);
	for (INT PSARCIndex = 0; PSARCIndex < PSARCFiles.Num(); PSARCIndex++)
	{
		PS3Fd FileHandle = PS3CheckedFsOpen(TCHAR_TO_ANSI(*PSARCFiles(PSARCIndex)), fios::kO_RDONLY | fios::kO_EDATA);
		PSARCFileHandles.AddItem(FileHandle);

		printf("Using EDATA encrypted PSARC file %ls\n", *PSARCFiles(PSARCIndex));
	}
	PSARCFiles.Empty();

	// handle sdata files
	GFileManagerPS3->FindFiles(PSARCFiles, TEXT("/*.PSARC.SDAT"), TRUE, FALSE);
	for (INT PSARCIndex = 0; PSARCIndex < PSARCFiles.Num(); PSARCIndex++)
	{
		PS3Fd FileHandle = PS3CheckedFsOpen(TCHAR_TO_ANSI(*PSARCFiles(PSARCIndex)), fios::kO_RDONLY | fios::kO_SDATA);
		PSARCFileHandles.AddItem(FileHandle);

		printf("Using SDATA encrypted PSARC file %ls\n", *PSARCFiles(PSARCIndex));
	}
#endif
}

void FFIOSStack::PostGSysCreation()
{
	UBOOL bIsHDDBoot = FFileManagerPS3::IsHDDBoot();
	UBOOL bShouldLoadFromBD = FFileManagerPS3::ShouldLoadFromBD();

	if (!bIsHDDBoot || bWriteToHostFS)
	{
		ANSICHAR PlatformFilename[PS3_MAX_PLATFORM_FILENAME];
		GetPlatformFilename(*GSys->ScreenShotPath, PlatformFilename);

		CellFsErrno Err = OverlayMiscFiles->addLayer(PlatformFilename, NULL, bWriteToHostFS ? MediaHost : MediaHDD0, PlatformFilename, fios::overlay::kOPAQUE, &DummyID);
		check(Err == fios::CELL_FIOS_NOERROR);
	}

#if SUPPORT_DOWNLOAD_CACHE
	if (!bIsHDDBoot)
	{
		// force the cache directory to be PS3CACHE to match what we had above (there are lots of underlying directory
		// complications, so since its unlikely that anyone would change the cache directory, we force it, and tell
		// the user
		if (GSys->CachePath != (appGameDir() + TEXT("PS3Cache")))
		{
			GSys->CachePath = appGameDir() + TEXT("PS3Cache");
			debugf(TEXT("WARNING: With FIOS, the download cache directory must be %s, forcing it to be so"), *GSys->CachePath);
		}
	}
#endif
}

/**
 * Teardown the stack and free any allocated resources
 */
void FFIOSStack::DestroyCurrentStack(UBOOL bShouldTerminateFIOS)
{
#if USE_PSARC_FILES
	// close all the PSARC file handles
	for (INT PSARCIndex = 0; PSARCIndex < PSARCFileHandles.Num(); PSARCIndex++)
	{
		GScheduler->closeFileSync(NULL, PSARCFileHandles(PSARCIndex));
	}
	delete DecompressorZlib; DecompressorZlib = NULL;
#endif

	// Destroy the schedulers and media objects in reverse order of creation.
	if (SchedulerMain) fios::scheduler::destroyScheduler(SchedulerMain); SchedulerMain = NULL;
	delete Dearchiver; Dearchiver = NULL;
	delete RAMCache; RAMCache = NULL;
	delete OverlayMiscFiles; OverlayMiscFiles = NULL;
	delete OverlayDLCFiles; OverlayDLCFiles = NULL;
	delete CacheHDD1; CacheHDD1 = NULL;
	if (SchedulerHDD1) fios::scheduler::destroyScheduler(SchedulerHDD1); SchedulerHDD1 = NULL;
	delete MediaHost; MediaHost = NULL;
	delete MediaBluRay; MediaBluRay = NULL;
	delete MediaHDD0Content; MediaHDD0Content = NULL;
	delete MediaHDD1; MediaHDD1 = NULL;
	delete MediaHDD0; MediaHDD0 = NULL;

	// unset the GScheduler
	GScheduler = NULL;

	// Free up all remaining FIOS allocations.
	if (bShouldTerminateFIOS)
	{
		FIOSTerminate();
	}
}



/**
 * Find patch files, and return their paths
 */
void FFIOSStack::HandlePatchFiles(TArray<FString>& PatchFiles)
{
	// while looking for the files in the patch, we need to look in the PATCH directory directly, but
	// later, we will allow FIOS to remap things in /PATCH to the root
	fios::overlay::id_t TempPatchID = fios::overlay::kINVALID_ID;
	OverlayDLCFiles->addLayer("/PATCH", NULL, MediaHDD0, "/PATCH", fios::overlay::kOPAQUE, &TempPatchID);

	// look for files in /PATCH (using a UE3ish name)
	TArray<FString> Results;
	appFindFilesInDirectory(Results, TEXT("..\\PATCH"), TRUE, TRUE);

	// remove the direct /PATCH lookup
	OverlayDLCFiles->deleteLayer(TempPatchID);

	// loop through all the results
	for (INT FileIndex = 0; FileIndex < Results.Num(); FileIndex++)
	{
		// add new entries to the TOC in TOC format
		PatchFiles.AddItem(Results(FileIndex).Replace(TEXT("\\"), TEXT("/")).Replace(TEXT("../PATCH/"), TEXT("/")));
		printf("Found patch file %S...\n", *PatchFiles(PatchFiles.Num() - 1));
	}

	if (Results.Num() > 0)
	{
		// Look for all files under GHDD0 PATCH, this must happen before
		// looking for files under /INSTALL, but files should not exist in
		// both places as that wastes disk space.
		OverlayDLCFiles->addLayer("/", NULL, MediaHDD0, "/PATCH", fios::overlay::kDEFAULT, &DummyID);
	}
}

/**
 * Look for DLC and redirect reads to there if they exist
 * 
 * @return TRUE if there were any DLC files installed
 */
UBOOL FFIOSStack::HandleDLC()
{
	fios::overlay::id_t LayerID = fios::overlay::kINVALID_ID;
	if (!FFileManagerPS3::IsHDDBoot())
	{
		// redirect DLC files to HDD0 
		CellFsErrno Err = OverlayDLCFiles->addLayer("/DLC", NULL, MediaHDD0, "/DLC", cell::fios::overlay::kOPAQUE, &LayerID);
		check(Err == fios::CELL_FIOS_NOERROR);
	}

	// look for any DLC
	const FString DLCRoot = TEXT("/DLC/*");
	TArray<FString> DLCDirectories;
	GFileManagerPS3->FindFiles(DLCDirectories, *DLCRoot, FALSE, TRUE);

	// if there aren't any DLC's, remove the layer (no need for overhead) unless we can import
	// mods at runtime, then we need to keep the layer
#if !SUPPORT_MOD_IMPORT
	if (LayerID != fios::overlay::kINVALID_ID && DLCDirectories.Num() == 0)
	{
		OverlayDLCFiles->deleteLayer(LayerID);
		return FALSE;
	}
#endif

	// return that we have DLC
	return TRUE;
}

/**
 * @return if it's okay to read/write using HostFS (different then if we should load source files from Host)
 */
UBOOL FFIOSStack::ShouldLoadFromHostFS() const
{
	UBOOL bUseHostFS = TRUE;

#if FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE
	// In FR, use HostFS if we aren't booting from BD, otherwise, skip Host
	bUseHostFS = !FFileManagerPS3::ShouldLoadFromBD();
#endif

	// if we wanted to use HOSTFS, make sure it exists
	if (bUseHostFS)
	{
		// look for the appGameDir() on HOSTFS (this is a good indication that HOSTFS is set up
		// properly, no matter how we were booted
		ANSICHAR Filename[PS3_MAX_PLATFORM_FILENAME];
		strcpy(Filename, DRIVE_APP);

		// GetPlatformFilename now uppercasify's everything, and we can't put DRIVE_APP into
		// uppercase, so just do it on the appGameDir part
		ANSICHAR GameDirFilename[PS3_MAX_PLATFORM_FILENAME];
		GetPlatformFilename(*(appGameDir()), GameDirFilename);
		strcat(Filename, GameDirFilename);

		CellFsStat Stat;
		if (cellFsStat(Filename, &Stat) != CELL_FS_SUCCEEDED)
		{
			bUseHostFS = FALSE;
			printf("HOSTFS was desired, but could not be found [looked for %s]\n", Filename);
		}
	}

	// hook up a FIOS media for it if we are using it
	if (bUseHostFS)
	{
		printf("HOSTFS will be used\n");
	}
	else
	{
		printf("HOSTFS will not be used\n");
	}

	return bUseHostFS;
}

/**
 * Cleans up memory after FindFiles reads directories
 */
void FFIOSStack::CleanupFindFiles()
{
#if USE_PSARC_FILES
	Dearchiver->freeManifests();
#endif
}


/**
 * Tosses HDD1 cached files
 */
void FFIOSStack::FlushCache()
{
	CacheHDD1->flush();
}


/*=============================================================================
	PS3Launch.cpp: Game launcher.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "LaunchPrivate.h"

#include <sysutil/sysutil_common.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_msgdialog.h>
#include <sys/dbg.h>
#include <cell/fs/cell_fs_file_api.h>

#include "EngineUserInterfaceClasses.h"
#include "PS3NetworkPlatform.h"

// Specify thread size
#define MAIN_THREAD_STACK_SIZE 256 * 1024

// Specify thread priority
#define MAIN_THREAD_PRIORITY 2000

// Exception handler priority
#define EXCEPTION_HANDLER_PRIORITY 0x3e7

// How often to trim memory
#define TIME_BETWEEN_MEMORY_TRIM 1.0f

// specify priority and stack size of default thread
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
#if USE_MALLOC_PROFILER
// need a bit more stack space for tracking allocations before main thread startup
SYS_PROCESS_PARAM(3070, 64 * 1024)
#else
SYS_PROCESS_PARAM(3070, 16 * 1024)
#endif
#else
SYS_PROCESS_PARAM(MAIN_THREAD_PRIORITY, MAIN_THREAD_STACK_SIZE)
#endif

/** Global engine loop object												*/
FEngineLoop	EngineLoop;

/*-----------------------------------------------------------------------------
	Guarded code.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

//
// Immediate exit.
//

/**
 * This function will "cleanly" shutdown the PS3 and exit to the system software
 * It must be called from the main thread.
 */
void appPS3QuitToSystem()
{
	static UBOOL Once = FALSE;
	if (Once)
	{
		return;
	}
	Once = TRUE;

	// deal with the lock file to detect crashes
	printf("Deleting creash detection lock file...\n");
	if (GScheduler->unlinkSync(NULL, "/INSTALL/___LOCK") != CELL_FS_SUCCEEDED)
	{
		printf("Deleting lock file failed\n");
	}

	// sleep a little to let everything catch up (may not be needed, but should make it a little safer)
	appSleep(1.0f);

	// stop a movie playing if there is one
	if (GFullScreenMovie && GFullScreenMovie->GameThreadIsMoviePlaying(TEXT("")))
	{
		GFullScreenMovie->GameThreadStopMovie(0, TRUE, TRUE);
	}

	if (GIsThreadedRendering)
	{
		// Stop the rendering thread.s
		StopRenderingThread();
	}

	// make sure all flips and commands are complete (TRC 23.4)
#if !USE_NULL_RHI
	cellGcmSetWaitFlip();
	if (GPS3Gcm)
	{
#if USE_BINK_CODEC
		// this will make sure the movie player is shutdown, so no threading issues will crash the game
		// it includes a call to GPS3Gcm->BlockUntilGPUIdle(), so we don't call it here
		extern void FreeBinkStuff();
		FreeBinkStuff();
#else
		GPS3Gcm->BlockUntilGPUIdle();
#endif
	}
#endif	//#if !USE_NULL_RHI

	// Retrieve IO system
	if (GIOManager)
	{
		FIOSystem* IO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
		if (IO)
		{
			IO->CancelAllOutstandingRequests();
			IO->BlockTillAllRequestsFinishedAndFlushHandles();
		}
	}

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if (GIsRunning)
	{
		EngineLoop.Exit();
	}

#endif

	// shutdown multistream
	for (TObjectIterator<UAudioDevice> It; It; ++It)
	{
		It->ShutdownAfterError();
	}

	if (GPS3NetworkPlatform)
	{
		GPS3NetworkPlatform->Teardown();
	}

	// shut down spurs
	extern void appPS3ShutdownSPU();
	appPS3ShutdownSPU();

	cellSysmoduleUnloadModule( CELL_SYSMODULE_FS );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SPURS );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_RTC );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SHEAP );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_GCM_SYS );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_AUDIO );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_IO );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_NET );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_NETCTL );
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	cellSysmoduleUnloadModule( CELL_SYSMODULE_LV2DBG );
#endif
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL_SAVEDATA );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL_NP );
	cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL_NP_TROPHY );
	

#if !USE_NULL_RHI
	extern UBOOL GIsShutdownAlready;
	GIsShutdownAlready = TRUE;
#endif	//#if !USE_NULL_RHI

	_Exit(0);
}

struct FQuitMessageHandler : public FTickableObjectRenderThread
{
	FQuitMessageHandler()
	: FTickableObjectRenderThread(FALSE) // in rendering thread and do not register immediately as the RENDER_COMMAND will do that
	{
	}

	~FQuitMessageHandler()
	{
//		cellSysutilUnregisterCallback(0);
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		return TRUE;
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTick.cpp after ticking all actors.
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	void Tick(FLOAT DeltaTime)
	{
		// process any sysutil messages
		cellSysutilCheckCallback();
	}
};

/*-----------------------------------------------------------------------------
SHA-1 functions.
-----------------------------------------------------------------------------*/

/**
 * Get the hash values out of the executable hash section made with ppu-lv2-objcopy (see makefile)
 */
void InitSHAHashes()
{
// this is defined in the makefile/project
#if USE_HASHES_SHA
	extern char _binary_hashes_sha_start[];
	extern char _binary_hashes_sha_size[];

	// if it exists, parse it
	DWORD Offset = 0;
	DWORD SectionSize = (DWORD)_binary_hashes_sha_size;
	BYTE* SectionData = (BYTE*)_binary_hashes_sha_start;

	// there may be a dummy byte so the file is not empty
	if (SectionSize <= 1)
	{
		return;
	}

	// load up the global sha maps
	FSHA1::InitializeFileHashesFromBuffer(SectionData, SectionSize);
#endif
}


/**
* Callback that is called if the asynchronous SHA verification fails
* This will be called from a pooled thread.
*
* @param FailedPathname Pathname of file that failed to verify
* @param bFailedDueToMissingHash TRUE if the reason for the failure was that the hash was missing, and that was set as being an error condition
*/
void appOnFailSHAVerification(const TCHAR* FailedPathname, UBOOL bFailedDueToMissingHash)
{
#if USE_HASHES_SHA
	debugf(TEXT("SHA Verification failed for '%s'. Reason: %s"), 
		FailedPathname ? FailedPathname : TEXT("Unknown file"),
		bFailedDueToMissingHash ? TEXT("Missing hash") : TEXT("Bad hash"));

	// what is best response for SHA failure? not crash, I'd hope
	appHandleIOFailure(FailedPathname);
#else
	debugfSuppressed(NAME_DevSHA, TEXT("SHA Verification failed for '%s'. Reason: %s"), 
		FailedPathname ? FailedPathname : TEXT("Unknown file"),
		bFailedDueToMissingHash ? TEXT("Missing hash") : TEXT("Bad hash"));
#endif
}

static void GuardedMain()
{
	// load the hashes from the executable
	InitSHAHashes();

	// make a temp copy of the command line which is already filled out by UnPS3, 
	// but will get modified and refilled in in appInit
	extern TCHAR GCmdLine[16384];
	EngineLoop.PreInit(*FString(GCmdLine));

	// start up the system shutdown message handler/ticker for the entirety of the game
	// runs in the rendering thread which needs to be active so that the system menu can
	// render anyway to be able to shutdown
	FQuitMessageHandler* QuitMessageHandler = new FQuitMessageHandler();  // this will be deleted by the RENDER_COMMAND below
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( RegisterQuitHandler, 
		FQuitMessageHandler*, QuitMessageHandler, QuitMessageHandler, 
	{ 
		QuitMessageHandler->Register( TRUE ); 
	} );

	if (GIsRequestingExit)
	{
		printf("I had requested exit, so we will skip to shutting down.\n");
	}
	else
	{
		EngineLoop.Init();

	#if FINAL_RELEASE
		// remove the console keys for FINAL_RELEASE
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		GameEngine->GameViewport->ViewportConsole->ConsoleKey = NAME_None;
		GameEngine->GameViewport->ViewportConsole->TypeKey = NAME_None;
	#endif

	// wasn't suspended in UT (has movies)
	#if ((GAMENAME != UTGAME) && (GAMENAME != GEARGAME))
		// Resume rendering again now that we're done loading 
		ENQUEUE_UNIQUE_RENDER_COMMAND( ResumeRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = FALSE; RHIResumeRendering(); } );
	#endif

		FLOAT TimeUntilTrim = TIME_BETWEEN_MEMORY_TRIM;
		while (!GIsRequestingExit)
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_FrameTime);
				EngineLoop.Tick();
			}

	#if STATS
			// Write all stats for this frame out
			GStatManager.AdvanceFrame();

			if(GIsThreadedRendering)
			{
				ENQUEUE_UNIQUE_RENDER_COMMAND(AdvanceFrame,{RenderingThreadAdvanceFrame();});
			}
	#endif		

			// trim after a countdown
			TimeUntilTrim -= GDeltaTime;
			if (TimeUntilTrim <= 0)
			{
				// trim as much memory as possible to give back to OS to reduce fragmentation
				GMalloc->TrimMemory(0);

				// reset the countdown
				TimeUntilTrim = TIME_BETWEEN_MEMORY_TRIM;
			}
		}

		// this deletes the FQuitMessageHandler that we new'd above
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( DeleteQuitHandler, 
			FQuitMessageHandler*, QuitMessageHandler, QuitMessageHandler, 
		{ 
			delete QuitMessageHandler;
		} );

	}

	appRequestExit(1);
}

void ThreadedMain(QWORD Arg)
{
	QWORD ReturnValue = 0;

	// transfer ownership of debugfs to this thread
	GLog->SetCurrentThreadAsMasterThread();

	// default to no game
	appStrcpy(GGameName, TEXT("None"));

	// Begin guarded code.
	GIsStarted = 1;
	GIsGuarded = 0; //@todo gcc: "guard" up GuardedMain at some point for builds running outside the debugger
	GuardedMain();
	GIsGuarded = 0;

	// Final shut down.
	appExit();

	GIsStarted = 0;

	printf("...Goodbye!\n");
	printf("\x1A"); // causes ps3run to terminate

	// return from the thread with the exit code
	sys_ppu_thread_exit(ReturnValue);
}

/**
 * Exception handling function
 * @param Cause Exception type
 * @param ThreadID ID of thread that crashed
 * @param DAR Data address register
 */
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
void _ExceptionHandler(uint64_t Cause, sys_ppu_thread_t ThreadID, uint64_t DAR)
{
	// skip any shutdown errors
	if (GIsRequestingExit)
	{
		return;
	}

	printf("Caught an exception %x\n", Cause);

	// get thread context
	FThreadContext Context;
	sys_dbg_read_ppu_thread_context(ThreadID, &Context);

	// dump callstack for that thread
	PS3Callstack(&Context);
}
#endif

/** Global variable that says if the exception handler was installed or not */
UBOOL GPS3ExceptionHandlerInstalled;

int main(int argc, char* const argv[])
{
	appPS3Init(argc, argv);

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// startup exception handling for dev/test kits (this will return an error on retails)
	GPS3ExceptionHandlerInstalled = FALSE;
	if (sys_dbg_initialize_ppu_exception_handler(EXCEPTION_HANDLER_PRIORITY) == CELL_OK)
	{
		if (sys_dbg_register_ppu_exception_handler(_ExceptionHandler, SYS_DBG_PPU_THREAD_STOP | SYS_DBG_SPU_THREAD_GROUP_STOP ) == CELL_OK)
		{
			printf("Exception handler was installed!\n");
			GPS3ExceptionHandlerInstalled = TRUE;
		}
	}

	// uynload the lv2dbg prx if we can't use it
	if (!GPS3ExceptionHandlerInstalled)
	{
		printf("Exception handler was NOT installed!\n");
		cellSysmoduleUnloadModule(CELL_SYSMODULE_LV2DBG);
	}

	// start a new thread so we can catch exceptions
	sys_ppu_thread_t Thread;
	INT Result = sys_ppu_thread_create(
		&Thread,						// return value
		ThreadedMain,					// entry point
		0,								// parameter
		MAIN_THREAD_PRIORITY,			// priority as a number [0..4095]
		MAIN_THREAD_STACK_SIZE,			// stack size for thread
		SYS_PPU_THREAD_CREATE_JOINABLE,	// flag as joinable, non-interrupt thread
		"main");						// name the thread for the debugger

	QWORD ReturnValue;
	sys_ppu_thread_join(Thread, &ReturnValue);

	// shut down exception handler
	sys_dbg_finalize_ppu_exception_handler();
	sys_dbg_unregister_ppu_exception_handler();

	return ReturnValue;
#else
	// no need for separate thread in final release since there's no exception handling.
	ThreadedMain(0);
	return 0;
#endif
}


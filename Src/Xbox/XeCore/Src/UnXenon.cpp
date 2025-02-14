/*=============================================================================
	UnXenon.cpp: Visual C++ Xenon core.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Core includes.
#include "CorePrivate.h"
#include "XeD3DDrvPrivate.h"
#include "UnIpDrv.h"
#include "ChartCreation.h"
#include "ProfilingHelpers.h"
#include "EngineAudioDeviceClasses.h"
#include "XAudio2Device.h"

#include <xfilecache.h>

#if ALLOW_TRACEDUMP
#pragma comment( lib, "tracerecording.lib" )
#endif

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
#pragma pack(push,8)
#include <xbdm.h>
#pragma pack(pop)
#pragma comment( lib, "xbdm.lib" )
#endif

/*-----------------------------------------------------------------------------
	Vararg.
-----------------------------------------------------------------------------*/

#define VSNPRINTF _vsnwprintf
#define VSNPRINTFA _vsnprintf

/**
* Helper function to write formatted output using an argument list
*
* @param Dest - destination string buffer
* @param DestSize - size of destination buffer
* @param Count - number of characters to write (not including null terminating character)
* @param Fmt - string to print
* @param Args - argument list
* @return number of characters written or -1 if truncated
*/
INT appGetVarArgs( TCHAR* Dest, SIZE_T /*DestSize*/, INT Count, const TCHAR*& Fmt, va_list ArgPtr )
{
	INT Result = VSNPRINTF( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}

/**
* Helper function to write formatted output using an argument list
* ASCII version
*
* @param Dest - destination string buffer
* @param DestSize - size of destination buffer
* @param Count - number of characters to write (not including null terminating character)
* @param Fmt - string to print
* @param Args - argument list
* @return number of characters written or -1 if truncated
*/
INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T /*DestSize*/, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr )
{
	INT Result = VSNPRINTFA( Dest, Count, Fmt, ArgPtr );
	va_end( ArgPtr );
	return Result;
}

// Disable deferred logging if you hit deadlocks when dumping from the malloc
// Deferred logging is a performance gain only when capturing logs and isn't
// meant to be used in when trying to log from malloc
#if USE_MALLOC_PROFILER || ENABLE_MEM_TAGGING || TRACK_MEM_USING_TAGS
	#define WANTS_DEFERRED_LOGGING 0
#else
	#define WANTS_DEFERRED_LOGGING (!FINAL_RELEASE_DEBUGCONSOLE && !FINAL_RELEASE)
#endif

#if WANTS_DEFERRED_LOGGING

#pragma pack(push,8)
	#include <xmcore.h>
#pragma pack(pop)
#if _DEBUG
	#pragma comment(lib,"xmcored.lib")
#else
	#pragma comment(lib,"xmcore.lib")
#endif

/**
 * Uses the Xbox Lock Free Log class to write to output debug device
 */
class FBackgroundLogger :
	public XLockFreeLog
{
protected:
	/**
	 * Whether the lock free logger is valid
	 */
	UBOOL bInitialized;

	/**
	 * Initializes the base class specifying the sizes and whether to block when the queue is full
	 */
	FBackgroundLogger() :
		XLockFreeLog()
	{
		// We'll use 256KB for buffer and block if we overrun
		InitializeUser(BACKGROUND_LOGGING_HWTHREAD,2096,128,TRUE);
		bInitialized = TRUE;
	}

	/**
	 * Calls the destroy on the base class
	 */
	virtual ~FBackgroundLogger()
	{
		Destroy();
	}

public:
	/**
	 * Adds a string to the queue
	 *
	 * @param Message the string to add to the queue
	 */
	void QueueString(const TCHAR* Message)
	{
		if (bInitialized)
		{
			// The extra format is so that embedded %s don't crash
			LogPrint("%s",TCHAR_TO_ANSI(Message));
		}
		else
		{
			// Don't queue, since the thread isn't functioning. Just call directly
			LogMessage((PBYTE)TCHAR_TO_ANSI(Message),2048);
		}
	}

	/**
	 * Adds a string to the queue
	 *
	 * @param Message the string to add to the queue
	 */
	void QueueString(const ANSICHAR* Message)
	{
		if (bInitialized)
		{
			// The extra format is so that embedded %s don't crash
			LogPrint("%s",Message);
		}
		else
		{
			// Don't queue, since the thread isn't functioning. Just call directly
			LogMessage((PBYTE)Message,2048);
		}
	}

	/** Stops the background logging thread */
	void StopBackgroundLogging(void)
	{
		bInitialized = FALSE;
		Destroy();
	}
};

	/**
 * Uses the our base lock free logger to output debug strings
	 */
class FBackgroundOutputDebugString :
	public FBackgroundLogger
{
	/**
	 * Lets the base class do all of the work
	 */
	FBackgroundOutputDebugString() :
		FBackgroundLogger()
	{
	}

	/**
     * Callback that happens on the background thread
	 */
	virtual HRESULT CALLBACK LogMessage(__in_bcount(length) PBYTE buffer, __in DWORD length)
	{
	    UNREFERENCED_PARAMETER(length);
	    OutputDebugStringA((LPCSTR)buffer);
		return S_OK;
	}

public:
	/**
	 * Factory method for creating the instance
	 *
	 * @return instance of the background logger
	 */
	static FBackgroundOutputDebugString* GetInstance(void)
	{
		static FBackgroundOutputDebugString* Instance = NULL;
		// Create the new instance that will run it
		if (Instance == NULL)
		{
			Instance = new FBackgroundOutputDebugString();
		}
		return Instance;
	}
};

	/**
 * Uses the our base lock free logger to output debug strings
	 */
class FBackgroundDmNotifier :
	public FBackgroundLogger
{
	/**
	 * Lets the base class do all of the work
	 */
	FBackgroundDmNotifier() :
		FBackgroundLogger()
	{
	}

    /**
     * Callback that happens on the calling thread (background or caller if background was destroyed)
     */
	virtual HRESULT CALLBACK LogMessage(__in_bcount(length) PBYTE buffer, __in DWORD length)
	{
	    UNREFERENCED_PARAMETER(length);
	    DmSendNotificationString((LPCSTR)buffer);
		return S_OK;
	}

public:
	/**
	 * Factory method for creating the instance
	 *
	 * @return instance of the background logger
	 */
	static FBackgroundDmNotifier* GetInstance(void)
	{
		static FBackgroundDmNotifier* Instance = NULL;
		// Create the new instance that will run it
		if (Instance == NULL)
		{
			Instance = new FBackgroundDmNotifier();
		}
		return Instance;
	}
};
#endif

/** Used to flush and stop the background logging thread */
void appStopLoggingThread(void)
{
#if WANTS_DEFERRED_LOGGING
	FBackgroundOutputDebugString::GetInstance()->StopBackgroundLogging();
	FBackgroundDmNotifier::GetInstance()->StopBackgroundLogging();
#endif
}

/**
 * Rebuild the commandline if needed
 *
 * @param NewCommandLine The commandline to fill out
 *
 * @return TRUE if NewCommandLine should be pushed to GCmdLine
 */
UBOOL appResetCommandLine(TCHAR NewCommandLine[16384])
{
	return FALSE;
}

/*
 * Sends the message to the debugging output
 *
 * @param Message the message to log out
 */
void appOutputDebugString( const TCHAR* Message )
{
#if WANTS_DEFERRED_LOGGING
	FBackgroundOutputDebugString::GetInstance()->QueueString(Message);
#elif ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	OutputDebugString(Message);
#endif
}

/*
 * Sends a message to a remote tool
 *
 * @param Message the message to log out
 */
void appSendNotificationString( const ANSICHAR* Message )
{
#if WANTS_DEFERRED_LOGGING
	FBackgroundDmNotifier::GetInstance()->QueueString(Message);
#elif ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	DmSendNotificationString(Message);
#endif
}

/*
 * Sends a message to a remote tool
 *
 * @param Message the message to log out
 */
void appSendNotificationString( const TCHAR* Message )
{
#if WANTS_DEFERRED_LOGGING
	FBackgroundDmNotifier::GetInstance()->QueueString(Message);
#elif ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	DmSendNotificationString(TCHAR_TO_ANSI(Message));
#endif
}

//
// Immediate exit.
//
void appRequestExit( UBOOL Force )
{
	debugf( TEXT("appRequestExit(%i)"), Force );
	if( Force )
	{
		// Don't exit.
		appDebugBreak();
		Sleep( INFINITE );
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
	}
}

/**
 * Returns the last system error code in string form.  NOTE: Only one return value is valid at a time!
 *
 * @param OutBuffer the buffer to be filled with the error message
 * @param BufferLength the size of the buffer in character count
 * @param Error the error code to convert to string form
 */
const TCHAR* appGetSystemErrorMessage(TCHAR* OutBuffer,INT BufferCount,INT Error)
{
	check(OutBuffer && BufferCount >= MAX_SPRINTF);
	if (Error == 0)
	{
		Error = GetLastError();
	}
	appSprintf(OutBuffer,TEXT("appGetSystemErrorMessage not supported.  Error: %d"),Error);
	warnf(OutBuffer);
	return OutBuffer;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

//
// Copy text to clipboard.
//
void appClipboardCopy( const TCHAR* Str )
{
}

//
// Paste text from clipboard.
//
FString appClipboardPaste()
{
	return TEXT("");
}

/*-----------------------------------------------------------------------------
	DLLs.
-----------------------------------------------------------------------------*/

//
// Load a library.
//
void* appGetDllHandle( const TCHAR* Filename )
{
	return NULL;
}

//
// Free a DLL.
//
void appFreeDllHandle( void* DllHandle )
{
}

//
// Lookup the address of a DLL function.
//
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	return NULL;
}

void appDebugMessagef( const TCHAR* Fmt, ... )
{
	warnf(TEXT("appDebugMessagef not supported."));
}

/**
 *
 * @param Type - dictates the type of dialog we're displaying
 *
 **/
VARARG_BODY( UBOOL, appMsgf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) )
{
	TCHAR MsgString[4096]=TEXT("");
	GET_VARARGS( MsgString, ARRAY_COUNT(MsgString), ARRAY_COUNT(MsgString)-1, Fmt, Fmt );

	warnf( TEXT("appMsgf not supported.  Message was: %s" ), MsgString );
	return 1;
}

void appGetLastError( void )
{
	warnf( TEXT("appGetLastError():  Not supported") );
}

// Interface for recording loading errors in the editor
void EdClearLoadErrors()
{
	GEdLoadErrors.Empty();
}

VARARG_BODY( void VARARGS, EdLoadErrorf, const TCHAR*, VARARG_EXTRA(INT Type) )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );

	// Check to see if this error already exists ... if so, don't add it.
	// NOTE : for some reason, I can't use AddUniqueItem here or it crashes
	for( INT x = 0 ; x < GEdLoadErrors.Num() ; ++x )
		if( GEdLoadErrors(x) == FEdLoadError( Type, TempStr ) )
			return;

	new( GEdLoadErrors )FEdLoadError( Type, TempStr );
}


/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// Sleep this thread for Seconds, 0.0 means release the current
// timeslice to let other threads get some attention.
//
void appSleep( FLOAT Seconds )
{
	Sleep( (DWORD)(Seconds * 1000.0) );
}

/**
 * Sleeps forever. This function does not return!
 */
void appSleepInfinite()
{
	Sleep(INFINITE);
}

//
// Return the system time.
//
void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	SYSTEMTIME st;
	GetLocalTime( &st );

	Year		= st.wYear;
	Month		= st.wMonth;
	DayOfWeek	= st.wDayOfWeek;
	Day			= st.wDay;
	Hour		= st.wHour;
	Min			= st.wMinute;
	Sec			= st.wSecond;
	MSec		= st.wMilliseconds;
}

//
// Return the UTC time.
//
void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	SYSTEMTIME st;
	GetSystemTime( &st );

	Year		= st.wYear;
	Month		= st.wMonth;
	DayOfWeek	= st.wDayOfWeek;
	Day			= st.wDay;
	Hour		= st.wHour;
	Min			= st.wMinute;
	Sec			= st.wSecond;
	MSec		= st.wMilliseconds;
}

/*-----------------------------------------------------------------------------
	Profiling.
-----------------------------------------------------------------------------*/

#if ALLOW_TRACEDUMP

/** Filename of current trace. Needs to be kept around as stopping trace will transfer it to host via UnrealConsole. */
FString CurrentTraceFilename;
/** Whether we are currently tracing. */
UBOOL bIsCurrentlyTracing = FALSE;

/**
 * Starts a CPU trace if the passed in trace name matches the global current requested trace.
 *
 * @param	TraceName					trace to potentially start
 * @param	bShouldSuspendGameThread	if TRUE game thread will be suspended for duration of trace
 * @param	bShouldSuspendRenderThread	if TRUE render thread will be suspended for duration of trace
 * @param	TraceSizeInMegs				size of trace buffer size to allocate in MByte
 * @param	FunctionPointer				if != NULL, function pointer if tracing should be limited to single function
 */
void appStartCPUTrace( FName TraceName, UBOOL bShouldSuspendGameThread, UBOOL bShouldSuspendRenderThread, INT TraceSizeInMegs, void* FunctionPointer )
{
	if( TraceName == GCurrentTraceName )
	{
		// Can't start a trace if one is already in progress.
		check( GCurrentTraceName != NAME_None && bIsCurrentlyTracing == FALSE );
		// Keeping track of whether we are tracing independently from trace name as STOP can get called before START.
		GCurrentTraceName = TraceName;
		bIsCurrentlyTracing = TRUE;

		// Use a fixed time stamp for tracing next frame. This allows comparing apples to apples when profiling changes.
		GUseFixedTimeStep = TRUE;

		// Suspend render or game thread if requested. Usually the thread not being profiled is being put to sleep to aide the
		// trace library and not distort results. This is optional though.
		GShouldSuspendGameThread		= bShouldSuspendGameThread;
		GShouldSuspendRenderingThread	= bShouldSuspendRenderThread;

		// Sleep for 500 ms to make sure that the game/ render threads are paused before profiling begins.
		if( GShouldSuspendGameThread || GShouldSuspendRenderingThread )
		{
			appSleep( 0.5 );
		}

		// Set the buffer size to the requested amount. If this OOMs or fails a good trick is to reduce the texture pool size
		// and disable texture streaming if it's orthogonal to the problem being profiled.
		XTraceSetBufferSize( TraceSizeInMegs * 1024 * 1024 );
		// Need to keep the filename around as we're going to copy it to the host later.

		const FString PathName = *( FString(TEXT("GAME:\\")) + FString::Printf( TEXT("%sGame\\Profiling\\"), GGameName )+ TEXT("Trace") PATH_SEPARATOR );
		GFileManager->MakeDirectory( *PathName );

        // needed to correctly create the long set of directories
		const FString CreateDirectoriesString = CreateProfileDirectoryAndFilename( TEXT("Trace"), TEXT(".pix2") );

		const FString Filename = CreateProfileFilename( TEXT(".pix2"), TRUE  );
		const FString FilenameFull = PathName + Filename;

		CurrentTraceFilename = FilenameFull;;

	    CurrentTraceFilename = FString::Printf(TEXT("GAME:\\trace-%s-%s.pix2"), *TraceName.ToString(), *appSystemTimeString());	

		// Start the fun!
		if( FunctionPointer )
		{
			XTraceStartRecordingFunction( TCHAR_TO_ANSI(*CurrentTraceFilename ), FunctionPointer, TRUE );
		}
		else
		{
			XTraceStartRecording( TCHAR_TO_ANSI(*CurrentTraceFilename ) );
		}
	}
}

/**
 * Stops tracing if currently active and matching trace name.
 *
 * @param	TraceToStop		name of trace to stop
 */
void appStopCPUTrace( FName TraceToStop )
{
	// It's okay to stop tracing even if no trace is running as it makes the code flow easier.
	if( bIsCurrentlyTracing && TraceToStop == GCurrentTraceName )
	{
		check( GCurrentTraceName != NAME_None );

		// We're no longer performing a trace
		bIsCurrentlyTracing = FALSE;

		// Stop the trace.
		GCurrentTraceName = NAME_None;
		XTraceStopRecording();

		// Notify Unreal console of newly available trace to copy to PC.
		SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!GAME:"), CurrentTraceFilename );

		// Unsuspend render and game threads now that we're done.
		GShouldSuspendGameThread		= FALSE;
		GShouldSuspendRenderingThread	= FALSE;
	}
}

#endif


/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	warnf( TEXT( "appLaunchURL() was called" ) );
}

/**
 * Attempt to launch the provided file name in its default external application. Similar to appLaunchURL,
 * with the exception that if a default application isn't found for the file, the user will be prompted with
 * an "Open With..." dialog.
 *
 * @param	FileName	Name of the file to attempt to launch in its default external application
 * @param	Parms		Optional parameters to the default application
 */
void appLaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms /*= NULL*/ )
{
	appErrorf(TEXT("appLaunchFileInDefaultExternalApplication not supported."));
}

/**
 * Attempt to "explore" the folder specified by the provided file path
 *
 * @param	FilePath	File path specifying a folder to explore
 */
void appExploreFolder( const TCHAR* FilePath )
{
	appErrorf(TEXT("appExploreFolder not supported."));
}

/**
 * Creates a new process and its primary thread. The new process runs the
 * specified executable file in the security context of the calling process.
 * @param URL					executable name
 * @param Parms					command line arguments
 * @param bLaunchDetached		if TRUE, the new process will have its own window
 * @param bLaunchHidded			if TRUE, the new process will be minimized in the task bar
 * @param bLaunchReallyHidden	if TRUE, the new process will not have a window or be in the task bar
 * @param OutProcessId			if non-NULL, this will be filled in with the ProcessId
 * @param PriorityModifier		-2 idle, -1 low, 0 normal, 1 high, 2 higher
 * @return	The process handle for use in other process functions
 */
void* appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached, UBOOL bLaunchHidden, UBOOL bLaunchReallyHidden, DWORD* OutProcessID, INT PriorityModifier )
{
	appErrorf(TEXT("appCreateProc not supported."));
	return NULL;
}

/** Returns TRUE if the specified process is running 
*
* @param ProcessHandle handle returned from appCreateProc
* @return TRUE if the process is still running
*/
UBOOL appIsProcRunning( void* )
{
	appErrorf(TEXT("appIsProcRunning not supported"));
	return FALSE;
}

/** Waits for a process to stop
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appWaitForProc( void* )
{
	appErrorf(TEXT("appWaitForProc not supported"));
}

/** Terminates a process
*
* @param ProcessHandle handle returned from appCreateProc
*/
void appTerminateProc( void* )
{
	appErrorf(TEXT("appTerminateProc not supported"));
}

/** Retrieves the ProcessId of this process
*
* @return the ProcessId of this process
*/
DWORD appGetCurrentProcessId()
{
	appErrorf(TEXT("appGetCurrentProcessId not supported"));
	return 0;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	appErrorf(TEXT("appGetProcReturnCode not supported."));
	return false;
}

/** Returns TRUE if the specified application is running */
UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	appErrorf(TEXT("appIsApplicationRunning not implemented."));
	return FALSE;
}

/*-----------------------------------------------------------------------------
	File handling.
-----------------------------------------------------------------------------*/

void appCleanFileCache()
{
	// do standard cache cleanup
	GSys->PerformPeriodicCacheCleanup();

	// perform any other platform-specific cleanup here
}

/**
 * Handles IO failure by either signaling an event for the render thread or handling
 * the event from the render thread.
 *
 * @param Filename	If not NULL, name of the file the I/O error occured with
 */
void appHandleIOFailure( const TCHAR* Filename )
{
	// @todo ship: For games about to ship, this should removed or turned to a debugf, because it will 
	// get in the way of the DirtyDiscErrorUI(). However, until then, even in FINAL_RELEASE, we want
	// the game to crash to get the callstack
	appErrorf(TEXT("I/O failure operating on '%s'"), Filename ? Filename : TEXT("Unknown file"));

	// Signals to the rendering thread to handle the dirty disc error and 
	// signals to other threads to stop doing anything meaningful
	GHandleDirtyDiscError = TRUE;

	if (IsInRenderingThread())
	{
		RHISuspendRendering();
		XShowDirtyDiscErrorUI(0); //@todo TCR: use the proper index
	}
	else
	{
		// Stall indefinitely on this thread
		appSleepInfinite();
	}
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

/**
 * @return Returns a new globally unique identifier.
 */
FGuid appCreateGuid()
{
	FGuid GUID;
	// Make a cryptographically random value
	if (XNetRandom((BYTE*)&GUID,sizeof(FGuid)))
	{
		SYSTEMTIME Time;
		GetLocalTime( &Time );
		// Failed to create so use the old way
		GUID.A = Time.wDay | (Time.wHour << 16);
		GUID.B = Time.wMonth | (Time.wSecond << 16);
		GUID.C = Time.wMilliseconds | (Time.wMinute << 16);
		GUID.D = Time.wYear ^ appCycles();
	}
	return GUID;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
const TCHAR* appBaseDir()
{
	static TCHAR Result[256]=TEXT("");
	return Result;
}

// Get computer name.
const TCHAR* appComputerName()
{
	static TCHAR Result[256]=TEXT("Xbox360");
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	char Temp[256];
	DWORD Num = 256;
	// Get the real xbox name
	if (DmGetXboxName(Temp,&Num) == XBDM_NOERR)
	{
		// Copy it into the static
		appStrcpy(Result,ANSI_TO_TCHAR(Temp));
	}
#endif
	return Result;
}

// Get user name.
const TCHAR* appUserName()
{
	static TCHAR Result[256]=TEXT("User");
	return Result;
}

// shader dir relative to appBaseDir
const TCHAR* appShaderDir()
{
	static TCHAR Result[256] = PATH_SEPARATOR TEXT("Engine") PATH_SEPARATOR TEXT("Shaders");
	return Result;
}

/** Returns name of currently running executable. */
const TCHAR* appExecutableName()
{
	static TCHAR Result[256]=TEXT("Unknown");
#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	DM_XBE XBEInfo;
	if( SUCCEEDED(DmGetXbeInfo("",&XBEInfo)) )
	{
		FFilename ExecutableName = XBEInfo.LaunchPath;
		appStrncpy(Result,*ExecutableName.GetBaseFilename(),ARRAY_COUNT(Result)) ;
	}
#endif
	return Result;
}


/*-----------------------------------------------------------------------------
	SHA-1 functions.
-----------------------------------------------------------------------------*/

/**
 * Get the hash values out of the executable hash section
 *
 * NOTE: To put hash keys into the executable, you will need to put a line like the
 *		 following into your xbox linker settings:
 *			/XEXSECTION:HashSec=..\..\..\MyGame\CookedXenon\Hashes.sha
 *
 *		 Then, use the -sha option to the cooker (must be from commandline, not
 *       frontend) to generate the hashes for .ini, loc, and startup packages
 *
 *		 You probably will want to make and checkin an empty file called Hashses.sha
 *		 into your source control to avoid linker warnings. Then for testing or final
 *		 final build ONLY, use the -sha command and relink your executable to put the
 *		 hashes for the current files into the executable.
 */
void InitSHAHashes()
{
	void* SectionData;
	DWORD SectionSize;
	// look for the hash section
	if (XGetModuleSection(GetModuleHandle(NULL), "HashSec", &SectionData, &SectionSize))
	{
		// there may be a dummy byte for platforms that can't handle empty files for linking
		if (SectionSize <= 1)
		{
			return;
		}

		// load up the global sha maps
		FSHA1::InitializeFileHashesFromBuffer((BYTE*)SectionData, SectionSize);
	}
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
 	debugf(TEXT("SHA Verification failed for '%s'. Reason: %s"), 
 		FailedPathname ? FailedPathname : TEXT("Unknown file"),
 		bFailedDueToMissingHash ? TEXT("Missing hash") : TEXT("Bad hash"));

	// popup the dirty disc message
	XShowDirtyDiscErrorUI(0);
}

/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

/** Whether networking has been successfully initialized. Not clobbered by the memory image code.				*/
UBOOL					GIpDrvInitialized;

#include "UnThreadingWindows.h"
static FSynchronizeFactoryWin	SynchronizeFactory;
static FThreadFactoryWin		ThreadFactory;
/** Thread pool for shared async work */
static FQueuedThreadPoolWin		ThreadPool;

/** Thread pool for hi priority work */
static FQueuedThreadPoolWin		HiPriThreadPool;

QWORD GCyclesPerSecond = 1;

/**
 * Does per platform initialization of timing information and returns the current time. This function is
 * called before the execution of main as GStartTime is statically initialized by it. The function also
 * internally sets GSecondsPerCycle, which is safe to do as static initialization order enforces complex
 * initialization after the initial 0 initialization of the value.
 *
 * @return	current time
 */
DOUBLE appInitTiming()
{
	LARGE_INTEGER Frequency;
	verify( QueryPerformanceFrequency(&Frequency) );
	GCyclesPerSecond = Frequency.QuadPart;
	GSecondsPerCycle = 1.0 / Frequency.QuadPart;
	return appSeconds();
}

/**
 * Xenon specific initialization we do up- front.
 */
void appXenonInit( const TCHAR* CommandLine )
{
#if FINAL_RELEASE
	// Set file meta data cache to 128 KByte (default is 64 KByte).
	XSetFileCacheSize( 128 * 1024 );
#else
	// Development does lots of logging so make a larger buffer to reduce hitches
	XSetFileCacheSize( 1024 * 1024 );
#endif

	// copy the command line since we need it before appInit gets called
	extern TCHAR GCmdLine[16384];
	appStrncpy( GCmdLine, CommandLine, 16384 );

	//
	//	Initialiaze threading.
	// 

	// Initialiaze global threading and synchronization factories. This is done in FEngineLoop::Init on PC.
	GSynchronizeFactory			= &SynchronizeFactory;
	GThreadFactory				= &ThreadFactory;
	GThreadPool					= &ThreadPool;
	// Create the pool of threads that will be used (see UnXenon.h)
	verify(GThreadPool->Create(THREAD_POOL_COUNT,THREAD_POOL_HWTHREAD,32*1024,TPri_BelowNormal));

	GHiPriThreadPoolNumThreads = HIPRI_THREAD_POOL_COUNT;
	GHiPriThreadPool = &HiPriThreadPool;
	verify(GHiPriThreadPool->Create(GHiPriThreadPoolNumThreads,HIPRI_THREAD_POOL_HWTHREAD,32*1024,TPri_Normal));
	

	//
	//	Initialize D3D.
	//
#if USE_XeD3D_RHI
	XeInitD3DDevice();	
#endif // USE_XeD3D_RHI

	// Early init of the XAudio2 sound system for Bink audio support
	UXAudio2Device::InitHardware();

	// initialize stuff for the splash screen
	appXenonInitSplash();

	// pull SHA hashes from the executable
	InitSHAHashes();
}

void appPlatformPreInit()
{
}

void appPlatformInit()
{
	// Randomize.
	srand( 0 );
   
	// Identity.
	debugf( NAME_Init, TEXT("Computer: %s"), appComputerName() );

	// Get CPU info.
	GNumHardwareThreads = 6;

	// Load the DrawUP check counts from the configuration file
#if !FINAL_RELEASE
	if (GConfig)
	{
		INT Temporary = 0;
		if (GConfig->GetInt(TEXT("XeD3D"), TEXT("DrawUPVertexCheckCount"), Temporary, GEngineIni) == TRUE)
		{
			GDrawUPVertexCheckCount = Temporary;
		}
		if (GConfig->GetInt(TEXT("XeD3D"), TEXT("DrawUPIndexCheckCount"), Temporary, GEngineIni) == TRUE)
		{
			GDrawUPIndexCheckCount = Temporary;
		}
	}
#endif

#if USE_XeD3D_RHI
	// Check for, and set if present, RingBuffer configuration
	XeSetRingBufferParametersFromConfig();
	
	if (GConfig)
	{
		// Corrective gamma ramp seetting
		GConfig->GetBool(TEXT("XeD3D"), TEXT("bUseCorrectiveGammaRamp"), GUseCorrectiveGammaRamp, GEngineIni);
		XeSetCorrectiveGammaRamp(GUseCorrectiveGammaRamp);
	}
#endif  // USE_XeD3D_RHI

	// Initialize HDD caching.
	XeInitHDDCaching();
}

void appPlatformPostInit()
{
#if STATS
	// Read the total physical memory available, and tell the stat manager
	MEMORYSTATUS MemStatus;
	GlobalMemoryStatus(&MemStatus);
	GStatManager.SetAvailableMemory(MCR_Physical, MemStatus.dwTotalPhys);
#endif
}


#include "zlib.h"

/**
 * zlib's memory allocator, overridden to use our allocator.
 *
 * @param	Opaque		unused
 * @param	NumItems	number of items to allocate
 * @param	Size		size in bytes of a single item
 *
 * @return	pointer to allocated memory
 */
extern "C" voidpf zcalloc( voidpf* /*Opaque*/, unsigned int NumItems, unsigned int Size )
{
	return appMalloc( NumItems * Size );
}

/**
 * zlib's memory de-allocator, overridden to use our allocator.
 *
 * @param	Opaque		unused
 * @param	Pointer		pointer to block of memory to free
 */
extern "C" void zcfree( voidpf /*Opaque*/, voidpf Pointer )
{
	appFree( Pointer );
}

/** 
 * Returns the language setting that is configured for the platform 
 *
 * Known language extensions are specified in the engine.ini config files.  See baseEngine.ini for the default list
 */
FString appGetLanguageExt( void )
{
	static FString LangExt = TEXT( "" );
	if (LangExt.Len())
	{
		return LangExt;
	}

	// Get the language registered in the dash.
	const DWORD LangIndex = XGetLanguage();
	const DWORD LocaleIndex = XGetLocale();

	// The dashboard has only one Spanish language setting.  So, check the region to
	// determine whether we should use Iberian (Europe) or Latam (North America).

	FString LanguageExtension = TEXT( "INT" );
	switch( LangIndex )
	{
	case XC_LANGUAGE_ENGLISH:
		LanguageExtension = TEXT( "INT" );
		break;
	case XC_LANGUAGE_JAPANESE:
		LanguageExtension = TEXT( "JPN" );
		break;
	case XC_LANGUAGE_GERMAN:
		LanguageExtension = TEXT( "DEU" );
		break;
	case XC_LANGUAGE_FRENCH:
		LanguageExtension = TEXT( "FRA" );
		break;
	case XC_LANGUAGE_SPANISH:
		if( LocaleIndex == XC_LOCALE_SPAIN )
		{
			LanguageExtension = TEXT( "ESN" );
		}
		else
		{
			LanguageExtension = TEXT( "ESM" );
		}
		break;
	case XC_LANGUAGE_ITALIAN:
		LanguageExtension = TEXT( "ITA" );
		break;
	case XC_LANGUAGE_KOREAN:
		LanguageExtension = TEXT( "KOR" );
		break;
	case XC_LANGUAGE_SCHINESE:
	case XC_LANGUAGE_TCHINESE:
		LanguageExtension = TEXT( "CHN" );
		break;
	case XC_LANGUAGE_POLISH:
		LanguageExtension = TEXT( "POL" );
		break;
	case XC_LANGUAGE_PORTUGUESE:
		LanguageExtension = TEXT( "PTB" );
		break;
	case XC_LANGUAGE_RUSSIAN:
		LanguageExtension = TEXT( "RUS" );
		break;
	}

	// Always override with the CZE, SLO or HUN locales
	switch( LocaleIndex )
	{
	case XC_LOCALE_CZECH_REPUBLIC:
	case XC_LOCALE_SLOVAK_REPUBLIC:
		LanguageExtension = TEXT( "CZE" );
		break;
	case XC_LOCALE_HUNGARY:
		LanguageExtension = TEXT( "HUN" );
		break;
	}

	// Special case handling of Spanish
	if( LanguageExtension == TEXT( "ESN" ) || LanguageExtension == TEXT( "ESM" ) )
	{
		// If the full loc version of the requested Spanish does not exist, try the other type
		FString TOCFileName = FString::Printf( TEXT( "%sGame\\Xbox360TOC_%s.txt" ), appGetGameName(), *LanguageExtension );
		if( GFileManager->FileSize( *TOCFileName ) == -1 )
		{
			if( LanguageExtension == TEXT( "ESN" ) )
			{
				LanguageExtension = TEXT( "ESM" );
			}
			else
			{
				LanguageExtension = TEXT( "ESN" );
			}
		}
	}

	LangExt = LanguageExtension;

	// Allow for overriding the language settings via the commandline
	FString CmdLineLang;
	if (Parse(appCmdLine(), TEXT("LANGUAGE="), CmdLineLang))
	{
		warnf(NAME_Log, TEXT("Overriding lang %s w/ command-line option of %s"), *LangExt, *CmdLineLang);
		LangExt = CmdLineLang;
	}

	LangExt = LangExt.ToUpper();

	// make sure the language is one that is known (GKnownLanguages)
	if (appIsKnownLanguageExt(LangExt) == FALSE)
	{
		// default back to INT if the language wasn't known
		warnf(NAME_Warning, TEXT("Unknown language extension %s. Defaulting to INT"), *LangExt);
		LangExt = TEXT("INT");
	}

	return LangExt;
}

/*-----------------------------------------------------------------------------
	Misc
-----------------------------------------------------------------------------*/
#include "UMemoryDefines.h"

/**
* Returns the number of modules loaded by the currently running process.
*/
INT appGetProcessModuleCount()
{
	INT ModuleCount = 0;

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	PDM_WALK_MODULES WalkMod = NULL;
	DMN_MODLOAD ModLoad;

	while(XBDM_NOERR == DmWalkLoadedModules(&WalkMod, &ModLoad))
	{
		++ModuleCount;
	}

	DmCloseLoadedModules(WalkMod);
#endif

	return ModuleCount;
}

/**
* Gets the signature for every module loaded by the currently running process.
*
* @param	ModuleSignatures		An array to retrieve the module signatures.
* @param	ModuleSignaturesSize	The size of the array pointed to by ModuleSignatures.
*
* @return	The number of modules copied into ModuleSignatures
*/
INT appGetProcessModuleSignatures(FModuleInfo *ModuleSignatures, const INT ModuleSignaturesSize)
{
	INT ModuleCount = 0;

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	PDM_WALK_MODULES WalkMod = NULL;
	DMN_MODLOAD ModLoad;
	DM_PDB_SIGNATURE Signature;	
	FModuleInfo ModuleInfo;

	while(XBDM_NOERR == DmWalkLoadedModules(&WalkMod, &ModLoad))
	{
		if(SUCCEEDED(DmFindPdbSignature(ModLoad.BaseAddress, &Signature)))
		{
			appMemzero(&ModuleInfo, sizeof(ModuleInfo));
			ModuleInfo.PdbSig70 = Signature.Guid;
			//xbox will always be 32-bit and casting to DWORD64 causes the high order bytes to be filled in with 0xff
			ModuleInfo.BaseOfImage = (DWORD)ModLoad.BaseAddress;
			ModuleInfo.ImageSize = ModLoad.Size;
			ModuleInfo.PdbSig = ModLoad.CheckSum;
			ModuleInfo.TimeDateStamp = ModLoad.TimeStamp;
			ModuleInfo.PdbAge = Signature.Age;

			//NOTE: When the PDB signature is read in it is little endian. We need to manually convert it into big endian.
			ModuleInfo.PdbSig70.Data1 = BYTESWAP_ORDER32_unsigned(ModuleInfo.PdbSig70.Data1);
			ModuleInfo.PdbSig70.Data2 = BYTESWAP_ORDER16_unsigned(ModuleInfo.PdbSig70.Data2);
			ModuleInfo.PdbSig70.Data3 = BYTESWAP_ORDER16_unsigned(ModuleInfo.PdbSig70.Data3);
			ModuleInfo.PdbAge = BYTESWAP_ORDER32_unsigned(ModuleInfo.PdbAge);

			_snwprintf_s(ModuleInfo.LoadedImageName, 256, 256-1, L"%S", Signature.Path);
			_snwprintf_s(ModuleInfo.ImageName, 256, 256-1, L"%S", Signature.Path);
			_snwprintf_s(ModuleInfo.ModuleName, 32, 32-1, L"%S", ModLoad.Name);

			ModuleSignatures[ModuleCount] = ModuleInfo;

			++ModuleCount;
		}
	}

	DmCloseLoadedModules(WalkMod);
#endif

	return ModuleCount;
}

/**
* Converts the passed in program counter address to a human readable string and appends it to the passed in one.
* @warning: The code assumes that HumanReadableString is large enough to contain the information.
*
* @param	ProgramCounter			Address to look symbol information up for
* @param	HumanReadableString		String to concatenate information with
* @param	HumanReadableStringSize size of string in characters
* @param	VerbosityFlags			Bit field of requested data for output. -1 for all output.
*/ 
void appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags )
{
    if (HumanReadableString && HumanReadableStringSize > 0)
    {
        ANSICHAR TempArray[MAX_SPRINTF];
        appSprintfANSI(TempArray, "[Xbox360Callstack] 0x%.8x\n", (DWORD) ProgramCounter);
        appStrcatANSI( HumanReadableString, HumanReadableStringSize, TempArray );

        if( VerbosityFlags & VF_DISPLAY_FILENAME )
        {
            //Append the filename to the string here
        }
    }
}

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @param	Context				Optional thread context information
 * @return	Number of function pointers captured
 */
DWORD appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, void* Context)
{
	// Zero out all entries.
	appMemzero( BackTrace, sizeof(QWORD) * MaxDepth );

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
	check(sizeof(PVOID*)<=sizeof(QWORD));
	PVOID* DmBackTrace = (PVOID*) BackTrace;

	// capture the stack
	if (DmCaptureStackBackTrace(MaxDepth, DmBackTrace) ==  XBDM_NOERR)
	{
		// Expand from PVOID* to QWORD in reverse order.
		for( INT i=MaxDepth-1; i>=0; i-- )
		{
			BackTrace[i] = (QWORD) DmBackTrace[i];
		}

		// count depth
		DWORD Depth;
		for (Depth = 0; Depth < MaxDepth; Depth++)
		{
			// go until we hit a zero
			if (BackTrace[Depth] == 0)
			{
				break;
			}
		}
		return Depth;
	}
	else
	{
		return 0;
	}
#else
	return 0;
#endif
}


/** 
 * This will update the passed in FMemoryChartEntry with the platform specific data
 *
 * @param FMemoryChartEntry the struct to fill in
 **/
void appUpdateMemoryChartStats( struct FMemoryChartEntry& MemoryEntry )
{
#if DO_CHARTING
	//@todo fill in these values

	// set the memory chart data
	MemoryEntry.GPUMemUsed = 0;

	MemoryEntry.NumAllocations = 0;
	MemoryEntry.AllocationOverhead = 0;
	MemoryEntry.AllignmentWaste = 0;
#endif // DO_CHARTING
}

/**
 * Adds a PIX event
 *
 * @note: XeD3DUtil.cpp has the implementation for appBeginDrawEvent, which is identical code
 *
 * @param Color The color to draw the event as
 * @param Text The text displayed with the event
 */
void appBeginNamedEvent(const FColor& Color,const TCHAR* Text)
{
#if LINK_AGAINST_PROFILING_D3D_LIBRARIES
	PIXBeginNamedEvent(Color.DWColor(),TCHAR_TO_ANSI(Text));
#endif
}

/**
 * Ends the current PIX event
 *
 * @note: XeD3DUtil.cpp has the implementation for appEndDrawEvent, which is identical code
 */
void appEndNamedEvent(void)
{
#if LINK_AGAINST_PROFILING_D3D_LIBRARIES
	PIXEndNamedEvent();
#endif
}


/*----------------------------------------------------------------------------
	Xbox automatic HDD file caching.
----------------------------------------------------------------------------*/

/**
 * Whether the HDD file caching system is enabled or not.
 * Note: Doesn't reflect whether background caching is currently on or off.
 */
static UBOOL GIsHDDCachingEnabled = false;

/**
 * Returns whether the HDD file caching system is enabled or not.
 * Note: Doesn't reflect whether background caching is currently on or off.
 */
UBOOL XeIsHDDCachingEnabled()
{
	return GIsHDDCachingEnabled;
}

/**
 * Controls the background thread that performs HDD file caching.
 *
 * @param bEnableBackgroundCaching	If TRUE, enable background file caching. If FALSE, pause it.
 */
void XeControlHDDCaching( UBOOL bEnableBackgroundCaching )
{
	if ( GIsHDDCachingEnabled )
	{
		DWORD CacheMode = 0;
		if ( bEnableBackgroundCaching )
		{
			CacheMode |= XFILECACHE_BACKGROUND_ON;
		}
		else
		{
			CacheMode |= XFILECACHE_BACKGROUND_OFF;
		}

//@TODO: Not exactly sure what these flags do...
// 		if ( bAnyFilePriority )
// 		{
// 			CacheMode |= XFILECACHE_NORMAL_FILES;
// 		}
// 		else
// 		{
// 			CacheMode |= XFILECACHE_STARTUP_FILES;
// 		}
		XFileCacheControl( CacheMode );
	}
}

/**
 * Outputs information about the HDD file cache to the log.
 *
 * @param bViewHDDCacheFiles	Whether to list all files that are currently in the HDD cache.
 */
void XeLogHDDCacheStatus( UBOOL bViewHDDCacheFiles )
{
	if ( GIsHDDCachingEnabled )
	{
		if ( bViewHDDCacheFiles )
		{
#ifdef _DEBUG
			XFILECACHE_FIND_DATA FindData;
			HANDLE FindHandle = XFileCacheFindFirstFile( NULL, &FindData );
			UBOOL bOk = FindHandle != INVALID_HANDLE_VALUE;
			while ( bOk )
			{
				FLOAT FileSizeMB = (QWORD(FindData.nFileSizeHigh)<<32 | QWORD(FindData.nFileSizeLow)) / 1024.0f / 1024.0f;
				debugf( NAME_DevHDDCaching, TEXT("%s (%.2f MB)"), ANSI_TO_TCHAR(FindData.cFileName), FileSizeMB );
				bOk = XFileCacheFindNextFile( FindHandle, &FindData );
			}
			XFileCacheFindClose( FindHandle );
#endif
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE

			INT TotalSize = 0;
			INT NumDirs = 0;
			INT NumFiles = 0;

			// Enumerate root files
			TArray<FString> Files;
			GFileManager->FindFiles(Files, TEXT("cache:\\*.*"), TRUE, FALSE);
			NumFiles += Files.Num();
			for (INT FileIdx=0; FileIdx<Files.Num(); FileIdx++)
			{
				FString Fullpath = FString::Printf(TEXT("cache:\\%s"), *Files(FileIdx));
				INT FileSize = GFileManager->FileSize(*Fullpath);
				TotalSize += FileSize;
				debugf( NAME_DevHDDCaching, TEXT("%s Size: %d bytes"), *Fullpath, FileSize);
			}
			
			// Enumerate root subdirectories
			TArray<FString> Directories;
			GFileManager->FindFiles(Directories, TEXT("cache:\\*.*"), FALSE, TRUE);
			NumDirs += Directories.Num();
			while (Directories.Num() > 0)
			{
				FString CurrentDir = FString(TEXT("cache:\\")) + Directories(0) + PATH_SEPARATOR + TEXT("*.*");
				Files.Empty();

				// Enumerate subdirectories
				TArray<FString> Subdirectories;
				GFileManager->FindFiles(Subdirectories, *CurrentDir, FALSE, TRUE);
				NumDirs += Subdirectories.Num();

				// Print the files in the current directory
				GFileManager->FindFiles(Files, *CurrentDir, TRUE, FALSE);
				NumFiles += Files.Num();
				for (INT FileIdx=0; FileIdx<Files.Num(); FileIdx++)
				{
					FString Fullpath = FString::Printf(TEXT("cache:\\%s\\%s"), *Directories(0), *Files(FileIdx));
					INT FileSize = GFileManager->FileSize(*Fullpath);
					TotalSize += FileSize;
					debugf( NAME_DevHDDCaching, TEXT("%s Size: %d bytes"), *Fullpath, FileSize);
				}

				// Remove current directory
				Directories.Remove(0);

				// Add subdirectories to beginning (depth first)
				for (INT i=0; i<Subdirectories.Num(); i++)
				{
					Directories.InsertItem(Subdirectories(i), 0);
				}
			}

			debugf(TEXT("Total Dirs: %d Total Files: %d Total Size on Disk: %.3f MB"), NumDirs, NumFiles, (FLOAT)TotalSize / 1024.0f / 1024.f);
#endif
		}

		XFILECACHESTATS FileCacheStats;
		DWORD ReturnCode = XFileCacheGetStats( &FileCacheStats );
		if ( ReturnCode == ERROR_SUCCESS )
		{
			debugf( NAME_DevHDDCaching, TEXT("%d cached files, %.2f GB (%.2f MB copied, %.2f MB evicted, %d hits, %d misses)"),
				FileCacheStats.FileCount,
				FileCacheStats.CachedTotal / 1024.0f / 1024.0f / 1024.0f,
				FileCacheStats.CacheCopyBytes.QuadPart / 1024.0f / 1024.0f,
				FileCacheStats.CacheEvictBytes.QuadPart / 1024.0f / 1024.0f,
				FileCacheStats.CacheHitReads,
				FileCacheStats.CacheMissReads );
		}
	}
}

/**
 * Called by the Xbox file caching system when it wants to cache some bytes from a file on the game disc.
 * Periodically outputs debug log (depending on how many bytes that have been cached so far, in total).
 *
 * @param Context				User context, specified when registering the callback.
 * @param StrPth				Full path to the game disc file.
 * @param FileOffset			[opt] File offset, may be NULL.
 * @param NumberOfBytesToRead	Number of bytes the caching systems wants to cache.
 * @return						Returns XFILECACHE_READFILEADVISORY_OK to allow the caching system to cache these bytes.
 */
static DWORD XeFileCacheCallback( PVOID Context, const CHAR* StrPath, PLARGE_INTEGER FileOffset, DWORD NumberOfBytesToRead )
{
	static INT NumBytesBeforeLogging = 100*1024*1024;	// 100 MB
	NumBytesBeforeLogging -= NumberOfBytesToRead;
	if ( NumBytesBeforeLogging < 0 )
	{
		NumBytesBeforeLogging = 200*1024*1024;			// 200 MB
		FLOAT AmountCachedMB = FileOffset ? (FileOffset->QuadPart + NumberOfBytesToRead)/1024.0f/1024.0f : NumberOfBytesToRead/1024.0f/1024.0f;
		debugf( NAME_DevHDDCaching, TEXT("Cached %.1f MB from %s."), AmountCachedMB, ANSI_TO_TCHAR(StrPath) );
		XeLogHDDCacheStatus( FALSE );
	}
	return XFILECACHE_READFILEADVISORY_OK;
}

/**
 * Initializes the automatic Xbox HDD file caching system, if we're not playing from hard disk.
 * Uses the timestamp of the TOCXbox360.txt file (in seconds since Jan 1, 2000) as cache version.
 * Note: The Xbox automatically wipes the HDD cache if the cache version is not identical to last run.
 * 
 * Reads optional parameters from command-line:
 * -DisableHDDCache		Disables HDD caching. The system will not be initialized.
 * -ClearHDDCache		Clears the cache and removes all files currently on the HDD cache.
 * -ViewHDDCache		Outputs a list of all files in the HDD cache.
 */
void XeInitHDDCaching()
{
	GIsHDDCachingEnabled = FALSE;

	// Did the user pass -DisableHDDCache on the command line?
	UBOOL bDisableHDDCaching = ParseParam(appCmdLine(), TEXT("DisableHDDCache"));

	// Did the user pass -ClearHDDCache on the command line?
	UBOOL bClearHDDCache = ParseParam(appCmdLine(), TEXT("ClearHDDCache"));

	// Did the user pass -ViewHDDCache on the command line?
	UBOOL bViewHDDCache = ParseParam(appCmdLine(), TEXT("ViewHDDCache"));

	// Test whether we are being played from hard drive
	DWORD LicenseMask;
	BOOL bIsPlayedFromHardDrive = ( XContentGetLicenseMask( &LicenseMask, NULL ) == ERROR_SUCCESS );
	if ( bIsPlayedFromHardDrive )
	{
		// Turn off HDD caching
		bDisableHDDCaching = TRUE;
	}

	if ( !bDisableHDDCaching )
	{
		// Generate a file cache version ID from the timestamp of the TOC file.
		FString TOCFileName;
		if ( GUseCoderMode )
		{
			TOCFileName = FString::Printf( TEXT( "%sGame\\Xbox360TOC_CC.txt" ), appGetGameName() );
		}
		else
		{
			TOCFileName = FString::Printf( TEXT( "%sGame\\Xbox360TOC.txt" ), appGetGameName() );
		}
		FFileManager::FTimeStamp TocTimeStamp;
		DWORD FileCacheVersion = 0;	//GPackageFileVersion | (GPackageFileLicenseeVersion << 16);
		if ( GFileManager->GetTimestamp( *TOCFileName, TocTimeStamp ) )
		{
			// Approx. number of seconds since Jan 1, 2000.
			FileCacheVersion = (TocTimeStamp.Year - 2000)	* 12*31*24*60*60 +
							   TocTimeStamp.Month			* 31*24*60*60 +
							   (TocTimeStamp.Day - 1)		* 24*60*60 +
							   TocTimeStamp.Hour			* 60*60 +
							   TocTimeStamp.Minute			* 60 +
							   TocTimeStamp.Second;
		}
		else
		{
			bClearHDDCache = TRUE;
			FileCacheVersion = 0;
		}

		DWORD CacheFlags = XFILECACHE_OPPORTUNISTIC_OFF;
		if ( bClearHDDCache )
		{
			CacheFlags |= XFILECACHE_CLEAR_ALL;
		}
		else
		{
			CacheFlags |= XFILECACHE_PRESERVE;
		}
		DWORD ReturnCode = XFileCacheInit(
			CacheFlags,					// Initialization flags
			FILECACHE_CACHESIZE,		// Use full 2Gig minus some reserved for game allocations (See UnXenon.h)				
			FILECACHE_HWTHREAD,			// Hardware thread for background copying (See UnXenon.h)
			192*1024,					// Use 192 KB of scratch buffer size
			FileCacheVersion			// File cache version number
			);

		if ( ReturnCode == ERROR_SUCCESS )
		{
			// Set up callback that gets called when XFile wants to cache data.
			XFileCacheSetFileIoCallbacks( XeFileCacheCallback, NULL );
		}

		DOUBLE StartTime = appSeconds();
		const FString GameDiscPath = FString::Printf( TEXT("game:\\%sGame\\CookedXenon\\"), GGameName );
		if ( ReturnCode == ERROR_SUCCESS )
		{
			ReturnCode = XFileCachePreload( XFILECACHE_NORMAL_FILES, TCHAR_TO_ANSI(*(GameDiscPath + TEXT("*.tfc"))) );
		}
		if ( ReturnCode == ERROR_SUCCESS )
		{
			ReturnCode = XFileCachePreload( XFILECACHE_NORMAL_FILES, TCHAR_TO_ANSI(*(GameDiscPath + TEXT("GUD*.xxx"))) );
		}
		DOUBLE EndTime = appSeconds();
		DOUBLE Duration = (EndTime - StartTime) * 1000.0;

		if ( ReturnCode == ERROR_SUCCESS )
		{
			GIsHDDCachingEnabled = TRUE;

			XeLogHDDCacheStatus( bViewHDDCache );
			if ( GLog )
			{
				GLog->Flush();	// Note: Doesn't flush FBackgroundLoggingOutput (OutputDebugString background thread).
				appSleep(0.0f);
			}
		}
	}
}

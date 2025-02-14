/*=============================================================================
	UnPS3.cpp: GCC PS3 core.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sys/types.h>
#include <sys/sys_time.h>
#include <sys/timer.h>
#include <sys/memory.h>
#include <unistd.h>
#include <sys/process.h>
#include <sys/raw_spu.h>
#include <sys/spu_initialize.h>
#include <cell/fs/cell_fs_file_api.h>
#include <sys/paths.h>
#include <cell/sysmodule.h>
#include <cell/rtc.h>
#include <sysutil/sysutil_sysparam.h>

#include <cell/spurs.h>
#include <sysutil/sysutil_oskdialog.h>
#include <np.h>

// Core includes.
#include "PS3Drv.h"
#include "PS3Gcm.h"
#include "FFileManagerPS3.h"
#include "FMallocPS3.h"
#include "../../Core/Inc/CorePrivate.h"
#include "UnIpDrv.h"
#include "ChartCreation.h"


#include "PS3NetworkPlatform.h"

// Edge includes
#include "EdgeZlibWrapper.h"
#include "SpursUtilSPUPrintfService.h"

/*-----------------------------------------------------------------------------
Global variables.
-----------------------------------------------------------------------------*/

#if USE_PS3_RHI
/** Screen width  */
INT						GScreenWidth;
/** Screen height */
INT						GScreenHeight;
#endif // USE_PS3_RHI

/** Number of timebase ticks per second */
QWORD					GTicksPerSeconds = 1ull;

CellSpurs				GSPURSObject __attribute__((aligned (128)));
CellSpurs				GSPURSObject2 __attribute__((aligned (128)));

/** The 5-SPU SPURS Instance. */
CellSpurs *				GSPURS = NULL;
/** The 1-SPU SPURS Instance. */
CellSpurs *				GSPURS2 = NULL;

/** Printf handler for both spurs instances */
static SampleUtilSpursPrintfService GPrintfService;
static SampleUtilSpursPrintfService GPrintfService2;

#define SPU_THREAD_GROUP_PRIORITY	0x7f		/** Thread group priority for SPURS */

extern TCHAR GCmdLine[16384];

#if ENABLE_VECTORINTRINSICS

/** Vector that represents negative zero (-0,-0,-0,-0) */
const VectorRegister PS3ALTIVEC_NEGZERO = MakeVectorRegister( 0x80000000, 0x80000000, 0x80000000, (DWORD)0x80000000 );

/** Vector that represents (1,1,1,1) */
const VectorRegister PS3ALTIVEC_ONE = { 1.0f, 1.0f, 1.0f, 1.0f };

/** Select mask that selects Src1.XYZ and Src2.W to make (X,Y,Z,W). */
const VectorRegister PS3ALTIVEC_SELECT_MASK = MakeVectorRegister( 0x00000000, 0x00000000, 0x00000000, (DWORD)0xffffffff );

/** Swizzle mask that selects each byte in X from Src1 and the last byte of W from Src2 to make (WWWX[0],WWWX[1],WWWX[2],WWWX[3]). */
const VectorRegister	PS3ALTIVEC_SWIZZLE_MASK_UNPACK = MakeVectorRegister( 0x1f1f1f0c, 0x1f1f1f0d, 0x1f1f1f0e, (DWORD)0x1f1f1f0f );

/** Swizzle mask that selects each byte in X in reverse order from Src1 and the last byte of W from Src2 to make (WWWX[3],WWWX[2],WWWX[1],WWWX[0]). */
const VectorRegister	PS3ALTIVEC_SWIZZLE_MASK_UNPACK_REVERSE = MakeVectorRegister( 0x1f1f1f0f, 0x1f1f1f0e, 0x1f1f1f0d, (DWORD)0x1f1f1f0c );

#if !USE_PRECOMPUTED_SWIZZLE_MASKS

/** Swizzle mask to form an YZXW vector. */
const VectorRegister	PS3ALTIVEC_SWIZZLE_YZXW = ((VectorRegister) SWIZZLEMASK(1,2,0,3));

/** Swizzle mask to form a ZXYW vector. */
const VectorRegister	PS3ALTIVEC_SWIZZLE_ZXYW = ((VectorRegister) SWIZZLEMASK(2,0,1,3));

#else

#define MAKE_SWIZZLEFLOAT( C )	((DWORD)(((C)*4 + 0)<<24) | (((C)*4 + 1)<<16) | (((C)*4 + 2)<<8) | ((C)*4 + 3))
#define MAKE_SWIZZLEMASK( X, Y, Z, W )		\
	((vector unsigned char) MakeVectorRegister( MAKE_SWIZZLEFLOAT(X), MAKE_SWIZZLEFLOAT(Y), MAKE_SWIZZLEFLOAT(Z), MAKE_SWIZZLEFLOAT(W) ) )

/** Swizzle mask to form an YZXW vector. */
const VectorRegister	PS3ALTIVEC_SWIZZLE_YZXW = ((VectorRegister) MAKE_SWIZZLEMASK(1,2,0,3));

/** Swizzle mask to form a ZXYW vector. */
const VectorRegister	PS3ALTIVEC_SWIZZLE_ZXYW = ((VectorRegister) MAKE_SWIZZLEMASK(2,0,1,3));


#define SWIZZLEMASK_DEF_XYZW( X, Y, Z, W ) \
	const vector unsigned char SWIZZLEMASK_##X##Y##Z##W = MAKE_SWIZZLEMASK(X,Y,Z,W);

#define SWIZZLEMASK_DEF_W( X, Y, Z ) \
	SWIZZLEMASK_DEF_XYZW(X,Y,Z,0) \
	SWIZZLEMASK_DEF_XYZW(X,Y,Z,1) \
	SWIZZLEMASK_DEF_XYZW(X,Y,Z,2) \
	SWIZZLEMASK_DEF_XYZW(X,Y,Z,3)

#define SWIZZLEMASK_DEF_Z( X, Y ) \
	SWIZZLEMASK_DEF_W(X,Y,0) \
	SWIZZLEMASK_DEF_W(X,Y,1) \
	SWIZZLEMASK_DEF_W(X,Y,2) \
	SWIZZLEMASK_DEF_W(X,Y,3)

#define SWIZZLEMASK_DEF_Y( X ) \
	SWIZZLEMASK_DEF_Z(X,0) \
	SWIZZLEMASK_DEF_Z(X,1) \
	SWIZZLEMASK_DEF_Z(X,2) \
	SWIZZLEMASK_DEF_Z(X,3)

SWIZZLEMASK_DEF_Y(0)
SWIZZLEMASK_DEF_Y(1)
SWIZZLEMASK_DEF_Y(2)
SWIZZLEMASK_DEF_Y(3)

#endif



#endif

/*----------------------------------------------------------------------------
	Initialization.
----------------------------------------------------------------------------*/

void SysUtilCallback(uint64_t status, uint64_t param, void * userdata)
{
	if (status == CELL_SYSUTIL_REQUEST_EXITGAME)
	{
		// don't allow shutting down while saving
		FScopeLock ScopeLock(&GNoShutdownWhileSavingSection);

		printf("Requesting exit via SysUtil HUD\n");
		GIsRequestingExit = TRUE;
	}
};


UBOOL PS3InitSPU()
{
	// Initialize SPU Management.
	sys_spu_initialize(6, 0);

	// Get the priority of the current (main) thread.
	sys_ppu_thread_t ThreadID;
	int ThreadPrio;
	sys_ppu_thread_get_id(&ThreadID);
	sys_ppu_thread_get_priority(ThreadID, &ThreadPrio);

	// Initialize SPURS with slightly higher priority.
	INT ThreadPriorityPPU = Max( ThreadPrio - 1, 0 );
	{
		CellSpursAttribute attr;
		cellSpursAttributeInitialize(&attr, SPU_NUM_SPURS, SPU_THREAD_GROUP_PRIORITY, ThreadPriorityPPU, FALSE);

#if FINAL_RELEASE
		// Never paged out, so don't need the context store. Note that the context is needed for spu printf to work.
		cellSpursAttributeSetSpuThreadGroupType(&attr, SYS_SPU_THREAD_GROUP_TYPE_EXCLUSIVE_NON_CONTEXT);
#endif

		INT ReturnCode = cellSpursInitializeWithAttribute( &GSPURSObject, &attr );
		check(ReturnCode == CELL_OK);
		GSPURS = &GSPURSObject;
	}

	// Initialize second SPURS instance of 1 SPU for sharing the 6th SPU with the OS SPU Thread.
	// Second SPURS instance currently only used for Edge Zlib
	{
		CellSpursAttribute attr;
		cellSpursAttributeInitialize(&attr, 1, SPU_THREAD_GROUP_PRIORITY, ThreadPriorityPPU, FALSE);

		INT ReturnCode = cellSpursInitializeWithAttribute( &GSPURSObject2, &attr );
		check(ReturnCode == CELL_OK);
		GSPURS2 = &GSPURSObject2;
	}

	INT ReturnCode = sampleSpursUtilSpuPrintfServiceInitialize( &GPrintfService, GSPURS, ThreadPriorityPPU);
	check(ReturnCode == CELL_OK);
	ReturnCode = sampleSpursUtilSpuPrintfServiceInitialize( &GPrintfService2, GSPURS2, ThreadPriorityPPU);
	check(ReturnCode == CELL_OK);

	// Initialize Edge Zlib SPURS Taskset and Task on 1SPU SPURS instance.
	edge_zlib_set_spurs(GSPURS2);
	edge_zlib_create_taskset_and_start(1);

	return TRUE;
}

/**
 * Shutdown both spurs instances
 */
void appPS3ShutdownSPU()
{
	edge_zlib_shutdown();

	INT ReturnCode = sampleSpursUtilSpuPrintfServiceFinalize( &GPrintfService );
	ReturnCode = sampleSpursUtilSpuPrintfServiceFinalize( &GPrintfService2 );

	ReturnCode = cellSpursFinalize( &GSPURSObject );
	ReturnCode = cellSpursFinalize( &GSPURSObject2 );
}

void appPS3Init(int argc, char* const argv[])
{
	// Load the PRX modules we use.
	INT PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_FS );
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SPURS );
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL );
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_RTC );

#if !FINAL_RELEASE 
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SHEAP );
#else
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_GCM_SYS );
#endif

	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_AUDIO );
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_IO );

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// for exception handling on test/devkits
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_LV2DBG );
#endif

	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_SAVEDATA );
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_GAME );

	// always load NP and trophies since they are TRC requirements
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_NP );
	PrxReturnCode = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_NP_TROPHY );

	// print out how much free memory we have
	sys_memory_info_t MemInfo;
	sys_memory_get_user_memory_size(&MemInfo);
	printf("------------------------------------------------------------------------\n");
	printf("System has %d megabytes of memory\n", MemInfo.total_user_memory / (1024 * 1024));
	printf("%.2f megs were free at startup\n", (FLOAT)GPS3MemoryAvailableAtBoot / (1024.0f * 1024.0f));
	if (MemInfo.total_user_memory > 256 * 1024 * 1024)
	{
		printf("Running in devkit mode. Free memory is not indicative of retail console!\n");
	}
	printf("------------------------------------------------------------------------\n");


	// We need the commandline before the FileManager and FPS3Gcm is initialized:
	printf("Commandline init...\n");

	// initialize commandline to empty
	GCmdLine[0] = 0;

	FFileManagerPS3::StaticInit();

	if (argc > 1)
	{
		// Build up the command line to pass to the game, skipping over argv[0] which is the executable name
		for (INT Option = 1; Option < argc; Option++)
		{
			appStrcat(GCmdLine, ANSI_TO_TCHAR(argv[Option]));
			appStrcat(GCmdLine, TEXT(" "));
		}
	}
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else
	{
		// only look on host for this file (we don't haven't set up FIOS yet, and we need the commandline
		// to know how to set up FIOS!)
		const ANSICHAR* CmdLineName = DRIVE_APP "/Binaries/PS3/PS3CommandLine.txt";
		CellFsStat FileStat;
		if (cellFsStat(CmdLineName, &FileStat) == CELL_FS_SUCCEEDED)
		{
			INT CmdLineFile;
			// open the file
			if (cellFsOpen(CmdLineName, CELL_FS_O_RDONLY, &CmdLineFile, NULL, 0) == CELL_FS_SUCCEEDED)
			{
				// allocate space
				ANSICHAR* FileContents = (ANSICHAR*)appMalloc((DWORD)FileStat.st_size + 1);

				// read in the file
				QWORD NumRead;
				INT Error = cellFsRead(CmdLineFile, FileContents, FileStat.st_size, &NumRead);
				checkf(Error == CELL_FS_SUCCEEDED, TEXT("Read of PS3CommandLine.txt failed"));
				cellFsClose(CmdLineFile);

				// process it
				FileContents[FileStat.st_size] = 0;
				ANSICHAR* NewLine = strchr(FileContents, '\n');
				if (NewLine)
				{
					*NewLine = 0;
				}
				NewLine = strchr(FileContents, '\r');
				if (NewLine)
				{
					*NewLine = 0;
				}
				NewLine = strchr(FileContents, 10);
				if (NewLine)
				{
					*NewLine = 0;
				}
				NewLine = strchr(FileContents, 10);
				if (NewLine)
				{
					*NewLine = 0;
				}
				
				appStrcpy(GCmdLine, ANSI_TO_TCHAR(FileContents));

				// free temp mem
				appFree(FileContents);
			}
			else if (FileStat.st_size > 0)
			{
				printf("Failed to open command line file, but it exists!\n");
			}
		}
	}
#endif

#if PS3_DEMO_MODE
	appStrcat(GCmdLine, TEXT("-exec=../Binaries/PS3/demoexec.txt"));
#endif

	printf("CommandLine: %s\n", TCHAR_TO_ANSI(GCmdLine));

	// Do early graphics initialization by instancing the global gcm manager
	printf("Gcm/RHI Init...\n");
#if USE_PS3_RHI
	GPS3Gcm = new FPS3Gcm;
#endif // USE_PS3_RHI

	printf("Completed\n");

	// register a sysutil callback function
	cellSysutilRegisterCallback(0, SysUtilCallback, NULL);

	// start GCM flipping so that any error messages can be displayed with sysutil
	appPS3StartFlipPumperThread();

	printf("SPU init...\n");
	PS3InitSPU();

	// the controllers use the SPU (for Move), so this must happen after PS3InitSPU
	FPS3Controller::StaticInit();
	FPS3Viewport::StaticInit();


#if 0
	printf("Testing wide char functions\n");
	wprintf(TEXT("wprintf passed\n"));
	printf(TCHAR_TO_ANSI(TEXT("TCHAR_TO_ANSI passed\n")));
	TCHAR Buffer1[1024];
	TCHAR Buffer2[1024];

	wcscpy(Buffer1, TEXT("wcscpy passed\n"));
	printf(TCHAR_TO_ANSI(Buffer1));
	printf("wcslen passed? %d == 15\n", wcslen(Buffer1));
	swprintf(Buffer2, 1024, TEXT("wsprintf passed iff 5 == %d && Hi == %s\n"), 5, TEXT("Hi"));
	printf("\n--1\n%s\n--1\n", TCHAR_TO_ANSI(Buffer2));
	swprintf(Buffer2, 1024, TEXT("wsprintf with %%ls passed iff 5 == %d && Hi == %ls\n"), 5, TEXT("Hi"));
	printf("\n--2\n%s\n--\n", TCHAR_TO_ANSI(Buffer2));
	swprintf(Buffer2, 1024, TEXT("wsprintf with %%S passed iff 5 == %d && Hi == %S\n"), 5, TEXT("Hi"));
	printf("\n--3\n%s\n--3\n", TCHAR_TO_ANSI(Buffer2));
#endif
}

/**
 * @return TRUE if the user has chosen to use Circle button as the accept button
 */
UBOOL appPS3UseCircleToAccept()
{
	// default to X (US standard)
	INT ButtonAssignment = CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS;
	cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &ButtonAssignment);

	// return if the user has chosen circle
	return ButtonAssignment == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE;
}

/*-----------------------------------------------------------------------------
Global functions.
-----------------------------------------------------------------------------*/
#include "UMemoryDefines.h"

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

void appPlatformTick( FLOAT DeltaTime )
{
}

//
// Sends the message to the debugging output.
//
void appOutputDebugString( const TCHAR *Message )
{
	// PS3 wprintf doesn't work now, so we have to use printf :(
	printf( "%s", TCHAR_TO_ANSI(Message) );

#if WITH_UE3_NETWORKING
	if ( GDebugChannel )
	{
		GDebugChannel->SendText( Message );
	}
#endif	//#if WITH_UE3_NETWORKING
}


/** Sends a message to a remote tool. */
void appSendNotificationString( const ANSICHAR *Message )
{
#if WITH_UE3_NETWORKING
	if ( GDebugChannel )
	{
		GDebugChannel->SendText( ANSI_TO_TCHAR( Message ) );
	}
#endif	//#if WITH_UE3_NETWORKING
}

/** Sends a message to a remote tool. */
void appSendNotificationString( const TCHAR* Message )
{
	appSendNotificationString(TCHAR_TO_ANSI(Message));
}

void appRequestExit( UBOOL Force )
{
	debugf( TEXT("appRequestExit(%i)"), Force );
	if( Force )
	{
        appPS3QuitToSystem();
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
	}
}

//
// Show callstack.
//
void PS3Callstack(FThreadContext* Context /*= NULL*/)
{
    static const INT MAX_BACKTRACE_DEPTH = 32;
	QWORD Backtrace[ MAX_BACKTRACE_DEPTH ];
	DWORD Depth = appCaptureStackBackTrace( Backtrace, MAX_BACKTRACE_DEPTH, Context );

    const INT HumanReadableStringSize = 64;
    ANSICHAR HumanReadableString[HumanReadableStringSize];
	for ( INT StackFrame=0; StackFrame < Depth; ++StackFrame )
	{
		HumanReadableString[0] = 0;
        appProgramCounterToHumanReadableString( Backtrace[StackFrame], HumanReadableString, HumanReadableStringSize, VF_DISPLAY_FILENAME );
        printf("%s", HumanReadableString);
	}
	printf( "[PS3Callstack] 0x00000000\n" );
	fflush(stdout);
	appSleep(1.0f);	// Make sure all the TTY output makes it before the OS reports the crash
}

/**
 * Returns the number of modules loaded by the currently running process.
 */
INT appGetProcessModuleCount()
{
	return 0;
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
	return 0;
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
        appSprintfANSI(TempArray, "[PS3Callstack] 0x%.8x\n", (DWORD) ProgramCounter);
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
DWORD appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, FThreadContext* Context /*= NULL*/ )
{
	// call a dummy function so this is not treated as a leaf function
	extern INT PS3DummyFunction();
	PS3DummyFunction();

	QWORD* StackFramePtr;
	if (Context)
	{
		// get the frame address fmor the thread context
		StackFramePtr = (QWORD*)(PTRINT)(Context->gpr[1]);
	}
	else
	{
#if PS3_SNC
		StackFramePtr = (QWORD *)__builtin_frame_address();
#else
		StackFramePtr = (QWORD *)__builtin_frame_address(0);
#endif
	}

	// Find the innermost scoped debug info for this thread.
	FScopedDebugInfo* CurrentDebugInfo = FScopedDebugInfo::GetDebugInfoStack();

	// Skip the first two stack frames
// 	for ( INT SkipFrames=0; SkipFrames < 1 && StackFramePtr && *StackFramePtr; SkipFrames++ )
// 	{
// 		StackFramePtr = (QWORD*) *StackFramePtr;
// 	}

	DWORD StackFrames = 0;
	while ( StackFrames < MaxDepth && StackFramePtr && *StackFramePtr )
	{
		QWORD ReturnAddress = *(StackFramePtr + 2);
		if ( !(ReturnAddress & 0xffffffff00000000ull) && ReturnAddress )	// Valid return address?
		{
			// Instead of capturing the return address, capture the instruction BEFORE
			// the return address. This will more likely result in a symbol translation that matches
			// the call site of the stack frame. For instance, in optimized builds, the next instruction
			// could very well be no where near the call site due to reordering. 
			// FWIW, this makes the callstack match what the PS3 outputs itself.
			BackTrace[StackFrames++] = ReturnAddress-4;
		}
		StackFramePtr = (QWORD*)(PTRINT) *StackFramePtr;
	}

	// Zero out remaining entries.
	for( INT i=StackFrames; i<MaxDepth; i++ )
	{
		BackTrace[i] = 0;
	}

	return StackFrames;
}


//
// Sleep this thread for Seconds, 0.0 means release the current
// timeslice to let other threads get some attention.
//
void appSleep( FLOAT Seconds )
{
	usecond_t MicroSeconds = usecond_t(Seconds * 1000000.0f);
	if ( MicroSeconds < 30 )
	{
		MicroSeconds = 30;
	}
	sys_timer_usleep(MicroSeconds);
}

/**
* Sleeps forever. This function does not return!
*/
void appSleepInfinite()
{
	sys_timer_sleep((second_t)FLT_MAX);
}

//
// Return the system time.
//
void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	CellRtcDateTime Clock;
	
	// get real time clock values
	cellRtcGetCurrentClockLocalTime(&Clock);

	Year = Clock.year;
	Month = Clock.month;
	Day = Clock.day;
	Hour = Clock.hour;
	Min = Clock.minute;
	Sec = Clock.second;
	MSec = Clock.microsecond;

	// calculate day of week
	DayOfWeek = cellRtcGetDayOfWeek(Year, Month, Day);
}

//
// Return the UTC time.
//
void appUtcTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	CellRtcDateTime Clock;

	// get real time clock values
	cellRtcGetCurrentClockUtc(&Clock);

	Year = Clock.year;
	Month = Clock.month;
	Day = Clock.day;
	Hour = Clock.hour;
	Min = Clock.minute;
	Sec = Clock.second;
	MSec = Clock.microsecond;

	// calculate day of week
	DayOfWeek = cellRtcGetDayOfWeek(Year, Month, Day);
}

// Create a new globally unique identifier.
FGuid appCreateGuid()
{
	INT Year=0, Month=0, DayOfWeek=0, Day=0, Hour=0, Min=0, Sec=0, MSec=0;
	appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );

	FGuid	GUID;
	GUID.A = Day | (Hour << 16);
	GUID.B = Month | (Sec << 16);
	GUID.C = MSec | (Min << 16);
	GUID.D = Year ^ appCycles();
	return GUID;
}

// Get startup directory.
const TCHAR* appBaseDir()
{
	static TCHAR Result[256]=TEXT("");
	return Result;
}

// shader dir relative to appBaseDir
const TCHAR* appShaderDir()
{
	static TCHAR Result[256] = TEXT("..") PATH_SEPARATOR TEXT("Engine") PATH_SEPARATOR TEXT("Shaders");
	return Result;
}

// Get computer name.
const TCHAR* appComputerName()
{
#if WITH_UE3_NETWORKING
	extern TCHAR GHostName[];
	return GHostName;
#else	//#if WITH_UE3_NETWORKING
	return TEXT("");
#endif	//#if WITH_UE3_NETWORKING
}

// Get user name.
const TCHAR* appUserName()
{
	return TEXT("PS3User");
}

void appPlatformPreInit()
{
}

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
	GTicksPerSeconds = sys_time_get_timebase_frequency();
	GSecondsPerCycle = 1.0f / GTicksPerSeconds;
	return appSeconds();
}

void appPlatformInit()
{
	debugf( TEXT("appPlatformInit starting... [GameName = %s]\n"), appGetGameName() );

#if USE_PS3_RHI
	// if the game has any startup movies, then don't show the loading screen
	FString SomeStartupMovie;
	UBOOL bHasStartupMovies = GConfig->GetString(TEXT("FullScreenMovie"), TEXT("StartupMovies"), SomeStartupMovie, GEngineIni);

	// try to find a splash screen if there aren't any movies
	if (GPS3Gcm && !bHasStartupMovies)
	{
		FString SplashPath;
		if (appGetSplashPath(TEXT("PS3\\Splash.bmp"), SplashPath) == TRUE)
		{
			// show the loading screen nice and early
			GPS3Gcm->DrawLoadingScreen(*SplashPath);
		}
	}
#endif

	// Randomize.
	if( GIsBenchmarking )
	{
		srand(0);
	}
	else
	{
		srand((DWORD)sys_time_get_system_time());
	}

	// Identity.
	debugf( NAME_Init, TEXT("Computer: %s"), appComputerName() );
	debugf( NAME_Init, TEXT("User: %s"), appUserName() );

	// Use dual-thread rendering.
	GNumHardwareThreads = 2;

	// Timer resolution.
	debugf( NAME_Init, TEXT("High frequency timer resolution: %f MHz"), 0.000001 / GSecondsPerCycle); 

	debugf( TEXT("appPlatformInit ending...") );

}

void appPlatformPostInit()
{
	debugf( TEXT("appPlatformPostInit starting...") );

	// the flip pumper can interfere with rendering resource init
	appPS3StopFlipPumperThread();

#if USE_PS3_RHI

	// support booting without graphics
	if (GPS3Gcm)
	{
		// initialize rhi after app has been brought up
		PS3PostAppInitRHIInitialization();

#if STATS
		// get size of all regions
		FMemoryAllocationStats MemStats;
		GMalloc->GetAllocationInfo(MemStats);

		GStatManager.SetAvailableMemory(MCR_Physical, MemStats.CPUUsed + MemStats.CPUSlack + MemStats.CPUWaste + MemStats.HostUsed + MemStats.HostSlack + MemStats.HostWaste);
		GStatManager.SetAvailableMemory(MCR_GPU, MemStats.GPUUsed + MemStats.GPUWaste + MemStats.GPUSlack);
		GStatManager.SetAvailableMemory(MCR_GPUSystem, MemStats.HostUsed + MemStats.HostSlack + MemStats.HostWaste);
#endif
	}

	// start up the SPURS jobs
	extern void PS3LoadSPUJobs();
	PS3LoadSPUJobs();

#endif

}

// wide printf routines want %ls instead of %s for wide string params. It assumes %s is a ANSICHAR*
#define VSNPRINTF(msg, len, fmt, ap) vswprintf(msg, len, *FString(fmt).Replace(TEXT("%s"), TEXT("%ls")), ap);
#define VSNPRINTFA vsnprintf

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

/*-----------------------------------------------------------------------------
	Unimplemented dummies.
-----------------------------------------------------------------------------*/

void appClipboardCopy( const TCHAR* Str )
{
}
FString appClipboardPaste()
{
	return TEXT("");
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
	appSprintf(OutBuffer,TEXT("appGetSystemErrorMessage not supported.  Error: %d"),Error);
	warnf(OutBuffer);
	return OutBuffer;
}

void* appGetDllHandle( const TCHAR* Filename )
{
	return NULL;
}
void appFreeDllHandle( void* DllHandle )
{
}
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	return NULL;
}
void appDebugMessagef( const TCHAR* Fmt, ... )
{
	appErrorf(TEXT("Not supported"));
}
VARARG_BODY( UBOOL, appMsgf, const TCHAR*, VARARG_EXTRA(EAppMsgType Type) )
{
	TCHAR TempStr[4096]=TEXT("");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
	SET_WARN_COLOR(COLOR_RED);
	warnf(TEXT("MESSAGE: %s"), TempStr);
	CLEAR_WARN_COLOR();
	return 1;
}
void appGetLastError( void )
{
	appErrorf(TEXT("Not supported"));
}
void EdClearLoadErrors()
{
	appErrorf(TEXT("Not supported"));
}
VARARG_BODY( void VARARGS, EdLoadErrorf, const TCHAR*, VARARG_EXTRA(INT Type) )
{
	appErrorf(TEXT("Not supported"));
}
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	appErrorf(TEXT("appLaunchURL not supported"));
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
void *appCreateProc( const TCHAR* URL, const TCHAR* Parms, UBOOL bLaunchDetached, UBOOL bLaunchHidden, UBOOL bLaunchReallyHidden, DWORD* OutProcessID, INT PriorityModifier )
{
	appErrorf(TEXT("appCreateProc not supported"));
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
	appErrorf(TEXT("appGetProcReturnCode not supported"));
	return false;
}
/** Returns TRUE if the specified application is running */
UBOOL appIsApplicationRunning( DWORD ProcessId )
{
	appErrorf(TEXT("appIsApplicationRunning not implemented."));
	return FALSE;
}


/** If the game crashed, we need to clean the cache at bootup, just in case */
UBOOL GNeedsClearAutodownloadCache = FALSE;

void appCleanFileCache()
{
	// do standard cache cleanup
	GSys->PerformPeriodicCacheCleanup(GNeedsClearAutodownloadCache);

	GNeedsClearAutodownloadCache = FALSE;

	// perform any other platform-specific cleanup here
}

/**
 * Prints out detailed memory info (slow)
 *
 * @param Ar Device to send output to
 */
void appPS3DumpDetailedMemoryInformation(FOutputDevice& Ar)
{
	// steal some global vars so we can print them out
	extern INT GPS3MemoryAvailableAtBoot;
	extern INT GPS3PRXMemoryMegs;

	// get the default allocation info (this is slow, it tells us exactly how much has been allocated by malloc)
	FMemoryAllocationStats MemStats;
	GMalloc->GetAllocationInfo(MemStats);

	// print out system memory stats
	Ar.Logf(TEXT("System memory total:           %6.2f MByte (%i Bytes)"), ( MemStats.OSReportedUsed + MemStats.OSReportedFree ) / 1024.f / 1024.f, ( MemStats.OSReportedUsed + MemStats.OSReportedFree ));
	Ar.Logf(TEXT("System memory available:       %6.2f MByte (%i Bytes)"), MemStats.OSReportedFree / 1024.f / 1024.f, MemStats.OSReportedFree);
	Ar.Logf(TEXT(""));
	Ar.Logf(TEXT("  Used by elf/statics:         %6.2f MByte (%i Bytes)"), MemStats.ImageSize / 1024.f / 1024.f, MemStats.ImageSize);
	Ar.Logf(TEXT("  Used by PRXs/threads:        %6.2f MByte (%i Bytes)"), (FLOAT)GPS3PRXMemoryMegs, GPS3PRXMemoryMegs * 1024 * 1024);
	Ar.Logf(TEXT(""));
	Ar.Logf(TEXT("  Total allocated from OS:     %6.2f MByte (%i Bytes)"), MemStats.TotalAllocated / 1024.f / 1024.f, MemStats.TotalAllocated);
	Ar.Logf(TEXT("  Total allocated to game:     %6.2f MByte (%i Bytes)"), MemStats.TotalUsed / 1024.f / 1024.f, MemStats.TotalUsed);
	Ar.Logf(TEXT(""));
	// print out GPU memory info
	Ar.Logf(TEXT("GPU memory info:               %6.2f MByte total"), ( MemStats.GPUUsed + MemStats.GPUSlack ) / 1024.f / 1024.f);
	Ar.Logf(TEXT("  Total allocated:             %6.2f MByte (%i Bytes)"), MemStats.GPUUsed / 1024.f / 1024.f, MemStats.GPUUsed);
#if !USE_NULL_RHI
	Ar.Logf(TEXT("  GPU mem bookkeeping:         %6.2f KByte"), MemStats.GPUWaste / 1024.f);
#endif
	Ar.Logf(TEXT(""));
	// print out host memory info
	Ar.Logf(TEXT("Host memory info:              %6.2f MByte total"),  ( MemStats.HostUsed + MemStats.HostSlack ) / 1024.f / 1024.f);
	Ar.Logf(TEXT("  Total allocated:             %6.2f MByte (%i Bytes)"), MemStats.HostUsed / 1024.f / 1024.f, MemStats.HostUsed);
	Ar.Logf(TEXT(""));
	Ar.Logf(TEXT("Misc allocation info:"));
	Ar.Logf(TEXT(""));

	// get additional info from allocator
	GMalloc->Exec(TEXT("DUMPINFO"),Ar);
}


void UpdateMemStats()
{
#if USE_PS3_RHI
	FMemoryAllocationStats MemStats;
	GMalloc->GetAllocationInfo(MemStats);

	SET_DWORD_STAT(STAT_PhysicalAllocSize, MemStats.CPUUsed);
	SET_DWORD_STAT(STAT_GPUAllocSize, MemStats.GPUUsed);
	SET_DWORD_STAT(STAT_HostAllocSize, MemStats.HostUsed);

	GPS3Gcm->UpdateMemStats();
#endif // USE_PS3_RHI
}


/** 
 * This will update the passed in FMemoryChartEntry with the platform specific data
 *
 * @param FMemoryChartEntry the struct to fill in
 **/
void appUpdateMemoryChartStats( struct FMemoryChartEntry& MemoryEntry )
{
#if DO_CHARTING
#if USE_PS3_RHI
#if STATS
	// get some info by region
	FMemoryAllocationStats MemStats;
	GMalloc->GetAllocationInfo(MemStats);

	// set the memory chart data
	MemoryEntry.PhysicalMemUsed = MemStats.CPUUsed;
	MemoryEntry.GPUMemUsed = MemStats.GPUUsed;

	MemoryEntry.NumAllocations = 0;
	MemoryEntry.AllocationOverhead = 0;
	MemoryEntry.AllignmentWaste = MemStats.CPUWaste + MemStats.GPUWaste;

	// ps3 doesn't have virtual memory
	MemoryEntry.VirtualMemUsed = 0;
#endif // STATS
#endif // USE_PS3_RHI
#endif // DO_CHARTING
}

static ANSICHAR Remapper[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 
	0x60, 'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',  
	'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z',  0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

int std::strcasecmp(const char *a, const char *b)
{
	// walk the strings, comparing them case insensitively
	for (; *a || *b; a++, b++)
	{
		TCHAR A = Remapper[(unsigned char)*a], B = Remapper[(unsigned char)*b];
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

int std::strncasecmp(const char *a, const char *b, size_t size)
{
	// walk the strings, comparing them case insensitively, up to a max size
	for (; (*a || *b) && size; a++, b++, size--)
	{
		TCHAR A = Remapper[(unsigned char)*a], B = Remapper[(unsigned char)*b];
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

/*-----------------------------------------------------------------------------
	Replacement library functions
-----------------------------------------------------------------------------*/
int wgccstrcasecmp(const TCHAR *a, const TCHAR *b)
{
	// walk the strings, comparing them case insensitively
	for (; *a || *b; a++, b++)
	{
		TCHAR A = towupper(*a), B = towupper(*b);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

int wgccstrncasecmp(const TCHAR *a, const TCHAR *b, size_t size)
{
	// walk the strings, comparing them case insensitively, up to a max size
	for (; (*a || *b) && size; a++, b++, size--)
	{
		TCHAR A = towupper(*a), B = towupper(*b);
		if (A != B)
		{
			return A - B;
		}
	}
	return 0;
}

/** Returns the language setting that is configured for the platform */
FString appGetLanguageExt(void)
{
	static FString LangExt = TEXT("");
	if (LangExt.Len())
	{
		return LangExt;
	}

	// default to int the default extension
	LangExt = TEXT("INT");

	INT LanguageCode;
	if (cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_LANG, &LanguageCode) == CELL_OK)
	{
		// These are from the ISO standard 639-2 which can be found at:
		// http://www.loc.gov/standards/iso639-2/englangn.html
		TCHAR* LanguageExtensions[] = 
		{
			TEXT("JPN"),
			TEXT("INT"),
			TEXT("FRA"),
			TEXT("ESN"), // @todo: this is north american spanish, Latam. Support region for Iberian, non-NA spanish?
			TEXT("DEU"),
			TEXT("ITA"),
			TEXT("DUT"),
			TEXT("POR"),
			TEXT("RUS"),
			TEXT("KOR"),
			TEXT("CHT"), // @todo: these 2 aren't standard, need to get standards for traditional vs simplified chinese
			TEXT("CHN"), // @todo: 
			TEXT("FIN"),
			TEXT("SWE"),
			TEXT("DAN"),
			TEXT("NOR"),
		};

		// lookup the language extension by the language code, assuming the table has all of the languages
		if (LanguageCode < ARRAY_COUNT(LanguageExtensions))
		{
			LangExt = LanguageExtensions[LanguageCode];
		}
	}

	// Allow for overriding the language settings via the commandline
	FString CmdLineLang;
	if (Parse(appCmdLine(), TEXT("LANGUAGE="), CmdLineLang))
	{
		// warnf(NAME_Log, TEXT("Overriding lang %s w/ command-line option of %s"), *LangExt, *CmdLineLang);
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
	On screen keyboard functions
-----------------------------------------------------------------------------*/


/** Possible states for the OSK to be in */
enum EKeyboardState
{
	KS_Inactive,	// Keyboard is completely inactive, no memory is allocated to it
	KS_Showing,		// Currently onscreen and active
	KS_Complete,	// User has pressed accept or cancel
	KS_Closing,		// OSK is currently shutting down, memory is still allocated
	KS_Closed,		// OSK has closed down, okay to free memory
};

/** Current keyboard state */
EKeyboardState GKeyboardState = KS_Inactive;

/** Cached string result from the OSK */
FString GKeyboardResult;

/** Was the keyboard canceled? (Empty string could mean empty input or canceled input) */
UBOOL GKeyboardWasCanceled = FALSE;


/**
 * Standard sysutil callback handler, handles only the OSK messages
 */
static void KeyboardCallback(uint64_t Status, uint64_t Param, void* Userdata)
{
	switch(Status)
	{
		// user is done
		case CELL_SYSUTIL_OSKDIALOG_FINISHED:
			GKeyboardState = KS_Complete;
			break;
		
		// user canceled
		case CELL_SYSUTIL_OSKDIALOG_INPUT_CANCELED:
			cellOskDialogAbort();
			break;

		// check if user started using a keyboard
		case CELL_SYSUTIL_OSKDIALOG_INPUT_DEVICE_CHANGED:
			if(Param == CELL_OSKDIALOG_INPUT_DEVICE_KEYBOARD )
			{
				// If the input device becomes the keyboard, stop receiving input from the onscreen keyboard dialog
				cellOskDialogSetDeviceMask( CELL_OSKDIALOG_DEVICE_MASK_PAD );
			}
			break;

		case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
			GKeyboardState = KS_Closed;
			break;

		default:
			break;
	}
}

/**
 * Displays the async on screen keyboard. Must be ticked to close and get string.
 *
 * @param Prompt String to display to user describing what they are entering
 * @param InitialText Default string displayed in the keyboard
 * @param MaxLength Max length of the string the user can enter
 * @param bIsPassword If true, use a password keyboard (shows *'s instead of letters)
 * 
 * @return TRUE if the keyboard was successfully shown
 */
UBOOL appPS3ShowVirtualKeyboard(const TCHAR* Prompt, const TCHAR* InitialText, INT MaxLength, UBOOL bIsPassword)
{
	// fill out the string info structure
	CellOskDialogInputFieldInfo InputFieldInfo;
	InputFieldInfo.message = (uint16_t*)Prompt;
	InputFieldInfo.init_text = (uint16_t*)InitialText;
	InputFieldInfo.limit_length = Min(MaxLength, CELL_OSKDIALOG_STRING_SIZE);

	// center the keyboard
	cellOskDialogSetLayoutMode(CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER | CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_CENTER);

	// set the keyboard layout desired
	cellOskDialogSetKeyLayoutOption(CELL_OSKDIALOG_10KEY_PANEL | CELL_OSKDIALOG_FULLKEY_PANEL);
	cellOskDialogSetInitialKeyLayout(CELL_OSKDIALOG_INITIAL_PANEL_LAYOUT_FULLKEY);

	// fill out dialog info structure
	CellOskDialogParam DialogParam;
	DialogParam.allowOskPanelFlg = (bIsPassword ? CELL_OSKDIALOG_PANELMODE_PASSWORD : CELL_OSKDIALOG_PANELMODE_DEFAULT);
	DialogParam.firstViewPanel = CELL_OSKDIALOG_PANELMODE_DEFAULT;
	DialogParam.controlPoint.x = 0;
	DialogParam.controlPoint.y = 0;
	DialogParam.prohibitFlgs = CELL_OSKDIALOG_NO_RETURN;

	// show dialog and return success
	cellSysutilRegisterCallback(1, KeyboardCallback, NULL);
	INT Return = cellOskDialogLoadAsync(SYS_MEMORY_CONTAINER_ID_INVALID, &DialogParam, &InputFieldInfo) == 0;

	// were we successful?
	if (Return)
	{
		GKeyboardState = KS_Showing;
	}

	return Return != FALSE;
}

/**
 * Pumps the on screen keyboard, and it TRUE, the result is ready to go, and the keyboard
 * has closed. Cancel on the keyboard will return an empty string.
 * @todo Handle cancel as a special case (ON ALL PLATFORMS!)
 *
 * @param Result Returns the string entered when the keyboard is closed
 * @param bWasCanceled whether the user canceled the input or not
 * 
 * @return TRUE if the keyboard was closed, and Result is ready for use
 */
UBOOL appPS3TickVirtualKeyboard(FString& Result,UBOOL& bWasCanceled)
{
	switch (GKeyboardState)
	{
		case KS_Inactive:
			break;

		case KS_Showing:
		case KS_Closing:
			// pump the callbacks
			cellSysutilCheckCallback();
			break;

		case KS_Complete:
			{
				// prepare to get result
				uint16_t ResultString[CELL_OSKDIALOG_STRING_SIZE + 1];
				CellOskDialogCallbackReturnParam OutputInfo;
				OutputInfo.numCharsResultString = CELL_OSKDIALOG_STRING_SIZE;
				OutputInfo.pResultString = ResultString;

				// unload keyboard and get string
				cellOskDialogUnloadAsync(&OutputInfo);
				GKeyboardState = KS_Closing;

				// get result and save off
				switch (OutputInfo.result)
				{
					case CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK:
						GKeyboardResult = (TCHAR*)OutputInfo.pResultString;
						GKeyboardWasCanceled = FALSE;
						break;

					case CELL_OSKDIALOG_INPUT_FIELD_RESULT_CANCELED:
					case CELL_OSKDIALOG_INPUT_FIELD_RESULT_ABORT:
					case CELL_OSKDIALOG_INPUT_FIELD_RESULT_NO_INPUT_TEXT:
						GKeyboardResult = TEXT("");
						GKeyboardWasCanceled = TRUE;
						break;

				};
			}
			break;

		case KS_Closed:
			// stop callbacks
			cellSysutilUnregisterCallback(1);

			// return the output
			Result = GKeyboardResult;
			bWasCanceled = GKeyboardWasCanceled;

			// mark keyboard inactive
			GKeyboardState = KS_Inactive;

			return TRUE;
	}

	return FALSE;
}


// Pretranslated messages that are needed potentially before the .int files are read in
// These arrays is parallel to the GKnownLanguageExtensions array
const TCHAR* GEarlyTranslatedMessages[][ELST_MAX] = 
{
	// int
	{
	TEXT("Cannot continue without enough HDD space. Please quit, delete other save games, game data, or other content, and try again."),
	TEXT("The game data is irreparably damaged. Please quit, delete the game data, and run again."),
	TEXT("For optimal performance, %d more MB of HDD space is needed. You may continue playing, or quit now to free up the space."),
	// @todo ps3 fios: It would be nice to have this translated to other languages
	TEXT("There has been a disc read error. The game cannot continue. Please quit and restart the game."),
	TEXT("Unreal Tournament ® 3"),
	TEXT("Installing required game data. Please do not turn off the PLAYSTATION®3 system."),
	TEXT("It appears that the game either lost power or crashed, potentially due to user created content. Would you like to disable user created content? (You can still delete and reenable them in the My Content menu)."),
	},

	// jpn
	{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	},

	// deu
	{
	TEXT("Ohne ausreichenden Speicherplatz können Sie nicht fortfahren. Bitte beende das Spiel, lösche andere gespeicherte Spiele, Spieldaten oder andere Inhalte, und versuche es erneut."),
	TEXT("Die Spieldaten sind beschädigt. Bitte beende das Spiel, lösche die Spieldaten und starte neu."),
	TEXT("Für optimale Leistung, werden %d mehr MB Festplatten-speicherplatz benötigt. Sie können fortfahren, oder das Spiel jetzt beenden, Speicherplatz freimachen, und neustarten."),
	TEXT("Die Disc wurde ausgeworfen. Das Spiel kann nicht fortgesetzt werden. Bitte beende das Spiel und starte neu."),
	TEXT("Unreal Tournament ® 3"),
	TEXT("Installiere benötigte Spieldaten. Bitte das PLAYSTATION®3-System nicht ausschalten."),
	TEXT("Das Spiel ist anscheinend abgestürzt, normalerweise is dies auf Benutzterdefinierter-Inhalt zurückzuführen. Möchten sie Benutzterdefinierter-Inhalt ausschalten? (Sie können sie trotzdem noch löschen und wieder freischalten durch das Mein Inhalt Menü.)"),
	},

	// fra
	{
	TEXT("L’espace libre sur le disque dur est  insuffisant pour continuer. Veuillez quitter, supprimer d’autres jeux sauvegardés,  données de jeu ou d’autres contenus, et réessayer."),
	TEXT("Les données de jeu sont irrémédiablement endommagées. Veuillez quitter, supprimer les données de jeu et réessayer."),
	TEXT("Pour des performances optimales, %d Mo supplémentaires sont nécessaires. Vous pouvez continuer ou quitter le jeu maintenant, libérer de l'espace disque dur et réessayer."),
	TEXT("Le disque a été éjecté. La partie ne peut pas continuer. Veuillez quitter et relancer le jeu."),
	TEXT("Unreal Tournament ® 3"),
	TEXT("Installation de données requises en cours. Veuillez ne pas éteindre le système PLAYSTATION®3."),
	TEXT("Il semble que le jeu ait perdu puissance ou soit tombé en panne, en général cela est dû au contenu créé par l'utilisateur. Voulez vous désactiver le contenu créé par l'utilisateur? (Vous pourrez toujours l'effacer ou le réactiver dans le menu \"Mon contenu\")."),
	},

	// esm
	{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	},

	// esn
	{
	TEXT("No se puede continuar sin espacio suficiente en el disco duro. Por favor, elimina otras partidas guardadas, información de juego u otros contenidos, y vuelve a intentarlo."),
	TEXT("La información del juego tiene daños irreparables. Por favor, sal, borra la información del juego y reinicia."),
	TEXT("Para un rendimiento óptimo hacen falta %d MB más. Puedes seguir jugando o salir ahora, liberar espacio de disco duro y reiniciar."),
	TEXT("El disco ha sido expulsado. El juego no puede continuar. Por favor, reinicie el juego."),
	TEXT("Unreal Tournament ® 3"),
	TEXT("Instalando datos necesarios del juego. Por favor no apague el Sistema PLAYSTATION®3."),
	TEXT("Parece que el juego ha perdido la conexión o se ha bloqueado, debido al contenido creado por el usuario. ¿Quieres invalidar el contenido creado por el usuario? (Podrás siempre borrarlo y volver a utilizarlo en el menú Mi Contenido)."),
	},

	// ita
	{
	TEXT("Non è possibile continuare senza spazio sufficiente sul disco fisso. Uscire ed eliminare le altre partite salvate, dati di partite o altri contenuti e riprova."),
	TEXT("I dati del gioco sono danneggiati irreparabilmente. Uscire ed eliminare i dati del gioco ed eseguire di nuovo."),
	TEXT("Per un rendimento ottimale occorrono %d MB di spazio libero sul disco fisso. Puoi continuare a giocare oppure uscire ora, liberare lo spazio libero e ricominciare."),
	TEXT("Il disco è stato espulso. Il gioco non può continuare. Esci dal gioco e riavviare."),
	TEXT("Unreal Tournament ® 3"),
	TEXT("L'installazione richiede dati di gioco. Non spegnere il sistema PLAYSTATION®3."),
	TEXT("Sembra che il gioco abbia perso potenza o sia bloccato, questo è dovuto di solito al contenuto creato dell'utente. Vuoi disabilitare il contenuto creato dall’utente? (Potrai ancora eliminarlo oppure riabilitarlo dal menu del Mio Contenuto)."),
	},
};

/**
 * Gets a localized string for a few string keys that need to used before loading the .int files
 * 
 * @param StringType Which string type to localize
 * 
 * @return Localized string in any of the supported GKnownLanguages array
 */
const TCHAR* appPS3GetEarlyLocalizedString(ELocalizedEarlyStringType StringType)
{
	// get the language directly (in case UObject::GetLanguage hasn't been called)
	FString Language = appGetLanguageExt();
	
	// Get a list of known language extensions
	const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

	// Find the matching entry in the KnownLanguageExtensions
	INT LangIndex = KnownLanguageExtensions.FindItemIndex( Language );

	// if we didn't find the language, default to INT
	if ( LangIndex == INDEX_NONE || LangIndex >= ARRAY_COUNT(GEarlyTranslatedMessages) )
	{
		LangIndex = 0;
	}

	// get the unicode string (or english if the language wasn't translated)
	return GEarlyTranslatedMessages[LangIndex][StringType] ? GEarlyTranslatedMessages[LangIndex][StringType] : GEarlyTranslatedMessages[0][StringType];
}

/**
 *	Checks whether a memory address is on the stack of the current thread.
 *
 *	@param Address	Memory address to check
 *	@return			TRUE if the address is on the stack
 */
UBOOL appPS3IsStackMemory( const void* Address )
{
	PTRINT Pointer = PTRINT(Address);
#if PS3_NO_TLS
	sys_ppu_thread_stack_t StackInfo;
	sys_ppu_thread_get_stack_information( &StackInfo );
	PTRINT StackStart = PTRINT(StackInfo.pst_addr);
	PTRINT StackEnd = StackStart + StackInfo.pst_size;
	return ( Pointer >= StackStart && Pointer < StackEnd );
#else
	appTryInitializeTls();
	return ( Pointer >= GTLS_StackBase && Pointer < (GTLS_StackBase + GTLS_StackSize) );
#endif
}

FCriticalSection UncompressZLibCriticalSection;

/**
 * PS3 specific ZLIB decompression using a spurs task
 *
 * @param	UncompressedBuffer			Buffer containing uncompressed data
 * @param	UncompressedSize			Size of uncompressed data in bytes
 * @param	CompressedBuffer			Buffer compressed data is going to be read from
 * @param	CompressedSize				Size of CompressedBuffer data in bytes
 * @param	bIsSourcePadded				Whether the source memory is padded with a full cache line at the end
 * @return TRUE if compression succeeds, FALSE if it fails because CompressedBuffer was too small or other reasons
 */
UBOOL appUncompressMemoryZLIBPS3( void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize, UBOOL bIsSourcePadded )
{
	// edge zlib is initialized in UnPS3.cpp.

	// validate the data
	BYTE* Bytes = (BYTE*)CompressedBuffer;
	if (Bytes[0] != 0x78 || (((INT)Bytes[0] * 256 + Bytes[1]) % 31) != 0 || (Bytes[1] & (1 << 5)))
	{
		debugf(TEXT("ZLIB header is wrong [%x %x], displaying corrupt data dialog"), Bytes[0], Bytes[1]);
		appHandleIOFailure(TEXT("Decompress"));
	}

	// check for buffers too big to fit on SPU (should be exceedingly rare)
	// or if a buffer is on the stack
	// @note: This is based on the PS3 OS specification. If we were ever to run this on PS3 Linux, this would not be a valid check
	UBOOL bSrcIsOnStack = appPS3IsStackMemory(CompressedBuffer);		//((DWORD)CompressedBuffer & 0xF0000000) == 0xD0000000;
	UBOOL bDestIsOnStack = appPS3IsStackMemory(UncompressedBuffer);	//((DWORD)UncompressedBuffer & 0xF0000000) == 0xD0000000;

	// Use PPU if the source memory is within 128 bytes from a 64 KB boundary, because the SPU align the read-size to 128 bytes
	// and can read outside the allocated memory (causing crash).
	DWORD DistanceTo64KBoundary = 0x10000 - ((PTRINT(CompressedBuffer) + CompressedSize) & 0xffff);
	if (UncompressedSize > 64*1024 || CompressedSize > 64*1024 || bSrcIsOnStack || bDestIsOnStack || (!bIsSourcePadded && DistanceTo64KBoundary < 128) )
	{
		debugf(NAME_PerfWarning, TEXT("Using PPU decompression (0x%08x,%d->0x%08x,%d)"), CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize);

		// use PPU decompression if the sizes are too big to fit on SPU
		extern UBOOL appUncompressMemoryZLIB( void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize );
		return appUncompressMemoryZLIB(UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
	}
	else
	{
		// The Edge zlib wrapper is not thread safe so block other threads from trying to decompress
		//@todo - can the wrapper be made thread safe so a critical section is not necessary?
		FScopeLock Lock(&UncompressZLibCriticalSection);
		// Edge does not want the zlib 2-byte header and 4-byte footer...
		INT InflateId = edge_zlib_inflate((const BYTE*)CompressedBuffer + 2, CompressedSize - (2 + 4), UncompressedBuffer, UncompressedSize);
		
		if (InflateId < 0)
		{
			return FALSE;
		}

		// wait for it to finish
		edge_zlib_wait(InflateId);

		return TRUE;
	}
}

/** Returns the user that is signed in locally */
FString appGetUserName(void)
{
	char Buffer[CELL_SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE];
	// Ask the PS3 for the user name
	if (cellSysutilGetSystemParamString(CELL_SYSUTIL_SYSTEMPARAM_ID_CURRENT_USERNAME,
		Buffer,
		CELL_SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE) == CELL_OK)
	{
		return FString(UTF8_TO_TCHAR(Buffer));
	}
	return FString();
}

/** @return Determines if the current PS3 user has an NP account */
UBOOL appDoesUserHaveNpAccount(void)
{
	INT HasAccount = 0;
	// Ask the PS3 whether the user has an account
	if (cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_CURRENT_USER_HAS_NP_ACCOUNT,
		&HasAccount) == CELL_OK)
	{
		return HasAccount != 0 ? TRUE : FALSE;
	}
	return FALSE;
}

/**
 * Checks to see if the specified controller is connected
 *
 * @param ControllerId the controller to check
 *
 * @return TRUE if connected, FALSE if not
 */
UBOOL appIsControllerPresent(INT ControllerId)
{
	CellPadInfo2 PadInfo;
	// Read the pad info for all pads
	if (cellPadGetInfo2(&PadInfo) == CELL_PAD_OK)
	{
		// Check the specific controller for connected state
		return (PadInfo.port_status[ControllerId] & CELL_PAD_STATUS_CONNECTED) ? TRUE : FALSE;
	}
	return FALSE;
}

/** @return The age rating of the game */
INT appGetGameAgeRating(void)
{
	INT AgeRating = 0;
	// Ask the PS3 for the game's age rating
	if (cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_GAME_PARENTAL_LEVEL,
		&AgeRating) == CELL_OK)
	{
		return AgeRating;
	}
	// Worst case this is AO
	return CELL_SYSUTIL_GAME_PARENTAL_LEVEL11;
}

/*----------------------------------------------------------------------------
	Misc functions.
----------------------------------------------------------------------------*/

/**
 * Platform specific function for adding a named event that can be viewed in PIX
 */
void appBeginNamedEvent(const FColor& Color,const TCHAR* Text)
{
}

/**
 * Platform specific function for closing a named event that can be viewed in PIX
 */
void appEndNamedEvent()
{
}


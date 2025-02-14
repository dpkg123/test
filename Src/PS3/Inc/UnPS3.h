/*=============================================================================
	UnPS3.h: Unreal definitions for PS3.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifndef UNPS3_HEADER
#define UNPS3_HEADER

#include <string.h>
#include <alloca.h>
#include <fastmath.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>

#include <common/include/sdk_version.h>
#include <common/include/edge/edge_version.h>
#include <ppu_intrinsics.h>
#include <sys/memory.h>
#include <sys/dbg.h>

#include <cell/fios.h>


// define what SDK we need to be at least as high as
// @ps3 - replace with current SDK version as it changes
#define PS3_REQUIRED_SDK_VERSION 0x300001
#define PS3_REQUIRED_EDGE_VERSION 0x00111000

// check that we are compiled with at least the minimum SDK
checkAtCompileTime(CELL_SDK_VERSION >= PS3_REQUIRED_SDK_VERSION, PS3_SDK_VERSION_DoesNotMatchRequiredVersion);
checkAtCompileTime(EDGE_SDK_VERSION >= PS3_REQUIRED_EDGE_VERSION, PS3_EDGE_VERSION_DoesNotMatchRequiredVersion);

#if FINAL_RELEASE && !FINAL_RELEASE_DEBUGCONSOLE
// don't allow printf in final release
#define printf(...) 
#endif

// Default #defines for the special builds.
#ifndef PS3_PASUITE
	#define PS3_PASUITE 0
#endif

// @todo: turn this on for demo builds, off for QA builds,e tc
#define PS3_DEMO_MODE 0

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Comment this out if you have no need for unicode strings (ie asian languages, etc).
#define UNICODE 1


// Generates GCC version like this:  xxyyzz (eg. 030401)
// xx: major version, yy: minor version, zz: patch level
#ifdef __GNUC__
#	define GCC_VERSION	(__GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL)
#endif

// Undo any defines.
#undef NULL
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef CDECL

// Make sure HANDLE is defined.
#ifndef _WINDOWS_
	#define HANDLE void*
	#define HINSTANCE void*
#endif

// Sizes.
enum {DEFAULT_ALIGNMENT = 8 }; // Default boundary to align memory allocations on.

#define RENDER_DATA_ALIGNMENT 128 // the value to align some renderer bulk data to

#if PS3_SNC

#define PRAGMA_DISABLE_OPTIMIZATION		_Pragma ("control %push postopt=0")
#define PRAGMA_ENABLE_OPTIMIZATION		_Pragma ("control %pop postopt")

#else

// Optimization macros (preceded by #pragma).
#define PRAGMA_DISABLE_OPTIMIZATION _Pragma("optimize(\"\",off)")
#ifdef _DEBUG
	#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("optimize(\"\",off)")
#else
	#define PRAGMA_ENABLE_OPTIMIZATION  _Pragma("optimize(\"\",on)")
#endif

#endif

// Function type macros.
#define VARARGS     					/* Functions with variable arguments */
#define CDECL	    					/* Standard C function */
#define STDCALL							/* Standard calling convention */


#if PS3_SNC
#define FORCEINLINE __attribute__ ((always_inline))     /* Force code to be inline */
#else
#define FORCEINLINE inline __attribute__ ((always_inline))    /* Force code to be inline */
#endif

#define FORCENOINLINE __attribute__((noinline))			/* Force code to be NOT inline */
#define ZEROARRAY						/* Zero-length arrays in structs */

// pointer aliasing restricting
#define RESTRICT __restrict

// Compiler name.
#ifdef _DEBUG
	#define COMPILER "Compiled with GCC/SNC debug"
#else
	#define COMPILER "Compiled with GCC/SNC"
#endif



// Unsigned base types.
typedef uint8_t					BYTE;		// 8-bit  unsigned.
typedef uint16_t				WORD;		// 16-bit unsigned.
typedef uint32_t				UINT;		// 32-bit unsigned.
typedef unsigned long			DWORD;		// 32-bit unsigned.
typedef uint64_t				QWORD;		// 64-bit unsigned.

// Signed base types.
typedef int8_t					SBYTE;		// 8-bit  signed.
typedef int16_t					SWORD;		// 16-bit signed.
typedef int32_t					INT;		// 32-bit signed.
typedef long					LONG;		// 32-bit signed.
typedef int64_t					SQWORD;		// 64-bit unsigned.

// Character types.
typedef char					ANSICHAR;	// An ANSI character. normally a signed type.
typedef wchar_t					UNICHAR;	// A unicode character. normally a signed type.
// WCHAR defined below

// Other base types.
typedef UINT					UBOOL;		// Boolean 0 (false) or 1 (true).
typedef INT						BOOL;		// Must be sint32 to match windows.h
typedef float					FLOAT;		// 32-bit IEEE floating point.
typedef double					DOUBLE;		// 64-bit IEEE double.
typedef unsigned long			SIZE_T;     // Should be size_t, but windows uses this
typedef intptr_t				PTRINT;		// Integer large enough to hold a pointer.
typedef uintptr_t				UPTRINT;	// Unsigned integer large enough to hold a pointer.

// Bitfield type.
typedef unsigned int			BITFIELD;	// For bitfields.

/** Represents a serializable object pointer in UnrealScript.  This is always 64-bits, even on 32-bit platforms. */
typedef	QWORD					ScriptPointerType;

#define DECLARE_UINT64(x)	x##ULL


// Make sure characters are unsigned.
#ifdef _CHAR_UNSIGNED
	#error "Bad VC++ option: Characters must be signed"
#endif

// No asm if not compiling for x86.
#if !(defined _M_IX86)
	#undef ASM_X86
	#define ASM_X86 0
#else
	#define ASM_X86 1
#endif

#if defined(__x86_64__) || defined(__i386__)
	#define __INTEL_BYTE_ORDER__ 1
#else
	#define __INTEL_BYTE_ORDER__ 0
#endif

#ifndef PLATFORM_64BITS
	#if __LP32__
		#define PLATFORM_64BITS 0
		#define PLATFORM_32BITS 1
	#else
		#error PS3 code assume pointer size of 32 bits
	#endif
#endif

// Virtual function override
#define OVERRIDE

// DLL file extension.
#define DLLEXT TEXT(".dll")

// Pathnames.
#define PATH(s) s

// NULL.
#define NULL 0

#define FALSE 0
#define TRUE 1

// Platform support options.
#define FORCE_ANSI_LOG           1

// Queued work needs special logic to go to SPUs
#define PLATFORM_NEEDS_SPECIAL_QUEUED_WORK 1

// OS unicode function calling.
typedef wchar_t TCHAR;
#define _TCHAR_DEFINED
//#define _TEXT_DEFINED
//typedef wchar_t TCHAR;
//#define TEXT(x) 23r23rfcas

// defines for the "standard" unicode-safe string functions
#define _tcscpy wcscpy
#define _tcslen wcslen
#define _tcsstr wcsstr
#define _tcschr wcschr
#define _tcsrchr wcsrchr
#define _tcscat wcscat
#define _tcscmp wcscmp
#define _stricmp strcasecmp
#define _tcsicmp wgccstrcasecmp
#define _tcsncmp wcsncmp
#define _tcsupr wgccupr //strupr
#define _tcstoul wcstoul
#define _tcstoui64 wcstoull
#define _tcsnicmp wgccstrncasecmp
#define _tstoi(s) wcstoul(s, 0, 10)
#define _tstoi64(s) wcstoull(s, 0, 10)
#define _tstof(s) wcstod(s, 0)
#define _tcstod wcstod
#define _tcsncpy wcsncpy
#define _stscanf swscanf

#define CP_OEMCP 1
#define CP_ACP 1

#include <wchar.h>
#include <wctype.h>
#include <xwcc.h>

// String conversion classes
#include "UnStringConv.h"

/**
* NOTE: The objects these macros declare have very short lifetimes. They are
* meant to be used as parameters to functions. You cannot assign a variable
* to the contents of the converted string as the object will go out of
* scope and the string released.
*
* NOTE: The parameter you pass in MUST be a proper string, as the parameter
* is typecast to a pointer. If you pass in a char, not char* it will crash.
*
* Usage:
*
*		SomeApi(TCHAR_TO_ANSI(SomeUnicodeString));
*
*		const char* SomePointer = TCHAR_TO_ANSI(SomeUnicodeString); <--- Bad!!!
*/
#define TCHAR_TO_ANSI(str) (ANSICHAR*)FTCHARToANSI((const TCHAR*)str)
// PS3 doesn't support to OEM conversions so just use ANSI
#define TCHAR_TO_OEM(str) (ANSICHAR*)FTCHARToANSI((const TCHAR*)str)
#define ANSI_TO_TCHAR(str) (TCHAR*)FANSIToTCHAR((const ANSICHAR*)str)
#define TCHAR_TO_UTF8(str) (ANSICHAR*)FTCHARToUTF8((const TCHAR*)str)
#define UTF8_TO_TCHAR(str) (TCHAR*)FUTF8ToTCHAR((const ANSICHAR*)str)
#undef CP_OEMCP
#undef CP_ACP

// Strings.
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("\\")
#define appIsPathSeparator( Ch )	((Ch) == TEXT('/') || (Ch) == TEXT('\\'))


// Alignment.
#define GCC_PACK(n) __attribute__((packed,aligned(n)))
#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define MS_ALIGN(n)

// GCC doesn't support __noop, but there is a workaround :)
#define COMPILER_SUPPORTS_NOOP 1
#define __noop(...)


/*----------------------------------------------------------------------------
	Globals.
----------------------------------------------------------------------------*/

// System identification.
extern "C"
{
	extern HINSTANCE      hInstance;
}

/** Screen width													*/
extern INT			GScreenWidth;
/** Screen height													*/
extern INT			GScreenHeight;

struct CellSpurs;
/** The Main 5-SPU SPURS Instance. */
extern CellSpurs *	GSPURS;
/** The 1-SPU SPURS Instance. */
extern CellSpurs *	GSPURS2;

/** Thread context */
typedef sys_dbg_ppu_thread_context_t		FThreadContext;

#define SPU_NUM_SPURS	5	/** Number of SPU:s to be managed by SPURS */
#define SPU_NUM_PHYSX	3	/** Maximum number of SPU:s used by PhysX */
#define SPU_PRIO_PHYSX	4	/** Priority for PhysX SPU tasks */
#define SPU_PRIO_AUDIO	8   /** Priority for Audio SPU tasks */
#define SPU_PRIO_MOVE	2	/** Priority for Move SPU tasks */
#define SPU_NUM_PNG		4	/** Max number used by PNG decoding */
#define SPU_PRIO_PNG	10	/** Low priority for PNGs */

/*----------------------------------------------------------------------------
Stack walking.
----------------------------------------------------------------------------*/

/** @name ObjectFlags
* Flags used to control the output from stack tracing
*/
typedef DWORD EVerbosityFlags;
#define VF_DISPLAY_BASIC		0x00000000
#define VF_DISPLAY_FILENAME		0x00000001
#define VF_DISPLAY_MODULE		0x00000002
#define VF_DISPLAY_ALL			0xffffffff
                               
/*----------------------------------------------------------------------------
	Initialization.
----------------------------------------------------------------------------*/

extern void appPS3Init(int argc, char* const argv[]);

/**
 * @return TRUE if the user has chosen to use Circle button as the accept button
 */
UBOOL appPS3UseCircleToAccept();

/**
 * This function will "cleanly" shutdown the PS3 and exit to the system software
 * It must be called from the main thread.
 */
void appPS3QuitToSystem();

/*----------------------------------------------------------------------------
	Math functions.
----------------------------------------------------------------------------*/

const FLOAT	SRandTemp = 1.f;
extern INT GSRandSeed;

inline INT appTrunc( FLOAT F )
{
	return (INT)F;
//	return (INT)truncf(F);
}
inline FLOAT appTruncFloat( FLOAT F )
{
//	return __fcfid(__fctidz(F));
	return (FLOAT)appTrunc(F);
}

inline FLOAT	appExp( FLOAT Value )			{ return expf(Value); }
inline FLOAT	appLoge( FLOAT Value )			{ return logf(Value); }
inline FLOAT	appFmod( FLOAT Y, FLOAT X )		{ return fmodf(Y,X); }
inline FLOAT	appSin( FLOAT Value )			{ return sinf(Value); }
inline FLOAT 	appAsin( FLOAT Value ) 			{ return asinf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
inline FLOAT 	appCos( FLOAT Value ) 			{ return cosf(Value); }
inline FLOAT 	appAcos( FLOAT Value ) 			{ return acosf( (Value<-1.f) ? -1.f : ((Value<1.f) ? Value : 1.f) ); }
inline FLOAT	appTan( FLOAT Value )			{ return tanf(Value); }
inline FLOAT	appAtan( FLOAT Value )			{ return atanf(Value); }
inline FLOAT	appAtan2( FLOAT Y, FLOAT X )	{ return atan2f(Y,X); }
inline FLOAT	appSqrt( FLOAT Value );
inline FLOAT	appPow( FLOAT A, FLOAT B )		{ return powf(A,B); }
inline UBOOL	appIsNaN( FLOAT A )				{ return isnan(A) != 0; }
inline UBOOL	appIsFinite( FLOAT A )			{ return isfinite(A) != 0; }
inline INT		appFloor( FLOAT F );
inline INT		appCeil( FLOAT Value )			{ return appTrunc(ceilf(Value)); }
inline INT		appRand()						{ return rand(); }
inline FLOAT	appCopySign( FLOAT A, FLOAT B ) { return copysignf(A,B); }
inline void		appRandInit(INT Seed)			{ srand( Seed ); }
inline void		appSRandInit( INT Seed )		{ GSRandSeed = Seed; }
inline FLOAT	appFractional( FLOAT Value )	{ return Value - appTruncFloat( Value ); }

/**
 * Counts the number of leading zeros in the bit representation of the value
 *
 * @param Value the value to determine the number of leading zeros for
 *
 * @return the number of zeros before the first "on" bit
 */
FORCEINLINE DWORD appCountLeadingZeros(DWORD Value)
{
	// Use the PS3 intrinsic
	return __cntlzw(Value);
}

/**
 * Computes the base 2 logarithm for an integer value that is greater than 0.
 * The result is rounded down to the nearest integer.
 *
 * @param Value the value to compute the log of
 */
inline DWORD appFloorLog2(DWORD Value)
{
	return 31 - appCountLeadingZeros(Value);
}

inline FLOAT appSRand() 
{ 
	GSRandSeed = (GSRandSeed * 196314165) + 907633515;
	union { FLOAT f; INT i; } Result;
	union { FLOAT f; INT i; } Temp;
	Temp.f = SRandTemp;
	Result.i = (Temp.i & 0xff800000) | (GSRandSeed & 0x007fffff);
	return appFractional( Result.f );
} 

//@todo: rand() is currently prohibitively expensive. We need a fast general purpose solution that isn't as bad as appSRand()
inline FLOAT appFrand() { return appSRand(); }

inline INT appRound( FLOAT F )
{
	return appTrunc(roundf(F));
}

inline INT appFloor( FLOAT F )
{
	return appTrunc(floorf(F));
}

inline FLOAT appInvSqrt( FLOAT F )
{
	return 1.0f / sqrtf(F);
}

/**
 * Fast inverse square root using the estimate intrinsic with Newton-Raphson refinement
 * Accurate to at least 0.00000001 of appInvSqrt() and 2.45x faster
 *
 * @param F the float to estimate the inverse square root of
 *
 * @return the inverse square root
 */
inline FLOAT appInvSqrtEst(FLOAT F)
{
	// 0.5 * rsqrtps * (3 - x * rsqrtps(x) * rsqrtps(x))
#if PS3_SNC
	FLOAT RecipSqRtEst = __builtin_frsqrte(F);
#else
	FLOAT RecipSqRtEst = __frsqrte(F);
#endif
	return 0.5f * RecipSqRtEst * (3.f - (F * (RecipSqRtEst * RecipSqRtEst)));
}

inline FLOAT appSqrt( FLOAT F )
{
	return sqrtf(F);
}

//#define appAlloca(size) _alloca((size+7)&~7)
#define appAlloca(size) ((size==0) ? 0 : alloca((size+7)&~7))

inline TCHAR* gccupr(TCHAR* str)
{
	for (TCHAR* c = str; *c; c++)
	{
		*c = toupper(*c);
	}
	return str;
}

inline TCHAR* wgccupr(TCHAR* str)
{
	for (TCHAR* c = str; *c; c++)
	{
		*c = towupper(*c);
	}
	return str;
}
int wgccstrcasecmp(const TCHAR *a, const TCHAR *b);
int wgccstrncasecmp(const TCHAR *a, const TCHAR *b, size_t size);

#define DEFINED_appSeconds 1

extern DOUBLE GSecondsPerCycle;
extern QWORD GTicksPerSeconds;
extern QWORD GNumTimingCodeCalls;

inline DOUBLE appSeconds()
{
#if !FINAL_RELEASE
	GNumTimingCodeCalls++;
#endif

	QWORD Cycles = __mftb();
	return (DOUBLE)Cycles * GSecondsPerCycle;
}

inline DWORD appCycles()
{
#if !FINAL_RELEASE
	GNumTimingCodeCalls++;
#endif

	return (DWORD)__mftb();
}

/*----------------------------------------------------------------------------
	Misc functions.
----------------------------------------------------------------------------*/

/** @return True if called from the rendering thread. */
extern UBOOL IsInRenderingThread();

/** @return True if called from the game thread. */
extern UBOOL IsInGameThread();

#include "UMemoryDefines.h"

/**
* Converts the passed in program counter address to a human readable string and appends it to the passed in one.
* @warning: The code assumes that HumanReadableString is large enough to contain the information.
*
* @param	ProgramCounter			Address to look symbol information up for
* @param	HumanReadableString		String to concatenate information with
* @param	HumanReadableStringSize size of string in characters
* @param	VerbosityFlags			Bit field of requested data for output. -1 for all output.
*/ 
void appProgramCounterToHumanReadableString( QWORD ProgramCounter, ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, EVerbosityFlags VerbosityFlags = VF_DISPLAY_ALL );

/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @param	Context				Optional thread context information
 * @return	Number of function pointers captured
 */
DWORD appCaptureStackBackTrace( QWORD* BackTrace, DWORD MaxDepth, FThreadContext* Context = NULL );

/**
 * Handles IO failure by ending gameplay.
 *
 * @param Filename	If not NULL, name of the file the I/O error occurred with
 */
void appHandleIOFailure( const TCHAR* Filename );

/** 
 * Returns whether the line can be broken between these two characters
 */
UBOOL appCanBreakLineAt( TCHAR Previous, TCHAR Current );

/**
 * Enforces strict memory load/store ordering across the memory barrier call.
 */
FORCEINLINE void appMemoryBarrier()
{
	__lwsync();
}

/** 
 * Support functions for overlaying an object/name pointer onto an index (like in script code
 */
inline ScriptPointerType appPointerToSPtr(void* Pointer)
{
	return (ScriptPointerType)Pointer;
}

inline void* appSPtrToPointer(ScriptPointerType Value)
{
	return (void*)(PTRINT)Value;
}

/**
 * Retrieve a environment variable from the system
 *
 * @param VariableName The name of the variable (ie "Path")
 * @param Result The string to copy the value of the variable into
 * @param ResultLength The size of the Result string
 */
inline void appGetEnvironmentVariable(const TCHAR* VariableName, TCHAR* Result, INT ResultLength)
{
	// return an empty string
	*Result = 0;
}

/**
 * Push a marker for external profilers.
 *
 * @param MarkerName A descriptive name for the marker to display in the profiler
 */
inline void appPushMarker( const TCHAR* MarkerName )
{
}

/**
 * Pop the previous marker for external profilers.
 */
inline void appPopMarker( )
{
}

// Variable arguments.
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
INT appGetVarArgs( TCHAR* Dest, SIZE_T DestSize, INT Count, const TCHAR*& Fmt, va_list ArgPtr );

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
INT appGetVarArgsAnsi( ANSICHAR* Dest, SIZE_T DestSize, INT Count, const ANSICHAR*& Fmt, va_list ArgPtr );

#define GET_VARARGS(msg,msgsize,len,lastarg,fmt) {va_list ap; va_start(ap,lastarg);appGetVarArgs(msg,msgsize,len,fmt,ap);}
#define GET_VARARGS_ANSI(msg,msgsize,len,lastarg,fmt) {va_list ap; va_start(ap,lastarg);appGetVarArgsAnsi(msg,msgsize,len,fmt,ap);}
#define GET_VARARGS_RESULT(msg,msgsize,len,lastarg,fmt,result) {va_list ap; va_start(ap, lastarg); result = appGetVarArgs(msg,msgsize,len,fmt,ap); }
#define GET_VARARGS_RESULT_ANSI(msg,msgsize,len,lastarg,fmt,result) {va_list ap; va_start(ap, lastarg); result = appGetVarArgsAnsi(msg,msgsize,len,fmt,ap); }
/**
 * Known early string types that work with appPS3GetEarlyLocalizedString
 */
enum ELocalizedEarlyStringType
{
	ELST_NoHDSpace,
	ELST_GameDataDamaged,
	ELST_LessHDThanDesired,
	ELST_BDEjected,
	ELST_GameDataDisplayName,
	ELST_InstallingRequiredFile,
	ELST_GameCrashed,
	ELST_MAX
};

/**
 * Gets a localized string for a few string keys that need to used before loading the .int files
 * 
 * @param StringType Which string type to localize
 * 
 * @return Localized string in any of the supported GKnownLanguages array
 */
const TCHAR* appPS3GetEarlyLocalizedString(ELocalizedEarlyStringType StringType);

/*----------------------------------------------------------------------------
	Memory helpers
----------------------------------------------------------------------------*/

#define appSystemMalloc		appPS3SystemMalloc
#define appSystemFree		appPS3SystemFree

/**
 * Wrappers around sys_memory_* functions to get memory directly from the system
 */
void* appPS3SystemMalloc(INT Size);
void appPS3SystemFree(void* Ptr);

/**
 * Prints out detailed memory info (slow)
 *
 * @param Ar Device to send output to
 */
void appPS3DumpDetailedMemoryInformation(class FOutputDevice& Ar);

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
UBOOL appUncompressMemoryZLIBPS3( void* UncompressedBuffer, INT UncompressedSize, const void* CompressedBuffer, INT CompressedSize, UBOOL bIsSourcePadded );

/**
 *	Checks whether a memory address is on the stack of the current thread.
 *
 *	@param Address	Memory address to check
 *	@return			TRUE if the address is on the stack
 */
UBOOL appPS3IsStackMemory( const void* Address );

/*-----------------------------------------------------------------------------
	On screen keyboard functions
-----------------------------------------------------------------------------*/

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
UBOOL appPS3ShowVirtualKeyboard(const TCHAR* Prompt, const TCHAR* InitialText, INT MaxLength, UBOOL bIsPassword);

/**
 * Pumps the on screen keyboard, and it TRUE, the result is ready to go, and the keyboard
 * has closed. Cancel on the keyboard will return an empty string.
 * @todo Handle cancel as a special case (ON ALL PLATFORMS!)
 *
 * @param Result Returns the string entered when the keyboard is closed
 * @param bWasCancelled whether the user canceled the input or not
 * 
 * @return TRUE if the keyboard was closed, and Result is ready for use
 */
UBOOL appPS3TickVirtualKeyboard(class FString& Result, UBOOL& bWasCancelled);

/**
 * Start up a thread that will flip the screen and check for sysutil callbacks
 * This should only be used BEFORE the rendering thread is up and going
 */
void appPS3StartFlipPumperThread();

/**
 * Kill the flipper thread start with appPS3StartFlipPumperThread
 */
void appPS3StopFlipPumperThread();

/** Returns the user that is signed in locally */
FString appGetUserName(void);

/** @return Determines if the current PS3 user has an NP account */
UBOOL appDoesUserHaveNpAccount(void);

/**
 * Checks to see if the specified controller is connected
 *
 * @param ControllerId the controller to check
 *
 * @return TRUE if connected, FALSE if not
 */
UBOOL appIsControllerPresent(INT ControllerId);

/** @return The age rating of the game */
INT appGetGameAgeRating(void);

#endif // UNPS3_HEADER

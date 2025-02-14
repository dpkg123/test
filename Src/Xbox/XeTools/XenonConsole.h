/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#pragma once

#pragma managed(push, off)

#include <Windows.h>
#include "..\..\Engine\Inc\UnConsoleTools.h"
#include <xbdm.h>

#pragma managed(pop)

namespace XeTools
{
	// forward declarations
	ref class XenonConsole;
	ref class ThreadData;

	public delegate DWORD ThreadCallbackDelegate(XDevkit::XboxConsole ^Target, ThreadData ^BaseThreadData);

	public value struct PDBSignature
	{
		System::String ^Path;
		DWORD Age;
		System::Guid Guid;
	};

	public ref struct PDBEntry
	{
		PDBSignature PDBSig;
		DWORD BaseAddress;
		DWORD Size;
	};

	public ref class ThreadData
	{
	public:
		//NOTE: We explicitly copy the function pointers as they can be changed by the UI thread while we're parsing callstack data
		TTYEventCallbackPtr TTYFunc;
		ThreadCallbackDelegate ^ThreadCallback;
		XenonConsole ^Console;

		ThreadData() : TTYFunc(NULL), ThreadCallback(nullptr), Console(nullptr) {}
	};

	public ref class CrashThreadData : public ThreadData
	{
	public:
		//NOTE: We explicitly copy the function pointers as they can be changed by the UI thread while we're parsing callstack data
		CrashCallbackPtr CrashFunc;
		System::String ^GameName;
		System::Collections::Generic::List<DWORD> ^CallStack;
		System::Collections::Generic::List<PDBEntry^> ^ModulePDBList;
		XDevkit::XboxDumpFlags DumpType;

		CrashThreadData() 
			: CrashFunc(NULL), GameName(System::String::Empty), CallStack(gcnew System::Collections::Generic::List<DWORD>()), ModulePDBList(gcnew System::Collections::Generic::List<PDBEntry^>()), DumpType(XDevkit::XboxDumpFlags::Normal)
		{}
	};

	public ref class ProfileThreadData : public ThreadData
	{
	public:
		FConsoleSupport::EProfileType Type;
		System::String ^FileName;
		BOOL bFullFileNameProvided;
	};

	public enum class XBDMErrorCodes
	{
		NOERR = XBDM_NOERR,

		UNDEFINED = XBDM_UNDEFINED,
		MAXCONNECT = XBDM_MAXCONNECT,
		NOSUCHFILE = XBDM_NOSUCHFILE,
		NOMODULE = XBDM_NOMODULE,
		MEMUNMAPPED = XBDM_MEMUNMAPPED,
		NOTHREAD = XBDM_NOTHREAD,
		CLOCKNOTSET = XBDM_CLOCKNOTSET,
		INVALIDCMD = XBDM_INVALIDCMD,
		NOTSTOPPED = XBDM_NOTSTOPPED,
		MUSTCOPY = XBDM_MUSTCOPY,
		ALREADYEXISTS = XBDM_ALREADYEXISTS,
		DIRNOTEMPTY = XBDM_DIRNOTEMPTY,
		BADFILENAME = XBDM_BADFILENAME,
		CANNOTCREATE = XBDM_CANNOTCREATE,
		CANNOTACCESS = XBDM_CANNOTACCESS,
		DEVICEFULL = XBDM_DEVICEFULL,
		NOTDEBUGGABLE = XBDM_NOTDEBUGGABLE,
		BADCOUNTTYPE = XBDM_BADCOUNTTYPE,
		COUNTUNAVAILABLE = XBDM_COUNTUNAVAILABLE,
		NOTLOCKED = XBDM_NOTLOCKED,
		KEYXCHG = XBDM_KEYXCHG,
		MUSTBEDEDICATED = XBDM_MUSTBEDEDICATED,
		INVALIDARG = XBDM_INVALIDARG,
		PROFILENOTSTARTED = XBDM_PROFILENOTSTARTED,
		PROFILEALREADYSTARTED = XBDM_PROFILEALREADYSTARTED,
		ALREADYSTOPPED = XBDM_ALREADYSTOPPED,
		FASTCAPNOTENABLED = XBDM_FASTCAPNOTENABLED,
		NOMEMORY = XBDM_NOMEMORY,
		TIMEOUT = XBDM_TIMEOUT,
		NOSUCHPATH = XBDM_NOSUCHPATH,
		INVALID_SCREEN_INPUT_FORMAT = XBDM_INVALID_SCREEN_INPUT_FORMAT,
		INVALID_SCREEN_OUTPUT_FORMAT = XBDM_INVALID_SCREEN_OUTPUT_FORMAT,
		CALLCAPNOTENABLED = XBDM_CALLCAPNOTENABLED,
		INVALIDCAPCFG = XBDM_INVALIDCAPCFG,
		CAPNOTENABLED = XBDM_CAPNOTENABLED,
		TOOBIGJUMP = XBDM_TOOBIGJUMP,
		FIELDNOTPRESENT = XBDM_FIELDNOTPRESENT,
		OUTPUTBUFFERTOOSMALL = XBDM_OUTPUTBUFFERTOOSMALL,
		PROFILEREBOOT = XBDM_PROFILEREBOOT,
		MAXDURATIONEXCEEDED = XBDM_MAXDURATIONEXCEEDED,
		INVALIDSTATE = XBDM_INVALIDSTATE,
		MAXEXTENSIONS = XBDM_MAXEXTENSIONS,
		D3D_DEBUG_COMMAND_NOT_IMPLEMENTED = XBDM_D3D_DEBUG_COMMAND_NOT_IMPLEMENTED,
		D3D_INVALID_SURFACE = XBDM_D3D_INVALID_SURFACE,
		CANNOTCONNECT = XBDM_CANNOTCONNECT,
		CONNECTIONLOST = XBDM_CONNECTIONLOST,
		FILEERROR = XBDM_FILEERROR,
		ENDOFLIST = XBDM_ENDOFLIST,
		BUFFER_TOO_SMALL = XBDM_BUFFER_TOO_SMALL,
		NOTXBEFILE = XBDM_NOTXBEFILE,
		MEMSETINCOMPLETE = XBDM_MEMSETINCOMPLETE,
		NOXBOXNAME = XBDM_NOXBOXNAME,
		NOERRORSTRING = XBDM_NOERRORSTRING,
		INVALIDSTATUS = XBDM_INVALIDSTATUS,
		TASK_PENDING = XBDM_TASK_PENDING,

		CONNECTED = XBDM_CONNECTED,
		MULTIRESPONSE = XBDM_MULTIRESPONSE,
		BINRESPONSE = XBDM_BINRESPONSE,
		READYFORBIN = XBDM_READYFORBIN,
		DEDICATED = XBDM_DEDICATED,
		PROFILERESTARTED = XBDM_PROFILERESTARTED,
		FASTCAPENABLED = XBDM_FASTCAPENABLED,
		CALLCAPENABLED = XBDM_CALLCAPENABLED,
	};

	public ref class XenonConsole
	{
	private:
		static Object ^mSymbolSync = gcnew Object();

		System::String ^mTargetManagerName;
		XDevkit::XboxConsole ^mConsole;
		XDevkit::IXboxDebugTarget ^mDebugTarget;
		bool mCurrentlyConnected;
		bool mCurrentlyDebugging;
		XDevkit::XboxDumpFlags mDumpType;
		XDevkit::XboxExecutionState mExecState;
		System::DateTime mLastExceptionTime;
		FConsoleSupport::ECrashReportFilter mCrashFilter;

		TTYEventCallbackPtr mTxtCallback;
		CrashCallbackPtr mCrashCallback;

		XDevkit::XboxEvents_OnStdNotifyEventHandler^ mOnStdNotify;
		XDevkit::XboxEvents_OnTextNotifyEventHandler^ mOnTxtNotify;

		// These are global mutexes, with the 'target manager name' appended to limit their scope per-target
		// 
		// There is a regular mutex and a trump mutex.  Only the trump mutex allows the process to handle remote-triggered
		// actions like a crash dump or profiling trace.
		//
		// Only a process with a TTY handler can own the trump mutex long-term, otherwise the threads try to own the regular mutex
		// long-term, and borrow the trump mutex whenever processing needs to be done.  The regular mutex prevents non-TTY processes
		// from just sequentially filing in to process the same event.
		//
		// This system is safe against two different processes running at the same time and receiving the notify event in either order
		// with any relative timing, but isn't safe against a TTY process starting up and getting a notify N at some point after a
		// non-TTY process handles N.  In this case, the two will both handle the event, but they do so sequentially, so the sharing
		// violations would still be avoided.  The window where the newly started process gets the notify N queued up but hasn't acquired
		// the TTY lock should be small in any event.
		System::Threading::Mutex^ mHandleRemoteActionsMutex;
		bool mHandleRemoteActionsMutexHeld;

		System::Threading::Mutex^ mTrumpHandleRemoteActionsMutex;
		bool mTrumpHandleRemoteActionsMutexHeld;
	private:
		/// <summary>
		/// Event handler for system events from the target.
		/// </summary>
		/// <param name="EventCode">The type of event that has occured.</param>
		/// <param name="EventInfo">Additional information about the event.</param>
		void OnStdNotify(XDevkit::XboxDebugEventType EventCode, XDevkit::IXboxEventInfo ^EventInfo);

		/// <summary>
		/// Event handler for notifications from the game.
		/// </summary>
		/// <param name="Source">The channel the notification is occuring on.</param>
		/// <param name="Notification">The notification message.</param>
		void OnTextNotify(System::String ^Source, System::String ^Notification);

		/// <summary>
		/// Determines an appropriate application to view the transferred file, based on Data->Type, and
		/// launches that application (if any) with an appropriate command line to display the file.
		/// </summary>
		/// <param name="Data">Context information about the thread launching the application.</param>
		/// <param name="LocalFilename">Path to the filename to view.</param>
		static bool LaunchApplicationOnTransferredFile(ProfileThreadData^ Data, System::String^ LocalFilename);

		/// <summary>
		/// Launches a specified application with a specified command line.
		/// </summary>
		/// <param name="Data">Context information about the thread launching the application.</param>
		/// <param name="ExePath">Path to the application to execute.</param>
		/// <param name="CmdLine">The commandline for the application being executed.</param>
		static bool LaunchApplication(ThreadData ^Data, System::String ^ExePath, System::String ^CmdLine);

		/// <summary>
		/// Main entry point for any operation for a specific target that occurs on a worker thread.
		/// </summary>
		/// <param name="lpParameter">Pointer to extra thread data.</param>
		static void XenonConsole::ThreadMain(Object ^lpParameter);

		/// <summary>
		/// Handles a crash on a target.
		/// </summary>
		/// <param name="Target">The target that the crash occured on.</param>
		/// <param name="BaseThreadData">Context information for the thread.</param>
		static DWORD CrashThreadProc(XDevkit::XboxConsole ^Target, ThreadData ^BaseThreadData);

		/// <summary>
		/// Thread callback for processing a profile.
		/// </summary>
		/// <param name="Target">The target that generated the profile.</param>
		/// <param name="BaseThreadData">Contextual information.</param>
		static DWORD ProfileThreadProc(XDevkit::XboxConsole ^Target, ThreadData ^BaseThreadData);

		/// <summary>
		/// Generates the call stack for a thread that has crashed.
		/// </summary>
		/// <param name="Thread">The thread the crash occured on.</param>
		/// <param name="Buffer">Receives the addresses of the callstack.</param>
		static void ParseStackFrame(XDevkit::IXboxThread ^Thread, System::Collections::Generic::List<DWORD> ^Buffer);

		/// <summary>
		/// Copies the pdb file for the currently running process of the supplied target.
		/// </summary>
		/// <param name="Target">The target that the crash occured on.</param>
		/// <param name="Data">Context information for the thread.</param>
		/// <param name="SymbolPath">The override symbol path to use.</param>
		/// <param name="LocalExe">The local exe name.</param>
		static bool InternalRetrievePdbFile(XDevkit::XboxConsole ^Target, ThreadData ^Data, System::String ^%SymbolPath, System::String ^%LocalExe, System::String ^LocalPathOverride);

		/// Returns true if this process is allowed to handle remote triggered events
		bool ShouldProcessNotify();

		/// This should be called after the critical section protected by ShouldProcessNotify().
		void CleanupAfterProcessingNotify();
	public:
		property System::String^ TargetManagerName
		{
			System::String^ get();
		}

		property XDevkit::XboxConsole^ Console
		{
			XDevkit::XboxConsole^ get();
		}

		property XDevkit::IXboxDebugTarget^ DebugTarget
		{
			XDevkit::IXboxDebugTarget^ get();
		}

		property XDevkit::XboxDumpFlags DumpFlags
		{
			XDevkit::XboxDumpFlags get();
			void set(XDevkit::XboxDumpFlags value);
		}

		property XDevkit::XboxExecutionState ExecState
		{
			XDevkit::XboxExecutionState get();
		}

		property FConsoleSupport::ECrashReportFilter CrashFilter
		{
			FConsoleSupport::ECrashReportFilter get();
			void set(FConsoleSupport::ECrashReportFilter value);
		}

		property System::IntPtr TTYCallback
		{
			System::IntPtr get();
			void set(System::IntPtr value);
		}

		property System::IntPtr CrashCallback
		{
			System::IntPtr get();
			void set(System::IntPtr value);
		}

		property bool IsConnected
		{
			bool get();
		}

	public:
		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="TMName">The name of the target as its known to the target manager (Xbox Neighborhood).</param>
		/// <param name="ConsolePtr">A pointer to the target.</param>
		XenonConsole(System::String ^TMName, XDevkit::XboxConsole ^ConsolePtr);

		/// <summary>
		/// Connects to the console as a debugger.
		/// </summary>
		/// <param name="DebuggerName">The name that the debug connection will be registered under.</param>
		void ConnectAsDebugger(System::String ^DebuggerName);

		/// <summary>
		/// Disconnects from the console as a debugger.
		/// </summary>
		void DisconnectAsDebugger();

		/// <summary>
		/// Sends a command to the console.
		/// <param name="Cmd">The command to be sent.</param>
		System::String^ SendCommand(System::String ^Cmd);

		/// <summary>
		/// Outputs an exception to the system debug output.
		/// </summary>
		/// <param name="Message">Information about the exception.</param>
		/// <param name="ex">The exception to be output.</param>
		static void OutputDebugException(System::String ^Message, System::Exception ^ex);

		/// <summary>
		/// Determines if a file on the local HD and a file on the console are not identical and therefore require syncing.
		/// </summary>
		/// <param name="Target">The console to check.</param>
		/// <param name="SourceFilename">The source file on the local HD.</param>
		/// <param name="DestFilename">The destination file on the console.</param>
		/// <param name="bReverse">Reverse the dest and source file names.</param>
		static bool NeedsToCopyFile(XDevkit::XboxConsole ^Target, System::String ^SourceFilename, System::String ^DestFilename, bool bReverse);

		/// <summary>
		/// Retrieves a file from a target console to the host PC, logging about the copy and any errors that occurred using the supplied TTY callback.
		/// Returns true if the copy succeeded, or the copy was skipped because the files matched and bForceCopy = false.
		/// </summary>
		/// <param name="Target">The console to copy from.</param>
		/// <param name="LocalPath">The source file on the console.</param>
		/// <param name="RemotePath">The destination file on the local HD.</param>
		/// <param name="TTYFunc">The TTY callback to use for logging (can be NULL).</param>
		/// <param name="bForceCopy">If TRUE, the file is always copied.  If FALSE, NeedsToCopyFile() is called to determine if the copy is needed.</param>
		static bool RetrieveFileFromTarget(XDevkit::XboxConsole^ Target, System::String^ LocalPath, System::String^ RemotePath, TTYEventCallbackPtr TTYFunc, bool bForceCopy);

		/// <summary>
		/// Converts and XBDM HRESULT to a string.
		/// </summary>
		/// <param name="Result">The HRESULT to get a string for.</param>
		static System::String^ XBDMHResultToString(HRESULT Result);

		/// <summary>
		/// Standardizes a remote path so that it is always fully rooted with xe:\
		/// </summary>
		static System::String^ StandardizeRemotePathToXe(System::String^ RemotePath);
	};
}

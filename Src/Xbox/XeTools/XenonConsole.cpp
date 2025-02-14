/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include "XenonConsole.h"
#include <vcclr.h>
#include <msclr\auto_handle.h>

using namespace System;
using namespace System::Reflection;

#define MS_VC_EXCEPTION 0x406D1388
#define EXCEPTION_INTERVAL 10

#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif

#ifdef GetEnvironmentVariable
#undef GetEnvironmentVariable
#endif

namespace XeTools
{
	// properties

	enum XBDMCodes
	{
		Success = 0x2da0000,
		Error = 0x82da0000,
		NoError = Success,
		CannotAccess = Error | 0x00E,
		NoSuchFile = Error | 0x002,
		NoThread = Error | 0x005,
		NoSuchPath = Error | 0x01E,
		CannotConnect = Error | 0x100,
		ConnectionLost = Error| 0x101,
		NoXboxName = Error | 0x108
	};

	String^ XenonConsole::TargetManagerName::get()
	{
		return mTargetManagerName;
	}

	XDevkit::XboxConsole^ XenonConsole::Console::get()
	{
		return mConsole;
	}

	XDevkit::IXboxDebugTarget^ XenonConsole::DebugTarget::get()
	{
		return mDebugTarget;
	}

	XDevkit::XboxDumpFlags XenonConsole::DumpFlags::get()
	{
		return mDumpType;
	}

	void XenonConsole::DumpFlags::set(XDevkit::XboxDumpFlags value)
	{
		mDumpType = value;
	}

	XDevkit::XboxExecutionState XenonConsole::ExecState::get()
	{
		return mExecState;
	}

	FConsoleSupport::ECrashReportFilter XenonConsole::CrashFilter::get()
	{
		return mCrashFilter;
	}

	void XenonConsole::CrashFilter::set(FConsoleSupport::ECrashReportFilter value)
	{
		mCrashFilter = value;
	}

	IntPtr XenonConsole::TTYCallback::get()
	{
		return IntPtr(mTxtCallback);
	}

	void XenonConsole::TTYCallback::set(IntPtr value)
	{
		mTxtCallback = (TTYEventCallbackPtr)value.ToPointer();
	}

	IntPtr XenonConsole::CrashCallback::get()
	{
		return IntPtr(mCrashCallback);
	}

	void XenonConsole::CrashCallback::set(IntPtr value)
	{
		mCrashCallback = (CrashCallbackPtr)value.ToPointer();
	}

	bool XenonConsole::IsConnected::get()
	{
		return mCurrentlyConnected;
	}

	// functions

	/// <summary>
	/// Assembly resolve method to pick correct StandaloneSymbolParser DLL
	/// </summary>
	System::Reflection::Assembly^ CurrentDomain_AssemblyResolve(System::Object^, System::ResolveEventArgs^ args)
	{
		// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
		array<String^> ^AssemblyInfo = args->Name->Split(gcnew array<Char> { ',' });
		String^ AssemblyName = AssemblyInfo[0];

		if (AssemblyName->Equals("standalonesymbolparser", StringComparison::InvariantCultureIgnoreCase))
		{
			Char Buffer[MAX_PATH];
			GetModuleFileNameW(NULL, Buffer, MAX_PATH);
			String^ StartupPath = IO::Path::GetDirectoryName(gcnew String(Buffer));

			if (IntPtr::Size == 8)
			{
				AssemblyName = StartupPath + "\\Win64\\" + AssemblyName + ".dll";
			}
			else
			{
				AssemblyName = StartupPath + "\\Win32\\" + AssemblyName + ".dll";
			}

			//Debug.WriteLineIf( System.Diagnostics.Debugger.IsAttached, "Loading assembly: " + AssemblyName );

			return System::Reflection::Assembly::LoadFile(AssemblyName);
		}

		return nullptr;
	}

	/// <summary>
	/// Constructor.
	/// </summary>
	/// <param name="TMName">The name of the target as its known to the target manager (Xbox Neighborhood).</param>
	/// <param name="ConsolePtr">A pointer to the target.</param>
	XenonConsole::XenonConsole(System::String ^TMName, XDevkit::XboxConsole ^ConsolePtr)
		: mTargetManagerName(TMName)
		, mConsole(ConsolePtr)
		, mDebugTarget(ConsolePtr->DebugTarget)
		, mCurrentlyConnected(false)
		, mCurrentlyDebugging(false)
		, mDumpType(XDevkit::XboxDumpFlags::Normal)
		, mExecState(XDevkit::XboxExecutionState::Stopped)
		, mLastExceptionTime(DateTime::Now.Subtract(TimeSpan(0, 0, EXCEPTION_INTERVAL)))
		, mCrashFilter(FConsoleSupport::CRF_All)
		, mTxtCallback(NULL)
		, mCrashCallback(NULL)
		, mHandleRemoteActionsMutex(nullptr)
		, mHandleRemoteActionsMutexHeld(false)
		, mTrumpHandleRemoteActionsMutex(nullptr)
		, mTrumpHandleRemoteActionsMutexHeld(false)
	{
		if(TMName == nullptr)
		{
			throw gcnew ArgumentNullException("TMName");
		}

		if(ConsolePtr == nullptr)
		{
			throw gcnew ArgumentNullException("ConsolePtr");
		}

		AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler(&CurrentDomain_AssemblyResolve);
	}

	/// <summary>
	/// Connects to the console as a debugger.
	/// </summary>
	/// <param name="DebuggerName">The name that the debug connection will be registered under.</param>
	void XenonConsole::ConnectAsDebugger(System::String ^DebuggerName)
	{
		if(!mCurrentlyConnected)
		{
			mDebugTarget->ConnectAsDebugger(DebuggerName, XDevkit::XboxDebugConnectFlags::Force);
			mCurrentlyConnected = true;

			String^ MutexName = TEXT("XeTools_NotifyMutex_") + mTargetManagerName;
			String^ SuperMutexName = TEXT("XeTools_TtyNotifyMutex_") + mTargetManagerName;

			// Create (but do not lock) the global mutexes
			mHandleRemoteActionsMutex = gcnew Threading::Mutex(false, MutexName);
			mTrumpHandleRemoteActionsMutex = gcnew Threading::Mutex(false, SuperMutexName);

			mCurrentlyDebugging = true;
			if (mCurrentlyDebugging)
			{
				mOnStdNotify = gcnew XDevkit::XboxEvents_OnStdNotifyEventHandler(this, &XenonConsole::OnStdNotify);
				mConsole->OnStdNotify += mOnStdNotify;

				mOnTxtNotify = gcnew XDevkit::XboxEvents_OnTextNotifyEventHandler(this, &XenonConsole::OnTextNotify);
				mConsole->OnTextNotify += mOnTxtNotify;
			}
		}
	}

	/// <summary>
	/// Disconnects from the console as a debugger.
	/// </summary>
	void XenonConsole::DisconnectAsDebugger()
	{
		if (mCurrentlyConnected)
		{
			mCurrentlyConnected = false;

			if(mCurrentlyDebugging)
			{
				mCurrentlyDebugging = false;
				mConsole->OnStdNotify -= mOnStdNotify;
				mConsole->OnTextNotify -= mOnTxtNotify;
				mOnStdNotify = nullptr;
				mOnTxtNotify = nullptr;
			}

			// Release the global mutexes
			if (mHandleRemoteActionsMutexHeld)
			{
				mHandleRemoteActionsMutex->ReleaseMutex();
				mHandleRemoteActionsMutexHeld = false;
			}
			mHandleRemoteActionsMutex->Close();
			mHandleRemoteActionsMutex = nullptr;

			if (mTrumpHandleRemoteActionsMutexHeld)
			{
				mTrumpHandleRemoteActionsMutex->ReleaseMutex();
				mTrumpHandleRemoteActionsMutexHeld = false;
			}
			mTrumpHandleRemoteActionsMutex->Close();
			mTrumpHandleRemoteActionsMutex = nullptr;

			//
			mDebugTarget->DisconnectAsDebugger();
		}
	}

	/// <summary>
	/// Sends a command to the console.
	/// </summary>
	/// <param name="Cmd">The command to be sent.</param>
	System::String^ XenonConsole::SendCommand(System::String ^Cmd)
	{
		String ^Response = String::Empty;

		DWORD Connection = mConsole->OpenConnection("UNREAL");
		mConsole->SendTextCommand(Connection, Cmd, Response);
		mConsole->CloseConnection(Connection);

		return Response;
	}

	/// <summary>
	/// Outputs an exception to the system debug output.
	/// </summary>
	/// <param name="Message">Information about the exception.</param>
	/// <param name="ex">The exception to be output.</param>
	void XenonConsole::OutputDebugException(System::String ^Message, System::Exception ^ex)
	{
		pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("(XeTools.dll) {0}:\r\n", Message));
		pin_ptr<const wchar_t> NativeException = PtrToStringChars(ex->ToString() + "\r\n");

		OutputDebugStringW(NativeMsg);
		OutputDebugStringW(NativeException);
	}

	/// <summary>
	/// Determines if a file on the local HD and a file on the console are not identical and therefore require syncing.
	/// </summary>
	/// <param name="Target">The console to check.</param>
	/// <param name="SourceFilename">The source file on the local HD.</param>
	/// <param name="DestFilename">The destination file on the console.</param>
	/// <param name="bReverse">Reverse the dest and source file names.</param>
	bool XenonConsole::NeedsToCopyFile( XDevkit::XboxConsole^ Target, String^ SourceFilename, String^ DestFilename, bool bReverse )
	{
		pin_ptr<const wchar_t> NativeSourceFileName = PtrToStringChars(SourceFilename);

		// make sure the local file exists
		WIN32_FILE_ATTRIBUTE_DATA LocalAttrs;
		if(GetFileAttributesExW(NativeSourceFileName, GetFileExInfoStandard, &LocalAttrs) == 0)
		{
			// If they must be identical, then we would want to copy in this case.
			// (The identical case determines if we need to copy from XDK to local)
			return bReverse;
		}

		System::String ^RemoteFullPath = DestFilename;
		if (RemoteFullPath->StartsWith("xe:\\") == false)
		{
			RemoteFullPath = System::IO::Path::Combine("xe:\\", DestFilename);
		}
		XDevkit::IXboxFile ^XeDestinationFile = Target->GetFileObject(RemoteFullPath);

		unsigned __int64 XboxFileSize = XeDestinationFile->Size;

		// get local file size
		ULONGLONG LocalFileSize = ((ULONGLONG)LocalAttrs.nFileSizeHigh << 32) | ((ULONGLONG)LocalAttrs.nFileSizeLow);
		if(XboxFileSize != LocalFileSize)
		{
			return true;
		}

		// get the xbox file time
		VARIANT Variant;
		System::Runtime::InteropServices::Marshal::GetNativeVariantForObject(XeDestinationFile->ChangeTime, IntPtr(&Variant));

		// convert it to be useful
		SYSTEMTIME XboxSystemTime;
		VariantTimeToSystemTime(Variant.dblVal, &XboxSystemTime);

		TIME_ZONE_INFORMATION TimeZoneInfo;
		GetTimeZoneInformation(&TimeZoneInfo);

		SYSTEMTIME XboxUTCTime;
		TzSpecificLocalTimeToSystemTime(&TimeZoneInfo, &XboxSystemTime, &XboxUTCTime);

		FILETIME XboxFileTime;
		SystemTimeToFileTime(&XboxUTCTime, &XboxFileTime);

		// compare file times to see if local file is newer
		long FileTimeCmp = CompareFileTime(&XboxFileTime, &LocalAttrs.ftLastWriteTime);
		if ((!bReverse && (FileTimeCmp < 0)) ||
			(bReverse && (FileTimeCmp > 0)))
		{
			return true;
		}

		// if we get here, the files are the same
		return false;
	}

	/// <summary>
	/// Retrieves a file from a target console to the host PC, logging about the copy and any errors that occurred using the supplied TTY callback.
	/// Returns true if the copy succeeded, or the copy was skipped because the files matched and bForceCopy = false.
	/// </summary>
	/// <param name="Target">The console to copy from.</param>
	/// <param name="LocalPath">The source file on the console.</param>
	/// <param name="RemotePath">The destination file on the local HD.</param>
	/// <param name="TTYFunc">The TTY callback to use for logging (can be NULL).</param>
	/// <param name="bForceCopy">If TRUE, the file is always copied.  If FALSE, NeedsToCopyFile() is called to determine if the copy is needed.</param>
	bool XenonConsole::RetrieveFileFromTarget(XDevkit::XboxConsole^ Target, String^ LocalPath, String^ RemotePath, TTYEventCallbackPtr TTYFunc, bool bForceCopy)
	{
		// Determine if the file needs to be copied
		bool bNeedsToCopy = bForceCopy;
		if (!bNeedsToCopy)
		{
			try
			{
				bNeedsToCopy = NeedsToCopyFile(Target, LocalPath, RemotePath, true);
			}
			catch(System::Runtime::InteropServices::COMException ^comex)
			{
				if(comex->ErrorCode == NoSuchFile)
				{
					return false;
				}

				OutputDebugException(__FUNCTION__, comex);

				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("NeedsToCopyFile() threw an exception \'{0}\'\r\n", comex->ToString()));
				if(TTYFunc)
				{
					TTYFunc(NativeMsg);
				}
			}
			catch(Exception ^ex)
			{
				OutputDebugException(__FUNCTION__, ex);

				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("NeedsToCopyFile() threw an exception \'{0}\'\r\n", ex->ToString()));
				if(TTYFunc)
				{
					TTYFunc(NativeMsg);
				}
			}
		}

		if (bNeedsToCopy)
		{
			bool bSucceeded = false;

			// Print out that we are copying a file
			if(TTYFunc)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Copying \'{0}\' to \'{1}\'\r\n", RemotePath, LocalPath));
				TTYFunc(NativeMsg);
			}

			// Try to copy the file
			try
			{
				Target->ReceiveFile(LocalPath, RemotePath);
				bSucceeded = TRUE;
			}
			catch(Exception ^ex)
			{
				OutputDebugException(__FUNCTION__, ex);

				if(TTYFunc)
				{
					pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Failed to copy \'{0}\'\r\n", RemotePath));
					TTYFunc(NativeMsg);

					pin_ptr<const wchar_t> NativeException = PtrToStringChars(" Reason: " + ex->ToString() + "\r\n");
					TTYFunc(NativeException);
				}
			}

			return bSucceeded;
		}
		else
		{
			// Print out that we are skipping this copy
			if(TTYFunc)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Skipping copy of \'{0}\' to \'{1}\' as the two files match.\r\n", RemotePath, LocalPath));
				TTYFunc(NativeMsg);
			}

			return true;
		}
	}

	/// Returns true if this process is allowed to handle remote triggered events
	bool XenonConsole::ShouldProcessNotify()
	{
		bool OwnsStandardMutex = false;
		bool OwnsTrumpMutex = false;
		bool BorrowingTrumpMutex = false;

		if (mTxtCallback)
		{
			// Try to lock the TTY trump mutex
			OwnsTrumpMutex = mTrumpHandleRemoteActionsMutexHeld;
			if (!mTrumpHandleRemoteActionsMutexHeld)
			{
				OwnsTrumpMutex = mTrumpHandleRemoteActionsMutexHeld = mTrumpHandleRemoteActionsMutex->WaitOne(0) == true;
			}
		}
		else
		{
			// Try to lock the regular mutex
			OwnsStandardMutex = mHandleRemoteActionsMutexHeld;
			if (!mHandleRemoteActionsMutexHeld)
			{
				OwnsStandardMutex = mHandleRemoteActionsMutexHeld = mHandleRemoteActionsMutex->WaitOne(0) == true;
			}

			// Try to lock the TTY mutex (if we can't lock this; a process with TTY out exists and 'trumps' this one
			BorrowingTrumpMutex = mTrumpHandleRemoteActionsMutex->WaitOne(0) == true;

			// Record that we locked it temporarily so we can release it
			mTrumpHandleRemoteActionsMutexHeld = BorrowingTrumpMutex;
		}

		return OwnsTrumpMutex || (OwnsStandardMutex && BorrowingTrumpMutex);
	}

	/// This should be called after the critical section protected by ShouldProcessNotify().
	void XenonConsole::CleanupAfterProcessingNotify()
	{
		// Release the trump mutex if we borrowed it
		bool BorrowingTrumpMutex = mHandleRemoteActionsMutexHeld && mTrumpHandleRemoteActionsMutexHeld;
		if (BorrowingTrumpMutex)
		{
			mTrumpHandleRemoteActionsMutex->ReleaseMutex();
			mTrumpHandleRemoteActionsMutexHeld = false;
		}
	}

	/// <summary>
	/// Event handler for system events from the target.
	/// </summary>
	/// <param name="EventCode">The type of event that has occured.</param>
	/// <param name="EventInfo">Additional information about the event.</param>
	void XenonConsole::OnStdNotify(XDevkit::XboxDebugEventType EventCode, XDevkit::IXboxEventInfo ^EventInfo)
	{
		bool AllowedToProcess = ShouldProcessNotify();

		try
		{
			XDevkit::XBOX_EVENT_INFO Info = EventInfo->default;

			switch(EventCode)
			{
			case XDevkit::XboxDebugEventType::AssertionFailed:
			case XDevkit::XboxDebugEventType::AssertionFailedEx:
			case XDevkit::XboxDebugEventType::RIP:
			case XDevkit::XboxDebugEventType::Exception:
				{
					TimeSpan Interval = DateTime::Now.Subtract(mLastExceptionTime);

					if(AllowedToProcess && Interval.TotalSeconds >= EXCEPTION_INTERVAL && !(EventCode == XDevkit::XboxDebugEventType::Exception && Info.Code == MS_VC_EXCEPTION))
					{
						if(this->mTxtCallback)
						{
							this->mTxtCallback(L"A crash has been detected and UnrealConsole will now attempt to parse its symbols and generate a mini-dump.\r\n");
						}

						// update this here in case we hit an early exit condition as well as at the end
						mLastExceptionTime = DateTime::Now;

						XDevkit::XBOX_PROCESS_INFO ProcInfo = mConsole->RunningProcessInfo;

						String ^ModuleName = System::IO::Path::GetFileNameWithoutExtension(ProcInfo.ProgramName);

						if(ModuleName->IndexOf("Xbox360-Debug", StringComparison::OrdinalIgnoreCase) != -1 && !(mCrashFilter & FConsoleSupport::CRF_Debug))
						{
							break;
						}
						else if(ModuleName->IndexOf("Xbox360-Release", StringComparison::OrdinalIgnoreCase) != -1 && !(mCrashFilter & FConsoleSupport::CRF_Release))
						{
							break;
						}
						else if(ModuleName->IndexOf("Xbox360-Shipping", StringComparison::OrdinalIgnoreCase) != -1 && !(mCrashFilter & FConsoleSupport::CRF_Shipping))
						{
							break;
						}
						else if(ModuleName->IndexOf("Xbox360-Test", StringComparison::OrdinalIgnoreCase) != -1 && !(mCrashFilter & FConsoleSupport::CRF_Test))
						{
							break;
						}

						CrashThreadData ^Data = gcnew CrashThreadData();
						Data->Console = this;
						Data->CrashFunc = this->mCrashCallback;
						Data->TTYFunc = this->mTxtCallback;
						Data->ThreadCallback = gcnew ThreadCallbackDelegate(&XenonConsole::CrashThreadProc);
						Data->DumpType = this->mDumpType;

						int GameNameEnd = ModuleName->LastIndexOf("Game", StringComparison::OrdinalIgnoreCase);

						if(GameNameEnd != -1)
						{
							Data->GameName = ModuleName->Substring(0, GameNameEnd);
						}
						else
						{
							Data->GameName = ModuleName;
						}


						Data->CallStack->Add(Info.Address);
						ParseStackFrame(Info.Thread, Data->CallStack);

						IntPtr NativeTMName = System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(mTargetManagerName);

						// Load the current xbox into the DM API. It's ok to do this here because this is in the STA and is guaranteed to be thread safe
						DmSetXboxNameNoRegister((LPCSTR)NativeTMName.ToPointer());

						System::Runtime::InteropServices::Marshal::FreeHGlobal(NativeTMName);
						NativeTMName = IntPtr::Zero;

						DM_PDB_SIGNATURE NativeSignature;
						for each(XDevkit::IXboxModule ^CurModule in mDebugTarget->Modules)
						{
							XDevkit::XBOX_MODULE_INFO ModuleInfo = CurModule->default;

							if(SUCCEEDED(DmFindPdbSignature((PVOID)((__int64)ModuleInfo.BaseAddress), &NativeSignature)))
							{
								PDBEntry ^SymbolEntry = gcnew PDBEntry();
								SymbolEntry->BaseAddress = ModuleInfo.BaseAddress;
								SymbolEntry->Size = ModuleInfo.Size;
								SymbolEntry->PDBSig.Age = NativeSignature.Age;
								SymbolEntry->PDBSig.Path = gcnew String(NativeSignature.Path);
								SymbolEntry->PDBSig.Guid = System::Guid(NativeSignature.Guid.Data1, NativeSignature.Guid.Data2, NativeSignature.Guid.Data3, NativeSignature.Guid.Data4[0], NativeSignature.Guid.Data4[1],
									NativeSignature.Guid.Data4[2], NativeSignature.Guid.Data4[3], NativeSignature.Guid.Data4[4], NativeSignature.Guid.Data4[5], NativeSignature.Guid.Data4[6], NativeSignature.Guid.Data4[7]);

								Data->ModulePDBList->Add(SymbolEntry);
							}
						}

						System::Threading::Thread ^NewThread = gcnew System::Threading::Thread(gcnew System::Threading::ParameterizedThreadStart(&XenonConsole::ThreadMain));
						NewThread->ApartmentState = System::Threading::ApartmentState::STA;
						NewThread->IsBackground = true;
						NewThread->Start(Data);

						// put this here again to update it as it's possible all of this parsing took a while
						mLastExceptionTime = DateTime::Now;
					}

					break;
				}
			case XDevkit::XboxDebugEventType::ExecStateChange:
				{
					// as long as this is called from the STA we don't need synchronization here
					mExecState = Info.ExecState;

					break;
				}
			case XDevkit::XboxDebugEventType::DebugString:
				{
					if(mTxtCallback)
					{
						try
						{
							pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(Info.Message);
							mTxtCallback(NativeMsg);
						}
						catch(Exception ^ex)
						{
							OutputDebugException("OnStdNotify() Could not display TTY output", ex);
						}
					}
					
					break;
				}
			}
		}
		catch(Exception ^ex)
		{
			OutputDebugException(__FUNCTION__, ex);
		}

		// Release the trump mutex if we borrowed it
		CleanupAfterProcessingNotify();
	}

	/// <summary>
	/// Event handler for notifications from the game.
	/// </summary>
	/// <param name="Source">The channel the notification is occuring on.</param>
	/// <param name="Notification">The notification message.</param>
	void XenonConsole::OnTextNotify(System::String ^Source, System::String ^Notification)
	{
		try
		{
			if(!Source->Equals("UE_PROFILER", StringComparison::OrdinalIgnoreCase))
			{
				return;
			}

			// Determine the type of profile...
			array<String^> ^NotificationSplit = Notification->Split(gcnew array<Char> { ':' }, StringSplitOptions::RemoveEmptyEntries);

			// We expect 2 ':". One after UE_PROFILER and one after the type. There might be a 3rd for fully qualified paths.
			BOOL bFullFilenameProvided = FALSE;
			if(NotificationSplit->Length != 2 && NotificationSplit->Length != 3)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Invalid format of notification string: {0}\n", Notification));
				OutputDebugStringW(NativeMsg);
				return;
			}
			// Special case handling of string containing ':' for fully qualified path. Skip drive letter as it's handled later.
			else if(NotificationSplit->Length == 3)
			{
				String^ GameDrive = gcnew String("GAME");
				bFullFilenameProvided = NotificationSplit[1] != GameDrive;
				NotificationSplit[1] = NotificationSplit[2];
			}

			String ^EventType = NotificationSplit[0];
			String ^FilenameStr = NotificationSplit[1];

			FConsoleSupport::EProfileType ProfType = FConsoleSupport::PT_Invalid;

			// now EventType which ProfileType this is based on what the engine has passed into us
			if(EventType->Equals("RENDER", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_RenderTick;
			}
			else if(EventType->Equals("GAME", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_GameTick;
			}
			else if(EventType->Equals("SCRIPT", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_Script;
			}
			else if(EventType->Equals("MEMORY", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_Memory;
			}
			else if(EventType->Equals("UE3STATS", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_UE3Stats;
			}
			else if(EventType->Equals("MEMLEAK", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_MemLeaks;
			}
			else if(EventType->Equals("FPSCHART", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_FPSCharts;
			}
			else if(EventType->Equals("BUGIT", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_BugIt;
			}
			else if(EventType->Equals("MISCFILE", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_MiscFile;
			}
			else if(EventType->Equals("NETWORK", StringComparison::OrdinalIgnoreCase))
			{
				ProfType = FConsoleSupport::PT_Network;
			}

			if(ProfType == FConsoleSupport::PT_Invalid)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Invalid ProfilerType in notification string: {0}\n", Notification));
				OutputDebugStringW(NativeMsg);

				return;
			}

			// Deal(aka remove) with any 'special' stuff tagged on the front of the filename...
			{
				String^ badPrefix1 = gcnew String("..\\..\\");
				String^ badPrefix2 = gcnew String("\\");
				if (FilenameStr->IndexOf(badPrefix1) == 0)
				{
					FilenameStr = FilenameStr->Substring(badPrefix1->Length);
				}
				else if (FilenameStr->IndexOf(badPrefix2) == 0)
				{
					FilenameStr = FilenameStr->Substring(badPrefix2->Length);
				}
			}

			// Now determine if we should actually act on the message
			bool AllowedToProcess = ShouldProcessNotify();
			
			if (AllowedToProcess)
			{
				ProfileThreadData ^Data = gcnew ProfileThreadData();
				Data->Console = this;
				Data->TTYFunc = this->mTxtCallback;
				Data->FileName = FilenameStr;
				Data->Type = ProfType;
				Data->bFullFileNameProvided = bFullFilenameProvided;
				Data->ThreadCallback = gcnew ThreadCallbackDelegate(&XenonConsole::ProfileThreadProc);

				System::Threading::Thread ^NewThread = gcnew System::Threading::Thread(gcnew System::Threading::ParameterizedThreadStart(&XenonConsole::ThreadMain));

				NewThread->Start(Data);
				delete NewThread;
			}

			// Release the trump mutex if we borrowed it
			CleanupAfterProcessingNotify();
		}
		catch(Exception ^ex)
		{
			OutputDebugException(__FUNCTION__, ex);
		}
	}

	/// <summary>
	/// Generates the call stack for a thread that has crashed.
	/// </summary>
	/// <param name="Thread">The thread the crash occured on.</param>
	/// <param name="Buffer">Receives the addresses of the callstack.</param>
	void XenonConsole::ParseStackFrame(XDevkit::IXboxThread ^Thread, System::Collections::Generic::List<DWORD> ^Buffer)
	{
		// get top of stack
		XDevkit::IXboxStackFrame ^StackFrame = Thread->TopOfStack;
		bool bReachedTop = false;

		// walk the stack until we reach the top or we are out of space in the buffer
		// sometimes when handling multiple crashes in a row StackFrame can receive a NULL pointer for some crazy reason so we want to check for it
		while(!bReachedTop && StackFrame)
		{
			// get the function address for the stack
			DWORD Address = StackFrame->ReturnAddress;

			if(Address == 0)
			{
				bReachedTop = true;
			}
			else
			{
				Buffer->Add(Address);

				try
				{
					StackFrame = StackFrame->NextStackFrame;
				}
				catch(System::Exception^)
				{
					StackFrame = nullptr;
					bReachedTop = true;
				}
			}
		}
	}

	/// <summary>
	/// Handles a crash on a target.
	/// </summary>
	/// <param name="Target">The target that the crash occured on.</param>
	/// <param name="BaseThreadData">Context information for the thread.</param>
	DWORD XenonConsole::CrashThreadProc(XDevkit::XboxConsole ^Target, ThreadData ^BaseThreadData)
	{
		CrashThreadData ^Data = (CrashThreadData^)BaseThreadData;
		String ^SymbolPath;
		String ^LocalExe;

		bool bHasPdb = false;

		try
		{
			bHasPdb = InternalRetrievePdbFile(Target, Data, SymbolPath, LocalExe, String::Empty);
		}
		catch(Exception ^ex)
		{
			OutputDebugException(__FUNCTION__, ex);
		}

		if(SymbolPath->Length == 0)
		{
			SymbolPath = "Xbox360";
		}

		DWORD RetCode = 0;
		array<StandaloneSymbolParser::ModuleInfo^> ^ModuleList = gcnew array<StandaloneSymbolParser::ModuleInfo^>(Data->ModulePDBList->Count);

		StandaloneSymbolParser::XenonSymbolParser ^SymbolParser = gcnew StandaloneSymbolParser::XenonSymbolParser();
		SymbolParser->SymbolServer = String::Format("{0};{1}", SymbolParser->SymbolServer, SymbolPath);

		for(int ModuleIndex = 0; ModuleIndex < Data->ModulePDBList->Count; ++ModuleIndex)
		{
			PDBEntry ^CurEntry = Data->ModulePDBList[ModuleIndex];

			ModuleList[ModuleIndex] = gcnew StandaloneSymbolParser::ModuleInfo(CurEntry->BaseAddress, CurEntry->Size, CurEntry->PDBSig.Age, CurEntry->PDBSig.Guid, CurEntry->PDBSig.Path);
		}

		System::Text::StringBuilder ^Bldr = gcnew System::Text::StringBuilder();
		String ^FileName = String::Empty;
		String ^Function = String::Empty;
		int LineNumber = 0;

		System::Threading::Monitor::Enter(mSymbolSync);

		try
		{
			SymbolParser->LoadSymbols(LocalExe, ModuleList);

			for each(DWORD Address in Data->CallStack)
			{
				SymbolParser->ResolveAddressToSymboInfo(Address, FileName, Function, LineNumber);

				Bldr->Append(SymbolParser->FormatCallstackLine(FileName, Function, LineNumber));
			}

			SymbolParser->UnloadSymbols();
		}
		catch(Exception ^ex)
		{
			if(Data->TTYFunc)
			{
				pin_ptr<const wchar_t> Desc = PtrToStringChars(ex->ToString());
				Data->TTYFunc(Desc);
			}
		}
		finally
		{
			// NOTE: You must delete this before LeaveCriticalSection() as it may need to call UnloadSymbols() if an exception occurred after LoadSymbols()
			delete SymbolParser;
			SymbolParser = nullptr;
			System::Threading::Monitor::Exit(mSymbolSync);
		}

		String ^DumpPath = String::Empty;

		try
		{
			XDevkit::IXboxDebugTarget ^DbgTarget = Target->DebugTarget;
			DumpPath = "Xbox360\\Dumps\\" + DbgTarget->default;

			System::IO::Directory::CreateDirectory(DumpPath);

			DumpPath += String::Format("\\UnrealConsole_{0}.dmp", DateTime::Now.ToString("yyyy-M-d_H-m-s"));
#ifdef _WIN64
			DbgTarget->ConnectAsDebugger("Xbox360Tools_x64.dll", XDevkit::XboxDebugConnectFlags::Force);
#else
			DbgTarget->ConnectAsDebugger("Xbox360Tools.dll", XDevkit::XboxDebugConnectFlags::Force);
#endif
			DbgTarget->WriteDump(DumpPath, Data->DumpType);
			DbgTarget->DisconnectAsDebugger();
		}
		catch(Exception ^ex)
		{
			OutputDebugException("XenonConsole::CrashThreadProc(): Failed to initiate crash dump", ex);

			if(Data->TTYFunc)
			{
				Data->TTYFunc(L"Failed to initiate crash dump!\r\n");
			}
		}

		pin_ptr<const wchar_t> FinalCallstack = PtrToStringChars(Bldr->ToString());
		pin_ptr<const wchar_t> DumpFileName = PtrToStringChars(DumpPath);
		pin_ptr<const wchar_t> GameName = PtrToStringChars(Data->GameName);

		if(Data->CrashFunc)
		{
			Data->CrashFunc(GameName, FinalCallstack, DumpFileName);
		}
		else if(Data->TTYFunc)
		{
			Data->TTYFunc(L"\r\n");
			Data->TTYFunc(FinalCallstack);
		}

		return RetCode;
	}

	/// <summary>
	/// Main entry point for any operation for a specific target that occurs on a worker thread.
	/// </summary>
	/// <param name="lpParameter">Pointer to extra thread data.</param>
	void XenonConsole::ThreadMain(Object ^lpParameter)
	{
		try
		{
			ThreadData ^Data = (ThreadData^)lpParameter;
			XDevkit::XboxManagerClass ^XenonManager = gcnew XDevkit::XboxManagerClass();

			for each(String ^CurConsoleName in XenonManager->Consoles)
			{
				// TMName only gets set once when the object is created so this shouldn't be an issue
				if(CurConsoleName->Equals(Data->Console->TargetManagerName, StringComparison::OrdinalIgnoreCase))
				{
					try
					{
						Data->ThreadCallback(XenonManager->OpenConsole(CurConsoleName), Data);
					}
					catch(Exception ^ex)
					{
						OutputDebugException(__FUNCTION__, ex);
					}
					break;
				}
			}
		}
		catch(Exception ^ex)
		{
			OutputDebugException(__FUNCTION__, ex);
		}
	}

	/// <summary>
	/// Copies the pdb file for the currently running process of the supplied target.
	/// </summary>
	/// <param name="Target">The target that the crash occured on.</param>
	/// <param name="Data">Context information for the thread.</param>
	/// <param name="SymbolPath">The override symbol path to use.</param>
	/// <param name="LocalExe">The local exe name.</param>
	bool XenonConsole::InternalRetrievePdbFile(XDevkit::XboxConsole ^Target, ThreadData ^Data, System::String ^%SymbolPath, System::String ^%LocalExe, System::String ^LocalPathOverride)
	{
		XDevkit::XBOX_PROCESS_INFO Info = Target->RunningProcessInfo;
		String ^XexName = System::IO::Path::GetFileNameWithoutExtension(Info.ProgramName);
		String ^LocalDir = System::IO::Directory::GetCurrentDirectory();

		LocalExe = System::IO::Path::Combine(System::IO::Path::GetFullPath(".."), XexName + ".xex");

		if(LocalPathOverride != nullptr && LocalPathOverride->Length > 0)
		{
			SymbolPath = LocalPathOverride;
		}
		else
		{
			// Set the PDBLocal directory, and create it if it is not present
			SymbolPath = System::IO::Path::Combine(LocalDir, "Xbox360\\Dumps");
			System::IO::Directory::CreateDirectory(SymbolPath);
		}

		if(LocalPathOverride == nullptr || LocalPathOverride->Length == 0)
		{
			SymbolPath = System::IO::Path::Combine(SymbolPath, Target->DebugTarget->default);
		}

		System::IO::Directory::CreateDirectory(SymbolPath);

		String ^LocalPDBFile = System::IO::Path::Combine(SymbolPath, XexName + ".pdb");

		String ^DirectoryName = System::IO::Path::GetDirectoryName(Info.ProgramName);
		array<String^>^ DirectoriesToTry = gcnew array<String^> 
		{
			DirectoryName,
			System::IO::Path::Combine(DirectoryName, ".."),
			System::IO::Path::Combine(DirectoryName, "Xbox360"),
			"xe:" + System::IO::Path::DirectorySeparatorChar + DirectoryName->Substring(DirectoryName->IndexOf(":")+2),
			"dvd:" + System::IO::Path::DirectorySeparatorChar + DirectoryName->Substring(DirectoryName->IndexOf(":")+2),
			"xe:" + System::IO::Path::DirectorySeparatorChar,
			"dvd:" + System::IO::Path::DirectorySeparatorChar,
			"update:" + System::IO::Path::DirectorySeparatorChar,
			"A:" + System::IO::Path::DirectorySeparatorChar
		};

		for each(String ^DirectoryToCheck in DirectoriesToTry)
		{
			String^ ActualDirectoryToCheck = System::IO::Path::Combine(DirectoryToCheck, XexName + ".pdb");

			if(RetrieveFileFromTarget(Target, LocalPDBFile, ActualDirectoryToCheck, Data->TTYFunc, /*bForceCopy=*/false))
			{
				return true;
			}
		}

		SymbolPath += ";" + System::IO::Directory::GetCurrentDirectory() + ";" +
			System::IO::Path::Combine(System::IO::Directory::GetCurrentDirectory(), "..") + ";" +
			System::IO::Path::Combine(System::IO::Directory::GetCurrentDirectory(), "Xbox360");

		return false;
	}

	/// <summary>
	/// Standardizes a remote path so that it is always fully rooted with xe:\
	/// </summary>
	String^ XenonConsole::StandardizeRemotePathToXe(String^ RemotePath)
	{
		if(System::IO::Path::IsPathRooted(RemotePath))
		{
			return "xe:" + RemotePath->Remove(0, 2);
		}
		else
		{
			return System::IO::Path::Combine("xe:\\", RemotePath);
		}
	}

	/// <summary>
	/// Thread callback for processing a profile.
	/// </summary>
	/// <param name="Target">The target that generated the profile.</param>
	/// <param name="BaseThreadData">Contextual information.</param>
	DWORD XenonConsole::ProfileThreadProc(XDevkit::XboxConsole ^Target, ThreadData ^BaseThreadData)
	{
		if(!Target)
		{
			return 1;
		}

		bool bSuccess = true;

		try
		{
			ProfileThreadData ^Data = (ProfileThreadData^)BaseThreadData;

			bool bRequiresPDB = false;

			// Determine the remote source directory
			XDevkit::XBOX_PROCESS_INFO Info = Target->DebugTarget->RunningProcessInfo;
			String ^RemoteDir;

			RemoteDir = Data->bFullFileNameProvided ? System::IO::Path::GetPathRoot(Info.ProgramName) : System::IO::Path::GetDirectoryName(Info.ProgramName);
			RemoteDir = StandardizeRemotePathToXe(RemoteDir);

			// Determine the local destination directory
			String ^LocalDir = System::IO::Path::Combine(System::IO::Directory::GetCurrentDirectory(), Target->DebugTarget->default);
			LocalDir = System::IO::Path::Combine(LocalDir, "Profiling");

			switch (Data->Type)
			{
			case FConsoleSupport::PT_Script:
				LocalDir = System::IO::Path::Combine(LocalDir, "Script");
				break;
			case FConsoleSupport::PT_GameTick:
			case FConsoleSupport::PT_RenderTick:
				bRequiresPDB = true;
				LocalDir = System::IO::Path::Combine(LocalDir, "Trace");
				break;
			case FConsoleSupport::PT_Memory:
				bRequiresPDB = true;
				LocalDir = System::IO::Path::Combine(LocalDir, "Memory");
				break;
			case FConsoleSupport::PT_UE3Stats:
				LocalDir = System::IO::Path::Combine(LocalDir, "UE3Stats");
				break; 
			case FConsoleSupport::PT_MemLeaks:
				LocalDir = System::IO::Path::Combine(LocalDir, "MemLeaks");
				break; 
			case FConsoleSupport::PT_FPSCharts:
				LocalDir = System::IO::Path::Combine(LocalDir, "FPSCharts");
				break; 
			case FConsoleSupport::PT_BugIt:
				LocalDir = System::IO::Path::Combine(LocalDir, "BugIt");
				break; 
			case FConsoleSupport::PT_MiscFile:
				LocalDir = System::IO::Path::Combine(LocalDir, "MiscFiles");
				break; 
			case FConsoleSupport::PT_Network:
				LocalDir = System::IO::Path::Combine(LocalDir, "Network");
				break; 
			}

			// Ensure that LocalDir exists
			System::IO::Directory::CreateDirectory(LocalDir);

			// Determine LocalSubDirPath
			String ^LocalSubDirPath = System::IO::Path::GetDirectoryName(Data->FileName);

			if(LocalSubDirPath != nullptr && LocalSubDirPath->Length > 0)
			{
				int DirIndex = LocalSubDirPath->LastIndexOf("\\", StringComparison::OrdinalIgnoreCase);
				
				if(DirIndex != -1)
				{
					LocalSubDirPath = LocalSubDirPath->Substring(DirIndex + 1);
					LocalDir = System::IO::Path::Combine(LocalDir, LocalSubDirPath);
					LocalSubDirPath = LocalDir;

					System::IO::Directory::CreateDirectory(LocalDir);
				}
			}
			else
			{
				LocalSubDirPath = LocalDir;
			}

			// Store off the local path to use for the local file
			String^ LocalFilename = System::IO::Path::Combine(LocalDir, System::IO::Path::GetFileName(Data->FileName));


			String^ RemoteFilename = System::IO::Path::Combine(RemoteDir, Data->FileName);

			// Copy the file from the console to the PC
			bool bSuccessOnMainCopy = RetrieveFileFromTarget(Target, LocalFilename, RemoteFilename, Data->TTYFunc, /*bForceCopy=*/false);
			if (!bSuccessOnMainCopy)
			{
				return 1;
			}

			//@TODO: The following will get to the XEX (and PDB) on the host PC, and could be used as a source for a regular copy
			// instead of copying from the remote target in order to speed up the process a bit
			// *under the non-trivial assumption that the build we're connected to was just launched from this PC*
			// String^ LocalExe = System::IO::Path::Combine(System::IO::Path::GetFullPath(".."), XexName + ".xex");

			// If useful for the type of file just transferred, also copy the "cached"/"local" version of the .pdb and .xex
			// to the same local location so there is a nice little package which will persist through time
			if(bRequiresPDB)
			{
				//@TODO: Figure out why we're using LocalSubDirPath here instead of LocalDir
				String^ RemoteProgramDir = System::IO::Path::GetDirectoryName(Info.ProgramName);
				String^ ExecutableNameNoExt = System::IO::Path::GetFileNameWithoutExtension(Info.ProgramName);

				// Copy the XEX back to the PC
				String^ XexFilename = ExecutableNameNoExt + ".xex";
				String^ RemoteExeFilename = System::IO::Path::Combine(RemoteProgramDir, XexFilename);
				String^ LocalExeFilename = System::IO::Path::Combine(LocalSubDirPath, XexFilename);

				RetrieveFileFromTarget(Target, LocalExeFilename, RemoteExeFilename, Data->TTYFunc, /*bForceCopy=*/false);

				// Copy the .PDB back to the PC
				String^ PdbFilename = ExecutableNameNoExt + ".pdb";
				String^ RemotePdbFilename = System::IO::Path::Combine(RemoteProgramDir, PdbFilename);
				String^ LocalPdbFilename = System::IO::Path::Combine(LocalSubDirPath, PdbFilename);

				RetrieveFileFromTarget(Target, LocalPdbFilename, RemotePdbFilename, Data->TTYFunc, /*bForceCopy=*/false);
			}


			// Try to launch something to view the file
			bSuccess = LaunchApplicationOnTransferredFile(Data, LocalFilename);
		}
		catch(Exception ^ex)
		{
			bSuccess = false;
			OutputDebugException(__FUNCTION__, ex);
		}

		return bSuccess ? 0 : 1;
	}

	/// <summary>
	/// Determines an appropriate application to view the transferred file, based on Data->Type, and
	/// launches that application (if any) with an appropriate command line to display the file.
	/// </summary>
	/// <param name="Data">Context information about the thread launching the application.</param>
	/// <param name="LocalFilename">Path to the filename to view.</param>
	bool XenonConsole::LaunchApplicationOnTransferredFile(ProfileThreadData^ Data, System::String^ LocalFilename)
	{
		bool bSuccess = false;

		// Quote the filename
		LocalFilename = String::Format("\"{0}\"", LocalFilename);

		switch(Data->Type)
		{
		case FConsoleSupport::PT_GameTick:
		case FConsoleSupport::PT_RenderTick:
			{
				// look for the DLL in the installed path
				String ^ExePath = System::IO::Path::Combine(Environment::GetEnvironmentVariable("XEDK"), "Bin\\Win32\\PIX.exe");
				bSuccess = LaunchApplication(Data, ExePath, LocalFilename);

				break;
			}
			// refactor this to be function
		case FConsoleSupport::PT_Script:
			{
				bSuccess = LaunchApplication(Data, "GameplayProfiler.exe", LocalFilename);

				break;
			}
		case FConsoleSupport::PT_Memory:
			{
				// Check to make sure it's the final file in the sequence
				if (System::IO::Path::GetExtension(LocalFilename) == ".mprof")
				{
					bSuccess = LaunchApplication(Data, "MemoryProfiler2.exe", LocalFilename);
				}
				break;
			}
		case FConsoleSupport::PT_UE3Stats:
			{
				bSuccess = LaunchApplication(Data, "StatsViewer.exe", LocalFilename);

				break;
			}
		case FConsoleSupport::PT_MemLeaks:
			{
				bSuccess = LaunchApplication(Data, "Notepad.exe", LocalFilename);

				break;
			}
		case FConsoleSupport::PT_FPSCharts:
			{
				// do nothing atm they just want them copied
				break;
			}
		case FConsoleSupport::PT_BugIt:
			{
				// do nothing atm they just want them copied
				break;
			}
		case FConsoleSupport::PT_MiscFile:
			{
				// do nothing atm they just want them copied
				break;
			}
		case FConsoleSupport::PT_Network:
			{
				// Upload to the DB.
				bSuccess = LaunchApplication(Data, "NetworkProfiler.exe", LocalFilename);
				break;
			}
		}

		return bSuccess;
	}

	/// <summary>
	/// Generates the call stack for a thread that has crashed.
	/// </summary>
	/// <param name="Data">Context information about the thread launching the application.</param>
	/// <param name="ExePath">Path to the application to execute.</param>
	/// <param name="CmdLine">The commandline for the application being executed.</param>
	bool XenonConsole::LaunchApplication(ThreadData ^Data, System::String ^ExePath, System::String ^CmdLine)
	{
		bool bSucceeded = false;

		try
		{
			System::Diagnostics::Process ^Proc = System::Diagnostics::Process::Start(ExePath, CmdLine);
			bSucceeded = true;

			if(Proc != nullptr)
			{
				delete Proc;
			}

			if(Data->TTYFunc)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Profile opened with \'{0} {1}\'\r\n", ExePath, CmdLine));
				Data->TTYFunc(NativeMsg);
			}
		}
		catch(Exception ^ex)
		{
			OutputDebugException(__FUNCTION__, ex);

			if(Data->TTYFunc)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(String::Format("Could not start \'{0}\'\r\n", System::IO::Path::GetFileName(ExePath)));
				Data->TTYFunc(NativeMsg);
			}
		}

		return bSucceeded;
	}

	/// <summary>
	/// Converts and XBDM HRESULT to a string.
	/// </summary>
	/// <param name="Result">The HRESULT to get a string for.</param>
	System::String^ XenonConsole::XBDMHResultToString(HRESULT Result)
	{
		if(Enum::IsDefined(XBDMErrorCodes::typeid, Result))
		{
			return ((XBDMErrorCodes)Result).ToString();
		}

		return Result.ToString("X");
	}
}

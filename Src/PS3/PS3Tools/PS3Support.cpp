/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#define _WIN32_DCOM

#include <winsock2.h>
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <set>
#include <string>
#include <Ws2tcpip.h>
#include <vcclr.h>

using namespace std;
using namespace System;
using namespace System::Text;

#include "PS3Tools.h"

#if !PS3MOD
#include <ps3tmapi.h>
#endif

#if PS3MOD
typedef int HTARGET;
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/**
 * Conversion routines from string to wstring and back
 */
inline void wstring2string(string& Dest, const wstring& Src)
{
	Dest.assign(Src.begin(), Src.end());
}

inline void string2wstring(wstring& Dest, const string& Src)
{
	Dest.assign(Src.begin(), Src.end());
}


enum EMenuItem
{
	MI_Reboot,
	MI_Run,
	MI_Sep1,
//	MI_RebootOnPOPS3,
//	MI_WriteCommandLine,
//	MI_UseDebugElf,
//	MI_Sep2,
	MI_SetIPAddress,
	MI_SetBaseDirectory,
	MI_Sep3,
	MI_Help,
	MI_Sep4,
	MI_FirstTarget,
};
const wchar_t* Key_BaseDirectory = L"BaseDirectory";
const wchar_t* Key_IPAddress = L"IPAddress";

#define TTY_BUFFER_SIZE 65536


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


class FPS3Target
{
public:
	/**
	 * Constructor
	 */
	FPS3Target(HTARGET InTargetID, const char* InName, bool InIsDebugKit) : TxtCallback(NULL), CrashCallback(NULL), CrashFilter(FConsoleSupport::CRF_All)
	{
		TargetID = InTargetID;
		// copy thwe name and convert to wide string
		string2wstring(Name, InName);

		bIsDebugKit = InIsDebugKit;
		if (bIsDebugKit)
		{
			DisplayName = Name + wstring(L"*");
		}
		else
		{
			DisplayName = Name;
		}

		bWasConnected = false;
		bWasTTYRegistered = false;
		
		// by default, not running
		ConsoleState = FConsoleSupport::CS_NotRunning;

		AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler(&CurrentDomain_AssemblyResolve);
	}

	/** Target name */
	wstring Name;

	/** Target name */
	wstring DisplayName;

	/** Handle to the target */
	HTARGET TargetID;

	/** Type of target */
	bool bIsDebugKit;

	/** Is the target connected? */
	bool bWasConnected;

	/** Has the TTY callback been registered? */
	bool bWasTTYRegistered;

	/** Have we received thec allback that the screenshot has been created? */
	bool bIsScreenshotReady;

	/** The IP address sent from PS3 on the special channel */
	string CachedGameIP;

	/** Did we crash */
	FConsoleSupport::EConsoleState ConsoleState;

	/** ProcessID we crashed on */
	UINT32 CrashedProcessID;

	/** ID of the crashed thread */
	UINT64 CrashedThreadID;

	/** PC of crashed thread */
	UINT64 CrashedPC;

	/** The callback to be called when text output has been received. */
	TTYEventCallbackPtr TxtCallback;

	/** The callback to be called when a crash has occured. */
	CrashCallbackPtr CrashCallback;

	/** Buffers a crash callstack for the crash event handler. */
	wstring CrashCallstackBuffer;

	/** This is the buffer for TTY output as only complete lines are outputted. */
	string TTYBuffer;

	/** Holds the addresses of a callstack */
	vector<DWORD64> Callstack;
	vector<DWORD64> Lv2Callstack;

	/** Filters crash report based on exe configuration. */
	DWORD CrashFilter;

	void ParseLine(const string &Line);
	bool GetExePath(string &OutPath);
	void OnTTY(const wchar_t *Txt);
	void OnTTY(const string &Txt);
	bool GenerateCallstack(std::vector<DWORD64> &CallstackAddresses);
};

bool FPS3Target::GenerateCallstack(std::vector<DWORD64> &CallstackAddresses)
{
	string ExePath;
	bool bSucceeded = false;

	if(CrashCallback && GetExePath(ExePath))
	{
		try
		{
			StandaloneSymbolParser::PS3SymbolParser ^SymbolParser = gcnew StandaloneSymbolParser::PS3SymbolParser();

			if(SymbolParser->LoadSymbols(gcnew String(ExePath.c_str()), gcnew array<StandaloneSymbolParser::ModuleInfo^>(0)))
			{
				StringBuilder ^Bldr = gcnew StringBuilder();
				String ^FileName = String::Empty;
				String ^Function = String::Empty;
				int LineNumber = 0;

				try
				{
					for(size_t CallStackIndex = 0; CallStackIndex < CallstackAddresses.size(); ++CallStackIndex)
					{
						SymbolParser->ResolveAddressToSymboInfo(CallstackAddresses[CallStackIndex], FileName, Function, LineNumber);
						Bldr->Append(SymbolParser->FormatCallstackLine(FileName, Function, LineNumber));
					}

					bSucceeded = true;
				}
				finally
				{
					SymbolParser->UnloadSymbols();
				}

				String ^GameName = System::IO::Path::GetFileNameWithoutExtension(gcnew String(ExePath.c_str()));
				int GameNameIndex = GameName->LastIndexOf("Game-PS3", StringComparison::OrdinalIgnoreCase);

				if(GameNameIndex != -1)
				{
					GameName = GameName->Substring(0, GameNameIndex);
				}

				pin_ptr<const wchar_t> GameNamePtr = PtrToStringChars(GameName);
				pin_ptr<const wchar_t> FinalCallstack = PtrToStringChars(Bldr->ToString());

				CrashCallback(GameNamePtr, FinalCallstack, L"");
			}
		}
		catch(Exception ^ex)
		{
			pin_ptr<const wchar_t> Txt = PtrToStringChars(ex->ToString());
			OnTTY(Txt);
		}

		OnTTY("\r\n");
	}
	else if(CrashCallback)
	{
		OnTTY(L"Couldn't get the path to the executable of the currently running process!\r\n");
	}

	return bSucceeded;
}

void FPS3Target::ParseLine(const string &Line)
{
	const string::size_type CALLSTACK_LEN = strlen("[PS3Callstack]");
	const string::size_type LV2CALLSTACK_LEN = strlen("lv2(2): #   0x");

	DWORD64 Lv2Address = 0;
	DWORD Address = 0;

#if _DEBUG
	OutputDebugStringA(Line.c_str());
#endif

	OnTTY(Line);

	string::size_type TokIndex = Line.find("[PS3Callstack]");

	while(TokIndex != string::npos)
	{
		if(sscanf_s(&Line[TokIndex], "[PS3Callstack] %10x", &Address) == 1)
		{
			if(Address != 0)
			{
				Callstack.push_back(Address);
			}
			else
			{
				OnTTY(L"\r\nStack Trace:\r\n");
				
				if(!GenerateCallstack(Callstack))
				{
					OnTTY(L"A problem occured while generating the callstack!");
				}

				CrashCallstackBuffer.clear();
				Callstack.clear();
				break;
			}
		}

		TokIndex = Line.find("[PS3Callstack]", TokIndex + CALLSTACK_LEN);
	}

	TokIndex = Line.find("lv2(2): #   0x");

	while(TokIndex != string::npos)
	{
		if(sscanf_s(&Line[TokIndex], "lv2(2): #   %18I64x", &Lv2Address) == 1)
		{
			if((Lv2Address & 0xbadadd0000000000) == 0xbadadd0000000000)
			{
				OnTTY(L"\r\nLV2 Stack Trace:\r\n");

				if(!GenerateCallstack(Lv2Callstack))
				{
					OnTTY(L"A problem occured while generating the LV2 callstack!");
				}

				Lv2Callstack.clear();
				break;
			}
			else
			{
				Lv2Callstack.push_back(Lv2Address);
			}
		}

		TokIndex = Line.find("lv2(2): #   0x", TokIndex + LV2CALLSTACK_LEN);
	}
}

bool FPS3Target::GetExePath(string &OutPath)
{
	// get running exe name
	UINT BufferSize = 64;
	UINT Buffer[64];
	SNRESULT Res = SNPS3ProcessList(TargetID, &BufferSize, Buffer);

	// this should succeed on devkits, but not test kits
	if(Res == SN_S_OK)
	{
		// get the first process info
		BufferSize = sizeof(SNPS3PROCESSINFO_HDR) + 512;
		SNPS3PROCESSINFO* ProcessInfo = (SNPS3PROCESSINFO*)malloc(BufferSize);
		memset(ProcessInfo, 0, BufferSize);
		Res = SNPS3ProcessInfo(TargetID, Buffer[0], &BufferSize, ProcessInfo);
		// make sure the buffer was big enough
		if (Res == SN_E_OUT_OF_MEM)
		{
			// if not, free old one and try again
			free(ProcessInfo);
			ProcessInfo = (SNPS3PROCESSINFO*)malloc(BufferSize);
			Res = SNPS3ProcessInfo(TargetID, Buffer[0], &BufferSize, ProcessInfo);
			if (Res != SN_S_OK)
			{
				free(ProcessInfo);
				return false;
			}
		}

		// copy out the executable name
		OutPath = ProcessInfo->Hdr.szPath;

		// strip off the /app_home/ junk
		OutPath.replace(0, strlen("/app_home/"), "");

		// free allocated memory
		free(ProcessInfo);
	}

	// convert .elf to .xelf
	if (OutPath.length() > 4 && _stricmp(OutPath.substr(OutPath.length() - 4, 4).c_str(), ".elf") == 0)
	{
		OutPath.replace(OutPath.length() - 4, 4, ".xelf");
	}

	return true;
}

void FPS3Target::OnTTY(const wchar_t *Txt)
{
	if(TxtCallback)
	{
		TxtCallback(Txt);
	}
}

void FPS3Target::OnTTY(const string &Txt)
{
	wstring Final;

	string2wstring(Final, Txt);
	OnTTY(Final.c_str());
}

vector<FPS3Target> PS3Targets;
set<HTARGET> PS3Handles;

int EnumCallBack(HTARGET TargetID)
{
#ifndef PS3MOD
	SNPS3TargetInfo TargetInfoQuery;


	// fill out a query TI
	TargetInfoQuery.hTarget = TargetID;
	TargetInfoQuery.nFlags = SN_TI_TARGETID;
	TargetInfoQuery.pszName = NULL;
	TargetInfoQuery.pszType = NULL;

	// get the info 
	if (PS3Handles.find(TargetID) == PS3Handles.end() && SNPS3GetTargetInfo(&TargetInfoQuery) == SN_S_OK)
	{
		// make a new target
		bool bIsDebugKit = _stricmp(TargetInfoQuery.pszType, "PS3_DECI_NET") == 0 || _stricmp(TargetInfoQuery.pszType, "PS3_DBG_DEX") == 0;
		FPS3Target Target(TargetID, TargetInfoQuery.pszName, bIsDebugKit);

		// add it to the list
		PS3Targets.push_back(Target);
		PS3Handles.insert(TargetID);

		// 0 is success - keep enumerating
		return 0;
	}
#endif

	// stop enumerating on error
	return 1;
}

#ifndef PS3MOD
// TTY Event Callback
void __stdcall TTYEventCallback(HTARGET /*TargetID*/, unsigned int /*EventType*/, unsigned int Event, SNRESULT /*Result*/, unsigned int Length, BYTE* Data, void* User)
{
	// get the user data for the target
	FPS3Target* Target = (FPS3Target*)User;
	char* Text = (char*)Data;

	// special channel for IP address reporting
	if (Event == 17)
	{
		//		MessageBox(NULL, "Special data", Text, MB_OK);
		if (_strnicmp(Text, "GAMEIP", 6) == 0)
		{
			Target->CachedGameIP = Text + 6;
		}
		else if (_strnicmp(Text, "SCREENSHOTREADY", 15) == 0)
		{
			Target->bIsScreenshotReady = true;
		}
		return;
	}

	// don't do any further processing of text if there is no one that cares
	if (Target->TxtCallback == NULL)
	{
		return;
	}

	/*Target->ParseCallstack( Text, Length );
	Target->ParseLv2Callstack( Text, Length );*/

	string TxtBuf(Text, Length);
	string::size_type LastLineIndex = TxtBuf.find_last_of('\n');

	if(LastLineIndex == string::npos)
	{
		Target->TTYBuffer += TxtBuf;
	}
	else
	{
		string LineSegment = TxtBuf.substr(0, LastLineIndex + 1);

		if(Target->TTYBuffer.size() > 0)
		{
			Target->TTYBuffer += LineSegment;
			Target->ParseLine(Target->TTYBuffer);
		}
		else
		{
			Target->ParseLine(LineSegment);
		}

		Target->TTYBuffer = TxtBuf.substr(LastLineIndex + 1);
	}

	/*OutputDebugStringA(TxtBuf.c_str());*/

	/*if(TTYCallbackPtr)
	{
		TTYCallbackPtr(Data, Length);
	}*/
}

// Target Event Callback
void TargetEventCallback(HTARGET /*TargetID*/, unsigned int EventType, unsigned int /*Event*/, SNRESULT Result, unsigned int /*Length*/, BYTE* Data, void* User)
{
	// make sure its the stuff we want
	if (SN_FAILED(Result))
	{
		return;
	}

	if (EventType != SN_EVENT_TARGET)
	{
		return;
	}

	// get event header info
	SN_EVENT_TARGET_HDR *Header = (SN_EVENT_TARGET_HDR*)Data;

	// we only want target specific events
	if (Header->uEvent != SN_TGT_EVENT_TARGET_SPECIFIC)
	{
		return;
	}

	// get the user data for the target
	FPS3Target* Target = (FPS3Target*)User;

	// extended debug event info
	SNPS3_DBG_EVENT_HDR* DebugHeader = (SNPS3_DBG_EVENT_HDR*)(Data + sizeof(SN_EVENT_TARGET_HDR));
	SNPS3_DBG_EVENT_DATA* DebugData = (SNPS3_DBG_EVENT_DATA*)(Data + sizeof(SN_EVENT_TARGET_HDR) + sizeof(SNPS3_DBG_EVENT_HDR));

	bool bWasCrashed = false;
	// handle the events we care about
	switch (DebugData->uEventType)
	{
		// handle the crashes
		case SNPS3_DBG_EVENT_PPU_EXP_TRAP:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_trap.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_PREV_INT:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_prev_int.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_ALIGNMENT:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_alignment.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_ILL_INST:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_ill_inst.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_TEXT_HTAB_MISS:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_text_htab_miss.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_TEXT_SLB_MISS:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_text_slb_miss.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_DATA_HTAB_MISS:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_data_htab_miss.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_FLOAT:	
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_float.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_DATA_SLB_MISS:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_data_slb_miss.uPC;
			break;

		case SNPS3_DBG_EVENT_PPU_EXP_DABR_MATCH:
			bWasCrashed = true;
			Target->CrashedPC = DebugData->ppu_exc_dabr_match.uPC;
			break;

//		case SNPS3_DBG_EVENT_PPU_EXP_STOP:
//			bWasCrashed = true;
//			Target->CrashedPC = DebugData->ppu_exc_stop.uPC;
//			break;

//		case SNPS3_DBG_EVENT_PPU_EXP_STOP_INIT:
//			bWasCrashed = true;
//			Target->CrashedPC = DebugData->ppu_exc_stop_init.uPC;
//			break;

		case SNPS3_DBG_EVENT_PROCESS_CREATE:
		case SNPS3_DBG_EVENT_PPU_THREAD_CREATE:
			if ( Target->ConsoleState != FConsoleSupport::CS_Asserted && Target->ConsoleState != FConsoleSupport::CS_Crashed )
			{
				Target->ConsoleState = FConsoleSupport::CS_Running;
			}
			break;

		case SNPS3_DBG_EVENT_PROCESS_EXIT:
		case SNPS3_DBG_EVENT_PROCESS_KILL:
			if ( Target->ConsoleState != FConsoleSupport::CS_Asserted && Target->ConsoleState != FConsoleSupport::CS_Crashed )
			{
				Target->ConsoleState = FConsoleSupport::CS_NotRunning;
			}
			break;


		// for other types, do nothing
		default:
			break;
	}

	if (bWasCrashed)
	{
		// all of the EXP events have the thread ID as the first parameter, so just use the SNPS3_PPU_EXP_TRAP_DATA, why not?
		Target->CrashedThreadID = DebugData->ppu_exc_trap.uPPUThreadID;
		Target->CrashedProcessID = DebugHeader->uProcessID;

		// if we were running and got a crash, detect it here
		if (Target->ConsoleState == FConsoleSupport::CS_Running)
		{
			Target->ConsoleState = FConsoleSupport::CS_Crashed;
		}
	}
}
#endif //end #ifndef PS3MOD


class FPS3Support : public FConsoleSupport
{
protected:
	/** Cache the gamename coming from the editor */
	wstring GameName;

	/** Cache the configuration (debug/release) of the editor */
	wstring Configuration;

	/** Holds all the menu item strings */
	vector<wstring> MenuItems;

	/** The target the user has chosen */
	int SelectedTargetIndex;

	wstring CachedBaseDirectory;

	/** Saves the last selected menu item for the SetValueCallback */
	EMenuItem LastMenuItem;

	/** Relative path from current working directory to the binaries directory */
	wstring RelativeBinariesDir;


public:
	FPS3Support()
	{
#if !PS3MOD

//		CoInitializeEx(NULL, COINIT_MULTITHREADED);

		// Initialise PS3tmapi.
		if (SN_FAILED( SNPS3InitTargetComms() ))
		{
			OutputDebugStringW(L"(PS3Tools.dll) FPS3Support(): Failed to initialise PS3TM SDK!\n");
		}
#endif
	}

	~FPS3Support()
	{
#ifndef PS3MOD
//		CoUninitialize();
#endif
	}

	/**
	 * Retrieves a target with the specified handle.
	 *
	 * @param	Handle	The handle of the target to retrieve.
	 */
	FPS3Target* GetTarget(TARGETHANDLE Handle)
	{
		size_t Index = (size_t)Handle;

		if(Index >= 0 && Index < PS3Targets.size())
		{
			return &PS3Targets[Index];
		}

		return NULL;
	}

	/**
	 * Returns a string suitable for putting in the menu for what PS3 to reboot
	 */
	wstring GetRebootText()
	{
		// if we have any targets, use the selected one
		if (PS3Targets.size() > 0)
		{
			return wstring(L"Reboot PS3");
		}

		// otherwise, we can't do anything
		return wstring(L"<Can't reboot without a target!>");
	}

	/**
	 * Returns a string suitable for putting in the menu for what PS3 to run
	 */
	wstring GetRunText()
	{
		// if we have any targets, use the selected one
		if (PS3Targets.size() > 0)
		{
			return wstring(L"Run PS3");
		}

		// otherwise, we can't do anything
		return wstring(L"<Can't run without a target!>");
	}



	/** Returns a string with the relative path to the binaries folder prepended */
	wstring GetRelativePathString( TCHAR* InWideString )
	{
		TCHAR TempWide[1024];
		wcscpy_s( TempWide, ARRAY_COUNT( TempWide ), RelativeBinariesDir.c_str() );
		wcscat_s( TempWide, ARRAY_COUNT( TempWide ), InWideString );
		return TempWide;
	}


	int GetIniInt(const TCHAR* Key, int Default)
	{
		// this integer should never be the default for a value
		const int DUMMY_VALUE = -1234567;

		// look in the user settings
		int Value = GetPrivateProfileInt(L"PlayOnPS3", Key, DUMMY_VALUE, GetRelativePathString( L"PS3\\PlayOnPS3.ini" ).c_str() );
		// if not found, look in the default ini, which really should have it (but can still use the default if not)
		if (Value == DUMMY_VALUE)
		{
			Value = GetPrivateProfileInt(L"PlayOnPS3", Key, Default, GetRelativePathString( L"PS3\\DefaultPlayOnPS3.ini" ).c_str() );
		}
		return Value;
	}


	void GetIniString(const TCHAR* Key, const TCHAR* Default, TCHAR* Value, int ValueSize)
	{
		const TCHAR* DUMMY_VALUE = L"DUMMY";
		assert((unsigned int)ValueSize > wcslen(DUMMY_VALUE) + 1);

		GetPrivateProfileString(L"PlayOnPS3", Key, DUMMY_VALUE, Value, ValueSize, GetRelativePathString( L"PS3\\PlayOnPS3.ini" ).c_str() );
		if (wcscmp(Value, DUMMY_VALUE) == 0)
		{
			GetPrivateProfileString(L"PlayOnPS3", Key, Default, Value, ValueSize, GetRelativePathString( L"PS3\\DefaultPlayOnPS3.ini" ).c_str() );
		}
	}
	
	bool GetIniBool(const TCHAR* Key, bool Default=false)
	{
		TCHAR Value[16];
		GetIniString(Key, Default ? L"true" : L"false", Value, ARRAY_COUNT(Value));
		return wcsicmp(Value, L"true") == 0;
	}

	void SetIniInt(const TCHAR* Key, int Value)
	{
		TCHAR ValueStr[65];
		_itow(Value, ValueStr, 10);
		SetIniString(Key, ValueStr);
	}

	void SetIniString(const TCHAR* Key, const TCHAR* Value)
	{
		WritePrivateProfileString(L"PlayOnPS3", Key, Value, GetRelativePathString( L"PS3\\PlayOnPS3.ini" ).c_str() );
	}

	void SetIniBool(const TCHAR* Key, bool Value)
	{
		SetIniString(Key, Value ? L"true" : L"false");
	}

	bool FileExists(const TCHAR* Filename)
	{
		return GetFileAttributes(Filename) != INVALID_FILE_ATTRIBUTES;
	}


	/**
	 * FConsoleSupport interface
	 */
	const wchar_t* GetConsoleName()
	{
		// this is our hardcoded name
		return CONSOLESUPPORT_NAME_PS3;
	}

	/**
	 * Return the default IP address to use when sending data into the game for object propagation
	 * Note that this is probably different than the IP address used to debug (reboot, run executable, etc)
	 * the console. (required to implement)
	 *
	 * @param	Handle The handle of the console to retrieve the information from.
	 *
	 * @return	The in-game IP address of the console, in an Intel byte order 32 bit integer
	 */
	unsigned int GetIPAddress(TARGETHANDLE Handle)
	{
#ifndef PS3MOD
		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return 0;
		}

		SNPS3GamePortIPAddressData IpAddrData;
		
		if(SN_FAILED(SNPS3GetGamePortIPAddrData(Target->TargetID, "eth0", &IpAddrData)))
		{
			return 0;
		}

		return ntohl(IpAddrData.uIPAddress);
#else
		return 0;
#endif
	}

	/**
	 * Retrieves the IP address for the debug channel at the specific target.
	 *
	 * @param	Handle	The handle to the target to retrieve the IP address for.
	 */
	virtual unsigned int GetDebugChannelIPAddress(TARGETHANDLE Handle)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return 0;
		}

		addrinfo Hint;
		ZeroMemory(&Hint, sizeof(Hint));
		Hint.ai_family = PF_UNSPEC;
		Hint.ai_flags = AI_CANONNAME;
		DWORD IPAddr = 0;

		addrinfo *Result = NULL;

		string TargetName;
		wstring2string(TargetName, Target->Name);

		if(getaddrinfo(TargetName.c_str(), NULL, &Hint, &Result) == 0)
		{
			sockaddr_in *Addr = (sockaddr_in*)Result->ai_addr;
			IPAddr = Addr->sin_addr.s_addr;

			freeaddrinfo(Result);
		}

		return IPAddr;
	}

	/**
	 * Returns true if the platform uses intel byte ordering.
	 */
	bool GetIntelByteOrder()
	{
		return false;
	}

	/** Initialize the DLL with some information about the game/editor
	 *
	 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
	 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
	 */
	void Initialize(const wchar_t* InGameName, const wchar_t* InConfiguration)
	{
		// Setup relative binaries path
		RelativeBinariesDir = L"";
		String^ CWD = Environment::CurrentDirectory->ToLower();
		if( !CWD->EndsWith( "binaries" ) )
		{
			RelativeBinariesDir = L"..\\";
		}

		// get the stored value
		SelectedTargetIndex = GetIniInt(L"TargetIndex", 0);
		// set it to 0 if it's out of range
		if (SelectedTargetIndex >= (int)PS3Targets.size())
		{
			SelectedTargetIndex = 0;
		}

		// build the menu
		MenuItems.push_back(GetRebootText());
		MenuItems.push_back(GetRunText());
//		MenuItems.push_back(MENU_SEPARATOR_LABEL);
//		MenuItems.push_back(L"Reboot on POPS3?");
//		MenuItems.push_back(L"Write To PS3CommandLine.txt?");
//		MenuItems.push_back(L"Use Debug .elf?");
		MenuItems.push_back(MENU_SEPARATOR_LABEL);
		MenuItems.push_back(L"Set Live Update IP (Game IP)...");
		MenuItems.push_back(L"Set PS3 Base Directory (for HD caching)...");
		MenuItems.push_back(MENU_SEPARATOR_LABEL);
		MenuItems.push_back(L"PS3 Help");
		MenuItems.push_back(MENU_SEPARATOR_LABEL);

		// add the target names to the end
		for (size_t TargetIndex = 0; TargetIndex < PS3Targets.size(); TargetIndex++)
		{
			MenuItems.push_back(PS3Targets[TargetIndex].DisplayName);
		}

		// lookup the saved base directory
		TCHAR BaseDir[64];
		GetIniString(L"BaseDirectory", L"UnrealEngine3", BaseDir, ARRAY_COUNT(BaseDir));
		CachedBaseDirectory = BaseDir;

		if(CachedBaseDirectory.size() == 0)
		{
			CachedBaseDirectory = L"UnrealEngine3";
		}

		GameName = InGameName;
		Configuration = InConfiguration;
	}

	/**
	 * Run the game on the target console (required to implement)
	 *
	 * @param	TargetList				The list of handles of consoles to run the game on.
	 * @param	NumTargets				The number of handles in TargetList.
	 * @param	MapName					The name of the map that is going to be loaded.
	 * @param	URL						The map name and options to run the game with
	 * @param	OutputConsoleCommand	A buffer that the menu item can fill out with a console command to be run by the Editor on return from this function
	 * @param	CommandBufferSize		The size of the buffer pointed to by OutputConsoleCommand
	 *
	 * @return	Returns true if the run was successful
	 */
	bool RunGame(TARGETHANDLE *TargetList, int NumTargets, const wchar_t* MapName, const wchar_t* URL, wchar_t* OutputConsoleCommand, int CommandBufferSize)
	{
		wchar_t* URLCopy = _wcsdup(URL);
		//wchar_t* MapName = wcstok(URLCopy, L"? .#");

		// need to reboot the PS3(s) before cooking, they may have the TFC files open
		for(int i = 0; i < NumTargets; ++i)
		{
			if (ConnectToTarget(TargetList[i]) == false)
			{
				MessageBox(NULL, L"Failed to connect to a target. Check your settings and the Target Manager", L"Connect failed", MB_ICONSTOP);
				return false;
			}
			Reboot(TargetList[i]);
		}
		
		// get all the user options from the ini file
		bool bRebootPS3 = true;//GetIniBool("RebootPS3", true);
		bool bWriteCommandLine = false;//GetIniBool("WriteCommandLine", true);
		bool bUseDebugElf = false; //GetIniBool("UseDebugElf");

//		char Errors[1024];
		DWORD ReturnValue;
		wchar_t Command[1024];

		// cook the map
		swprintf(Command,
			ARRAY_COUNT(Command), 
			L"%s%s cookpackages %s -platform=ps3",
			_wcsicmp(Configuration.c_str(), L"Debug") == 0 ? L"Debug-" : L"",
			GameName.c_str(),
			MapName);
		string CommandString;
		wstring2string(CommandString, Command);

		// spawn the child process with no DOS window popping up
//		RunChildProcess(CommandString.c_str(), Errors, ARRAY_COUNT(Errors) - 1, &ReturnValue);
		ReturnValue = system(CommandString.c_str());

		if (ReturnValue != 0)
		{
			MessageBox(NULL, L"Cooking failed. See Logs\\Launch.log for more information", L"Cooking failed", MB_ICONSTOP);
			return false;
		}

		// update the TOC
		swprintf(Command,
			ARRAY_COUNT(Command), 
			L"%sCookerSync %s -p PS3 -nd",
			RelativeBinariesDir.c_str(),
			GameName.c_str());
		wstring2string(CommandString, Command);

		// spawn the child process with no DOS window popping up
//		RunChildProcess(CommandString.c_str(), Errors, ARRAY_COUNT(Errors) - 1, &ReturnValue);
		ReturnValue = system(CommandString.c_str());

		if (ReturnValue != 0)
		{
			MessageBox(NULL, L"CookerSync failed. See Logs\\Launch.log for more information", L"Synching failed", MB_ICONSTOP);
			return false;
		}

		for(int i = 0; i < NumTargets; ++i)
		{
			if (ConnectToTarget(TargetList[i]) == false)
			{
				MessageBox(NULL, L"Failed to connect to a target. Check your settings and the Target Manager", L"Connect failed", MB_ICONSTOP);
				return false;
			}

			if (bRebootPS3)
			{
				if (RebootAndRun(TargetList[i], bUseDebugElf ? L"Debug" : L"Release", CachedBaseDirectory.c_str(), GameName.c_str(), URL, false) == false)
				{
					MessageBox(NULL, L"Running failed. Check your settings and the TTY output in the Target Manager.", L"Run failed", MB_ICONSTOP);
					return false;
				}
			}
		}
		

		// if desired, save out the commandline, in case the PS3REMOTE command fails, the user can reboot the PS3 with no commandline, and it will use this map
		if (bWriteCommandLine)
		{
			wchar_t PS3CommandLineFile[1024];
			swprintf( PS3CommandLineFile, ARRAY_COUNT( PS3CommandLineFile ), L"%sPS3\\PS3CommandLine.txt", RelativeBinariesDir.c_str() );

			// open the command line file
			FILE* CommandLineFile = _wfopen(PS3CommandLineFile, L"w");
			// write out the URL to the commandline
			fwprintf(CommandLineFile, URL);
			fclose(CommandLineFile);
		}

		// attempt to tell a running PS3 to open the map if we don't reboot
		if (!bRebootPS3)
		{
			wcscpy_s(OutputConsoleCommand, CommandBufferSize, L"PS3REMOTE ");
			wcscat_s(OutputConsoleCommand, CommandBufferSize, URL);
		}

		free(URLCopy);

		return true;
	}

	/**
	 * Gets a list of targets that have been selected via menu's in UnrealEd.
	 *
	 * @param	OutTargetList			The list to be filled with target handles.
	 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
	 */
	virtual void GetMenuSelectedTargets(TARGETHANDLE *OutTargetList, int &InOutTargetListSize)
	{
		int HandlesCopied = 0;

		for(int i = 0; HandlesCopied < InOutTargetListSize && i < (int)PS3Targets.size(); ++i)
		{
			bool bIsChecked;
			bool bIsRadio;
			TARGETHANDLE Handle;

			GetMenuItem(i + MI_FirstTarget, bIsChecked, bIsRadio, Handle);

			if(bIsChecked)
			{
				OutTargetList[HandlesCopied] = (TARGETHANDLE)i;
				++HandlesCopied;
			}
		}

		InOutTargetListSize = HandlesCopied;
	}

	/**
	 * Send a text command to the target
	 * 
	 * @param Handle The handle of the console to retrieve the information from.
	 * @param Command Command to send to the target
	 */
	void SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command)
	{
#if !PS3MOD
		if (ConnectToTarget(Handle) == false)
		{
			MessageBox(NULL, L"Failed to connect to a target. Check your settings and the Target Manager", L"Connect failed", MB_ICONSTOP);
			return;
		}

		string CommandStr;
		wstring2string(CommandStr, Command);
		CommandStr = CommandStr + "\n";

		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return;
		}

		if (SN_FAILED(SNPS3SendTTY(Target->TargetID, 6, CommandStr.c_str())))
		{
			MessageBox(NULL, L"Failed to send command", Command, MB_ICONSTOP);
		}
// 		else
// 		{
// 			MessageBox(NULL, "Sent command", CommandStr.c_str(), MB_ICONSTOP);
// 		}
#endif
	}

	/**
	 * Have the console take a screenshot and dump to a file
	 * 
	 * @param Handle The handle of the console to retrieve the information from.
	 * @param Filename Location to place the .bmp file
	 *
	 * @return true if successful
	 */
	bool ScreenshotBMP(TARGETHANDLE Handle, const wchar_t* Filename)
	{
		bool bSucceeded = false;
#if !PS3MOD

		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return false;
		}

		// build a command to send to the PS3 (we run one up from Binaries, the caller is thinking this is Binaries relative)
		wstring Command = L"PS3SCREENSHOT ..\\Binaries\\";
		Command += Filename;

		// send the command
		SendConsoleCommand(Handle, Command.c_str());
		
		// wait for the screenshot to be generated
		Target->bIsScreenshotReady = false;

		// wait up to 10 seconds for a reply
		for (INT Wait = 0; Wait < 20; Wait++)
		{
			// let the TTY get processed
			SNPS3Kick();

			// did the screenshot get made?
			if (Target->bIsScreenshotReady)
			{
				bSucceeded = true;
				break;
			}

			// wait a second to try again
			Sleep(500);
		}
#endif
		return bSucceeded;
	}


	/**
	 * Retrieve the state of the console (running, not running, crashed, etc)
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 *
	 * @return the current state
	 */
	EConsoleState GetConsoleState(TARGETHANDLE Handle)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return CS_NotRunning;
		}

		return Target->ConsoleState;
	}

	/**
	 * Return the number of console-specific menu items this platform wants to add to the main
	 * menu in UnrealEd.
	 *
	 * @return	The number of menu items for this console
	 */
	int GetNumMenuItems()
	{
		return (int)MenuItems.size();
	}

	/**
	 * Return the string label for the requested menu item
	 * @param	Index		The requested menu item
	 * @param	bIsChecked	Is this menu item checked (or selected if radio menu item)
	 * @param	bIsRadio	Is this menu item part of a radio group?
	 * @param	OutHandle	Receives the handle of the target associated with the menu item.
	 *
	 * @return	Label for the requested menu item
	 */
	const wchar_t* GetMenuItem(int Index, bool& bIsChecked, bool& bIsRadio, TARGETHANDLE& OutHandle)
	{
		// set defaults
		bIsChecked = false;
		bIsRadio = false;
/*
		if (Index == MI_RebootOnPOPS3)
		{
			bIsChecked = GetIniBool("RebootPS3", true);
		}
		else if (Index == MI_WriteCommandLine)
		{
			bIsChecked = GetIniBool("WriteCommandLine", true);
		}
		else if (Index == MI_UseDebugElf)
		{
			bIsChecked = GetIniBool("UseDebugElf", false);
		}
		else 
*/
		if (Index >= MI_FirstTarget)
		{
			// is it selected?
			bIsRadio = true;
			bIsChecked = SelectedTargetIndex == (Index - MI_FirstTarget);
			OutHandle = (TARGETHANDLE)(Index - MI_FirstTarget);
		}

		return MenuItems[Index].c_str();
	}

	/**
	 * Internally process the given menu item when it is selected in the editor
	 * @param	Index		The selected menu item
	 * @param	OutputConsoleCommand	A buffer that the menu item can fill out with a console command to be run by the Editor on return from this function
	 * @param	CommandBufferSize		The size of the buffer pointed to by OutputConsoleCommand
	 */
	void ProcessMenuItem(int Index, wchar_t* OutputConsoleCommand, int CommandBufferSize)
	{
		// remember the selected item
		LastMenuItem = (EMenuItem)Index;

		if (Index == MI_Reboot)
		{
			ConnectToTarget((TARGETHANDLE)SelectedTargetIndex);
			Reboot((TARGETHANDLE)SelectedTargetIndex);
		}
		else if (Index == MI_Run)
		{
			ConnectToTarget((TARGETHANDLE)SelectedTargetIndex);
			RebootAndRun((TARGETHANDLE)SelectedTargetIndex, GetIniBool(L"UseDebugElf") ? L"Debug" : L"Release", CachedBaseDirectory.c_str(), GameName.c_str(), L"", false);
		}
/*		else if (Index == MI_RebootOnPOPS3)
		{
			SetIniBool("RebootPS3", !GetIniBool("RebootPS3", true));
		}
		else if (Index == MI_WriteCommandLine)
		{
			SetIniBool("WriteCommandLine", !GetIniBool("WriteCommandLine", true));
		}
		else if (Index == MI_UseDebugElf)
		{
			SetIniBool("UseDebugElf", !GetIniBool("UseDebugElf", false));
		}
*/		else if (Index == MI_SetBaseDirectory)
		{
			swprintf(OutputConsoleCommand, CommandBufferSize, L"QUERYVALUE %s \"Enter the Base Directory you use on the PS3 (should match CookerFrontEnd.exe)\" \"%s\"", Key_BaseDirectory, CachedBaseDirectory.c_str());
		}
		else if (Index == MI_SetIPAddress)
		{
			TCHAR IPString[64];
			GetIniString(L"PS3IPAddress", L"0.0.0.0", IPString, ARRAY_COUNT(IPString));

			swprintf(OutputConsoleCommand, CommandBufferSize, L"QUERYVALUE %s \"Enter IP Address for Live Update (shown in PS3 log, NOT the IP used in Target Manager)\" \"%s\"", Key_IPAddress, IPString);
		}
		else if (Index == MI_Help)
		{
			MessageBox(NULL, 
				L"See https://udn.epicgames.com/bin/view/Three/PlayOnPS3 for documentation.",
				L"PS3 Editor Integration Help", 
				MB_OK);
		}
		// select the target index
		else if (Index >= MI_FirstTarget)
		{
			SelectedTargetIndex = Index - MI_FirstTarget;
			SetIniInt(L"TargetIndex", SelectedTargetIndex);
		}
	}

	/**
	 * Handles receiving a value from the application, when ProcessMenuItem returns that the ConsoleSupport needs to get a value
	 * @param	Value		The actual value received from user
	 */
	void SetValueCallback(const wchar_t* Value)
	{
		if (LastMenuItem == MI_SetIPAddress)
		{
			SetIniString(L"PS3IPAddress", Value);
		}
		else if (LastMenuItem == MI_SetBaseDirectory)
		{
			CachedBaseDirectory = Value;
			SetIniString(L"BaseDirectory", Value);
		}
	}


	/**
	 * @return the number of known xbox targets
	 */
	int GetNumTargets()
	{
		return (int)PS3Targets.size();
	}

	TCHAR* GetSNResultString(SNRESULT SNResult)
	{
		switch (SNResult)
		{
 		case SN_S_OK:					return L"SN_S_OK";					// The operation completed successfully.
 		case SN_S_PENDING:				return L"SN_S_PENDING";				// The operation is pending completion.
 		case SN_S_CONNECTED:			return L"SN_S_CONNECTED";			// The local user is already connected to the target.
 		case SN_S_NO_MSG:				return L"SN_S_NO_MSG";				// There are no messages to pump. 
 		case SN_E_ERROR:				return L"SN_E_ERROR";				// Internal error.
 		case SN_E_NOT_IMPL:				return L"SN_E_NOT_IMPL";				// Function is not implemented.
 		case SN_E_TM_NOT_RUNNING:		return L"SN_E_TM_NOT_RUNNING";		// Initialization of DLL failed because the Target Manager could not be started.
 		case SN_E_BAD_TARGET:			return L"SN_E_BAD_TARGET";			// Specified target does not exist.
 		case SN_E_NOT_CONNECTED:		return L"SN_E_NOT_CONNECTED";		// Function failed because target was not connected.
 		case SN_E_COMMS_ERR:			return L"SN_E_COMMS_ERR";			// Function failed because of a communications error with the target.
 		case SN_E_TM_COMMS_ERR:			return L"SN_E_TM_COMMS_ERR";			// Function failed because of a communications error with the Target Manager. When this error occurs, you should call: SNPS3CloseTargetComms() then, SNPS3InitTargetComms() to reinitialize target communications.
 		case SN_E_TIMEOUT:				return L"SN_E_TIMEOUT";				// Timeout occurred during the execution of the function.
 		case SN_E_HOST_NOT_FOUND:		return L"SN_E_HOST_NOT_FOUND";		// During a connect operation, the host could not be found.
 		case SN_E_TARGET_IN_USE:		return L"SN_E_TARGET_IN_USE";		// Connect operation failed because the specified target was in use.
 		case SN_E_LOAD_ELF_FAILED:		return L"SN_E_LOAD_ELF_FAILED";		// Failed to load ELF file.
 		case SN_E_BAD_UNIT:				return L"SN_E_BAD_UNIT";				// Specified unit is not valid.
 		case SN_E_OUT_OF_MEM:			return L"SN_E_OUT_OF_MEM";			// Internal memory allocation failure.
 		case SN_E_NOT_LISTED:			return L"SN_E_NOT_LISTED";			// Specified client is not in list for either unit update notification or TTY update notification. 
 		case SN_E_TM_VERSION:			return L"SN_E_TM_VERSION";			// Version of Target Manager is incorrect. Contact SN Systems Support for the latest version (www.snsys.com).
 		case SN_E_DLL_NOT_INITIALISED:	return L"SN_E_DLL_NOT_INITIALISED";	// The DLL has not been successfully initialized. Call SNPS3InitTargetComms().
 		case SN_E_TARGET_RUNNING:		return L"SN_E_TARGET_RUNNING";		// Operation could not be completed because specified unit is running. 
 		case SN_E_BAD_MEMSPACE:			return L"SN_E_BAD_MEMSPACE";			// Specified memory space was not valid.
 		case SN_E_NO_TARGETS:			return L"SN_E_NO_TARGETS";			// Pick Target operation failed because there are no targets in the Target Manager. 
 		case SN_E_NO_SEL:				return L"SN_E_NO_SEL";				// Pick Target operation failed because no target was selected or the user pressed Cancel.
 		case SN_E_BAD_PARAM:			return L"SN_E_BAD_PARAM";			// An invalid parameter was passed to the function. 
 		case SN_E_BUSY:					return L"SN_E_BUSY";					// A command is still pending completion. You must wait for this command to complete before issuing another.
 		case SN_E_DECI_ERROR:			return L"SN_E_DECI_ERROR";			// The Debug Agent returned an error. 
 		case SN_E_EXISTING_CALLBACK:	return L"SN_E_EXISTING_CALLBACK";
 		case SN_E_INSUFFICIENT_DATA:	return L"SN_E_INSUFFICIENT_DATA";
 		} 
 		return L"*** UNKNOWN ***";
 	}

	/**
	 * Open an internal connection to a target. This is used so that each operation is "atomic" in 
	 * that connection failures can quickly be 'remembered' for one "sync", etc operation, so
	 * that a connection is attempted for each file in the sync operation...
	 * For the next operation, the connection can try to be opened again.
	 *
	 * @param Handle The handle of the console to connect to.
	 *
	 * @return false for failure.
	 */
	bool ConnectToTarget(TARGETHANDLE Handle)
	{
#ifndef PS3MOD

		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return false;
		}

		// get connection status
		unsigned int Status;
		char* UsageString;
		if (SN_FAILED(SNPS3GetConnectStatus(Target->TargetID, (ECONNECTSTATUS*)&Status, &UsageString)))
		{
			return false;
		}

		switch (Status)
		{
			// can't use this if unavailable
			case CS_UNAVAILABLE:
				return false;

			// is it in use already?
			case CS_IN_USE:
				// if it's in-use, force disconnect the other user :)
				// @todo: Make this an .xml option or something
				SNPS3ForceDisconnect(Target->TargetID);

				// make sure it disconnected
				if (SN_FAILED(SNPS3GetConnectStatus(Target->TargetID, (ECONNECTSTATUS*)&Status, &UsageString)))
				{
					return false;
				}

				// if it didn't disconnect, then abort
				if (Status != CS_NOT_CONNECTED)
				{
					return false;
				}

				// otherwise FALL THROUGH!!!!!!! and connect

			// were we not previously connected
			case CS_NOT_CONNECTED:
				// remember that we weren't connected
				Target->bWasConnected = FALSE;

				// note that we need to register the TTY
				Target->bWasTTYRegistered = false;

				// connect to the target
				if (SN_FAILED(SNPS3Connect(Target->TargetID, "UnrealFronted")))
				{
					return false;
				}

				break;

			// were we already connected?
			case CS_CONNECTED:
				// remember that we were connected
				Target->bWasConnected = TRUE;

				// no need to do anything
				break;
		}


		// register the TTY callback if needed
		if (!Target->bWasTTYRegistered)
		{

			// get number of possible TTYs
			unsigned int NumTTYStreams; 
			SNPS3ListTTYStreams(Target->TargetID, &NumTTYStreams, NULL);

			// allocate space for all of them
			TTYSTREAM* Streams = new TTYSTREAM[NumTTYStreams];
			SNPS3ListTTYStreams(Target->TargetID, &NumTTYStreams, Streams);

			SNRESULT SNResult = SNPS3RegisterTTYEventHandler(Target->TargetID, SNPS3_TTY_ALL_STREAMS, TTYEventCallback, Target);
			if (SN_FAILED(SNResult))
			{
				TCHAR ErrorStr[2048];
				wsprintf(ErrorStr, L"Failed to register TTY event handler - %s\n", GetSNResultString(SNResult));
				MessageBox(NULL, ErrorStr, L"ERROR", MB_ICONERROR);
			}

			// free memory
			delete [] Streams;

			// mark as registered
			Target->bWasTTYRegistered = true;
		}

		// by default (for now) we're running
		if ( Target->ConsoleState != FConsoleSupport::CS_Asserted && Target->ConsoleState != FConsoleSupport::CS_Crashed )
		{
			Target->ConsoleState = CS_Running;
		}

		return true;

#else //#ifndef PS3MOD

		return false;
#endif
	}

	/**
	 * Open an internal connection to a target. This is used so that each operation is "atomic" in 
	 * that connection failures can quickly be 'remembered' for one "sync", etc operation, so
	 * that a connection is attempted for each file in the sync operation...
	 * For the next operation, the connection can try to be opened again.
	 *
	 * @param TargetName	Name of target.
	 * @param OutHandle		Receives a handle to the target.
	 *
	 * @return True if the target was successfully connected to.
	 */
	virtual bool ConnectToTarget(TARGETHANDLE& OutHandle, const wchar_t* TargetName)
	{
		bool bSucceeded = false;
		OutHandle = INVALID_TARGETHANDLE;

#if !PS3MOD

		if (TargetName == NULL)
		{
			HTARGET Target;
			if (SNPS3PickTarget(NULL, &Target) == SN_S_OK)
			{
				// look for a matching target by handle
				for (size_t TargetIndex = 0; TargetIndex < PS3Targets.size(); TargetIndex++)
				{
					if (PS3Targets[TargetIndex].TargetID == Target)
					{
						OutHandle = (TARGETHANDLE)TargetIndex;

						// connect and check for failure
						if (ConnectToTarget((TARGETHANDLE)TargetIndex))
						{
							bSucceeded = true;
						}
						break;
					}
				}
			}
		}
		else
		{
			// look for a matching target by name
			for (size_t TargetIndex = 0; TargetIndex < PS3Targets.size(); TargetIndex++)
			{
				FPS3Target& Target = PS3Targets[TargetIndex];

				if (_wcsicmp(TargetName, Target.Name.c_str()) == 0 || _wcsicmp(TargetName, Target.DisplayName.c_str()) == 0)
				{
					OutHandle = (TARGETHANDLE)TargetIndex;

					// connect and check for failure
					if (ConnectToTarget((TARGETHANDLE)TargetIndex))
					{
						bSucceeded = true;
					}
					break;
				}
			}
		}
#endif

		return bSucceeded;
	}

	/**
	 * Called after an atomic operation to close any open connections
	 *
	 * @param Handle The handle of the console to disconnect.
	 */
	void DisconnectFromTarget(TARGETHANDLE Handle)
	{
		FPS3Target *Target = GetTarget(Handle);
		SNPS3Disconnect(Target->TargetID);
	}

	/**
	 * Determines if the given file needs to be copied
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 * @param SourceFilename	Path of the source file on PC
	 * @param DestFilename		Platform-independent destination filename (ie, no xe:\\ for Xbox, etc)
	 * @param bReverse			If true, then copying from platform (dest) to PC (src);
	 *
	 * @return true if successful
	 */
	bool NeedsToCopyFile(TARGETHANDLE /*Handle*/, const wchar_t* /*SourceFilename*/, const wchar_t* /*DestFilename*/, bool /*bReverse*/)
	{
		// PS3 never copies files
		return false;
	}

	/**
	 * Get the name of the specified target
	 *
	 * @param	Handle The handle of the console to retrieve the information from.
	 * @return Name of the target, or NULL if the Index is out of bounds
	 */
	const wchar_t* GetTargetName(TARGETHANDLE Handle)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(Target)
		{
			return Target->DisplayName.c_str();
		}

		return NULL;
	}

	/**
	 * @return true if this platform needs to have files copied from PC->target (required to implement)
	 */
	bool PlatformNeedsToCopyFiles()
	{
		return false;
	}

	/**
	 * Reboots the target console. Must be implemented
	 *
	 * @param Handle The handle of the console to retrieve the information from.
	 * 
	 * @return true if successful
	 */
	bool Reboot(TARGETHANDLE Handle)
	{
#ifndef PS3MOD

		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return false;
		}

		char  Buffer[MAX_PATH];
		char* FilePart = 0;

		wchar_t RelativePathWide[1024];
		swprintf(RelativePathWide,
			ARRAY_COUNT(RelativePathWide), 
			L"%s..",
			RelativeBinariesDir.c_str() );
		string RelativePathString;
		wstring2string( RelativePathString, RelativePathWide );
		GetFullPathNameA( RelativePathString.c_str(), MAX_PATH, Buffer, &FilePart);

		// set file serving directory
		SNPS3TargetInfo TargetInfo;
		TargetInfo.hTarget  = Target->TargetID;
		TargetInfo.nFlags   = SN_TI_FILESERVEDIR | SN_TI_TARGETID;
		TargetInfo.pszFSDir = Buffer;

		SNPS3SetTargetInfo(&TargetInfo);

		// reboot the ps3 into system mode and return if it succeeded
		bool bSucceeded = SN_SUCCEEDED(SNPS3ResetEx(Target->TargetID, SNPS3TM_BOOTP_SYSTEM_MODE, SNPS3TM_BOOTP_BOOT_MODE_MASK, 0, 0, 0, 0));
//		bool bSucceeded = SN_SUCCEEDED(SNPS3Reset(Target->TargetID, SNPS3TM_RESETP_SOFT_RESET));
		if (Target->bIsDebugKit && !bSucceeded)
		{
			// if a debug kit "failed" to reboot, try connecting to it
			bSucceeded = SN_SUCCEEDED(SNPS3Connect(Target->TargetID, "UnrealFrontend"));
		}
		return bSucceeded;

#else //#ifndef PS3MOD
		return false;
#endif
	}




	/**
	 * @temp debugging function copied from msdn
	 */
	void ErrorBox(LPTSTR lpszFunction) 
	{ 
		LPVOID lpMsgBuf;
		LPVOID lpDisplayBuf;
		DWORD dw = GetLastError(); 

		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, NULL );

		lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
			(lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+40)*sizeof(TCHAR)); 
		wsprintf((LPTSTR)lpDisplayBuf, 
			TEXT("%s failed with error %d: %s"), 
			lpszFunction, dw, lpMsgBuf); 
		MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

		LocalFree(lpMsgBuf);
		LocalFree(lpDisplayBuf);
	}






	/**
	 * Reboots the target console. Must be implemented
	 *
	 * @param Handle			The handle of the console to retrieve the information from.
	 * @param Configuration		Build type to run (Debug, Release, RelaseLTCG, etc)
	 * @param BaseDirectory		Location of the build on the console (can be empty for platforms that don't copy to the console)
	 * @param GameName			Name of the game to run (Example, UT, etc)
	 * @param URL				Optional URL to pass to the executable
	 * @param bForceGameName	Forces the name of the executable to be only what's in GameName instead of being auto-generated
	 * 
	 * @return true if successful
	 */
	bool RebootAndRun(TARGETHANDLE Handle, const wchar_t* Configuration, const wchar_t* BaseDirectory, const wchar_t* GameName, const wchar_t* URL, bool bForceGameName)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return false;
		}

		wchar_t PS3CommandLineFile[1024];
		swprintf( PS3CommandLineFile, ARRAY_COUNT( PS3CommandLineFile ), L"%sPS3\\PS3CommandLine.txt", RelativeBinariesDir.c_str() );


		if (Target->bIsDebugKit)
		{
			wchar_t Filename[MAX_PATH];	

			if(bForceGameName)
			{
				swprintf(Filename, ARRAY_COUNT(Filename), L"%sPS3\\%s", RelativeBinariesDir.c_str(), GameName);
			}
			else
			{
				swprintf(Filename, ARRAY_COUNT(Filename), L"%sPS3\\%s-PS3-%s.elf", RelativeBinariesDir.c_str(), GameName, Configuration);
			}


			// open the command line file
			FILE* CommandLineFile = _wfopen(PS3CommandLineFile, L"w");
			// write out the URL to the commandline
			fwprintf(CommandLineFile, L"%s -BaseDir=%s", URL, BaseDirectory);
			fclose(CommandLineFile);

			wchar_t BootDir[1024];
			swprintf( BootDir, ARRAY_COUNT( BootDir ), L"%s..\\PS3_GAME", RelativeBinariesDir.c_str() );
			CreateDirectoryW(BootDir, NULL);
			swprintf( BootDir, ARRAY_COUNT( BootDir ), L"%s..\\PS3_GAME\\USRDIR", RelativeBinariesDir.c_str() );
			CreateDirectoryW(BootDir, NULL);

			wchar_t BootFile[1024];
			swprintf( BootFile, ARRAY_COUNT( BootFile ), L"%s..\\PS3_GAME\\USRDIR\\EBOOT.BIN", RelativeBinariesDir.c_str() );

			wchar_t BootPath[MAX_PATH];
			wchar_t* FilePart = 0;

			::GetFullPathNameW(BootFile, MAX_PATH, BootPath, &FilePart);

			// delete exisitng EBOOT.BIN
			if (::DeleteFileW(BootPath) == 0 && GetLastError() != 2)
			{
				ErrorBox(L"DeleteFile");
			}

			// copy the executable file to the file that the debug kit will launch
			if (::CopyFile(Filename, BootPath, FALSE) != 0)
			{
				// make the file not read-only in case the source was checked in
				SetFileAttributes(BootPath, 0);
			}
			else
			{
				ErrorBox(L"CopyFile[EBOOT.BIN]");
				return false;
			}

			wchar_t TempFile[1024];
			wchar_t ParamSource[1024];
			wchar_t ParamDest[1024];
			swprintf( TempFile, ARRAY_COUNT( TempFile ), L"%s..\\%s\\Build\\PS3\\PARAM.SFO", RelativeBinariesDir.c_str(), GameName );
			::GetFullPathNameW(TempFile, MAX_PATH, ParamSource, &FilePart);
			swprintf( TempFile, ARRAY_COUNT( TempFile ), L"%s..\\PS3_GAME\\PARAM.SFO", RelativeBinariesDir.c_str() );
			::GetFullPathNameW(TempFile, MAX_PATH, ParamDest, &FilePart);
			// copy the param SFO file to the file that the debug kit will launch
			if (::CopyFileW(ParamSource, ParamDest, FALSE) != 0)
			{
				// make the file not read-only in case the source was checked in
				SetFileAttributesW(ParamDest, 0);
			}
			else
			{
				ErrorBox(L"CopyFile[PARAM.SFO]");
				return false;
			}

			wchar_t IconSource[1024];
			wchar_t IconDest[1024];
			swprintf( TempFile, ARRAY_COUNT( TempFile ), L"%s..\\%s\\Build\\PS3\\ICON0.PNG", RelativeBinariesDir.c_str(), GameName );
			::GetFullPathNameW(TempFile, MAX_PATH, IconSource, &FilePart);
			swprintf( TempFile, ARRAY_COUNT( TempFile ), L"%s..\\PS3_GAME\\ICON0.PNG", RelativeBinariesDir.c_str() );
			::GetFullPathNameW(TempFile, MAX_PATH, IconDest, &FilePart);
			// copy the icon file to the file that the debug kit will launch (if it exists, it's okay to fail this one)
			if (::CopyFileW(IconSource, IconDest, FALSE) != 0)
			{
				// make the file not read-only in case the source was checked in
				SetFileAttributesW(IconDest, 0);
			}

			swprintf( TempFile, ARRAY_COUNT( TempFile ), L"%s..\\%s\\Build\\PS3\\PIC1.PNG", RelativeBinariesDir.c_str(), GameName );
			::GetFullPathNameW(TempFile, MAX_PATH, IconSource, &FilePart);
			swprintf( TempFile, ARRAY_COUNT( TempFile ), L"%s..\\PS3_GAME\\PIC1.PNG", RelativeBinariesDir.c_str() );
			::GetFullPathNameW(TempFile, MAX_PATH, IconDest, &FilePart);
			// copy the icon file to the file that the debug kit will launch (if it exists, it's okay to fail this one)
			if (::CopyFileW(IconSource, IconDest, FALSE) != 0)
			{
				// make the file not read-only in case the source was checked in
				SetFileAttributesW(IconDest, 0);
			}

			// reboot the debug kit, this will allow the launcher to run, which will auto-launch the EBOOT.BIN
			return Reboot(Handle);
		}
		else
		{
			// delete any test kit commandline files
			::DeleteFileW(PS3CommandLineFile);

			wchar_t Command[1024];

			// reboot the ps3 with ProDG
			if(bForceGameName)
			{
				swprintf(Command,
					ARRAY_COUNT(Command), 
					L"ps3run -r -h%s.. -f%s.. -t%s %sPS3\\%s %s -BaseDir=%s",
					RelativeBinariesDir.c_str(),
					RelativeBinariesDir.c_str(),
					Target->Name.c_str(),
					RelativeBinariesDir.c_str(),
					GameName,
					URL,
					BaseDirectory);
			}
			else
			{
				swprintf(Command,
					ARRAY_COUNT(Command), 
					L"ps3run -r -h%s.. -f%s.. -t%s %sPS3\\%s-PS3-%s.elf %s -BaseDir=%s",
					RelativeBinariesDir.c_str(),
					RelativeBinariesDir.c_str(),
					Target->Name.c_str(),
					RelativeBinariesDir.c_str(),
					GameName,
					Configuration,
					URL,
					BaseDirectory);
			}

			TCHAR Errors[1024];
			DWORD ReturnValue;
			// spawn the child process with no DOS window popping up
			RunChildProcess(Command, Errors, ARRAY_COUNT(Errors) - 1, &ReturnValue);

			return ReturnValue == 0;
		}
	}



	/**
	 * Returns the global sound cooker object.
	 *
	 * @return global sound cooker object, or NULL if none exists
	 */
	virtual FConsoleSoundCooker* GetGlobalSoundCooker() 
	{ 
		static FPS3SoundCooker* GlobalSoundCooker = NULL;
		if( !GlobalSoundCooker )
		{
			GlobalSoundCooker = new FPS3SoundCooker();
		}
		return GlobalSoundCooker;
	}

	/**
	 * Returns the global texture cooker object.
	 *
	 * @return global sound cooker object, or NULL if none exists
	 */
	virtual FConsoleTextureCooker* GetGlobalTextureCooker()
	{
		static FPS3TextureCooker* GlobalTextureCooker = NULL;
		if( !GlobalTextureCooker )
		{
			GlobalTextureCooker = new FPS3TextureCooker();
		}
		return GlobalTextureCooker;
	}

	/**
	 * Returns the global skeletal mesh cooker object.
	 *
	 * @return global skeletal mesh cooker object, or NULL if none exists
	 */
	virtual FConsoleSkeletalMeshCooker* GetGlobalSkeletalMeshCooker() 
	{ 
		static FPS3SkeletalMeshCooker* GlobalSkeletalMeshCooker = NULL;
		if( !GlobalSkeletalMeshCooker )
		{
			GlobalSkeletalMeshCooker = new FPS3SkeletalMeshCooker();
		}
		return GlobalSkeletalMeshCooker;
	}

	/**
	 * Returns the global static mesh cooker object.
	 *
	 * @return global static mesh cooker object, or NULL if none exists
	 */
	virtual FConsoleStaticMeshCooker* GetGlobalStaticMeshCooker() 
	{ 
		static FPS3StaticMeshCooker* GlobalStaticMeshCooker = NULL;
		if( !GlobalStaticMeshCooker )
		{
			GlobalStaticMeshCooker = new FPS3StaticMeshCooker();
		}
		return GlobalStaticMeshCooker;
	}

	virtual FConsoleShaderPrecompiler* GetGlobalShaderPrecompiler()
	{
		static FPS3ShaderPrecompiler* GlobalShaderPrecompiler = NULL;
		if(!GlobalShaderPrecompiler)
		{
			GlobalShaderPrecompiler = new FPS3ShaderPrecompiler();
		}
		return GlobalShaderPrecompiler;
	}

	virtual void FreeReturnedMemory(void* Pointer)
	{
		free(Pointer);
	}

	/**
	* Retrieves the handle of the default console.
	*/
	virtual TARGETHANDLE GetDefaultTarget()
	{
		if(PS3Targets.size() > 0)
		{
			return (TARGETHANDLE)0;
		}

		return INVALID_TARGETHANDLE;
	}

	/**
	* This function exists to delete an instance of FConsoleSupport that has been allocated from a *Tools.dll. Do not call this function from the destructor.
	*/
	virtual void Destroy()
	{
		// do nothing for PS3 verison
	}

	/**
	* Returns true if the specified target is connected for debugging and sending commands.
	* 
	*  @param Handle			The handle of the console to retrieve the information from.
	*/
	virtual bool GetIsTargetConnected(TARGETHANDLE Handle)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(!Target)
		{
			return false;
		}

		// get connection status
		unsigned int Status;
		char* UsageString;
		if (SN_FAILED(SNPS3GetConnectStatus(Target->TargetID, (ECONNECTSTATUS*)&Status, &UsageString)))
		{
			return false;
		}

		return Status == CS_CONNECTED;
	}

	/**
	 * Retrieves a handle to each available target.
	 *
	 * @param	OutTargetList			An array to copy all of the target handles into.
	 * @param	InOutTargetListSize		This variable needs to contain the size of OutTargetList. When the function returns it will contain the number of target handles copied into OutTargetList.
	 */
	virtual void GetTargets(TARGETHANDLE *OutTargetList, int *InOutTargetListSize)
	{
		int i;

		for(i = 0; i < (int)PS3Targets.size() && i < *InOutTargetListSize; ++i)
		{
			OutTargetList[i] = (TARGETHANDLE)i;
		}

		*InOutTargetListSize = i;
	}

	/**
	 * Sets the callback function for TTY output.
	 *
	 * @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
	 * @param	Handle		The handle to the target that will register the callback.
	 */
	virtual void SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(Target)
		{
			Target->TxtCallback = Callback;
		}
	}

	/**
	 * Sets the callback function for handling crashes.
	 *
	 * @param	Callback	Pointer to a callback function or NULL if handling crashes is to be disabled.
	 * @param	Handle		The handle to the target that will register the callback.
	 */
	virtual void SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(Target)
		{
			Target->CrashCallback = Callback;
		}
	}

	/**
	 * Returns the type of the specified target.
	 */
	virtual FConsoleSupport::ETargetType GetTargetType(TARGETHANDLE Handle)
	{
		FConsoleSupport::ETargetType RetVal = FConsoleSupport::TART_Unknown;
		FPS3Target *Target = GetTarget(Handle);

		if(Target)
		{
			RetVal = Target->bIsDebugKit ? FConsoleSupport::TART_TestKit : FConsoleSupport::TART_DevKit;
		}

		return RetVal;
	}

	/**
	 * Returns the platform of the specified target.
	 */
	virtual FConsoleSupport::EPlatformType GetPlatformType(TARGETHANDLE /*Handle*/)
	{
		return EPlatformType_PS3;
	}

	/**
	* Sets flags controlling how crash reports will be filtered.
	*
	* @param	Handle	The handle to the target to set the filter for.
	* @param	Filter	Flags controlling how crash reports will be filtered.
	*/
	virtual bool SetCrashReportFilter(TARGETHANDLE Handle, ECrashReportFilter Filter)
	{
		FPS3Target *Target = GetTarget(Handle);

		if(Target)
		{
			Target->CrashFilter = Filter;
			return true;
		}

		return false;
	}

	/**
	 * Gets the name of a console as displayed by the target manager.
	 *
	 * @param	Handle	The handle to the target to set the filter for.
	 */
	virtual const wchar_t* GetTargetManagerNameForConsole(TARGETHANDLE Handle)
	{
		return GetTargetName(Handle);
	}

	/**
	 * Enumerates all available targets for the platform.
	 *
	 * @returns The number of available targets.
	 */
	virtual int EnumerateAvailableTargets()
	{
		// Enumerate available targets.
		if(SN_FAILED( SNPS3EnumTargets(EnumCallBack) ))
		{
			OutputDebugStringW(L"(PS3Tools.dll) EnumerateAvailableTargets(): SNPS3EnmTargets() failed!\n");
		}

		return (int)PS3Targets.size();
	}
};


/** 
 * Returns a pointer to a subclass of FConsoleSupport.
 *
 * @return The pointer to the console specific FConsoleSupport
 */
CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport()
{
	static FPS3Support PS3Support;
	return &PS3Support;
}

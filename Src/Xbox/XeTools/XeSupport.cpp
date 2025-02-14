/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

// needed for CoInitializeEx()
#define _WIN32_DCOM
#define _WIN32_WINNT 0x0500 // minimum windows version of windows server 2000

#include <io.h>
#include <direct.h>
#include "XeSupport.h"
#include "XenonConsole.h"

using namespace XeTools;

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

// colors for TTY output during sync

#define COLOR_BLACK		0x000000ff
#define COLOR_YELLOW	0xf0f000ff
#define COLOR_RED		0xff0000ff

#define SENDFILE_RETRY	10000

struct FSyncThreadInfo
{
	ColoredTTYEventCallbackPtr OutputCallback;
	wstring TargetToSync;
	gcroot<array<System::String^>^> DirectoriesToCreate;
	gcroot<array<System::String^>^> SrcFilesToSync;
	gcroot<array<System::String^>^> DestFilesToSync;
};

/**
 * Conversion routinges from string to wstring and back
 */
inline void wstring2string(string& Dest, const wstring& Src)
{
	Dest.assign(Src.begin(), Src.end());
}

inline void string2wstring(wstring& Dest, const string& Src)
{
	Dest.assign(Src.begin(), Src.end());
}

struct FRunGameThreadData
{
	wstring URL;
	wstring MapName;
	wstring Configuration;
	wstring GameName;
	vector<wstring> XenonNames;
	vector<wstring> XenonTargetManagerNames;
};

///////////////////////////////////////////////////////////////////////////////////////////// FXenonSupport /////////////////////////////////////////////////////////////////////////////////////////////

FXenonSupport::FXenonSupport()
{
	XbdmLibrary = NULL;
	bIsProperlySetup = Setup();

	MenuItems.push_back(FMenuItem(INVALID_TARGETHANDLE, false)); // reboot xenon
	MenuItems.push_back(FMenuItem(INVALID_TARGETHANDLE, false)); // menu separator
}

FXenonSupport::~FXenonSupport()
{
}

bool FXenonSupport::Setup()
{
	// XMAInMemoryEncode doesn't actually encode in memory; it leaves 1000s of temp file droppings that eventually break the system
	// The code here is to workaround that by deleting them on startup
	{
		DWORD Result;
		intptr_t FindResult;
		wchar_t TempPath[MAX_PATH] = { 0 };
		wchar_t SearchPath[MAX_PATH] = { 0 };
		wchar_t FilePath[MAX_PATH] = { 0 };

		// Get temporary path
		Result = GetTempPath( MAX_PATH, TempPath );
		if( Result > 0 && Result < MAX_PATH )
		{
			Result = GetLongPathName( TempPath, TempPath, MAX_PATH );
			if( Result > 0 && Result < MAX_PATH )
			{
				swprintf_s( SearchPath, TEXT("%sEncStrm*"), TempPath );

				_wfinddata32_t FindData;

				FindResult = _wfindfirst32( SearchPath, &FindData );
				if( FindResult != -1 )
				{
					do 
					{
						swprintf_s( FilePath, TEXT("%s%s"), TempPath, FindData.name );
						_wremove( FilePath );
					} 
					while( !_wfindnext32( FindResult, &FindData ) );
				}
				_findclose( FindResult );
			}
		}
	}
	return true;
}

void FXenonSupport::Cleanup()
{
	for(TargetHandleMap::iterator Iter = CachedConnections.begin(); Iter != CachedConnections.end(); ++Iter)
	{
		try
		{
			if(Iter->second->IsConnected)
			{
				Iter->second->DisconnectAsDebugger();
			}
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}

	ConnectionNames.clear();
	CachedConnections.clear();
	
	MenuItems.clear();
	MenuItems.push_back(FMenuItem(INVALID_TARGETHANDLE, false)); // reboot xenon
	MenuItems.push_back(FMenuItem(INVALID_TARGETHANDLE, false)); // menu separator

	bIsProperlySetup = false;
}

/**
* Retrieves the target with the specified handle if it exists.
*
* @param	Handle	The handle of the target to retrieve.
*/
TARGET FXenonSupport::GetTarget(TARGETHANDLE Handle)
{
	TargetHandleMap::iterator Iter = CachedConnections.find(Handle);

	if(Iter != CachedConnections.end())
	{
		return Iter->second;
	}

	return nullptr;
}

/**
 * Retrieves the target with the specified name if it exists.
 *
 * @param	Name		The name of the target to retrieve.
 * @param	OutHandle	The handle of the target that retrieved.
 */
TARGET FXenonSupport::GetTarget(const wchar_t *Name, TARGETHANDLE &OutHandle)
{
	OutHandle = INVALID_TARGETHANDLE;
	System::String ^ManagedName = gcnew System::String(Name);

	for(TargetHandleMap::iterator Iter = CachedConnections.begin(); Iter != CachedConnections.end(); ++Iter)
	{
		XenonConsole ^CurConsole = Iter->second;

		if(CurConsole->TargetManagerName->Equals(ManagedName, System::StringComparison::OrdinalIgnoreCase) || CurConsole->DebugTarget->default->Equals(ManagedName, System::StringComparison::OrdinalIgnoreCase))
		{
			OutHandle = Iter->first;
			return Iter->second;
		}
	}

	return nullptr;
}

/** Initialize the DLL with some information about the game/editor
 *
 * @param	GameName		The name of the current game ("ExampleGame", "UTGame", etc)
 * @param	Configuration	The name of the configuration to run ("Debug", "Release", etc)
 */
void FXenonSupport::Initialize(const wchar_t* InGameName, const wchar_t* InConfiguration)
{
	// cache the parameters
	GameName = InGameName;
	Configuration = InConfiguration;
}

/**
 * Return a string name descriptor for this platform (required to implement)
 *
 * @return	The name of the platform
 */
const wchar_t* FXenonSupport::GetConsoleName()
{
	// this is our hardcoded name
	return CONSOLESUPPORT_NAME_360;
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
unsigned int FXenonSupport::GetIPAddress(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);
	DWORD TitleIP = 0;
	
	if(Target)
	{
		try
		{
			// ask the console for its game/title ip address
			TitleIP = System::Net::IPAddress::NetworkToHostOrder((int)Target->Console->IPAddressTitle);
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}
	
	return TitleIP;
}

/**
 * Return whether or not this console Intel byte order (required to implement)
 *
 * @return	True if the console is Intel byte order
 */
bool FXenonSupport::GetIntelByteOrder()
{
	return false;
}

/**
 * @return the number of known xbox targets
 */
int FXenonSupport::GetNumTargets()
{
	return (int)CachedConnections.size();
}

/**
 * Retrieves a handle to each available target.
 *
 * @param	OutTargetList			An array to copy all of the target handles into.
 * @param	InOutTargetListSize		This variable needs to contain the size of OutTargetList. When the function returns it will contain the number of target handles copied into OutTargetList.
 */
void FXenonSupport::GetTargets(TARGETHANDLE *OutTargetList, int *InOutTargetListSize)
{
	assert(OutTargetList);
	assert(InOutTargetListSize);

	int Index = 0;
	for(TargetHandleMap::iterator Iter = CachedConnections.begin(); Iter != CachedConnections.end() && Index < *InOutTargetListSize; ++Iter, ++Index)
	{
		OutTargetList[Index] = Iter->first;
	}

	*InOutTargetListSize = Index;
}

/**
 * Get the name of the specified target
 *
 * @param	Handle The handle of the console to retrieve the information from.
 * @return Name of the target, or NULL if the Index is out of bounds
 */
const wchar_t* FXenonSupport::GetTargetName(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		try
		{
			pin_ptr<const wchar_t> NativeName = PtrToStringChars(Target->DebugTarget->default);
			XenonNameCache = NativeName;
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);

			pin_ptr<const wchar_t> NativeName = PtrToStringChars(Target->TargetManagerName);
			XenonNameCache = NativeName;
		}
	}

	return XenonNameCache.c_str();
}

/**
 * @return true if this platform needs to have files copied from PC->target (required to implement)
 */
bool FXenonSupport::PlatformNeedsToCopyFiles()
{
	return true;
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
bool FXenonSupport::ConnectToTarget(TARGETHANDLE& OutHandle, const wchar_t* TargetName)
{
	TARGET Target = GetTarget(TargetName, OutHandle);
	bool bSucceeded = false;

	if(Target)
	{
		try
		{
			Target->ConnectAsDebugger("XeTools.dll");
			bSucceeded = true;
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
			OutHandle = INVALID_TARGETHANDLE;
		}
	}

	return bSucceeded;
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
bool FXenonSupport::ConnectToTarget(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);
	bool bSucceeded = false;

	if(Target)
	{
		try
		{
			Target->ConnectAsDebugger("XeTools.dll");
			bSucceeded = true;
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}

	return bSucceeded;
}

/**
* Called after an atomic operation to close any open connections
*
* @param Handle The handle of the console to disconnect.
*/
void FXenonSupport::DisconnectFromTarget(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		try
		{
			Target->DisconnectAsDebugger();
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}
}

/**
 * Creates a directory
 *
 * @param Handle The handle of the console to retrieve the information from.
 * @param SourceFilename Platform-independent directory name (ie UnrealEngine3\Binaries)
 *
 * @return true if successful
 */
bool FXenonSupport::MakeDirectory(TARGETHANDLE Handle, const wchar_t* DirectoryName)
{
	TARGET Target = GetTarget(Handle);
	bool bRetVal = false;

	if(Target)
	{
		try
		{
			System::String ^FinalPath = System::IO::Path::Combine("xe:\\", gcnew System::String(DirectoryName));

			Target->Console->MakeDirectory(FinalPath);
			bRetVal = true;
		}
		catch(System::Runtime::InteropServices::COMException ^ex)
		{
			if(ex->ErrorCode == XBDM_ALREADYEXISTS)
			{
				bRetVal = true;
			}
			else
			{
				XenonConsole::OutputDebugException(__FUNCTION__, ex);
			}
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}

	return bRetVal;
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
bool FXenonSupport::NeedsToCopyFile(TARGETHANDLE Handle, const wchar_t* SourceFilename, const wchar_t* DestFilename, bool bReverse)
{
	TARGET Target = GetTarget(Handle);

	if(!Target)
	{
		return false;
	}

	try
	{
		return XenonConsole::NeedsToCopyFile(Target->Console, gcnew System::String(SourceFilename), gcnew System::String(DestFilename), bReverse);
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}

	return true;
}

/**
 * Copies a single file from PC to target
 *
 * @param Handle The handle of the console to retrieve the information from.
 * @param SourceFilename Path of the source file on PC
 * @param DestFilename Platform-independent destination filename (ie, no xe:\\ for Xbox, etc)
 *
 * @return true if successful
 */
bool FXenonSupport::CopyFile(TARGETHANDLE Handle, const wchar_t* SourceFilename, const wchar_t* DestFilename)
{
	TARGET Target = GetTarget(Handle);
	
	// make sure it succeeded
	if (!Target)
	{
		return false;
	}

	bool bSucceeded = false;

	try
	{
		Target->Console->SendFile(gcnew System::String(SourceFilename), System::IO::Path::Combine("xe:\\", gcnew System::String(DestFilename)));
		bSucceeded = true;
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}
	
	return bSucceeded;
}

/**
 *	Copies a single file from the target to the PC
 *
 *  @param	Handle			The handle of the console to retrieve the information from.
 *	@param	SourceFilename	Platform-independent source filename (ie, no xe:\\ for Xbox, etc)
 *	@param	DestFilename	Path of the destination file on PC
 *	
 *	@return	bool			true if successful, false otherwise
 */
bool FXenonSupport::RetrieveFile(TARGETHANDLE Handle, const wchar_t* SourceFilename, const wchar_t* DestFilename)
{
	TARGET Target = GetTarget(Handle);

	// make sure it succeeded
	if (!Target)
	{
		return false;
	}

	bool bSucceeded = false;

	try
	{
		Target->Console->ReceiveFile(gcnew System::String(DestFilename), System::IO::Path::Combine("xe:\\", gcnew System::String(SourceFilename)));
		bSucceeded = true;
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}

	return bSucceeded;
}

/**
 * Sets the name of the layout file for the DVD, so that GetDVDFileStartSector can be used
 * 
 * @param DVDLayoutFile Name of the layout file
 *
 * @return true if successful
 */
bool FXenonSupport::SetDVDLayoutFile(const wchar_t* DVDLayoutFile)
{
	string LayoutFileString;
	wstring2string(LayoutFileString, DVDLayoutFile);
	// load the file
	return DVDLayout.LoadFile(LayoutFileString.c_str());
}

/**
 * Processes a DVD layout file "object" to see if it's the matching file.
 * 
 * @param Object XmlElement in question
 * @param Filename Filename (no path) we are trying to match
 * @param LBA Output LBA information
 * 
 * @return true if successful
 */
bool FXenonSupport::HandleDVDObject(TiXmlElement* Object, const char* Filename, __int64& LBA)
{
	const char* Name = Object->Attribute("Name");
	if (Name == NULL)
	{
		return false;
	}

	// does the DVD layout filename match what we are looking for?
	if (_stricmp(Filename, Name) == 0)
	{
		// get the LBA attrib
		const char* LBAStr = Object->Attribute("LBA");
		if (LBAStr == NULL)
		{
			return false;
		}

		// cnvert LBA to an int
		LBA = _atoi64(LBAStr);

		return true;
	}

	// if we didn;t get the file, return
	return false;
}

/**
 * Gets the starting sector of a file on the DVD (or whatever optical medium)
 *
 * @param DVDFilename Path to the file on the DVD
 * @param SectorHigh High 32 bits of the sector location
 * @param SectorLow Low 32 bits of the sector location
 * 
 * @return true if the start sector was found for the file
 */
bool FXenonSupport::GetDVDFileStartSector(const wchar_t* DVDFilename, unsigned int& SectorHigh, unsigned int& SectorLow)
{
	// strip off the directory info (we assume that all files are uniquely named, so we ignore the path)
	const wchar_t* FilePart = wcsrchr(DVDFilename, '\\');
	if (FilePart == NULL)
	{
		FilePart = DVDFilename;
	}
	else
	{
		FilePart = FilePart + 1;
	}

	// convert to ascii string
	string DVDFile;
	wstring2string(DVDFile, FilePart);

	// parse the layout for the file of interest
	TiXmlElement* XboxGameDiscLayout = NULL;
	TiXmlElement* Disc = NULL;
	TiXmlElement* Files = NULL;

	// get the layout element
	XboxGameDiscLayout = DVDLayout.FirstChildElement("XboxGameDiscLayout");

	// this element is required
	if (XboxGameDiscLayout == NULL) 
	{
		return false;
	}

	// get the first disc
	Disc = XboxGameDiscLayout->FirstChildElement("Disc");
	// we need at least one
	if (Disc == NULL)
	{
		return false;
	}

	__int64 LBA = -1;
	// loop over all the discs 
	for(; Disc != NULL; Disc = Disc->NextSiblingElement("Disc"))
	{
		// get the first Files element
		Files = Disc->FirstChildElement("Files");

		// we need at least one
		if (Files == NULL)
		{
			return false;
		}

		// loop over all the files groups (one group per layer)
		for (; Files != NULL; Files = Files->NextSiblingElement("Files"))
		{
			// get the first group
			TiXmlElement* Group = Files->FirstChildElement("Group");

			// loop over all groups until we find it
			for(; Group != NULL && LBA == -1; Group = Group->NextSiblingElement("Group")) 
			{
				// get all the files in this group
				TiXmlElement* Object = Group->FirstChildElement("Object");
				for(; Object != NULL; Object = Object->NextSiblingElement("Object")) 
				{
					// see if the object matches, and stop searching if it does
					if (HandleDVDObject(Object, DVDFile.c_str(), LBA))
					{
						break;
					}
				}
			}

			// loop over all the files not in the group until we find it
			TiXmlElement* Object = Files->FirstChildElement("Object");
			for(; Object != NULL && LBA == -1; Object = Object->NextSiblingElement("Object")) 
			{
				// see if the object matches, and stop searching if it does
				if (HandleDVDObject(Object, DVDFile.c_str(), LBA))
				{
					break;
				}
			}
		}
	}

	// if we found the file, return the values
	if (LBA != -1)
	{
		SectorHigh = (unsigned int)(LBA >> 32);
		SectorLow = (unsigned int)(LBA & 0xFFFFFFFF);
		return true;
	}

	return false;
}

/**
 * Reboots the target console. Must be implemented
 *
 * @param Handle The handle of the console to retrieve the information from.
 * 
 * @return true if successful
 */
bool FXenonSupport::Reboot(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);
	// make sure it succeeded
	if (!Target)
	{
		return false;
	}

	bool bSucceeded = false;

	try
	{
		Target->Console->Reboot(nullptr, nullptr, nullptr, XDevkit::XboxRebootFlags::Title);
		bSucceeded = true;
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}

	return bSucceeded;
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
bool FXenonSupport::RebootAndRun( TARGETHANDLE Handle, const wchar_t* Configuration, const wchar_t* BaseDirectory, const wchar_t* GameName, const wchar_t* URL, bool bForceGameName )
{
	TARGET Target = GetTarget( Handle );

	// make sure it succeeded
	if( !Target )
	{
		return false;
	}

	bool bSucceeded = false;

	try
	{
		System::String^ ManagedBaseDir = gcnew System::String( BaseDirectory );
		System::String^ PathName = System::IO::Path::Combine( "xe:\\", ManagedBaseDir );
		System::String^ ExeName = System::IO::Path::Combine( PathName, gcnew System::String( GameName ) );

		if( !bForceGameName )
		{
			ExeName += "-Xbox360-" + gcnew System::String( Configuration ) + ".xex";			
		}

		Target->Console->Reboot( ExeName, PathName, gcnew System::String( URL ), XDevkit::XboxRebootFlags::Title );
		bSucceeded = true;
	}
	catch( System::Exception^ ex )
	{
		XenonConsole::OutputDebugException( __FUNCTION__, ex );
	}

	return bSucceeded;
}

/**
 * This function is run on a separate thread for cooking, copying, and running an autosaved level on an xbox 360.
 *
 * @param	Data	A pointer to data passed into the thread providing it with extra data needed to do its job.
 */
void __cdecl FXenonSupport::RunGameThread(void *Data)
{
	FRunGameThreadData *ThreadData = (FRunGameThreadData*)Data;

	if(ThreadData && ThreadData->XenonNames.size() > 0 && ThreadData->XenonNames.size() == ThreadData->XenonTargetManagerNames.size())
	{
		wchar_t *XDKDir = _wgetenv(L"XEDK");

		if(XDKDir)
		{
			wstring XBReboot(XDKDir);
			XBReboot += L"\\bin\\win32\\xbreboot.exe";

			/*wstring XBRebootParamXenon(L"/x:");
			XBRebootParamXenon += ThreadData->XenonTargetManagerName;*/

			// NOTE: Shouldn't need this because CookerSync now forces a reboot
			/*ShellExecuteW(NULL, L"open", XBReboot.c_str(), XBRebootParamXenon.c_str(), NULL, SW_HIDE);*/

			wstring AppName;


			AppName += ThreadData->GameName;

			// In release mode, the platform type is not appended to the exe name
			if( ThreadData->Configuration != L"Release")
			{
#if _WIN64
				AppName += L"-Win64";
#else
				AppName += L"-Win32";
#endif
			}

			if(ThreadData->Configuration == L"Debug")
			{
				AppName += L"-Debug";
			}
			else if( ThreadData->Configuration == L"Shipping")
			{
				AppName += L"-Shipping";
			}
		
			AppName += L".exe";

			wstring CmdLine(L"cookpackages ");
			CmdLine += ThreadData->MapName;
			CmdLine += L" -nopause -platform=xenon ";

			PROCESS_INFORMATION ProcInfo;
			STARTUPINFOW StartInfo;

			ZeroMemory(&StartInfo, sizeof(StartInfo));
			StartInfo.cb = sizeof(StartInfo);

			CmdLine = AppName + L" " + CmdLine;

			// Unforunately CreateProcessW() can modify the cmd line string so we have to create a non-const buffer when passing it in
			wchar_t *CmdLineStr = new wchar_t[CmdLine.size() + 1];
			wcscpy_s(CmdLineStr, CmdLine.size() + 1, CmdLine.c_str());

			if(!CreateProcessW(NULL, CmdLineStr, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &StartInfo, &ProcInfo))
			{
				OutputDebugStringW(L"Could not create cooker process!\n");
			}
			else
			{
				WaitForSingleObject(ProcInfo.hProcess, INFINITE);
				CloseHandle(ProcInfo.hThread);
				CloseHandle(ProcInfo.hProcess);

				CmdLine = L"..\\CookerSync.exe ";
				CmdLine += ThreadData->GameName;

				for(size_t i = 0; i < ThreadData->XenonNames.size(); ++i)
				{
					CmdLine += L" ";
					CmdLine += ThreadData->XenonNames[i];
				}

				// -d, -deftarget     : If no targets are specified try and use the default target.
				// -ni, -noninter     : Non interactive mode; don't prompt the user for input.
				// -x, -tagset        : Only sync files in tag set <TagSet>. See CookerSync.xml for tagsets
				// -p, -platform      : Specify platform <Platform> to be used
				CmdLine += L" -deftarget -noninter -platform xbox360 -x ConsoleSync";

				pin_ptr<const wchar_t> MachineName = PtrToStringChars(System::String::Concat(L"UE3-", System::Environment::MachineName));

				CmdLine += L" -b \"";
				CmdLine += MachineName;
				CmdLine += L"\"";

				// Unforunately CreateProcessW() can modify the cmd line string so we have to create a non-const buffer when passing it in
				delete [] CmdLineStr;
				CmdLineStr = new wchar_t[CmdLine.size() + 1];
				wcscpy_s(CmdLineStr, CmdLine.size() + 1, CmdLine.c_str());

				if(!CreateProcessW(NULL, CmdLineStr, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &StartInfo, &ProcInfo))
				{
					OutputDebugStringW(L"Could not create cooker sync process!\n");
				}
				else
				{
					WaitForSingleObject(ProcInfo.hProcess, INFINITE);
					CloseHandle(ProcInfo.hThread);
					CloseHandle(ProcInfo.hProcess);

					wstring XBRebootParms = L" xe:\\";
					XBRebootParms += MachineName;
					XBRebootParms += L'\\';
					XBRebootParms += ThreadData->GameName;
					XBRebootParms += L"-Xbox360-Release.xex ";
					XBRebootParms += ThreadData->URL;

					for(size_t i = 0; i < ThreadData->XenonTargetManagerNames.size(); ++i)
					{
						CmdLine = L"/x:";
						CmdLine += ThreadData->XenonTargetManagerNames[i];
						CmdLine += XBRebootParms;

						ShellExecuteW(NULL, L"open", XBReboot.c_str(), CmdLine.c_str(), NULL, SW_HIDE);
					}
				}
			}

			delete [] CmdLineStr;
			CmdLineStr = NULL;
		}

		delete ThreadData;
		ThreadData = NULL;
	}
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
bool FXenonSupport::RunGame(TARGETHANDLE *TargetList, int NumTargets, const wchar_t* MapName, const wchar_t* URL, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
{
	FRunGameThreadData *Data = new FRunGameThreadData();

	Data->MapName = MapName;
	Data->GameName = GameName;
	Data->Configuration = Configuration;
	Data->URL = URL;

	for(int i = 0; i < NumTargets; ++i)
	{
		TARGET Target = GetTarget(TargetList[i]);

		if(!Target)
		{
			continue;
		}

		try
		{
			pin_ptr<const wchar_t> TMName = PtrToStringChars(Target->TargetManagerName);
			pin_ptr<const wchar_t> Name = PtrToStringChars(Target->DebugTarget->default);

			Data->XenonNames.push_back(Name);
			Data->XenonTargetManagerNames.push_back(TMName);
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}

	// Do all cooking, copying, and running on a separate thread so the UI doesn't hang.
	_beginthread(&FXenonSupport::RunGameThread, 0, Data);

	return true;
}

/**
 * Gets a list of targets that have been selected via menu's in UnrealEd.
 *
 * @param	OutTargetList			The list to be filled with target handles.
 * @param	InOutTargetListSize		Contains the size of OutTargetList. When the function returns it contains the # of target handles copied over.
 */
void FXenonSupport::GetMenuSelectedTargets(TARGETHANDLE *OutTargetList, int &InOutTargetListSize)
{
	int CopiedHandles = 0;

	for(size_t i = 0; i < MenuItems.size() && CopiedHandles < InOutTargetListSize; ++i)
	{
		if(MenuItems[i].bChecked)
		{
			OutTargetList[CopiedHandles] = MenuItems[i].Target;
			++CopiedHandles;
		}
	}

	InOutTargetListSize = CopiedHandles;
}

/**
 * Send a text command to the target
 * 
 * @param Handle The handle of the console to retrieve the information from.
 * @param Command Command to send to the target
 */
void FXenonSupport::SendConsoleCommand(TARGETHANDLE Handle, const wchar_t* Command)
{
	TARGET Target = GetTarget(Handle);
	// make sure it succeeded
	if(!Target)
	{
		return;
	}

	try
	{
		Target->SendCommand("UNREAL!" + gcnew System::String(Command));
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}
}

/**
 * Retrieve the state of the console (running, not running, crashed, etc)
 *
 * @param Handle The handle of the console to retrieve the information from.
 *
 * @return the current state
 */
FConsoleSupport::EConsoleState FXenonSupport::GetConsoleState(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);
	FConsoleSupport::EConsoleState RetState = FConsoleSupport::CS_NotRunning;

	if(Target)
	{
		switch(Target->ExecState)
		{
		case XDevkit::XboxExecutionState::Running:
			{
				RetState = FConsoleSupport::CS_Running;
				break;
			}
		case XDevkit::XboxExecutionState::Rebooting:
		case XDevkit::XboxExecutionState::RebootingTitle:
		case XDevkit::XboxExecutionState::Pending:
		case XDevkit::XboxExecutionState::PendingTitle:
			{
				RetState = FConsoleSupport::CS_Rebooting;
				break;
			}
		case XDevkit::XboxExecutionState::Stopped:
		default:
			{
				RetState = FConsoleSupport::CS_NotRunning;
				break;
			}
		}
	}

	return RetState;
}

/**
 * Have the console take a screenshot and dump to a file
 * 
 * @param Handle The handle of the console to retrieve the information from.
 * @param Filename Location to place the .bmp file
 *
 * @return true if successful
 */
bool FXenonSupport::ScreenshotBMP(TARGETHANDLE Handle, const wchar_t* Filename)
{
	TARGET Target = GetTarget(Handle);
	// make sure it succeeded
	if(!Target)
	{
		return false;
	}

	bool bSucceeded = false;

	try
	{
		Target->Console->ScreenShot(gcnew System::String(Filename));
		bSucceeded = true;
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}
	
	return bSucceeded;
}

/**
 * Return the number of console-specific menu items this platform wants to add to the main
 * menu in UnrealEd.
 *
 * @return	The number of menu items for this console
 */
int FXenonSupport::GetNumMenuItems() 
{
	return (int)MenuItems.size();
}

/**
 * Return the string label for the requested menu item
 * @param	Index		The requested menu item
 * @param	bIsChecked	Is this menu item checked (or selected if radio menu item)
 * @param	bIsRadio	Is this menu item part of a radio group?
 *
 * @return	Label for the requested menu item
 */
const wchar_t* FXenonSupport::GetMenuItem(int Index, bool& bIsChecked, bool& bIsRadio, TARGETHANDLE& OutHandle)
{
	// xenon selection is a radio group
	bIsRadio = Index > 1;
	
	if(bIsRadio)
	{
		bIsChecked = MenuItems[Index].bChecked;

		OutHandle = MenuItems[Index].Target;
		TARGET Target = GetTarget(MenuItems[Index].Target);

		if(Target)
		{
			try
			{
				pin_ptr<const wchar_t> NativeName = PtrToStringChars(Target->DebugTarget->default);
				XenonNameCache = NativeName;
			}
			catch(System::Exception ^ex)
			{
				XenonConsole::OutputDebugException(__FUNCTION__, ex);
				
				pin_ptr<const wchar_t> NativeName = PtrToStringChars(Target->TargetManagerName);
				XenonNameCache = NativeName;
			}

			return XenonNameCache.c_str();
		}
	}
	else
	{
		OutHandle = INVALID_TARGETHANDLE;
	}

	if(Index == 0)
	{
		return L"Reboot Active Xenon";
	}
	else if(Index == 1)
	{
		return MENU_SEPARATOR_LABEL;
	}

	return NULL;
}

/**
 * Internally process the given menu item when it is selected in the editor
 * @param	Index		The selected menu item
 * @param	OutputConsoleCommand	A buffer that the menu item can fill out with a console command to be run by the Editor on return from this function
 * @param	CommandBufferSize		The size of the buffer pointed to by OutputConsoleCommand
 */
void FXenonSupport::ProcessMenuItem(int Index, wchar_t* /*OutputConsoleCommand*/, int /*CommandBufferSize*/)
{
	if(Index == 0)
	{
		// this is the first menu item
		for(size_t i = 2; i < MenuItems.size(); ++i)
		{
			if(!MenuItems[i].bChecked)
			{
				continue;
			}

			TARGET Target = GetTarget(MenuItems[i].Target);

			if(Target)
			{
				// reboot the xbox
				try
				{
					Target->Console->Reboot(nullptr, nullptr, nullptr, XDevkit::XboxRebootFlags::Title);
				}
				catch(System::Exception ^ex)
				{
					XenonConsole::OutputDebugException(__FUNCTION__, ex);
				}
			}
		}
	}
	else if (Index > 1)
	{
		MenuItems[Index].bChecked = !MenuItems[Index].bChecked;
	}
}

/**
 * Returns the global sound cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleSoundCooker* FXenonSupport::GetGlobalSoundCooker() 
{ 
	static FXenonSoundCooker* GlobalSoundCooker = NULL;
	if( !GlobalSoundCooker )
	{
		GlobalSoundCooker = new FXenonSoundCooker();
	}
	return GlobalSoundCooker;
}

/**
 * Returns the global texture cooker object.
 *
 * @return global sound cooker object, or NULL if none exists
 */
FConsoleTextureCooker* FXenonSupport::GetGlobalTextureCooker()
{
	static FXenonTextureCooker* GlobalTextureCooker = NULL;
	if( !GlobalTextureCooker )
	{
		GlobalTextureCooker = new FXenonTextureCooker();
	}
	return GlobalTextureCooker;
}

/**
 * Returns the global skeletal mesh cooker object.
 *
 * @return global skeletal mesh cooker object, or NULL if none exists
 */
FConsoleSkeletalMeshCooker* FXenonSupport::GetGlobalSkeletalMeshCooker() 
{ 
	static FXenonSkeletalMeshCooker* GlobalSkeletalMeshCooker = NULL;
	if( !GlobalSkeletalMeshCooker )
	{
		GlobalSkeletalMeshCooker = new FXenonSkeletalMeshCooker();
	}
	return GlobalSkeletalMeshCooker;
}

/**
 * Returns the global static mesh cooker object.
 *
 * @return global static mesh cooker object, or NULL if none exists
 */
FConsoleStaticMeshCooker* FXenonSupport::GetGlobalStaticMeshCooker() 
{ 
	static FXenonStaticMeshCooker* GlobalStaticMeshCooker = NULL;
	if( !GlobalStaticMeshCooker )
	{
		GlobalStaticMeshCooker = new FXenonStaticMeshCooker();
	}
	return GlobalStaticMeshCooker;
}

/**
 * Returns the global shader precompiler object.
 * @return global shader precompiler object, or NULL if none exists.
 */
FConsoleShaderPrecompiler* FXenonSupport::GetGlobalShaderPrecompiler()
{
	static FXenonShaderPrecompiler* GlobalShaderPrecompiler = NULL;
	if(!GlobalShaderPrecompiler)
	{
		GlobalShaderPrecompiler = new FXenonShaderPrecompiler();
	}
	return GlobalShaderPrecompiler;
}

/**
 * Converts an Unreal Engine texture format to a Xenon texture format.
 *
 * @param	UnrealFormat	The unreal format.
 * @param	Flags			Extra flags describing the format.
 * @return	The associated Xenon format.
 */
D3DFORMAT FXenonSupport::ConvertToXenonFormat(DWORD UnrealFormat, DWORD Flags)
{
	// convert to platform specific format
	D3DFORMAT D3DFormat = ConvertUnrealFormatToD3D(UnrealFormat);
	// sRGB is handled as a surface format on Xe
	if( Flags&TexCreate_SRGB )
	{
		D3DFormat = (D3DFORMAT)MAKESRGBFMT(D3DFormat);
	}	
	// handle un-tiled formats
	if( (Flags&TexCreate_NoTiling) || (Flags & TexCreate_Uncooked) )
	{
		D3DFormat = (D3DFORMAT)MAKELINFMT(D3DFormat);
	}
	return D3DFormat;
}

/**
 *  Gets the platform-specific size required for the given texture.
 *
 *	@param	UnrealFormat	Unreal pixel format
 *	@param	Width			Width of texture (in pixels)
 *	@param	Height			Height of texture (in pixels)
 *	@param	NumMips			Number of miplevels
 *	@param	CreateFlags		Platform-specific creation flags
 *
 *	@return	INT				The size of the memory allocation needed for the texture.
 */
INT FXenonSupport::GetPlatformTextureSize(DWORD UnrealFormat, UINT Width, UINT Height, UINT NumMips, DWORD CreateFlags)
{
	D3DFORMAT Format = ConvertToXenonFormat(UnrealFormat, CreateFlags);
	DWORD D3DCreateFlags = 0;
	if( CreateFlags&TexCreate_NoMipTail ) 
	{
		D3DCreateFlags |= XGHEADEREX_NONPACKED;
	}

	D3DTexture DummyTexture;
	// Create a dummy texture we can query the mip alignment from.
	INT TextureSize = XGSetTextureHeaderEx( 
		Width,							// Width
		Height,							// Height
		NumMips,						// Levels
		0,								// Usage
		Format,							// Format
		0,								// ExpBias
		D3DCreateFlags,					// Flags
		0,								// BaseOffset
		XGHEADER_CONTIGUOUS_MIP_OFFSET,	// MipOffset
		0,								// Pitch
		&DummyTexture,					// D3D texture
		NULL,							// unused
		NULL							// unused
		);

	return TextureSize;
}

/**
 *  Gets the platform-specific size required for the given cubemap texture.
 *
 *	@param	UnrealFormat	Unreal pixel format
 *	@param	Size			Size of the cube edge (in pixels)
 *	@param	NumMips			Number of miplevels
 *	@param	CreateFlags		Platform-specific creation flags
 *
 *	@return	INT				The size of the memory allocation needed for the texture.
 */
INT FXenonSupport::GetPlatformCubeTextureSize(DWORD UnrealFormat, UINT Size, UINT NumMips, DWORD CreateFlags)
{
	D3DFORMAT Format = ConvertToXenonFormat(UnrealFormat, CreateFlags);
	DWORD D3DCreateFlags = 0;
	if( CreateFlags&TextureCreate_NoPackedMip ) 
	{
		D3DCreateFlags |= XGHEADEREX_NONPACKED;
	}

	D3DCubeTexture DummyTexture;
	// Create a dummy texture we can query the mip alignment from.
	INT TextureSize = XGSetCubeTextureHeaderEx( 
		Size,							// Size
		NumMips,						// Levels
		0,								// Usage
		Format,							// Format
		0,								// ExpBias
		D3DCreateFlags,					// Flags
		0,								// BaseOffset
		XGHEADER_CONTIGUOUS_MIP_OFFSET,	// MipOffset
		&DummyTexture,					// D3D texture
		NULL,							// unused
		NULL							// unused
		);

	return TextureSize;
}

/**
 * Retrieves the handle of the default console.
 */
TARGETHANDLE FXenonSupport::GetDefaultTarget()
{
	TARGETHANDLE Handle = INVALID_TARGETHANDLE;

	try
	{
		XDevkit::XboxManagerClass ^Manager = gcnew XDevkit::XboxManagerClass();
		pin_ptr<const wchar_t> NativeDefaultTarget = PtrToStringChars(Manager->DefaultConsole);

		GetTarget(NativeDefaultTarget, Handle);
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}

	return Handle;
}

/**
 * This function exists to delete an instance of FConsoleSupport that has been allocated from a *Tools.dll. Do not call this function from the destructor.
 */
void FXenonSupport::Destroy()
{
	Cleanup();
}

/**
 * Returns true if the specified target is connected for debugging and sending commands.
 * 
 *  @param Handle			The handle of the console to retrieve the information from.
 */
bool FXenonSupport::GetIsTargetConnected(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);
	bool bIsConnected = false;

	if(Target)
	{
		bIsConnected = Target->IsConnected;
	}

	return bIsConnected;
}

/**
* Sets the callback function for TTY output.
*
* @param	Callback	Pointer to a callback function or NULL if TTY output is to be disabled.
* @param	Handle		The handle to the target that will register the callback.
*/
void FXenonSupport::SetTTYCallback(TARGETHANDLE Handle, TTYEventCallbackPtr Callback)
{
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		Target->TTYCallback = System::IntPtr(Callback);
	}
}

/**
* Sets the callback function for handling crashes.
*
* @param	Callback	Pointer to a callback function or NULL if handling crashes is to be disabled.
* @param	Handle		The handle to the target that will register the callback.
*/
void FXenonSupport::SetCrashCallback(TARGETHANDLE Handle, CrashCallbackPtr Callback)
{
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		Target->CrashCallback = System::IntPtr(Callback);
	}
}

/**
 * Retrieves the IP address for the debug channel at the specific target.
 *
 * @param	Handle	The handle to the target to retrieve the IP address for.
 */
unsigned int FXenonSupport::GetDebugChannelIPAddress(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);
	DWORD IPAddr = 0;

	if(Target)
	{
		try
		{
			// ask the console for its game/title ip address
			IPAddr = System::Net::IPAddress::NetworkToHostOrder((int)Target->Console->IPAddress);
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}

	return IPAddr;
}

/**
 * Returns the type of the specified target.
 */
FConsoleSupport::ETargetType FXenonSupport::GetTargetType(TARGETHANDLE Handle)
{
	FConsoleSupport::ETargetType RetVal = FConsoleSupport::TART_Unknown;
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		try
		{
			switch(Target->Console->ConsoleType)
			{
			case XDevkit::XboxConsoleType::DevelopmentKit:
				{
					RetVal = FConsoleSupport::TART_DevKit;
					break;
				}
			case XDevkit::XboxConsoleType::ReviewerKit:
				{
					RetVal = FConsoleSupport::TART_ReviewerKit;
					break;
				}
			case XDevkit::XboxConsoleType::TestKit:
				{
					RetVal = FConsoleSupport::TART_TestKit;
					break;
				}
			}
		}
		catch(System::Exception ^ex)
		{
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}

	return RetVal;
}

/**
 * Sets flags controlling how crash reports will be filtered.
 *
 * @param	Handle	The handle to the target to set the filter for.
 * @param	Filter	Flags controlling how crash reports will be filtered.
 */
bool FXenonSupport::SetCrashReportFilter(TARGETHANDLE Handle, FConsoleSupport::ECrashReportFilter Filter)
{
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		Target->CrashFilter = Filter;
	}

	return true;
}

/**
 * Gets the name of a console as displayed by the target manager.
 *
 * @param	Handle	The handle to the target to set the filter for.
 */
const wchar_t* FXenonSupport::GetTargetManagerNameForConsole(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);

	if(Target)
	{
		pin_ptr<const wchar_t> NativeName = PtrToStringChars(Target->TargetManagerName);
		XenonNameCache = NativeName;

		return XenonNameCache.c_str();
	}

	return NULL;
}

/**
 * Enumerates all available targets for the platform.
 *
 * @returns The number of available targets.
 */
int FXenonSupport::EnumerateAvailableTargets()
{
	static size_t TargetHandles = 1;

	if(!bIsProperlySetup)
	{
		OutputDebugString(TEXT("Warning: FXenonSupport::FXenonSupport() failed setup!\n"));
		return (int)CachedConnections.size();
	}

	try
	{
		// create an IXboxManager - the only way to get a list of the xenons in the Xenon Neighborhood!
		XDevkit::XboxManagerClass ^XenonManager = gcnew XDevkit::XboxManagerClass();

		for each(System::String^ CurConsoleName in XenonManager->Consoles)
		{
			try
			{
				pin_ptr<const wchar_t> TMName = PtrToStringChars(CurConsoleName);

				if(ConnectionNames.find(TMName) == ConnectionNames.end())
				{
					XDevkit::XboxConsole ^CurConsole = XenonManager->OpenConsole(CurConsoleName);

					CurConsole->ConnectTimeout = 500;
					CurConsole->ConversationTimeout = 500;

					XenonConsole ^NewConsole = gcnew XenonConsole(CurConsoleName, CurConsole);

					ConnectionNames.insert(TargetMap::value_type(TMName, NewConsole));
					CachedConnections.insert(TargetHandleMap::value_type((TARGETHANDLE)TargetHandles, NewConsole));
					MenuItems.push_back(FMenuItem((TARGETHANDLE)TargetHandles, CurConsoleName->Equals(XenonManager->DefaultConsole)));
					++TargetHandles;
				}
			}
			catch(System::Exception ^ex)
			{
				XenonConsole::OutputDebugException(System::String::Format("Warning: FXenonSupport::FXenonSupport() Could not open console \'{0}\'", CurConsoleName), ex);
			}
		}
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}

	return (int)CachedConnections.size();
}

/**
 * Asynchronously copies a set of files to a set of targets.
 *
 * @param	Handles						An array of targets to receive the files.
 * @param	HandlesSize					The size of the array pointed to by Handles.
 * @param	SrcFilesToSync				An array of source files to copy. This must be the same size as DestFilesToSync.
 * @param	DestFilesToSync				An array of destination files to copy to. This must be the same size as SrcFilesToSync.
 * @param	FilesToSyncSize				The size of the SrcFilesToSync and DestFilesToSync arrays.
 * @param	DirectoriesToCreate			An array of directories to be created on the targets.
 * @param	DirectoriesToCreateSize		The size of the DirectoriesToCreate array.
 * @param	OutputCallback				TTY callback for receiving colored output.
 */
bool FXenonSupport::SyncFiles(TARGETHANDLE *Handles, int HandlesSize, const wchar_t **SrcFilesToSync, const wchar_t **DestFilesToSync, int FilesToSyncSize, const wchar_t **DirectoriesToCreate, int DirectoriesToCreateSize, ColoredTTYEventCallbackPtr OutputCallback)
{
	array<System::String^> ^ManagedDirectoriesToCreate = gcnew array<System::String^>(DirectoriesToCreateSize);

	for(int DirectoryToCreateIndex = 0; DirectoryToCreateIndex < DirectoriesToCreateSize; ++DirectoryToCreateIndex)
	{
		ManagedDirectoriesToCreate[DirectoryToCreateIndex] = gcnew System::String(DirectoriesToCreate[DirectoryToCreateIndex]);
	}

	// NOTE: We have to copy file names into another array because this function can return before the copying thread does
	array<System::String^> ^ManagedSrcFilesToSync = gcnew array<System::String^>(FilesToSyncSize);
	array<System::String^> ^ManagedDestFilesToSync = gcnew array<System::String^>(FilesToSyncSize);
	
	for(int i = 0; i < FilesToSyncSize; ++i)
	{
		ManagedSrcFilesToSync[i] = gcnew System::String(SrcFilesToSync[i]);
		ManagedDestFilesToSync[i] = gcnew System::String(DestFilesToSync[i]);
	}

	vector<HANDLE> ThreadHandles;
	bool bRetVal = true;

	for(int i = 0; i < HandlesSize; ++i)
	{
		TARGET CurTarget = GetTarget(Handles[i]);

		if(CurTarget)
		{
			FSyncThreadInfo *Info = new FSyncThreadInfo();

			try
			{
				Info->OutputCallback = OutputCallback;
				Info->DirectoriesToCreate = ManagedDirectoriesToCreate;
				Info->DestFilesToSync = ManagedDestFilesToSync;
				Info->SrcFilesToSync = ManagedSrcFilesToSync;

				pin_ptr<const wchar_t> NativeTMName = PtrToStringChars(CurTarget->TargetManagerName);
				Info->TargetToSync = NativeTMName;

				DWORD ThreadId;
				HANDLE SyncThreadHandle = CreateThread(NULL, 0, SyncThreadMain, Info, 0, &ThreadId);

				if(!SyncThreadHandle)
				{
					throw gcnew System::Exception();
				}

				ThreadHandles.push_back(SyncThreadHandle);
			}
			catch(System::Exception ^ex)
			{
				delete Info;
				Info = NULL;

				XenonConsole::OutputDebugException(__FUNCTION__, ex);
				bRetVal = false;

				try
				{
					if(OutputCallback)
					{
						pin_ptr<const wchar_t> Msg = PtrToStringChars(System::String::Format("Could not create sync thread for target \'{0}\'.", CurTarget->DebugTarget->default));
						OutputCallback(COLOR_RED, Msg);
					}
				}
				catch(System::Exception ^ex)
				{
					XenonConsole::OutputDebugException(__FUNCTION__, ex);
				}
			}
		}
		else
		{
			bRetVal = false;
		}
	}

	// block-local variable 
	DWORD ThreadReturnCode; 
	MSG msg; 
	int ActiveThreadCount = 0;

	// Read all of the messages in this next loop, 
	// removing each message as we read it.
	do
	{ 
		ActiveThreadCount = 0;

		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// If it is a quit message, exit.
			if(msg.message == WM_QUIT)
			{
				bRetVal = false;
				break;
			}

			// Otherwise, dispatch the message.
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		for(size_t i = 0; i < ThreadHandles.size(); ++i)
		{
			GetExitCodeThread(ThreadHandles[i], &ThreadReturnCode);

			if(ThreadReturnCode != STILL_ACTIVE)
			{
				if(ThreadReturnCode != 0)
				{
					bRetVal = false;
				}
			}
			else
			{
				++ActiveThreadCount;
			}
		}
	} while(ActiveThreadCount > 0);

	for(size_t i = 0; i < ThreadHandles.size(); ++i)
	{
		CloseHandle(ThreadHandles[i]);
	}

	return bRetVal;
}

/**
 * This handler creates a multi-threaded apartment from which to create all of the target COM objects so we can sync in multiple worker threads.
 *
 * @param	lpParameter		Pointer to extra thread data.
 */
DWORD WINAPI FXenonSupport::SyncThreadMain(void *lpParameter)
{
	FSyncThreadInfo *ThreadData = (FSyncThreadInfo*)lpParameter;

	// initialize this thread in the STA
	if(FAILED(CoInitialize(NULL)) || ThreadData == NULL)
	{
		delete ThreadData;
		return 1;
	}

	DWORD ReturnCode = 0;

	try
	{
		System::String ^TargetMgrName = gcnew System::String(ThreadData->TargetToSync.c_str());
		XDevkit::XboxManagerClass ^XenonManager = gcnew XDevkit::XboxManagerClass();
		XDevkit::XboxConsole ^Target = nullptr;
		System::String ^TargetName = System::String::Empty;

		for each(System::String ^CurTargetName in XenonManager->Consoles)
		{
			if(CurTargetName->Equals(TargetMgrName, System::StringComparison::OrdinalIgnoreCase))
			{
				Target = XenonManager->OpenConsole(CurTargetName);
				TargetName = Target->DebugTarget->default;
				break;
			}
		}

		for(int i = 0; i < ThreadData->DirectoriesToCreate->Length; ++i)
		{
			if(!MakeDirectoryHierarchy(Target, ThreadData->DirectoriesToCreate[i]))
			{
				ReturnCode = 1;
			}
		}

		for(int i = 0; i < ThreadData->SrcFilesToSync->Length; ++i)
		{
			System::String ^DestFileToCopy = ThreadData->DestFilesToSync[i];
			System::String ^SrcFileToCopy = ThreadData->SrcFilesToSync[i];
			System::String ^ResolvedDestFileToCopy = "xe:\\" + DestFileToCopy;
			bool bShouldCopy = true;

			try
			{
				bShouldCopy = XenonConsole::NeedsToCopyFile(Target, SrcFileToCopy, DestFileToCopy, false);
			}
			catch(System::Exception^)
			{
				bShouldCopy = true;
			}
	
			if(bShouldCopy)
			{
				DWORD Ticks = GetTickCount();
				bool bCopiedFile = false;
				bool bWarnedAboutRetry = false;
	
				do 
				{
					try
					{
						Target->SendFile(SrcFileToCopy, ResolvedDestFileToCopy);
						bCopiedFile = true;
						ReturnCode = 0;

						if(ThreadData->OutputCallback)
						{
							pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(System::String::Format("Copied file \'{0}\' to target \'{1}\'", ResolvedDestFileToCopy, TargetName));
							ThreadData->OutputCallback(COLOR_BLACK, NativeMsg);
						}
					}
					catch(System::Runtime::InteropServices::COMException ^ex)
					{
						ReturnCode = 1;

						if(ex->ErrorCode == XBDM_BADFILENAME || ex->ErrorCode == XBDM_NOSUCHFILE || ex->ErrorCode == XBDM_NOSUCHPATH)
						{
							// exit loop since the file can never be copied
							bCopiedFile = true;
							System::String ^ErrMsg = System::String::Format("Failed to copy file \'{0}\' to target \'{1}\'. Final error code is \'{2}\'.", ResolvedDestFileToCopy, TargetName, XenonConsole::XBDMHResultToString(ex->ErrorCode));
							XenonConsole::OutputDebugException(ErrMsg, ex);

							if(ThreadData->OutputCallback)
							{
								pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(ErrMsg);
								ThreadData->OutputCallback(COLOR_RED, NativeMsg);
							}
						}
						else if (!bWarnedAboutRetry)
						{
							System::String ^ErrMsg = System::String::Format("Retrying copy from \'{0}\' to target \'{1}\', error code was \'{2}\'.", ResolvedDestFileToCopy, TargetName, XenonConsole::XBDMHResultToString(ex->ErrorCode));
							XenonConsole::OutputDebugException(ErrMsg, ex);

							if(ThreadData->OutputCallback)
							{
								pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(ErrMsg);
								ThreadData->OutputCallback(COLOR_YELLOW, NativeMsg);
							}
							bWarnedAboutRetry = true;
						}
					}
					catch(System::Exception ^ex)
					{
						ReturnCode = 1;

						System::String ^ErrMsg = System::String::Format("Failed to copy file \'{0}\' to target \'{1}\'.", ResolvedDestFileToCopy, Target->DebugTarget->default);
						XenonConsole::OutputDebugException(ErrMsg, ex);

						if(ThreadData->OutputCallback)
						{
							pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(ErrMsg);
							ThreadData->OutputCallback(COLOR_RED, NativeMsg);
						}
					}
				}
				while(!bCopiedFile && GetTickCount() - Ticks < SENDFILE_RETRY);
	
				if(!bCopiedFile && GetTickCount() - Ticks >= SENDFILE_RETRY)
				{
					ReturnCode = 1;
	
					if(ThreadData->OutputCallback)
					{
						pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(System::String::Format("Failed to copy file \'{0}\' to target \'{1}\' due to timeout.", ResolvedDestFileToCopy, TargetName));
						ThreadData->OutputCallback(COLOR_RED, NativeMsg);
					}
				}
			}
#if _DEBUG
			else if(ThreadData->OutputCallback)
			{
				pin_ptr<const wchar_t> NativeMsg = PtrToStringChars(System::String::Format("Skipped file \'{0}\' on target \'{1}\'", ResolvedDestFileToCopy, TargetName));
				ThreadData->OutputCallback(COLOR_BLACK, NativeMsg);
			}
#endif
		}
	}
	catch(System::Exception ^ex)
	{
		ReturnCode = 1;
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
	}

	CoUninitialize();

	delete ThreadData;

	return ReturnCode;
}

/**
 * Creates a directory hierarchy on the target.
 *
 * @param	DirectoryHierarchy	The directory hierarchy to be created.
 */
bool FXenonSupport::MakeDirectoryHierarchy(XDevkit::XboxConsole ^Target, System::String ^DirectoryHierarchy)
{
	// find the last slash, if there is one
	int LastSlash = DirectoryHierarchy->LastIndexOf(L'\\');
	bool bResult = true;

	// make sure there was a slash, and it wasn't the first character
	if(LastSlash != -1)
	{
		// if there was a valid slash, make the outer directory
		bResult = MakeDirectoryHierarchy(Target, DirectoryHierarchy->Substring(0, LastSlash));

		if(!bResult)
		{
			return bResult;
		}
	}

	if(DirectoryHierarchy->EndsWith("\\"))
	{
		DirectoryHierarchy = DirectoryHierarchy->Substring(0, DirectoryHierarchy->Length - 1);
	}

	System::String ^FinalDirectory = "xe:\\" +  DirectoryHierarchy;

	try
	{
		Target->MakeDirectory(FinalDirectory);
	}
	catch(System::Runtime::InteropServices::COMException ^ex)
	{
		if(ex->ErrorCode != XBDM_ALREADYEXISTS)
		{
			bResult = false;
			XenonConsole::OutputDebugException(__FUNCTION__, ex);
		}
	}
	catch(System::Exception ^ex)
	{
		XenonConsole::OutputDebugException(__FUNCTION__, ex);
		bResult = false;
	}

	return bResult;
}

/**
 * Gets the dump type the current target is set to.
 *
 * @param	Handle	The handle to the target to get the dump type from.
 */
FConsoleSupport::EDumpType FXenonSupport::GetDumpType(TARGETHANDLE Handle)
{
	TARGET Target = GetTarget(Handle);

	if(!Target)
	{
		return FConsoleSupport::DMPT_Normal;
	}

	FConsoleSupport::EDumpType DmpType = FConsoleSupport::DMPT_Normal;

	switch(Target->DumpFlags)
	{
	case XDevkit::XboxDumpFlags::WithFullMemory:
		{
			DmpType = FConsoleSupport::DMPT_WithFullMemory;
			break;
		}
	}

	return DmpType;
}

/**
 * Sets the dump type of the current target.
 *
 * @param	DmpType		The new dump type of the target.
 * @param	Handle		The handle to the target to set the dump type for.
 */
void FXenonSupport::SetDumpType(TARGETHANDLE Handle, EDumpType DmpType)
{
	TARGET Target = GetTarget(Handle);

	if(!Target)
	{
		return;
	}

	XDevkit::XboxDumpFlags DmpFlags = XDevkit::XboxDumpFlags::Normal;

	switch(DmpType)
	{
	case FConsoleSupport::DMPT_WithFullMemory:
		{
			DmpFlags = XDevkit::XboxDumpFlags::WithFullMemory;
			break;
		}
	}

	Target->DumpFlags = DmpFlags;
}





using namespace System;
using namespace System::Reflection;

// the windows UNICODE renaming stuff is conflicting with Managed code here
#undef GetEnvironmentVariable

/**
 * Redirect the system request for the XDevkit dll to the one in the Xenon subdirectory, 
 * so it doesn't need to live in Binaries, Win32, and Win64
 */
static Assembly^ MyResolveHandler(Object^ /*Sender*/, ResolveEventArgs^ Args)
{
	// only handle the one we want
	if (Args->Name->Contains("XDevkit"))
	{
		// only needs loading once
		static gcroot<Assembly^> APIAssembly = nullptr;
		if ((Assembly^)APIAssembly == nullptr)
		{
			// get the path to the xenon subdirectory from Binaries or a subdirectory (UFE.exe vs ExampleGame.exe)
			String^ DLLPath = Environment::CurrentDirectory;
			if (!DLLPath->EndsWith("Binaries", System::StringComparison::OrdinalIgnoreCase))
			{
				DLLPath = System::IO::Path::Combine(DLLPath, "..");
			}
			DLLPath = System::IO::Path::Combine(DLLPath, "Xbox360\\Interop.XDevkit.1.0.dll");

			// this will throw an error on failure
			APIAssembly = Assembly::LoadFrom(DLLPath);
		}

		return APIAssembly;
	}

	return nullptr;
}

/** 
 * Returns a pointer to a subclass of FConsoleSupport.
 *
 * @return The pointer to the console specific FConsoleSupport
 */
CONSOLETOOLS_API FConsoleSupport* GetConsoleSupport()
{
	static FXenonSupport* XenonSupport = NULL;
	if (XenonSupport == NULL)
	{
		// add support for finding interop DLL in XDK directory
		AppDomain::CurrentDomain->AssemblyResolve += gcnew ResolveEventHandler(MyResolveHandler);

		XenonSupport = new FXenonSupport();
	}

	if(XenonSupport->IsProperlySetup())
	{
		return XenonSupport;
	}
	else
	{
		return NULL;
	}
}

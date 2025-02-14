/*=============================================================================
	PS3Client.cpp: UPS3Client code.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PS3Drv.h"
#include "FFileManagerPS3.h"
#include <sys/tty.h>
	
#if !FINAL_RELEASE
#include <sysutil/sysutil_msgdialog.h>
#include "PS3Gcm.h"
#include "PS3DownloadableContent.h"
#include "UnIpDrv.h"
#endif

/** dummy function used by caller to be treated as a leaf function */
INT PS3DummyFunction()
{
	static INT TempValue=0;
	return TempValue;
}

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UPS3Client);

/*-----------------------------------------------------------------------------
	UPS3Client implementation.
-----------------------------------------------------------------------------*/

//
// UPS3Client constructor.
//
UPS3Client::UPS3Client()
: Viewport(NULL)
, Engine(NULL)
, AudioDevice(NULL)
{
}

//
// Static init.
//
void UPS3Client::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UPS3Client, Engine ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( UPS3Client, AudioDevice ) );
}

void UPS3Client::Init( UEngine* InEngine )
{
	Engine = InEngine;

	// Initialize the audio device.
	if( GEngine->bUseSound )
	{
		AudioDevice = ConstructObject<UAudioDevice>(UPS3AudioDevice::StaticClass());
		if( !AudioDevice->Init() )
		{
			AudioDevice = NULL;
		}
	}

	// remove bulk data if no sounds were initialized
	if( AudioDevice == NULL )
	{
		appSoundNodeRemoveBulkData();
	}

	// Success.
	debugf( NAME_Init, TEXT("Client initialized") );
}

//
//	UPS3Client::Serialize - Make sure objects the client reference aren't garbage collected.
//
void UPS3Client::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Engine << AudioDevice;
}


//
//	UPS3Client::Destroy - Shut down the platform-specific viewport manager subsystem.
//
void UPS3Client::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if( GFullScreenMovie )
		{
			// force movie playback to stop
			GFullScreenMovie->GameThreadStopMovie(0,FALSE,TRUE);
		}

		// Close all viewports.
		Viewport->Destroy();
		delete Viewport;

		debugf( NAME_Exit, TEXT("PS3 client shut down") );
	}

	Super::FinishDestroy();
}

//
//	UPS3Client::Tick - Perform timer-tick processing on all visible viewports.
//
#if !FINAL_RELEASE
FString GTestSaveName;
#endif

void UPS3Client::Tick( FLOAT DeltaTime )
{
	// Process input.
	SCOPE_CYCLE_COUNTER(STAT_InputTime);

	GInputLatencyTimer.GameThreadTick();
	Viewport->ProcessInput(DeltaTime);

#if !FINAL_RELEASE
	// check for delete completion, so then we just 
	if (GFileManagerPS3->TickDeleteFromList())
	{
		Exec(*(FString(TEXT("PS3SAVE ")) + GTestSaveName), *GNull);
	}
#endif

}


#if !FINAL_RELEASE

/** The file manager will store the imported mod name so we can clean it up */
FString GFIOSTestModName;
void FIOSTestCallback(INT ButtonType, void *UserData)
{
	FString SourceFilename = GSys->CachePath * TEXT("CacheTestSource.bin");
	FString DestFilename = GSys->CachePath * TEXT("CacheTestDest.bin");
	GFileManager->Delete(*SourceFilename);
	GFileManager->Delete(*DestFilename);

	if (GFIOSTestModName != TEXT(""))
	{
		GPlatformDownloadableContent->DeleteDownloadableContent(*GFIOSTestModName);
	}

	// let the user delete from a list (and tests this code at the same time)
	GFileManagerPS3->BeginDeleteFromList();				
}
#endif


//
//	UPS3Client::Exec
//

UBOOL UPS3Client::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if( Super::Exec(Cmd,Ar) )
	{
		return TRUE;
	}
#if !FINAL_RELEASE
	else if (ParseCommand(&Cmd, TEXT("TESTFIOS")))
	{
		// perform a bunch of stuff to test fios functionality

		////////////////////////////////////////////////////////////////
		// test screenshots
		////////////////////////////////////////////////////////////////
		if (GEngine->GamePlayers(0))
		{
			GEngine->GamePlayers(0)->Exec(TEXT("SHOT"), Ar);
		}

		////////////////////////////////////////////////////////////////
		// test movie playback
		////////////////////////////////////////////////////////////////
		GEngine->Exec(TEXT("MOVIETEST UE3_Logo"), Ar);

		////////////////////////////////////////////////////////////////
		// test cache directory (with rename like UDownload)
		////////////////////////////////////////////////////////////////
		FString SourceFilename = GSys->CachePath * TEXT("CacheTestSource.bin");
		FString DestFilename = GSys->CachePath * TEXT("CacheTestDest.bin");

		// create a file in the cache directory
		FArchive* CacheDirTest = GFileManager->CreateFileWriter(*SourceFilename, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
		CacheDirTest->Serialize((void*)"**", 2);
		delete CacheDirTest;

		// move and verify
		GFileManager->Move(*DestFilename, *SourceFilename);
		check(GFileManager->FileSize(*DestFilename) == 2);


		////////////////////////////////////////////////////////////////
		// test save games
		////////////////////////////////////////////////////////////////
		// storage for more error info
		FStringOutputDevice ErrorString;

		// open save file for writing
		FArchive* SaveFile = GFileManager->CreateFileWriter(TEXT("FIOSTEST\\test.txt"), FILEWRITE_SaveGame | FILEWRITE_Async, &ErrorString);

		// create "data"
		FString Payload(TEXT("FIOS Unittest"));

		// write out the data
		(*SaveFile) << Payload;

		// start async writing
		SaveFile->Close();

		// use a flip pumper to show the save indicator, as well as the following mod import screen
		FlushRenderingCommands();
		appPS3StartFlipPumperThread();

		appSleep(1.0f);

		////////////////////////////////////////////////////////////////
		// test mod importing
		////////////////////////////////////////////////////////////////
		GFIOSTestModName = TEXT("");
		GFileManagerPS3->BeginImportModFromMedia();

		INT ReturnValue;
		do
		{
			appSleep(0.3);
			ReturnValue = GFileManagerPS3->TickImportMod();
		}
		while (!(ReturnValue == MIS_Succeeded || ReturnValue == MIS_Failed || ReturnValue == MIS_Canceled || ReturnValue == MIS_Inactive));

		appPS3StopFlipPumperThread();

		////////////////////////////////////////////////////////////////
		// tell the user to check out the HDD and then hit OK to clean up
		////////////////////////////////////////////////////////////////

		extern char GSonyTitleID[10];
		extern FString GBaseDirectory;

		debugf(TEXT("============================================================================================"));
		debugf(TEXT("Go to target manager, FileSync tab, and make sure you see the following:"));
		debugf(TEXT("  /dev_hdd0/game/%s/USRDIR/%s/%sGAME/PS3CACHE/CACHETESTDEST.BIN"), ANSI_TO_TCHAR(GSonyTitleID), *GBaseDirectory.ToUpper(), *FString(appGetGameName()).ToUpper());
		debugf(TEXT("  /dev_hdd0/game/%s/USRDIR/DLC/%s/"), ANSI_TO_TCHAR(GSonyTitleID), *GFIOSTestModName.ToUpper());

		cell::fios::filerange_t ScreenShotDir;
		ScreenShotDir.offset = 0;
		ScreenShotDir.length = FIOS_PATH_MAX;
		GetPlatformFilename(*GSys->ScreenShotPath, ScreenShotDir.path);
		GScheduler->resolveSync(NULL, &ScreenShotDir, NULL);
		debugf(TEXT("Also, there should be screenshot in %s (which won't be deleted, sorry)"), ANSI_TO_TCHAR(ScreenShotDir.path));
		debugf(TEXT("============================================================================================"));

		cellMsgDialogOpen2(CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL | CELL_MSGDIALOG_TYPE_BG_VISIBLE | CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK,
			"View the TTY log for what to look for on the PS3 HD, then press X to delete temp files. You will then be asked to choose a file to delete. Verify the icon is correct, then delete the MOST RECENT file (should be top-most).",
			FIOSTestCallback, NULL, NULL);

		return TRUE;
	}

#if WITH_UE3_NETWORKING
	else if (ParseCommand(&Cmd, TEXT("DUMPGUIDCACHE")))
	{
		for (TObjectIterator<UGuidCache> It; It; ++It)
		{
			for (TMap<FName, FGuid>::TIterator It2(It->PackageGuidMap); It2; ++It2)
			{
				debugf(TEXT("%s -> %s"), *It2.Key().ToString(), *It2.Value().String());
			}
		}
		return TRUE;
	}
#endif

#endif
	else if (ParseCommand(&Cmd, TEXT("PS3CALLSTACK")))
	{
		PS3Callstack();
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("FLUSHFILECACHE")))
	{
		GFileManagerPS3->FlushFileCache();
		GPackageFileCache->CachePaths();

		return TRUE;
	}
	//CNN - add tilt sensor commands
// 	else if (ParseCommand(&Cmd, TEXT("PS3TILT")))
// 	{
// 		extern UBOOL bToggleUseTilt;
// 		FString ValueString;
// 		const INT Value = ParseToken(Cmd,ValueString,TRUE) ? appAtoi(*ValueString) : 0;
// 		bToggleUseTilt = (Value > 0) ? TRUE : FALSE;
// 		debugf(TEXT("PS3 controller tilt sensor %s"), bToggleUseTilt ? TEXT("ON") : TEXT("OFF"));
// 		return TRUE;
// 	}
// 	else if (ParseCommand(&Cmd, TEXT("PS3TILTFILTER")))
// 	{
// 		extern UBOOL bToggleUseFilter;
// 		FString ValueString;
// 		const INT Value = ParseToken(Cmd,ValueString,TRUE) ? appAtoi(*ValueString) : 0;
// 		bToggleUseFilter = (Value > 0) ? TRUE : FALSE;
// 		debugf(TEXT("PS3 controller tilt sensor filtering %s"), bToggleUseFilter ? TEXT("ON") : TEXT("OFF"));
// 		return TRUE;
// 	}
	else if (ParseCommand(&Cmd, TEXT("PS3TILTCALIBRATE")))
	{
		extern UBOOL bCalibrateThisFrame;
		bCalibrateThisFrame = TRUE;
		debugf(TEXT("PS3 controller tilt sensor calibrated"));
		return TRUE;
	}
	//CNN - END
	else if (ParseCommand(&Cmd, TEXT("PS3DUMPGPU")))
	{
#if USE_PS3_RHI
		// does user want long or short version
		UBOOL bShowIndividual = appStricmp(Cmd, TEXT("long")) == 0;
		GPS3Gcm->GetLocalAllocator()->Dump(TEXT("Local (GPU) memory"), bShowIndividual);

		for( INT HostMemIdx=0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
		{
			const EHostMemoryHeapType HostMemType = (EHostMemoryHeapType)HostMemIdx; 
			if( GPS3Gcm->HasHostAllocator(HostMemType) )
			{
				GPS3Gcm->GetHostAllocator(HostMemType)->Dump(GPS3Gcm->GetHostAllocatorDesc(HostMemType), bShowIndividual);
			}
		}
		INT TexPoolUsedSize=0;
		INT TexPoolAvailableSize=0;
		INT PendingMemoryAdjustment=0;
		RHIGetTextureMemoryStats(TexPoolUsedSize, TexPoolAvailableSize, PendingMemoryAdjustment);
		debugf(TEXT("Texture Memory Pool %.2f KB [%.2f KB]"),TexPoolUsedSize,TexPoolAvailableSize);
		//	GPS3Gcm->CompressionTagMem->Dump(TEXT("Compression Tag Mem"));
#endif // USE_PS3_RHI
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3DUMPGPU2AR")))
	{
#if USE_PS3_RHI
		// does user want long or short version
		UBOOL bShowIndividual = appStricmp(Cmd, TEXT("long")) == 0;
		GPS3Gcm->GetLocalAllocator()->DumpToArchive(TEXT("Local (GPU) memory"), bShowIndividual, Ar);

		for( INT HostMemIdx=0; HostMemIdx < HostMem_MAX; HostMemIdx++ )
		{
			const EHostMemoryHeapType HostMemType = (EHostMemoryHeapType)HostMemIdx; 
			if( GPS3Gcm->HasHostAllocator(HostMemType) )
			{
				GPS3Gcm->GetHostAllocator(HostMemType)->DumpToArchive(GPS3Gcm->GetHostAllocatorDesc(HostMemType), bShowIndividual, Ar);
			}
		}
		INT TexPoolUsedSize=0;
		INT TexPoolAvailableSize=0;
		INT PendingMemoryAdjustment=0;
		RHIGetTextureMemoryStats(TexPoolUsedSize, TexPoolAvailableSize, PendingMemoryAdjustment);
		Ar.Logf(TEXT("Texture Memory Pool %.2f KB [%.2f KB]"),TexPoolUsedSize,TexPoolAvailableSize);
		//	GPS3Gcm->CompressionTagMem->Dump(TEXT("Compression Tag Mem"));
#endif // USE_PS3_RHI
		return TRUE;
	}
#if WITH_UE3_NETWORKING
	else if (ParseCommand(&Cmd, TEXT("PS3GETGAMEIP")))
	{
		extern TCHAR GHostIP[64];
		unsigned int NumBytes;
		FString Out = TEXT("GAMEIP");
		Out += GHostIP;
		sys_tty_write(SYS_TTYP15, TCHAR_TO_ANSI(*Out), Out.Len(), &NumBytes);
		debugf(TEXT("PS3 game IP is %s"), GHostIP);
		return TRUE;
	}
#endif	//#if WITH_UE3_NETWORKING
	else if (ParseCommand(&Cmd, TEXT("PS3SCREENSHOT")))
	{
		TArray<FColor>	Bitmap;
		if(Viewport && Viewport->ReadPixels(Bitmap))
		{
			check(Bitmap.Num() == Viewport->GetSizeX() * Viewport->GetSizeY());

			// Save the contents of the array to the specified bmp file
			appCreateBitmap(Cmd, Viewport->GetSizeX(), Viewport->GetSizeY(), &Bitmap(0), GFileManager);			
		}
		// tell the DLL that we are done with the screenshot
		unsigned int NumBytes;
		FString Out = "SCREENSHOTREADY";
		sys_tty_write(SYS_TTYP15, TCHAR_TO_ANSI(*Out), Out.Len(), &NumBytes);

		return TRUE;
	}
#if !FINAL_RELEASE
	// some test execs for testing save/load
	else if (ParseCommand(&Cmd, TEXT("PS3SAVE")))
	{
		// save off the name for when we call it again after delete from list
		GTestSaveName = Cmd;

		FString SaveName = FString(Cmd) + TEXT("\\save.txt");

		// storage for more error info
		FStringOutputDevice ErrorString;

		// open save file for writing
		FArchive* SaveFile = GFileManager->CreateFileWriter(*SaveName, FILEWRITE_SaveGame, &ErrorString);

		// create "data"
		FString Payload = FString(Cmd) + TEXT(" has saved the game");

		// write out the data
		(*SaveFile) << Payload;

		// close the archive, and check for an error
		UBOOL bSaveFailed = !SaveFile->Close();

		// delete the object
		delete SaveFile;

		static UBOOL bAttemptDelete = TRUE;
		// why did save fail?
		if (bSaveFailed)
		{
			if (ErrorString == TEXT("NOSPACE") && bAttemptDelete)
			{
				// if we are out of space, then we need to let user delete something
				GFileManagerPS3->BeginDeleteFromList();				

				// only allow for delete one time, so user can back out
				bAttemptDelete = FALSE;
			}
			else
			{
				debugf(TEXT("Failed to autosave [%s]!"), *ErrorString);
				bAttemptDelete = TRUE;
			}
		}
		else
		{
			bAttemptDelete = TRUE;
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3LOAD")))
	{
		FString SaveName = FString(Cmd) + TEXT("\\save.txt");

		// open save file for reading
		FArchive* SaveFile = GFileManager->CreateFileReader(*SaveName, FILEREAD_SaveGame);
		debugf(TEXT("Errored? %d"), SaveFile->Close());
/*
		FString Payload = "ERROR";
		if (SaveFile)
		{
			// read "data"
			(*SaveFile) << Payload;
		}

		// print out data
		debugf(TEXT("Payload is %s"), *Payload);
*/
		// close the file
		delete SaveFile;

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3IMPORT")))
	{
		GFileManagerPS3->BeginImportModFromMedia();
		return TRUE;
	}

	else if (ParseCommand(&Cmd, TEXT("PS3SUSPEND")))
	{
		// Enqueue command to suspend rendering during blocking load.
		ENQUEUE_UNIQUE_RENDER_COMMAND( SuspendRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = TRUE; } );
		// Flush rendering commands to ensure we don't have any outstanding work on rendering thread
		// and rendering is indeed suspended.
		FlushRenderingCommands();

		appSleep(Max(1.0f, appAtof(Cmd)));

		// Resume rendering again now that we're done loading.
		ENQUEUE_UNIQUE_RENDER_COMMAND( ResumeRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = FALSE; RHIResumeRendering(); } );

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3DLCLIST")))
	{
		TArray<FString> DLCs = GDownloadableContent->GetDownloadableContentList();
		for (INT DLCIndex = 0; DLCIndex < DLCs.Num(); DLCIndex++)
		{
			debugf(TEXT("DLC[%d] = %s"), DLCIndex, *DLCs(DLCIndex));
		}

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3DLCDEL")))
	{
		GPlatformDownloadableContent->DeleteDownloadableContent(Cmd);
		GDownloadableContent->RemoveAllDownloadableContent();
		GPlatformDownloadableContent->FindDownloadableContent();
		Exec(TEXT("PS3DLCLIST"), Ar);
		return TRUE;
	}

	else if (ParseCommand(&Cmd, TEXT("PS3WRITE")))
	{
		INT Flags = 0;
		if (appStricmp(Cmd, TEXT("APPEND")) == 0)
		{
			Flags = FILEWRITE_Append;
		}
		else if (appStricmp(Cmd, TEXT("READONLY")) == 0)
		{
			Flags = FILEWRITE_EvenIfReadOnly;
		}
		else if (appStricmp(Cmd, TEXT("NOREPLACE")) == 0)
		{
			Flags = FILEWRITE_NoReplaceExisting;
		}
		if (appStricmp(Cmd, TEXT("READ")) == 0)
		{
			Flags = FILEWRITE_AllowRead;
		}

		FArchive* Ar = GFileManager->CreateFileWriter(TEXT("..\\Binaries\\PS3Test.txt"), Flags);
		debugf(TEXT("PS3WRITE: Flags = %x, Ar = %x"), Flags, Ar);
		if (Ar)
		{
			Ar->Serialize((void*)"hi mom\r\n", 8);
		}
		delete Ar;

		// make sure it's cached to HD
		Ar = GFileManager->CreateFileReader(TEXT("..\\Binaries\\PS3Test.txt"));
		delete Ar;

		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3DELETE")))
	{
		UBOOL Ret = GFileManager->Delete(TEXT("..\\Binaries\\PS3Test.txt"), appStricmp(Cmd, TEXT("REQUIRE")) == 0, appStricmp(Cmd, TEXT("READONLY")) == 0);
		debugf(TEXT("PS3DELETE [%s] = %d"), Cmd, Ret);

		return TRUE;
	}
#if !USE_NULL_RHI
	else if (ParseCommand(&Cmd, TEXT("PS3SINGLESTEP")))
	{
		extern UBOOL GSingleStepGPU;
		GSingleStepGPU = !GSingleStepGPU;
		Ar.Logf( TEXT("Single-stepping each drawcall: %s"), GSingleStepGPU ? TEXT("ENABLED") : TEXT("DISABLED") );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3PPUPATCHING")))
	{
		extern UBOOL bGUseSPUPatching;
		bGUseSPUPatching = !bGUseSPUPatching;
		Ar.Logf( TEXT("Patching pixelshaders on %s"), bGUseSPUPatching ? TEXT("SPU") : TEXT("PPU") );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3CRASHRSX")))
	{
		printf( "Causing a crash on the RSX!\n" );
		extern UBOOL GCrashRSX;
		GCrashRSX = TRUE;
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3TOGGLEDRAWLABEL")))
	{
		extern UBOOL GEnableDrawCounter;
		GEnableDrawCounter = !GEnableDrawCounter;
		Ar.Logf( TEXT("Adding a backend label between each drawcall: %s"), GEnableDrawCounter ? TEXT("ENABLED") : TEXT("DISABLED") );
		return TRUE;
	}
	else if (ParseCommand(&Cmd, TEXT("PS3THROTTLETEXTUREQUADS")))
	{
		extern UBOOL GThrottleTextureQuads;
		GThrottleTextureQuads = !GThrottleTextureQuads;
		Ar.Logf( TEXT("Throttle the number of texture quads used: %s"), GThrottleTextureQuads ? TEXT("ENABLED") : TEXT("DISABLED") );
		return TRUE;
	}
#endif	//#if !USE_NULL_RHI
	else if (ParseCommand(&Cmd, TEXT("PS3RENDERTHREADPRIO")))
	{
		INT NewRenderThreadPriority;
		NewRenderThreadPriority = appAtoi(Cmd);
		Ar.Logf( TEXT("NewRenderThreadPriority = %d"), NewRenderThreadPriority );
		extern FRunnableThread* GRenderingThread;
		sys_ppu_thread_set_priority( GRenderingThread->GetThreadID(), NewRenderThreadPriority );
		return TRUE;
	}

#endif	// !FINAL_RELEASE
#if !USE_NULL_RHI
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	else if (ParseCommand(&Cmd, TEXT("ToggleDefrag")))
	{
		extern UBOOL GEnableTextureMemoryDefragmentation;
		GEnableTextureMemoryDefragmentation = !GEnableTextureMemoryDefragmentation;
		Ar.Logf( TEXT("Texture memory defragmentation: %s"), GEnableTextureMemoryDefragmentation ? TEXT("ENABLED") : TEXT("DISABLED") );
		return TRUE;
	}
#endif
#endif	//#if !USE_NULL_RHI
#if USE_PS3_RHI && RHI_SUPPORTS_PREVERTEXSHADER_CULLING
	else if( ParseCommand(&Cmd,TEXT("TOGGLEEDGEGEOMETRY")) )
	{
		extern UBOOL GUseEdgeGeometry;
		GUseEdgeGeometry = !GUseEdgeGeometry;
		debugf(TEXT("Edge Geometry is now %s"), GUseEdgeGeometry ? TEXT("enabled") : TEXT("disabled"));
		return TRUE;
	}
#endif

	return FALSE;
}

FViewportFrame* UPS3Client::CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen)
{
	check(Viewport == NULL);
	Viewport = new FPS3Viewport(this, ViewportClient, SizeX, SizeY);
	return Viewport;
}

//
//	UPS3Client::CreateWindowChildViewport
//
FViewport* UPS3Client::CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX,UINT SizeY,INT InPosX, INT InPosY)
{
	check(0);
	return NULL;
}

//
//	UPS3Client::CloseViewport
//
void UPS3Client::CloseViewport(FViewport* Viewport)
{
	FPS3Viewport* PS3Viewport = (FPS3Viewport*)Viewport;
	PS3Viewport->Destroy();
}

/**
 * Retrieves the name of the key mapped to the specified character code.
 *
 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
 */
FName UPS3Client::GetVirtualKeyName( INT KeyCode ) const
{
	if ( KeyCode < 0xFFFF )
	{
		return FPS3Controller::GetVirtualKeyName((WORD)KeyCode);
	}

	return NAME_None;
}


/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsPS3Drv( INT& Lookup )
{
	UPS3AudioDevice::StaticClass();
	UPS3Client::StaticClass();
	UPS3ForceFeedbackManager::StaticClass();
}

/**
 * Auto generates names.
 */
void AutoRegisterNamesPS3Drv()
{
}



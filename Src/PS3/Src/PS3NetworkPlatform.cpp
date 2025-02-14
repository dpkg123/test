/*=============================================================================
	PS3NetworkPlatform.cpp: Implementation of classes used for basic NP support on PS3.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "PS3NetworkPlatform.h"
#include <cell/codec.h>
#include <sysutil/sysutil_gamecontent.h>

/** global instance */
FPS3NetworkPlatform* GPS3NetworkPlatform = NULL;



/**
 * Constructor
 */
FPS3NetworkPlatform::FPS3NetworkPlatform()
	: bIsInitialized(FALSE)
{
}


/**
 * Starts up Np and required services
 *
 * @return TRUE if successful
 */
UBOOL FPS3NetworkPlatform::Initialize()
{
	// if it's already running, then there's nothing to do
	if (bIsInitialized)
	{
		return TRUE;
	}

	// bring up Np
	INT Result = sceNpInit(NP_POOL_SIZE,NpPool);
	if (Result >= 0 || Result == SCE_NP_COMMUNITY_ERROR_ALREADY_INITIALIZED)
	{
		bIsInitialized = TRUE;

		// fill out the global Communication Id structure
		SceNpCommunicationId TempId = {
			{ PS3_COMMUNICATION_ID },
			'\0',
			0,
			0 };
		appMemcpy(&CommunicationId, &TempId, sizeof(SceNpCommunicationId));
	}
	else
	{
		debugf(NAME_Error, TEXT("Np failed to initialize with error %d"), Result);
	}

	return bIsInitialized;
}

/**
* Shutdown down Np, it can no longer be used until Initialize is called again
*/
void FPS3NetworkPlatform::Teardown()
{
	// if we are already shutdown, then there's nothing to do
	if (!bIsInitialized)
	{
		return;
	}
	
	// shutdown trophies
	TeardownTrophies();

	sceNpTerm();

	bIsInitialized = FALSE;
}


/*-----------------------------------------------------------------------------
	Trophies
-----------------------------------------------------------------------------*/

/** Struct to hold result information from a PNG decode operation */
struct FTrophyImageResult
{
	/** A buffer to hold the decoded data */
	void* DecodedBuffer;

	/** Image width */
	INT SizeX;

	/** Image height */
	INT SizeY;
};


/** Wrapper struct to give the sub thread a command */
struct FTrophyCommand
{
	enum ETrophyCommand
	{
		TC_UpdateState,		// Param of 0 is quick update, Param of 1 is complete update
		TC_UnlockTrophy,	// Param is which trophy to unlock
	};

	enum ETrophyUpdateType
	{
		TUT_Strings = 1,	// Read the strings from the trophy pack
		TUT_Images = 2,		// Read the images from the trophy pack and make textures
	};

	/** What type of command is this? */
	BYTE Command;

	/** The result of the operation */
	BYTE bSucceeded;

	/** Per-command parameter */
	WORD Param;

	/** Array for all images to decode into */
	FTrophyImageResult* ImageResults;
};


static int _NpTrophyCallback(SceNpTrophyContext Context, SceNpTrophyStatus Status, int Completed, int Total, void *This)
{
	//	debugf(TEXT("TrophyCallback CTX:0x%08x STAT:0x%08x Comp:%d Total:%d"), Context, Status, Completed, Total);
	return 0;
}

/** Runnable that queries the state of the trophies for a given player */
struct FTrophyUpdateInfoRunnable : public FRunnable
{
public:
	/**
	 * Sets the pointers to update when the job is complete
	 *
	 * @param InTrophyContext the trophy context to use 
	 * @param InTrophyHandle the trophy handle to use
	 * @param InTrophyInfo the trophy array to fill with details
	 * @param InReturnCode the result of the operation
	 */
	FTrophyUpdateInfoRunnable() :
		TrophyContext(GPS3NetworkPlatform->TrophyContext),
		TrophyHandle(GPS3NetworkPlatform->TrophyHandle),
		TrophyInfo(GPS3NetworkPlatform->TrophyInformation),
		KillFlag(0)
		{
		  // create an event for the thread, so its asleep unless needed
		  WakeupEvent = GSynchronizeFactory->CreateSynchEvent();
		}

	/** 
	 * Safely updates the unlocked state of the array
	 */
	void UpdateUnlockState()
	{
		// all commands will end with unlock state updating
		size_t TrophyCount;
		SceNpTrophyFlagArray TrophyUnlockedStatusArray;
		SCE_NP_TROPHY_FLAG_ZERO(&TrophyUnlockedStatusArray);
		INT Result = sceNpTrophyGetTrophyUnlockState(TrophyContext, TrophyHandle, &TrophyUnlockedStatusArray, &TrophyCount);

		// if it succeeded, update the global array
		if (Result == 0)
		{
			FScopeLock ScopeLock(&GPS3NetworkPlatform->GetTrophyCriticalSection());

			// get the flag for whether or not each trophy is unlocked
			for (INT TrophyIndex = 0; TrophyIndex < TrophyInfo.Num(); TrophyIndex++)
			{
				TrophyInfo(TrophyIndex).bWasAchievedOnline = SCE_NP_TROPHY_FLAG_ISSET(TrophyIndex, &TrophyUnlockedStatusArray) != 0;
			}
		}
		else
		{
			debugf(NAME_DevOnline, TEXT("sceNpTrophyGetTrophyUnlockState failed with code 0x%08x"), Result);
		}
	}

	/**
	 * Decompress an image from the given input .png data
	 *
	 * @param PNGBuffer Input png data
	 * @param PNGSize Size of PNGBuffer
	 * @param ImageResult [out] Holds the output information
	 *
	 */
	void DecodePNG(void* PNGBuffer, INT PNGSize, FTrophyImageResult& ImageResult)
	{
		// now we need to decode the PNG data
		CellPngDecSubHandle TaskHandle;

		// initialize the result
		ImageResult.DecodedBuffer = NULL;

		// create the task to decode the PNG
		CellPngDecSrc TaskSource;
		CellPngDecOpnInfo OpenInfo;
		TaskSource.srcSelect = CELL_PNGDEC_BUFFER;
		TaskSource.streamPtr = PNGBuffer;
		TaskSource.streamSize = PNGSize;
		TaskSource.spuThreadEnable = CELL_PNGDEC_SPU_THREAD_ENABLE;
		INT Result = cellPngDecOpen(PNGDecoder, &TaskHandle, &TaskSource, &OpenInfo);
		if (Result != 0)
		{
			cellPngDecClose(PNGDecoder, TaskHandle);
			debugf(NAME_DevOnline, TEXT("cellPngDecOpen faled with error 0x%08x"), Result);
			return;
		}

		// get info from the PNG data
		CellPngDecInfo PNGInfo;
		Result = cellPngDecReadHeader(PNGDecoder, TaskHandle, &PNGInfo);
		if (Result != 0)
		{
			cellPngDecClose(PNGDecoder, TaskHandle);
			debugf(NAME_DevOnline, TEXT("cellPngDecReadHeader faled with error 0x%08x"), Result);
			return;
		}

		// make sure that we are compatible with the code
		check(PNGInfo.numComponents == 4);
		check(PNGInfo.colorSpace == CELL_PNGDEC_RGBA || PNGInfo.colorSpace == CELL_PNGDEC_ARGB);

		// set decode parameters
		CellPngDecInParam InParam;
		CellPngDecOutParam OutParam;
		InParam.commandPtr = NULL;
		InParam.outputMode = CELL_PNGDEC_TOP_TO_BOTTOM;
		InParam.outputColorSpace = CELL_PNGDEC_RGBA;
		InParam.outputBitDepth = 8;
		InParam.outputPackFlag = CELL_PNGDEC_1BYTE_PER_1PIXEL;
		InParam.outputAlphaSelect = CELL_PNGDEC_STREAM_ALPHA;
		InParam.outputColorAlpha = 255;
		Result = cellPngDecSetParameter(PNGDecoder, TaskHandle, &InParam, &OutParam);
		if (Result != 0)
		{
			cellPngDecClose(PNGDecoder, TaskHandle);
			debugf(NAME_DevOnline, TEXT("cellPngDecSetParameter faled with error 0x%08x"), Result);
			return;
		}

		// allocate space for the data
		INT TexStride = (PNGInfo.imageWidth / GPixelFormats[PF_A8R8G8B8].BlockSizeY) * GPixelFormats[PF_A8R8G8B8].BlockBytes;
		BYTE* DecodedBuffer = (BYTE*)appMalloc(TexStride * PNGInfo.imageHeight);

		// decode the data!
		CellPngDecDataCtrlParam ControlParam;
		CellPngDecDataOutInfo OutInfo;
		ControlParam.outputBytesPerLine = TexStride;
		Result = cellPngDecDecodeData(PNGDecoder, TaskHandle, DecodedBuffer, &ControlParam, &OutInfo);
		if (Result != 0)
		{
			cellPngDecClose(PNGDecoder, TaskHandle);
			appFree(DecodedBuffer);
			debugf(NAME_DevOnline, TEXT("cellPngDecDecodeData faled with error 0x%08x"), Result);
			return;
		}

		// task is complete, close it
		cellPngDecClose(PNGDecoder, TaskHandle);

		// return the image
		ImageResult.DecodedBuffer = DecodedBuffer;
		ImageResult.SizeX = PNGInfo.imageWidth;
		ImageResult.SizeY = PNGInfo.imageHeight;
	}


	/** Wrappers for appMalloc/free used by PNG decoder */
	static void* PNGMalloc(uint32_t Size, void* User)
	{
		return appMalloc(Size);
	}
	static int32_t PNGFree(void* Ptr, void* User)
	{
		appFree(Ptr);
		return 0;
	}

	/**
	 * Initializes the trophy system
	 */
	virtual DWORD Run(void)
	{
		// see how much space we need for the trophy data (0 after it's been installed)
		QWORD RequiredTrophySize;
		sceNpTrophyGetRequiredDiskSpace(TrophyContext, TrophyHandle, &RequiredTrophySize, 0);

		// get amount of free space
		UINT SectorSize;
		QWORD UnusedSectors;
		cellFsGetFreeSize(SYS_DEV_HDD0, &SectorSize, &UnusedSectors);

		// if not enough space, tell user and quit
		if (SectorSize * UnusedSectors < RequiredTrophySize)
		{
			// show the user how many KB to free up
			cellGameContentErrorDialog(CELL_GAME_ERRDIALOG_NOSPACE_EXIT, (INT)((RequiredTrophySize - SectorSize * UnusedSectors) + 1023) / 1024, NULL);
			sceNpTrophyTerm();
			return 1;
		}

		// register the trophy .trp file
		// @todo ship: Use the second line here when shipping and you have a .trp file created. This will meet TRC, 
		// but we can't enable it all the time during development, no one would ever boot! 
		INT Result = sceNpTrophyRegisterContext(TrophyContext, TrophyHandle, _NpTrophyCallback, NULL, 0);
//		INT Result = sceNpTrophyRegisterContext(TrophyContext, TrophyHandle, _NpTrophyCallback, NULL, SCE_NP_TROPHY_OPTIONS_REGISTER_CONTEXT_SHOW_ERROR_EXIT);
		if (Result != 0)
		{
			sceNpTrophyTerm();
			debugf(NAME_DevOnline, TEXT("Failed to register Trophy context with error code 0x%08X, aborting trophies"),Result);
			return 1;
		}

		SceNpTrophyGameDetails GameDetails;
		sceNpTrophyGetGameInfo(TrophyContext, TrophyHandle, &GameDetails, NULL);

		// allocate space for our trophy information (game thread won't use the array until bIsTrophyInitialized is set below)
		TrophyInfo.AddZeroed(GameDetails.numTrophies);

		// get the unlock state of the trophies initially
		UpdateUnlockState();

		// Let the main thread know the results
		appInterlockedExchange(&GPS3NetworkPlatform->bIsTrophyInitialized, 1);

		// loop until stopped
		while (KillFlag == 0)
		{
			// do nothing unless requested
			WakeupEvent->Wait();

			// process all of the commands that exist
			UBOOL bHasMoreCommands = TRUE;
			while (bHasMoreCommands && !KillFlag)
			{
				// pull a command off the queue (protected by critical section)
				FTrophyCommand Command;
				{
					FScopeLock ScopeLock(&CommandQueueCriticalSection);
					// make sure there's something to do
					if (CommandQueue.Num() == 0)
					{
						bHasMoreCommands = FALSE;
						continue;
					}

					// copy off the first command
					Command = CommandQueue(0);
					CommandQueue.Remove(0);
				}

				// default to success
				Command.bSucceeded = TRUE;

				// process the command
				INT Result;
				switch (Command.Command)
				{
					case FTrophyCommand::TC_UpdateState:

						// create a PNG decoder if we'll be decoding PNGs
						if (Command.Param & FTrophyCommand::TUT_Images)
						{
							// load the prx into memory
							cellSysmoduleLoadModule( CELL_SYSMODULE_PNGDEC );

							CellPngDecThreadInParam InParam;
							CellPngDecThreadOutParam OutParam;
							CellPngDecExtThreadInParam InParamExt;
							CellPngDecExtThreadOutParam OutParamExt;

							// create the decoder
							InParam.spuThreadEnable = CELL_PNGDEC_SPU_THREAD_ENABLE;
							InParam.ppuThreadPriority = 3000;
							InParam.spuThreadPriority = 200;
							InParam.cbCtrlMallocFunc = PNGMalloc;
							InParam.cbCtrlMallocArg = NULL;
							InParam.cbCtrlFreeFunc = PNGFree;
							InParam.cbCtrlFreeArg = NULL;
							InParamExt.spurs = GSPURS;
							appMemset(InParamExt.priority, SPU_PRIO_PNG, sizeof(InParamExt.priority));
							InParamExt.maxContention = SPU_NUM_PNG;
							INT Result = cellPngDecExtCreate(&PNGDecoder, &InParam, &OutParam, &InParamExt, &OutParamExt);
							if (Result != 0)
							{
								debugf(NAME_DevOnline, TEXT("Failed to create PNG decoder with 0x%08x"), Result);
								// since we can't get images, remove the image portion of the command
								Command.Param &= ~FTrophyCommand::TUT_Images;
							}

							// allocate space to hold the decoded buffers
							Command.ImageResults = (FTrophyImageResult*)appMalloc(sizeof(FTrophyImageResult) * TrophyInfo.Num());
							appMemzero(Command.ImageResults, sizeof(FTrophyImageResult) * TrophyInfo.Num());
						}

						// do we want a full update?
						if (Command.Param & (FTrophyCommand::TUT_Images | FTrophyCommand::TUT_Strings))
						{
							// update each trophy one at a time
							for (INT TrophyIndex = 0; TrophyIndex < TrophyInfo.Num() && !KillFlag; TrophyIndex++)
							{
								FAchievementDetails& Details = TrophyInfo(TrophyIndex);

								// do we want to update the strings?
								if (Command.Param & (FTrophyCommand::TUT_Strings))
								{
									SceNpTrophyDetails TrophyDetails;

									Result = sceNpTrophyGetTrophyInfo(TrophyContext, TrophyHandle, TrophyIndex, &TrophyDetails, NULL);
									if (Result != 0)
									{
										debugf(NAME_DevOnline, TEXT("sceNpTrophyGetTrophyInfo failed with code 0x%08x"), Result);
										Command.bSucceeded = FALSE;
										break;
									}


									// cache the data
									Details.Id = TrophyDetails.trophyId;
									Details.AchievementName = UTF8_TO_TCHAR(TrophyDetails.name);
									Details.HowTo = Details.Description = UTF8_TO_TCHAR(TrophyDetails.description);
									Details.bIsSecret = TrophyDetails.hidden ? TRUE : FALSE;

									switch (TrophyDetails.trophyGrade)
									{
										case SCE_NP_TROPHY_GRADE_UNKNOWN:
										case SCE_NP_TROPHY_GRADE_PLATINUM:
											Details.GamerPoints = 0;
											break;

										case SCE_NP_TROPHY_GRADE_GOLD:
											Details.GamerPoints = 90;
											break;

										case SCE_NP_TROPHY_GRADE_SILVER:
											Details.GamerPoints = 30;
											break;

										case SCE_NP_TROPHY_GRADE_BRONZE:
											Details.GamerPoints = 15;
											break;
									}
								}

								// do we want to update the images?
								if (Command.Param & FTrophyCommand::TUT_Images)
								{
									// find out how much memory is needed to store the PNG data
									size_t SizeNeeded = 0;
									Result = sceNpTrophyGetTrophyIcon(TrophyContext, TrophyHandle, TrophyIndex, NULL, &SizeNeeded);

									// if the trophy is locked or hidden, there is no image to get return
									if (Result == SCE_NP_TROPHY_ERROR_LOCKED || Result == SCE_NP_TROPHY_ERROR_HIDDEN)
									{
										continue;
									}
									else if (Result != 0)
									{
										debugf(NAME_DevOnline, TEXT("Failed to get trophy icon size 0x%08x"), Result);
										continue;
									}

									// create room for the PNG and get it
									void* PNGBuffer = appMalloc(SizeNeeded);
									Result = sceNpTrophyGetTrophyIcon(TrophyContext, TrophyHandle, TrophyIndex, PNGBuffer, &SizeNeeded);
									if (Result != 0)
									{
										debugf(NAME_DevOnline, TEXT("Failed to get trophy icon 0x%08x"), Result);
										continue;
									}

									// create the texture!
									DecodePNG(PNGBuffer, SizeNeeded, Command.ImageResults[TrophyIndex]);

									// free temp memory
									appFree(PNGBuffer);
								}
							}
						}

						// destroy the PNG decoder
						if (Command.Param & FTrophyCommand::TUT_Images)
						{
							cellPngDecDestroy(PNGDecoder);
							cellSysmoduleUnloadModule( CELL_SYSMODULE_PNGDEC );
						}

						break;

					// unlock a trophy
					case FTrophyCommand::TC_UnlockTrophy:

						SceNpTrophyId PlatinumId;
						Result = sceNpTrophyUnlockTrophy(GPS3NetworkPlatform->TrophyContext, GPS3NetworkPlatform->TrophyHandle, Command.Param, &PlatinumId);
						debugf(NAME_DevOnline, TEXT("Unlocked trophy %d with result 0x%x"), Command.Param, Result);

						// @todo: should already unlocked be a success or a failure
						if (Result != 0)// && Result != SCE_NP_TROPHY_ERROR_ALREADY_UNLOCKED)
						{
							Command.bSucceeded = FALSE;
						}
						break;
				}

				// all commands end with updating trophy unlocked state
				UpdateUnlockState();

				debugf(NAME_DevOnline, TEXT("Completed trophy command [%d / %d]"), Command.Command, Command.Param);

				{
					// push the command on the result queue, so we now it's done
					FScopeLock ScopeLock(&ResultQueueCriticalSection);
					ResultQueue.AddItem(Command);
				}
			}
		}

		return 0;
	}

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop(void)
	{
		// tell the thread to stop
		appInterlockedIncrement(&KillFlag);

		// make sure the thread is awake to quit out
		StartUpdateInfo(FALSE, FALSE);
	}

	/**
	 * Tells the thread to start getting trophy info
	 *
	 * @param bShouldReadStrings TRUE if the thread should do a full string read
	 * @param bShouldReadImages TRUE if the thread should do a full image read
	 */
	void StartUpdateInfo(UBOOL bShouldReadStrings, UBOOL bShouldReadImages)
	{
		FScopeLock ScopeLock(&CommandQueueCriticalSection);

		// enqueue the command
		FTrophyCommand Command;
		Command.Command = FTrophyCommand::TC_UpdateState;
		
		// set up what to read
		Command.Param = 0;
		if (bShouldReadStrings)
		{
			Command.Param |= FTrophyCommand::TUT_Strings;
		}
		if (bShouldReadImages)
		{
			Command.Param |= FTrophyCommand::TUT_Images;
		}

		Command.bSucceeded = FALSE;
		Command.ImageResults = NULL;
		CommandQueue.AddItem(Command);

		// trigger the thread to wake up and get trophy info
		WakeupEvent->Trigger();
	}

	/**
	 * Tells the thread to unlock a trophy
	 *
	 * @param TrophyIndex Which trophy to unlock
	 */
	void UnlockTrophy(INT TrophyIndex)
	{
		FScopeLock ScopeLock(&CommandQueueCriticalSection);

		// enqueue the command
		FTrophyCommand Command;
		Command.Command = FTrophyCommand::TC_UnlockTrophy;
		Command.Param = TrophyIndex;
		Command.bSucceeded = FALSE;
		Command.ImageResults = NULL;
		CommandQueue.AddItem(Command);

		// tell the thread to wake up
		WakeupEvent->Trigger();
	}

	/**
	* Ignored
	*/
	virtual UBOOL Init()
	{
		return TRUE;
	}
	virtual void Exit()
	{
	}



// public members for ease of communication with main thread
public:

	/** Critical section for protecting the command queue */
	FCriticalSection CommandQueueCriticalSection;

	/** Critical section to protect the ResultQueue */
	FCriticalSection ResultQueueCriticalSection;

	/** Command queue to give the thread commands */
	TArray<FTrophyCommand> CommandQueue;

	/** Queue to send completion information back to the main thread */
	TArray<FTrophyCommand> ResultQueue;

protected:
	/** Trophy context handle pointer **/
	SceNpTrophyContext TrophyContext;
	/** Trophy system handle **/
	SceNpTrophyHandle TrophyHandle;

	/** Trophy details array */
	TArray<FAchievementDetails>& TrophyInfo;

	/** Incremented when external thread wants to stop this thread (shutting down trophies, etc) */
	INT KillFlag;

	/** Event to tell the thread to perform work */
	FEvent* WakeupEvent;

	/** PNG Decoder object */
	CellPngDecMainHandle PNGDecoder;
};


/**
 * Starts up the trophy system
 *
 * @return TRUE if successful
 */
UBOOL FPS3NetworkPlatform::InitializeTrophies()
{
	bIsTrophyInitialized = FALSE;
	bHasCompletedStringsRead = FALSE;
	bIsReadingState = FALSE;
	TrophyInfoRunnable = NULL;
	TrophyInfoThread = NULL;

	debugf(NAME_DevOnline, TEXT("Starting up trophy system!"));

	// fill out the communication signature (as received from Sony)
	const SceNpCommunicationSignature TempSignature = 
	{
		{ PS3_COMMUNICATION_SIG }
	};
	appMemcpy(&CommSignature, &TempSignature, sizeof(SceNpCommunicationSignature));


	INT Result = sceNpTrophyInit(NULL, 0, SYS_MEMORY_CONTAINER_ID_INVALID, 0);
	if (Result == 0)
	{
		Result = sceNpTrophyCreateContext(&TrophyContext, &CommunicationId, &CommSignature, 0);

		if (Result == 0)
		{
			Result = sceNpTrophyCreateHandle(&TrophyHandle);
			if (Result == 0)
			{
				// start up trophy info query thread
				TrophyInfoRunnable = new FTrophyUpdateInfoRunnable();
				TrophyInfoThread = GThreadFactory->CreateThread(TrophyInfoRunnable, TEXT("TrophyInfo"), FALSE, FALSE, 16 * 1024, TPri_BelowNormal);
			}
		}

		if (Result != 0)
		{
			debugf(TEXT("Failed to intialize trophy system with error code 0x%08X"), Result);
			TeardownTrophies();

			return FALSE;
		}
	}
	else
	{
		debugf(TEXT("Failed to init NP Trophy with error code 0x%08X"),Result);
		return FALSE;
	}

	return TRUE;
}

/**
 * Shuts down trophy support
 */
void FPS3NetworkPlatform::TeardownTrophies()
{
	debugf(NAME_DevOnline, TEXT("Tearing down trophy system"));

	// make sure that the info thread is complete
	if (TrophyInfoRunnable)
	{
		TrophyInfoRunnable->Stop();
	}
	if (TrophyInfoThread)
	{
		TrophyInfoThread->WaitForCompletion();
	}

	INT Return;
	if (TrophyHandle != SCE_NP_TROPHY_INVALID_HANDLE)
	{
		Return = sceNpTrophyDestroyHandle(TrophyHandle);
		TrophyHandle = SCE_NP_TROPHY_INVALID_HANDLE;
		if (Return != 0) 
		{
			debugf(TEXT("sceNpTrophyDestroyHandle() failed. ret = 0x%08x"), Return);
		}
	}

	if (TrophyContext != SCE_NP_TROPHY_INVALID_CONTEXT)
	{
		Return = sceNpTrophyDestroyContext(TrophyContext);
		TrophyContext = SCE_NP_TROPHY_INVALID_CONTEXT;
		if (Return != 0)
		{
			debugf(TEXT("sceNpTrophyDestroyContext() failed. ret = 0x%08x"), Return);
		}
	}

	sceNpTrophyTerm();
	bIsTrophyInitialized = FALSE;
}

/**
 * Unlocks the given trophy index
 *
 * @param TrophyId Id of the trophy (must match what's in the trophy pack)
 *
 * @return TRUE if successful 
 */
UBOOL FPS3NetworkPlatform::UnlockTrophy(INT TrophyId)
{
	debugf(NAME_DevOnline, TEXT("Unlocking trophy %d"), TrophyId);

	// unlock it on the thread
	TrophyInfoRunnable->UnlockTrophy(TrophyId);

	return TRUE;
}

/**
 * Returns whether or not the trophy was previously unlocked in any session
 *
 * @param TrophyId Id of the trophy (must match what's in the trophy pack)
 *
 * @return TRUE if the trophy has already been unlocked
 */
UBOOL FPS3NetworkPlatform::IsTrophyUnlocked(INT TrophyId)
{
	if (!bIsTrophyInitialized)
	{
		debugf(NAME_DevOnline,TEXT("Trophy system not initialized"));
		return FALSE;
	}

	// make sure we aren't updating the value at the moment
	FScopeLock ScopeLock(&TrophyUpdateCriticalSection);

	// get the cached value
	return TrophyInformation(TrophyId).bWasAchievedOnline;
}

/**
 * Makes sure the trophy information is ready for GetTrophyInformation
 * 
 * @param bShouldUpdateStrings If TRUE, string data will be updated
 * @param bShouldUpdateImages If TRUE, image data will be updated
 *
 * @return TRUE if successful
 */
UBOOL FPS3NetworkPlatform::UpdateTrophyInformation(UBOOL bShouldUpdateStrings, UBOOL bShouldUpdateImages)
{
// @todo debug
// bShouldUpdateImages = TRUE;

	// make sure we can get trophy data
	if (!bIsTrophyInitialized)
	{
		debugf(NAME_DevOnline,TEXT("FPS3NetworkPlatform::UpdateTrophyInformation(): Trophy system not initialized"));
		return FALSE;
	}

	// don't double book the read
	if (bIsReadingState)
	{
		return TRUE;
	}

	// mark that we are now getting trophy info
	bIsReadingState = TRUE;

	// figure out what slow reads we need to do, if any
	UBOOL bShouldReadStrings = bShouldUpdateStrings && !bHasCompletedStringsRead;
	// we always read images because they can change depending on unlocked state and could be garbage collected
	UBOOL bShouldReadImages = bShouldUpdateImages;

	debugf(NAME_DevOnline, TEXT("Starting trophy update, slow string read? %d Slow image read? %d"), bShouldReadStrings, bShouldReadImages);

	// tell the runnable to wake up and work on full read
	TrophyInfoRunnable->StartUpdateInfo(bShouldReadStrings, bShouldReadImages);

	return TRUE;
}

/**
 * @return a flag indicating if a task was completed, and if so, which one
 */
ETrophyTaskComplete FPS3NetworkPlatform::GetTrophyTaskState(UBOOL &bSucceeded)
{
	FScopeLock ScopeLock(&TrophyInfoRunnable->ResultQueueCriticalSection);
	
	// if there was nothing completed, just return nothing
	if (TrophyInfoRunnable->ResultQueue.Num() == 0)
	{
		return TTC_None;
	}

	// otherwise, pull the first one off, and return it's info
	FTrophyCommand Command = TrophyInfoRunnable->ResultQueue(0);
	TrophyInfoRunnable->ResultQueue.Remove(0);

	// return success variable
	bSucceeded = Command.bSucceeded;

	// clear out reading state flag
	if (Command.Command == FTrophyCommand::TC_UpdateState)
	{
		bIsReadingState = FALSE;

		// if we successfully read in strings/images, note that so we don't do it again
		if (Command.bSucceeded && Command.Param & FTrophyCommand::TUT_Strings)
		{
			bHasCompletedStringsRead = TRUE;
		}
	}

	// if we finished an image query, and there is a buffer, turn the images into UTexture2D (have to happen on main thread)
	// @todo: Create the UTexture2Ds over multiple frames, as they come back (will take some synchronization, etc)
	if (Command.Command == FTrophyCommand::TC_UpdateState && Command.ImageResults != NULL)
	{
		for (INT TrophyIndex = 0; TrophyIndex < TrophyInformation.Num(); TrophyIndex++)
		{
			// get in/out objects
			FTrophyImageResult& Result = Command.ImageResults[TrophyIndex];
			FAchievementDetails& Details = TrophyInformation(TrophyIndex);

			// only make an image if we decoded something
			if (Result.DecodedBuffer == NULL)
			{
				Details.Image = NULL;
				continue;
			}
			// now create the target object to decode into
			FString ImageName = FString::Printf(TEXT("AchievementIcon_%d"), TrophyIndex);
			UTexture2D* NewTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), INVALID_OBJECT, FName(*ImageName));
			// disable compression, tiling, sRGB, and streaming for the texture
			NewTexture->CompressionNone			= TRUE;
			NewTexture->CompressionSettings		= TC_Default;
			NewTexture->MipGenSettings			= TMGS_NoMipmaps;
			NewTexture->CompressionNoAlpha		= TRUE;
			NewTexture->DeferCompression		= FALSE;
			NewTexture->bNoTiling				= TRUE;
			NewTexture->SRGB					= FALSE;
			NewTexture->NeverStream				= TRUE;
			NewTexture->LODGroup				= TEXTUREGROUP_UI;	

			// all PS3 trophy icons are 240x240
			// if we support the game icon, we will need to change this
			NewTexture->Init(Result.SizeX, Result.SizeY, PF_A8R8G8B8);

			// Only the first mip level is used
			check(NewTexture->Mips.Num() > 0);

			// Mip 0 is locked for the duration of the read request
			BYTE* MipData = (BYTE*)NewTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
			if (MipData)
			{
				// copy from thread-made data to texture
				INT TexStride = (Result.SizeX / GPixelFormats[PF_A8R8G8B8].BlockSizeY) * GPixelFormats[PF_A8R8G8B8].BlockBytes;
				appMemcpy(MipData, Result.DecodedBuffer, TexStride * Result.SizeY);
			}

			NewTexture->Mips(0).Data.Unlock();
			NewTexture->UpdateResource();				

			// set the image into the achievements array
			Details.Image = NewTexture;

			// free the image buffer
			appFree(Result.DecodedBuffer);
		}

		// free the results buffer
		appFree(Command.ImageResults);

		debugf(NAME_DevOnline, TEXT("Converted decoded images to UTexture2Ds"));
	}
	// return what we just finished
	return Command.Command == FTrophyCommand::TC_UpdateState ? TTC_Update : TTC_Unlock;
}

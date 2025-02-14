/*=============================================================================
	PS3NetworkPlatform.h: Definitions of classes used for basic NP support on PS3.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3NETWORKPLATFORM_H__
#define __PS3NETWORKPLATFORM_H__

// NP headers
#include <np.h>
#include <np/trophy.h>

// various needed IDs
#include "PS3IDs.h"

enum ETrophyTaskComplete
{
	TTC_None,		// nothing was completed
	TTC_Update,		// an update was completed
	TTC_Unlock,		// a trophy unlock was completed
};

// Min size needed by NP
#define NP_POOL_SIZE (128 * 1024)

/**
 * Wrapper class around sceNp functionality required by all games (even if non-MP)	
 */
class FPS3NetworkPlatform
{
public:
	/**
	 * Constructor
	 */
	FPS3NetworkPlatform();

	/**
	 * Starts up Np and required services
	 *
	 * @return TRUE if successful
	 */
	UBOOL Initialize();

	/**
	 * Shutdown down Np, it can no longer be used until Initialize is called again
	 */
	void Teardown();

	/**
	 * Starts up the trophy system
	 *
	 * @return TRUE if successful
	 */
	UBOOL InitializeTrophies();

	/**
	 * Shuts down trophy support
	 */
	void TeardownTrophies();

	/**
	 * @return TRUE if Np is up and running (it can be taken down while the game is running)
	 */
	UBOOL IsInitialized()
	{
		return bIsInitialized;
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
		// no need to Tick if we aren't initialized
		return bIsInitialized;
	}

	/**
	 * @return the single Communication Id for this title
	 */
	const SceNpCommunicationId& GetCommunicationId()
	{
		return CommunicationId;
	}

	/**
	 * Unlocks the given trophy index
	 *
	 * @param TrophyId Id of the trophy (must match what's in the trophy pack)
	 *
	 * @return TRUE if successful 
	 */
	UBOOL UnlockTrophy(INT TrophyId);

	/**
	 * Returns whether or not the trophy was previously unlocked in any session
	 *
	 * @param TrophyId Id of the trophy (must match what's in the trophy pack)
	 *
	 * @return TRUE if the trophy has already been unlocked
	 */
	UBOOL IsTrophyUnlocked(INT TrophyId);

	/**
	 * Makes sure the trophy information is ready for GetTrophyInformation
	 * 
	 * @param bShouldUpdateStrings If TRUE, string data will be updated
	 * @param bShouldUpdateImages If TRUE, image data will be updated
	 *
	 * @return TRUE if successful
	 */
	UBOOL UpdateTrophyInformation(UBOOL bShouldUpdateStrings, UBOOL bShouldUpdateImages);

	/**
	 * @return a flag indicating if a task was completed, and if so, which one
	 */
	ETrophyTaskComplete GetTrophyTaskState(UBOOL& bSucceeded);

	/**
	 * @return the list of all achievement information
	 */
	TArray<FAchievementDetails>& GetTrophyInformation()
	{
		return TrophyInformation;
	}

	/**
	 * @return Critical section so we don't have to threads getting/setting the unlock state at the same time 
	 */
	FCriticalSection& GetTrophyCriticalSection()
	{
		return TrophyUpdateCriticalSection;
	}

private:

	/** Is Np up and running? */
	UBOOL bIsInitialized;

	/** Memory used by NP */
	BYTE NpPool[NP_POOL_SIZE];

	/** Are trophies initialized (INT for appInterlocked) */
	INT bIsTrophyInitialized;

	/** Have we ever done the slow read of all trophy string data */
	UBOOL bHasCompletedStringsRead;

	/** The communication id to pass around */
	SceNpCommunicationId CommunicationId;

	/** Trophy system context */
	SceNpTrophyContext TrophyContext;

	/** Trophy system handle */
	SceNpTrophyHandle TrophyHandle;

	/** Trophy signature */
	SceNpCommunicationSignature CommSignature;

	/** State of reading the trophy details */
	UBOOL bIsReadingState;

	/** Cached trophy details (name, description, etc) */
	TArray<FAchievementDetails> TrophyInformation;

	/** Thread code to get trophy info, too slow for Async Workd */
	struct FTrophyUpdateInfoRunnable* TrophyInfoRunnable;

	/** Thread for the TrophyInfoRunnable */
	FRunnableThread* TrophyInfoThread;

	/** Critical section so we don't have to threads getting/setting the unlock state at the same time */
	FCriticalSection TrophyUpdateCriticalSection;

	friend struct FAsyncTrophyInit;
	friend struct FTrophyUpdateInfoRunnable;
};

/** global instance */
extern FPS3NetworkPlatform* GPS3NetworkPlatform;


#endif
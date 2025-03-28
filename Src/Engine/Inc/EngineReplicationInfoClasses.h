/*===========================================================================
    C++ class definitions exported from UnrealScript.
    This is automatically generated by the tools.
    DO NOT modify this manually! Edit the corresponding .uc files instead!
    Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
===========================================================================*/
#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

#include "EngineNames.h"

// Split enums from the rest of the header so they can be included earlier
// than the rest of the header file by including this file twice with different
// #define wrappers. See Engine.h and look at EngineClasses.h for an example.
#if !NO_ENUMS && !defined(NAMES_ONLY)

#ifndef INCLUDED_ENGINE_REPLICATIONINFO_ENUMS
#define INCLUDED_ENGINE_REPLICATIONINFO_ENUMS 1


#endif // !INCLUDED_ENGINE_REPLICATIONINFO_ENUMS
#endif // !NO_ENUMS

#if !ENUMS_ONLY

#ifndef NAMES_ONLY
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

#ifndef INCLUDED_ENGINE_REPLICATIONINFO_CLASSES
#define INCLUDED_ENGINE_REPLICATIONINFO_CLASSES 1
#define ENABLE_DECLARECLASS_MACRO 1
#include "UnObjBas.h"
#undef ENABLE_DECLARECLASS_MACRO

class AReplicationInfo : public AInfo
{
public:
    //## BEGIN PROPS ReplicationInfo
    //## END PROPS ReplicationInfo

    DECLARE_ABSTRACT_CLASS(AReplicationInfo,AInfo,0,Engine)
	INT* GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
};

struct GameReplicationInfo_eventShouldShowGore_Parms
{
    UBOOL ReturnValue;
    GameReplicationInfo_eventShouldShowGore_Parms(EEventParm)
    {
    }
};
class AGameReplicationInfo : public AReplicationInfo
{
public:
    //## BEGIN PROPS GameReplicationInfo
    class UClass* GameClass;
    BITFIELD bStopCountDown:1;
    BITFIELD bMatchHasBegun:1;
    BITFIELD bMatchIsOver:1;
    INT RemainingTime;
    INT ElapsedTime;
    INT RemainingMinute;
    INT GoalScore;
    INT TimeLimit;
    TArrayNoInit<class ATeamInfo*> Teams;
    FStringNoInit ServerName;
    class AActor* Winner;
    TArrayNoInit<class APlayerReplicationInfo*> PRIArray;
    TArrayNoInit<class APlayerReplicationInfo*> InactivePRIArray;
    //## END PROPS GameReplicationInfo

    virtual UBOOL OnSameTeam(class AActor* A,class AActor* B);
    DECLARE_FUNCTION(execOnSameTeam)
    {
        P_GET_OBJECT(AActor,A);
        P_GET_OBJECT(AActor,B);
        P_FINISH;
        *(UBOOL*)Result=this->OnSameTeam(A,B);
    }
    UBOOL eventShouldShowGore()
    {
        GameReplicationInfo_eventShouldShowGore_Parms Parms(EC_EventParm);
        Parms.ReturnValue=FALSE;
        ProcessEvent(FindFunctionChecked(ENGINE_ShouldShowGore),&Parms);
        return Parms.ReturnValue;
    }
    DECLARE_CLASS(AGameReplicationInfo,AReplicationInfo,0|CLASS_Config|CLASS_NativeReplication,Engine)
    static const TCHAR* StaticConfigName() {return TEXT("Game");}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	
	/**
	 * Builds a list of components that are hidden for scene capture
	 *
	 * @param HiddenComponents the list to add to/remove from
	 */
	virtual void UpdateHiddenComponentsForSceneCapture(TSet<UPrimitiveComponent*>& HiddenComponents) {}

	/**
	 * Helper to return the default object of the GameInfo class corresponding to this GRI
	 */
	AGameInfo *GetDefaultGameInfo();
};

struct FAutomatedTestingDatum
{
    INT NumberOfMatchesPlayed;
    INT NumMapListCyclesDone;

    /** Constructors */
    FAutomatedTestingDatum() {}
    FAutomatedTestingDatum(EEventParm)
    {
        appMemzero(this, sizeof(FAutomatedTestingDatum));
    }
};

struct PlayerReplicationInfo_eventSetPlayerName_Parms
{
    FString S;
    PlayerReplicationInfo_eventSetPlayerName_Parms(EEventParm)
    {
    }
};
class APlayerReplicationInfo : public AReplicationInfo
{
public:
    //## BEGIN PROPS PlayerReplicationInfo
    FLOAT Score;
    INT Deaths;
    BYTE Ping;
    BYTE TTSSpeaker;
    INT NumLives;
    FStringNoInit PlayerName;
    FStringNoInit OldName;
    INT PlayerID;
    class ATeamInfo* Team;
    BITFIELD bAdmin:1;
    BITFIELD bIsSpectator:1;
    BITFIELD bOnlySpectator:1;
    BITFIELD bWaitingPlayer:1;
    BITFIELD bReadyToPlay:1;
    BITFIELD bOutOfLives:1;
    BITFIELD bBot:1;
    BITFIELD bHasBeenWelcomed:1;
    BITFIELD bIsInactive:1;
    BITFIELD bFromPreviousLevel:1;
    INT StartTime;
    FStringNoInit StringSpectating;
    FStringNoInit StringUnknown;
    INT Kills;
    class UClass* GameMessageClass;
    FLOAT ExactPing;
    FStringNoInit SavedNetworkAddress;
    struct FUniqueNetId UniqueId;
    FName SessionName;
    struct FAutomatedTestingDatum AutomatedTestingData;
    INT StatConnectionCounts;
    INT StatPingTotals;
    INT StatPingMin;
    INT StatPingMax;
    INT StatPKLTotal;
    INT StatPKLMin;
    INT StatPKLMax;
    INT StatMaxInBPS;
    INT StatAvgInBPS;
    INT StatMaxOutBPS;
    INT StatAvgOutBPS;
    class UTexture2D* Avatar;
    //## END PROPS PlayerReplicationInfo

    void UpdatePing(FLOAT TimeStamp);
    virtual BYTE GetTeamNum();
    DECLARE_FUNCTION(execUpdatePing)
    {
        P_GET_FLOAT(TimeStamp);
        P_FINISH;
        this->UpdatePing(TimeStamp);
    }
    void eventSetPlayerName(const FString& S)
    {
        PlayerReplicationInfo_eventSetPlayerName_Parms Parms(EC_EventParm);
        Parms.S=S;
        ProcessEvent(FindFunctionChecked(ENGINE_SetPlayerName),&Parms);
    }
    DECLARE_CLASS(APlayerReplicationInfo,AReplicationInfo,0|CLASS_NativeReplication,Engine)
	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
};

class ATeamInfo : public AReplicationInfo
{
public:
    //## BEGIN PROPS TeamInfo
    FStringNoInit TeamName;
    INT Size;
    FLOAT Score;
    INT TeamIndex;
    FColor TeamColor;
    //## END PROPS TeamInfo

    virtual BYTE GetTeamNum();
    DECLARE_CLASS(ATeamInfo,AReplicationInfo,0|CLASS_NativeReplication,Engine)
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
};

#undef DECLARE_CLASS
#undef DECLARE_CASTED_CLASS
#undef DECLARE_ABSTRACT_CLASS
#undef DECLARE_ABSTRACT_CASTED_CLASS
#endif // !INCLUDED_ENGINE_REPLICATIONINFO_CLASSES
#endif // !NAMES_ONLY

AUTOGENERATE_FUNCTION(AGameReplicationInfo,-1,execOnSameTeam);
AUTOGENERATE_FUNCTION(APlayerReplicationInfo,-1,execGetTeamNum);
AUTOGENERATE_FUNCTION(APlayerReplicationInfo,-1,execUpdatePing);
AUTOGENERATE_FUNCTION(ATeamInfo,-1,execGetTeamNum);

#ifndef NAMES_ONLY
#undef AUTOGENERATE_FUNCTION
#endif

#ifdef STATIC_LINKING_MOJO
#ifndef ENGINE_REPLICATIONINFO_NATIVE_DEFS
#define ENGINE_REPLICATIONINFO_NATIVE_DEFS

#define AUTO_INITIALIZE_REGISTRANTS_ENGINE_REPLICATIONINFO \
	AReplicationInfo::StaticClass(); \
	AGameReplicationInfo::StaticClass(); \
	GNativeLookupFuncs.Set(FName("GameReplicationInfo"), GEngineAGameReplicationInfoNatives); \
	APlayerReplicationInfo::StaticClass(); \
	GNativeLookupFuncs.Set(FName("PlayerReplicationInfo"), GEngineAPlayerReplicationInfoNatives); \
	ATeamInfo::StaticClass(); \
	GNativeLookupFuncs.Set(FName("TeamInfo"), GEngineATeamInfoNatives); \

#endif // ENGINE_REPLICATIONINFO_NATIVE_DEFS

#ifdef NATIVES_ONLY
FNativeFunctionLookup GEngineAGameReplicationInfoNatives[] = 
{ 
	MAP_NATIVE(AGameReplicationInfo, execOnSameTeam)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineAPlayerReplicationInfoNatives[] = 
{ 
	MAP_NATIVE(APlayerReplicationInfo, execGetTeamNum)
	MAP_NATIVE(APlayerReplicationInfo, execUpdatePing)
	{NULL, NULL}
};

FNativeFunctionLookup GEngineATeamInfoNatives[] = 
{ 
	MAP_NATIVE(ATeamInfo, execGetTeamNum)
	{NULL, NULL}
};

#endif // NATIVES_ONLY
#endif // STATIC_LINKING_MOJO

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_SIZE_NODIE(AReplicationInfo)
VERIFY_CLASS_OFFSET_NODIE(AGameReplicationInfo,GameReplicationInfo,GameClass)
VERIFY_CLASS_OFFSET_NODIE(AGameReplicationInfo,GameReplicationInfo,InactivePRIArray)
VERIFY_CLASS_SIZE_NODIE(AGameReplicationInfo)
VERIFY_CLASS_OFFSET_NODIE(APlayerReplicationInfo,PlayerReplicationInfo,Score)
VERIFY_CLASS_OFFSET_NODIE(APlayerReplicationInfo,PlayerReplicationInfo,Avatar)
VERIFY_CLASS_SIZE_NODIE(APlayerReplicationInfo)
VERIFY_CLASS_OFFSET_NODIE(ATeamInfo,TeamInfo,TeamName)
VERIFY_CLASS_OFFSET_NODIE(ATeamInfo,TeamInfo,TeamColor)
VERIFY_CLASS_SIZE_NODIE(ATeamInfo)
#endif // VERIFY_CLASS_SIZES
#endif // !ENUMS_ONLY

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

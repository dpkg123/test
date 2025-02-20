
/*=============================================================================
UnPenLev.cpp: Unreal pending level class.
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "DemoRecording.h"
#include "NetworkProfiler.h"

/*-----------------------------------------------------------------------------
UPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UPendingLevel::UPendingLevel( const FURL& InURL )
	: ULevelBase( InURL )
	, NetDriver(NULL)
	, DemoRecDriver(NULL)
	, PeerNetDriver(NULL)
	, bSuccessfullyConnected(FALSE)
	, bSentJoinRequest(FALSE)
	, FilesNeeded(0)
{}
IMPLEMENT_CLASS(UPendingLevel);


void UPendingLevel::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << NetDriver << DemoRecDriver << PeerNetDriver;
	}
}

/*-----------------------------------------------------------------------------
UNetPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UNetPendingLevel::UNetPendingLevel( const FURL& InURL )
: UPendingLevel( InURL )
{
	if (!GDisallowNetworkTravel)
	{
		NETWORK_PROFILER(GNetworkProfiler.TrackSessionChange(TRUE,InURL));

		// Try to create network driver.
		NetDriver = GEngine->ConstructNetDriver();
		check(NetDriver);

		if( NetDriver->InitConnect( this, URL, ConnectionError ) )
		{
			// Try to create the peer network driver if peer to peer connections are allowed
			if (NetDriver->AllowPeerConnections)
			{
				PeerNetDriver = GEngine->ConstructNetDriver();
				if (PeerNetDriver != NULL)
				{
					PeerNetDriver->bIsPeer = TRUE;
					// Create the peer net driver and start listening for new connections
					InitPeerListen();
				}
			}

			// Send initial message.
			BYTE PlatformType = BYTE(appGetPlatformType());
#if WITH_STEAMWORKS
			if( GSteamworksInitialized )
			{
				// Steam requires client authentication before the connection request can be processed
				FNetControlMessage<NMT_AuthHello>::Send(NetDriver->ServerConnection, PlatformType, GEngineMinNetVersion, GEngineVersion);
			}
			else
#endif // WITH_STEAMWORKS
			{
#if UDK
				FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, PlatformType, GEngineMinNetVersion, GEngineVersion, GModGUID);
#else
				FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, PlatformType, GEngineMinNetVersion, GEngineVersion);
#endif // UDK
			}
			NetDriver->ServerConnection->FlushNet();

			// cache the guid caches
			for (TObjectIterator<UGuidCache> It; It; ++It)
			{
				CachedGuidCaches.AddItem(*It);
			}
		}
		else
		{
			// error initializing the network stack...
			NetDriver = NULL;

			// ConnectionError should be set by calling InitConnect...however, if we set NetDriver to NULL without setting a
			// value for ConnectionError, we'll trigger the assertion at the top of UNetPendingLevel::Tick() so make sure it's set
			if ( ConnectionError.Len() == 0 )
			{
				ConnectionError = LocalizeError(TEXT("NetworkInit"), TEXT("Engine"));
			}
		}
	}
	else
	{
		ConnectionError = LocalizeError(TEXT("UsedCheatCommands"), TEXT("Engine"));
	}
}

//
// FNetworkNotify interface.
//
EAcceptConnection UNetPendingLevel::NotifyAcceptingConnection()
{
	return ACCEPTC_Reject; 
}
void UNetPendingLevel::NotifyAcceptedConnection( class UNetConnection* Connection )
{
}
UBOOL UNetPendingLevel::NotifyAcceptingChannel( class UChannel* Channel )
{
	return 0;
}
UWorld* UNetPendingLevel::NotifyGetWorld()
{
	return NULL;
}
void UNetPendingLevel::NotifyControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch)
{
	check(Connection==NetDriver->ServerConnection);

#if !SHIPPING_PC_GAME
	debugf(NAME_DevNet, TEXT("PendingLevel received: %s"), FNetControlMessageInfo::GetName(MessageType));
#endif

	// This client got a response from the server.
	switch (MessageType)
	{
		case NMT_Upgrade:
			// Report mismatch.
			INT RemoteMinVer, RemoteVer;
			FNetControlMessage<NMT_Upgrade>::Receive(Bunch, RemoteMinVer, RemoteVer);
			if (GEngineVersion < RemoteMinVer)
			{
				ConnectionError = LocalizeError(TEXT("ClientOutdated"),TEXT("Engine"));
				// Upgrade message.
				GEngine->SetProgress(PMT_ConnectionFailure, LocalizeError(TEXT("ConnectionFailed_Title"), TEXT("Engine")), ConnectionError);
			}
			else
			{
				ConnectionError = LocalizeError(TEXT("ServerOutdated"),TEXT("Engine"));
				// Downgrade message.
				GEngine->SetProgress(PMT_ConnectionFailure, LocalizeError(TEXT("ConnectionFailed_Title"), TEXT("Engine")), ConnectionError);
			}
			break;

#if WITH_STEAMWORKS
		// NOTE: If you update this code, update the code for this in UnWorld.cpp too!
		case NMT_AuthServer:
			{
				QWORD ServerSteamID = 0;
				BYTE bServerAntiCheatEnabled = 0;
				FNetControlMessage<NMT_AuthServer>::Receive(Bunch, ServerSteamID, bServerAntiCheatEnabled);

				if (ServerSteamID != 0)  // if server isn't running Steam (id==0), just go ahead to NMT_Hello.
				{
					// get signed blob for steam auth
					BYTE AuthBlob[2048];
					INT BlobLen = 0;

					// this is currently only used by the Steamworks support, but could be of use to other platforms, so it's generalized into the protocol.
					extern INT appSteamInitiateGameConnection(void *AuthBlob, INT BlobSize, QWORD ServerSteamID, DWORD IPAddr, INT Port, UBOOL ServerVACEnabled);
					BlobLen = appSteamInitiateGameConnection( (void*)&AuthBlob, ARRAY_COUNT(AuthBlob), ServerSteamID, Connection->GetAddrAsInt(), Connection->GetAddrPort(), bServerAntiCheatEnabled != 0 );

					// break up the blob into chunks small enough to fit in a packet
					const INT MaxSubBlobSize = (NetDriver->ServerConnection->MaxPacket - 32) / 4;
					BYTE NumSubBlobs = (BYTE) ((BlobLen + (MaxSubBlobSize - 1)) / MaxSubBlobSize);

					INT Offset = 0;
					for (BYTE SubBlob = 0; SubBlob < NumSubBlobs; SubBlob++)
					{
						// calc size of the next one to go
						const INT SubBlobSize = Min(MaxSubBlobSize, BlobLen - Offset);

						// send it
						FString AuthBlobString = appBlobToString(AuthBlob + Offset, SubBlobSize);
						FNetControlMessage<NMT_AuthBlob>::Send(NetDriver->ServerConnection, SubBlob, NumSubBlobs, AuthBlobString);
						NetDriver->ServerConnection->FlushNet();

						// move up in the world
						Offset += SubBlobSize;
					}
				}

				BYTE PlatformType = BYTE(appGetPlatformType());
#if UDK
				FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, PlatformType, GEngineMinNetVersion, GEngineVersion, GModGUID);
#else
				FNetControlMessage<NMT_Hello>::Send(NetDriver->ServerConnection, PlatformType, GEngineMinNetVersion, GEngineVersion);
#endif // UDK
				NetDriver->ServerConnection->FlushNet();
			break;
			}
#endif

		case NMT_Uses:
		{
			// Dependency information.
			FPackageInfo Info(NULL);
			Connection->ParsePackageInfo(Bunch, Info);

#if !SHIPPING_PC_GAME
			debugf(NAME_DevNet, TEXT(" ---> Package: %s, GUID: %s, Generation: %i, BasePkg: %s"), *Info.PackageName.ToString(), *Info.Guid.String(), Info.RemoteGeneration, *Info.ForcedExportBasePackageName.ToString());
#endif

			// verify that we have this package (and Guid matches) 
			FString Filename;
			UBOOL bWasPackageFound = FALSE;

			// use guid caches for seekfree loading, since we may not have the original package on disk
			if (GUseSeekFreeLoading)
			{
				UBOOL bFoundCache = FALSE;
				// go over all guid caches
				for (INT CacheIndex = 0; CacheIndex < CachedGuidCaches.Num() && !bWasPackageFound; CacheIndex++)
				{
					UGuidCache* Cache = CachedGuidCaches(CacheIndex);

					// note that we found one
					bFoundCache = TRUE;

					FGuid Guid;
					// look for this package in it
					if (Cache->GetPackageGuid(Info.PackageName, Guid))
					{
						// does the Guid match?
						if (Guid == Info.Guid)
						{
							bWasPackageFound = TRUE;
						}
					}
				}

				if (!bFoundCache)
				{
					debugf(NAME_DevNet, TEXT("  Failed to find Guid because there was no package cache"));
				}

				if (!bWasPackageFound)
				{
					debugf(NAME_DevNet, TEXT("Failed to match package %s [%s] in guid cache"), *Info.PackageName.ToString(), *Info.Guid.String());
				}
			}

			// if the packge guid wasn't found using GuidCache above, then look for it on disk (for mods, etc, or the PC)
			if (!bWasPackageFound)
			{
				if (GPackageFileCache->FindPackageFile(*Info.PackageName.ToString(), &Info.Guid, Filename, NULL))
				{
					// if we are not doing seekfree loading, then open the package and tell the server we have it
					// (seekfree loading case will open the packages in UGameEngine::LoadMap)
					if (!GUseSeekFreeLoading)
					{
						Info.Parent = CreatePackage(NULL, *Info.PackageName.ToString());

						BeginLoad();
						ULinkerLoad* Linker = GetPackageLinker(Info.Parent, NULL, LOAD_NoWarn | LOAD_NoVerify | LOAD_Quiet, NULL, &Info.Guid);
						EndLoad();
						if (Linker)
						{
							Info.LocalGeneration = Linker->Summary.Generations.Num();
							// tell the server what we have
							FNetControlMessage<NMT_Have>::Send(NetDriver->ServerConnection, Linker->Summary.Guid, Info.LocalGeneration);

							bWasPackageFound = TRUE;
						}
						else
						{
							// there must be some problem with the cache if this happens, as the package was found with the right Guid,
							// but then it failed to load, so log a message and mark that we didn't find it
							debugf(NAME_Warning, TEXT("Linker for package %s failed to load, even though it should have [from %s]"), *Info.PackageName.ToString(), *Filename);
						}
					}
					else
					{
						bWasPackageFound = TRUE;
					}
				}
			}

			if (!bWasPackageFound)
			{
				// we need to download this package
				FilesNeeded++;
				Info.PackageFlags |= PKG_Need;
#if IPHONE
				// Currently, we do not allow downloading on iDevices
				// Just execute the 'AllowDownloads == false' handling code...
#else
				if (!NetDriver->AllowDownloads || !(Info.PackageFlags & PKG_AllowDownload))
#endif
				{
					ConnectionError = FString::Printf(LocalizeSecure(LocalizeError(TEXT("NoDownload"), TEXT("Engine")), *Info.PackageName.ToString()));
					NetDriver->ServerConnection->Close();
					return;
				}
			}
			Connection->PackageMap->AddPackageInfo(Info);
			break;
		}
		case  NMT_Unload:
		{
			// remove a package from the package map
			FGuid Guid;
			FNetControlMessage<NMT_Unload>::Receive(Bunch, Guid);
			Connection->PackageMap->RemovePackageByGuid(Guid);
			break;
		}
		case NMT_Failure:
		{
			// our connection attempt failed for some reason, for example a synchronization mismatch (bad GUID, etc) or because the server rejected our join attempt (too many players, etc)
			// here we can further parse the string to determine the reason that the server closed our connection and present it to the user

			FString Error;
			FNetControlMessage<NMT_Failure>::Receive(Bunch, Error);
			if (Error != TEXT(""))
			{
				// try to get localized error message
				TArray<FString> Pieces;
				Error.ParseIntoArray(&Pieces, TEXT("."), TRUE);
				if (Pieces.Num() >= 3)
				{
					Error = Localize(*Pieces(1), *Pieces(2), *Pieces(0), NULL, TRUE);
				}
			}
			ConnectionError = Error;

			// Force close the session
			Connection->Close();
			break;
		}
		case NMT_Challenge:
		{
			// Challenged by server.
			FNetControlMessage<NMT_Challenge>::Receive(Bunch, Connection->NegotiatedVer, Connection->Challenge);

			FURL PartialURL(URL);
			PartialURL.Host = TEXT("");
			for (INT i = URL.Op.Num() - 1; i >= 0; i--)
			{
				if (URL.Op(i).Left(5) == TEXT("game="))
				{
					URL.Op.Remove(i);
				}
			}
			// ask the viewport if it wants to override the player's name (e.g. profile name)
			if (GEngine->GamePlayers.Num() > 0 && GEngine->GamePlayers(0) != NULL)
			{
				FString OverrideName = GEngine->GamePlayers(0)->eventGetNickname();
				if (OverrideName.Len() > 0)
				{
					PartialURL.AddOption(*FString::Printf(TEXT("Name=%s"), *OverrideName));
				}
			}
#if WITH_GAMESPY
			extern void appGetOnlineChallengeResponse(UNetConnection*,UBOOL);
			// Calculate the response string based upon the user's CD Key
			appGetOnlineChallengeResponse(Connection,FALSE);
#else
			Connection->ClientResponse = TEXT("0");
#endif
			FNetControlMessage<NMT_Netspeed>::Send(Connection, Connection->CurrentNetSpeed);
			FString URLString(PartialURL.String());
			FNetControlMessage<NMT_Login>::Send(Connection, Connection->ClientResponse, URLString);
			NetDriver->ServerConnection->FlushNet();
			break;
		}
		case NMT_DLMgr:
		{
			INT i = Connection->DownloadInfo.AddZeroed();
			FNetControlMessage<NMT_DLMgr>::Receive(Bunch, Connection->DownloadInfo(i).ClassName, Connection->DownloadInfo(i).Params, Connection->DownloadInfo(i).Compression);
			Connection->DownloadInfo(i).Class = StaticLoadClass( UDownload::StaticClass(), NULL, *Connection->DownloadInfo(i).ClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL );
			if (Connection->DownloadInfo(i).Class == NULL)
			{
				Connection->DownloadInfo.Remove(i);
			}
			break;
		}
		case NMT_Welcome:
		{
			// Server accepted connection.
			FString GameName;
			FNetControlMessage<NMT_Welcome>::Receive(Bunch, URL.Map, GameName);

			debugf(NAME_DevNet, TEXT("Welcomed by server (Level: %s, Game: %s)"), *URL.Map, *GameName);

			if (GameName.Len() > 0)
			{
				URL.AddOption(*FString::Printf(TEXT("game=%s"), *GameName));
			}

			// Send first download request.
			ReceiveNextFile(Connection);

			// We have successfully connected.
			bSuccessfullyConnected = TRUE;
			break;
		}
		case NMT_PeerConnect:
		{
			// Client was just told about a new client peer by the server.
			FClientPeerInfo ClientPeerInfo;
			FNetControlMessage<NMT_PeerConnect>::Receive(Bunch,ClientPeerInfo);

			if (PeerNetDriver != NULL)
			{
				// Determine the URL to connect to the remote peer
				FString PeerConnectionError;
				FURL PeerConnectURL;
				PeerConnectURL.Host = ClientPeerInfo.GetPeerConnectStr(FALSE);
				PeerConnectURL.Port = ClientPeerInfo.PeerPort;

				debugf(NAME_DevNet,TEXT("UNetPendingLevel: NMT_PeerConnect received. Connecting to new client peer PlayerId=0x%016I64X at remote address=%s"),
					ClientPeerInfo.PlayerId.Uid,*ClientPeerInfo.GetPeerConnectStr(TRUE));

				if (ClientPeerInfo.IsValid())
				{
					// Get the net id of the primary local player
					FUniqueNetId PrimaryPlayerId(EC_EventParm);
					if (GEngine->GamePlayers.Num() > 0 && GEngine->GamePlayers(0) != NULL)
					{
						PrimaryPlayerId = GEngine->GamePlayers(0)->eventGetUniqueNetId();
					}
					// Create a new client connection to the remote peer
					if (!PeerNetDriver->InitPeer(this,PeerConnectURL,ClientPeerInfo.PlayerId,PrimaryPlayerId,PeerConnectionError))
					{
						debugf(NAME_DevNet,TEXT("UNetPendingLevel: NMT_PeerConnect failed. Connection error =%s"),*PeerConnectionError);
					}
				}
				else
				{
					debugf(NAME_DevNet,TEXT("UNetPendingLevel: NMT_PeerConnect failed. Invalid peer ip addr"));
				}
			}
			else
			{
				debugf(NAME_DevNet,TEXT("UNetPendingLevel: NMT_PeerConnect failed. No valid PeerNetDriver."));
			}

			break;
		}
		default:
			debugf(NAME_DevNet, TEXT(" --- Unknown/unexpected message for pending level"));
			break;
	}
}

UBOOL UNetPendingLevel::TrySkipFile()
{
	return NetDriver->ServerConnection->Download && NetDriver->ServerConnection->Download->TrySkipFile();
}
void UNetPendingLevel::NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* InError, UBOOL Skipped )
{
	check(Connection->PackageMap->List.IsValidIndex(PackageIndex));

	// Map pack to package.
	FPackageInfo& Info = Connection->PackageMap->List(PackageIndex);
	if( *InError )
	{
		if( Connection->DownloadInfo.Num() > 1 )
		{
			// Try with the next download method.
			Connection->DownloadInfo.Remove(0);
			ReceiveNextFile( Connection );
		}
		else
		{
			// If transfer failed, so propagate error.
			if( ConnectionError==TEXT("") )
			{
				ConnectionError = FString::Printf( LocalizeSecure(LocalizeError(TEXT("DownloadFailed"),TEXT("Engine")), *Info.PackageName.ToString(), InError) );
// 				FailedConnectionType = FCT_VersionMismatch;
			}
		}
	}
	else
	{
		// Now that a file has been successfully received, mark its package as downloaded.
		check(Connection==NetDriver->ServerConnection);
		check(Info.PackageFlags&PKG_Need);
		Info.PackageFlags &= ~PKG_Need;
		FilesNeeded--;
		if( Skipped )
		{
			Connection->PackageMap->List.Remove( PackageIndex );
		}
		else
		{
			if (!GUseSeekFreeLoading)
			{
				// load it, verify, then tell the server
				Info.Parent = CreatePackage(NULL, *Info.PackageName.ToString());
				BeginLoad();
				ULinkerLoad* Linker = GetPackageLinker(Info.Parent, NULL, LOAD_NoWarn | LOAD_NoVerify | LOAD_Quiet, NULL, &Info.Guid);
				EndLoad();
				if (Linker == NULL || Linker->Summary.Guid != Info.Guid)
				{
					debugf(NAME_DevNet, TEXT("Downloaded package '%s' mismatched - Server GUID: %s Client GUID: %s"), *Info.Parent->GetName(), *Info.Guid.String(), *Linker->Summary.Guid.String());
#if OLD_CONNECTION_FAILURE_CODE
 					ConnectionError = FString::Printf(TEXT("Package '%s' version mismatch"), *Info.Parent->GetName());
 					FailedConnectionType = FCT_VersionMismatch;
#else
					ConnectionError = FString::Printf(LocalizeSecure(LocalizeError(TEXT("PackageVersion"),TEXT("Core")), *Info.Parent->GetName()));
#endif
					NetDriver->ServerConnection->Close();
				}
				else
				{
					Info.LocalGeneration = Linker->Summary.Generations.Num();
					// tell the server what we have
					FNetControlMessage<NMT_Have>::Send(NetDriver->ServerConnection, Linker->Summary.Guid, Info.LocalGeneration);
				}
			}
		}

		// Send next download request.
		ReceiveNextFile( Connection );
	}
}
void UNetPendingLevel::ReceiveNextFile( UNetConnection* Connection )
{
	// look for a file that needs downloading
	for (INT PackageIndex = 0; PackageIndex < Connection->PackageMap->List.Num(); PackageIndex++)
	{
		if (Connection->PackageMap->List(PackageIndex).PackageFlags & PKG_Need)
		{
			Connection->ReceiveFile(PackageIndex); 
			return;
		}
	}

	// if no more are needed, cleanup the downloader
	if (Connection->Download != NULL)
	{
		Connection->Download->CleanUp();
	}
}

UBOOL UNetPendingLevel::NotifySendingFile( UNetConnection* Connection, FGuid Guid )
{
	// Server has requested a file from this client.
	debugf( NAME_DevNet, *LocalizeError(TEXT("RequestDenied"),TEXT("Engine")) );
	return 0;

}

void UNetPendingLevel::NotifyProgress( EProgressMessageType MessageType, const FString& Title, const FString& Message )
{
	GEngine->SetProgress( MessageType, Title, Message );
}


/**
 * Create the peer net driver and a socket to listen for new client peer connections.
 */
void UNetPendingLevel::InitPeerListen()
{
	if (NetDriver != NULL && 
		NetDriver->ServerConnection != NULL &&
		PeerNetDriver != NULL)
	{
		FURL PeerListenURL;
		PeerListenURL.Port = FURL::DefaultPeerPort;
		FString PeerListenError;
		if (!PeerNetDriver->InitListen(this,PeerListenURL,PeerListenError))
		{
			// If peer didn't start listening then no other peers will be able connect to it
			debugf( NAME_DevNet, TEXT("Peer net driver failed to listen: %s"), *PeerListenError );
			
			// Notify peer connection failure
			GEngine->SetProgress(PMT_PeerConnectionFailure,
 					LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")),
					FString::Printf(LocalizeSecure(LocalizeError(TEXT("PeerListen"),TEXT("Engine")),*PeerListenError)) );

			PeerNetDriver = NULL;			
		}
	}	
}

/**
 * Determine if peer connections are currently being accepted
 *
 * @return EAcceptConnection type based on if ready to accept a new peer connection
 */
EAcceptConnection UNetPendingLevel::NotifyAcceptingPeerConnection()
{
	debugf( NAME_NetComeGo, TEXT("UNetPendingLevel: Attemping to accept new peer on %s"), *GetName());
	return ACCEPTC_Accept;
}

/**
 * Notify that a new peer connection was created from the listening socket
 *
 * @param Connection net connection that was just created
 */
void UNetPendingLevel::NotifyAcceptedPeerConnection( class UNetConnection* Connection )
{
	debugf( NAME_NetComeGo, TEXT("UNetPendingLevel: New peer connection %s %s %s"), *GetName(), appTimestamp(), *Connection->LowLevelGetRemoteAddress() );
}

/**
 * Handler for control channel messages sent on a peer connection
 *
 * @param Connection net connection that received the message bunch
 * @param MessageType type of the message bunch
 * @param Bunch bunch containing the data for the message type read from the connection
 */
void UNetPendingLevel::NotifyPeerControlMessage(UNetConnection* Connection, BYTE MessageType, class FInBunch& Bunch)
{
	check(Connection != NULL && Connection->Driver->bIsPeer);
	
	// Peer network control traffic
	switch (MessageType)
	{
		case NMT_PeerJoinResponse:
		{
			// Client peer received response for join request
			BYTE PeerJoinResponse = PeerJoin_Denied;
			FNetControlMessage<NMT_PeerJoinResponse>::Receive(Bunch,PeerJoinResponse);

			if (PeerJoinResponse == PeerJoin_Accepted)
			{
				debugf(NAME_DevNet,TEXT("UNetPendingLevel: received NMT_PeerJoinResponse. Peer join request was accepted."));
				Connection->State = USOCK_Open;
#if !FINAL_RELEASE
				Connection->Driver->Exec(TEXT("PEER SOCKETS"));
#endif			
			}
			else
			{
				debugf(NAME_DevNet,TEXT("UNetPendingLevel: received NMT_PeerJoinResponse. Peer join request was denied."));

				// Notify peer connection failure
				GEngine->SetProgress(PMT_PeerConnectionFailure,
 					LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")),
					FString::Printf(LocalizeSecure(LocalizeError(TEXT("PeerConnection"),TEXT("Engine")),TEXT("Peer join request was denied."))) );

				if (Connection->Actor != NULL)
				{
					Connection->Actor->eventRemovePeer(Connection->PlayerId);
				}
				Connection->FlushNet(TRUE);
				Connection->Close();
			}

			break;
		}
		case NMT_Failure:
		{
			// failure notification from another peer
			FString FailureStr;
			FNetControlMessage<NMT_Failure>::Receive(Bunch,FailureStr);			

			debugf(NAME_DevNet,TEXT("UNetPendingLevel: NMT_Failure str=[%s]"),*FailureStr);

			// Notify peer connection failure
			GEngine->SetProgress(PMT_PeerConnectionFailure,
 					LocalizeError(TEXT("ConnectionFailed_Title"),TEXT("Engine")),
					FString::Printf(LocalizeSecure(LocalizeError(TEXT("PeerConnection"),TEXT("Engine")),*FailureStr)) );

			// force close the peer that sent the failure
			Connection->Close();
			break;
		}
		default:
			debugf(NAME_DevNet, TEXT("UNetPendingLevel --- Unknown/unexpected peer control message type=%d"),MessageType);
	};
}


//
// Update the pending level's status.
//
void UNetPendingLevel::Tick( FLOAT DeltaTime )
{
	check(NetDriver);
	check(NetDriver->ServerConnection);

	// Handle timed out or failed connection.
	if( NetDriver->ServerConnection->State==USOCK_Closed && ConnectionError==TEXT("") )
	{
		ConnectionError = LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine"));
		return;
	}

	// Update network driver.
	NetDriver->TickDispatch( DeltaTime );
	NetDriver->TickFlush();	
	
	// Update peer network driver
	if (PeerNetDriver != NULL)
	{
		APlayerController* NetOwner = (NetDriver != NULL && NetDriver->ServerConnection != NULL) ? NetDriver->ServerConnection->Actor : NULL;
		PeerNetDriver->TickDispatch(DeltaTime);
		PeerNetDriver->UpdatePeerConnections(NetOwner);
		PeerNetDriver->TickFlush();
	}

	// All net drivers have had a chance to process local voice data
	UNetDriver::ClearLocalVoicePackets();
}

//
// Send JOIN to other end
//
void UNetPendingLevel::SendJoin()
{
	bSentJoinRequest = TRUE;
	FUniqueNetId PrimaryPlayerId(EC_EventParm);
	if (GEngine->GamePlayers.Num() > 0 && GEngine->GamePlayers(0) != NULL)
	{
		PrimaryPlayerId = GEngine->GamePlayers(0)->eventGetUniqueNetId();
	}
	FNetControlMessage<NMT_Join>::Send(NetDriver->ServerConnection, PrimaryPlayerId);
	
	// Let server know that this client is now listening for new peer connections, has to occur after Net Id is sent
	if (PeerNetDriver != NULL)
	{
		FURL PeerListenURL;
		PeerListenURL.Port = FURL::DefaultPeerPort;
		FNetControlMessage<NMT_PeerListen>::Send(NetDriver->ServerConnection,(DWORD&)PeerListenURL.Port);
	}

	NetDriver->ServerConnection->FlushNet(TRUE);
}
IMPLEMENT_CLASS(UNetPendingLevel);


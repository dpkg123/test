/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include <netex/net.h>
#include <netex/libnetctl.h>
#include <netex/ifctl.h>
#include <sys/ansi.h>
#include <netinet/in.h>
#include <netex/sockinfo.h >

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sysutil/sysutil_sysparam.h>
#include <cell/sysmodule.h>

#include "NetworkProfiler.h"

#if EPIC_INTERNAL
	// @see OnlineSubsystemGameSpy.h
	#define WANTS_NAT_TRAVERSAL 0
#endif

/**
 * @todo licensee: Licensees that are not using NP should define this to 0 so that
 * the Network Startup Dialog Utility does not look for NP status and can avoid a memory
 * container allocation
 */
#define START_NETWORK_FOR_NP 1


// @todo: document
FSocketSubsystemPS3 SocketSubsystem;
UBOOL GIsNetworkInitialized = FALSE;
TCHAR GHostName[CELL_NET_CTL_HOSTNAME_LEN];
TCHAR GHostIP[64];

/**
 * Starts up the socket subsystem 
 *
 * @param bIsEarlyInit If TRUE, this function is being called before GCmdLine, GFileManager, GConfig, etc are setup. If FALSE, they have been initialized properly
 */
void appSocketInit(UBOOL bIsEarlyInit)
{
	// currently, FSocketSubsystemPS3::Initialize needs GConfig, so we can't initialize early
	if (bIsEarlyInit)
	{
		return;
	}
	GSocketSubsystem = &SocketSubsystem;
	FString Error;
	if (GSocketSubsystem->Initialize(Error) == FALSE)
	{
		debugf(NAME_Init,TEXT("Failed to initialize socket subsystem: (%s)"),*Error);
	}
}

/** Shuts down the socket subsystem */
void appSocketExit(void)
{
	GSocketSubsystem->Destroy();
}

/**
 * Sets this socket into non-blocking mode
 *
 * @param bIsNonBlocking whether to enable blocking or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::SetNonBlocking(UBOOL bIsNonBlocking)
{
	INT Value = bIsNonBlocking ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_NBIO,&Value,sizeof(Value)) == 0;
}

/**
 * Sets a socket into broadcast mode (UDP only)
 *
 * @param bAllowBroadcast whether to enable broadcast or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::SetBroadcast(UBOOL bAllowBroadcast)
{
	INT Value = bAllowBroadcast ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,&Value,sizeof(Value)) == 0;
}

/**
 * Sets whether a socket can be bound to an address in use
 *
 * @param bAllowReuse whether to allow reuse or not
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketPS3::SetReuseAddr(UBOOL bAllowReuse)
{
	INT Value = bAllowReuse ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,&Value,sizeof(INT)) == 0;
}

/**
 * Sets whether and how long a socket will linger after closing
 *
 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
 * @param Timeout the amount of time to linger before closing
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketPS3::SetLinger(UBOOL bShouldLinger,INT Timeout)
{
	//@TODO: Is this function necessary?
	check(0);
	return FALSE;
}

/**
 * Enables error queue support for the socket
 *
 * @param bUseErrorQueue whether to enable error queueing or not
 *
 * @return TRUE if the call succeeded, FALSE otherwise
 */
UBOOL FSocketPS3::SetRecvErr(UBOOL bUseErrorQueue)
{
	return TRUE;
}

/**
 * Sets the size of the send buffer to use
 *
 * @param Size the size to change it to
 * @param NewSize the out value returning the size that was set (in case OS can't set that)
 *
 * @return TRUE if the call succedded, FALSE otherwise
 */
UBOOL FSocketPS3::SetSendBufferSize(INT,INT& NewSize)
{
	SOCKLEN SizeSize = sizeof(INT);
	// Just read the value of the network stack
	return getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&NewSize,GCC_OPT_INT_CAST &SizeSize) == 0;
}

/**
 * Sets the size of the receive buffer to use
 *
 * @param Size the size to change it to
 * @param NewSize the out value returning the size that was set (in case OS can't set that)
 *
 * @return TRUE if the call succedded, FALSE otherwise
 */
UBOOL FSocketPS3::SetReceiveBufferSize(INT Size,INT& NewSize)
{
	SOCKLEN SizeSize = sizeof(INT);
	UBOOL bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(INT)) == 0;
	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize,GCC_OPT_INT_CAST &SizeSize);
	return bOk;
}

/**
 * Closes the socket
 *
 * @param TRUE if it closes without errors, FALSE otherwise
 */
UBOOL FSocketPS3::Close(void)
{
	return socketclose(Socket) == 0;
}

/**
 * Binds a socket to a network byte ordered address
 *
 * @param Addr the address to bind to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::Bind(const FInternetIpAddr& Addr)
{
	return bind(Socket,Addr,sizeof(SOCKADDR_IN)) == 0;
}

/**
 * Connects a socket to a network byte ordered address
 *
 * @param Addr the address to connect to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::Connect(const FInternetIpAddr& Addr)
{
	INT Return = connect(Socket,Addr,sizeof(SOCKADDR_IN));
	// if there was an error code, the actual error is in sys_net_errno
	if (Return < 0)
	{
		Return = sys_net_errno;
	}
	// The API docs say EALREADY is possible, but it is undefined currently
	return Return == SE_EINPROGRESS || Return == 0;
}

/**
 * Places the socket into a state to listen for incoming connections
 *
 * @param MaxBacklog the number of connections to queue before refusing them
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::Listen(INT MaxBacklog)
{
	return listen(Socket,MaxBacklog) == 0;
}

/**
 * Queries the socket to determine if there is a pending connection
 *
 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::HasPendingConnection(UBOOL& bHasPendingConnection)
{
	UBOOL bHasSucceeded = FALSE;
	bHasPendingConnection = FALSE;
	// Check and return without waiting
	timeval Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the bits. First check for errors
	INT SelectStatus = socketselect(Socket + 1,NULL,NULL,&SocketSet,&Time);
	if (SelectStatus == 0)
	{
		FD_ZERO(&SocketSet);
		FD_SET(Socket,&SocketSet);
		// Now check to see if it has a pending connection
		SelectStatus = socketselect(Socket + 1,&SocketSet,NULL,NULL,&Time);
		// One or more means there is a pending connection
		bHasPendingConnection = SelectStatus > 0;
		// Non negative means it worked
		bHasSucceeded = SelectStatus >= 0;
	}
	return bHasSucceeded;
}

/**
 * Queries the socket to determine if there is pending data on the queue
 *
 * @param PendingDataSize out parameter indicating how much data is on the pipe for a single recv call
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3::HasPendingData(UINT& PendingDataSize)
{
	UBOOL bHasSucceeded = FALSE;
	PendingDataSize = 0;
	// Check and return without waiting
	timeval Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the read socket.
	INT SelectStatus = socketselect(Socket + 1,&SocketSet,NULL,NULL,&Time);
	if (SelectStatus > 0)
	{				
#if 0	//Not sure which is better, 
		char TempBuf[256];
		// See if there is any pending data on the read socket
		int NumBytes = recv( Socket, TempBuf, 1, MSG_PEEK );
		//if (ioctlsocket( Socket, FIONREAD, (ULONG*)&PendingDataSize ) == 0)
		if (NumBytes >= 0)
		{
			PendingDataSize = (UINT)NumBytes;
			bHasSucceeded = TRUE;
		}
#else  //below option is from SCE DevNet

		sys_net_sockinfo_t SocketInfo;
		if (sys_net_get_sockinfo( Socket, &SocketInfo, 1) >= 0)
		{
			PendingDataSize = (UINT)SocketInfo.recv_queue_length;
			bHasSucceeded = TRUE;
		}
#endif
	}
	return bHasSucceeded;
}

/**
 * Accepts a connection that is pending
 *
 * @param		SocketDescription debug description of socket
 *
 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
 */
FSocket* FSocketPS3::Accept(const FString& SocketDescription)
{
	SOCKET NewSocket = accept(Socket,NULL,NULL);
	if ( NewSocket != INVALID_SOCKET )
	{
		return new FSocketPS3( NewSocket, SocketType, SocketDescription );
	}
	return NULL;
}

/**
 * Accepts a connection that is pending
 *
 * @param		OutAddr the address of the connection
 * @param		SocketDescription debug description of socket
 *
 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
 */
FSocket* FSocketPS3::Accept(FInternetIpAddr& OutAddr,const FString& SocketDescription)
{
	socklen_t SizeOf = sizeof(SOCKADDR_IN);
	SOCKET NewSocket = accept(Socket,OutAddr,&SizeOf);
	if ( NewSocket != INVALID_SOCKET )
	{
		return new FSocketPS3( NewSocket, SocketType, SocketDescription );
	}
	return NULL;
}

/**
 * Sends a buffer to a network byte ordered address
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 * @param Destination the network byte ordered address to send to
 */
UBOOL FSocketPS3::SendTo(const BYTE* Data,INT Count,INT& BytesSent,const FInternetIpAddr& Destination)
{
	// Write the data and see how much was written
	BytesSent = sendto(Socket,(const char*)Data,Count,0,Destination,sizeof(SOCKADDR_IN));
	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));
	return BytesSent >= 0;
}

/**
 * Sends a buffer on a connected socket
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 */
UBOOL FSocketPS3::Send(const BYTE* Data,INT Count,INT& BytesSent)
{
	BytesSent = send(Socket,(const char*)Data,Count,0);
	NETWORK_PROFILER(FSocket::Send(Data,Count,BytesSent));
	return BytesSent >= 0;
}

/**
 * Reads a chunk of data from the socket. Gathers the source address too
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 * @param Source out param receiving the address of the sender of the data
 */
UBOOL FSocketPS3::RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,FInternetIpAddr& Source)
{
	SOCKLEN Size = sizeof(SOCKADDR_IN);
	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket,(char*)Data,BufferSize,0,Source,&Size);
	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));
	return BytesRead >= 0;
}

/**
 * Reads a chunk of data from a connected socket
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 */
UBOOL FSocketPS3::Recv(BYTE* Data,INT BufferSize,INT& BytesRead)
{
	BytesRead = recv(Socket,(char*)Data,BufferSize,0);
	NETWORK_PROFILER(FSocket::Recv(Data,BufferSize,BytesRead));
	return BytesRead >= 0;
}

/**
 * Determines the connection state of the socket
 */
ESocketConnectionState FSocketPS3::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;
	// Check and return without waiting
	timeval Time = {0,0};
	fd_set SocketSet;
	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket,&SocketSet);
	// Check the status of the bits. First check for errors
	INT SelectStatus = socketselect(Socket + 1,NULL,NULL,&SocketSet,&Time);
	if (SelectStatus == 0)
	{
		FD_ZERO(&SocketSet);
		FD_SET(Socket,&SocketSet);
		// Now check to see if it's connected (writable means connected)
		SelectStatus = socketselect(Socket + 1,NULL,&SocketSet,NULL,&Time);
		if (SelectStatus > 0)
		{
			CurrentState = SCS_Connected;
		}
		// Zero means it is still pending
		if (SelectStatus == 0)
		{
			CurrentState = SCS_NotConnected;
		}
	}
	return CurrentState;
}

/**
 * Reads the address the socket is bound to and returns it
 */
FInternetIpAddr FSocketPS3::GetAddress(void)
{
	FInternetIpAddr Addr;
	SOCKLEN Size = sizeof(SOCKADDR_IN);
	// Figure out what ip/port we are bound to
	UBOOL bOk = getsockname(Socket,Addr,&Size) == 0;
	if (bOk == FALSE)
	{
		debugf(NAME_Error,TEXT("Failed to read address for socket (%d)"),
			GSocketSubsystem->GetSocketError());
	}
	return Addr;
}

/**
 * Reads the port this socket is bound to.
 */ 
INT FSocketPS3::GetPortNo(void)
{
	const FInternetIpAddr& Addr = GetAddress();
	// Read the port number
	return Addr.GetPort();
}

/**
 * Binds a socket to a network byte ordered address
 *
 * @param Addr the address to bind to
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketPS3UdpP2p::Bind(const FInternetIpAddr& Addr)
{
	sockaddr_in_p2p P2pAddr;
	P2pAddr.sin_family = AF_INET;
	P2pAddr.sin_addr.s_addr = INADDR_ANY;
	P2pAddr.sin_port = htons(SCE_NP_PORT);
	P2pAddr.sin_vport = htons(FURL::DefaultPort);
	// Now bind with the virtual port handling
	return bind(Socket,(sockaddr*)&P2pAddr,sizeof(sockaddr_in_p2p)) == 0;
}

/**
 * Reads a chunk of data from the socket. Gathers the source address too
 *
 * @param Data the buffer to read into
 * @param BufferSize the max size of the buffer
 * @param BytesRead out param indicating how many bytes were read from the socket
 * @param Source out param receiving the address of the sender of the data
 */
UBOOL FSocketPS3UdpP2p::RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,FInternetIpAddr& Source)
{
	SOCKLEN Size = sizeof(sockaddr_in_p2p);
	sockaddr_in_p2p P2pAddr;
	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket,(char*)Data,BufferSize,0,(sockaddr*)&P2pAddr,&Size);
	if (BytesRead >= 0)
	{
		P2pAddr.sin_vport = htons(0);
		appMemcpy((SOCKADDR*)Source,&P2pAddr,sizeof(SOCKADDR_IN));
	}
	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));
	return BytesRead >= 0;
}

/**
 * Sends a buffer to a network byte ordered address
 *
 * @param Data the buffer to send
 * @param Count the size of the data to send
 * @param BytesSent out param indicating how much was sent
 * @param Destination the network byte ordered address to send to
 */
UBOOL FSocketPS3UdpP2p::SendTo(const BYTE* Data,INT Count,INT& BytesSent,const FInternetIpAddr& Destination)
{
	sockaddr_in_p2p P2pAddr;
	// Copy the inbound data which we normally use
	appMemcpy(&P2pAddr,Destination,sizeof(SOCKADDR_IN));
	// And mark the virtual port with our port and set the real port to the NP port
	P2pAddr.sin_vport = htons(FURL::DefaultPort);
	P2pAddr.sin_port = htons(Destination.GetPort());
	// Now send with the virtual port handling
	BytesSent = sendto(Socket,(const char*)Data,Count,0,(sockaddr*)&P2pAddr,sizeof(sockaddr_in_p2p));
	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));
	return BytesSent >= 0;
}

/**
 * Make sure a network is connected, and if so, get info about it
 */
void UpdateGlobalNetworkInfo()
{
	debugf(TEXT("Updating Network Info!"));
	INT State;
	// check our net state
	INT Error = cellNetCtlGetState(&State);

	// remember current state
	UBOOL bWasNetworkInitialized = appInterlockedExchange((INT*)&GIsNetworkInitialized,FALSE);

	// if it failed, we can't use the network
	if (Error < 0) 
	{
		debugf(TEXT("cellNetCtlGetState() failed (%x)"), Error);
	}
	else if (State == CELL_NET_CTL_STATE_IPObtained)
	{
		appInterlockedExchange((INT*)&GIsNetworkInitialized,TRUE);
	}

	// did the state change?
	if (bWasNetworkInitialized != GIsNetworkInitialized)
	{
		// if we went from disconnected to connected, get info
		if (GIsNetworkInitialized == TRUE)
		{
			debugf(TEXT("Network has been connected"));

			// get IP address that was obtained
			CellNetCtlInfo NetInfo;
			Error = cellNetCtlGetInfo(CELL_NET_CTL_INFO_IP_ADDRESS, &NetInfo);
			checkf(Error == 0, TEXT("cellNetCtlGetInfo() failed (%x) to get IP address, after IP was obtained!"));

			// copy it to a global
			appStrcpy(GHostIP, ANSI_TO_TCHAR(NetInfo.ip_address));

			// Get the hostname (as set thru the OSD)
			Error = cellNetCtlGetInfo(CELL_NET_CTL_INFO_DHCP_HOSTNAME, &NetInfo);
			if (Error == 0 && NetInfo.dhcp_hostname[0] )
			{
				// get DHCP name if given
				appStrcpy(GHostName, ANSI_TO_TCHAR(NetInfo.dhcp_hostname));
			}
			else
			{
				// otherwise use the PS3's nickname
				ANSICHAR HostName[CELL_NET_CTL_HOSTNAME_LEN];
				Error = cellSysutilGetSystemParamString(CELL_SYSUTIL_SYSTEMPARAM_ID_NICKNAME, HostName, CELL_SYSUTIL_SYSTEMPARAM_NICKNAME_SIZE);
				if ( Error == 0 )
				{
					appStrcpy(GHostName, ANSI_TO_TCHAR(HostName));
				}
			}
		}
		// if we disconnected, then set IP to 0
		else
		{
			appStrcpy(GHostIP, TEXT("0.0.0.0"));
		}
	}


	if (GIsNetworkInitialized)
	{
		debugf(TEXT("===================================================="));
		debugf(TEXT("Network is running, the IP address is %s, hostname is %s"), GHostIP, GHostName);
		debugf(TEXT("===================================================="));
	}
	else
	{
		debugf(TEXT("===================================================="));
		debugf(TEXT("Network IS NOT RUNNING. Unable to get IPAddress (plugin in a network cable, or check system settings)"));
		debugf(TEXT("===================================================="));
	}
}

/** 
 * Callback function called when the network state changes
 */
void NetConnectionStateChangeFunc(INT PreviousState, INT NewState, INT Event, INT ErrorCode, void* UserData)
{
	// printout out state
	if (NewState == CELL_NET_CTL_STATE_IPObtained)
	{
		debugf(TEXT("Network connected! [Event was %d]"), Event);
	}
	else if (NewState == CELL_NET_CTL_STATE_Disconnected)
	{
		debugf(TEXT("Network disconnected! [Event was %d]"), Event);
	}
	else if (NewState == CELL_NET_CTL_STATE_Connecting)
	{
		debugf(TEXT("Network connecting... [Event was %d]"), Event);
	}
	else if (NewState == CELL_NET_CTL_STATE_IPObtaining)
	{
		debugf(TEXT("Obtaining IP address... [Event was %d]"), Event);
	}

	// if going into or out of connected state, update connection info
	if (PreviousState == CELL_NET_CTL_STATE_IPObtained || NewState == CELL_NET_CTL_STATE_IPObtained)
	{
		UpdateGlobalNetworkInfo();
	}
}



/**
 * Does PS3 platform specific initialization of the sockets library
 *
 * @param Error a string that is filled with error information
 *
 * @return TRUE if initialized ok, FALSE otherwise
 */
UBOOL FSocketSubsystemPS3::Initialize(FString& Error)
{
	appStrcpy(GHostName, TEXT("PS3-Unknown"));
	appStrcpy(GHostIP, TEXT("0.0.0.0"));

	if (GConfig != NULL)
	{
		// Whether to use UDPP2P for nat traversal or not
		GConfig->GetBool(SOCKET_API,TEXT("bShouldUseNatTraversal"),bShouldUseNatTraversal,GEngineIni);
	}

	// start up networking
	cellSysmoduleLoadModule( CELL_SYSMODULE_NET );
	cellSysmoduleLoadModule( CELL_SYSMODULE_NETCTL );
	sys_net_initialize_network();

	appInterlockedExchange((INT*)&GIsNetworkInitialized,FALSE);

	// init netctl
	INT Ret = cellNetCtlInit();
	// make sure it succeeded
	Error = FString::Printf(TEXT("cellNetCtlInit() failed(%x)"), Ret);

	// CNN - If GPAD is running, it will already have initialized the network, so ignore if it's already running
	if ((Ret != CELL_NET_CTL_ERROR_NOT_TERMINATED) && (Ret != CELL_OK))
	{
		checkf(Ret == 0, *Error);
	}

	UpdateGlobalNetworkInfo();

	// install disconnection handler
	INT HandlerID;
	cellNetCtlAddHandler(NetConnectionStateChangeFunc, NULL, &HandlerID);

	// note that the socket subsystem has been started
	GIpDrvInitialized = TRUE;
	return GIpDrvInitialized;
}

/**
 * Performs PS3 specific socket clean up
 */
void FSocketSubsystemPS3::Destroy(void)
{
}

/**
 * Creates a data gram (UDP) socket
 *
 * @param SocketDescription debug description
 * @param bShouldForceUDP whether UDP should be forced on or if UDPP2P should be the default
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemPS3::CreateDGramSocket(const FString& SocketDescription, UBOOL bShouldForceUDP)
{
	if ( GIsNetworkInitialized )
	{
#if WANTS_NAT_TRAVERSAL
		// Global to get at this information without modifying tons of code
		extern UBOOL GWantsNatTraversal;
		// Use the standard socket if nat traversal is disabled or the caller
		// has requested to override it
		if (bShouldForceUDP || bShouldUseNatTraversal == FALSE || GWantsNatTraversal == FALSE)
#endif
		{
			// Create a standard UDP socket
			SOCKET Socket = socket(AF_INET,SOCK_DGRAM ,IPPROTO_UDP);
			if (Socket >= 0)
			{
				return new FSocketPS3(Socket,SOCKTYPE_Datagram,SocketDescription);
			}
		}
#if WANTS_NAT_TRAVERSAL
		else
		{
			// Create a socket with virtualized port support
			// NOTE: Do not set a protocol!
			SOCKET Socket = socket(AF_INET,SOCK_DGRAM_P2P,0);
			if (Socket >= 0)
			{
				return new FSocketPS3UdpP2p(Socket,SOCKTYPE_Datagram,SocketDescription);
			}
		}
#endif
	}
	return NULL;
}

/**
 * Creates a stream socket
 *
 * @param SocketDescription debug description
 *
 * @return the new socket or NULL if failed
 */
FSocket* FSocketSubsystemPS3::CreateStreamSocket(const FString& SocketDescription)
{
	if ( GIsNetworkInitialized )
	{
		SOCKET Socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		return Socket != INVALID_SOCKET ? new FSocketPS3(Socket,SOCKTYPE_Streaming,SocketDescription) : NULL;
	}
	return NULL;
}

/**
 * Does a DNS look up of a host name
 *
 * @param HostName the name of the host to look up
 * @param Addr the address to copy the IP address to
 */
INT FSocketSubsystemPS3::GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr)
{
	INT ErrorCode = 0;
	// Threadsafe on the PS3 (according to docs)
	HOSTENT* HostEnt = gethostbyname(HostName);
	if (HostEnt != NULL)
	{
		// Make sure it's a valid type and if it is copy it
		if (HostEnt->h_addrtype == PF_INET)
		{
			Addr.SetIp(*(in_addr*)(*HostEnt->h_addr_list));
		}
		else
		{
			ErrorCode = SE_HOST_NOT_FOUND;
		}
	}
	else
	{
		ErrorCode = GetLastErrorCode();
	}
	return ErrorCode;
}

/**
 * Determines the name of the local machine
 *
 * @param HostName the string that receives the data
 *
 * @return TRUE if successful, FALSE otherwise
 */
UBOOL FSocketSubsystemPS3::GetHostName(FString& HostName)
{
	HostName = appComputerName();
	return TRUE;
}

/**
 * Returns a human readable string from an error code
 *
 * @param Code the error code to check
 */
const TCHAR* FSocketSubsystemPS3::GetSocketError(INT Code)
{
#if !FINAL_RELEASE
	if (Code == -1)
	{
		Code = GSocketSubsystem->GetLastErrorCode();
	}
	switch (Code)
	{
		case SE_NO_ERROR: return TEXT("SE_NO_ERROR");
		case SE_EINTR: return TEXT("SE_EINTR");
		case SE_EBADF: return TEXT("SE_EBADF");
		case SE_EACCES: return TEXT("SE_EACCES");
		case SE_EFAULT: return TEXT("SE_EFAULT");
		case SE_EINVAL: return TEXT("SE_EINVAL");
		case SE_EMFILE: return TEXT("SE_EMFILE");
		case SE_EWOULDBLOCK: return TEXT("SE_EWOULDBLOCK");
		case SE_EINPROGRESS: return TEXT("SE_EINPROGRESS");
		case SE_EALREADY: return TEXT("SE_EALREADY");
		case SE_ENOTSOCK: return TEXT("SE_ENOTSOCK");
		case SE_EDESTADDRREQ: return TEXT("SE_EDESTADDRREQ");
		case SE_EMSGSIZE: return TEXT("SE_EMSGSIZE");
		case SE_EPROTOTYPE: return TEXT("SE_EPROTOTYPE");
		case SE_ENOPROTOOPT: return TEXT("SE_ENOPROTOOPT");
		case SE_EPROTONOSUPPORT: return TEXT("SE_EPROTONOSUPPORT");
		case SE_ESOCKTNOSUPPORT: return TEXT("SE_ESOCKTNOSUPPORT");
		case SE_EOPNOTSUPP: return TEXT("SE_EOPNOTSUPP");
		case SE_EPFNOSUPPORT: return TEXT("SE_EPFNOSUPPORT");
		case SE_EAFNOSUPPORT: return TEXT("SE_EAFNOSUPPORT");
		case SE_EADDRINUSE: return TEXT("SE_EADDRINUSE");
		case SE_EADDRNOTAVAIL: return TEXT("SE_EADDRNOTAVAIL");
		case SE_ENETDOWN: return TEXT("SE_ENETDOWN");
		case SE_ENETUNREACH: return TEXT("SE_ENETUNREACH");
		case SE_ENETRESET: return TEXT("SE_ENETRESET");
		case SE_ECONNABORTED: return TEXT("SE_ECONNABORTED");
		case SE_ECONNRESET: return TEXT("SE_ECONNRESET");
		case SE_ENOBUFS: return TEXT("SE_ENOBUFS");
		case SE_EISCONN: return TEXT("SE_EISCONN");
		case SE_ENOTCONN: return TEXT("SE_ENOTCONN");
		case SE_ESHUTDOWN: return TEXT("SE_ESHUTDOWN");
		case SE_ETOOMANYREFS: return TEXT("SE_ETOOMANYREFS");
		case SE_ETIMEDOUT: return TEXT("SE_ETIMEDOUT");
		case SE_ECONNREFUSED: return TEXT("SE_ECONNREFUSED");
		case SE_ELOOP: return TEXT("SE_ELOOP");
		case SE_ENAMETOOLONG: return TEXT("SE_ENAMETOOLONG");
		case SE_EHOSTDOWN: return TEXT("SE_EHOSTDOWN");
		case SE_EHOSTUNREACH: return TEXT("SE_EHOSTUNREACH");
		case SE_ENOTEMPTY: return TEXT("SE_ENOTEMPTY");
		case SE_EPROCLIM: return TEXT("SE_EPROCLIM");
		case SE_EUSERS: return TEXT("SE_EUSERS");
		case SE_EDQUOT: return TEXT("SE_EDQUOT");
		case SE_ESTALE: return TEXT("SE_ESTALE");
		case SE_EREMOTE: return TEXT("SE_EREMOTE");
		case SE_EDISCON: return TEXT("SE_EDISCON");
		case SE_HOST_NOT_FOUND: return TEXT("SE_HOST_NOT_FOUND");
		case SE_TRY_AGAIN: return TEXT("SE_TRY_AGAIN");
		case SE_NO_RECOVERY: return TEXT("SE_NO_RECOVERY");
		case SE_NO_DATA: return TEXT("SE_NO_DATA");
		default: return TEXT("Unknown Error");
	};
#else
	return TEXT("");
#endif
}

/**
 * Cleans up a socket class
 *
 * @param Socket the socket object to destroy
 */
void FSocketSubsystemPS3::DestroySocket(FSocket* Socket)
{
	delete Socket;
}

/**
 * Uses the socket libs to look up the host address
 *
 * @param Out the output device to log messages to
 * @param HostAddr the out param receiving the host address
 *
 * @return always TRUE
 */
UBOOL FSocketSubsystemPS3::GetLocalHostAddr(FOutputDevice& Out,
	FInternetIpAddr& HostAddr)
{
	// Clear out any values in there
	HostAddr.SetAnyAddress();

	if (!GIsNetworkInitialized)
	{
		return FALSE;
	}

	// convert IP addr string to FInternetIpAddr
	UBOOL bIsValid;
	HostAddr.SetIp(GHostIP, bIsValid);
	check(bIsValid);

	return TRUE;
}

/**
 * @return Whether the machine has a properly configured network device or not
 */
UBOOL FSocketSubsystemPS3::HasNetworkDevice(void)
{
	return GIsNetworkInitialized;
}

/**
 * Helper for tracking net start progress
 */
struct FNetworkStartData
{
	/** Thread safe counter to be incremented when complete */
	FThreadSafeCounter* Counter;

	/** TRUE if the user is successfully connected to the network (and maybe NP if desired)*/
	UBOOL bWasConnected;
};

static FNetworkStartData* GPS3NetworkStartData = NULL;

void NetStartDialogFunc(uint64_t Status, uint64_t Param, void* UserData)
{
	switch (Status) 
	{
		case CELL_SYSUTIL_NET_CTL_NETSTART_LOADED:
			debugf(TEXT("CELL_SYSUTIL_NET_CTL_NETSTART_LOADED"));
			break;
		case CELL_SYSUTIL_NET_CTL_NETSTART_FINISHED:
			debugf(TEXT("CELL_SYSUTIL_NET_CTL_NETSTART_FINISHED"));
			
			{
				CellNetCtlNetStartDialogResult Result;
				Result.size = sizeof(Result);
				// get result
				cellNetCtlNetStartDialogUnloadAsync(&Result);

				// was the connection successful?
				GPS3NetworkStartData->bWasConnected = Result.result == 0;
			}
			
			break;
		case CELL_SYSUTIL_NET_CTL_NETSTART_UNLOADED:
			debugf(TEXT("CELL_SYSUTIL_NET_CTL_NETSTART_UNLOADED"));

			// mark as as complete
			GPS3NetworkStartData->Counter->Increment();

			// we no longer need the screen to be cleared
#if !USE_NULL_RHI
			GPS3Gcm->SetNeedsScreenClear(FALSE);
#endif

			break;
	}

}


/**
 * Start the async task to make sure user is connected to NP (and therefore
 * gamespy if so set up)
 */
void appPS3BeginNetworkStartUtility()
{
	// make sure connection is good
	CellNetCtlNetStartDialogParam NetStartParam;
	NetStartParam.size = sizeof(NetStartParam);

#if START_NETWORK_FOR_NP
	// starting NP
	NetStartParam.type = CELL_NET_CTL_NETSTART_TYPE_NP;
	NetStartParam.cid = SYS_MEMORY_CONTAINER_ID_INVALID;
#else
	NetStartParam.type = CELL_NET_CTL_NETSTART_TYPE_NET;
	NetStartParam.cid = SYS_MEMORY_CONTAINER_ID_INVALID;
#endif

	// create the persistent data
	GPS3NetworkStartData = new FNetworkStartData;
	GPS3NetworkStartData->Counter = new FThreadSafeCounter;

	// pass the memory container ID to the callback so it can be freed asap
	cellSysutilRegisterCallback(2, NetStartDialogFunc, NULL);

	// start async connection checker
	INT Ret = cellNetCtlNetStartDialogLoadAsync(&NetStartParam);

	debugf(TEXT("Call cellNetCtlNetStartDialogLoadAsync... Result = %d"), Ret);

	// tell the rendered to clear the screen, since the net start dialog doesn't render it
#if !USE_NULL_RHI
	GPS3Gcm->SetNeedsScreenClear(TRUE);
#endif
}

/**
 * Tick the async network start utility
 *
 * @param bIsConnected [out] TRUE if the user was connected by the end of the startup process. Only valid if this function returns TRUE
 *
 * @return The async operation was finished, now bIsConnected is valid, and ticking can stop
 */
UBOOL appPS3TickNetworkStartUtility(UBOOL& bIsConnected)
{
	// if we aren't running the utility, return FALSE
	if (GPS3NetworkStartData == NULL)
	{
		return FALSE;
	}

	// if we have't been incremented yet, then we are still going
	if (GPS3NetworkStartData->Counter->GetValue() == 0)
	{
		return FALSE;
	}

	// otherwise, we are complete, so set connection state 
	bIsConnected = GPS3NetworkStartData->bWasConnected;

	// clean up memory
	delete GPS3NetworkStartData->Counter;
	delete GPS3NetworkStartData;
	GPS3NetworkStartData = NULL;

	// disable the callback
	cellSysutilUnregisterCallback(2);

	// return complete (TRUE)
	return TRUE;
}

#endif	//#if WITH_UE3_NETWORKING

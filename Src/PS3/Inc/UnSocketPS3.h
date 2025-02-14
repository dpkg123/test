/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
#if WITH_UE3_NETWORKING

#ifndef __UNSOCKETPS3_H__
#define __UNSOCKETPS3_H__

/** Platform specific errors mapped to our error codes */
enum ESocketErrors
{
	SE_NO_ERROR,
	SE_EINTR = SYS_NET_EINTR,
	SE_EBADF = SYS_NET_EBADF,
	SE_EACCES = SYS_NET_EACCES,
	SE_EFAULT = SYS_NET_EFAULT,
	SE_EINVAL = SYS_NET_EINVAL,
	SE_EMFILE = SYS_NET_EMFILE,
	SE_EWOULDBLOCK = SYS_NET_EWOULDBLOCK,
	SE_EINPROGRESS = SYS_NET_EINPROGRESS,
	SE_EALREADY = SYS_NET_EALREADY,
	SE_ENOTSOCK = SYS_NET_ENOTSOCK,
	SE_EDESTADDRREQ = SYS_NET_EDESTADDRREQ,
	SE_EMSGSIZE = SYS_NET_EMSGSIZE,
	SE_EPROTOTYPE = SYS_NET_EPROTOTYPE,
	SE_ENOPROTOOPT = SYS_NET_ENOPROTOOPT,
	SE_EPROTONOSUPPORT = SYS_NET_EPROTONOSUPPORT,
	SE_ESOCKTNOSUPPORT = SYS_NET_ESOCKTNOSUPPORT,
	SE_EOPNOTSUPP = SYS_NET_EOPNOTSUPP,
	SE_EPFNOSUPPORT = SYS_NET_EPFNOSUPPORT,
	SE_EAFNOSUPPORT = SYS_NET_EAFNOSUPPORT,
	SE_EADDRINUSE = SYS_NET_EADDRINUSE,
	SE_EADDRNOTAVAIL = SYS_NET_EADDRNOTAVAIL,
	SE_ENETDOWN = SYS_NET_ENETDOWN,
	SE_ENETUNREACH = SYS_NET_ENETUNREACH,
	SE_ENETRESET = SYS_NET_ENETRESET,
	SE_ECONNABORTED = SYS_NET_ECONNABORTED,
	SE_ECONNRESET = SYS_NET_ECONNRESET,
	SE_ENOBUFS = SYS_NET_ENOBUFS,
	SE_EISCONN = SYS_NET_EISCONN,
	SE_ENOTCONN = SYS_NET_ENOTCONN,
	SE_ESHUTDOWN = SYS_NET_ESHUTDOWN,
	SE_ETOOMANYREFS = SYS_NET_ETOOMANYREFS,
	SE_ETIMEDOUT = SYS_NET_ETIMEDOUT,
	SE_ECONNREFUSED = SYS_NET_ECONNREFUSED,
	SE_ELOOP = SYS_NET_ELOOP,
	SE_ENAMETOOLONG = SYS_NET_ENAMETOOLONG,
	SE_EHOSTDOWN = SYS_NET_EHOSTDOWN,
	SE_EHOSTUNREACH = SYS_NET_EHOSTUNREACH,
	SE_ENOTEMPTY = SYS_NET_ENOTEMPTY,
	SE_EPROCLIM = SYS_NET_EPROCLIM,
	SE_EUSERS = SYS_NET_EUSERS,
	SE_EDQUOT = SYS_NET_EDQUOT,
	SE_ESTALE = SYS_NET_ESTALE,
	SE_EREMOTE = SYS_NET_EREMOTE,
	SE_EDISCON = -1,
	SE_SYSNOTREADY = -1,
	SE_VERNOTSUPPORTED = -1,
	SE_NOTINITIALISED = -1,
	SE_HOST_NOT_FOUND = HOST_NOT_FOUND,
	SE_TRY_AGAIN = TRY_AGAIN,
	SE_NO_RECOVERY = NO_RECOVERY,
	SE_NO_DATA = SYS_NET_ENODATA,
	SE_UDP_ERR_PORT_UNREACH = SYS_NET_ECONNRESET
};

/**
 * This is the Windows specific socket class
 */
class FSocketPS3 :
	public FSocket
{
protected:
	/** The PS3 specific socket object */
	SOCKET Socket;

public:
	/**
	 * Assigns a PS3 socket to this object
	 *
	 * @param InSocket the socket to assign to this object
	 * @param InSocketType the type of socket that was created
	 * @param InSocketDescription the debug description of the socket
	 */
	FSocketPS3(SOCKET InSocket,ESocketType InSocketType,const FString& InSocketDescription) :
		FSocket(InSocketType,InSocketDescription),
		Socket(InSocket)
	{
	}
	/** Closes the socket if it is still open */
	virtual ~FSocketPS3(void)
	{
		Close();
	}
	/**
	 * Closes the socket
	 *
	 * @param TRUE if it closes without errors, FALSE otherwise
	 */
	virtual UBOOL Close(void);
	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Bind(const FInternetIpAddr& Addr);
	/**
	 * Connects a socket to a network byte ordered address
	 *
	 * @param Addr the address to connect to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Connect(const FInternetIpAddr& Addr);
	/**
	 * Places the socket into a state to listen for incoming connections
	 *
	 * @param MaxBacklog the number of connections to queue before refusing them
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Listen(INT MaxBacklog);
	/**
	 * Queries the socket to determine if there is a pending connection
	 *
	 * @param bHasPendingConnection out parameter indicating whether a connection is pending or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL HasPendingConnection(UBOOL& bHasPendingConnection);
	/**
	 * Queries the socket to determine if there is pending data on the queue
	 *
	 * @param PendingDataSize out parameter indicating how much data is on the pipe for a single recv call
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL HasPendingData(UINT& PendingDataSize);
	/**
	 * Accepts a connection that is pending
	 *
	 * @param		SocketDescription debug description of socket
	 *
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(const FString& SocketDescription);
	/**
	 * Accepts a connection that is pending
	 *
	 * @param OutAddr the address of the connection
	 * @param		SocketDescription debug description of socket
	 *
	 * @return		The new (heap-allocated) socket, or NULL if unsuccessful.
	 */
	virtual FSocket* Accept(FInternetIpAddr& OutAddr,const FString& SocketDescription);
	/**
	 * Sends a buffer to a network byte ordered address
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 * @param Destination the network byte ordered address to send to
	 */
	virtual UBOOL SendTo(const BYTE* Data,INT Count,INT& BytesSent,const FInternetIpAddr& Destination);
	/**
	 * Sends a buffer on a connected socket
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 */
	virtual UBOOL Send(const BYTE* Data,INT Count,INT& BytesSent);
	/**
	 * Reads a chunk of data from the socket. Gathers the source address too
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 * @param Source out param receiving the address of the sender of the data
	 */
	virtual UBOOL RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,FInternetIpAddr& Source);
	/**
	 * Reads a chunk of data from a connected socket
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 */
	virtual UBOOL Recv(BYTE* Data,INT BufferSize,INT& BytesRead);
	/**
	 * Determines the connection state of the socket
	 */
	virtual ESocketConnectionState GetConnectionState(void);
	/**
	 * Reads the address the socket is bound to and returns it
	 */
	virtual FInternetIpAddr GetAddress(void);
	/**
	 * Sets this socket into non-blocking mode
	 *
	 * @param bIsNonBlocking whether to enable broadcast or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetNonBlocking(UBOOL bIsNonBlocking = TRUE);
	/**
	 * Sets a socket into broadcast mode (UDP only)
	 *
	 * @param bAllowBroadcast whether to enable broadcast or not
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL SetBroadcast(UBOOL bAllowBroadcast = TRUE);
	/**
	 * Sets whether a socket can be bound to an address in use
	 *
	 * @param bAllowReuse whether to allow reuse or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReuseAddr(UBOOL bAllowReuse = TRUE);
	/**
	 * Sets whether and how long a socket will linger after closing
	 *
	 * @param bShouldLinger whether to have the socket remain open for a time period after closing or not
	 * @param Timeout the amount of time to linger before closing
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetLinger(UBOOL bShouldLinger = TRUE,INT Timeout = 0);
	/**
	 * Enables error queue support for the socket
	 *
	 * @param bUseErrorQueue whether to enable error queueing or not
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetRecvErr(UBOOL bUseErrorQueue = TRUE);
	/**
	 * Sets the size of the send buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetSendBufferSize(INT Size,INT& NewSize);
	/**
	 * Sets the size of the receive buffer to use
	 *
	 * @param Size the size to change it to
	 * @param NewSize the out value returning the size that was set (in case OS can't set that)
	 *
	 * @return TRUE if the call succeeded, FALSE otherwise
	 */
	virtual UBOOL SetReceiveBufferSize(INT Size,INT& NewSize);
	/**
	 * Reads the port this socket is bound to.
	 */ 
	virtual INT GetPortNo(void);
	/**
	 * Fetches the IP address that generated the error
	 *
	 * @param FromAddr the out param getting the address
	 *
	 * @return TRUE if succeeded, FALSE otherwise
	 */
	virtual UBOOL GetErrorOriginatingAddress(FInternetIpAddr& FromAddr)
	{
		return TRUE;
	}
};

/**
 * This socket class can only send/receive data using the nat traversal protocol
 */
class FSocketPS3UdpP2p :
	public FSocketPS3
{
public:
	/**
	 * Assigns a UDPP2P PS3 socket to this object
	 *
	 * @param InSocket the socket to assign to this object
	 * @param InSocketType the type of socket that was created
	 * @param InSocketDescription debug description of socket
	 */
	FSocketPS3UdpP2p(SOCKET InSocket,ESocketType InSocketType,const FString& InSocketDescription) :
		FSocketPS3(InSocket,InSocketType,InSocketDescription)
	{
	}
	/**
	 * Binds a socket to a network byte ordered address
	 *
	 * @param Addr the address to bind to
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL Bind(const FInternetIpAddr& Addr);
	/**
	 * Reads a chunk of data from the socket. Gathers the source address too
	 *
	 * @param Data the buffer to read into
	 * @param BufferSize the max size of the buffer
	 * @param BytesRead out param indicating how many bytes were read from the socket
	 * @param Source out param receiving the address of the sender of the data
	 */
	virtual UBOOL RecvFrom(BYTE* Data,INT BufferSize,INT& BytesRead,FInternetIpAddr& Source);
	/**
	 * Sends a buffer to a network byte ordered address
	 *
	 * @param Data the buffer to send
	 * @param Count the size of the data to send
	 * @param BytesSent out param indicating how much was sent
	 * @param Destination the network byte ordered address to send to
	 */
	virtual UBOOL SendTo(const BYTE* Data,INT Count,INT& BytesSent,const FInternetIpAddr& Destination);
};


/**
 * PS3 specific socket subsystem implementation
 */
class FSocketSubsystemPS3 :
	public FSocketSubsystem
{
	/** Whether to enable nat traversal or not */
	UBOOL bShouldUseNatTraversal;

public:
	/** Zeroes members */
	FSocketSubsystemPS3(void) :
		bShouldUseNatTraversal(FALSE)
	{
	}

	/**
	 * Does per platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return TRUE if initialized ok, FALSE otherwise
	 */
	virtual UBOOL Initialize(FString& Error);
	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Destroy(void);
	/**
	 * Creates a data gram socket
	 *
 	 * @param SocketDescription debug description
	 * @param bShouldForceUDP whether UDP should be forced on or if UDPP2P should be the default
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateDGramSocket(const FString& SocketDescription, UBOOL bShouldForceUDP);
	/**
	 * Creates a stream socket
	 *
 	 * @param SocketDescription debug description
	 *
	 * @return the new socket or NULL if failed
	 */
	virtual FSocket* CreateStreamSocket(const FString& SockedDescription);
	/**
	 * Cleans up a socket class
	 *
	 * @param Socket the socket object to destroy
	 */
	virtual void DestroySocket(FSocket* Socket);
	/**
	 * Returns a human readable string from an error code
	 *
	 * @param Code the error code to check
	 */
	virtual const TCHAR* GetSocketError(INT Code = -1);
	/**
	 * Returns the last error that has happened
	 */
	virtual INT GetLastErrorCode(void)
	{
		return sys_net_errno;
	}
	/**
	 * Does a DNS look up of a host name
	 *
	 * @param HostName the name of the host to look up
	 * @param Addr the address to copy the IP address to
	 */
	virtual INT GetHostByName(ANSICHAR* HostName,FInternetIpAddr& Addr);
	/**
	 * Some platforms require chat data (voice, text, etc.) to be placed into
	 * packets in a special way. This function tells the net connection
	 * whether this is required for this platform
	 */
	virtual UBOOL RequiresChatDataBeSeparate(void)
	{
		return FALSE;
	}
	/**
	 * Determines the name of the local machine
	 *
	 * @param HostName the string that receives the data
	 *
	 * @return TRUE if successful, FALSE otherwise
	 */
	virtual UBOOL GetHostName(FString& HostName);
	/**
	 * Uses the platform specific look up to determine the host address
	 *
	 * @param Out the output device to log messages to
	 * @param HostAddr the out param receiving the host address
	 *
	 * @return TRUE if all can be bound (no primarynet), FALSE otherwise
	 */
	virtual UBOOL GetLocalHostAddr(FOutputDevice& Out,FInternetIpAddr& HostAddr);
	/**
	 * @return Whether the machine has a properly configured network device or not
	 */
	virtual UBOOL HasNetworkDevice(void);
};


/**
 * Start the async task to make sure user is connected to NP (and therefore
 * gamespy if so set up)
 */
void appPS3BeginNetworkStartUtility();

/**
 * Tick the async network start utility
 *
 * @param bIsConnected [out] TRUE if the user was connected by the end of the startup process. Only valid if this function returns TRUE
 *
 * @return The async operation was finished, now bIsConnected is valid, and ticking can stop
 */
UBOOL appPS3TickNetworkStartUtility(UBOOL& bIsConnected);

#endif

#endif	//#if WITH_UE3_NETWORKING

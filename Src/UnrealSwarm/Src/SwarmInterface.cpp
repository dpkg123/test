/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * SwarmInterface.cpp - Implementation of the C++ and C++/CLR interfaces to
 * the SwarmAgent.
 */

#if _WIN64
#pragma pack ( push, 8 )
#else
#pragma pack ( push, 4 )
#endif

// Define WIN32_LEAN_AND_MEAN to exclude rarely-used services from windows headers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <vcclr.h>

// All CLR includes and usings
#using <System.Runtime.Remoting.dll>
#using <System.Windows.Forms.dll>
#using <System.dll>

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Diagnostics;
using namespace System::Drawing;
using namespace System::IO;
using namespace System::IO::Compression;
using namespace System::Runtime::Remoting;
using namespace System::Runtime::Remoting::Channels;
using namespace System::Runtime::Remoting::Channels::Tcp;
using namespace System::Runtime::Remoting::Messaging;
using namespace System::Threading;
using namespace System::Windows::Forms;

using namespace AgentInterface;

using System::Collections::Hashtable;

#include "SwarmDefines.h"
#include "SwarmInterface.h"
#include "Util.h"

namespace NSwarm
{

///////////////////////////////////////////////////////////////////////////////

/*
 * PerfTiming
 *
 * An instance of a timing object. It tracks a single timing; the total time and the number of calls
 */
ref class PerfTiming
{
public:
	String^					Name;
	Diagnostics::Stopwatch^	StopWatchTimer;
	Int32					Count;
	UBOOL					Accumulate;
	Int64					Counter;

	PerfTiming( String^ InName, UBOOL InAccumulate )
	{
		Name = InName;
		StopWatchTimer = gcnew Diagnostics::Stopwatch();
		Count = 0;
		Accumulate = InAccumulate;
		Counter = 0;
	}

	void Start( void )
	{
		StopWatchTimer->Start();
	}

	void Stop( void )					
	{
		Count++;
		StopWatchTimer->Stop();
	}

	void IncrementCounter( Int64 Adder )
	{
		Counter += Adder;
	}
};

/*
 * PerfTimer
 *
 * Tracks a dictionary of PerfTimings
 */
ref class PerfTimer
{
public:
	PerfTimer( void );
	~PerfTimer( void ) {}

	double											Frequency;
	Stack<PerfTiming^>^								LastTimers;
	ReaderWriterDictionary<String^, PerfTiming^>^	Timings;

	void Start( String^ Name, UBOOL Accum, Int64 Adder );
	void Stop( void );
	String^ DumpTimings( void );
};

PerfTimer::PerfTimer( void )
{
	Timings = gcnew ReaderWriterDictionary<String^, PerfTiming^>();
	LastTimers = gcnew Stack<PerfTiming^>();
}

void PerfTimer::Start( String^ Name, UBOOL Accum, Int64 Adder )
{
	Monitor::Enter( LastTimers );

	PerfTiming^ Timing = nullptr;
	if( !Timings->TryGetValue( Name, Timing ) )
	{
		Timing = gcnew PerfTiming( Name, Accum );
		Timings->Add( Name, Timing );
	}

	LastTimers->Push( Timing );

	Timing->IncrementCounter( Adder );
	Timing->Start();

	Monitor::Exit( LastTimers );
}

void PerfTimer::Stop( void )
{
	Monitor::Enter( LastTimers );

	PerfTiming^ Timing = LastTimers->Pop();
	Timing->Stop();

	Monitor::Exit( LastTimers );
}

String^ PerfTimer::DumpTimings( void )
{
	String^ Output = gcnew String( "" );

	double TotalTime = 0.0;
	for each( PerfTiming^ Timing in Timings->Values )
	{
		if( Timing->Count > 0 )
		{
			double Elapsed = Timing->StopWatchTimer->Elapsed.TotalSeconds;
			double Average = ( Elapsed * 1000000.0 ) / Timing->Count;
			Output += gcnew String( Timing->Name->PadLeft( 30 ) + " : " + Elapsed.ToString( "F3" ) + "s in " + Timing->Count + " calls (" + Average.ToString( "F0" ) + "us per call)" );

			if( Timing->Counter > 0 )
			{
				Output += " (" + ( Timing->Counter / 1024 ) + "k)";
			}

			Output += "\n";
			if( Timing->Accumulate )
			{
				TotalTime += Elapsed;
			}
		}
	}

	Output += "\nTotal time inside Swarm: " + TotalTime.ToString( "F3" ) + "s\n";

	return( Output );
}

///////////////////////////////////////////////////////////////////////////////

/**
 * Connection configuration parameters, filled in by OpenConnection
 */
ref class AgentConfiguration
{
public:
	AgentConfiguration()
	{
		AgentProcessID = -1;
		AgentCachePath = nullptr;
		AgentJobGuid = nullptr;
		IsPureLocalConnection = false;
	}

	// Process ID of the owning process for the agent, which can be used to
	// monitor it for crashes or hangs
	Int32			AgentProcessID;

	// The full path of the cache directory this agent is using
	String^			AgentCachePath;

	// The GUID of the job the agent has associated this connection with, if any
	AgentGuid^		AgentJobGuid;

	// An indication of whether we're considered a "pure" local connection by the
	// Agent, potentially relieving us from Agent coordination when using Channels
	bool			IsPureLocalConnection;
};

///////////////////////////////////////////////////////////////////////////////

/**
 * A wrapper to abstract away the complexity of asynchronous calls for the
 * Agent API and the monitoring of the connection
 */
ref class IAgentInterfaceWrapper
{
public:
	IAgentInterfaceWrapper( void )
	{
		// TODO: Make URL configurable
		Connection = ( IAgentInterface^ )Activator::GetObject( IAgentInterface::typeid, "tcp://127.0.0.1:8008/SwarmAgent" );
		ConnectionDroppedEvent = gcnew ManualResetEvent( false );
	}
	virtual ~IAgentInterfaceWrapper( void )
	{
	}

	void SignalConnectionDropped()
	{
		ConnectionDroppedEvent->Set();
	}

	///////////////////////////////////////////////////////////////////////////

	// A duplication of the IAgentInterface API which this class wraps
	Int32 OpenConnection( Process^ AgentProcess, UBOOL AgentProcessOwner, Int32 LocalProcessID, ELogFlags LoggingFlags, AgentConfiguration^% NewConfiguration )
	{
		OpenConnectionDelegate^ DOpenConnection = gcnew OpenConnectionDelegate( Connection, &IAgentInterface::OpenConnection );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["ProcessID"] = LocalProcessID;
		InParameters["ProcessIsOwner"] = ( AgentProcessOwner == TRUE ? true : false );
		InParameters["LoggingFlags"] = LoggingFlags;

		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DOpenConnection->BeginInvoke( InParameters, OutParameters, nullptr, nullptr );
		
		// This will wait with an occasional wake up to check to see if the
		// agent process is still alive and kicking (avoids infinite wait,
		// allows for very long start up times while debugging)
		INT StartupSleep = 1000;
		while( ( Result->AsyncWaitHandle->WaitOne( StartupSleep ) == false ) &&
			   ( AgentProcess->HasExited == false ) &&
			   ( AgentProcess->Responding == true ) )
		{
			// While the application is alive and responding, wait
			Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Waiting for agent to respond ..." );
		}

		if( Result->IsCompleted )
		{
			// If the invocation didn't fail, end to get the result
			Int32 ReturnValue = DOpenConnection->EndInvoke( OutParameters, Result );
			if( OutParameters != nullptr )
			{
				if( ( Int32 )OutParameters["Version"] == VERSION_1_0 )
				{
					NewConfiguration = gcnew AgentConfiguration();
					NewConfiguration->AgentProcessID = ( Int32 )OutParameters["AgentProcessID"];
					NewConfiguration->AgentCachePath = ( String^ )OutParameters["AgentCachePath"];
					NewConfiguration->AgentJobGuid = ( AgentGuid^ )OutParameters["AgentJobGuid"];

					if( OutParameters->ContainsKey( "IsPureLocalConnection" ) )
					{
						NewConfiguration->IsPureLocalConnection = ( bool )OutParameters["IsPureLocalConnection"];
					}

					// Complete and successful
					return ReturnValue;
				}
			}
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 CloseConnection( Int32 ConnectionHandle )
	{
		CloseConnectionDelegate^ DCloseConnection = gcnew CloseConnectionDelegate( Connection, &IAgentInterface::CloseConnection );

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ InParameters = nullptr;
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DCloseConnection->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DCloseConnection->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 SendMessage( Int32 ConnectionHandle, AgentMessage^ NewMessage )
	{
		SendMessageDelegate^ DSendMessage = gcnew SendMessageDelegate( Connection, &IAgentInterface::SendMessage );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["Message"] = NewMessage;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DSendMessage->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DSendMessage->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 GetMessage( Int32 ConnectionHandle, AgentMessage^% NextMessage, Int32 Timeout )
	{
		GetMessageDelegate^ DGetMessage = gcnew GetMessageDelegate( Connection, &IAgentInterface::GetMessage );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["Timeout"] = ( Int32 )Timeout;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DGetMessage->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation didn't fail, end to get the result
			Int32 ReturnValue = DGetMessage->EndInvoke( OutParameters, Result );
			if( OutParameters != nullptr )
			{
				if( ( Int32 )OutParameters["Version"] == VERSION_1_0 )
				{
					NextMessage = ( AgentMessage^ )OutParameters["Message"];

					// Complete and successful
					return ReturnValue;
				}
			}
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 AddChannel( Int32 ConnectionHandle, String^ FullPath, String^ ChannelName )
	{
		AddChannelDelegate^ DAddChannel = gcnew AddChannelDelegate( Connection, &IAgentInterface::AddChannel );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["FullPath"] = FullPath;
		InParameters["ChannelName"] = ChannelName;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DAddChannel->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DAddChannel->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 TestChannel( Int32 ConnectionHandle, String^ ChannelName )
	{
		TestChannelDelegate^ DTestChannel = gcnew TestChannelDelegate( Connection, &IAgentInterface::TestChannel );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["ChannelName"] = ChannelName;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DTestChannel->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DTestChannel->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 OpenChannel( Int32 ConnectionHandle, String^ ChannelName, EChannelFlags ChannelFlags )
	{
		OpenChannelDelegate^ DOpenChannel = gcnew OpenChannelDelegate( Connection, &IAgentInterface::OpenChannel );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["ChannelName"] = ChannelName;
		InParameters["ChannelFlags"] = ChannelFlags;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DOpenChannel->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DOpenChannel->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 CloseChannel( Int32 ConnectionHandle, Int32 ChannelHandle )
	{
		CloseChannelDelegate^ DCloseChannel = gcnew CloseChannelDelegate( Connection, &IAgentInterface::CloseChannel );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["ChannelHandle"] = ChannelHandle;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DCloseChannel->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DCloseChannel->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 OpenJob( Int32 ConnectionHandle, AgentGuid^ JobGuid )
	{
		OpenJobDelegate^ DOpenJob = gcnew OpenJobDelegate( Connection, &IAgentInterface::OpenJob );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["JobGuid"] = JobGuid;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DOpenJob->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DOpenJob->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 BeginJobSpecification( Int32 ConnectionHandle, AgentJobSpecification^ Specification32, Hashtable^ Description32, AgentJobSpecification^ Specification64, Hashtable^ Description64 )
	{
		BeginJobSpecificationDelegate^ DBeginJobSpecification = gcnew BeginJobSpecificationDelegate( Connection, &IAgentInterface::BeginJobSpecification );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["Specification32"] = Specification32;
		InParameters["Specification64"] = Specification64;
		InParameters["Description32"] = Description32;
		InParameters["Description64"] = Description64;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DBeginJobSpecification->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DBeginJobSpecification->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 AddTask( Int32 ConnectionHandle, List<AgentTaskSpecification^>^ Specifications )
	{
		AddTaskDelegate^ DAddTask = gcnew AddTaskDelegate( Connection, &IAgentInterface::AddTask );

		// Set up the versioned hashtable input parameters
		Hashtable^ InParameters = gcnew Hashtable();
		InParameters["Version"] = ( Int32 )VERSION_1_0;
		InParameters["Specifications"] = Specifications;

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DAddTask->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DAddTask->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 EndJobSpecification( Int32 ConnectionHandle )
	{
		EndJobSpecificationDelegate^ DEndJobSpecification = gcnew EndJobSpecificationDelegate( Connection, &IAgentInterface::EndJobSpecification );

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ InParameters = nullptr;
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DEndJobSpecification->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DEndJobSpecification->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 CloseJob( Int32 ConnectionHandle )
	{
		CloseJobDelegate^ DCloseJob = gcnew CloseJobDelegate( Connection, &IAgentInterface::CloseJob );

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		Hashtable^ InParameters = nullptr;
		Hashtable^ OutParameters = nullptr;
		IAsyncResult^ Result = DCloseJob->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DCloseJob->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 Method( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters )
	{
		MethodDelegate^ DMethod = gcnew MethodDelegate( Connection, &IAgentInterface::Method );

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		IAsyncResult^ Result = DMethod->BeginInvoke( ConnectionHandle, InParameters, OutParameters, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DMethod->EndInvoke( OutParameters, Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

	Int32 Log( EVerbosityLevel Verbosity, Color TextColour, String^ Line )
	{
		LogDelegate^ DLog = gcnew LogDelegate( Connection, &IAgentInterface::Log );

		// Invoke the method, then wait for it to finish or to be notified that the connection dropped
		IAsyncResult^ Result = DLog->BeginInvoke( Verbosity, TextColour, Line, nullptr, nullptr );
		WaitHandle::WaitAny( gcnew array<WaitHandle^>( 2 ){ Result->AsyncWaitHandle, ConnectionDroppedEvent } );

		// If the method completed normally, return the result
		if( Result->IsCompleted )
		{
			// If the invocation completed, success
			return DLog->EndInvoke( Result );
		}
		// Otherwise, error
		return Constants::ERROR_CONNECTION_DISCONNECTED;
	}

	///////////////////////////////////////////////////////////////////////////

private:
	// The wrapped connection
	IAgentInterface^	Connection;
	ManualResetEvent^	ConnectionDroppedEvent;

	// Delegate type declarations
	delegate Int32 OpenConnectionDelegate( Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 CloseConnectionDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 SendMessageDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 GetMessageDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 AddChannelDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 TestChannelDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 OpenChannelDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 CloseChannelDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 OpenJobDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 BeginJobSpecificationDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 AddTaskDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 EndJobSpecificationDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 CloseJobDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 MethodDelegate( Int32 ConnectionHandle, Hashtable^ InParameters, Hashtable^% OutParameters );
	delegate Int32 LogDelegate( EVerbosityLevel Verbosity, Color TextColour, String^ Line );
};

///////////////////////////////////////////////////////////////////////////////

/**
 * The managed C++ implementation of FSwarmInterface
 */
ref class FSwarmInterfaceManagedImpl
{
public:
	FSwarmInterfaceManagedImpl( void );
	virtual ~FSwarmInterfaceManagedImpl( void );

	/**
	 * Opens a new connection to the Swarm
	 *
	 * @param CallbackFunc The callback function Swarm will use to communicate back to the Instigator
	 *
	 * @return An INT containing the error code (if < 0) or the handle (>= 0) which is useful for debugging only
	 */
	virtual INT OpenConnection( FConnectionCallback CallbackFunc, void* CallbackData, ELogFlags LoggingFlags );

	/**
	 * Closes an existing connection to the Swarm
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT CloseConnection( void );

	/**
	 * Sends a message to an Agent (return messages are sent via the FConnectionCallback)
	 *
	 * @param Message The message being sent
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT SendMessage( const FMessage& NativeMessage );

	/**
	 * Adds an existing file to the cache. Note, any existing channel with the same
	 * name will be overwritten.
	 *
	 * @param FullPath The full path name to the file that should be copied into the cache
	 * @param ChannelName The name of the channel once it's in the cache
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT AddChannel( const TCHAR* FullPath, const TCHAR* ChannelName );

	/**
	 * Determines if the named channel is in the cache
	 *
	 * @param ChannelName The name of the channel to look for
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT TestChannel( const TCHAR* ChannelName );

	/**
	 * Opens a data channel for streaming data into the cache associated with an Agent
	 *
	 * @param ChannelName The name of the channel being opened
	 * @param ChannelFlags The mode, access, and other attributes of the channel being opened
	 *
	 * @return A handle to the opened channel (< 0 is error)
	 */
	virtual INT OpenChannel( const TCHAR* ChannelName, TChannelFlags ChannelFlags );

	/**
	 * Closes an open channel
	 *
	 * @param Channel An open channel handle, returned by OpenChannel
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT CloseChannel( INT Channel );

	/**
	 * Writes the provided data to the open channel opened for WRITE
	 *
	 * @param Channel An open channel handle, returned by OpenChannel
	 * @param Data Source buffer for the write
	 * @param Data Size of the source buffer
	 *
	 * @return The number of bytes written (< 0 is error)
	 */
	virtual INT WriteChannel( INT Channel, const void* Data, INT DataSize );

	/**
	 * Reads data from a channel opened for READ into the provided buffer
	 *
	 * @param Channel An open channel handle, returned by OpenChannel
	 * @param Data Destination buffer for the read
	 * @param Data Size of the destination buffer
	 *
	 * @return The number of bytes read (< 0 is error)
	 */
	virtual INT ReadChannel( INT Channel, void* Data, INT DataSize );

	/**
	 * Opens a Job session, which allows a Job to be specified, Tasks added, Job
	 * channels opened and used, etc. When the Job is complete and no more Job
	 * related data is needed from the Swarm, call CloseJob.
	 *
	 * @param JobGuid A GUID that uniquely identifies this Job, generated by the caller
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT OpenJob( const FGuid& JobGuid );

	/**
	 * Begins a Job specification, which allows a series of Tasks to be specified
	 * via AddTask. When Tasks are done being specified, call EndJobSpecification.
	 *
	 * The default behavior will be to execute the Job executable with the
	 * specified parameters. If Tasks are added for the Job, they are expected
	 * to be requested by the executable run for the Job. If no Tasks are added
	 * for the Job, it is expected that the Job executable will perform its
	 * operations without additional Task input from Swarm.
	 *
	 * @param Specification32 A structure describing a new 32-bit Job (can be an empty specification)
	 * @param Specification64 A structure describing a new 64-bit Job (can be an empty specification)
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT BeginJobSpecification( const FJobSpecification& Specification32, const FJobSpecification& Specification64 );

	/**
	 * Adds a Task to the current Job
	 *
	 * @param Specification A structure describing the new Task
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT AddTask( const FTaskSpecification& Specification );

	/**
	 * Ends the Job specification, after which no additional Tasks may be defined. Also,
	 * this is generally the point when the Agent will validate and launch the Job executable,
	 * potentially distributing the Job to other Agents.
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT EndJobSpecification( void );

	/**
	 * Ends the Job, after which all Job-related API usage (except OpenJob) will be rejected
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT CloseJob( void );

	/**
	 * Adds a line of text to the Agent log window
	 *
	 * @param Verbosity the importance of this message
	 * @param TextColour the colour of the text
	 * @param Message the line of text to add
	 */
	virtual INT Log( EVerbosityLevel Verbosity, Color TextColour, String^ Message );

	/**
	 * The function for the message queue monitoring thread used for
	 * calling the callback from the remote Agent
	 */
	static void MessageThreadProc( Object^ ThreadParameters );

	/**
	 * The function for the agent monitoring thread used to watch
	 * for potential crashed or hung agents
	 */
	static void MonitorThreadProc( Object^ ThreadParameters );

	/*
	 * Start a timer going
	 */
	void StartTiming( String^ Name, UBOOL Accum, Int64 Adder );
	void StartTiming( String^ Name, UBOOL Accum );

	/*
	 * Stop a timer
	 */
	void StopTiming( void );

	/*
	 * Print out all the timing info
	 */
	void DumpTimings( void );

	/*
	 * Clean up all of the remainders from a closed connection, including the network channels, etc.
	 */
	INT CleanupClosedConnection();

private:
	/** All agent/swarm-specific variables */
	Process^									AgentProcess;
	UBOOL										AgentProcessOwner;
	
	/** All connection-specific variables */
	IAgentInterfaceWrapper^						Connection;
	INT											ConnectionHandle;
	Thread^										ConnectionMessageThread;
	Thread^										ConnectionMonitorThread;
	FConnectionCallback							ConnectionCallback;
	void*										ConnectionCallbackData;
	ELogFlags									ConnectionLoggingFlags;
	AgentConfiguration^							ConnectionConfiguration;
	Object^										CleanupClosedConnectionLock;

	/** A convenience class for packing a few managed types together */
	ref class ChannelInfo
	{
	public:
		String^			ChannelName;
		TChannelFlags	ChannelFlags;
		INT				ChannelHandle;
		Stream^			ChannelFileStream;

		// For buffered reads and writes
		array<unsigned char>^	ChannelData;
		Int32					ChannelOffset;
	};

	ReaderWriterDictionary<INT, ChannelInfo^>^	OpenChannels;
	Stack<array<unsigned char>^>^				FreeChannelWriteBuffers;
	INT											BaseChannelHandle;

	/** While a job is being specified, collect the task specifications until it's done, then submit all at once */
	List<AgentTaskSpecification^>^				PendingTasks;

	/** A pair of locations needed for using channels */
	String^										AgentCacheFolder;

	/** The network channel used to communicate with the Agent */
	TcpClientChannel^							NetworkChannel;

	/** A helper class for timing info */
	PerfTimer^									PerfTimerInstance;

	INT TryOpenConnection( FConnectionCallback CallbackFunc, void* CallbackData, ELogFlags LoggingFlags );
	void EnsureAgentIsRunning();

	/**
	 * Converts an native job specification to a managed version.
	 *
	 * @param Specification	Native job specification (may be empty)
	 * @param bIs64bit		Whether the job specification is 64-bit or not
	 * @return				Newly created managed job specification, or nullptr if the specification was empty
	 */
	AgentJobSpecification^ ConvertJobSpecification( const FJobSpecification& Specification, bool bIs64bit );

	/*
	 * Generates the full channel name, including Job path and staging area path if necessary
	 */
	String^ GenerateFullChannelName( String^ ManagedChannelName, TChannelFlags ChannelFlags );

	/*
	 * CacheFileAndConvertName
	 *
	 * Checks to see if the file with the correct timestamp and size exists in the cache
	 * If it does not, add it to the cache
	 */
	INT CacheFileAndConvertName( String^ FullPathName, String^& CachedFileName, bool bUse64bitNamingConvention );

	/*
	 * CacheAllFiles
	 *
	 * Ensures all executables and dependencies are correctly placed in the cache
	 */
	INT CacheAllFiles( AgentJobSpecification^ JobSpecification );
	INT CacheAllFiles( AgentTaskSpecification^ TaskSpecification );

	/*
	 * Flushes any buffered writes to the channel
	 */
	INT FlushChannel( ChannelInfo^ ChannelToFlush );
};

/**
 * Helper struct for getting the message processing thread started
 */
ref struct MessageThreadData
{
	FSwarmInterfaceManagedImpl^	Owner;
	IAgentInterfaceWrapper^		Connection;
	INT							ConnectionHandle;
	FConnectionCallback			ConnectionCallback;
	void*						ConnectionCallbackData;
	AgentConfiguration^			ConnectionConfiguration;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSwarmInterfaceManagedImpl::FSwarmInterfaceManagedImpl( void )
	:	AgentProcess( nullptr )
	,	AgentProcessOwner( FALSE )
	,	Connection( nullptr )
	,	ConnectionHandle( Constants::INVALID )
	,	ConnectionMessageThread( nullptr )
	,	ConnectionMonitorThread( nullptr )
	,	ConnectionConfiguration( nullptr )
	,	ConnectionCallback( NULL )
	,	BaseChannelHandle( 0 )
	,	PendingTasks( nullptr )
	,	NetworkChannel( nullptr )
	,	PerfTimerInstance( nullptr )
{
	OpenChannels = gcnew ReaderWriterDictionary<INT, ChannelInfo^>();
	FreeChannelWriteBuffers = gcnew Stack<array<unsigned char>^>();
	CleanupClosedConnectionLock = gcnew Object();

	// TODO: Delete old files
}

FSwarmInterfaceManagedImpl::~FSwarmInterfaceManagedImpl( void )
{
}

INT FSwarmInterfaceManagedImpl::CleanupClosedConnection( void )
{
	Monitor::Enter( CleanupClosedConnectionLock );

	// NOTE: Do not make any calls to the real Connection in here!
	// If, for any reason, the connection has died, calling into it from here will
	// end up causing this thread to hang, waiting for the dead connection to respond.
	Debug::WriteLineIf( Debugger::IsAttached, "[Interface:CleanupClosedConnection] Closing all connections to the Agent" );

	// Reset all necessary assigned variables
	AgentProcess = nullptr;
	AgentProcessOwner = FALSE;

	if( Connection != nullptr )
	{
		// Notify the connection wrapper that the connection is gone
		Connection->SignalConnectionDropped();
		Connection = nullptr;
	}
	ConnectionHandle = Constants::INVALID;
	ConnectionConfiguration = nullptr;

	// Clean up and close up the connection callback
	if( ConnectionCallback != NULL )
	{
		FMessage QuitMessage( VERSION_1_0, MESSAGE_QUIT );
		ConnectionCallback( &QuitMessage, ConnectionCallbackData );
	}
	ConnectionCallback = NULL;
	ConnectionCallbackData = NULL;

	// Close all associated channels
	if( OpenChannels->Count > 0 )
	{
		for each( ChannelInfo^ NextChannel in OpenChannels->Values )
		{
			// Just close the handle and we'll clear the entire list later
			if( NextChannel->ChannelFileStream != nullptr )
			{
				NextChannel->ChannelFileStream->Close();
				NextChannel->ChannelFileStream = nullptr;
			}
		}
		OpenChannels->Clear();
	}

	// Unregister the primary network communication channel
	if( NetworkChannel != nullptr )
	{
		ChannelServices::UnregisterChannel( NetworkChannel );
		NetworkChannel = nullptr;
	}

	Monitor::Exit( CleanupClosedConnectionLock );

	return Constants::SUCCESS;
}

INT FSwarmInterfaceManagedImpl::TryOpenConnection( FConnectionCallback CallbackFunc, void* CallbackData, ELogFlags LoggingFlags )
{
	try
	{
		// Allocate our new connection wrapper object
		Connection = gcnew IAgentInterfaceWrapper();

		// Make sure the agent is alive and responsive before continuing
		Debug::WriteLineIf( Debugger::IsAttached, "[TryOpenConnection] Testing the Agent" );
		Hashtable^ InParameters = nullptr;
		Hashtable^ OutParameters = nullptr;
		bool AgentIsReady = false;
		while( !AgentIsReady )
		{
			try
			{
				// Simply try to call the method and if it doesn't throw
				// an exception, consider it a success
				Connection->Method( 0, InParameters, OutParameters );
				AgentIsReady = true;
			}
			catch( Exception^ )
			{
				// Wait a little longer
				Debug::WriteLineIf( Debugger::IsAttached, "[TryOpenConnection] Waiting for the agent to start up ..." );
				Thread::Sleep( 500 );
			}
		}

		// Request an official connection to the Agent
		Debug::WriteLineIf( Debugger::IsAttached, "[TryOpenConnection] Opening Connection to Agent" );
		Debug::WriteLineIf( Debugger::IsAttached, "[TryOpenConnection] Local Process ID is " + Process::GetCurrentProcess()->Id.ToString() );

		StartTiming( gcnew String( "OpenConnection-Remote" ), FALSE );
		ConnectionHandle = Connection->OpenConnection( AgentProcess, AgentProcessOwner, Process::GetCurrentProcess()->Id, LoggingFlags, ConnectionConfiguration );
		StopTiming();

		if( ConnectionHandle >= 0 )
		{
			Log( EVerbosityLevel::Informative, Color::DarkGreen, "[Interface:TryOpenConnection] Local connection established" );

			// Spawn a thread to monitor the message queue
			MessageThreadData^ ThreadData = gcnew MessageThreadData();
			ThreadData->Owner = this;
			ThreadData->Connection = Connection;
			ThreadData->ConnectionHandle = ConnectionHandle;
			ThreadData->ConnectionCallback = CallbackFunc;
			ThreadData->ConnectionCallbackData = CallbackData;
			ThreadData->ConnectionConfiguration = ConnectionConfiguration;

			// Launch the message queue thread
			ConnectionMessageThread = gcnew Thread( gcnew ParameterizedThreadStart( &FSwarmInterfaceManagedImpl::MessageThreadProc ) );
			ConnectionMessageThread->Name = "ConnectionMessageThread";
			ConnectionMessageThread->Start( ThreadData );

			// Launch the agent monitor thread
			ConnectionMonitorThread = gcnew Thread( gcnew ParameterizedThreadStart( &FSwarmInterfaceManagedImpl::MonitorThreadProc ) );
			ConnectionMonitorThread->Name = "ConnectionMonitorThread";
			ConnectionMonitorThread->Start( ThreadData );

			// Save the user's callback routine
			ConnectionCallback = CallbackFunc;
			ConnectionCallbackData = CallbackData;
			ConnectionLoggingFlags = LoggingFlags;
		}
	}
	catch( Exception^ Ex )
	{
		Debug::WriteLineIf( Debugger::IsAttached, "[TryOpenConnection] Error: " + Ex->Message );
		ConnectionHandle = Constants::INVALID;
		Connection = nullptr;
	}

	return( ConnectionHandle );
}

void FSwarmInterfaceManagedImpl::EnsureAgentIsRunning()
{
	// See if an agent is already running, and if not, launch one
	AgentProcess = nullptr;
	AgentProcessOwner = FALSE;

	array<Process^>^ RunningSwarmAgents = Process::GetProcessesByName( "SwarmAgent" );
	if( RunningSwarmAgents->Length > 0 )
	{
		// If any are running, just grab the first one
		AgentProcess = RunningSwarmAgents[0];
	}

	if( AgentProcess == nullptr )
	{
		// If none, launch the Agent binary
		Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Spawning agent: " + Application::StartupPath + "\\SwarmAgent.exe" );
		if( File::Exists( Application::StartupPath + "\\..\\SwarmAgent.exe" ) )
		{
			AgentProcess = Process::Start( Application::StartupPath + "\\..\\SwarmAgent.exe" );
			if( AgentProcess == nullptr )
			{
				Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Failed to start the Agent executable: " + Application::StartupPath + "\\SwarmAgent.exe" );
			}
			else
			{
				// If we started the process, we own it and control its lifetime
				AgentProcessOwner = TRUE;
			}
		}
		else
		{
			Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Failed to find the Agent executable: " + Application::StartupPath + "\\SwarmAgent.exe" );
		}
	}
}

/**
 * Opens a new connection to the Swarm
 *
 * @param CallbackFunc The callback function Swarm will use to communicate back to the Instigator
 *
 * @return An INT containing the error code (if < 0) or the handle (>= 0) which is useful for debugging only
 */
INT FSwarmInterfaceManagedImpl::OpenConnection( FConnectionCallback CallbackFunc, void* CallbackData, ELogFlags LoggingFlags )
{
	// Checked here so we can time OpenConnection
	if( ( LoggingFlags & ELogFlags::LOG_TIMINGS ) == ELogFlags::LOG_TIMINGS )
	{
		PerfTimerInstance = gcnew PerfTimer();
	}

	StartTiming( gcnew String( "OpenConnection-Managed" ), TRUE );

	// Establish a connection to the local Agent server object
	ConnectionHandle = Constants::INVALID;
	INT ReturnValue = Constants::INVALID;
	try
	{
		Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Registering TCP channel ..." );

		// Start up network services, by opening a network communication channel
		NetworkChannel = gcnew TcpClientChannel();
		ChannelServices::RegisterChannel( NetworkChannel, false );

		// See if an agent is already running, and if not, launch one
		EnsureAgentIsRunning();
		if( AgentProcess != nullptr )
		{
			Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Connecting to agent ..." );
			ReturnValue = TryOpenConnection( CallbackFunc, CallbackData, LoggingFlags );
			if( ReturnValue >= 0 )
			{
				AgentCacheFolder = ConnectionConfiguration->AgentCachePath;
				if( AgentCacheFolder->Length == 0 )
				{
					CloseConnection();
					ReturnValue = Constants::ERROR_FILE_FOUND_NOT;
				}
			}
		}
	}
	catch( Exception^ Ex )
	{
		Debug::WriteLineIf( Debugger::IsAttached, "[OpenConnection] Error: " + Ex->Message );
		ReturnValue = Constants::ERROR_EXCEPTION;
	}

	// Finally, if there have been no errors, assign the connection handle 
	if( ReturnValue >= 0 )
	{
		ConnectionHandle = ReturnValue;
	}
	else
	{
		// If we've failed for any reason, call the clean up routine
		CleanupClosedConnection();
	}

	StopTiming();
	return( ConnectionHandle );
}

/**
 * Closes an existing connection to the Swarm
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::CloseConnection( void )
{
	// Dump any collected timing info
	DumpTimings();

	// Close the connection
	StartTiming( gcnew String( "CloseConnection-Managed" ), TRUE );

	INT ConnectionState = Constants::INVALID;
	if( Connection != nullptr )
	{
		try
		{
			StartTiming( gcnew String( "CloseConnection-Remote" ), FALSE );
			Connection->CloseConnection( ConnectionHandle );
			StopTiming();

			ConnectionState = Constants::SUCCESS;
		}
		catch( Exception^ Ex )
		{
			ConnectionState = Constants::ERROR_EXCEPTION;
			Debug::WriteLineIf( Debugger::IsAttached, "[CloseConnection] Error: " + Ex->Message );
		}

		// Clean up the state of the object with the connection now closed
		CleanupClosedConnection();

		// With the connecton completely closed, clean up our threads
		if( ConnectionMessageThread->Join( 1000 ) == false )
		{
			// After calling CloseConnection, this thread is fair game to kill at any time
			Debug::WriteLineIf( Debugger::IsAttached, "[CloseConnection] Error: Message queue thread failed to quit before timeout, killing." );
			ConnectionMessageThread->Abort();
		}
		ConnectionMessageThread = nullptr;
		if( ConnectionMonitorThread->Join( 0 ) == false )
		{
			// We expect to abort this thread, no message necessary
			ConnectionMonitorThread->Abort();
		}
		ConnectionMonitorThread = nullptr;
	}
	else
	{
		ConnectionState = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ConnectionState );
}

/**
 * Adds an existing file to the cache. Note, any existing channel with the same
 * name will be overwritten.
 *
 * @param FullPath The full path name to the file that should be copied into the cache
 * @param ChannelName The name of the channel once it's in the cache
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::AddChannel( const TCHAR* FullPath, const TCHAR* ChannelName )
{
	StartTiming( gcnew String( "AddChannel-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		String^ ManagedFullPath = gcnew String( FullPath );
		String^ ManagedChannelName = gcnew String( ChannelName );
		try
		{
			StartTiming( gcnew String( "AddChannel-Remote" ), FALSE );
			ReturnValue = Connection->AddChannel( ConnectionHandle, ManagedFullPath, ManagedChannelName );
			StopTiming();
		}
		catch( Exception^ Ex )
		{
			Log( EVerbosityLevel::Critical, Color::Red, "[Interface:AddChannel] Error: " + Ex->Message );
			ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
			CleanupClosedConnection();
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/*
 * Generates the full channel name, including Job path and staging area path if necessary
 */
String^ FSwarmInterfaceManagedImpl::GenerateFullChannelName( String^ ManagedChannelName, TChannelFlags ChannelFlags )
{
	if( ChannelFlags & SWARM_CHANNEL_TYPE_PERSISTENT )
	{
		if( ChannelFlags & SWARM_CHANNEL_ACCESS_WRITE )
		{
			// A persistent cache channel open for writing opens in the staging area
			String^ StagingAreaName = Path::Combine( AgentCacheFolder, "AgentStagingArea" );
			return Path::Combine( StagingAreaName, ManagedChannelName );
		}
		else if( ChannelFlags & SWARM_CHANNEL_ACCESS_READ )
		{
			// A persistent cache channel open for reading opens directly in the cache
			return Path::Combine( AgentCacheFolder, ManagedChannelName );
		}
	}
	else if( ChannelFlags & SWARM_CHANNEL_TYPE_JOB_ONLY )
	{
		AgentGuid^ JobGuid = ConnectionConfiguration->AgentJobGuid;
		if( JobGuid == nullptr )
		{
			// If there's no Job associated with this connection at this point, provide
			// a default GUID for one for debugging access to the agent cache
			JobGuid = gcnew AgentGuid( 0x00000123, 0x00004567, 0x000089ab, 0x0000cdef );
		}

		// A Job Channel opens directly in the Job-specific directory
		String^ JobsFolder = Path::Combine( AgentCacheFolder, "Jobs" );
		String^ JobFolderName = Path::Combine( JobsFolder, "Job-" + JobGuid->ToString() );
		return Path::Combine( JobFolderName, ManagedChannelName );
	}

	return gcnew String( "" );
}

/**
 * Determines if the named channel is in the cache
 *
 * @param ChannelName The name of the channel to look for
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::TestChannel( const TCHAR* ChannelName )
{
	StartTiming( gcnew String( "TestChannel-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		String^ ManagedChannelName = gcnew String( ChannelName );
		try
		{
			if( !ConnectionConfiguration->IsPureLocalConnection )
			{
				StartTiming( gcnew String( "TestChannel-Remote" ), FALSE );
				ReturnValue = Connection->TestChannel( ConnectionHandle, ManagedChannelName );
				StopTiming();
			}
			else
			{
				// Testing a channel only tests the main, persistent cache for files to read
				TChannelFlags ChannelFlags = ( TChannelFlags )( SWARM_CHANNEL_TYPE_PERSISTENT |  SWARM_CHANNEL_ACCESS_READ );
				String^ FullManagedName = GenerateFullChannelName( ManagedChannelName, ChannelFlags );

				if( File::Exists( FullManagedName ) )
				{
					ReturnValue = Constants::SUCCESS;
				}
				else
				{
					ReturnValue = Constants::ERROR_FILE_FOUND_NOT;
				}
			}
		}
		catch( Exception^ Ex )
		{
			Log( EVerbosityLevel::Critical, Color::Red, "[Interface:TestChannel] Error: " + Ex->Message );
			ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
			CleanupClosedConnection();
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Opens a data channel for streaming data into the cache associated with an Agent
 *
 * @param ChannelName The name of the channel being opened
 * @param ChannelFlags The mode, access, and other attributes of the channel being opened
 *
 * @return A handle to the opened channel (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::OpenChannel( const TCHAR* ChannelName, TChannelFlags ChannelFlags )
{
	StartTiming( gcnew String( "OpenChannel-Managed" ), TRUE );

	INT ChannelHandle = Constants::INVALID;
	UBOOL NewChannelSuccessfullyCreated = FALSE;
	if( Connection != nullptr )
	{
		try
		{
			String^ ManagedChannelName = gcnew String( ChannelName );

			// Ask the Agent if the file is safe to open, if required
			if( !ConnectionConfiguration->IsPureLocalConnection )
			{
				StartTiming( gcnew String( "OpenChannel-Remote" ), FALSE );
				ChannelHandle = Connection->OpenChannel( ConnectionHandle, ManagedChannelName, ( EChannelFlags )ChannelFlags );
				StopTiming();
			}
			else
			{
				// If this is a pure local connection, then all we need to assure
				// if that the handle we generate here is unique to the connection
				ChannelHandle = Interlocked::Increment( BaseChannelHandle );
			}

			// If the channel is safe to open, open it up
			if( ChannelHandle >= 0 )
			{
				// Track the newly created temp file
				ChannelInfo^ NewChannelInfo = gcnew ChannelInfo();
				NewChannelInfo->ChannelName = ManagedChannelName;
				NewChannelInfo->ChannelFlags = ChannelFlags;
				NewChannelInfo->ChannelHandle = ChannelHandle;
				NewChannelInfo->ChannelFileStream = nullptr;
				NewChannelInfo->ChannelData = nullptr;
				NewChannelInfo->ChannelOffset = 0;

				// Determine the proper path name for the file
				String^ FullManagedName = GenerateFullChannelName( ManagedChannelName, ChannelFlags );
				
				// Try to open the file
				if( ChannelFlags & SWARM_CHANNEL_ACCESS_WRITE )
				{
					// Open the file stream
					NewChannelInfo->ChannelFileStream = gcnew FileStream( FullManagedName, FileMode::Create, FileAccess::Write, FileShare::None );

					// Slightly different path for compressed files
					if( ChannelFlags & SWARM_CHANNEL_MISC_ENABLE_COMPRESSION )
					{
						Stream^ NewChannelStream = NewChannelInfo->ChannelFileStream;
						NewChannelInfo->ChannelFileStream = gcnew GZipStream( NewChannelStream, CompressionMode::Compress, false );
					}
					
					// If we were able to open the file, add it to the active channel list
					Monitor::Enter( FreeChannelWriteBuffers );
					try
					{
						// If available, take the next free write buffer from the list
						if( FreeChannelWriteBuffers->Count > 0 )
						{
							NewChannelInfo->ChannelData = FreeChannelWriteBuffers->Pop();
						}
						else
						{
							// Otherwise, allocate a new write buffer for this channel (default to 1MB)
							NewChannelInfo->ChannelData = gcnew array<unsigned char>( 1024 * 1024 );
						}
					}
					finally
					{
						Monitor::Exit( FreeChannelWriteBuffers );
					}

					// Track the newly created file
					OpenChannels->Add( ChannelHandle, NewChannelInfo );
					NewChannelSuccessfullyCreated = TRUE;
				}
				else if( ChannelFlags & SWARM_CHANNEL_ACCESS_READ )
				{
					if( File::Exists( FullManagedName ) )
					{
						// Slightly different paths for compressed and non-compressed files
						if( ChannelFlags & SWARM_CHANNEL_MISC_ENABLE_COMPRESSION )
						{
							// Open the input stream, loading it entirely into memory
							array<unsigned char>^ RawCompressedData = File::ReadAllBytes( FullManagedName );

							// Allocate the destination buffer
							// The size of the uncompressed data is contained in the last four bytes of the file
							// http://www.ietf.org/rfc/rfc1952.txt?number=1952
							Int32 UncompressedSize = BitConverter::ToInt32( RawCompressedData, RawCompressedData->Length - 4 );
							NewChannelInfo->ChannelData = gcnew array<unsigned char>( UncompressedSize ); 

							// Open the decompression stream and decompress directly into the destination
							Stream^ DecompressionChannelStream = gcnew GZipStream( gcnew MemoryStream( RawCompressedData ), CompressionMode::Decompress, false );
							DecompressionChannelStream->Read( NewChannelInfo->ChannelData, 0, UncompressedSize );
							DecompressionChannelStream->Close();
						}
						else
						{
							// Simply read in the entire file in one go
							NewChannelInfo->ChannelData = File::ReadAllBytes( FullManagedName );
						}

						// Track the newly created channel
						OpenChannels->Add( ChannelHandle, NewChannelInfo );
						NewChannelSuccessfullyCreated = TRUE;
					}
					else
					{
						// Failed to find the channel to read, return an error
						ChannelHandle = Constants::ERROR_CHANNEL_NOT_FOUND;
					}
				}
			}
		}
		catch( Exception^ Ex )
		{
			Log( EVerbosityLevel::Critical, Color::Red, "[Interface:OpenChannel] Error: " + Ex->ToString() );
			ChannelHandle = Constants::ERROR_CONNECTION_DISCONNECTED;
			CleanupClosedConnection();
		}

		// If we opened the channel on the agent, but failed to create
		// the file, close it on the agent
		if( ( ChannelHandle >= 0 ) &&
			( NewChannelSuccessfullyCreated == FALSE ) )
		{
			if( !ConnectionConfiguration->IsPureLocalConnection )
			{
				StartTiming( gcnew String( "CloseChannel-Remote" ), FALSE );
				try
				{
					Connection->CloseChannel( ConnectionHandle, ChannelHandle );
				}
				catch( Exception^ Ex )
				{
					Log( EVerbosityLevel::Critical, Color::Red, "[Interface:OpenChannel] Cleanup error: " + Ex->Message );
					CleanupClosedConnection();
				}
				StopTiming();
			}
			ChannelHandle = Constants::ERROR_CHANNEL_IO_FAILED;
		}
	}
	else
	{
		ChannelHandle = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ChannelHandle );
}

/**
 * Closes an open channel
 *
 * @param Channel An open channel handle, returned by OpenChannel
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::CloseChannel( INT Channel )
{
	StartTiming( gcnew String( "CloseChannel-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		// Get the channel info, so we can close the connection on the Agent
		ChannelInfo^ ChannelToClose = nullptr;
		if( OpenChannels->TryGetValue( Channel, ChannelToClose ) )
		{
			try
			{
				// If the channel was open for WRITE, make sure any buffers are flushed
				if( ChannelToClose->ChannelFlags & SWARM_CHANNEL_ACCESS_WRITE )
				{
					FlushChannel( ChannelToClose );

					// Now that we're done with the write buffer, add it to the free stack
					// for another channel to use (only for WRITE)
					Monitor::Enter( FreeChannelWriteBuffers );
					try
					{
						FreeChannelWriteBuffers->Push( ChannelToClose->ChannelData );
					}
					finally
					{
						Monitor::Exit( FreeChannelWriteBuffers );
					}
				}

				// Remove the channel from the collection of open channels for this connection
				OpenChannels->Remove( Channel );

				// Close the file handle
				if( ChannelToClose->ChannelFileStream != nullptr )
				{
					ChannelToClose->ChannelFileStream->Close();
					ChannelToClose->ChannelFileStream = nullptr;
				}

				// Notify the Agent that the channel is closed, if required
				if( !ConnectionConfiguration->IsPureLocalConnection )
				{
					StartTiming( gcnew String( "CloseChannel-Remote" ), FALSE );
					Connection->CloseChannel( ConnectionHandle, ChannelToClose->ChannelHandle );
					StopTiming();
				}
				else
				{
					// Since this is a pure local connection, all we need to do is make
					// sure the now-closed channel is moved from the staging area into
					// the main cache, but only if the channel is PERSISTENT and WRITE
					if( ChannelToClose->ChannelFlags & SWARM_CHANNEL_TYPE_PERSISTENT )
					{
						if( ChannelToClose->ChannelFlags & SWARM_CHANNEL_ACCESS_WRITE )
						{
							// Get the final location path
							TChannelFlags WriteChannelFlags = ( TChannelFlags )( SWARM_CHANNEL_TYPE_PERSISTENT |  SWARM_CHANNEL_ACCESS_WRITE );
							String^ SrcChannelName = GenerateFullChannelName( ChannelToClose->ChannelName, WriteChannelFlags );
							TChannelFlags ReadChannelFlags = ( TChannelFlags )( SWARM_CHANNEL_TYPE_PERSISTENT |  SWARM_CHANNEL_ACCESS_READ );
							String^ DstChannelName = GenerateFullChannelName( ChannelToClose->ChannelName, ReadChannelFlags );

							// Always remove the destination file if it already exists
							FileInfo^ DstChannel = gcnew FileInfo( DstChannelName );
							if( DstChannel->Exists )
							{
								DstChannel->IsReadOnly = false;
								DstChannel->Delete();
							}

							// Copy if the paper trail is enabled; Move otherwise
							if( ChannelToClose->ChannelFlags & SWARM_CHANNEL_MISC_ENABLE_PAPER_TRAIL )
							{
								File::Copy( SrcChannelName, DstChannelName );
							}
							else
							{
								File::Move( SrcChannelName, DstChannelName );
							}
						}
					}
				}

				ReturnValue = Constants::SUCCESS;
			}
			catch( Exception^ Ex )
			{
				Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CloseChannel] Error: " + Ex->Message );
				ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
				CleanupClosedConnection();
			}
		}
		else
		{
			ReturnValue = Constants::ERROR_CHANNEL_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Writes the provided data to the open channel opened for WRITE
 *
 * @param Channel An open channel handle, returned by OpenChannel
 * @param Data Source buffer for the write
 * @param Data Size of the source buffer
 *
 * @return The number of bytes written (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::WriteChannel( INT Channel, const void* Data, Int32 DataSize )
{
	StartTiming( gcnew String( "WriteChannel-Managed" ), TRUE, DataSize );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		ChannelInfo^ ChannelToWrite = nullptr;
		if( ( OpenChannels->TryGetValue( Channel, ChannelToWrite ) ) &&
			( ChannelToWrite->ChannelFileStream != nullptr ) )
		{
			try
			{
				UBOOL WriteBuffered = FALSE;
				if( ChannelToWrite->ChannelData != nullptr )
				{
					pin_ptr<unsigned char> PinnedBufferData = &ChannelToWrite->ChannelData[0];

					// See if the new data will fit into the write buffer
					if( ( ChannelToWrite->ChannelOffset + DataSize ) <= ChannelToWrite->ChannelData->Length )
					{
						memcpy( PinnedBufferData + ChannelToWrite->ChannelOffset, Data, DataSize );
						ChannelToWrite->ChannelOffset += DataSize;
						ReturnValue = DataSize;
						WriteBuffered = TRUE;
					}
					else
					{
						// Otherwise, flush any pending writes and try again with the reset buffer
						FlushChannel( ChannelToWrite );
						if( DataSize <= ChannelToWrite->ChannelData->Length )
						{
							memcpy( PinnedBufferData, Data, DataSize );
							ChannelToWrite->ChannelOffset = DataSize;
							ReturnValue = DataSize;
							WriteBuffered = TRUE;
						}
					}
				}

				// Write was not buffered, just write it directly out
				if( !WriteBuffered )
				{
					try
					{
						// Allocate a temporary buffer and copy the data in
						array<unsigned char>^ TempChannelData = gcnew array<unsigned char>( DataSize );
						{
							pin_ptr<unsigned char> PinnedTempChannelData = &TempChannelData[0];
							memcpy( PinnedTempChannelData, Data, DataSize );
						}
						ChannelToWrite->ChannelFileStream->Write( TempChannelData, 0, DataSize );
						ReturnValue = DataSize;
					}
					catch( Exception^ Ex )
					{
						Log( EVerbosityLevel::Critical, Color::Red, "[Interface:WriteChannel] Error: " + Ex->Message );
						ReturnValue = Constants::ERROR_CHANNEL_IO_FAILED;
					}
				}
			}
			catch( Exception^ Ex )
			{
				Log( EVerbosityLevel::Critical, Color::Red, "[Interface:WriteChannel] Error: " + Ex->Message );
				ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
				CleanupClosedConnection();
			}

			// If this was not a successful IO operation, remove the channel
			// from the set of active channels
			if( ReturnValue < 0 )
			{
				OpenChannels->Remove( Channel );

				try
				{
					ChannelToWrite->ChannelFileStream->Close();
					ChannelToWrite->ChannelFileStream = nullptr;
				}
				catch( Exception^ Ex )
				{
					Log( EVerbosityLevel::Critical, Color::Red, "[Interface:WriteChannel] Error: " + Ex->Message );
					ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
					CleanupClosedConnection();
				}
			}
		}
		else
		{
			ReturnValue = Constants::ERROR_CHANNEL_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

INT FSwarmInterfaceManagedImpl::FlushChannel( ChannelInfo^ ChannelToFlush )
{
	INT ReturnValue = Constants::INVALID;
	if( ( ChannelToFlush->ChannelFlags & SWARM_CHANNEL_ACCESS_WRITE ) &&
		( ChannelToFlush->ChannelOffset > 0 ) &&
		( ChannelToFlush->ChannelData != nullptr ) &&
		( ChannelToFlush->ChannelFileStream != nullptr ) )
	{
		try
		{
			ChannelToFlush->ChannelFileStream->Write( ChannelToFlush->ChannelData, 0, ChannelToFlush->ChannelOffset );
			ReturnValue = ChannelToFlush->ChannelOffset;
		}
		catch( Exception^ Ex )
		{
			Log( EVerbosityLevel::Critical, Color::Red, "[Interface:FlushChannel] Error: " + Ex->Message );
			ReturnValue = Constants::ERROR_CHANNEL_IO_FAILED;
		}
		ChannelToFlush->ChannelOffset = 0;
	}
	return ReturnValue;
}

/**
 * Reads data from a channel opened for READ into the provided buffer
 *
 * @param Channel An open channel handle, returned by OpenChannel
 * @param Data Destination buffer for the read
 * @param Data Size of the destination buffer
 *
 * @return The number of bytes read (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::ReadChannel( INT Channel, void* Data, Int32 DataSize )
{
	StartTiming( gcnew String( "ReadChannel-Managed" ), TRUE, DataSize );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		ChannelInfo^ ChannelToRead = nullptr;
		if( ( OpenChannels->TryGetValue( Channel, ChannelToRead ) ) &&
			( ChannelToRead->ChannelData != nullptr ) )
		{
			try
			{
				// Read the data directly from our buffered copy of the file
				Int32 DataRemaining = ChannelToRead->ChannelData->Length - ChannelToRead->ChannelOffset;
				if( DataRemaining >= 0 )
				{
					Int32 SizeToRead = DataRemaining < DataSize ? DataRemaining : DataSize;
					if( SizeToRead > 0 )
					{
						pin_ptr<unsigned char> PinnedFileData = &ChannelToRead->ChannelData[0];
						memcpy( Data, PinnedFileData + ChannelToRead->ChannelOffset, SizeToRead );
						ChannelToRead->ChannelOffset += SizeToRead;
					}
					ReturnValue = SizeToRead;
				}
				else
				{
					ReturnValue = Constants::ERROR_CHANNEL_IO_FAILED;
				}
			}
			catch( Exception^ Ex )
			{
				Log( EVerbosityLevel::Critical, Color::Red, "[Interface:ReadChannel] Error: " + Ex->Message );
				ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
				CleanupClosedConnection();
			}

			// If this was not a successful IO operation, remove the channel
			// from the set of active channels
			if( ReturnValue < 0 )
			{
				OpenChannels->Remove( Channel );
			}
		}
		else
		{
			ReturnValue = Constants::ERROR_CHANNEL_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Sends a message to an Agent (return messages are sent via the FConnectionCallback)
 *
 * @param Message The message being sent
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceManagedImpl::SendMessage( const FMessage& NativeMessage )
{
	StartTiming( gcnew String( "SendMessage-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		AgentMessage^ ManagedMessage = nullptr;

		// TODO: As we add additional versions, convert to a switch rather than if-else.
		// For now, just use a simple if since we only have one version and a switch is
		// overkill.
		if( NativeMessage.Version == VERSION_1_0 )
		{
			switch( NativeMessage.Type )
			{
				case MESSAGE_TASK_REQUEST_RESPONSE:
					// Swallow this message, since it should not be sent along to a local connection
					// since all Job and Task information is contained within the Agent itself
				break;

				case MESSAGE_TASK_STATE:
				{
					const FTaskState& NativeTaskStateMessage = ( const FTaskState& )NativeMessage;
					AgentGuid^ ManagedTaskGuid = gcnew AgentGuid( NativeTaskStateMessage.TaskGuid.A,
																  NativeTaskStateMessage.TaskGuid.B,
																  NativeTaskStateMessage.TaskGuid.C,
																  NativeTaskStateMessage.TaskGuid.D );
					EJobTaskState TaskState = ( EJobTaskState )NativeTaskStateMessage.TaskState;
					AgentTaskState^ ManagedTaskStateMessage = gcnew AgentTaskState( nullptr, ManagedTaskGuid, TaskState );

					ManagedTaskStateMessage->TaskExitCode = NativeTaskStateMessage.TaskExitCode;
					ManagedTaskStateMessage->TaskRunningTime = NativeTaskStateMessage.TaskRunningTime;

					// If there is a message, be sure copy and pass it on
					if( NativeTaskStateMessage.TaskMessage != NULL )
					{
						ManagedTaskStateMessage->TaskMessage = gcnew String( NativeTaskStateMessage.TaskMessage );
					}

					ManagedMessage = ManagedTaskStateMessage;
				}
				break;

				case MESSAGE_INFO:
				{
					// Create the managed version of the info message
					const FInfoMessage& NativeInfoMessage = ( const FInfoMessage& )NativeMessage;
					AgentInfoMessage^ ManagedInfoMessage = gcnew AgentInfoMessage();
					if( NativeInfoMessage.TextMessage != NULL )
					{
						ManagedInfoMessage->TextMessage = gcnew String( NativeInfoMessage.TextMessage );
					}
					ManagedMessage = ManagedInfoMessage;
				}
				break;

				case MESSAGE_ALERT:
				{
					// Create the managed version of the alert message
					const FAlertMessage& NativeAlertMessage = (const FAlertMessage&)NativeMessage;
					AgentGuid^ JobGuid = gcnew AgentGuid(	NativeAlertMessage.JobGuid.A,
															NativeAlertMessage.JobGuid.B,
															NativeAlertMessage.JobGuid.C,
															NativeAlertMessage.JobGuid.D);
					AgentAlertMessage^ ManagedAlertMessage = gcnew AgentAlertMessage(JobGuid);
					ManagedAlertMessage->AlertLevel = (EAlertLevel)(NativeAlertMessage.AlertLevel);
					AgentGuid^ ObjectGuid = gcnew AgentGuid(NativeAlertMessage.ObjectGuid.A,
															NativeAlertMessage.ObjectGuid.B,
															NativeAlertMessage.ObjectGuid.C,
															NativeAlertMessage.ObjectGuid.D);
					ManagedAlertMessage->ObjectGuid = ObjectGuid;
					ManagedAlertMessage->TypeId = NativeAlertMessage.TypeId;
					if (NativeAlertMessage.TextMessage != NULL)
					{
						ManagedAlertMessage->TextMessage = gcnew String(NativeAlertMessage.TextMessage);
					}
					ManagedMessage = ManagedAlertMessage;
				}
				break;

				case MESSAGE_TIMING:
				{
					// Create the managed version of the info message
					const FTimingMessage& NativeTimingMessage = ( const FTimingMessage& )NativeMessage;
					AgentTimingMessage^ ManagedTimingMessage = gcnew AgentTimingMessage( ( EProgressionState )NativeTimingMessage.State, NativeTimingMessage.ThreadNum );
					ManagedMessage = ManagedTimingMessage;
				}
				break;

				default:
					// By default, just pass the message version and type through, but
					// any additional payload of a specialized type will be lost
					ManagedMessage = gcnew AgentMessage( ( AgentInterface::EMessageType )NativeMessage.Type );
				break;
			}
		}

		if( ManagedMessage != nullptr )
		{
			try
			{
				// Finally, send the message to the Agent
				StartTiming( gcnew String( "SendMessage-Remote" ), FALSE );
				Connection->SendMessage( ConnectionHandle, ManagedMessage );
				StopTiming();
				ReturnValue = Constants::SUCCESS;
			}
			catch( Exception^ Ex )
			{
				Log( EVerbosityLevel::Critical, Color::Red, "[Interface:SendMessage] Error: " + Ex->Message );
				ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
				CleanupClosedConnection();
			}
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/*
 * CacheFileAndConvertName
 *
 * Checks to see if the file with the correct timestamp and size exists in the cache
 * If it does not, add it to the cache
 */
INT FSwarmInterfaceManagedImpl::CacheFileAndConvertName( String^ FullPathName, String^& CachedFileName, bool bUse64bitNamingConvention )
{
	INT ReturnValue = Constants::INVALID;
	CachedFileName = "";

	// First, check that the original file exists
	if( File::Exists( FullPathName ) )
	{
		// The full cache name is based on the file name, the last modified timestamp on the file, and the file size
		FileInfo^ FullPathFileInfo = gcnew FileInfo( FullPathName );

		// The last modified timestamp
		String^ LastModifiedTimeUTCString = FullPathFileInfo->LastWriteTimeUtc.ToString( "yyyy-MM-dd_HH-mm-ss" );

		// The file size, which we get by opening the file
		String^ FileSizeInBytesString = FullPathFileInfo->Length.ToString();

		String^ Suffix64bit = bUse64bitNamingConvention ? "-64bit" : "";

		// Compose the full cache name
		CachedFileName = String::Format( "{0}_{1}_{2}{3}{4}",
			Path::GetFileNameWithoutExtension( FullPathName ),
			LastModifiedTimeUTCString,
			FileSizeInBytesString,
			Suffix64bit,
			FullPathFileInfo->Extension );

		// Test the cache with the cached file name
		if( Connection->TestChannel( ConnectionHandle, CachedFileName ) < 0 )
		{
			// If not already in the cache, attempt to add it now
			ReturnValue = Connection->AddChannel( ConnectionHandle, FullPathName, CachedFileName );
		}
		else
		{
			ReturnValue = Constants::SUCCESS;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_FILE_FOUND_NOT;
	}

	// Didn't find the file, return failure
	return( ReturnValue );
}

/*
 * CacheAllFiles
 *
 * Ensures all executables and dependencies are correctly placed in the cache
 */
INT FSwarmInterfaceManagedImpl::CacheAllFiles( AgentJobSpecification^ JobSpecification )
{
	INT ReturnValue = Constants::INVALID;
	try
	{
		// Allocate the dictionary we'll use for the name mapping
		Dictionary<String^, String^>^ DependenciesOriginalNames = gcnew Dictionary<String^, String^>();

		bool bUse64bitNamingConvention = int(JobSpecification->JobFlags & EJobTaskFlags::FLAG_64BIT) ? true : false;

		// Check for and possibly cache the executable
		String^ OriginalExecutableFullPath = gcnew String( JobSpecification->ExecutableName );
		String^ CachedExecutableName;
		ReturnValue = CacheFileAndConvertName( OriginalExecutableFullPath, CachedExecutableName, bUse64bitNamingConvention );
		if( ReturnValue < 0 )
		{
			// Failed to cache the executable, return failure
			Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CacheAllFiles] Failed to cache the executable: " + OriginalExecutableFullPath );
			return( ReturnValue );
		}

		JobSpecification->ExecutableName = CachedExecutableName;

		String^ OriginalExecutableName = Path::GetFileName( OriginalExecutableFullPath );
		DependenciesOriginalNames->Add( CachedExecutableName, OriginalExecutableName );

		// Check and cache all the required dependencies
		if( ( JobSpecification->RequiredDependencies != nullptr ) &&
			( JobSpecification->RequiredDependencies->Count > 0 ) )
		{
			for( INT i = 0; i < JobSpecification->RequiredDependencies->Count; i++ )
			{
				String^ OriginalDependencyFullPath = gcnew String( JobSpecification->RequiredDependencies[i] );
				String^ CachedDependencyName;
				ReturnValue = CacheFileAndConvertName( OriginalDependencyFullPath, CachedDependencyName, bUse64bitNamingConvention );
				if( ReturnValue < 0 )
				{
					// Failed to cache the dependency, return failure
					Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CacheAllFiles] Failed to cache the required dependency: " + OriginalDependencyFullPath );
					return( ReturnValue );
				}

				JobSpecification->RequiredDependencies[i] = CachedDependencyName;

				String^ OriginalDependencyName = Path::GetFileName( OriginalDependencyFullPath );
				DependenciesOriginalNames->Add( CachedDependencyName, OriginalDependencyName );
			}
		}
		// Check and cache any optional dependencies
		if( ( JobSpecification->OptionalDependencies != nullptr ) &&
			( JobSpecification->OptionalDependencies->Count > 0 ) )
		{
			for( INT i = 0; i < JobSpecification->OptionalDependencies->Count; i++ )
			{
				String^ OriginalDependencyFullPath = gcnew String( JobSpecification->OptionalDependencies[i] );
				String^ CachedDependencyName;
				ReturnValue = CacheFileAndConvertName( OriginalDependencyFullPath, CachedDependencyName, bUse64bitNamingConvention );
				if( ReturnValue < 0 )
				{
					// Failed to cache the dependency, log a warning
					Log( EVerbosityLevel::Verbose, Color::Orange, "[Interface:CacheAllFiles] Failed to cache an optional dependency: " + OriginalDependencyFullPath );
				}
				else
				{
					JobSpecification->OptionalDependencies[i] = CachedDependencyName;

					String^ OriginalDependencyName = Path::GetFileName( OriginalDependencyFullPath );
					DependenciesOriginalNames->Add( CachedDependencyName, OriginalDependencyName );
				}
			}
		}

		// Set the newly created name mapping dictionary
		JobSpecification->DependenciesOriginalNames = DependenciesOriginalNames;

		ReturnValue = Constants::SUCCESS;
	}
	catch( Exception^ Ex )
	{
		Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CacheAllFiles] Error: " + Ex->Message );
		ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
		CleanupClosedConnection();
	}
	return( ReturnValue );
}

INT FSwarmInterfaceManagedImpl::CacheAllFiles( AgentTaskSpecification^ TaskSpecification )
{
	INT ReturnValue = Constants::INVALID;
	try
	{
		TaskSpecification->DependenciesOriginalNames = nullptr;
		if( ( TaskSpecification->Dependencies != nullptr ) &&
			( TaskSpecification->Dependencies->Count > 0 ) )
		{
			// Allocate the dictionary we'll use for the name mapping
			Dictionary<String^, String^>^ DependenciesOriginalNames = gcnew Dictionary<String^, String^>();

			// Check and cache all the dependencies
			for( INT i = 0; i < TaskSpecification->Dependencies->Count; i++ )
			{
				String^ OriginalDependencyFullPath = gcnew String( TaskSpecification->Dependencies[i] );
				String^ CachedDependencyName;
				ReturnValue = CacheFileAndConvertName( OriginalDependencyFullPath, CachedDependencyName, false );
				if( ReturnValue < 0 )
				{
					// Failed to cache the dependency, return failure
					Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CacheAllFiles] Failed to cache the dependency: " + OriginalDependencyFullPath );
					return( ReturnValue );
				}

				TaskSpecification->Dependencies[i] = CachedDependencyName;

				String^ OriginalDependencyName = Path::GetFileName( OriginalDependencyFullPath );
				DependenciesOriginalNames->Add( CachedDependencyName, OriginalDependencyName );
			}

			// Set the newly created name mapping dictionary
			TaskSpecification->DependenciesOriginalNames = DependenciesOriginalNames;
		}
		ReturnValue = Constants::SUCCESS;
	}
	catch( Exception^ Ex )
	{
		Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CacheAllFiles] Error: " + Ex->Message );
		ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
		CleanupClosedConnection();
	}
	return( ReturnValue );
}

/**
 * Opens a Job session, which allows a Job to be specified, Tasks added, Job
 * channels opened and used, etc. When the Job is complete and no more Job
 * related data is needed from the Swarm, call CloseJob.
 *
 * @param JobGuid A GUID that uniquely identifies this Job, generated by the caller
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceManagedImpl::OpenJob( const FGuid& JobGuid )
{
	StartTiming( gcnew String( "OpenJob-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		StartTiming( gcnew String( "OpenJob-Remote" ), FALSE );
		try
		{
			AgentGuid^ ManagedJobGuid = gcnew AgentGuid( JobGuid.A, JobGuid.B, JobGuid.C, JobGuid.D );
			ReturnValue = Connection->OpenJob( ConnectionHandle, ManagedJobGuid );
			if( ReturnValue >= 0 )
			{
				// If the call was successful, assign the Job Guid as the active one
				ConnectionConfiguration->AgentJobGuid = ManagedJobGuid;

				// Allocate a new list to collect tasks until the specification is complete
				PendingTasks = gcnew List<AgentTaskSpecification^>();
			}
		}
		catch( Exception^ Ex )
		{
			Log( EVerbosityLevel::Critical, Color::Red, "[Interface:OpenJob] Error: " + Ex->Message );
			ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
			CleanupClosedConnection();
		}
		StopTiming();
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Converts an native job specification to a managed version.
 *
 * @param Specification	Native job specification (may be empty)
 * @param bIs64bit		Whether the job specification is 64-bit or not
 * @return				Newly created managed job specification, or nullptr if the specification was empty
 */
AgentJobSpecification^ FSwarmInterfaceManagedImpl::ConvertJobSpecification( const FJobSpecification& Specification, bool bIs64bit )
{
	if ( Specification.ExecutableName == NULL )
	{
		return nullptr;
	}
	else
	{
		// Convert the parameters from native to managed
		String^ ExecutableName = gcnew String( Specification.ExecutableName );
		String^ Parameters = gcnew String( Specification.Parameters );
		TJobTaskFlags Flags = TJobTaskFlags( (Specification.Flags & ~JOB_FLAG_64BIT) | (bIs64bit ? JOB_FLAG_64BIT : 0) ); 

		List<String^>^ RequiredDependencies = nullptr;
		if( Specification.RequiredDependencyCount > 0 )
		{
			RequiredDependencies = gcnew List<String^>();
			for( UINT i = 0; i < Specification.RequiredDependencyCount; i++ )
			{
				RequiredDependencies->Add( gcnew String( Specification.RequiredDependencies[i] ) );
			}
		}
		List<String^>^ OptionalDependencies = nullptr;
		if( Specification.OptionalDependencyCount > 0 )
		{
			OptionalDependencies = gcnew List<String^>();
			for( UINT i = 0; i < Specification.OptionalDependencyCount; i++ )
			{
				OptionalDependencies->Add( gcnew String( Specification.OptionalDependencies[i] ) );
			}
		}

		return gcnew AgentJobSpecification( ConnectionConfiguration->AgentJobGuid, ( EJobTaskFlags )Flags, ExecutableName, Parameters, RequiredDependencies, OptionalDependencies );
	}
}

/**
 * Begins a Job specification, which allows a series of Tasks to be specified
 * via AddTask. When Tasks are done being specified, call EndJobSpecification.
 *
 * The default behavior will be to execute the Job executable with the
 * specified parameters. If Tasks are added for the Job, they are expected
 * to be requested by the executable run for the Job. If no Tasks are added
 * for the Job, it is expected that the Job executable will perform its
 * operations without additional Task input from Swarm.
 *
 * @param Specification32 A structure describing a new 32-bit Job (can be an empty specification)
 * @param Specification64 A structure describing a new 64-bit Job (can be an empty specification)
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceManagedImpl::BeginJobSpecification( const FJobSpecification& Specification32, const FJobSpecification& Specification64 )
{
	StartTiming( gcnew String( "BeginJobSpecification-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		if( ConnectionConfiguration->AgentJobGuid != nullptr )
		{
			// Convert the specifications from native to managed
			AgentJobSpecification^ NewSpecification32 = ConvertJobSpecification( Specification32, false );
			AgentJobSpecification^ NewSpecification64 = ConvertJobSpecification( Specification64, true  );

			// Ensure all the files are in the cache with the right cache compatible name
			if ( NewSpecification32 )
			{
				ReturnValue = CacheAllFiles(NewSpecification32);
			}
			if ( NewSpecification64 && (NewSpecification32 == nullptr || ReturnValue >= 0) )
			{
				ReturnValue = CacheAllFiles(NewSpecification64);
			}

			if( ReturnValue >= 0 )
			{
				// Pack up the optional descriptions into Hashtables and pass them along
				UINT DescriptionIndex;
				Hashtable^ NewDescription32 = nullptr;
				Hashtable^ NewDescription64 = nullptr;
				// 32-bit specification description 
				if( Specification32.DescriptionCount > 0 )
				{
					try
					{
						NewDescription32 = gcnew Hashtable();
						NewDescription32["Version"] = ( Int32 )VERSION_1_0;
						for( DescriptionIndex = 0; DescriptionIndex < Specification32.DescriptionCount; DescriptionIndex++ )
						{
							String^ NewKey = gcnew String( Specification32.DescriptionKeys[DescriptionIndex] );
							String^ NewValue = gcnew String( Specification32.DescriptionValues[DescriptionIndex] );
							NewDescription32[NewKey] = NewValue;
						}
					}
					catch( Exception^ Ex )
					{
						// Failed to transfer optional description, log and continue
						Log( EVerbosityLevel::Critical, Color::Red, "[Interface:BeginJobSpecification] Error with Specification32 Description: " + Ex->Message );
						NewDescription32 = nullptr;
					}
				}
				// 64-bit specification description 
				if( Specification64.DescriptionCount > 0 )
				{
					try
					{
						NewDescription64 = gcnew Hashtable();
						NewDescription64["Version"] = ( Int32 )VERSION_1_0;
						for( DescriptionIndex = 0; DescriptionIndex < Specification64.DescriptionCount; DescriptionIndex++ )
						{
							String^ NewKey = gcnew String( Specification64.DescriptionKeys[DescriptionIndex] );
							String^ NewValue = gcnew String( Specification64.DescriptionValues[DescriptionIndex] );
							NewDescription64[NewKey] = NewValue;
						}
					}
					catch( Exception^ Ex )
					{
						// Failed to transfer optional description, log and continue
						Log( EVerbosityLevel::Critical, Color::Red, "[Interface:BeginJobSpecification] Error with Specification64 Description: " + Ex->Message );
						NewDescription64 = nullptr;
					}
				}

				StartTiming( gcnew String( "BeginJobSpecification-Remote" ), FALSE );
				try
				{
					ReturnValue = Connection->BeginJobSpecification( ConnectionHandle, NewSpecification32, NewDescription32, NewSpecification64, NewDescription64 );
				}
				catch( Exception^ Ex )
				{
					Log( EVerbosityLevel::Critical, Color::Red, "[Interface:BeginJobSpecification] Error: " + Ex->Message );
					ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
					CleanupClosedConnection();
				}
				StopTiming();
			}
		}
		else
		{
			ReturnValue = Constants::ERROR_JOB_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Adds a Task to the current Job
 *
 * @param Specification A structure describing the new Task
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceManagedImpl::AddTask( const FTaskSpecification& Specification )
{
	StartTiming( gcnew String( "AddTask-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		if( ConnectionConfiguration->AgentJobGuid != nullptr )
		{
			// Convert the parameters from native to managed
			AgentGuid^ TaskGuid = gcnew AgentGuid( Specification.TaskGuid.A,
												   Specification.TaskGuid.B,
												   Specification.TaskGuid.C,
												   Specification.TaskGuid.D );

			String^ Parameters = gcnew String( Specification.Parameters );

			List<String^>^ Dependencies = nullptr;
			if( Specification.DependencyCount > 0 )
			{
				Dependencies = gcnew List<String^>();
				for( UINT i = 0; i < Specification.DependencyCount; i++ )
				{
					Dependencies->Add( gcnew String( Specification.Dependencies[i] ) );
				}
			}

			AgentTaskSpecification^ NewSpecification =
				gcnew AgentTaskSpecification( ConnectionConfiguration->AgentJobGuid, TaskGuid, Specification.Flags, Parameters, Specification.Cost, Dependencies );

			// Ensure all the files are in the cache with the right cache compatible name
			ReturnValue = CacheAllFiles( NewSpecification );
			if( ReturnValue >= 0 )
			{
				// Queue up all tasks until the specification is complete and submit them all at once
				PendingTasks->Add( NewSpecification );
			}
		}
		else
		{
			ReturnValue = Constants::ERROR_JOB_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Ends the Job specification, after which no additional Tasks may be defined. Also,
 * this is generally the point when the Agent will validate and launch the Job executable,
 * potentially distributing the Job to other Agents.
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceManagedImpl::EndJobSpecification( void )
{
	StartTiming( gcnew String( "EndJobSpecification-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		if( ConnectionConfiguration->AgentJobGuid != nullptr )
		{
			StartTiming( gcnew String( "EndJobSpecification-Remote" ), FALSE );
			try
			{
				// Add all queued up and pending tasks now, all at once
				ReturnValue = Connection->AddTask( ConnectionHandle, PendingTasks );
				if( ReturnValue >= 0 )
				{
					// Finally, end the specification
					ReturnValue = Connection->EndJobSpecification( ConnectionHandle );
				}
			}
			catch( Exception^ Ex )
			{
				Log( EVerbosityLevel::Critical, Color::Red, "[Interface:EndJobSpecification] Error: " + Ex->Message );
				ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
				CleanupClosedConnection();
			}
			PendingTasks = nullptr;
			StopTiming();
		}
		else
		{
			ReturnValue = Constants::ERROR_JOB_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

/**
 * Ends the Job, after which all Job-related API usage (except OpenJob) will be rejected
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceManagedImpl::CloseJob( void )
{
	StartTiming( gcnew String( "CloseJob-Managed" ), TRUE );

	INT ReturnValue = Constants::INVALID;
	if( Connection != nullptr )
	{
		if( ConnectionConfiguration->AgentJobGuid != nullptr )
		{
			StartTiming( gcnew String( "CloseJob-Remote" ), FALSE );
			try
			{
				ReturnValue = Connection->CloseJob( ConnectionHandle );
			}
			catch( Exception^ Ex )
			{
				Log( EVerbosityLevel::Critical, Color::Red, "[Interface:CloseJob] Error: " + Ex->Message );
				ReturnValue = Constants::ERROR_CONNECTION_DISCONNECTED;
				CleanupClosedConnection();
			}
			StopTiming();

			// Reset the active Job Guid value
			try
			{
				ConnectionConfiguration->AgentJobGuid = nullptr;
			}
			catch( Exception^ )
			{
				// The ConnectionConfiguration can be set to nullptr
				// asynchronously, so we'll need to try/catch here
			}
		}
		else
		{
			ReturnValue = Constants::ERROR_JOB_NOT_FOUND;
		}
	}
	else
	{
		ReturnValue = Constants::ERROR_CONNECTION_NOT_FOUND;
	}

	StopTiming();
	return( ReturnValue );
}

INT FSwarmInterfaceManagedImpl::Log( EVerbosityLevel Verbosity, Color TextColour, String^ Message )
{
	try
	{
		Connection->Log( Verbosity, TextColour, Message );
	}
	catch( Exception^ )
	{
	}

	return( Constants::SUCCESS );
}

void FSwarmInterfaceManagedImpl::MessageThreadProc( Object^ ThreadParameters )
{
	MessageThreadData^ ThreadData = (MessageThreadData^)ThreadParameters;
	IAgentInterfaceWrapper^ Connection = ThreadData->Connection;

	try
	{
		// Because the way we use the GetMessage call is blocking, if we ever break out, quit
		AgentMessage^ ManagedMessage = nullptr;
		while( Connection->GetMessage( ThreadData->ConnectionHandle, ManagedMessage, -1 ) >= 0 )
		{
			// TODO: As we add additional versions, convert to a switch rather than if-else.
			// For now, just use a simple if since we only have one version and a switch is
			// overkill.
			if( ManagedMessage->Version == ESwarmVersionValue::VER_1_0 )
			{
				// Variables to hold the message and any data needed by the message that
				// will be passed into the native C++ instigator application.
				FMessage* NativeMessage = NULL;
				pin_ptr<wchar_t> PinnedStringData = nullptr;

				switch (ManagedMessage->Type)
				{
					case EMessageType::JOB_STATE:
					{
						AgentJobState^ JobStateMessage = (AgentJobState^)ManagedMessage;
						FGuid JobGuid( JobStateMessage->JobGuid->A,
									   JobStateMessage->JobGuid->B,
									   JobStateMessage->JobGuid->C,
									   JobStateMessage->JobGuid->D );

						FJobState* NativeJobStateMessage = new FJobState( JobGuid, ( TJobTaskState )JobStateMessage->JobState );
						NativeJobStateMessage->JobExitCode = JobStateMessage->JobExitCode;
						NativeJobStateMessage->JobRunningTime = JobStateMessage->JobRunningTime;

						// If there is a message, be sure to pin and pass it on
						if (JobStateMessage->JobMessage != nullptr)
						{
							array<wchar_t>^ RawJobMessageData = JobStateMessage->JobMessage->ToCharArray();
							if (RawJobMessageData->Length > 0)
							{
								// Pin the string data for the message
								PinnedStringData = &RawJobMessageData[0];
								NativeJobStateMessage->JobMessage = PinnedStringData;
							}
						}

						NativeMessage = NativeJobStateMessage;
					}
					break;

					case EMessageType::TASK_REQUEST:
						// Swallow this message, since it should not be sent along to a local connection
						// since all Job and Task information is contained within the Agent itself
					break;

					case EMessageType::TASK_REQUEST_RESPONSE:
					{
						AgentTaskRequestResponse^ TaskRequestResponseMessage = (AgentTaskRequestResponse^)ManagedMessage;

						// Switch again on the response type
						ETaskRequestResponseType ResponseType = (ETaskRequestResponseType)TaskRequestResponseMessage->ResponseType;
						switch (ResponseType)
						{
							case ETaskRequestResponseType::RELEASE:
							case ETaskRequestResponseType::RESERVATION:
							{
								NativeMessage = new FTaskRequestResponse( ( TTaskRequestResponseType )ResponseType );
							}
							break;

							case ETaskRequestResponseType::SPECIFICATION:
							{
								AgentTaskSpecification^ TaskSpecificationMessage = (AgentTaskSpecification^)TaskRequestResponseMessage;
								FGuid TaskGuid( TaskSpecificationMessage->TaskGuid->A,
												TaskSpecificationMessage->TaskGuid->B,
												TaskSpecificationMessage->TaskGuid->C,
												TaskSpecificationMessage->TaskGuid->D );
								TJobTaskFlags TaskFlags = ( TJobTaskFlags )TaskSpecificationMessage->TaskFlags;

								array<wchar_t>^ RawParametersData = TaskSpecificationMessage->Parameters->ToCharArray();
								if (RawParametersData->Length > 0)
								{
									// Pin the string data for the message
									PinnedStringData = &RawParametersData[0];
								}

								NativeMessage = new FTaskSpecification( TaskGuid, PinnedStringData, TaskFlags );
							}
							break;
						}
					}
					break;

					case EMessageType::TASK_STATE:
					{
						AgentTaskState^ TaskStateMessage = (AgentTaskState^)ManagedMessage;

						// TODO: Assert that we have a valid Job GUID, since this must be the Instigator
						// TODO: Assert that the Job GUID of the message matches the ConnectionConfiguration->AgentJobGuid

						FGuid TaskGuid( TaskStateMessage->TaskGuid->A,
										TaskStateMessage->TaskGuid->B,
										TaskStateMessage->TaskGuid->C,
										TaskStateMessage->TaskGuid->D );

						FTaskState* NativeTaskStateMessage = new FTaskState( TaskGuid, ( TJobTaskState )TaskStateMessage->TaskState );

						// If there is a message, be sure to pin and pass it
						if (TaskStateMessage->TaskMessage != nullptr)
						{
							array<wchar_t>^ RawTaskMessageData = TaskStateMessage->TaskMessage->ToCharArray();
							if (RawTaskMessageData->Length > 0)
							{
								// Pin the string data for the message
								PinnedStringData = &RawTaskMessageData[0];
								NativeTaskStateMessage->TaskMessage = PinnedStringData;
							}
						}

						NativeMessage = NativeTaskStateMessage;
					}
					break;

					case EMessageType::INFO:
					{
						// Create the managed version of the info message
						AgentInfoMessage^ ManagedInfoMessage = (AgentInfoMessage^)ManagedMessage;
						FInfoMessage* NativeInfoMessage = new FInfoMessage(TEXT(""));
						if ( ManagedInfoMessage->TextMessage != nullptr )
						{
							array<wchar_t>^ RawTaskMessageData = ManagedInfoMessage->TextMessage->ToCharArray();
							if (RawTaskMessageData->Length > 0)
							{
								// Pin the string data for the message
								PinnedStringData = &RawTaskMessageData[0];
								NativeInfoMessage->TextMessage = PinnedStringData;
							}
						}

						NativeMessage = NativeInfoMessage;
					}
					break;

					case EMessageType::ALERT:
					{
						// Create the managed version of the info message
						AgentAlertMessage^ ManagedAlertMessage = (AgentAlertMessage^)ManagedMessage;
						FGuid JobGuid(	ManagedAlertMessage->JobGuid->A,
										ManagedAlertMessage->JobGuid->B,
										ManagedAlertMessage->JobGuid->C,
										ManagedAlertMessage->JobGuid->D);
						FGuid ObjectGuid(	ManagedAlertMessage->ObjectGuid->A,
											ManagedAlertMessage->ObjectGuid->B,
											ManagedAlertMessage->ObjectGuid->C,
											ManagedAlertMessage->ObjectGuid->D);
						FAlertMessage* NativeAlertMessage = new FAlertMessage(
							JobGuid, 
							(NSwarm::TAlertLevel)(ManagedAlertMessage->AlertLevel), 
							ObjectGuid, 
							ManagedAlertMessage->TypeId);
						if (ManagedAlertMessage->TextMessage != nullptr)
						{
							array<wchar_t>^ RawTaskMessageData = ManagedAlertMessage->TextMessage->ToCharArray();
							if (RawTaskMessageData->Length > 0)
							{
								// Pin the string data for the message
								PinnedStringData = &RawTaskMessageData[0];
								NativeAlertMessage->TextMessage = PinnedStringData;
							}
						}
						NativeMessage = NativeAlertMessage;
					}
					break;

					default:
						// By default, just pass the message version and type through, but
						// any additional payload of a specialized type will be lost
						NativeMessage = new FMessage( VERSION_1_0, ( TMessageType )ManagedMessage->Type );
					break;
				}

				// If a message was created to pass on to the user's callback, send it on
				if (NativeMessage != NULL)
				{
					// Call the user's callback function
					ThreadData->ConnectionCallback( NativeMessage, ThreadData->ConnectionCallbackData );

					// All finished with the message, free and set to null
					delete( NativeMessage );
					NativeMessage = NULL;
				}

				if (ManagedMessage->Type == EMessageType::QUIT)
				{
					// Return from this function, which will exit this message processing thread
					Debug::WriteLineIf( Debugger::IsAttached, "Message queue thread shutting down from a QUIT message" );
					return;
				}
			}

			// Reset the message handle
			ManagedMessage = nullptr;
		}
	}
	catch( ThreadAbortException^ )
	{
		// An expected exception when closing the connection
		Debug::WriteLineIf( Debugger::IsAttached, "Message queue thread shutting down normally after being closed by CloseConnection" );
	}
	catch( Exception^ Ex )
	{
		Debug::WriteLineIf( Debugger::IsAttached, "Error: Exception in the message queue thread: " + Ex->Message );

		// If the connection has thrown us an exception, close the connection
		Debug::WriteLineIf( Debugger::IsAttached, "MessageThreadProc calling CleanupClosedConnection" );
		ThreadData->Owner->CleanupClosedConnection();
	}

	// Only write out in debug
	Debug::WriteLineIf( Debugger::IsAttached, "Message queue thread shutting down normally" );
}

void FSwarmInterfaceManagedImpl::MonitorThreadProc( Object^ ThreadParameters )
{
	MessageThreadData^ ThreadData = (MessageThreadData^)ThreadParameters;
	AgentConfiguration^ ConnectionConfiguration = ThreadData->ConnectionConfiguration;
	IAgentInterfaceWrapper^ Connection = ThreadData->Connection;

	UBOOL NeedToCleanupClosedConnection = FALSE;
	try
	{
		if( Connection != nullptr )
		{
			// We'll just monitor the process itself to see if it exits
			Int32 ConnectionProcessID = ConnectionConfiguration->AgentProcessID;

			Process^ AgentProcess = Process::GetProcessById( ConnectionProcessID );
			if( AgentProcess != nullptr )
			{
				// As long as the process is still running, yield
				UBOOL KeepRunning = TRUE;
				while( KeepRunning )
				{
					// Sleep for a little bit at the beginning of each iteration
					Thread::Sleep( 1000 );

					if( AgentProcess->HasExited == true )
					{
						// If the Agent process has exited, game over
						NeedToCleanupClosedConnection = TRUE;
						KeepRunning = FALSE;
					}
				}
			}
		}
	}
	catch( ThreadAbortException^ )
	{
		// An expected exception when closing the connection
		Debug::WriteLineIf( Debugger::IsAttached, "Agent monitor thread shutting down normally after being closed by CloseConnection" );
	}
	catch( Exception^ Ex )
	{
		Debug::WriteLineIf( Debugger::IsAttached, "Exception in the agent monitor thread: " + Ex->Message );

		// If the connection has thrown us an exception, close the connection
		NeedToCleanupClosedConnection = TRUE;
	}

	// If needed, clean up the connection
	if( NeedToCleanupClosedConnection )
	{
		Debug::WriteLineIf( Debugger::IsAttached, "MonitorThreadProc calling CleanupClosedConnection" );
		ThreadData->Owner->CleanupClosedConnection();
	}

	Debug::WriteLineIf( Debugger::IsAttached, "Agent monitor thread shutting down normally" );
}

/*
 * Start a timer going
 */
void FSwarmInterfaceManagedImpl::StartTiming( String^ Name, UBOOL Accum, Int64 Adder )
{
	if( PerfTimerInstance != nullptr )
	{
		PerfTimerInstance->Start( Name, Accum, Adder );
	}
}

void FSwarmInterfaceManagedImpl::StartTiming( String^ Name, UBOOL Accum )
{
	StartTiming( Name, Accum, 0 );
}

/*
 * Stop a timer
 */
void FSwarmInterfaceManagedImpl::StopTiming( void )
{
	if( PerfTimerInstance != nullptr )
	{
		PerfTimerInstance->Stop();
	}
}

/*
 * Print out all the timing info
 */
void FSwarmInterfaceManagedImpl::DumpTimings( void )
{
	if( PerfTimerInstance != nullptr )
	{
		String^ PerfMessage = PerfTimerInstance->DumpTimings();

		pin_ptr<const TCHAR> NativePerfMessage = PtrToStringChars( PerfMessage );

		SendMessage( FInfoMessage( NativePerfMessage ) );

		PerfTimerInstance = nullptr;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/**
 * The managed C++ implementation of FSwarmInterface
 */
class FSwarmInterfaceImpl : public FSwarmInterface
{
public:
	FSwarmInterfaceImpl( void );
	virtual ~FSwarmInterfaceImpl( void );

	/**
	 * Opens a new connection to the Swarm
	 *
	 * @param CallbackFunc The callback function Swarm will use to communicate back to the Instigator
	 *
	 * @return An INT containing the error code (if < 0) or the handle (>= 0) which is useful for debugging only
	 */
	virtual INT OpenConnection( FConnectionCallback CallbackFunc, void* CallbackData, TLogFlags LoggingFlags );

	/**
	 * Closes an existing connection to the Swarm
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT CloseConnection( void );

	/**
	 * Sends a message to an Agent (return messages are sent via the FConnectionCallback)
	 *
	 * @param Message The message being sent
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT SendMessage( const FMessage& Message );

	/**
	 * Adds an existing file to the cache. Note, any existing channel with the same
	 * name will be overwritten.
	 *
	 * @param FullPath The full path name to the file that should be copied into the cache
	 * @param ChannelName The name of the channel once it's in the cache
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT AddChannel( const TCHAR* FullPath, const TCHAR* ChannelName );

	/**
	 * Determines if the named channel is in the cache
	 *
	 * @param ChannelName The name of the channel to look for
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT TestChannel( const TCHAR* ChannelName );

	/**
	 * Opens a data channel for streaming data into the cache associated with an Agent
	 *
	 * @param ChannelName The name of the channel being opened
	 * @param ChannelFlags The mode, access, and other attributes of the channel being opened
	 *
	 * @return A handle to the opened channel (< 0 is error)
	 */
	virtual INT OpenChannel( const TCHAR* ChannelName, TChannelFlags ChannelFlags );

	/**
	 * Closes an open channel
	 *
	 * @param Channel An open channel handle, returned by OpenChannel
	 *
	 * @return INT error code (< 0 is error)
	 */
	virtual INT CloseChannel( INT Channel );

	/**
	 * Writes the provided data to the open channel opened for WRITE
	 *
	 * @param Channel An open channel handle, returned by OpenChannel
	 * @param Data Source buffer for the write
	 * @param Data Size of the source buffer
	 *
	 * @return The number of bytes written (< 0 is error)
	 */
	virtual INT WriteChannel( INT Channel, const void* Data, INT DataSize );

	/**
	 * Reads data from a channel opened for READ into the provided buffer
	 *
	 * @param Channel An open channel handle, returned by OpenChannel
	 * @param Data Destination buffer for the read
	 * @param Data Size of the destination buffer
	 *
	 * @return The number of bytes read (< 0 is error)
	 */
	virtual INT ReadChannel( INT Channel, void* Data, INT DataSize );

	/**
	 * Opens a Job session, which allows a Job to be specified, Tasks added, Job
	 * channels opened and used, etc. When the Job is complete and no more Job
	 * related data is needed from the Swarm, call CloseJob.
	 *
	 * @param JobGuid A GUID that uniquely identifies this Job, generated by the caller
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT OpenJob( const FGuid& JobGuid );

	/**
	 * Begins a Job specification, which allows a series of Tasks to be specified
	 * via AddTask. When Tasks are done being specified, call EndJobSpecification.
	 *
	 * The default behavior will be to execute the Job executable with the
	 * specified parameters. If Tasks are added for the Job, they are expected
	 * to be requested by the executable run for the Job. If no Tasks are added
	 * for the Job, it is expected that the Job executable will perform its
	 * operations without additional Task input from Swarm.
	 *
	 * @param Specification32 A structure describing a new 32-bit Job
	 * @param Specification64 A structure describing a new 64-bit Job
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT BeginJobSpecification( const FJobSpecification& Specification32, const FJobSpecification& Specification64 );

	/**
	 * Adds a Task to the current Job
	 *
	 * @param Specification A structure describing the new Task
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT AddTask( const FTaskSpecification& Specification );

	/**
	 * Ends the Job specification, after which no additional Tasks may be defined. Also,
	 * this is generally the point when the Agent will validate and launch the Job executable,
	 * potentially distributing the Job to other Agents.
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT EndJobSpecification( void );

	/**
	 * Ends the Job, after which all Job-related API usage (except OpenJob) will be rejected
	 *
	 * @return INT Error code (< 0 is an error)
	 */
	virtual INT CloseJob( void );

	/**
	 * Adds a line of text to the Agent log window
	 *
	 * @param Verbosity the importance of this message
	 * @param TextColour the colour of the text
	 * @param Message the line of text to add
	 */
	virtual INT Log( TVerbosityLevel Verbosity, UINT TextColour, const TCHAR* Message );

private:
	gcroot<FSwarmInterfaceManagedImpl^>	ManagedSwarmInterface;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FSwarmInterfaceImpl::FSwarmInterfaceImpl( void )
{
	ManagedSwarmInterface = gcnew FSwarmInterfaceManagedImpl();
}

FSwarmInterfaceImpl::~FSwarmInterfaceImpl( void )
{
}

/**
 * @return The Swarm singleton
 */
FSwarmInterface* FSwarmInterface::GInstance = NULL;
FSwarmInterface& FSwarmInterface::Get( void )
{
	if( GInstance == NULL )
	{
		GInstance = new FSwarmInterfaceImpl();
	}

	return( *GInstance ); 
}

/**
 * Opens a new connection to the Swarm
 *
 * @param CallbackFunc The callback function Swarm will use to communicate back to the Instigator
 *
 * @return An INT containing the error code (if < 0) or the handle (>= 0) which is useful for debugging only
 */
INT FSwarmInterfaceImpl::OpenConnection( FConnectionCallback CallbackFunc, void* CallbackData, TLogFlags LoggingFlags )
{
	// CallbackFunc can be NULL
	// CallbackData can be NULL
	INT ReturnValue = ManagedSwarmInterface->OpenConnection( CallbackFunc, CallbackData, ( ELogFlags )LoggingFlags );

	return( ReturnValue );
}

/**
 * Closes an existing connection to the Swarm
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceImpl::CloseConnection( void )
{
	INT ReturnValue = ManagedSwarmInterface->CloseConnection();
	return( ReturnValue );
}

/**
 * Sends a message to an Agent (return messages are sent via the FConnectionCallback)
 *
 * @param Message The message being sent
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceImpl::SendMessage( const FMessage& Message )
{
	INT ReturnValue = ManagedSwarmInterface->SendMessage( Message );
	return( ReturnValue );
}

/**
 * Adds an existing file to the cache. Note, any existing channel with the same
 * name will be overwritten.
 *
 * @param FullPath The full path name to the file that should be copied into the cache
 * @param ChannelName The name of the channel once it's in the cache
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceImpl::AddChannel( const TCHAR* FullPath, const TCHAR* ChannelName )
{
	if( FullPath == NULL )
	{
		return( Constants::ERROR_INVALID_ARG1 );
	}

	if( ChannelName == NULL )
	{
		return( Constants::ERROR_INVALID_ARG2 );
	}

	INT ReturnValue = ManagedSwarmInterface->AddChannel( FullPath, ChannelName );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in AddChannel" ) );
	}

	return( ReturnValue );
}

/**
 * Determines if the named channel is in the cache
 *
 * @param ChannelName The name of the channel to look for
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceImpl::TestChannel( const TCHAR* ChannelName )
{
	if( ChannelName == NULL )
	{
		return( Constants::ERROR_INVALID_ARG1 );
	}

	INT ReturnValue = ManagedSwarmInterface->TestChannel( ChannelName );
	// Check for the one, known error code (file not found)
	if( ( ReturnValue < 0 ) &&
		( ReturnValue != Constants::ERROR_FILE_FOUND_NOT ) )
	{
		SendMessage( FInfoMessage( L"Error, fatal in TestChannel" ) );
	}

	return( ReturnValue );
}

/**
 * Opens a data channel for streaming data into the cache associated with an Agent
 *
 * @param ChannelName The name of the channel being opened
 * @param ChannelFlags The mode, access, and other attributes of the channel being opened
 *
 * @return A handle to the opened channel (< 0 is error)
 */
INT FSwarmInterfaceImpl::OpenChannel( const TCHAR* ChannelName, TChannelFlags ChannelFlags )
{
	if( ChannelName == NULL )
	{
		return( Constants::ERROR_INVALID_ARG1 );
	}

	INT ReturnValue = ManagedSwarmInterface->OpenChannel( ChannelName, ChannelFlags );
	if( ReturnValue < 0 && (ChannelFlags & SWARM_CHANNEL_ACCESS_WRITE) )
	{
		SendMessage( FInfoMessage( L"Error, fatal in OpenChannel" ) );
	}

	return( ReturnValue );
}

/**
 * Closes an open channel
 *
 * @param Channel An open channel handle, returned by OpenChannel
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceImpl::CloseChannel( INT Channel )
{
	if( Channel < 0 )
	{
		return( Constants::ERROR_INVALID_ARG1 );
	}

	INT ReturnValue = ManagedSwarmInterface->CloseChannel( Channel );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in CloseChannel" ) );
	}

	return( ReturnValue );
}

/**
 * Writes the provided data to the open channel opened for WRITE
 *
 * @param Channel An open channel handle, returned by OpenChannel
 * @param Data Source buffer for the write
 * @param Data Size of the source buffer
 *
 * @return The number of bytes written (< 0 is error)
 */
INT FSwarmInterfaceImpl::WriteChannel( INT Channel, const void* Data, INT DataSize )
{
	if( Channel < 0 )
	{
		return( Constants::ERROR_INVALID_ARG1 );
	}

	if( Data == NULL )
	{
		return( Constants::ERROR_INVALID_ARG2 );
	}

	if( DataSize < 0 )
	{
		return( Constants::ERROR_INVALID_ARG3 );
	}

	INT ReturnValue = ManagedSwarmInterface->WriteChannel( Channel, Data, DataSize );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in WriteChannel" ) );
	}

	return( ReturnValue );
}

/**
 * Reads data from a channel opened for READ into the provided buffer
 *
 * @param Channel An open channel handle, returned by OpenChannel
 * @param Data Destination buffer for the read
 * @param Data Size of the destination buffer
 *
 * @return The number of bytes read (< 0 is error)
 */
INT FSwarmInterfaceImpl::ReadChannel( INT Channel, void* Data, INT DataSize )
{
	if( Channel < 0 )
	{
		return( Constants::ERROR_INVALID_ARG1 );
	}

	if( Data == NULL )
	{
		return( Constants::ERROR_INVALID_ARG2 );
	}

	if( DataSize < 0 )
	{
		return( Constants::ERROR_INVALID_ARG3 );
	}

	INT ReturnValue = ManagedSwarmInterface->ReadChannel( Channel, Data, DataSize );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in ReadChannel" ) );
	}

	return( ReturnValue );
}

/**
 * Opens a Job session, which allows a Job to be specified, Tasks added, Job
 * channels opened and used, etc. When the Job is complete and no more Job
 * related data is needed from the Swarm, call CloseJob.
 *
 * @param JobGuid A GUID that uniquely identifies this Job, generated by the caller
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceImpl::OpenJob( const FGuid& JobGuid )
{
	INT ReturnValue = ManagedSwarmInterface->OpenJob( JobGuid );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in OpenJob" ) );
	}

	return( ReturnValue );
}

/**
 * Begins a Job specification, which allows a series of Tasks to be specified
 * via AddTask. When Tasks are done being specified, call EndJobSpecification.
 *
 * The default behavior will be to execute the Job executable with the
 * specified parameters. If Tasks are added for the Job, they are expected
 * to be requested by the executable run for the Job. If no Tasks are added
 * for the Job, it is expected that the Job executable will perform its
 * operations without additional Task input from Swarm.
 *
 * @param Specification32 A structure describing a new 32-bit Job
 * @param Specification64 A structure describing a new 64-bit Job
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceImpl::BeginJobSpecification( const FJobSpecification& Specification32, const FJobSpecification& Specification64 )
{
	if( Specification32.ExecutableName == NULL && Specification64.ExecutableName == NULL )
	{
		return( Constants::ERROR_INVALID_ARG );
	}

	if( Specification32.Parameters == NULL && Specification64.Parameters == NULL )
	{
		return( Constants::ERROR_INVALID_ARG );
	}

	if( (Specification32.RequiredDependencyCount > 0 && Specification32.RequiredDependencies == NULL) ||
		(Specification32.OptionalDependencyCount > 0 && Specification32.OptionalDependencies == NULL) ||
		(Specification64.RequiredDependencyCount > 0 && Specification64.RequiredDependencies == NULL) ||
		(Specification64.OptionalDependencyCount > 0 && Specification64.OptionalDependencies == NULL) )
	{
		return( Constants::ERROR_INVALID_ARG );
	}

	INT ReturnValue = ManagedSwarmInterface->BeginJobSpecification( Specification32, Specification64 );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in BeginJobSpecification" ) );
	}

	return( ReturnValue );
}

/**
 * Adds a Task to the current Job
 *
 * @param Specification A structure describing the new Task
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceImpl::AddTask( const FTaskSpecification& Specification )
{
	if( Specification.Parameters == NULL )
	{
		return( Constants::ERROR_INVALID_ARG );
	}

	if( ( Specification.DependencyCount > 0 ) &&
		( Specification.Dependencies == NULL ) )
	{
		return( Constants::ERROR_INVALID_ARG );
	}

	INT ReturnValue = ManagedSwarmInterface->AddTask( Specification );
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in AddTask" ) );
	}

	return( ReturnValue );
}

/**
 * Ends the Job specification, after which no additional Tasks may be defined. Also,
 * this is generally the point when the Agent will validate and launch the Job executable,
 * potentially distributing the Job to other Agents.
 *
 * @return INT Error code (< 0 is an error)
 */
INT FSwarmInterfaceImpl::EndJobSpecification( void )
{
	INT ReturnValue = ManagedSwarmInterface->EndJobSpecification();
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in EndJobSpecification" ) );
	}

	return( ReturnValue );
}

/**
 * Ends the definition period of a Job
 *
 * @param JobGuid The GUID of the Job specification
 *
 * @return INT error code (< 0 is error)
 */
INT FSwarmInterfaceImpl::CloseJob( void )
{
	INT ReturnValue = ManagedSwarmInterface->CloseJob();
	if( ReturnValue < 0 )
	{
		SendMessage( FInfoMessage( L"Error, fatal in CloseJob" ) );
	}

	return( ReturnValue );
}

/**
 * Adds a line of text to the Agent log window
 *
 * @param Verbosity the importance of this message
 * @param TextColour the colour of the text
 * @param Message the line of text to add
 */
INT FSwarmInterfaceImpl::Log( TVerbosityLevel Verbosity, UINT TextColour, const TCHAR* Message )
{
	if( Message == NULL )
	{
		return( Constants::ERROR_NULL_POINTER );
	}

	String^ ManagedMessage = gcnew String( Message );
	Color ManagedTextColour = Color::FromArgb( TextColour );

	INT ReturnValue = ManagedSwarmInterface->Log( ( EVerbosityLevel )Verbosity, ManagedTextColour, ManagedMessage );

	return( ReturnValue );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}	// namespace NSwarm

#pragma pack ( pop )

/*=============================================================================
	PS3AudioDevice.cpp: Unreal PS3 Audio interface object.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

//	OpenAL is RHS
//	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "PS3Drv.h"
#include "EngineSoundClasses.h"

#include <sysutil/sysutil_sysparam.h>

#define MAX_DSPPAGECOUNT			2
#define UPDATE_THREAD_PRIO			0
#define MULTISTREAM_STACK_SIZE		( 0x4000 ) // 16 kb
#define AUDIO_MASTERVOLUME			0.5f
#define DEFAULT_LISTENER			0
#define AUDIO_DISTANCE_FACTOR		0.0127f

/*------------------------------------------------------------------------------------
	Static variables from the early init
------------------------------------------------------------------------------------*/

// The number of speakers producing sound (stereo or 5.1)
INT UPS3AudioDevice::NumSpeakers							= 0;

/**
 * Checks for errors from multistream API calls
 */
UBOOL UPS3AudioDevice::ValidateAPICall( const TCHAR* Function, INT ErrorCode )
{
	if( ErrorCode < 0 )
	{
		INT DetailedError = cellMSSystemGetLastError();
		switch( DetailedError )
		{
		case CELL_MS_ERROR_SYSPAUSED:
			debugf( NAME_DevAudio, TEXT( "%s error: system paused (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_INVAL:
			debugf( NAME_DevAudio, TEXT( "%s error: parameter is invalid (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_WRONGSTATE:
			debugf( NAME_DevAudio, TEXT( "%s error: a state is wrong (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_RESOURCEUNAVAIL:
			debugf( NAME_DevAudio, TEXT( "%s error: a system resource is unavailable (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_CIRCULARDEPEND:
			debugf( NAME_DevAudio, TEXT( "%s error: there is a circular dependency (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_BUSMISMATCH:
			debugf( NAME_DevAudio, TEXT( "%s error: there is a bus mismatch (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_INVALIDELF:
			debugf( NAME_DevAudio, TEXT( "%s error: an elf file is invalid (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_FILEACCESS:
			debugf( NAME_DevAudio, TEXT( "%s error: access to a file failed (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_INVALIDENV:
			debugf( NAME_DevAudio, TEXT( "%s error: an envelope is invalid (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_OUTOFRANGE:
			debugf( NAME_DevAudio, TEXT( "%s error: out of range (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_BADMODE:
			debugf( NAME_DevAudio, TEXT( "%s error: there is a bad mode (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_NOFREESTREAMS:
			debugf( NAME_DevAudio, TEXT( "%s error: there are no free streams (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_SURROUNDNOTINIT:
			debugf( NAME_DevAudio, TEXT( "%s error: surround sond is not initialised (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_INVALID_CHANNEL:
			debugf( NAME_DevAudio, TEXT( "%s error: a channel is invalid (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_INVALID_TYPE:
			debugf( NAME_DevAudio, TEXT( "%s error: a type is invalid (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_AUDIO_CONFIGURE:
			debugf( NAME_DevAudio, TEXT( "%s error: cellAudioOutConfigure failed (%d)" ), Function, DetailedError );
			break;

		case CELL_MS_ERROR_AUDIO_GETSTATE:
			debugf( NAME_DevAudio, TEXT( "%s error: cellAudioOutGetState failed (%d)" ), Function, DetailedError );
			break;

		default:
			debugf( NAME_DevAudio, TEXT( "%s error: unknown error (%d)" ), Function, DetailedError );
			break;
		}

		return( FALSE );
	}

	return( TRUE );
}

/**
 * Callback when a loop (2>=N>infinity) sound is done so we can trap the loop, and continue playing
 */
void StreamCallback( int StreamNumber, void* UserData, int CallbackType, void* ReadBuffer, int ReadSize )
{
	if( CallbackType == CELL_MS_CALLBACK_MOREDATA )
	{
		FPS3SoundSource* Source = ( FPS3SoundSource* )UserData;
		// remember that we looped
		Source->bLoopWasReached = TRUE;
	}
}

/**
 * MultiStream update thread that pumps sound handling
 */
static void MultiStreamUpdateProc( uint64_t Param )
{
	UPS3AudioDevice* AudioDevice = ( UPS3AudioDevice* )( PTRINT )Param;

	cellAudioPortStart( AudioDevice->GetPortNum() );
	while( !GIsRequestingExit )
	{
		// this should loop *at least* 94 times a second (according to the docs)

		// serve sound
		FLOAT* Data = cellMSSystemSignalSPU();
		if( Data != NULL )
		{
			// let any callbacks needed get processed
			cellMSSystemGenerateCallbacks();
		}

		// Crutch up the PS3's threading model (6 is bad)
		appSleep( AudioDevice->TimeBetweenHWUpdates * 0.001f );
	}

	cellAudioPortStop( AudioDevice->GetPortNum() );
	sys_ppu_thread_exit( 0 );
}

/**
 * Convert Unreal units and coordinate system to OpenAL
 */
void CalculateALVectorsFromUnrealVectors( FVector* Location, FVector* Velocity, FVector* Front, FVector* Up )
{
	FLOAT Swap;

	// Flip Y and Z for OpenAL coordinate system
	if( Location )
	{
		Swap = Location->Y;
		Location->Y = Location->Z;
		Location->Z = Swap;
		*Location *= AUDIO_DISTANCE_FACTOR;
	}

	if( Velocity )
	{
		Swap = Velocity->Y;
		Velocity->Y = Velocity->Z;
		Velocity->Z = Swap;
		*Velocity *= AUDIO_DISTANCE_FACTOR;
	}

	if( Front )
	{
		Swap = Front->Y;
		Front->Y = Front->Z;
		Front->Z = Swap;
	}

	if( Up )
	{
		Swap = Up->Y;
		Up->Y = Up->Z;
		Up->Z = Swap;
	}
}

/*------------------------------------------------------------------------------------
	UPS3AudioDevice
------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS( UPS3AudioDevice );

// bus volumes. (Maximum volume for each speaker for each channel)
const FLOAT UPS3AudioDevice::MasterBusVolumes[64] =
{
	AUDIO_MASTERVOLUME, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, AUDIO_MASTERVOLUME, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, AUDIO_MASTERVOLUME, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, AUDIO_MASTERVOLUME, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, AUDIO_MASTERVOLUME, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, AUDIO_MASTERVOLUME, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

const FLOAT UPS3AudioDevice::MaxBusVolumes[64] =
{
	1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

const FLOAT UPS3AudioDevice::ZeroBusVolumes[64] = 
{
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

/** 
 * UPS3AudioDevice::StaticConstructor
 */
void UPS3AudioDevice::StaticConstructor( void )
{
	new( GetClass(), TEXT( "TimeBetweenHWUpdates" ), RF_Public ) UFloatProperty( CPP_PROPERTY( TimeBetweenHWUpdates ), TEXT( "PS3Drv" ), CPF_Config );
}

/**
 * UPS3AudioDevice::Init
 */
UBOOL UPS3AudioDevice::Init( void )
{
#if DEDICATED_SERVER
	return FALSE;
#endif

	// Default to sensible channel count.
	if( MaxChannels < 1 )
	{
		MaxChannels = 32;
	}

	// startup the multistream sound mixer
	StartMixer();

	// create our pool of sound sources
	for( INT SourceIndex = 0; SourceIndex < Min( MaxChannels, MAX_AUDIOCHANNELS ); SourceIndex++ )
	{
		// @todo: clean these up
		FPS3SoundSource* Source = new FPS3SoundSource( this, SourceIndex );
		Sources.AddItem( Source );
		FreeSources.AddItem( Source );
	}

	if( !Sources.Num() )
	{
		debugf( NAME_Error, TEXT( "PS3Audio: couldn't allocate sources" ) );
		return FALSE;
	}

	// Update MaxChannels in case we couldn't create enough sources.
	MaxChannels = Sources.Num();
	debugf( TEXT( "PS3AudioDevice: Allocated %i sources" ), MaxChannels );

	// Initialize permanent memory stack for initial & always loaded sound allocations.
	if( CommonAudioPoolSize )
	{
		debugf( TEXT( "PS3AudioDevice: Allocating %g MByte for always resident audio data" ), CommonAudioPoolSize / ( 1024.0f * 1024.0f ) );
		CommonAudioPoolFreeBytes = CommonAudioPoolSize;
		CommonAudioPool = ( BYTE* )appMalloc( CommonAudioPoolSize );
	}
	else
	{
		debugf( TEXT( "PS3AudioDevice: CommonAudioPoolSize is set to 0 - disabling persistent pool for audio data" ) );
		CommonAudioPoolFreeBytes = 0;
	}

#if USE_SULPHA_DEBUGGING
	SulphaMemory = appMalloc( SUPLHA_MEMORY_SIZE, 128 );
	cellMSSulphaInit( SulphaMemory, SUPLHA_MEMORY_SIZE, 70 );
	cellMSSulphaDECI3Start();
#endif

	// Initialized.
	NextResourceID = 1;

	// Initialize base class last as it's going to precache already loaded audio.
	Super::Init();

	return TRUE;
}

/**
 * Initialize the cell audio and configure multistream settings based on user's system config
 * Only initialized once
 *
 * @return Number of speakers, or 0 for error
 */
UBOOL UPS3AudioDevice::InitCellAudio(void)
{
	INT Error = CELL_OK;
	static UBOOL bCellAudioInitialized = FALSE;
	if( !bCellAudioInitialized )
	{
		// Configure MultiStream based on the user's preferences in the system
		unsigned int retSysUtil = cellMSSystemConfigureSysUtilEx( CELL_MS_AUDIOMODESELECT_SUPPORTSLPCM | CELL_MS_AUDIOMODESELECT_SUPPORTSDOLBY | CELL_MS_AUDIOMODESELECT_SUPPORTSDTS | CELL_MS_AUDIOMODESELECT_PREFERDOLBY );
		NumSpeakers = retSysUtil & 0x0000000F;
		debugf( NAME_Init, TEXT( "PS3Audio: Reported by SysUtil: Channels: %u, Dolby Digital: %s\n" ), NumSpeakers, ( retSysUtil & 0x00000010 ) ? TEXT( "Enabled" ) : TEXT( "Disabled" ) );

		// initialize libAudio
		Error = cellAudioInit();
		if( Error != CELL_OK )
		{
			debugf( NAME_Error, TEXT( "PS3Audio: cellAudioInit() error : %d" ), Error );
			return FALSE;
		}

		bCellAudioInitialized = TRUE;
	}
	return TRUE;
}

/**
 * Actually bring up the mixer, which has to happen pretty late in the current SDK (after all Waves are loaded)
 */
UBOOL UPS3AudioDevice::StartMixer( void )
{
	debugf( NAME_Init, TEXT( "PS3Audio: Starting Mixer" ) );

	if( !InitCellAudio() )
	{
		return FALSE;
	}

	INT Error = CELL_OK;

	// audio port open.
	CellAudioPortParam AudioParam;

	AudioParam.nChannel = CELL_AUDIO_PORT_8CH;
	AudioParam.nBlock = CELL_AUDIO_BLOCK_8;
	AudioParam.attr = CELL_AUDIO_PORTATTR_INITLEVEL;
	AudioParam.level = 1.0f;
	Error = cellAudioPortOpen( &AudioParam, &PortNum );
	if( Error != CELL_OK )
	{
		cellAudioQuit();
		debugf( NAME_Error, TEXT( "PS3Audio: Error cellAudioPortOpen()\n" ) );
		return FALSE;
	}

	// get port config.
	Error = cellAudioGetPortConfig( PortNum, &PortConfig );
	if( Error != CELL_OK )
	{
		cellAudioQuit();
		debugf( NAME_Error, TEXT( "PS3Audio: Error cellAudioGetPortConfig\n" ) );
		return FALSE;
	}

    // Setup system memory allocation
	CellMSSystemConfig SystemConfig;

	// Allocate plenty of channels give headroom for multichannel sounds
	SystemConfig.channelCount = MaxChannels * 4;
	SystemConfig.subCount = UN_BUS_COUNT;
	SystemConfig.dspPageCount = MAX_DSPPAGECOUNT;
	SystemConfig.flags = CELL_MS_DISABLE_SPU_PRINTF_SERVER | CELL_MS_TD_ONLY_512;
	INT MSMemoryNeeded = cellMSSystemGetNeededMemorySize( &SystemConfig );
	MultiStreamMemory = appMalloc( MSMemoryNeeded, 128 );

	// Specify 1-15 per SPU (0 to not use that SPU).
	BYTE SPUPriorities[8] = { SPU_PRIO_AUDIO, SPU_PRIO_AUDIO, SPU_PRIO_AUDIO, SPU_PRIO_AUDIO, SPU_PRIO_AUDIO, SPU_PRIO_AUDIO, SPU_PRIO_AUDIO, SPU_PRIO_AUDIO };
	INT ReturnCode = cellMSSystemInitSPURS( MultiStreamMemory, &SystemConfig, GSPURS, SPUPriorities );
	if( ReturnCode != CELL_OK )
	{
		ReturnCode = cellMSSystemGetLastError();
		debugf( NAME_Error, TEXT( "PS3Audio: Failed to initialize MultiStream SPURS task: %d" ), ReturnCode );
		return( FALSE );
	}

	debugf( TEXT( "... SPURS workspace allocated: %0.1fK" ), MSMemoryNeeded / 1024.0f );

	// Route every sub bus to master - set busses 3-1 to be in the time domain
	cellMSCoreRoutingInit( 0xe );

	// Setup the volumes on all the busses
	cellMSCoreSetVolume64( CELL_MS_MASTER_BUS, CELL_MS_DRY, MasterBusVolumes );
	cellMSCoreSetVolume64( UN_REVERB_BUS, CELL_MS_WET_AND_DRY, MaxBusVolumes );
	cellMSCoreSetVolume64( UN_EQ_BUS, CELL_MS_WET, MaxBusVolumes );
	cellMSCoreSetVolume64( UN_EQ_AND_REVERB_BUS, CELL_MS_WET, MaxBusVolumes );

	// Set up the bypassing to get the channel routing we want
	cellMSCoreSetBypass( CELL_MS_MASTER_BUS, CELL_MS_DRY, CELL_MS_NOTBYPASSED );
	cellMSCoreSetBypass( CELL_MS_MASTER_BUS, CELL_MS_WET, CELL_MS_BYPASSED );

	cellMSCoreSetBypass( UN_REVERB_BUS, CELL_MS_DRY, CELL_MS_NOTBYPASSED );
	cellMSCoreSetBypass( UN_REVERB_BUS, CELL_MS_WET, CELL_MS_NOTBYPASSED );
	cellMSCoreSetBypass( UN_EQ_BUS, CELL_MS_DRY, CELL_MS_BYPASSED );
	cellMSCoreSetBypass( UN_EQ_BUS, CELL_MS_WET, CELL_MS_NOTBYPASSED );
	cellMSCoreSetBypass( UN_EQ_AND_REVERB_BUS, CELL_MS_DRY, CELL_MS_BYPASSED );
	cellMSCoreSetBypass( UN_EQ_AND_REVERB_BUS, CELL_MS_WET, CELL_MS_NOTBYPASSED );

	// Set the bus routing (wet and dry are also handled above)
	cellMSCoreSetRouting( UN_REVERB_BUS, CELL_MS_MASTER_BUS, CELL_MS_WET_AND_DRY, CELL_MS_ROUTE_ON, 1.0f, 1.0f );
	cellMSCoreSetRouting( UN_EQ_BUS, CELL_MS_MASTER_BUS, CELL_MS_WET, CELL_MS_ROUTE_ON, 0.0f, 1.0f );
	cellMSCoreSetRouting( UN_EQ_AND_REVERB_BUS, UN_REVERB_BUS, CELL_MS_WET, CELL_MS_ROUTE_ON, 0.0f, 1.0f );
	cellMSCoreSetRouting( UN_EQ_AND_REVERB_BUS, CELL_MS_MASTER_BUS, CELL_MS_WET_AND_DRY, CELL_MS_ROUTE_OFF, 0.0f, 0.0f );

	cellMSSystemConfigureLibAudio( &AudioParam, &PortConfig );

	// Create and start the update thread
	Error = sys_ppu_thread_create( &MultiStreamUpdateThread, MultiStreamUpdateProc, ( uint64_t )this, UPDATE_THREAD_PRIO,
		MULTISTREAM_STACK_SIZE, SYS_PPU_THREAD_CREATE_JOINABLE, "MultiStream Update Thread" );

	if( Error != CELL_OK )
	{
		debugf( NAME_Error, TEXT( "PS3Audio: Failed to start Update Thread: %d" ), Error );
		return FALSE;
	}

	INT CompressedMemoryNeeded;

	// Set up MP3 decoding - worst case is 3 stereo sounds per source
	CompressedMemoryNeeded = cellMSMP3GetNeededMemorySize( MaxChannels * NUM_SUB_CHANNELS );
	CompressedMemory = appMalloc( CompressedMemoryNeeded, 128 );
	Error = cellMSMP3Init( MaxChannels * NUM_SUB_CHANNELS, CompressedMemory );
	if( Error != CELL_OK )
	{
		debugf( NAME_Error, TEXT( "PS3Audio: cellMSMP3Init() failed: 0x%x" ), Error );
		return FALSE;
	}

	debugf( TEXT( "cellMSMP3Init memory allocated = %0.1fK" ), CompressedMemoryNeeded / 1024.0f );

	// Setup the surround sound system

	// Get the amount of memory required in bytes for 0 global sources and 1 listener
	INT Size = cellMSSurroundGetInfoSize( 0, 1 );
	MSSurroundMemory = appMalloc( Size, 128 );
	debugf( TEXT( "cellMSSurroundGetInfoSize memory allocated = %0.1fK" ), Size / 1024.0f );

	// Init surround sound with 0 global sources and 1 listener
	Error = cellMSSurroundInit( MSSurroundMemory, 0, 1 );
	if( Error != CELL_OK )
	{
		debugf( TEXT( "cellMSSurroundInit() failed: 0x%x" ), Error );
		return( FALSE );
	}

	// Init Doppler shift parameters
	cellMSSurroundDopplerFactor( 10.0f );
	cellMSSurroundDopplerVelocity( 1.0f );
	// shouldn't need to modify this, but the number is from the OpenAL 1.1 spec available at www.openal.org
//	cellMSSurroundSpeedOfSound( 343.3f );

	// Disable attenuation, as unreal calculates it
	cellMSSurroundDistanceModel( CELL_MS_SURROUND_NONE );

	// Init the player as the listener
	FLOAT DefaultOrientation[] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
	cellMSSurroundListenerf( DEFAULT_LISTENER, CELL_MS_SURROUND_GAIN, 1.0f );
	cellMSSurroundListener3f( DEFAULT_LISTENER, CELL_MS_SURROUND_POSITION, 0.0f, 0.0f, 0.0f );
	cellMSSurroundListener3f( DEFAULT_LISTENER, CELL_MS_SURROUND_VELOCITY, 0.0f, 0.0f, 0.0f );
	cellMSSurroundListenerfv( DEFAULT_LISTENER, CELL_MS_SURROUND_ORIENTATION, DefaultOrientation );

	// Check for and init EFX extensions - must be on for PS3
	Effects = new FPS3AudioEffectsManager( this, Sources );

	debugf( NAME_Init, TEXT( "PS3Audio: Device initialized." ) );

	return( TRUE );
}

/**
 * Precaches the passed in sound node wave object.
 *
 * @param	SoundNodeWave	Resource to be precached.
 */
void UPS3AudioDevice::Precache( USoundNodeWave* SoundNodeWave )
{
	FPS3SoundBuffer::Init( SoundNodeWave, this );

	// Size of the decompressed data
	INC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->CompressedPS3Data.GetBulkDataSize() );
	INC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->CompressedPS3Data.GetBulkDataSize() );
}

/**
 * Frees the bulk resource data associated with this SoundNodeWave.
 *
 * @param	SoundNodeWave	wave object to free associated bulk data
 */
void UPS3AudioDevice::FreeResource( USoundNodeWave* SoundNodeWave )
{
	// Find buffer.
	FPS3SoundBuffer* Buffer = NULL;
	if( SoundNodeWave->ResourceID )
	{
		// Find buffer associated with resource id.
		Buffer = WaveBufferMap.FindRef( SoundNodeWave->ResourceID );

		// Remove from buffers array.
		Buffers.RemoveItem( Buffer );

		// Delete it. This will automatically remove itself from the WaveBufferMap.
		delete Buffer;
	}

	// Stat housekeeping
	DEC_DWORD_STAT_BY( STAT_AudioMemorySize, SoundNodeWave->CompressedPS3Data.GetBulkDataSize() );
	DEC_DWORD_STAT_BY( STAT_AudioMemory, SoundNodeWave->CompressedPS3Data.GetBulkDataSize() );
}

/**
 * UPS3AudioDevice::Update
 */
void UPS3AudioDevice::Update( UBOOL Realtime )
{
	Super::Update( Realtime );

	// Setup our location, velocity, and orientation vectors
	// Note that velocity is zero so we can adjust the doppler on a per source basis
	FVector Location, Velocity( 0.0f, 0.0f, 0.0f ), Orientation[2];
	Location = Listeners(0).Location;
	Orientation[0] = Listeners( 0 ).Front;
	Orientation[1] = Listeners( 0 ).Up;

	CalculateALVectorsFromUnrealVectors( &Location, &Velocity, &Orientation[0], &Orientation[1] );

	ValidateAPICall( TEXT( "cellMSSurroundListenerfv CELL_MS_SURROUND_POSITION" ), cellMSSurroundListenerfv( DEFAULT_LISTENER, CELL_MS_SURROUND_POSITION, ( FLOAT* )&Location ) );
	ValidateAPICall( TEXT( "cellMSSurroundListenerfv CELL_MS_SURROUND_VELOCITY" ), cellMSSurroundListenerfv( DEFAULT_LISTENER, CELL_MS_SURROUND_VELOCITY, ( FLOAT* )&Velocity ) );
	ValidateAPICall( TEXT( "cellMSSurroundListenerfv CELL_MS_SURROUND_ORIENTATION" ), cellMSSurroundListenerfv( DEFAULT_LISTENER, CELL_MS_SURROUND_ORIENTATION, ( FLOAT* )&Orientation[0] ) );

	// Print statistics for first non initial load allocation.
	static UBOOL bFirstTime = TRUE;
	if( bFirstTime && CommonAudioPoolSize != 0 )
	{
		bFirstTime = FALSE;
		if( CommonAudioPoolFreeBytes != 0 )
		{
			debugf( TEXT( "PS3Audio: Audio pool size mismatch by %d bytes. Please update CommonAudioPoolSize ini setting to %d to avoid waste!" ),
				CommonAudioPoolFreeBytes, CommonAudioPoolSize - CommonAudioPoolFreeBytes );;
		}
	}
}

// sort memory usage from large to small unless bAlphaSort
static UBOOL bAlphaSort = FALSE;
IMPLEMENT_COMPARE_POINTER( FPS3SoundBuffer, PS3AudioDevice, { return bAlphaSort ? appStricmp( *A->ResourceName, *B->ResourceName ) : ( A->GetSize() > B->GetSize() ) ? -1 : 1; } );


/**
 * This will return the name of the SoundClass of the SoundCue that this buffer(soundnodewave) belongs to.
 * NOTE: This will find the first cue in the ObjectIterator list.  So if we are using SoundNodeWaves in multiple
 * SoundCues we will pick up the first first one only.
 **/
static FName GetSoundClassNameFromBuffer( const FPS3SoundBuffer* const Buffer )
{
	// for each buffer
	// look at all of the SoundCue's SoundNodeWaves to see if the ResourceID matched
	// if it does then grab the SoundClass of the SoundCue (that the waves werre gotten from)

	for( TObjectIterator<USoundCue> It; It; ++It )
	{
		USoundCue* Cue = *It;
		TArray<USoundNodeWave*> OutWaves;
		Cue->RecursiveFindNode<USoundNodeWave>( Cue->FirstNode, OutWaves );

		for( INT WaveIndex = 0; WaveIndex < OutWaves.Num(); WaveIndex++ )
		{
			USoundNodeWave* WaveNode = OutWaves(WaveIndex);
			if( WaveNode != NULL )
			{
				if( WaveNode->ResourceID == Buffer->ResourceID )
				{
					return Cue->SoundClass;
				}
			}
		}
	}

	return NAME_None;
}

/**
 * Displays debug information about the loaded sounds
 *
 * @NOTE:  if you update this make certain you update XeAudioDevice.cpp ListSounds also
 */
void UPS3AudioDevice::ListSounds( const TCHAR* Cmd, FOutputDevice& Ar )
{
	bAlphaSort = ParseParam( Cmd, TEXT( "ALPHASORT" ) );

	INT	TotalResident = 0;
	INT	ResidentCount = 0;

	Ar.Logf( TEXT( ", Size Kb, NumChannels, Frequency, SoundName, bAllocationInPermanentPool, SoundClass" ) );

	TArray<FPS3SoundBuffer*> AllSounds;
	for( INT BufferIndex = 0; BufferIndex < Buffers.Num(); BufferIndex++ )
	{
		AllSounds.AddItem( Buffers( BufferIndex ) );
	}

	Sort<USE_COMPARE_POINTER( FPS3SoundBuffer, PS3AudioDevice )>( &AllSounds( 0 ), AllSounds.Num() );

	for( INT i = 0; i < AllSounds.Num(); ++i )
	{
		FPS3SoundBuffer* Buffer = AllSounds( i );
		INT NumChannels = 0;
		INT Pitch = 0;

		const FName SoundClassName = GetSoundClassNameFromBuffer( Buffer );

		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( Buffer->MSInfo[Index].numChannels )
			{
				NumChannels += Buffer->MSInfo[Index].numChannels;
				Pitch = Buffer->MSInfo[Index].Pitch;
			}
		}

		Ar.Logf( TEXT( ", %8.2f, %d channel(s), %d Hz, %s, %d, %s" ), ( FLOAT )Buffer->GetSize() / 1024.0f, NumChannels, Pitch, *Buffer->ResourceName, Buffer->bAllocationInPermanentPool, *SoundClassName.ToString() );
		TotalResident += Buffer->GetSize();
		ResidentCount++;
	}

	Ar.Logf( TEXT( "%8.2f Kb for %d resident sounds" ), ( FLOAT )TotalResident / 1024.0f, ResidentCount );
}

/**
 * Sets the 'pause' state of sounds which are always loaded.
 *
 * @param	bPaused			Pause sounds if TRUE, play paused sounds if FALSE.
 */
void UPS3AudioDevice::PauseAlwaysLoadedSounds( UBOOL bPaused )
{
	// Remember the state
	bAlwaysLoadedSoundsSwappedOut = bPaused;

	for( INT BufferIndex = 0; BufferIndex < Buffers.Num(); ++BufferIndex )
	{
		FPS3SoundBuffer* Buffer = Buffers( BufferIndex );

		if( Buffer->bAllocationInPermanentPool )
		{
			// Find the resource ID associated with the buffer.
			const INT* ResourceID = WaveBufferMap.FindKey( Buffer );
			if( ResourceID )
			{
				// Search the sound sources for the one with a matching ID.
				for( INT SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex )
				{
					FSoundSource* Source = Sources( SourceIndex );
					if( Source
						&& Source->GetWaveInstance()
						&& Source->GetWaveInstance()->WaveData
						&& Source->GetWaveInstance()->WaveData->ResourceID == *ResourceID )
					{
						// If we're pausing sounds and this one was playing . . .
						if( bPaused && Source->IsPlaying() )
						{
							// . . . pause it and mark the 'always-loaded paused' flag.
							Source->Pause();
							( ( FPS3SoundSource* )Source )->bAlwaysLoadedPause = TRUE;
						}
						else if( ( ( FPS3SoundSource* )Source )->bAlwaysLoadedPause )
						{
							// Otherwise, we're playing.  If the sound was marked with 'always-loaded paused',
							// play it and clear the flag.
							Source->Play();
							( ( FPS3SoundSource* )Source )->bAlwaysLoadedPause = FALSE;
						}
					}
				}
			}
		}
	}
}

/**
 *	UPS3AudioDevice::Teardown
 */
void UPS3AudioDevice::Teardown( void )
{
	Flush( NULL );

#if USE_SULPHA_DEBUGGING
	if( SulphaMemory )
	{
		cellMSSulphaShutdown();
		appFree( SulphaMemory );
	}
#endif

	if( MultiStreamMemory )
	{
		// Shut down the systems
		cellMSSurroundClose();
		cellMSSystemClose();

		// Free the allocated memory
		appFree( MultiStreamMemory );
		appFree( CompressedMemory );
		appFree( MSSurroundMemory );

		MultiStreamMemory = NULL;
		CompressedMemory = NULL;
		MSSurroundMemory = NULL;
	}
}

/**
 * UPS3AudioDevice::FinishDestroy
 */
void UPS3AudioDevice::FinishDestroy( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "PS3 Audio Device shut down." ) );
	}

	Super::FinishDestroy();
}

/**
 * UPS3AudioDevice::ShutdownAfterError
 */
void UPS3AudioDevice::ShutdownAfterError( void )
{
	if( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		Teardown();
		debugf( NAME_Exit, TEXT( "UPS3AudioDevice::ShutdownAfterError" ) );
	}

	Super::ShutdownAfterError();
}

/**
 * Allocates memory from permanent pool. This memory will NEVER be freed.
 *
 * @param	Size	Size of allocation.
 *
 * @return pointer to a chunk of memory with size Size
 */
void* UPS3AudioDevice::AllocatePermanentMemory( INT Size )
{
	void* Allocation = NULL;
	
	// Fall back to using regular allocator if there is not enough space in permanent memory pool.
	if( Size > CommonAudioPoolFreeBytes )
	{
		Allocation = appMalloc( Size, 128 );
		check( Allocation );
	}
	// Allocate memory from pool.
	else
	{
		BYTE* CommonAudioPoolAddress = ( BYTE* )CommonAudioPool;
		Allocation = CommonAudioPoolAddress + ( CommonAudioPoolSize - CommonAudioPoolFreeBytes );
	}
	// Decrement available size regardless of whether we allocated from pool or used regular allocator
	// to allow us to log suggested size at the end of initial loading.
	CommonAudioPoolFreeBytes -= Align( Size, 128 );

	return( Allocation );
}

/*------------------------------------------------------------------------------------
	FPS3SoundBuffer.
------------------------------------------------------------------------------------*/
FPS3SoundBuffer::FPS3SoundBuffer( UPS3AudioDevice* InAudioDevice )
{
	AudioDevice	= InAudioDevice;
	ResourceID = INDEX_NONE;
	bAllocationInPermanentPool = FALSE;
	CompressedData = NULL;

	for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
	{
		MSInfo[Index].numChannels = 0;
		MSInfo[Index].FirstBuffer = NULL;
		MSInfo[Index].FirstBufferSize = 0;
	}
}

FPS3SoundBuffer::~FPS3SoundBuffer( void )
{
	if( bAllocationInPermanentPool )
	{
		// if we are shutting down, this destructor could be called - don't crash
		if( GIsRequestingExit )
		{
			return;
		}

		appErrorf( TEXT( "Can't free resource '%s' as it was allocated in permanent pool." ), *ResourceName );
	}

	if( ResourceID )
	{
		AudioDevice->WaveBufferMap.Remove( ResourceID );

		// Wave data was kept in pBuffer so we need to free it.
		appFree( CompressedData );
	}
}

/**
 * Static function used to create a buffer.
 *
 * @param InWave		USoundNodeWave to use as template and wave source
 * @param AudioDevice	audio device to attach created buffer to
 * @return FXeSoundBuffer pointer if buffer creation succeeded, NULL otherwise
 */
FPS3SoundBuffer* FPS3SoundBuffer::Init( USoundNodeWave* Wave, UPS3AudioDevice* AudioDevice )
{
	// Can't create a buffer without any source data
	if( Wave == NULL || Wave->NumChannels == 0 )
	{
		return( NULL );
	}

	FPS3SoundBuffer* Buffer = NULL;

	// Check whether this buffer already has been initialized...
	if( Wave->ResourceID )
	{
		Buffer = AudioDevice->WaveBufferMap.FindRef( Wave->ResourceID );
	}

	if( Buffer )
	{
		// ... and return cached pointer if it has.
		return( Buffer );
	}
	else
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioResourceCreationTime );

		// Create new buffer.
		Buffer = new FPS3SoundBuffer( AudioDevice );

		if( !Wave->bUseTTS )
		{
			// Load raw data.  Audio that encountered troubles during the cooking phase may be reduced to zero bytes
			// here, so we need to handle that carefully.  We still need to create an audio buffer for these 'empty'
			// blobs since we never want to try to reload the data from disk.
			Buffer->CompressedDataSize = Wave->CompressedPS3Data.GetBulkDataSize();
			if( Buffer->CompressedDataSize > 0 )
			{
				BYTE* RawData = ( BYTE* )Wave->CompressedPS3Data.Lock( LOCK_READ_ONLY );

				// WORKAROUND: Add 128 dummy bytes, because ATRAC3 SPU code apparently align read sizes to 128 bytes
				// which can read outside the end of the sound data. This causes a DMA crash if it crosses into another 64 KB memory chunk that
				// has been unmapped by sys_memory_free (DLMalloc).
				INT SoundAllocationSize = Buffer->CompressedDataSize + 128;

				// attempt to load always loaded sounds into the never freed buffer
				if( Wave->HasAnyFlags( RF_RootSet ) )
				{
					// Allocate from permanent pool and mark buffer as non destructible.
					Buffer->CompressedData = ( BYTE* )AudioDevice->AllocatePermanentMemory( SoundAllocationSize );
					Buffer->bAllocationInPermanentPool = TRUE;
				}
				else
				{
					// Allocate via normal allocator.
					Buffer->CompressedData = ( BYTE* )appMalloc( SoundAllocationSize, 128 );
				}

				// copy off the sound data so it can unload
				appMemcpy( Buffer->CompressedData, RawData, Buffer->CompressedDataSize );

				// Data has 4 DWORDs of size of each mp3 stream (only first 3 are used)
				DWORD* MSFSizes = ( DWORD* )Buffer->CompressedData;
				BYTE* MSFRawData = Buffer->CompressedData + ( sizeof( DWORD ) * 4 );

				for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
				{
					if( MSFSizes[Index] )
					{
						// Examine the MSFHeader
						MSFHeader* MSFData = ( MSFHeader* )MSFRawData;

						// Ensure the mp3 was cooked with the looping option
						checkf( MSFData->LoopMarkers[1] & 0x10, TEXT( "%s was encoded without looping"), *Wave->GetPathName() );

						// fill out PS3 sound data structure
						Buffer->MSInfo[Index].SubBusGroup = static_cast<unsigned char>( CELL_MS_MASTER_BUS );
						Buffer->MSInfo[Index].FirstBuffer = MSFRawData + sizeof( MSFHeader );
						Buffer->MSInfo[Index].FirstBufferSize = MSFData->sampleSize;
						Buffer->MSInfo[Index].SecondBuffer = NULL;
						Buffer->MSInfo[Index].SecondBufferSize = 0;
						Buffer->MSInfo[Index].initialOffset = 0;
						Buffer->MSInfo[Index].Pitch = MSFData->LoopMarkers[0];
						Buffer->MSInfo[Index].numChannels = MSFData->channels;
						Buffer->MSInfo[Index].inputType = CELL_MS_MP3;
						// since we reuse streams, we tell it to not close the stream when the sound is done
						Buffer->MSInfo[Index].flags = CELL_MS_STREAM_NOFLAGS;

						MSFRawData += MSFSizes[Index];
					}
				}

				// Unload raw data.
				Wave->CompressedPS3Data.Unlock();
			}
		}

		// Allocate new resource ID and assign to USoundNodeWave. A value of 0 (default) means not yet registered.
		const INT ResourceID = AudioDevice->NextResourceID++;
		Buffer->ResourceID = ResourceID;
		Wave->ResourceID = ResourceID;

		AudioDevice->Buffers.AddItem( Buffer );
		AudioDevice->WaveBufferMap.Set( ResourceID, Buffer );

		// Keep track of associated resource name.
		Buffer->ResourceName = Wave->GetPathName();
		return( Buffer );
	}
}

/*------------------------------------------------------------------------------------
	FPS3SoundSource.
------------------------------------------------------------------------------------*/

FPS3SoundSource::FPS3SoundSource( UAudioDevice* InAudioDevice, DWORD InSourceIndex )
	:	FSoundSource( InAudioDevice )
	,	AudioDevice( ( UPS3AudioDevice* )InAudioDevice )
	,	PlayStartTime( 0.0 )
	,	SourceIndex( InSourceIndex )
	,	Buffer( NULL )
	,	bLoopWasReached( FALSE )
	,	bAlwaysLoadedPause( FALSE )
	,	FilterWorkMemory( NULL )
{
	// open a stream object
	for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
	{
		SoundStream[Index] = cellMSStreamOpen();
	}
}

FPS3SoundSource::~FPS3SoundSource( void )
{
	if( FilterWorkMemory )
	{
		appFree( FilterWorkMemory );
	}
}

/**
 * Returns the size of this buffer in bytes.
 *
 * @return Size in bytes
 */
INT FPS3SoundBuffer::GetSize( void )
{
	INT TotalSize = 0;

	for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
	{
		if( MSInfo[Index].numChannels )
		{
			TotalSize += MSInfo[Index].FirstBufferSize;
		}
	}

	return( TotalSize );
}

/**
 * Maps a sound with a given number of channels to to expected speakers
 */
void FPS3SoundSource::MapSpeakers( FLOAT Volume )
{
	FLOAT Volumes[4][8][8];
	INT Count = 0;

	appMemset( Volumes, 0, sizeof( Volumes ) );

	switch( WaveInstance->WaveData->NumChannels )
	{
	case 1:
		// Mono file
		Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_FL] = Volume;
		Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_FR] = Volume;

		if( UPS3AudioDevice::NumSpeakers == 6 )
		{
			Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed;
		}

		Count = 1;
		break;

	case 2:
		// Map to front speakers
		Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_FL] = Volume;
		Volumes[0][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_FR] = Volume;

		if( UPS3AudioDevice::NumSpeakers == 6 )
		{
			Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_RL] = Volume * StereoBleed;
			Volumes[0][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_RR] = Volume * StereoBleed;

			Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed * 0.5f;
			Volumes[0][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed * 0.5f;
		}

		Count = 1;
		break;

	case 4:
		// Map 4.0 sound to correct speakers
		Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_FL] = Volume;
		Volumes[0][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_FR] = Volume;
		Volumes[1][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_RL] = Volume;
		Volumes[1][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_RR] = Volume;

		if( UPS3AudioDevice::NumSpeakers == 6 )
		{
			Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed * 0.25f;
			Volumes[0][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed * 0.25f;
			Volumes[1][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed * 0.25f;
			Volumes[1][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_LFE] = Volume * LFEBleed * 0.25f;
		}

		Count = 2;
		break;

	case 6:
		// Map 5.1 sound to correct speakers
		Volumes[0][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_FL] = Volume;
		Volumes[0][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_FR] = Volume;
		Volumes[1][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_FC] = Volume;
		Volumes[1][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_LFE] = Volume;
		Volumes[2][CELL_MS_CHANNEL_0][CELL_MS_SPEAKER_RL] = Volume;
		Volumes[2][CELL_MS_CHANNEL_1][CELL_MS_SPEAKER_RR] = Volume;
		Count = 3;
		break;
	}

	for( INT Index = 0; Index < Count; Index++ )
	{
		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Wet" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_WET, CELL_MS_CHANNEL_0, Volumes[Index][0] ) );

		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Dry" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_DRY, CELL_MS_CHANNEL_0, Volumes[Index][0] ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Dry" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_DRY, CELL_MS_CHANNEL_1, Volumes[Index][1] ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Dry" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_DRY, CELL_MS_CHANNEL_2, Volumes[Index][2] ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Dry" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_DRY, CELL_MS_CHANNEL_3, Volumes[Index][3] ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Dry" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_DRY, CELL_MS_CHANNEL_4, Volumes[Index][4] ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSCoreSetVolume8 Dry" ), cellMSCoreSetVolume8( SoundStream[Index], CELL_MS_DRY, CELL_MS_CHANNEL_5, Volumes[Index][5] ) );
	}
}

/**
 *	FPS3SoundSource::Init
 */
UBOOL FPS3SoundSource::Init( FWaveInstance* InWaveInstance )
{
	UPS3AudioDevice* PS3AudioDevice = ( UPS3AudioDevice* )AudioDevice;

	// find the buffer matching the wave
	Buffer = FPS3SoundBuffer::Init( InWaveInstance->WaveData, PS3AudioDevice );
	if( Buffer )
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );

		if( Buffer->MSInfo[0].numChannels == 0 )
		{
			return( FALSE );
		}

		// Check for trying to play an always loaded sounds that is currently swapped out
		if( PS3AudioDevice->bAlwaysLoadedSoundsSwappedOut && Buffer->bAllocationInPermanentPool )
		{
			return( FALSE );
		}

		// cache the wave instance pointer
		WaveInstance = InWaveInstance;

		// Set whether to apply reverb
		SetReverbApplied( TRUE );

		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( Buffer->MSInfo[Index].numChannels )
			{
				if( WaveInstance->LoopingMode != LOOP_Never )
				{
					// if we're looping, then set both buffers to the same sound so that it will just start playing the sound again as soon as its done playing
					Buffer->MSInfo[Index].SecondBuffer = Buffer->MSInfo[Index].FirstBuffer;
					Buffer->MSInfo[Index].SecondBufferSize = Buffer->MSInfo[Index].FirstBufferSize;
				}
				else
				{
					// clear out the second buffer
					Buffer->MSInfo[Index].SecondBuffer = NULL;
					Buffer->MSInfo[Index].SecondBufferSize = 0;
				}

				// Initialise the stream
				AudioDevice->ValidateAPICall( TEXT( "cellMSCoreInit" ), cellMSCoreInit( SoundStream[Index] ) );

				// hook up the sound buffer's data to this stream
				AudioDevice->ValidateAPICall( TEXT( "cellMSStreamSetInfo" ), cellMSStreamSetInfo( SoundStream[Index], &Buffer->MSInfo[Index] ) );

				// clear the callback
				cellMSStreamSetCallbackFunc( SoundStream[Index], NULL );
			}
		}

		// if we need to notify when loop is complete, setup a callback for the first stream
		bLoopWasReached = FALSE;
		if( WaveInstance->LoopingMode == LOOP_WithNotification )
		{
			cellMSStreamSetCallbackFunc( SoundStream[0], StreamCallback );
			cellMSStreamSetCallbackData( SoundStream[0], this );
		}

		// Initialise the high shelf filter
		AudioDevice->InitEffect( this );
		
		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( Buffer->MSInfo[Index].numChannels )
			{
				if( WaveInstance->LoopingMode != LOOP_Never )
				{
					cellMSStreamSetFirstReadSkip( SoundStream[Index], Buffer->MSInfo[Index].FirstBuffer, Buffer->MSInfo[Index].FirstBufferSize, 0, 0 );
					cellMSStreamSetSecondReadSkip( SoundStream[Index], Buffer->MSInfo[Index].SecondBuffer, Buffer->MSInfo[Index].SecondBufferSize, 0, 0 );
				}

				// Bypass the envelope
				cellMSStreamSetEnvBypass( SoundStream[Index], CELL_MS_BYPASSED );
			}
		}

		// Turn on surround sound if we are a spatial sound
		if( WaveInstance->bUseSpatialization && WaveInstance->WaveData->NumChannels == 1 )
		{
			// Turn on surround sound for this stream and set it to use the default listener
			AudioDevice->ValidateAPICall( TEXT( "cellMSSurroundActive" ), cellMSSurroundActive( SoundStream[0], CELL_MS_SURROUND_ACTIVE_STREAM, DEFAULT_LISTENER, CELL_MS_WET_AND_DRY ) );
		}

		// Setup dynamic properties to their initial state
		Update();

		return( TRUE );
	}

	return( FALSE );
}

/**
 * FPS3SoundSource::Update
 */
void FPS3SoundSource::Update( void )
{
	if( !Buffer || Paused )
	{
		return;
	}

	// Better safe than sorry!
	const FLOAT Volume = Clamp<FLOAT>( WaveInstance->Volume * WaveInstance->VolumeMultiplier, 0.0f, 1.0f );
	const FLOAT Pitch = Clamp<FLOAT>( WaveInstance->Pitch, MIN_PITCH, MAX_PITCH );

	// Set whether to bleed to the rear speakers
	SetStereoBleed();

	// Set the amount to bleed to the LFE speaker
	SetLFEBleed();

	// Set the HighFrequencyGain value
	SetHighFrequencyGain();

	if( WaveInstance->bUseSpatialization && WaveInstance->WaveData->NumChannels == 1 )
	{
		// Update the sound's location and velocity
		FVector Location, Velocity;

		Location = WaveInstance->Location;
		Velocity = WaveInstance->Velocity;

		CalculateALVectorsFromUnrealVectors( &Location, &Velocity, NULL, NULL );

		AudioDevice->ValidateAPICall( TEXT( "cellMSSurroundSourcefv CELL_MS_SURROUND_POSITION" ), cellMSSurroundSourcefv( SoundStream[0], CELL_MS_SURROUND_POSITION, ( FLOAT* )&Location ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSSurroundSourcefv CELL_MS_SURROUND_VELOCITY" ), cellMSSurroundSourcefv( SoundStream[0], CELL_MS_SURROUND_VELOCITY, ( FLOAT* )&Velocity ) );

		// Set pitch and volume
		AudioDevice->ValidateAPICall( TEXT( "cellMSSurroundSourcef CELL_MS_SURROUND_PITCH" ), cellMSSurroundSourcef( SoundStream[0], CELL_MS_SURROUND_PITCH, Pitch ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSSurroundSourcef CELL_MS_SURROUND_GAIN" ), cellMSSurroundSourcef( SoundStream[0], CELL_MS_SURROUND_GAIN, Volume ) );
	}
	else
	{
		// Map the channels to the speakers
		MapSpeakers( Volume );

		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			// Set the pitch in terms of sample rate
			if( Buffer->MSInfo[Index].numChannels )
			{
				cellMSStreamSetPitch( SoundStream[Index], ( INT )( Buffer->MSInfo[Index].Pitch * Pitch ) );
			}
		}
	}

	// Platform dependent call to update the sound output with new parameters
	AudioDevice->UpdateEffect( this );
}

/**
 *	FPS3SoundSource::Play
 */
void FPS3SoundSource::Play( void )
{
	if( WaveInstance )
	{
		if( Paused )
		{
			// update flag
			Paused = FALSE;

			// update volumes
			Update();

			// unpause the stream
			cellMSSystemSetPause( CELL_MS_PAUSED );
			for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
			{
				if( Buffer->MSInfo[Index].numChannels )
				{
					cellMSCoreSetPause( SoundStream[Index], CELL_MS_NOTPAUSED );
				}
			}
			cellMSSystemSetPause( CELL_MS_NOTPAUSED );
		}
		else
		{
			// set status flags
			Paused = FALSE;
			Playing = TRUE;

			// play the sound!
			cellMSSystemSetPause( CELL_MS_PAUSED );
			for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
			{
				if( Buffer->MSInfo[Index].numChannels )
				{
					cellMSStreamPlay( SoundStream[Index] );
				}
			}
			cellMSSystemSetPause( CELL_MS_NOTPAUSED );

			// set volume, etc
			Update();
		}
	}
}

/**
 *	FPS3SoundSource::Stop
 */
void FPS3SoundSource::Stop( void )
{
	if( WaveInstance )
	{
		// stop the stream
		cellMSSystemSetPause( CELL_MS_PAUSED );
		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( Buffer->MSInfo[Index].numChannels )
			{
				cellMSCoreStop( SoundStream[Index], CELL_MS_KEYOFF );
			}
		}
		cellMSSystemSetPause( CELL_MS_NOTPAUSED );

		// update flags
		Paused = FALSE;
		Playing = FALSE;

		// unset our buffer (to be reset in Play)
		Buffer = NULL;
	}

	FSoundSource::Stop();
}

/**
 *	UPS3SoundSource::Pause
 */
void FPS3SoundSource::Pause( void )
{
	if( WaveInstance )
	{
		// pause the stream
		cellMSSystemSetPause( CELL_MS_PAUSED );
		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( Buffer->MSInfo[Index].numChannels )
			{
				cellMSCoreSetPause( SoundStream[Index], CELL_MS_PAUSED );
			}
		}
		cellMSSystemSetPause( CELL_MS_NOTPAUSED );

		// mark us paused
		Paused = TRUE;
	}
}

/**
 *	FPS3SoundSource::IsFinished
 */
UBOOL FPS3SoundSource::IsFinished( void )
{
	// Paused sounds never finish
	if( Paused )
	{
		return( FALSE );
	}

	if( WaveInstance )
	{
		// is the stream done playing?
		UBOOL bIsDonePlaying = ( cellMSStreamGetStatus( SoundStream[0] ) == CELL_MS_STREAM_OFF );

		// If stream is off, we're done playing
		if( bIsDonePlaying )
		{
			return( TRUE );
		}

		// If we're counting the number of loops, interrogate the wave instance to see if we have finished
		if( WaveInstance->LoopingMode == LOOP_WithNotification && bLoopWasReached )
		{
			bLoopWasReached = FALSE;
			// If the loop count has finished, kill the buffer swap, so the cellMSStreamGetStatus will return CELL_MS_STREAM_OFF
			// and hit the bIsDonePlaying check
			if( WaveInstance->NotifyFinished() )
			{
				cellMSStreamSetSecondReadSkip( SoundStream[0], NULL, 0, 0, 0 );
			}
		}

		return( FALSE );
	}

	// No wave instance means we must have finished
	return( TRUE );
}


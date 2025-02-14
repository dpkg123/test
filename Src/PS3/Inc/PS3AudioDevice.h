/*=============================================================================
	PS3AudioDevice.h: Unreal PS3Audio interface object.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3AUDIODEVICE_H__
#define __PS3AUDIODEVICE_H__

#include <cell/mstream.h>
#include <cell/spurs/control.h>
#include <cell/spurs/task.h>
#include <cell/spurs/event_flag.h>
#include <cell/audio.h>

class UPS3AudioDevice;

#define USE_SULPHA_DEBUGGING	0

#if USE_SULPHA_DEBUGGING
#define SUPLHA_MEMORY_SIZE		( 32 * 1024 * 1024 )
#endif

/** Bus to route signals for effects processing */
#define UN_REVERB_BUS			( 1 | CELL_MS_BUS_FLAG )
#define UN_EQ_BUS				( 2 | CELL_MS_BUS_FLAG )
#define UN_EQ_AND_REVERB_BUS	( 3 | CELL_MS_BUS_FLAG )
#define UN_BUS_COUNT			3

/** Each sound is made up of upto this many stereo sounds */
#define NUM_SUB_CHANNELS		3

struct MSFHeader
{
	char	header[4];
	int		compressionType;
	int		channels;
	int		sampleSize;			// size in bytes
	int		LoopMarkers[12];
};

class FPS3SoundBuffer
{
public:
	/** Constructor */
	FPS3SoundBuffer( UPS3AudioDevice* AudioDevice );

	/** Destructor */
	~FPS3SoundBuffer( void );

	/** Returns the size of this buffer in bytes. */
	INT GetSize( void );

	/** Audio device this buffer is attached to	*/
	UPS3AudioDevice*			AudioDevice;

	/** Structure required to interface with the PS3 API */
	CellMSInfo					MSInfo[NUM_SUB_CHANNELS];

	/** Blob containing header info and all associated mp3 streams */
	BYTE*						CompressedData;

	/** Size of all compressed data combined */
	DWORD						CompressedDataSize;

	/** Resource ID of associated USoundNodeWave */
	INT							ResourceID;

	/** Human readable name of resource, most likely name of UObject associated during caching. */
	FString						ResourceName;

	/** Whether the sound is to remain resident forever */
	UBOOL						bAllocationInPermanentPool;

	static FPS3SoundBuffer* Init( USoundNodeWave* Wave, UPS3AudioDevice* AudioDevice );
};

/*------------------------------------------------------------------------------------
	FPS3SoundSource
------------------------------------------------------------------------------------*/

class FPS3SoundSource : public FSoundSource
{
public:
	// Constructor/ Destructor.
	FPS3SoundSource( UAudioDevice* InAudioDevice, DWORD InSourceIndex );
	~FPS3SoundSource( void );

	// Initialization & update.
	virtual UBOOL Init( FWaveInstance* WaveInstance );
	virtual void Update( void );

	// Playback.
	virtual void Play( void );
	virtual void Stop( void );
	virtual void Pause( void );

	// Query.
	virtual UBOOL IsFinished( void );

	/** Stream number this source is playing on */
	INT							SoundStream[NUM_SUB_CHANNELS];

protected:
	/** Cached pointer to the audio device */
	class UPS3AudioDevice*		AudioDevice;

	/** The time this sound was started playing - needed to determine if the sound is finished */
	DOUBLE						PlayStartTime;
	
	/** How far into the sound we were when it was paused, used to reset starttime when unpausing */
	FLOAT						PausedOffset;						

	/** This is the index into the global channel strip list for each type */
	DWORD						SourceIndex;

	/** Currently bound wave objects */
	FPS3SoundBuffer*			Buffer;

	/** Was a loop reached in the callback? */
	UBOOL						bLoopWasReached;

	/** Paused as an always-loaded sound? */
	UBOOL						bAlwaysLoadedPause;

	/** Work memory required by the low pass filter */
	void*						FilterWorkMemory;

	/** 
	 * Maps a sound with a given number of channels to to expected speakers
	 */
	void MapSpeakers( FLOAT Volume );

	friend class UPS3AudioDevice;
	friend class FPS3AudioEffectsManager;

public:
	friend void StreamCallback( int StreamNumber, void* UserData, int CallbackType, void* ReadBuffer, int ReadSize );
};

/** 
 *
 */
class FPS3AudioEffectsManager : public FAudioEffectsManager
{
public:
	FPS3AudioEffectsManager( UAudioDevice* InDevice, TArray<FSoundSource*> Sources );
	~FPS3AudioEffectsManager( void );

	/** 
	 * Calls the platform specific code to set the parameters that define reverb
	 */
	virtual void SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters );

	/** 
	 * Calls the platform specific code to set the parameters that define EQ
	 */
	virtual void SetEQEffectParameters( const FAudioEQEffect& ReverbEffectParameters );

	/** 
	 * Platform dependent call to init effect data on a sound source
	 */
	virtual void* InitEffect( FSoundSource* Source );

	/** 
	 * Platform dependent call to update the sound output with new parameters
	 */
	virtual void* UpdateEffect( FSoundSource* Source );

private:
	/** 
	 * Load up a DSP effect and force the handle
	 */
	int LoadDSP( const TCHAR* Title, int Handle, void* dsp_pic_start );

	/** 
	 * Convert the standard I3DL2 reverb parameters to the PS3 specific
	 */
	void ConvertI3DL2ToPS3( const FAudioReverbEffect& ReverbEffectParameters, CellMSFXI3DL2Params* MSReverbParameters, INT NumChannels );

	/** 
	 * Convert the EQ ranges to the PS3 table
	 */
	void ConvertEQToPS3( const FAudioEQEffect& EQEffectParameters, CellMSFXParaEQ& MSEQParameters );

	/** Handle to the reverb DSP PIC */
	int			ReverbDSPHandle;
	char*		ReverbFilterParameters;
	void*		ReverbFilterBuffer;

	/** Handle to the para EQ DSP PIC */
	int			ParaEQDSPHandle;
	char*		EQFilterParameters;
	char*		EQ2FilterParameters;
	void*		EQFilterBuffer;
	void*		EQ2FilterBuffer;

	/** Handle to filter PIC */
	int			FilterDSPHandle;
};

/*------------------------------------------------------------------------------------
	UPS3AudioDevice
------------------------------------------------------------------------------------*/

class UPS3AudioDevice : public UAudioDevice
{
	DECLARE_CLASS_INTRINSIC( UPS3AudioDevice, UAudioDevice, CLASS_Config, PS3Drv )

	/**
	 * Static constructor, used to associate .ini options with member variables.	
	 */
	void StaticConstructor( void );

	// UAudioDevice interface.

	/**
	 * Initializes the audio device and creates sources.
	 *
	 * @return TRUE if initialization was successful, FALSE otherwise
	 */
	virtual UBOOL Init( void );

	/**
	 * Update the audio device
	 *
	 * @param	Realtime	whether we are paused or not
	 */
	virtual void Update( UBOOL Realtime );

	/**
	 * Precaches the passed in sound node wave object.
	 *
	 * @param	SoundNodeWave	Resource to be precached.
	 */
	virtual void Precache( USoundNodeWave* SoundNodeWave );

	/** 
	 * Lists all the loaded sounds and their memory footprint
	 */
	virtual void ListSounds( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Sets the 'pause' state of sounds which are always loaded.
	 *
	 * @param	bPaused			Pause sounds if TRUE, play paused sounds if FALSE.
	 */
	virtual void PauseAlwaysLoadedSounds(UBOOL bPaused);

	/**
	 * Frees the bulk resource data associated with this SoundNodeWave.
	 *
	 * @param	SoundNodeWave	wave object to free associated bulk data
	 */
	virtual void FreeResource( USoundNodeWave* SoundNodeWave );

	// UObject interface.

	/**
	 * Shuts down audio device. This will never be called with the memory image codepath.
	 */
	virtual void FinishDestroy( void );

	/**
	 * Special variant of Destroy that gets called on fatal exit. Doesn't really
	 * matter on the console so for now is just the same as Destroy so we can
	 * verify that the code correctly cleans up everything.
	 */
	virtual void ShutdownAfterError( void );

	/** 
	 * Check for errors and output a human readable string 
	 */
	virtual UBOOL ValidateAPICall( const TCHAR* Function, INT ErrorCode );

	/** Return the port number Multistream is outputting to */
	DWORD GetPortNum( void ) 
	{ 
		return( PortNum ); 
	}

	/** 
	 * Initialize the cell audio and configure multistream settings based on user's system config
	 * Only initialized once
	 *
	 * @return TRUE if initialized correctly
	 */
	static UBOOL InitCellAudio( void );

	/** Variables required for the early init */
	static INT							NumSpeakers;

protected:
	/** Actually bring up the mixer */
	UBOOL StartMixer( void );

	/** Cleanup. */
	void Teardown( void );

	/**
     * Allocates memory from permanent pool. This memory will NEVER be freed.
	 *
	 * @param	Size	Size of allocation.
	 *
	 * @return pointer to a chunk of memory with size Size
	 */
	void* AllocatePermanentMemory( INT Size );

	/** Array of all created buffers associated with this audio device */
	TArray<FPS3SoundBuffer*>			Buffers;
	/** Look up associating a USoundNodeWave's resource ID with low level sound buffers	*/
	TMap<INT, FPS3SoundBuffer*>	WaveBufferMap;

	/** Next resource ID value used for registering USoundNodeWave objects */
	INT									NextResourceID;

	UINT								PortNum;
	CellAudioPortConfig					PortConfig;
	void*								MultiStreamMemory;
	sys_ppu_thread_t					MultiStreamUpdateThread;
	void*								CompressedMemory;
	void*								MSSurroundMemory;

#if USE_SULPHA_DEBUGGING
	void*								SulphaMemory;
#endif

public:
	static const FLOAT MasterBusVolumes[64];
	static const FLOAT MaxBusVolumes[64];
	static const FLOAT ZeroBusVolumes[64];

	/** Sleep time (milliseconds) in multistream update - higher value is less intrusive */
	FLOAT								TimeBetweenHWUpdates;

	/** Flag to reflect the state of the memory container and the always loaded sounds */
	UBOOL								bAlwaysLoadedSoundsSwappedOut;

	friend class FPS3SoundSource;
	friend class FPS3SoundBuffer;
};

#endif

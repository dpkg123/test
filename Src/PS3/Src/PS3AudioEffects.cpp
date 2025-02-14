/*=============================================================================
	PS3AudioDevice.cpp: Unreal Multistream interface object.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.

	Unreal is RHS with Y and Z swapped (or technically LHS with flipped axis)

=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "Engine.h"
#include "EngineSoundClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "UnAudioEffect.h"
#include "PS3AudioDevice.h"

/** 
 * Addresses of the prebuilt pic SPU programs that are linked in
 */
extern char _binary_mstream_dsp_i3dl2_pic_start[];
extern char _binary_mstream_dsp_i3dl2_pic_end[];
extern char _binary_mstream_dsp_i3dl2_pic_size[];

extern char _binary_mstream_dsp_filter_pic_start[];
extern char _binary_mstream_dsp_filter_pic_end[];
extern char _binary_mstream_dsp_filter_pic_size[];

extern char _binary_mstream_dsp_para_eq_pic_start[];
extern char _binary_mstream_dsp_para_eq_pic_end[];
extern char _binary_mstream_dsp_para_eq_pic_size[];

/** 
 * Convert the standard I3DL2 reverb parameters to the PS3 specific
 */
void FPS3AudioEffectsManager::ConvertI3DL2ToPS3( const FAudioReverbEffect& ReverbEffectParameters, CellMSFXI3DL2Params* MSReverbParameters, INT NumChannels )
{
	MSReverbParameters[0].Room = VolumeToMilliBels( ReverbEffectParameters.Volume * ReverbEffectParameters.Gain, 0 );
	MSReverbParameters[0].Room_HF = VolumeToMilliBels( ReverbEffectParameters.GainHF, -45 );
	MSReverbParameters[0].Room_LF = VolumeToMilliBels( 1.0f - ReverbEffectParameters.GainHF, -45 );
	MSReverbParameters[0].Density = ReverbEffectParameters.Density * 100.0f;
	MSReverbParameters[0].Diffusion = ReverbEffectParameters.Diffusion * 100.0f;
	MSReverbParameters[0].Decay_time = ReverbEffectParameters.DecayTime;
	MSReverbParameters[0].Reflections = VolumeToMilliBels( ReverbEffectParameters.ReflectionsGain, 1000 );
	MSReverbParameters[0].Reflections_delay = ReverbEffectParameters.ReflectionsDelay;
	MSReverbParameters[0].Reverb = VolumeToMilliBels( ReverbEffectParameters.LateGain, 2000 );
	MSReverbParameters[0].Reverb_delay = ReverbEffectParameters.LateDelay;
	MSReverbParameters[0].EarlyReflectionScaler = 50.0f;			// scaler 0 -> 100% for early reflections. 0 == single reflection 100 == widely spread reflections
	MSReverbParameters[0].Decay_HF_ratio = ReverbEffectParameters.DecayHFRatio;
	MSReverbParameters[0].LF_reference = DEFAULT_LOW_FREQUENCY;
	MSReverbParameters[0].HF_reference = DEFAULT_HIGH_FREQUENCY;

	for( INT i = 0; i < NumChannels; i++ )
	{
		if( i != 0 )
		{
			MSReverbParameters[i] = MSReverbParameters[0];
		}

		switch( i )
		{
		case CELL_MS_SPEAKER_FL:
		case CELL_MS_SPEAKER_RL:
			MSReverbParameters[i].EarlyReflectionPattern = CELL_MSFX_ROOM1_LEFT;
			MSReverbParameters[i].LateReverbPattern = CELL_MSFX_ROOM1_LATE_LEFT;
			MSReverbParameters[i].MixAmount = 20.0f;
			MSReverbParameters[i].MixChannel = i + 1;
			break;

		case CELL_MS_SPEAKER_FR:
		case CELL_MS_SPEAKER_RR:
			MSReverbParameters[i].EarlyReflectionPattern = CELL_MSFX_ROOM1_RIGHT;
			MSReverbParameters[i].LateReverbPattern = CELL_MSFX_ROOM1_LATE_RIGHT;
			MSReverbParameters[i].MixAmount = 20.0f;
			MSReverbParameters[i].MixChannel = i - 1;
			break;

		case CELL_MS_SPEAKER_FC:
		case CELL_MS_SPEAKER_LFE:
		default:
			MSReverbParameters[i].EarlyReflectionPattern = CELL_MSFX_ROOM1_LEFT;
			MSReverbParameters[i].LateReverbPattern = CELL_MSFX_ROOM1_LATE_RIGHT;
			MSReverbParameters[i].MixAmount = 0.0f;
			MSReverbParameters[i].MixChannel = -1;
			break;
		}
	}
}

/** 
 * Convert frequency ranges to the PS3 table
 */
void FPS3AudioEffectsManager::ConvertEQToPS3( const FAudioEQEffect& EQEffectParameters, CellMSFXParaEQ& MSEQParameters )
{
	MSEQParameters.Filters[0].FilterMode = CELL_MSFX_FILTERMODE_LOWSHELF;
	MSEQParameters.Filters[0].fFrequency = EQEffectParameters.LFFrequency;
	MSEQParameters.Filters[0].fResonance = 0.0f;
	MSEQParameters.Filters[0].fGain = VolumeToDeciBels( EQEffectParameters.LFGain );

	MSEQParameters.Filters[1].FilterMode = CELL_MSFX_FILTERMODE_PEAK;
	MSEQParameters.Filters[1].fFrequency = EQEffectParameters.MFCutoffFrequency;
	MSEQParameters.Filters[1].fResonance = EQEffectParameters.MFBandwidth;
	MSEQParameters.Filters[1].fGain = VolumeToDeciBels( EQEffectParameters.MFGain );

	MSEQParameters.Filters[2].FilterMode = CELL_MSFX_FILTERMODE_HIGHSHELF;
	MSEQParameters.Filters[2].fFrequency = EQEffectParameters.HFFrequency;
	MSEQParameters.Filters[2].fResonance = 0.0f;
	MSEQParameters.Filters[2].fGain = VolumeToDeciBels( EQEffectParameters.HFGain );

	MSEQParameters.Filters[3].FilterMode = CELL_MSFX_FILTERMODE_OFF;
	MSEQParameters.Filters[3].fFrequency = 0.0f;
	MSEQParameters.Filters[3].fResonance = 0.0f;
	MSEQParameters.Filters[3].fGain = VolumeToDeciBels( 0.0f );
}

/** 
 * Load up a DSP effect and force the handle
 */
int FPS3AudioEffectsManager::LoadDSP( const TCHAR* Title, int Handle, void* dsp_pic_start )
{
	CellMSDSP DSPInfo = { 0 };

	Handle = cellMSDSPLoadDSPFromMemory( dsp_pic_start, &DSPInfo, Handle, CELL_MS_ALLOC_PAGE );
	if( Handle < 0 )
	{
		debugf( NAME_Sound, TEXT( "Error loading '%s' pic - Handle: %d, Error %08x" ), Title, Handle, cellMSSystemGetLastError() );
	}
	else
	{
		debugf( NAME_Sound, TEXT( "Loaded '%s' DSP. Mem used %d, mem available : %d" ), Title, DSPInfo.memoryUsed, DSPInfo.memoryAvail );
	}

	return( Handle );
}

/** 
 * Construction and init of all PS3 effect related sound code
 * 
 * Busses collect the streams and apply a filter to them
 * Slot is a DSP slot on a bus to hold a filter
 * Parameters are how the code passes arguments to the DSP
 * FilterBuffers are workspace memory for the DSP
 * Channels are what gets routed to the speakers
 */
FPS3AudioEffectsManager::FPS3AudioEffectsManager( UAudioDevice* InDevice, TArray<FSoundSource*> Sources )
	: FAudioEffectsManager( InDevice )
{
	INT	i, ErrorCode;

	ReverbDSPHandle = LoadDSP( TEXT( "I3DL2" ), 0, _binary_mstream_dsp_i3dl2_pic_start );
	ParaEQDSPHandle = LoadDSP( TEXT( "ParaEQ" ), 1, _binary_mstream_dsp_para_eq_pic_start );
	FilterDSPHandle = LoadDSP( TEXT( "Filter" ), 2, _binary_mstream_dsp_filter_pic_start );

	// Check to see if all DSPs loaded fine
	if( ReverbDSPHandle < 0 || FilterDSPHandle < 0 || ParaEQDSPHandle < 0 )
	{
		return;
	}

	// Make sure all busses do not input or output anything at all
	for( i = 0; i < 8; i++ )
	{
		cellMSCoreSetMask( UN_EQ_BUS, CELL_MS_INMASK, CELL_MS_DSP_SLOT_0, i, 0 );
		cellMSCoreSetMask( UN_EQ_BUS, CELL_MS_OUTMASK, CELL_MS_DSP_SLOT_0, i, 0 );

		cellMSCoreSetMask( UN_REVERB_BUS, CELL_MS_INMASK, CELL_MS_DSP_SLOT_0, i, 0 );
		cellMSCoreSetMask( UN_REVERB_BUS, CELL_MS_OUTMASK, CELL_MS_DSP_SLOT_0, i, 0 );

		cellMSCoreSetMask( UN_EQ_AND_REVERB_BUS, CELL_MS_INMASK, CELL_MS_DSP_SLOT_0, i, 0 );
		cellMSCoreSetMask( UN_EQ_AND_REVERB_BUS, CELL_MS_OUTMASK, CELL_MS_DSP_SLOT_0, i, 0 );
	}


	// Setup some defaults that have no reverb
	CellMSFXI3DL2Params MSReverbParameters[SPEAKER_COUNT];
	ConvertI3DL2ToPS3( ReverbPresets[0], MSReverbParameters, SPEAKER_COUNT );

	// Setup some defaults that have no EQ
	CellMSFXParaEQ MSEQParameters;
	FAudioEQEffect EQDefault;
	ConvertEQToPS3( EQDefault, MSEQParameters );

	// Set the DSP slots to hold the correct DSP pic
	cellMSCoreSetDSP( UN_EQ_BUS, CELL_MS_DSP_SLOT_0, ParaEQDSPHandle );
	cellMSCoreSetDSP( UN_REVERB_BUS, CELL_MS_DSP_SLOT_0, ReverbDSPHandle );
	cellMSCoreSetDSP( UN_EQ_AND_REVERB_BUS, CELL_MS_DSP_SLOT_0, ParaEQDSPHandle );

	// Get the address of the parameters for the DSP slot
	EQFilterParameters = ( char* )cellMSCoreGetDSPParamAddr( UN_EQ_BUS, CELL_MS_DSP_SLOT_0 );
	ReverbFilterParameters = ( char* )cellMSCoreGetDSPParamAddr( UN_REVERB_BUS, CELL_MS_DSP_SLOT_0 );
	EQ2FilterParameters = ( char* )cellMSCoreGetDSPParamAddr( UN_EQ_AND_REVERB_BUS, CELL_MS_DSP_SLOT_0 );

	// Get the EQ workspace memory for 8 channels
	INT ParaEQSize = cellMSFXParaEQGetNeededMemorySize( SPEAKER_COUNT );
	EQFilterBuffer = GMalloc->Malloc( ParaEQSize, 128 );
	EQ2FilterBuffer = GMalloc->Malloc( ParaEQSize, 128 );
	debugf( TEXT( "... allocated para EQ workspace: %0.1fK" ), ParaEQSize / 512.0f );

	// Get the reverb workspace memory for 8 channels
	INT ReverbSize = cellMSFXI3DL2GetNeededMemorySize( SPEAKER_COUNT );
	ReverbFilterBuffer = GMalloc->Malloc( ReverbSize, 128 );
	debugf( TEXT( "... allocated reverb workspace : %0.1fK" ), ReverbSize / 1024.0f );

	// Init the effect to use some default parameters
	AudioDevice->ValidateAPICall( TEXT( "cellMSFXParaEQInit" ), cellMSFXParaEQInit( EQFilterParameters, EQFilterBuffer, &MSEQParameters, SPEAKER_COUNT ) );
	AudioDevice->ValidateAPICall( TEXT( "cellMSFXI3DL2Init" ), cellMSFXI3DL2Init( ReverbFilterParameters, ReverbFilterBuffer, MSReverbParameters, SPEAKER_COUNT ) );
	AudioDevice->ValidateAPICall( TEXT( "cellMSFXParaEQInit" ), cellMSFXParaEQInit( EQ2FilterParameters, EQ2FilterBuffer, &MSEQParameters, SPEAKER_COUNT ) );

	// Set the input and output for the DSP slot
	for( i = 0; i < SPEAKER_COUNT; i++ )
	{
		cellMSCoreSetMask( UN_EQ_BUS, CELL_MS_INMASK, CELL_MS_DSP_SLOT_0, i, 1 << i );	
		cellMSCoreSetMask( UN_EQ_BUS, CELL_MS_OUTMASK, CELL_MS_DSP_SLOT_0, i, 1 << i );	

		cellMSCoreSetMask( UN_REVERB_BUS, CELL_MS_INMASK, CELL_MS_DSP_SLOT_0, i, 1 << i );	
		cellMSCoreSetMask( UN_REVERB_BUS, CELL_MS_OUTMASK, CELL_MS_DSP_SLOT_0, i, 1 << i );	

		cellMSCoreSetMask( UN_EQ_AND_REVERB_BUS, CELL_MS_INMASK, CELL_MS_DSP_SLOT_0, i, 1 << i );	
		cellMSCoreSetMask( UN_EQ_AND_REVERB_BUS, CELL_MS_OUTMASK, CELL_MS_DSP_SLOT_0, i, 1 << i );	
	}

	// Set the parameters for each channel of the bus
	for( i = 0; i < SPEAKER_COUNT; i++ )
	{
		AudioDevice->ValidateAPICall( TEXT( "cellMSFXParaEQSet" ), cellMSFXParaEQSet( EQFilterParameters, &MSEQParameters, i ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSFXI3DL2SetParams" ), cellMSFXI3DL2SetParams( ReverbFilterParameters, ReverbFilterBuffer, MSReverbParameters + i, i ) );
		AudioDevice->ValidateAPICall( TEXT( "cellMSFXParaEQSet" ), cellMSFXParaEQSet( EQ2FilterParameters, &MSEQParameters, i ) );
	}

	// Make sure the bus uses the DSP slot
	cellMSCoreBypassDSP( UN_EQ_BUS, CELL_MS_DSP_SLOT_0, CELL_MS_NOTBYPASSED );
	cellMSCoreBypassDSP( UN_REVERB_BUS, CELL_MS_DSP_SLOT_0, CELL_MS_NOTBYPASSED );
	cellMSCoreBypassDSP( UN_EQ_AND_REVERB_BUS, CELL_MS_DSP_SLOT_0, CELL_MS_NOTBYPASSED );

	bEffectsInitialised = TRUE;
}

FPS3AudioEffectsManager::~FPS3AudioEffectsManager( void )
{
	if( EQFilterBuffer )
	{
		GMalloc->Free( EQFilterBuffer );
	}

	if( ReverbFilterBuffer )
	{
		GMalloc->Free( ReverbFilterBuffer );
	}

	if( EQ2FilterBuffer )
	{
		GMalloc->Free( EQ2FilterBuffer );
	}
}

/** 
 * Platform dependent call to init effect data on a sound source
 */
void* FPS3AudioEffectsManager::InitEffect( FSoundSource* Source )
{
	if( bEffectsInitialised )
	{
		CellMSFXFilter FilterInfo;
		FPS3SoundSource* PS3Source = static_cast< FPS3SoundSource* >( Source );

		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( PS3Source->Buffer->MSInfo[Index].numChannels )
			{
				cellMSCoreSetDSP( PS3Source->SoundStream[Index], CELL_MS_DSP_SLOT_0, FilterDSPHandle );
				void* Parameters = cellMSCoreGetDSPParamAddr( PS3Source->SoundStream[Index], CELL_MS_DSP_SLOT_0 );

				FilterInfo.fFrequency = DEFAULT_HIGH_FREQUENCY;
				FilterInfo.FilterMode = CELL_MSFX_FILTERMODE_HIGHSHELF;
				FilterInfo.fResonance = 0.9f;
				FilterInfo.fGain = VolumeToDeciBels( PS3Source->HighFrequencyGain );

				if( PS3Source->FilterWorkMemory == NULL )
				{
					INT RequiredMemory = cellMSFXFilterGetNeededMemorySize( 2 );
					PS3Source->FilterWorkMemory = appMalloc( RequiredMemory, 128 );
				}

				AudioDevice->ValidateAPICall( TEXT( "cellMSFXFilterInit" ), cellMSFXFilterInit( Parameters, PS3Source->FilterWorkMemory, &FilterInfo ) );

				cellMSCoreBypassDSP( PS3Source->SoundStream[Index], CELL_MS_DSP_SLOT_0, CELL_MS_NOTBYPASSED );
			}
		}
	}

	return( NULL );
}

/** 
 * Sets the output of the source to send to the effect (reverb)
 */
void* FPS3AudioEffectsManager::UpdateEffect( FSoundSource* Source ) 
{
	CellMSFXFilter FilterInfo;

	if( !bEffectsInitialised )
	{
		return( NULL );
	}

	FPS3SoundSource* PS3Source = static_cast< FPS3SoundSource* >( Source );

	// Allow easy testing of EQ filter
	UBOOL bApplyEQ = ( Source->IsEQFilterApplied() );

	// Default bus if sound is completely dry
	INT DestBus = CELL_MS_MASTER_BUS;

	if( Source->IsReverbApplied() && bApplyEQ )
	{
		DestBus = UN_EQ_AND_REVERB_BUS;
	}
	else if( Source->IsReverbApplied() )
	{
		DestBus = UN_REVERB_BUS;
	}
	else if( bApplyEQ )
	{
		DestBus = UN_EQ_BUS;
	}

	if( PS3Source->HighFrequencyGain == 1.0f )
	{
		// If there's no LPF component, output the *DRY* signal to the sub bus
		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			if( PS3Source->Buffer->MSInfo[Index].numChannels )
			{
				AudioDevice->ValidateAPICall( TEXT( "cellMSStreamSetSub" ), cellMSStreamSetSub( PS3Source->SoundStream[Index], DestBus ) );

				cellMSCoreSetBypass( PS3Source->SoundStream[Index], CELL_MS_DRY, CELL_MS_NOTBYPASSED );
				cellMSCoreSetBypass( PS3Source->SoundStream[Index], CELL_MS_WET, CELL_MS_BYPASSED );	
			}
		}
	}
	else
	{
		for( INT Index = 0; Index < NUM_SUB_CHANNELS; Index++ )
		{
			// If there is an LPF component, output the *WET* signal to the sub bus
			if( PS3Source->Buffer->MSInfo[Index].numChannels )
			{
				// Enable the high shelf filter
				if( PS3Source->FilterWorkMemory )
				{
					// Set the high shelf filter parameters
					void* Parameters = cellMSCoreGetDSPParamAddr( PS3Source->SoundStream[Index], CELL_MS_DSP_SLOT_0 );

					FilterInfo.fFrequency = DEFAULT_HIGH_FREQUENCY;
					FilterInfo.FilterMode = CELL_MSFX_FILTERMODE_HIGHSHELF;
					FilterInfo.fResonance = AudioDevice->LowPassFilterResonance;
					FilterInfo.fGain = VolumeToDeciBels( PS3Source->HighFrequencyGain );

					cellMSFXFilterSet( Parameters, &FilterInfo, 0 );
				}

				AudioDevice->ValidateAPICall( TEXT( "cellMSStreamSetSub" ), cellMSStreamSetSub( PS3Source->SoundStream[Index], DestBus ) );

				cellMSCoreSetBypass( PS3Source->SoundStream[Index], CELL_MS_DRY, CELL_MS_BYPASSED );
				cellMSCoreSetBypass( PS3Source->SoundStream[Index], CELL_MS_WET, CELL_MS_NOTBYPASSED );	
			}
		}
	}

	return( NULL ); 
}

/** 
 * Calls the platform specific code to set the parameters that define reverb
 */
void FPS3AudioEffectsManager::SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters )
{
	if( bEffectsInitialised )
	{
		CellMSFXI3DL2Params MSReverbParameters[SPEAKER_COUNT];

		ConvertI3DL2ToPS3( ReverbEffectParameters, MSReverbParameters, SPEAKER_COUNT );
		switch( AudioDevice->GetMixDebugState() )
		{
		case DEBUGSTATE_None:
			cellMSCoreSetVolume64( UN_REVERB_BUS, CELL_MS_WET_AND_DRY, UPS3AudioDevice::MaxBusVolumes );
			break;

		case DEBUGSTATE_IsolateDryAudio:
			cellMSCoreSetVolume64( UN_REVERB_BUS, CELL_MS_DRY, UPS3AudioDevice::MaxBusVolumes );
			cellMSCoreSetVolume64( UN_REVERB_BUS, CELL_MS_WET, UPS3AudioDevice::ZeroBusVolumes );
			break;

		case DEBUGSTATE_IsolateReverb:
			cellMSCoreSetVolume64( UN_REVERB_BUS, CELL_MS_DRY, UPS3AudioDevice::ZeroBusVolumes );
			cellMSCoreSetVolume64( UN_REVERB_BUS, CELL_MS_WET, UPS3AudioDevice::MaxBusVolumes );
			break;
		}

		for( INT i = 0; i < SPEAKER_COUNT; i++ )
		{
			AudioDevice->ValidateAPICall( TEXT( "cellMSFXI3DL2SetParams" ), cellMSFXI3DL2SetParams( ReverbFilterParameters, ReverbFilterBuffer, MSReverbParameters + i, i ) );
		}
	}
}

/** 
 * Calls the platform specific code to set the parameters that define EQ
 */
void FPS3AudioEffectsManager::SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters ) 
{
	if( bEffectsInitialised )
	{
		CellMSFXParaEQ MSEQParameters;

		ConvertEQToPS3( EQEffectParameters, MSEQParameters );

		// Apply effects to all channels
		for( INT i = 0; i < SPEAKER_COUNT; i++ )
		{
			// Same parameters are applied to all channels
			AudioDevice->ValidateAPICall( TEXT( "cellMSFXParaEQSet" ), cellMSFXParaEQSet( EQFilterParameters, &MSEQParameters, i ) );
			AudioDevice->ValidateAPICall( TEXT( "cellMSFXParaEQSet" ), cellMSFXParaEQSet( EQ2FilterParameters, &MSEQParameters, i ) );
		}
	}
}

// end

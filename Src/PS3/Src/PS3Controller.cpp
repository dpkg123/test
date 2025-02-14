/*=============================================================================
	PS3Controller.cpp: UPS3ForceFeedbackManager code. This is the interface
	to a PS3 controller, as well as a platform specific implementation of 
	the UForceFeedbackManager interface.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sys/tty.h>
#include "PS3Drv.h"

#if !FINAL_RELEASE
#include "PS3Gcm.h"
#endif	// FINAL_RELEASE


#if USE_PS3_MOVE
// @todo ps3 move: This should be in the SDK at some point
#include "MoveKit.h"
#endif

#define ENABLE_PS3_RUMBLE 1


UBOOL bUseFilter;
UBOOL bCalibrateThisFrame;

IMPLEMENT_CLASS(UPS3ForceFeedbackManager);

TMap<WORD, FName> FPS3Controller::KeyMapVirtualToName;
TMap<FName, WORD> FPS3Controller::KeyMapNameToVirtual;
TMap<BYTE, FName> FPS3Controller::ButtonMapVirtualToName;
TMap<FName, BYTE> FPS3Controller::ButtonMapNameToVirtual;

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
TArray<FString> FPS3Controller::TTYCommands;
FCriticalSection FPS3Controller::TTYCriticalSection;

void _TTYThreadProc(QWORD Arg)
{
	// An ID that is this players console (debugger or some other TTY channel on PC host)
	const INT TTYChannel = (INT)Arg;
	// A buffer to queue up input characters as they are typed, when a CR is detected, send the command to the Input system
	ANSICHAR ConsoleInputBuffer[2048];
	// length of the string that was read in
	UINT NumBytes;

	// print out a welcome message
	sprintf( ConsoleInputBuffer, "\n\nUE3 Input Console (TTY channel %d [USER %d])\nEnter a console command and hit return.\nEnable echo for ProDG (and don't use backspace)...\nOr use 'dtccons %d' from commandline", TTYChannel, TTYChannel - 2, TTYChannel);
	sys_tty_write(TTYChannel, ConsoleInputBuffer, strlen(ConsoleInputBuffer), &NumBytes);

	printf("TTY Thread started up!\n");
	// wait for input (leaving one space for terminator)
	while(sys_tty_read(TTYChannel, ConsoleInputBuffer, ARRAY_COUNT(ConsoleInputBuffer) - 1, &NumBytes) == CELL_OK)
	{
		// terminate it, stripping off any trailing CRLF's 
		while ( NumBytes > 0 && (ConsoleInputBuffer[NumBytes - 1] == '\r' || ConsoleInputBuffer[NumBytes - 1] == '\n' ) )
		{
			ConsoleInputBuffer[NumBytes - 1] = 0;
			NumBytes--;
		}

#if WITH_UE3_NETWORKING
		if (strcasecmp(ConsoleInputBuffer, "PS3GETGAMEIP") == 0)
		{
			extern TCHAR GHostIP[64];
			unsigned int NumBytes2;
			FString Out = TEXT("GAMEIP");
			Out += GHostIP;
			sys_tty_write(SYS_TTYP15, TCHAR_TO_ANSI(*Out), Out.Len(), &NumBytes2);			
		}
		else 
#endif	//#if WITH_UE3_NETWORKING
		if (strcasecmp(ConsoleInputBuffer, "CLOSETEXTUREFILECACHE") == 0)
		{
			GStreamingManager->DisableResourceStreaming();

			// close the TFC file
			FIOSystem* AsyncIO = GIOManager->GetIOSystem( IOSYSTEM_GenericAsync );
			AsyncIO->HintDoneWithFile(appGameDir() + TEXT("CookedPS3\\Textures.tfc"));
			AsyncIO->HintDoneWithFile(appGameDir() + TEXT("CookedPS3\\Lighting.tfc"));
			AsyncIO->HintDoneWithFile(appGameDir() + TEXT("CookedPS3\\CharTextures.tfc"));

			printf("CLOSED\n");

			unsigned int NumBytes2;
			FString Out = TEXT("CLOSED");
			sys_tty_write(SYS_TTYP15, TCHAR_TO_ANSI(*Out), Out.Len(), &NumBytes2);			
		}
		else if ( NumBytes > 0 )
		{
			// add the command to the list
			FScopeLock Lock(&FPS3Controller::TTYCriticalSection);
			FPS3Controller::TTYCommands.AddItem(FString(ConsoleInputBuffer));
		}
	}
}
#endif

/**
 * Perform global initialization at program start for controller and keyboard libraries
 */
void FPS3Controller::StaticInit()
{
	// initialize controller and keyboard libraries
	cellPadInit(PS3_MAX_SUPPORTED_PLAYERS);
	cellKbInit(PS3_MAX_SUPPORTED_PLAYERS);
	cellMouseInit(PS3_MAX_SUPPORTED_PLAYERS);

#if USE_PS3_MOVE
	// hook in to the main SPURS instance
	const BYTE SPUPriorities[8] = { SPU_PRIO_MOVE, SPU_PRIO_MOVE, SPU_PRIO_MOVE, SPU_PRIO_MOVE, SPU_PRIO_MOVE, SPU_PRIO_MOVE, SPU_PRIO_MOVE, SPU_PRIO_MOVE};
	MoveKitInit(GSPURS, SPUPriorities);

	// default priority, developer can adjust as they need
	// @todo ps3 move: Put this and other thread priorities in a header
	MoveKitInitUpdateThread(999);
#endif

	bUseFilter = TRUE;
	bCalibrateThisFrame = FALSE;

    for (INT KeyboardIndex = 0; KeyboardIndex < PS3_MAX_SUPPORTED_PLAYERS; KeyboardIndex++)
	{
		// set the keyboard to generate raw keycodes, not ASCII characters
        if (cellKbSetCodeType(KeyboardIndex, CELL_KB_CODETYPE_ASCII) != CELL_KB_OK) 
		{
            debugf(TEXT("Cannot set code type for keyboard %d"), KeyboardIndex);
            cellKbEnd();
			break;
        }

		// set the keyboard to packet mode, which disables repeat
		if (cellKbSetReadMode(KeyboardIndex, CELL_KB_RMODE_PACKET) != CELL_KB_OK)
		{
            debugf(TEXT("Cannot set read mode for keyboard %d"), KeyboardIndex);
            cellKbEnd();
			break;
		}
	}

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	// start a single thread to manage the blocking TTY functions
	static sys_ppu_thread_t TTYThread = 0xFFFFFFFF;
	if (TTYThread == 0xFFFFFFFF)
	{
		sys_ppu_thread_create(
			&TTYThread,							// return value
			_TTYThreadProc,						// entry point
			SYS_TTYP4,							// parameter
			3000,								// priority as a number [0..4095]
			16 * 1024,							// stack size for thread
			SYS_PPU_THREAD_CREATE_JOINABLE,		// flag as joinable, non-interrupt thread // @todo - make a way to specify the flags, in the Runnable maybe?
			"TTY Thread");						// a standard thread name
	}
#endif
}

/**
 * Perform one time initialization of the controller/keyboard.
 * The controller and keyboard DO NOT need to be inserted at this point. This is just wiring up
 * to the rest of Unreal.
 *
 * @param	InClient			The PS3 client
 * @param	InViewport			The child viewport this controller is servicing
 * @param	InViewportClient	The viewport client controller by this controller
 * @param	InControllerIndex	The index of the controller/viewport
 */
FPS3Controller::FPS3Controller(UPS3Client* InClient, FViewport* InViewport, FViewportClient* InViewportClient, DWORD InControllerIndex)
{
	// remember our bindings to the outside world
	ViewportClient = InViewportClient;
	Client = InClient;
	Viewport = InViewport;
	ControllerIndex = InControllerIndex;

	bIsControllerInserted = FALSE;
	bIsKeyboardInserted = FALSE;
	bIsMouseInserted = FALSE;
	bIsGemInserted = FALSE;

	bCtrlPressed = bAltPressed = bShiftPressed = FALSE;
	CursorPosition.X = CursorPosition.Y = 0.f;

	RepeatKey = 0;

	LastMouseButtons = 0;
	SavedAnalog[0] = SavedAnalog[1] = SavedAnalog[2] = SavedAnalog[3] = 0.0f;

	bHasTiltSensor = FALSE;
	bIsFiltered = FALSE;
	bUseTilt = FALSE;
	bPlayerDesiresToUseTilt = FALSE;
	bOnlyUseTiltInput = FALSE;
	bUseTiltForwardAndBack = TRUE;

	bNeedsMoveYawSet = TRUE;

	SensorCenterOffsetX = 0;
	SensorCenterOffsetY = 0;
	SensorCenterOffsetZ = 0;
	SensorCenterOffsetGyro = 0;

	JumpThreshold = 0.2f;
	TurnDeadZoneX = 0.025f;  //0.005f;   tilt right left
	TurnDeadZoneY = 0.025f;  //0.005f;
	TurnDeadZoneZForward = -0.06f;  //0.005f;   tilt forward 
	TurnDeadZoneZBackward = 0.025f;  //0.005f;   tilt backward
	TurnDeadZoneGyro = 0.05f;  //0.005f;

	TurnSpeedX_Scalar = 1.5f; // tilt right left
	TurnSpeedY_Scalar = 1.0f;
	TurnSpeedZ_Scalar = 1.0f; //  tilt forward backward
	TurnSpeedGyro_Scalar = 1.0f;

	GemJerkMotionTimer = 0.0f;

	TurnClamp = 0.2f;
	OneOverTurnClamp = 1.0f/TurnClamp;

	appMemzero(ButtonsPressed, sizeof(ButtonsPressed));
}

/**
 * Handle controller input every tick
 *
 * @param DeltaTime The amount of elapsed time since the last update
 */
void FPS3Controller::Tick(FLOAT DeltaTime)
{
	if (GIsRequestingExit)
	{
		return;
	}

	// handle delayed initialization (if the controller is created before UEngine::Init is called, none of the 
	// key FNames (KEY_XboxTypeS_Start. etc) will have been initialized, and we can't attempt to use them yet)
	if (KeyMapVirtualToName.Num() == 0 && KEY_XboxTypeS_Start != NAME_None)
	{
		// generate the key/controller mapping
		#include "PS3KeyMap.h"

		// do the inverse mapping
		for (TMap<WORD, FName>::TIterator It(KeyMapVirtualToName); It; ++It)
		{
			KeyMapNameToVirtual.Set(It.Value(), It.Key());
		}
		for (TMap<BYTE, FName>::TIterator It(ButtonMapVirtualToName); It; ++It)
		{
			ButtonMapNameToVirtual.Set(It.Value(), It.Key());
		}
	}

	if (ViewportClient)
	{
		// handle any insertions or removals
		ProcessInputDevices(DeltaTime);

		// read any new input from the TTY window
		ProcessConsoleInput();
	}
}

/**
 * Handle controller insertion (initialization), removal and buttons of input devices
 * (controller, keyboard, mouse)
 *
 * @TODO we need to send an event back to the game land saying which device was modified and how
 *
 * @param DeltaTime The amount of elapsed time since the last update
 */
void FPS3Controller::ProcessInputDevices(FLOAT DeltaTime)
{
	// if these are false, then we got an error querying the devices - assume unplugged
	UBOOL bShouldProcessControllers = FALSE;
	UBOOL bShouldProcessKeyboards = FALSE;
	UBOOL bShouldProcessMice = FALSE;
	UBOOL bShouldProcessGem = FALSE;

	// remember if the devices were removed (to force release of all buttons)
	UBOOL bWasControllerRemoved = FALSE;
	UBOOL bWasKeyboardRemoved = FALSE;
	UBOOL bWasMouseRemoved = FALSE;
	UBOOL bWasGemRemoved = FALSE;

	CellPadInfo2 PadInfo;		// controller connection buffer
	if (cellPadGetInfo2(&PadInfo) == CELL_PAD_OK)
	{
		bShouldProcessControllers = TRUE;
	}

	CellKbInfo KeyboardInfo;	// keyboard connection buffer 
	if (cellKbGetInfo(&KeyboardInfo) == CELL_KB_OK)
	{
		bShouldProcessKeyboards = TRUE;
	}

	CellMouseInfo MouseInfo;	// mouse connection buffer
	if (cellMouseGetInfo(&MouseInfo) == CELL_MOUSE_OK)
	{
		bShouldProcessMice = TRUE;
	}

#if USE_PS3_MOVE
	CellGemInfo GemInfo;		// gem connection buffer
	if ( cellGemGetInfo(&GemInfo) == CELL_OK )
	{
		bShouldProcessGem = TRUE;
	}
#endif

	// process insertions
	if (bShouldProcessControllers && !bIsControllerInserted)
	{
		// if the status is not 0, then we have a controller plugged in
		if ( (PadInfo.port_status[ControllerIndex] & CELL_PAD_STATUS_CONNECTED) )
		{
			debugf(TEXT("Pad %d inserted"), ControllerIndex);
			bIsControllerInserted = TRUE;

			// clear the last frame buffer
			appMemzero(ButtonsPressed, sizeof(ButtonsPressed));

#if ENABLE_PS3_RUMBLE

			// note that the VSH turns this on and off...we don't have to care either way in code
			if ((PadInfo.device_capability[ControllerIndex] & CELL_PAD_CAPABILITY_ACTUATOR))
			{
				debugf(TEXT("Pad %d has rumble!"), ControllerIndex);
			}			

#endif // ENABLE_PS3_RUMBLE

			// check for tilt sensor and init the filters if found
			if ((PadInfo.device_capability[ControllerIndex] & CELL_PAD_CAPABILITY_SENSOR_MODE))
			{
				debugf(TEXT("Pad %d has tilt sensor..."), ControllerIndex);
				if (cellPadSetPortSetting(ControllerIndex, CELL_PAD_SETTING_SENSOR_ON) == CELL_PAD_OK)
				{
					debugf(TEXT("...enabled!"));
					bHasTiltSensor = TRUE;
					INT numAxisFiltered = 0;
					for (INT i=0; i<MAX_FILTER_AXES; i++)
					{
						appMemset(&sPadFilterData[i], 0, sizeof(CellPadFilterIIRSos));
						if (cellPadFilterIIRInit(&sPadFilterData[i], CELL_PADFILTER_IIR_CUTOFF_2ND_LPF_BT_050) == CELL_PADFILTER_OK)
						{
							numAxisFiltered++;
						}
					}
					if (numAxisFiltered == MAX_FILTER_AXES)
					{
						debugf(TEXT("...low pass pad filtering enabled!"));
						bIsFiltered = TRUE;
					}
				}
				else
				{
					debugf(TEXT("...but it failed to activate!"));
					bIsFiltered = FALSE;
					bHasTiltSensor = FALSE;
				}
			}
			else
			{
				debugf(TEXT("Pad %d does not have the tilt sensor. You lose."), ControllerIndex);
				bIsFiltered = FALSE;
				bHasTiltSensor = FALSE;
			}
		}
	}
	// process removals
	else if (bIsControllerInserted)
	{
		// was a controller removed?
		if ( !bShouldProcessControllers || !(PadInfo.port_status[ControllerIndex] && CELL_PAD_STATUS_CONNECTED) )
		{
			debugf(TEXT("Pad %d removed"), ControllerIndex);
			bIsControllerInserted = FALSE;
			bWasControllerRemoved = TRUE;
		}
	}

	// process keyboard insertions
	if (bShouldProcessKeyboards && !bIsKeyboardInserted)
	{
		// if the status is not 0, then we have a Keyboard plugged in
		if ( KeyboardInfo.status[ControllerIndex] )
		{
			debugf(TEXT("Keyboard %d inserted"), ControllerIndex);
			bIsKeyboardInserted = TRUE;
			cellKbClearBuf(ControllerIndex);
		}
	}
	// process removals
	else if (bIsKeyboardInserted)
	{
		// was a keyboard removed?
		if ( !bShouldProcessKeyboards || !KeyboardInfo.status[ControllerIndex] )
		{
			debugf(TEXT("Keyboard %d removed"), ControllerIndex);
			bIsKeyboardInserted = FALSE;
			bWasKeyboardRemoved = TRUE;
		}
	}

	// process mouse insertions
	if (bShouldProcessMice && !bIsMouseInserted)
	{
		// if the status is not 0, then we have a Mouse plugged in
		if ( MouseInfo.status[ControllerIndex] )
		{
			debugf(TEXT("Mouse %d inserted"), ControllerIndex);
			bIsMouseInserted = TRUE;
			cellMouseClearBuf(ControllerIndex);
		}
	}
	// process removals
	else if (bIsMouseInserted)
	{
		// was a mouse removed?
		if ( !bShouldProcessMice || !MouseInfo.status[ControllerIndex] )
		{
			debugf(TEXT("Mouse %d removed"), ControllerIndex);
			bIsMouseInserted = FALSE;
			bWasMouseRemoved = TRUE;
		}
	}

#if USE_PS3_MOVE
	if (bShouldProcessGem && !bIsGemInserted)
	{
		// if the status is not 0, then we have a Mouse plugged in
		if ( GemInfo.status[ControllerIndex] )
		{
			debugf(TEXT("PS Move %d inserted"), ControllerIndex);
			bIsGemInserted = TRUE;
			
			// should clear any buffered data?
		}
	}
	else if (bIsGemInserted)
	{
		// was a mouse removed?
		if ( !bShouldProcessGem || !GemInfo.status[ControllerIndex] )
		{
			debugf(TEXT("PS Move %d removed"), ControllerIndex);
			bIsGemInserted = FALSE;
			bWasGemRemoved = TRUE;
		}
	}
#endif

	// process all controller input, forcing release if sysutil has grabbed it (bit 0) or if the controller was just removed
	ProcessControllerInput(DeltaTime, bWasControllerRemoved || (PadInfo.system_info & CELL_PAD_INFO_INTERCEPTED) != 0);

	// process all keyboard input, forcing release if sysutil has grabbed it (bit 0) or if the keyboard was just removed
	ProcessKeyboardInput(DeltaTime, bWasKeyboardRemoved || (KeyboardInfo.info & CELL_PAD_INFO_INTERCEPTED) != 0);

	// process all mouse input, forcing release if sysutil has grabbed it (bit 0) or if the mouse was just removed
	ProcessMouseInput(DeltaTime, bWasMouseRemoved || (MouseInfo.info & CELL_PAD_INFO_INTERCEPTED) != 0);

#if USE_PS3_MOVE
	// process gem input (there's no intercepted bit)
	ProcessGemInput(DeltaTime, bWasGemRemoved);// || (GemInfo.info & CELL_GEM_INFO_INTERCEPTED) != 0 );
#endif
}

/** 
 * Handle button input from controller or Gem 
 * 
 * @param CurrentButtons Array of current button states
 */
void FPS3Controller::ProcessGenericButtonInput(BYTE CurrentButtons[NUM_PS3_CONTROLLER_BUTTONS])
{
	// save current time for repeat
	DOUBLE CurrentTime = appSeconds();

	DOUBLE InitialButtonRepeatDelay = Client ? Client->InitialButtonRepeatDelay : 0.2;
	DOUBLE ButtonRepeatDelay = Client ? Client->ButtonRepeatDelay : 0.1;

	for (BYTE Button = 0; Button < NUM_PS3_CONTROLLER_BUTTONS; Button++)
	{
		// only do something on the edge case (?)
		if (ButtonsPressed[Button] != CurrentButtons[Button])
		{
			// lookup the corresponding keycode
			const FName* const Key = ButtonMapVirtualToName.Find(Button);
			// if we found it, send it to the engine
			if (Key)
			{
				EInputEvent Event = CurrentButtons[Button] ? IE_Pressed : IE_Released;
				ViewportClient->InputKey(Viewport, ControllerIndex, *Key, Event, 1.f, TRUE);

				// if being pressed, reset the repeat timer
				if (Event == IE_Pressed)
				{
					ButtonRepeatTimes[Button] = CurrentTime + InitialButtonRepeatDelay;
				}
			}
			ButtonsPressed[Button] = CurrentButtons[Button];
		}
		// look to see if its time to send a key repeat event
		else if (CurrentButtons[Button] && ButtonRepeatTimes[Button] <= CurrentTime)
		{
			// lookup the corresponding keycode
			const FName* const Key = ButtonMapVirtualToName.Find(Button);
			// if we found it, send it to the engine
			if (Key)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, *Key, IE_Repeat, 1.f, TRUE);
				// reset repeat timer
				ButtonRepeatTimes[Button] = CurrentTime + ButtonRepeatDelay;
			}
		}
	}
}

/**
 * Handle controller input every tick
 *
 * @param DeltaTime The amount of elapsed time since the last update
 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
 */
void FPS3Controller::ProcessControllerInput(FLOAT DeltaTime, UBOOL bIsForcingRelease)
{
	// only do something if a controller is inserted
	if (bIsControllerInserted || bIsForcingRelease)
	{
		 //Gamapad data buffer
		CellPadData ControllerData;
		BYTE CurrentButtons[NUM_PS3_CONTROLLER_BUTTONS];
		FLOAT LeftX = 0.0f;
		FLOAT LeftY = 0.0f;
		FLOAT RightX = 0.0f;
		FLOAT RightY = 0.0f;
		UBOOL bSkipButtons = FALSE;

		// tilt sensor vars
		FLOAT SensorPosX = 0.0f;
		FLOAT SensorPosY = 0.0f;
		FLOAT SensorPosZ = 0.0f;
		FLOAT SensorGyro = 0.0f;

		// Read controller data unless we are forcing release
		if (!bIsForcingRelease)
		{
			if (cellPadGetData(ControllerIndex, &ControllerData) != CELL_PAD_OK || ControllerData.len == 0)
			{
				LeftX = SavedAnalog[0];
				LeftY = SavedAnalog[1];
				RightX = SavedAnalog[2];
				RightY = SavedAnalog[3];
				bSkipButtons = TRUE;
			}
		}
		else
		{
			appMemzero(CurrentButtons, sizeof(CurrentButtons));
		}

		if (!bSkipButtons)
		{
			if (!bIsForcingRelease)
			{
				// process each button that is pressed or not
				for (INT Button = 0; Button < 8; Button++)
				{
					CurrentButtons[Button + 0] = (ControllerData.button[2] & (1 << Button)) != 0;
					CurrentButtons[Button + 8] = (ControllerData.button[3] & (1 << Button)) != 0;
				}

				// get analog values
				LeftX  = (FLOAT)(ControllerData.button[6] - 127) * (1.0f / 127.0f);
				LeftY  = (FLOAT)(ControllerData.button[7] - 127) * (1.0f / 127.0f);
				RightX = (FLOAT)(ControllerData.button[4] - 127) * (1.0f / 127.0f);
				RightY = (FLOAT)(ControllerData.button[5] - 127) * (1.0f / 127.0f);

				// get tilt sensor data
				if (bHasTiltSensor && bUseTilt && bPlayerDesiresToUseTilt)
				{
					// calibrate if requested by the console
					if (bCalibrateThisFrame)
					{
						SensorCenterOffsetX = ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_X] - 511;
						SensorCenterOffsetY = ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_Y] - 511;
						SensorCenterOffsetZ = ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_Z] - 511;
						SensorCenterOffsetGyro = ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_G] - 511;
						bCalibrateThisFrame = FALSE;
					}

					// @TODO - recenter curve based on new calibration values
					if (bIsFiltered && bUseFilter)
					{
						SensorPosX  = (FLOAT)((INT)cellPadFilterIIRFilter(&sPadFilterData[0], ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_X]) - SensorCenterOffsetX - 511) * (1.0f / 511.0f);
						SensorPosY  = (FLOAT)((INT)cellPadFilterIIRFilter(&sPadFilterData[1], ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_Y]) - SensorCenterOffsetY - 511) * (1.0f / 511.0f);
						SensorPosZ = (FLOAT)((INT)cellPadFilterIIRFilter(&sPadFilterData[2], ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_Z]) - SensorCenterOffsetZ - 511) * (1.0f / 511.0f);
						SensorGyro = (FLOAT)((INT)cellPadFilterIIRFilter(&sPadFilterData[3], ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_G]) - SensorCenterOffsetGyro - 511) * (1.0f / 511.0f);
					}
					else
					{
						SensorPosX  = (FLOAT)(ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_X] - SensorCenterOffsetX - 511) * (1.0f / 511.0f);
						SensorPosY  = (FLOAT)(ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_Y] - SensorCenterOffsetY - 511) * (1.0f / 511.0f);
						SensorPosZ = (FLOAT)(ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_Z] - SensorCenterOffsetZ - 511) * (1.0f / 511.0f);
						SensorGyro = (FLOAT)(ControllerData.button[CELL_PAD_BTN_OFFSET_SENSOR_G] - SensorCenterOffsetGyro - 511) * (1.0f / 511.0f);
					}
				}
			}
		}

		if (!bSkipButtons)
		{
			// process the buttons against last frame's buttons
			ProcessGenericButtonInput(CurrentButtons);
		}


		// apply some deadzone (still needed?)
		if (Abs<FLOAT>(LeftX) > 0.2f)
		{
			ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_LeftX , Clamp<FLOAT>(LeftX, -1.0f, 1.0f),  DeltaTime, TRUE);
		}
		SavedAnalog[0] = LeftX;

		if (Abs<FLOAT>(LeftY) > 0.2f)
		{
			ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_LeftY , -Clamp<FLOAT>(LeftY, -1.0f, 1.0f),  DeltaTime, TRUE);
		}
		SavedAnalog[1] = LeftY;

		if( !( ( bOnlyUseTiltInput == TRUE ) && bHasTiltSensor && bUseTilt && bPlayerDesiresToUseTilt ) ) //  if we are using only tilt and we have a tilt controller!
		{
			if (Abs<FLOAT>(RightX) > 0.2f) 
			{	
				//warnf( TEXT( "RightX: %f"), RightX );
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_RightX, Clamp<FLOAT>(RightX, -1.0f, 1.0f), DeltaTime, TRUE);
			}
			SavedAnalog[2] = RightX;

			if (Abs<FLOAT>(RightY) > 0.2f) 
			{
				//warnf( TEXT( "RightY: %f"), RightY );
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_RightY, Clamp<FLOAT>(RightY, -1.0f, 1.0f), DeltaTime, TRUE);
			}
			SavedAnalog[3] = RightY;
		}

		// hack (only hoverboard is using this now
		if( bUseTiltForwardAndBack == FALSE )
		{
			if (Abs<FLOAT>(RightY) > 0.2f) 
			{
				//warnf( TEXT( "RightY: %f"), RightY );
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_RightY, Clamp<FLOAT>(RightY, -1.0f, 1.0f), DeltaTime, TRUE);
			}
			SavedAnalog[3] = RightY;
		}


 		// apply tilt sensor info
		if (bHasTiltSensor && bUseTilt && bPlayerDesiresToUseTilt)
		{
			const FLOAT RollAngle = (atan2(SensorPosX, -SensorPosY) / PI);
			const FLOAT PitchAngle = (atan2(SensorPosX, -SensorPosY) / PI);

/*
			if (SensorPosY > JumpThreshold)
			{
				#if   GAMENAME == UTGAME
					ViewportClient->InputKey(Viewport, ControllerIndex, KEY_XboxTypeS_A, IE_Pressed, 1.f, TRUE);
					ViewportClient->InputKey(Viewport, ControllerIndex, KEY_XboxTypeS_A, IE_Released, 1.f, TRUE);
				#elif GAMENAME == EXAMPLEGAME
					ViewportClient->InputKey(Viewport, ControllerIndex, KEY_XboxTypeS_B, IE_Pressed, 1.f, TRUE);
					ViewportClient->InputKey(Viewport, ControllerIndex, KEY_XboxTypeS_B, IE_Released, 1.f, TRUE);
				#else
					ViewportClient->InputKey(Viewport, ControllerIndex, KEY_XboxTypeS_B, IE_Pressed, 1.f, TRUE);
					ViewportClient->InputKey(Viewport, ControllerIndex, KEY_XboxTypeS_B, IE_Released, 1.f, TRUE);
				#endif
			}
*/

			// here is where we send the sixaxis values to the InputAxis
			// we might need to store these somewhere so we can do stuff based off them?

			if( Abs<FLOAT>(SensorPosX) > TurnDeadZoneX )
			{
				//warnf( TEXT("SensorPosX: %f"), SensorPosX );
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_SIXAXIS_AccelX, Clamp<FLOAT>(SensorPosX, -TurnClamp, TurnClamp) * OneOverTurnClamp * TurnSpeedX_Scalar, DeltaTime, TRUE);
			}

			if( Abs<FLOAT>(SensorPosY) > TurnDeadZoneY )
			{
				//warnf( TEXT("SensorPosY: %f"), SensorPosY );
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_SIXAXIS_AccelY, Clamp<FLOAT>(SensorPosY, -TurnClamp, TurnClamp) * OneOverTurnClamp * TurnSpeedY_Scalar, DeltaTime, TRUE);
			}

			if( bUseTiltForwardAndBack == TRUE )
			{
				if( ( SensorPosZ < TurnDeadZoneZForward )
					|| ( SensorPosZ > TurnDeadZoneZBackward )
					)
				{
					//warnf( TEXT("SensorPosZ: %f"), SensorPosZ );
					ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_SIXAXIS_AccelZ, Clamp<FLOAT>(-SensorPosZ, -TurnClamp, TurnClamp) * OneOverTurnClamp * TurnSpeedZ_Scalar, DeltaTime, TRUE);
				}
			}

			if( Abs<FLOAT>(SensorGyro) > TurnDeadZoneGyro )
			{
				//warnf( TEXT("SensorGyro: %f"), SensorPosZ );
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_SIXAXIS_Gyro, Clamp<FLOAT>(SensorGyro, -TurnClamp, TurnClamp) * OneOverTurnClamp * TurnSpeedGyro_Scalar, DeltaTime, TRUE);
			}
		}
	}
}


/**
 * Retrieves the name of the key mapped to the specified character code.
 *
 * @param	KeyCode	the key code to get the name for; should be the key's ANSI value
 */
FName FPS3Controller::GetVirtualKeyName( WORD KeyCode )
{
	const FName* const VirtualKeyName = KeyMapVirtualToName.Find(KeyCode);
	if ( VirtualKeyName != NULL )
	{
		return *VirtualKeyName;
	}
	return NAME_None;
}

/**
 * @return	TRUE if the specified key is pressed
 */
UBOOL FPS3Controller::IsKeyPressed( FName KeyName ) const
{
	UBOOL bIsPressed = FALSE;

	if ( IsKeyboardAvailable() )
	{
		const WORD* pKeyCode = KeyMapNameToVirtual.Find(KeyName);
		if ( pKeyCode )
		{
			const BYTE* pKeyState = KeysPressed.Find(*pKeyCode);
			if ( pKeyState )
			{
				bIsPressed = (*pKeyState) != 0;
			}
		}
		else
		{
			bIsPressed =((KeyName == KEY_LeftControl || KeyName == KEY_RightControl) && bCtrlPressed)
					||	((KeyName == KEY_LeftShift || KeyName == KEY_RightShift) && bShiftPressed)
					||	((KeyName == KEY_LeftAlt || KeyName == KEY_RightAlt) && bAltPressed);
		}
	}

	return bIsPressed;
}

/**
 * Handle keyboard input every tick
 *
 * @param DeltaTime The amount of elapsed time since the last update
 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
 */
void FPS3Controller::ProcessKeyboardInput(FLOAT DeltaTime, UBOOL bIsForcingRelease)
{
	// only do something if a controller is inserted
	if (bIsKeyboardInserted || bIsForcingRelease)
	{
		CellKbData KeyboardData;

		// read keyboard data
		// if we failed to read data or if we had no keypresses, leave now
        if (!bIsForcingRelease && cellKbRead(ControllerIndex, &KeyboardData) != CELL_KB_OK)
		{
			return;
		}
	
		DOUBLE CurrentTime = appSeconds();
		DOUBLE InitialButtonRepeatDelay = Client ? Client->InitialButtonRepeatDelay : 0.2;
		DOUBLE ButtonRepeatDelay = Client ? Client->ButtonRepeatDelay : 0.1;

		// if we are forcing release, clear all presses and don't do the key repeat
		if (bIsForcingRelease)
		{
			appMemzero( &KeyboardData, sizeof(KeyboardData) );
		}
		// if we got no changes, then repeat the last key if time has passed 
		else if (KeyboardData.len == 0)
		{
			if (RepeatKey != 0 && CurrentTime > KeyRepeatTime)
			{
				// send the actual repeated presses
				const FName* const KeyName = KeyMapVirtualToName.Find(RepeatKey);
				if (KeyName)
				{
					ViewportClient->InputKey(Viewport, ControllerIndex, *KeyName, IE_Repeat);
				}

				if ((RepeatKey & CELL_KB_RAWDAT) == 0)
				{
					// skip the InputChar call if both ctrl and alt are pressed
					if ( !(bCtrlPressed && bAltPressed) )
					{
						// send the actual char press
						TCHAR CharCode = (RepeatKey & ~CELL_KB_KEYPAD);
						if ( bCtrlPressed )
						{
							// convert this character into its corresponding ANSI control character
							CharCode = appToUpper(CharCode) - 0x40;
						}

						ViewportClient->InputChar(Viewport, ControllerIndex, CharCode);
					}
				}

				// wait for next repeat
				KeyRepeatTime = CurrentTime + ButtonRepeatDelay;
			}

			// nothing else to do
			return;
		}

		// set all currently pressed modifier keys
		bCtrlPressed = (KeyboardData.mkey&CELL_KB_MKEY_L_CTRL) != 0 || (KeyboardData.mkey&CELL_KB_MKEY_R_CTRL) != 0;
		bShiftPressed = (KeyboardData.mkey&CELL_KB_MKEY_L_SHIFT) != 0 || (KeyboardData.mkey&CELL_KB_MKEY_R_SHIFT) != 0;
		bAltPressed = (KeyboardData.mkey&CELL_KB_MKEY_L_ALT) != 0 || (KeyboardData.mkey&CELL_KB_MKEY_R_ALT) != 0;

		// find which keys are pressed, setting all to zero to start
		TMap<WORD, BYTE> NewKeysPressed;
		
		// set all keys that are pressed to 1
		for (INT KeyPressed = 0; KeyPressed < KeyboardData.len; KeyPressed++)
		{
			const WORD Key = KeyboardData.keycode[KeyPressed];

			// record that we pressed this key
			if ( Key != 0 )
			{
				NewKeysPressed.Set(Key, 1);
			}
		}

		// find ones that we pressed but weren't pressed before
		for (TMap<WORD, BYTE>::TIterator It(NewKeysPressed); It; ++It)
		{
			const WORD Key = It.Key();

			const BYTE* const OldKey = KeysPressed.Find(Key);
			if (!OldKey || *OldKey == 0)
			{
				const FName* const KeyName = KeyMapVirtualToName.Find(Key);
				if (KeyName)
				{
					ViewportClient->InputKey(Viewport, ControllerIndex, *KeyName, IE_Pressed);
				}

				if ((Key & CELL_KB_RAWDAT) == 0)
				{
					// skip the InputChar call if both ctrl and alt are pressed
					if ( !(bCtrlPressed && bAltPressed) )
					{
						// send the actual char press
						TCHAR CharCode = (Key & ~CELL_KB_KEYPAD);
						if ( bCtrlPressed )
						{
							// convert this character into its corresponding ANSI control character
							CharCode = appToUpper(CharCode) - 0x40;
						}

						ViewportClient->InputChar(Viewport, ControllerIndex, CharCode);
					}
				}

				// set up repeat
				RepeatKey = Key;
				KeyRepeatTime = CurrentTime + InitialButtonRepeatDelay;
			}

			// remember that we pressed this key
			KeysPressed.Set(Key, 1);
		}

		// find ones that we had pressed but weren't pressed this time
		for (TMap<WORD, BYTE>::TIterator It(KeysPressed); It; ++It)
		{
			const WORD Key = It.Key();

			// we only care if we had pressed it last frame (we'll get the ones we pressed this frame, but that's okay, i think)
			if (It.Value() == 1)
			{
				// was it pressed this frame?
				const BYTE* const NewKey = NewKeysPressed.Find(Key);

				// look to see if we didn't press it this frame
				if (!NewKey)
				{
					const FName* const KeyName = KeyMapVirtualToName.Find(Key);
					if (KeyName)
					{
						ViewportClient->InputKey(Viewport, ControllerIndex, *KeyName, IE_Released);
					}

					// remember that we didn't pressed this key
					KeysPressed.Set(Key, 0);

					// stop repeating if we released the repeat key
					if (RepeatKey == Key)
					{
						RepeatKey = 0;
					}
				}
			}
		}
	}
}

/**
 * Handle mouse input every tick
 *
 * @param DeltaTime The amount of elapsed time since the last update
 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
 */
void FPS3Controller::ProcessMouseInput(FLOAT DeltaTime, UBOOL bIsForcingRelease)
{
	// only do something if a mouse is inserted
	if ( bIsMouseInserted || bIsForcingRelease )
	{
		CellMouseDataList MouseDataList;

		if (!bIsForcingRelease)
		{
			// read mouse data
			// if we failed to read data or if we had no movement or buttons, leave now
			if ((cellMouseGetDataList(ControllerIndex, &MouseDataList) != CELL_MOUSE_OK))// || (MouseDataList.list_num == 0))
			{
				return;
			}
		}
		else
		{
			// if we are forcing release, then empty out any buttons/axes
			appMemset(MouseDataList.list, 0, sizeof(MouseDataList.list));
			MouseDataList.list_num = 1;
		}

		// go through the list of mouse data in case we are running at a low framerate
		for (INT iMouseData = 0; iMouseData < MouseDataList.list_num; iMouseData++)
		{
			CursorPosition.X = Clamp<FLOAT>(CursorPosition.X + (FLOAT)MouseDataList.list[iMouseData].x_axis, 0.f, Viewport->GetSizeX());
			CursorPosition.Y = Clamp<FLOAT>(CursorPosition.Y + (FLOAT)MouseDataList.list[iMouseData].y_axis, 0.f, Viewport->GetSizeY());

			// handle mouse movement
			ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_MouseX, (FLOAT)MouseDataList.list[iMouseData].x_axis, DeltaTime);
			ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_MouseY, (FLOAT)-MouseDataList.list[iMouseData].y_axis, DeltaTime);

			// handle mouse wheel
			if (MouseDataList.list[iMouseData].wheel > 0)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_MouseScrollUp, IE_Pressed);
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_MouseScrollUp, IE_Released);
			}
			if (MouseDataList.list[iMouseData].wheel < 0)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_MouseScrollDown, IE_Pressed);
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_MouseScrollDown, IE_Released);
			}

			// handle mouse buttons
			if ((MouseDataList.list[iMouseData].buttons & ~LastMouseButtons) & PS3_MOUSE_LEFTBUTTON)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_LeftMouseButton, IE_Pressed);
			}
			else if ((~MouseDataList.list[iMouseData].buttons & LastMouseButtons) & PS3_MOUSE_LEFTBUTTON)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_LeftMouseButton, IE_Released);
			}
			if ((MouseDataList.list[iMouseData].buttons & ~LastMouseButtons) & PS3_MOUSE_RIGHTBUTTON)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_RightMouseButton, IE_Pressed);
			}
			else if ((~MouseDataList.list[iMouseData].buttons & LastMouseButtons) & PS3_MOUSE_RIGHTBUTTON)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_RightMouseButton, IE_Released);
			}
			if ((MouseDataList.list[iMouseData].buttons & ~LastMouseButtons) & PS3_MOUSE_MIDDLEBUTTON)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_MiddleMouseButton, IE_Pressed);
			}
			else if ((~MouseDataList.list[iMouseData].buttons & LastMouseButtons) & PS3_MOUSE_MIDDLEBUTTON)
			{
				ViewportClient->InputKey(Viewport, ControllerIndex, KEY_MiddleMouseButton, IE_Released);
			}

			// save the button data for the next frame
			LastMouseButtons = MouseDataList.list[iMouseData].buttons;
		}
	}
}

#if USE_PS3_MOVE

/**
 * Handle Gem input every tick
 *
 * @param DeltaTime The amount of elapsed time since the last update
 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
 */
void FPS3Controller::ProcessGemInput(FLOAT DeltaTime, UBOOL bIsForcingRelease)
{
	if (bIsGemInserted || bIsForcingRelease)
	{
		// Check the cameras state
		MoveKitCameraState_t CameraState = MoveKitGetCameraState();
		MoveKitCameraStateChangePrinter(CameraState);
		MoveKitUpdateVrStates();

		INT GemState = MoveKitGetGemStateCode(ControllerIndex);

		MoveKitGemStateChangePrinter(ControllerIndex, GemState, MoveKitGetGemStatusFlags(ControllerIndex));


		// Configure Gem to track colored sphere
		if (GemState == CELL_GEM_HUE_NOT_SET)
		{
			if (MoveKitTrackAllSpheres() != CELL_OK)
			{
				debugf(TEXT("Can't find tracking hue for gem %d"), ControllerIndex);
			}
		}
		// calibrate the gem
		else if (GemState == CELL_GEM_SPHERE_NOT_CALIBRATED)
		{
			if (MoveKitGetVrDownPadButtons(ControllerIndex) & CELL_GEM_CTRL_MOVE)
			{
				cellGemCalibrate(ControllerIndex);
				bNeedsMoveYawSet = TRUE;
				debugf(TEXT("Gem[%d] calibrating."), ControllerIndex);
			}
		}

		// If Gem is track able, we can poll for data
		if (GemState == CELL_OK)
		{
			INT PressedButtons = 0;

			// Get Gem Data
			CellGemState GemStateData = MoveKitGetVrState(ControllerIndex);

			// check to see if we should set the yaw after the rest of the 
			if (bNeedsMoveYawSet)
			{
				// use the magnetometer and set the yaw to the current rotation
				cellGemEnableMagnetometer(ControllerIndex, 1);
				cellGemSetYaw(ControllerIndex, GemStateData.pos);

				bNeedsMoveYawSet = FALSE;
			}

			// union to get floats out of the Gem vectors
			typedef union { vec_float4 Vec; FLOAT Float[4]; } GemRetrieval;

			// get the buttons that are currently pressed (debounced, this is just used when not calibrated)
			UBOOL bIsJerkingUp = FALSE;
			UBOOL bIsJerkingDown = FALSE;

			if (!bIsForcingRelease)
			{
				PressedButtons = GemStateData.pad.digitalbuttons;

				// can we look for the jerk motion?
				if (GemJerkMotionTimer <= 0.0f)
				{
					// if so, get the velocity out of the gem info
					GemRetrieval Velocity;
					Velocity.Vec = GemStateData.vel;

					// is it a big enough motion?
					if (Abs<FLOAT>(Velocity.Float[1]) > 1000.0f)
					{
						bIsJerkingUp = Velocity.Float[1] > 1000.0f;
						bIsJerkingDown = Velocity.Float[1] < -1000.0f;

						// start a countdown before we look again - it's easy to get velocities like 1200, 800, 1100, which would 
						// look like 2 "button presses", not ideal, so we force a cool down
						GemJerkMotionTimer = 0.5f;
					}
				}
				else
				{
					GemJerkMotionTimer -= DeltaTime;
				}
			}

			BYTE CurrentButtons[NUM_PS3_CONTROLLER_BUTTONS];

			// map Gem buttons to controller buttons
			CurrentButtons[0] = (PressedButtons & CELL_GEM_CTRL_SELECT) != 0;	// Select
			CurrentButtons[1] = 0;												// L3
			CurrentButtons[2] = 0;												// R3
			CurrentButtons[3] = (PressedButtons & CELL_GEM_CTRL_START) != 0;	// Start
			CurrentButtons[4] = 0;												// D-Up
			CurrentButtons[5] = 0;												// D-Right
			CurrentButtons[6] = 0;												// D-Down
			CurrentButtons[7] = 0;												// D-Left
			CurrentButtons[8] = bIsJerkingUp;									// L2
			CurrentButtons[9] = bIsJerkingDown;									// R2
			CurrentButtons[10] = 0;												// L1
			CurrentButtons[11] = (PressedButtons & CELL_GEM_CTRL_T) != 0;		// R1
			CurrentButtons[12] = (PressedButtons & CELL_GEM_CTRL_TRIANGLE) != 0;// Triangle
			CurrentButtons[13] = (PressedButtons & CELL_GEM_CTRL_CIRCLE) != 0;	// Circle
			CurrentButtons[14] = (PressedButtons & CELL_GEM_CTRL_CROSS) != 0;	// X
			CurrentButtons[15] = (PressedButtons & CELL_GEM_CTRL_SQUARE) != 0;	// Square

			// If select & start button are both pressed, reset calibration
			// this is for testing only! There should be a menu option!
			// @todo ps3 move: Remove this if we have a menu option to reset, etc
			if ((PressedButtons & (CELL_GEM_CTRL_SELECT | CELL_GEM_CTRL_START)) == (CELL_GEM_CTRL_SELECT | CELL_GEM_CTRL_START))
			{
				cellGemInvalidateCalibration(ControllerIndex);
				cellGemReset(ControllerIndex);
				debugf(TEXT("Gem[%d] reset to uncalibrated"), ControllerIndex);
			}
			else
			{
				ProcessGenericButtonInput(CurrentButtons);
			}

			// now process some rotation

			// pull the world space rotation from the device
			GemRetrieval HandleRotation;
			HandleRotation.Vec = GemStateData.quat;
			FQuat QuatRotation(HandleRotation.Float[0], HandleRotation.Float[1], HandleRotation.Float[2], HandleRotation.Float[3]);
			FRotator Rotation(QuatRotation);

			// dump some controller data
			if (PressedButtons & (CELL_GEM_CTRL_TRIANGLE))
			{
				// get the rotation
				GemRetrieval AngularVelocity;
				AngularVelocity.Vec = GemStateData.angvel;

				// pull some data out of the structure
				debugf(TEXT("Rotation [%d, %d, %d]"), Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
				debugf(TEXT("Ang Vel [%.2f, %.2f, %.2f]"), AngularVelocity.Float[0], AngularVelocity.Float[1], AngularVelocity.Float[2]);
				debugf(TEXT("Analog trigger %d, %f"), GemStateData.pad.analog_T, (FLOAT)GemStateData.pad.analog_T / 255.0f);
			}

			// allow for walking forward with trigger for now
			if (GemStateData.pad.analog_T > 0)
			{
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_LeftY, (FLOAT)GemStateData.pad.analog_T / 255.0f,  DeltaTime, TRUE);
			}

			// pass rotation to engine
			if (Abs<INT>(Rotation.Roll) > 500)
			{
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_RightY , Clamp<FLOAT>((FLOAT)Rotation.Roll / 4000.0f, -1.0f, 1.0f),  DeltaTime, TRUE);
			}

			if (Abs<INT>(Rotation.Pitch) > 500)
			{
				ViewportClient->InputAxis(Viewport, ControllerIndex, KEY_XboxTypeS_RightX , Clamp<FLOAT>((FLOAT)Rotation.Pitch / 4000.0f, -1.0f, 1.0f),  DeltaTime, TRUE);
			}

		}
	}
}
#endif

/**
 * Process TTY input from the ProDG target manager, or possibly another LPID/LCID type window
 */
void FPS3Controller::ProcessConsoleInput()
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	if (!IsInGameThread())
	{
		return;
	}

	if (!GEngine || ControllerIndex >= GEngine->GamePlayers.Num())
	{
		return;
	}

	// Executing console commands can cause this function to be called again. Prevent re-entry.
	static UBOOL bProcessingDeferredCommands = FALSE;
	if ( !bProcessingDeferredCommands )
	{
		bProcessingDeferredCommands = TRUE;
		// protect the array
		FScopeLock Lock(&TTYCriticalSection);

		// process all the commands in the array
		for (INT CommandIndex = 0; CommandIndex < TTYCommands.Num(); CommandIndex++)
		{
			GEngine->GamePlayers(ControllerIndex)->Exec(*TTYCommands(CommandIndex), *GLog);
		}

		// empty the list of commands
		TTYCommands.Empty();
		bProcessingDeferredCommands = FALSE;
	}
#endif
}


/**
 * @return whether or not this Controller has Tilt Turned on
 **/
UBOOL FPS3Controller::IsControllerTiltActive() const
{
	//warnf( TEXT("FPS3Controller::IsControllerTiltActive CALLED!!! %i" ), bUseTilt );
	return bUseTilt;
}

/**
 * sets whether or not the Tilt functionality is turned on
 **/
void FPS3Controller::SetControllerTiltActive( UBOOL bActive )
{
	//warnf( TEXT("FPS3Controller::SetControllerTiltActive CALLED!!! %i" ), bActive );
	bUseTilt = bActive;
}

/**
 * sets whether or not the the player wants to utilize the Tilt functionality
 **/
void FPS3Controller::SetControllerTiltDesiredIfAvailable( UBOOL bActive )
{
	//warnf( TEXT("FPS3Controller::SetControllerTiltDesiredIfAvailable CALLED!!! %i" ), bActive );
	bPlayerDesiresToUseTilt = bActive;
}


/**
 * sets whether or not to ONLY use the tilt input controls
 **/
void FPS3Controller::SetOnlyUseControllerTiltInput( UBOOL bActive )
{
	//warnf( TEXT("FPS3Controller::SetOnlyUseControllerTiltInput CALLED!!! %i" ), bActive );
	bOnlyUseTiltInput = bActive;
}


/**
 * sets whether or not to use the tilt forward and back input controls
 **/
void FPS3Controller::SetUseTiltForwardAndBack( UBOOL bActive )
{
	//warnf( TEXT("FPS3Controller::SetUseTiltForwardAndBack CALLED!!! %i" ), bActive );
	bUseTiltForwardAndBack = bActive;
}

/**
 * @return whether or not this Controller has a keyboard available to be used
 **/
UBOOL FPS3Controller::IsKeyboardAvailable() const
{
	//warnf( TEXT("FPS3Controller::IsKeyboardAvailable CALLED!!! %i" ), KeyboardInserted );
	return bIsKeyboardInserted;
}

/**
 * @return whether or not this Controller has a mouse available to be used 
 **/
UBOOL FPS3Controller::IsMouseAvailable() const
{
	//warnf( TEXT("FPS3Controller::IsMouseAvailable CALLED!!! %i" ), MouseInserted );
	return bIsMouseInserted;
}

/**
 * Applies the current waveform data to the gamepad/mouse/etc
 *
 * @param DeviceID The device that needs updating
 * @param DeltaTime The amount of elapsed time since the last update
 */
void UPS3ForceFeedbackManager::ApplyForceFeedback(INT DeviceID, FLOAT DeltaTime)
{
#if ENABLE_PS3_RUMBLE
	extern UBOOL GEnableForceFeedback;
	if ( GEnableForceFeedback )
	{
		CellPadActParam Feedback;
		appMemzero(&Feedback, sizeof(Feedback));
		// Update the rumble data
		GetRumbleState(DeltaTime, Feedback.motor[PS3_SMALL_MOTOR], Feedback.motor[PS3_LARGE_MOTOR]);
		// Update the motor with the data
		cellPadSetActDirect(DeviceID, &Feedback);
	}
#endif // ENABLE_PS3_RUMBLE
}

/**
 * Determines the amount of rumble to apply to the left and right motors of the
 * specified gamepad
 *
 * @param flDelta The amount of time that has elapsed
 * @param LeftAmount The amount to make the left motor spin
 * @param RightAmount The amount to make the right motor spin
 */
void UPS3ForceFeedbackManager::GetRumbleState(FLOAT flDelta, BYTE& LeftAmount, BYTE& RightAmount)
{
#if ENABLE_PS3_RUMBLE
	LeftAmount = RightAmount = 0;
	// Only update time/sample if there is a current waveform playing
	if (FFWaveform != NULL && bIsPaused == 0)
	{
		// Update the current tick for this
		UpdateWaveformData(flDelta);
		// Only calculate a new waveform amount if the waveform is still valid
		if (FFWaveform != NULL)
		{
			// Evaluate the left function
			LeftAmount = EvaluateFunction(
				FFWaveform->Samples(CurrentSample).LeftFunction,
				FFWaveform->Samples(CurrentSample).LeftAmplitude,flDelta,
				FFWaveform->Samples(CurrentSample).Duration);
			// And now the right
			RightAmount = EvaluateFunction(
				FFWaveform->Samples(CurrentSample).RightFunction,
				FFWaveform->Samples(CurrentSample).RightAmplitude,flDelta,
				FFWaveform->Samples(CurrentSample).Duration);
			// Scale the samples if the user requested it
			if (ScaleAllWaveformsBy != 1.f)
			{
				LeftAmount = (BYTE)((FLOAT)LeftAmount * ScaleAllWaveformsBy);
				RightAmount = (BYTE)((FLOAT)RightAmount * ScaleAllWaveformsBy);
			}
			// The DUALSHOCK3 small motor is binary only, so let's fake an analog value with PCM
			INT Period = 400;	// ms
			INT PCM = (INT)(ElapsedTime*1000.f) % Period;
			INT Threshold = (LeftAmount*Period)/255;
			BYTE NewLeftAmount = (PCM < Threshold) ? 1 : 0;
#if 0
			debugf(TEXT("***** FFW=%s, D=%0.3f, ET=%0.3f, CS=%d, L=%02x(%x), R=%02x"), *FFWaveform->GetName(), FFWaveform->Samples(CurrentSample).Duration, ElapsedTime, CurrentSample, LeftAmount, NewLeftAmount, RightAmount);
#endif
			LeftAmount = NewLeftAmount;
		}
	}
#endif // ENABLE_PS3_RUMBLE
}

/**
 * Figures out which function to call to evaluate the current rumble amount. It
 * internally handles the constant function type.
 *
 * @param eFunc Which function to apply to the rumble
 * @param byAmplitude The max value to apply for this function
 * @param flDelta The amount of time that has elapsed
 * @param flDuration The max time for this waveform
 *
 * @return The amount to rumble the gamepad by
 */
BYTE UPS3ForceFeedbackManager::EvaluateFunction(BYTE eFunc, BYTE byAmplitude, FLOAT flDelta, FLOAT flDuration)
{
	// Defualt to no rumble in case of unknown type
	FLOAT Amount = 0;
	FLOAT fAmplitude = (FLOAT)byAmplitude;
	// Determine which evaluator to use
	switch(eFunc)
	{
		case WF_Constant:
		{
			// Constant values represent percentage
			Amount = fAmplitude * (255.f / 100.f);
			break;
		}
		case WF_LinearIncreasing:
		{
			// Calculate the value based upon elapsed time and the max in
			// byAmplitude. Goes from zero to byAmplitude.
			Amount = (fAmplitude * (ElapsedTime / flDuration)) * (255.f / 100.f);
			break;
		}
		case WF_LinearDecreasing:
		{
			// Calculate the value based upon elapsed time and the max in
			// byAmplitude. Goes from byAmplitude to zero.
			Amount = (fAmplitude * (1.f - (ElapsedTime / flDuration))) *
				(255.f / 100.f);
			break;
		}
		case WF_Sin0to90:
		{
			// Uses a sin(0..pi/2) function to go from zero to byAmplitude
			Amount = (fAmplitude * (appSin(PI/2 * (ElapsedTime / flDuration)))) *
				(255.f / 100.f);
			break;
		}
		case WF_Sin90to180:
		{
			// Uses a sin(pi/2..pi) function to go from byAmplitude to 0
			Amount = (fAmplitude * (appSin(PI/2 + PI/2 *
				(ElapsedTime / flDuration)))) * (255.f / 100.f);
			break;
		}
		case WF_Sin0to180:
		{
			// Uses a sin(0..pi) function to go from 0 to byAmplitude to 0
			Amount = (fAmplitude * (appSin(PI * (ElapsedTime / flDuration)))) *
				(255.f / 100.f);
			break;
		}
		case WF_Noise:
		{
			// Calculate a random value between 0 and byAmplitude
			Amount = (fAmplitude * (255.f / 100.f)) * appFrand();
			break;
		}
	};
	return (BYTE)Amount;
}

/** Clear any vibration going on this device right away. */
void UPS3ForceFeedbackManager::ForceClearWaveformData(INT DeviceID)
{
#if ENABLE_PS3_RUMBLE
	CellPadActParam Feedback;
	appMemzero(&Feedback, sizeof(Feedback));
	// Update the motor with the data
	cellPadSetActDirect(DeviceID, &Feedback);
#endif // ENABLE_PS3_RUMBLE
}

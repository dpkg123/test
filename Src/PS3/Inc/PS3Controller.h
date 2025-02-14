/*=============================================================================
PS3Controller.h: UPS3Controller definition
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3CONTROLLER_H__
#define __PS3CONTROLLER_H__

#include <cell/keyboard.h> 
#include <cell/pad.h>
#include <cell/padfilter.h>
#include <cell/mouse.h>



/**
 * Each game should define this to the max number of players that can be supported
 * at one time. 
 */
#define PS3_MAX_SUPPORTED_PLAYERS 2




#define NUM_PS3_CONTROLLER_BUTTONS 16

// PS3 mouse buttons
#define PS3_MOUSE_LEFTBUTTON	0x01
#define PS3_MOUSE_RIGHTBUTTON	0x02
#define PS3_MOUSE_MIDDLEBUTTON	0x04

// PS3 vibration parameters
#define PS3_SMALL_MOTOR			0
#define PS3_LARGE_MOTOR			1

// Number of tilt sensor axes per controller to filter
#define MAX_FILTER_AXES	4

enum EControllerVendor
{
	CV_SmartJoy,
	CV_ThunderGeneral,
	CV_Elecom,
	CV_LogitechDualAction,
	CV_SonyUSB,
};
/**
* This class manages the controller and keyboard for the PS3
*/
class FPS3Controller
{
public:
	/**
	* Perform global initialization at program start for controller and keyboard libraries
	*/
	static void StaticInit();

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
	FPS3Controller(class UPS3Client* InClient, FViewport* InViewport, FViewportClient* InViewportClient, DWORD InControllerIndex);

	/**
	* Handle controller input, rumble, and insertion/removal events every tick
	*
	* @param DeltaTime The amount of elapsed time since the last update
	*/
	void Tick(FLOAT DeltaTime);

	/**
	 * @return whether or not this Controller has Tilt Turned on
	 **/
	UBOOL IsControllerTiltActive() const;


	/**
	 * sets whether or not the the player wants to utilize the Tilt functionality
	 **/
	void SetControllerTiltDesiredIfAvailable( UBOOL bActive );


	/**
	 * sets whether or not the Tilt functionality is turned on
	 **/
	void SetControllerTiltActive( UBOOL bActive );


	/**
	 * sets whether or not to ONLY use the tilt input controls
	 **/
	void SetOnlyUseControllerTiltInput( UBOOL bActive );

	/**
	 * sets whether or not to use the tilt forward and back input controls
	 **/
	void SetUseTiltForwardAndBack( UBOOL bActive );

	/**
	 * @return whether or not this Controller has a keyboard available to be used
	 **/
	UBOOL IsKeyboardAvailable() const;

	/**
 	 * @return whether or not this Controller has a mouse available to be used 
	 **/
	UBOOL IsMouseAvailable() const;

	/**
	 * @return	TRUE if the specified key is pressed
	 */
	UBOOL IsKeyPressed( FName KeyName ) const;

	/** Returns this controller's cursor position */
	const FVector2D& GetCursorPos() const { return CursorPosition; }

	/** Changes the cursor position */
	void SetCursorPos( INT x, INT y ) { CursorPosition.X = x, CursorPosition.Y = y; }

	/** Set the viewport client */
	void SetViewportClient(FViewportClient* InViewportClient)
	{
		ViewportClient = InViewportClient;
	}

private:

	/** 
	 * Handle button input from controller or Gem 
	 * 
	 * @param CurrentButtons Array of current button states
	 */
	void ProcessGenericButtonInput(BYTE CurrentButtons[NUM_PS3_CONTROLLER_BUTTONS]);

	/**
	 * Handle controller insertion (initialization), removal and buttons of input devices
	 * (controller, keyboard, mouse)
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 */
	void ProcessInputDevices(FLOAT DeltaTime);

	/**
	 * Handle controller input every tick
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
	 */
	void ProcessControllerInput(FLOAT DeltaTime, UBOOL bIsForcingRelease);

	/**
	 * Handle keyboard input every tick
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
	 */
	void ProcessKeyboardInput(FLOAT DeltaTime, UBOOL bIsForcingRelease);

	/**
	 * Handle mouse input every tick
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
	 */
	void ProcessMouseInput(FLOAT DeltaTime, UBOOL bIsForcingRelease);

#if USE_PS3_MOVE
	/**
	 * Handle Gem input every tick
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 * @param bIsForcingRelease If TRUE, then we must force all buttons to be released
	 */
	void ProcessGemInput(FLOAT DeltaTime, UBOOL bIsForcingRelease);
#endif

	/**
	 * Process TTY input from the ProDG target manager, or possibly another LPID/LCID type window
	 */
	void ProcessConsoleInput();


protected:
	/** Is a valid controller inserted? */
	UBOOL bIsControllerInserted;
	/** Is a valid keyboard inserted? */
	UBOOL bIsKeyboardInserted;
	/** Is a valid mouse inserted? */
	UBOOL bIsMouseInserted;
	/** Is a valid Gem inserted? */
	UBOOL bIsGemInserted;

	/** The client we send input to */
	FViewportClient* ViewportClient;
	/** The index of this controller */
	DWORD ControllerIndex;
	/** The viewport serviced by this controller */
	FViewport* Viewport;
	/** The PS3 client */
	UPS3Client* Client;

	/** The current position of the mouse cursor; clamped to 0,0 => SizeX,SizeY */
	FVector2D CursorPosition;

	/** Remember last frames button state */
	TMap<WORD, BYTE>	KeysPressed;

	/** modifier keys */
	UBOOL				bCtrlPressed, bAltPressed, bShiftPressed;

	/** Last key pressed, can be repeated */
	WORD RepeatKey;

	/** Time to repeat the last key pressed */
	DOUBLE KeyRepeatTime;

	/** Remember last frames button state */
	BYTE ButtonsPressed[NUM_PS3_CONTROLLER_BUTTONS];

	/** Button repeat timers */
	DOUBLE ButtonRepeatTimes[NUM_PS3_CONTROLLER_BUTTONS];

	/** How much time until we look for a jerk motion again*/
	FLOAT GemJerkMotionTimer;


	FLOAT SavedAnalog[4];
	BYTE LastMouseButtons;
	/** Was this controller detected to be a SmartJoy controller or something else? */
	EControllerVendor ControllerVendor;

	/** Global mappings between key/controller codes and the Unreal Input FNames */
	static TMap<WORD, FName> KeyMapVirtualToName;
	static TMap<FName, WORD> KeyMapNameToVirtual;
	static TMap<BYTE, FName> ButtonMapVirtualToName;
	static TMap<FName, BYTE> ButtonMapNameToVirtual;

	// Tilt sensor variables per controller
	UBOOL bHasTiltSensor;
	UBOOL bIsFiltered;
    /** Whether or not this Controller is using the tilt functionality **/
	UBOOL bUseTilt;

	/** 
	 * This is set to say whether or not the play actually wants to utilize the tilt controller.  (e.g. even tho certain aspects of the
	 * game may be tilt enabled we do not force the player to use this functionality.
	 **/
	UBOOL bPlayerDesiresToUseTilt;


   /** Whether or not to ONLY use the tilt input controls **/
	UBOOL bOnlyUseTiltInput;

	/** whether to use the tilt forward back functionality of the tile controller **/
	UBOOL bUseTiltForwardAndBack;

	/** Whether or not the Yaw needs to be calibrated via the magnetometer */
	UBOOL bNeedsMoveYawSet;

	// Calibration values per controller
	INT					SensorCenterOffsetX;
	INT					SensorCenterOffsetY;
	INT					SensorCenterOffsetZ;
	INT					SensorCenterOffsetGyro;

	// Filter data per controller
	CellPadFilterIIRSos	sPadFilterData[MAX_FILTER_AXES];	// one for each axis we are going to filter

	// sixaxis vars that we will want to modify depending on what we are using the sixaxis for!
	// (e.g. when in a vehicle we may or may not want super sensitivity)
	FLOAT JumpThreshold;
	FLOAT TurnDeadZoneX;
	FLOAT TurnDeadZoneY;
	FLOAT TurnDeadZoneZForward;
	FLOAT TurnDeadZoneZBackward;
	FLOAT TurnDeadZoneGyro;

	FLOAT TurnSpeedX_Scalar;
	FLOAT TurnSpeedY_Scalar;
	FLOAT TurnSpeedZ_Scalar;
	FLOAT TurnSpeedGyro_Scalar;

	FLOAT TurnClamp;
	FLOAT OneOverTurnClamp;


public:
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	/** Array of commands generated on the console tty thread */
	static TArray<FString> TTYCommands;
	/** Critical section for TTYCommands thread safety */
	static FCriticalSection TTYCriticalSection;
#endif
	/**
	* Retrieves the name of the key mapped to the specified character code.
	*
	* @param	KeyCode	the key code to get the name for; should be the key's ANSI value
	*/
	static FName GetVirtualKeyName( WORD KeyCode );
};

/**
 * Empty force feedback class (PS3 controller doesn't support rumble)
 */
class UPS3ForceFeedbackManager : public UForceFeedbackManager
{
	DECLARE_CLASS_INTRINSIC(UPS3ForceFeedbackManager, UForceFeedbackManager, CLASS_Config | CLASS_Transient, PS3Drv)

	/**
	* Sets the defaults for this class since native only classes can't inherit
	* script class defaults.
	*/
	UPS3ForceFeedbackManager(void)
	{
		bAllowsForceFeedback = 1;
		ScaleAllWaveformsBy = 1.f;
	}

	/**
	* Applies the current waveform data to the gamepad/mouse/etc
	* This function is platform specific
	*
	* @param DeltaTime The amount of elapsed time since the last update
	* @param DeviceID The device that needs updating
	*/
	virtual void ApplyForceFeedback(INT DeviceID, FLOAT DeltaTime);

	/**
	* Determines the amount of rumble to apply to the left and right motors of the
	* specified gamepad
	*
	* @param flDelta The amount of time that has elapsed
	* @param LeftAmount The amount to make the left motor spin
	* @param RightAmount The amount to make the right motor spin
	*/
	void GetRumbleState(FLOAT flDelta, BYTE& LeftAmount, BYTE& RightAmount);

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
	BYTE EvaluateFunction(BYTE eFunc, BYTE byAmplitude, FLOAT flDelta, FLOAT flDuration);

	/**
	* Immediately clears all vibration.
	*
	* @param DeviceID The device that needs updating
	*/
	void ForceClearWaveformData(INT DeviceID);
};

#endif

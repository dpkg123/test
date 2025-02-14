class GOUPlayerController extends UTPlayerController;

var PlayerController PC;
//functions for zooming, dont work well, there is no zmoothing, and zoom is to small fix me.
/*
exec function FovIn()
{
	local GOUPawn P;

	P = GOUPawn (PC.Pawn);

	P.SetFov(50);
	/*
	//OnFootDefaultFOV=1.000000;
	FOVAngle=1.000000;
	DesiredFOV=1.000000;
	DefaultFOV=1.000000;
	*/
}

exec function FovOut()
{

		local GOUPawn P;

	P = GOUPawn (PC.Pawn);

	P.SetFov(50);
	/*
	//OnFootDefaultFOV=105.000000;
	FOVAngle=105.000000;
	DesiredFOV=105.000000;
	DefaultFOV=105.000000;
*/
}

simulated event PostBeginPlay()
{
	super.PostBeginPlay();

	MaxTimeMargin = class'GameInfo'.Default.MaxTimeMargin;
	MaxResponseTime = Default.MaxResponseTime * WorldInfo.TimeDilation;
	if ( WorldInfo.NetMode == NM_Client )
	{
		SpawnDefaultHUD();
	}
	else
	{
		AddCheats();
	}

	SetViewTarget(self);  // MUST have a view target!
	LastActiveTime = WorldInfo.TimeSeconds;

	OnlineSub = class'GameEngine'.static.GetOnlineSubsystem();
}
//this is hack, to hold 3rd person view i hardcoded >true< value, without that after respawn you get back to 1st person
function SetBehindView(bool bNewBehindView)
{
	bBehindView = bNewBehindView;
	if ( false )
	{
		bFreeCamera = false;
	}

	if (LocalPlayer(Player) == None)
	{
		ClientSetBehindView(true);
		
	}
	else if (GOUPawn(ViewTarget) != None)
	{
		GOUPawn(ViewTarget).SetThirdPersonCamera(true);
	}
	// make sure we recalculate camera position for this frame
	LastCameraTimeStamp = WorldInfo.TimeSeconds - 1.0;
}
*/

DefaultProperties
{
	bBehindView=True
	//OnFootDefaultFOV=105.000000
	FOVAngle=105.000000
	DesiredFOV=105.000000
	DefaultFOV=105.000000
	//InputClass=class'GearsOfUnreal.GOUPlayerInput'
}

// this pawn class is just for setting up a basic third person view
// the default player mesh (CharClassInfo) is defined in UTPlayerReplicationInfo
// I used the UDN example: https://docs.unrealengine.com/udk/Three/CameraTechnicalGuide.html#Example Third Person Camera
// I just wanted a basic third person view to compile all the GOW Content on UDK Ultimate. I only tested some few 3rd person scripts and no // one of them worked on consoles (PS3 and Xbox360). So because the BehindView Console Command worked on all platforms, it showed the third // person view correctly, I decided to use the behindview as a third person script UTPC.SetBehindView(true);
// About the other third person cameras scripts which did not work on consoles, I need to investigate further, however, as I am still 
// learning unrealscripting in this project UDK Ultimate, I prefeer to keep things as simple as possible
// the only problem with this behindview camera is that it does not support camera animations, so as I am simulating a GOW execution
//chainsaw camera, we need to change the camera to another new camera actor and position it with kismet (attach to actor and set camera target) so this new camera will play the chainsaw execution camera animation. There is a sample map GOW Chainsaw Execution

class UDKUltimatePawn extends UTPawn;

//this variable will store the animation node chainsaw attack
//this animation is setup on the Animtree
var AnimNodePlayCustomAnim ChainsawAnim;

//this function will find the animation node and store this to another variable
//this variable will be called by the weapon code
simulated event PostInitAnimTree(SkeletalMeshComponent SkelComp)
{
	super.PostInitAnimTree(SkelComp);

	if (SkelComp == Mesh)
	{
		ChainsawAnim = AnimNodePlayCustomAnim(SkelComp.FindAnimNode('ChainsawAnim'));
	}
}

//override to make player mesh visible by default
simulated event BecomeViewTarget( PlayerController PC )
{
   local UTPlayerController UTPC;

   Super.BecomeViewTarget(PC);

   if (LocalPlayer(PC.Player) != None)
   {
      UTPC = UTPlayerController(PC);
      if (UTPC != None)
      {
         //set player controller to behind view and make mesh visible
         UTPC.SetBehindView(true);
         SetMeshVisibility(UTPC.bBehindView);
      }
   }
}

simulated function bool CalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{
   local vector CamStart, HitLocation, HitNormal, CamDirX, CamDirY, CamDirZ, CurrentCamOffset;
   local float DesiredCameraZOffset;

   CamStart = Location;
   CurrentCamOffset = CamOffset;

   DesiredCameraZOffset = (Health > 0) ? 1.2 * GetCollisionHeight() + Mesh.Translation.Z : 0.f;
   CameraZOffset = (fDeltaTime < 0.2) ? DesiredCameraZOffset * 5 * fDeltaTime + (1 - 5*fDeltaTime) * CameraZOffset : DesiredCameraZOffset;
   
   if ( Health <= 0 )
   {
      CurrentCamOffset = vect(0,0,0);
      CurrentCamOffset.X = GetCollisionRadius();
   }

   CamStart.Z += CameraZOffset;
   GetAxes(out_CamRot, CamDirX, CamDirY, CamDirZ);
   CamDirX *= CurrentCameraScale;

   if ( (Health <= 0) || bFeigningDeath )
   {
      // adjust camera position to make sure it's not clipping into world
      // @todo fixmesteve.  Note that you can still get clipping if FindSpot fails (happens rarely)
      FindSpot(GetCollisionExtent(),CamStart);
   }
   if (CurrentCameraScale < CameraScale)
   {
      CurrentCameraScale = FMin(CameraScale, CurrentCameraScale + 5 * FMax(CameraScale - CurrentCameraScale, 0.3)*fDeltaTime);
   }
   else if (CurrentCameraScale > CameraScale)
   {
      CurrentCameraScale = FMax(CameraScale, CurrentCameraScale - 5 * FMax(CameraScale - CurrentCameraScale, 0.3)*fDeltaTime);
   }

   if (CamDirX.Z > GetCollisionHeight())
   {
      CamDirX *= square(cos(out_CamRot.Pitch * 0.0000958738)); // 0.0000958738 = 2*PI/65536
   }

   out_CamLoc = CamStart - CamDirX*CurrentCamOffset.X + CurrentCamOffset.Y*CamDirY + CurrentCamOffset.Z*CamDirZ;

   if (Trace(HitLocation, HitNormal, out_CamLoc, CamStart, false, vect(12,12,12)) != None)
   {
      out_CamLoc = HitLocation;
   }

   return true;
}   

defaultproperties
{
  
Health=300
HealthMax=300

	bPhysRigidBodyOutOfWorldCheck=TRUE
	bRunPhysicsWithNoController=true

	LeftFootControlName=LeftFootControl
	RightFootControlName=RightFootControl
	bEnableFootPlacement=true
	MaxFootPlacementDistSquared=56250000.0 // 7500 squared	
}
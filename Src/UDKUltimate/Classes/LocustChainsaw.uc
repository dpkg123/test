//this pawn class is for setting up a basic NPC (enemies) system
//UTFamilyInfo_GOW_Locust
//I called it LocustChainsaw because this classes is especially designed to receive the chainsaw execution animation (like Gears of War)

class LocustChainsaw extends UTPawn

  placeable;

var SkeletalMesh defaultMesh;
var MaterialInterface defaultMaterial0;
var AnimTree defaultAnimTree;
var array<AnimSet> defaultAnimSet;
var AnimNodeSequence defaultAnimSeq;
var PhysicsAsset defaultPhysicsAsset;

//add custom taunt and voice class to pawn
var class<UTVoice> VoiceClass;

//this variable will store the camera animation name
var CameraAnim MyCameraAnim;

static function class<UTVoice> GetVoiceClass()
{
	return Default.VoiceClass;
}

//override to do nothing
simulated function SetCharacterClassFromInfo(class<UTFamilyInfo> Info)
{
  Mesh.SetSkeletalMesh(defaultMesh);
  Mesh.SetMaterial(0,defaultMaterial0);
  Mesh.SetPhysicsAsset(defaultPhysicsAsset);
  Mesh.AnimSets=defaultAnimSet;
  Mesh.SetAnimTreeTemplate(defaultAnimTree);
}

simulated event PostBeginPlay()
{
  Super.PostBeginPlay();
}

//all the animation will be played inside the TakeDamage Event
event TakeDamage(int Damage, Controller EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType, optional TraceHitInfo HitInfo, optional Actor DamageCauser)
{
	
	local int OldHealth;
	
	//this variables will be used to play the camera animation on the player controller
    local Playercontroller localplayer;

	//this statement will check if the enemy was attacked by the chainsaw weapon
	//if yes, then it will play the Chainsaw Death Animation
	if (DamageType == class'UTDmgType_GOWChainsaw')
	{

	//this is the function to play the camera animation
	//we placed this function here because it must only play the animation if the enemy is hit by the chainsaw
    ForEach LocalPlayerControllers(class'PlayerController', localPlayer)
    {
      
	  UTPlayerController(localplayer).PlayCameraAnim(MyCameraAnim,,,,,false);
	  UTPlayerController(localplayer).ClientSpawnCameraLensEffect(class'ChainsawCameraBlood');
    }
	
	   //this statement will play the chainsaw death animation on the enemy pawn
       FullBodyAnimSlot.SetActorAnimEndNotification(true);
       FullBodyAnimSlot.PlayCustomAnim('Chainsaw_Anim_Death_Final', 1.0, 0.05, 0.05, true, false);
	   
	   //this will play the mesh ripp appart sound
	   PlaySound(SoundCue'WP_GOW.Sounds.ChainsawDeath_Cue', false, true, true, vect(0,0,0), false);
	   
       //this will freeze the enemy pawn
	   Super.DetachFromController();
	   Super.StopFiring();
       GroundSpeed=0;

	   //this is a timer that will call for the function deathsequence and come back to normal
       SetTimer(2.0,false,nameof(KillLocust));

	}
	
	// Attached Bio glob instigator always gets kill credit
	if (AttachedProj != None && !AttachedProj.bDeleteMe && AttachedProj.InstigatorController != None)
	{
		EventInstigator = AttachedProj.InstigatorController;
	}

	// reduce rocket jumping
	if (EventInstigator == Controller)
	{
		momentum *= 0.6;
	}

	// accumulate damage taken in a single tick
	if ( AccumulationTime != WorldInfo.TimeSeconds )
	{
		AccumulateDamage = 0;
		AccumulationTime = WorldInfo.TimeSeconds;
	}
    OldHealth = Health;
	AccumulateDamage += Damage;
	Super.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType, HitInfo, DamageCauser);
	AccumulateDamage = AccumulateDamage + OldHealth - Health - Damage;

}

//this function will make the enemy die
simulated function KillLocust()
{
    Super.Suicide();	
}

defaultproperties 
{
  defaultMesh=SkeletalMesh'UT3_Model_Locust.UT3_Model_Locust'
  defaultAnimTree=AnimTree'CH_AnimHuman_Tree.AT_CH_Human'
  defaultAnimSet(0)=AnimSet'CH_AnimHuman.Anims.K_AnimHuman_BaseMale'
  defaultPhysicsAsset=PhysicsAsset'CH_AnimCorrupt.Mesh.SK_CH_Corrupt_Male_Physics'
  
  Begin Object Name=WPawnSkeletalMeshComponent
    AnimTreeTemplate=AnimTree'CH_AnimHuman_Tree.AT_CH_Human'
  End Object
  
  Drawscale=1.075 

    CurrCharClassInfo=class'UTGame.UTFamilyInfo_GOW_Locust'
    SoundGroupClass=class'UTGame.UTPawnSoundGroup_Locust'
    //add custom taunt and voice class to pawn
    VoiceClass=class'UTGame.UTVoice_Locust'

Health=150
HealthMax=150


	bPhysRigidBodyOutOfWorldCheck=TRUE
	bRunPhysicsWithNoController=true

	LeftFootControlName=LeftFootControl
	RightFootControlName=RightFootControl
	bEnableFootPlacement=true
	MaxFootPlacementDistSquared=56250000.0 // 7500 squared

//here we inform the camera animation name
MyCameraAnim=CameraAnim'GOW_Cameras.ChainsawCam'

}
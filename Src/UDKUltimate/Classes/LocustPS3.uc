//this pawn class is for setting up a basic NPC (enemies) system
//UTFamilyInfo_GOW_Locust
//I called it LocustPS3 because the other Locust Enemies Classes use the Hitmask Actor Component to show the effect of blood wounds over the enemy body once it is shot. However, it seems that the PS3 version of Unreal Engine May 2011 does not support the Hitmask Actor (Xbox360 works flaswlessly), so because this I created this enemy class as a base to create another enemies classes to play on PS3

class LocustPS3 extends UTPawn

  placeable;

var SkeletalMesh defaultMesh;
var MaterialInterface defaultMaterial0;
var AnimTree defaultAnimTree;
var array<AnimSet> defaultAnimSet;
var AnimNodeSequence defaultAnimSeq;
var PhysicsAsset defaultPhysicsAsset;

//add custom taunt and voice class to pawn
var class<UTVoice> VoiceClass;

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
	
}
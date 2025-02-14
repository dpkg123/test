//this pawn class is for setting up a basic NPC (enemies) system
//UTFamilyInfo_GOW_Locust

class GOW_Hofman extends UTPawn

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
  defaultMesh=SkeletalMesh'UT3_Model_Hofman.UT3_Model_Hofman'
  defaultAnimTree=AnimTree'CH_AnimHuman_Tree.AT_CH_Human'
  defaultAnimSet(0)=AnimSet'CH_AnimHuman.Anims.K_AnimHuman_BaseMale'
  defaultPhysicsAsset=PhysicsAsset'CH_AnimCorrupt.Mesh.SK_CH_Corrupt_Male_Physics'
  
  Begin Object Name=WPawnSkeletalMeshComponent
    AnimTreeTemplate=AnimTree'CH_AnimHuman_Tree.AT_CH_Human'
  End Object
  
  Drawscale=1.075 

CurrCharClassInfo=class'UTGame.UTFamilyInfo_GOW_Hofman'
SoundGroupClass=class'UTGame.UTPawnSoundGroup_Hofman'

//add custom taunt and voice class to pawn
VoiceClass=class'UTGame.UTVoice_Hofman'

Health=300
HealthMax=300


	bPhysRigidBodyOutOfWorldCheck=TRUE
	bRunPhysicsWithNoController=true

	LeftFootControlName=LeftFootControl
	RightFootControlName=RightFootControl
	bEnableFootPlacement=true
	MaxFootPlacementDistSquared=56250000.0 // 7500 squared	
  
}
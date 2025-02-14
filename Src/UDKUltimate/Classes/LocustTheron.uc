class LocustTheron extends UTPawn
	placeable;

//hitmask variables
var() SceneCapture2DHitMaskComponent HitMaskComponent;
var TextureRenderTarget2D MaskTexture;

//add custom taunt and voice class to pawn
var class<UTVoice> VoiceClass;

static function class<UTVoice> GetVoiceClass()
{
	return Default.VoiceClass;
}

//hitmask funtions//////////////////////////////////////////////////////////////////////////////////////
simulated function PostBeginPlay( )
{
	local MaterialInstanceConstant MIC;

	Super.PostBeginPlay( );

	MaskTexture = class'TextureRenderTarget2D'.static.Create( 2048, 2048, PF_G8, MakeLinearColor( 0, 0, 0, 1 ) );

	if( MaskTexture != None )
	{
		HitMaskComponent.SetCaptureTargetTexture( MaskTexture );

		MIC = Mesh.CreateAndSetMaterialInstanceConstant( 0 );
		if( MIC != None )
		{
			MIC.SetTextureParameterValue( 'FilterMask', MaskTexture );
		}
	}
}

simulated function Destroyed( )
{
	Super.Destroyed( );

	MaskTexture = None;
}

function PlayHit( float Damage, Controller InstigatedBy, vector HitLocation, class<DamageType> damageType, vector Momentum, TraceHitInfo HitInfo )
{
	Super.PlayHit( Damage, InstigatedBy, HitLocation, damageType, Momentum, HitInfo );

	HitMaskComponent.SetCaptureParameters( HitLocation, 10.0f, HitLocation, False );
}
//hitmask funtions//////////////////////////////////////////////////////////////////////////////////////

simulated function SetCharacterClassFromInfo(class<UTFamilyInfo> Info)
{
}

DefaultProperties
{
	//initialize hitmask actor
	Begin Object Class=SceneCapture2DHitMaskComponent Name=HitMaskComp
	End Object
	HitMaskComponent=HitMaskComp
	Components.Add(HitMaskComp)
    
	//initialize skeletalmesh that will be attached to hitmask actor
    Begin Object class=SkeletalMeshComponent Name=SandboxPawnSkeletalMesh
	
    SkeletalMesh=SkeletalMesh'UT3_Model_Locust_Theron.UT3_Model_Locust_Theron'
    AnimSets(0)=AnimSet'CH_AnimHuman.Anims.K_AnimHuman_BaseMale'
    AnimTreeTemplate=AnimTree'CH_AnimHuman_Tree.AT_CH_Human'
	PhysicsAsset=PhysicsAsset'CH_AnimCorrupt.Mesh.SK_CH_Corrupt_Male_Physics'

		bCacheAnimSequenceNodes=FALSE
		AlwaysLoadOnClient=true
		AlwaysLoadOnServer=true
		bOwnerNoSee=true
		CastShadow=true
		BlockRigidBody=TRUE
		bUpdateSkelWhenNotRendered=false
		bIgnoreControllersWhenNotRendered=TRUE
		bUpdateKinematicBonesFromAnimation=true
		bCastDynamicShadow=true
		Translation=(Z=8.0)
		RBChannel=RBCC_Untitled3
		RBCollideWithChannels=(Untitled3=true)
		LightEnvironment=MyLightEnvironment
		bOverrideAttachmentOwnerVisibility=true
		bAcceptsDynamicDecals=true
		bHasPhysicsAssetInstance=true
		TickGroup=TG_PreAsyncWork
		MinDistFactorForKinematicUpdate=0.2
		bChartDistanceFactor=true
		//bSkipAllUpdateWhenPhysicsAsleep=TRUE
		RBDominanceGroup=20
		Scale=1.075
		// Nice lighting for hair
		bUseOnePassLightingOnTranslucency=TRUE
		bPerBoneMotionBlur=true	
	
    End Object
    Mesh=SandboxPawnSkeletalMesh
    Components.Add(SandboxPawnSkeletalMesh)	
		
    CurrCharClassInfo=class'UTGame.UTFamilyInfo_GOW_Locust_Theron'
    SoundGroupClass=class'UTGame.UTPawnSoundGroup_Locust'
    //add custom taunt and voice class to pawn
    VoiceClass=class'UTGame.UTVoice_Locust'

	Drawscale=1.075
	
    Health=150
    HealthMax=150
	
	bPhysRigidBodyOutOfWorldCheck=TRUE
	bRunPhysicsWithNoController=true

	LeftFootControlName=LeftFootControl
	RightFootControlName=RightFootControl
	bEnableFootPlacement=true
	MaxFootPlacementDistSquared=56250000.0 // 7500 squared	
	
}
class HitMaskModularActor extends UTPawn
	placeable;

var() SkeletalMeshComponent HeadComponent;

var() SceneCapture2DHitMaskComponent HitMaskComponents[2];
var TextureRenderTarget2D MaskTextures[2];

simulated function PostBeginPlay( )
{
	local MaterialInstanceConstant MIC[2];

	Super.PostBeginPlay( );

	MaskTextures[0] = class'TextureRenderTarget2D'.static.Create( 2048, 2048, PF_G8, MakeLinearColor( 0, 0, 0, 1 ) );
	MaskTextures[1] = class'TextureRenderTarget2D'.static.Create( 2048, 2048, PF_G8, MakeLinearColor( 0, 0, 0, 1 ) );

	if( MaskTextures[0] != None && MaskTextures[1] != None )
	{
		HitMaskComponents[0].SetCaptureTargetTexture( MaskTextures[0] );
		HitMaskComponents[1].SetCaptureTargetTexture( MaskTextures[1] );

		MIC[0] = Mesh.CreateAndSetMaterialInstanceConstant( 0 );
		MIC[1] = HeadComponent.CreateAndSetMaterialInstanceConstant( 0 );

		if( MIC[0] != None && MIC[1] != None )
		{
			MIC[0].SetTextureParameterValue( 'FilterMask', MaskTextures[0] );
			MIC[1].SetTextureParameterValue( 'FilterMask', MaskTextures[1] );
		}
	}
}

simulated function Destroyed( )
{
	Super.Destroyed( );

	MaskTextures[0] = None;
	MaskTextures[1] = None;
}

function PlayHit( float Damage, Controller InstigatedBy, vector HitLocation, class<DamageType> damageType, vector Momentum, TraceHitInfo HitInfo )
{
	Super.PlayHit( Damage, InstigatedBy, HitLocation, damageType, Momentum, HitInfo );

	HitMaskComponents[0].SetCaptureParameters( HitLocation, 10.0f, HitLocation, False );

	// HACK: force the component to the right component
	HitMaskComponents[1].SkeletalMeshComp = HeadComponent;
	HitMaskComponents[1].SetCaptureParameters( HitLocation, 10.0f, HitLocation, False );
}

simulated function SetCharacterClassFromInfo(class<UTFamilyInfo> Info)
{
}

DefaultProperties
{
	Begin Object Name=WPawnSkeletalMeshComponent
		
		// No light environments!
		LightEnvironment=None

	End Object

	Begin Object Class=SkeletalMeshComponent Name=HeadComp

		// No light environments!
		LightEnvironment=None

		// Assign the parent component for Animation and Shadows
		ParentAnimComponent=WPawnSkeletalMeshComponent
		ShadowParent=WPawnSkeletalMeshComponent

	End Object
	HeadComponent=HeadComp
	Components.Add(HeadComp)

	Begin Object Class=SceneCapture2DHitMaskComponent Name=HitMaskComp
	End Object
	HitMaskComponents[0]=HitMaskComp
	Components.Add(HitMaskComp)

	Begin Object Class=SceneCapture2DHitMaskComponent Name=HitMaskComp2
	End Object
	HitMaskComponents[1]=HitMaskComp2
	Components.Add(HitMaskComp2)
}
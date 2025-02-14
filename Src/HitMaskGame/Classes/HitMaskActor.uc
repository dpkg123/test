class HitMaskActor extends UTPawn
	placeable;

var() SceneCapture2DHitMaskComponent HitMaskComponent;
var TextureRenderTarget2D MaskTexture;

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

simulated function SetCharacterClassFromInfo(class<UTFamilyInfo> Info)
{
}

DefaultProperties
{
	Begin Object Class=SceneCapture2DHitMaskComponent Name=HitMaskComp
	End Object
	HitMaskComponent=HitMaskComp
	Components.Add(HitMaskComp)
}
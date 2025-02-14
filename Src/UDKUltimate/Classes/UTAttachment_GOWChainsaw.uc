/**
* Created using Unreal Script Wizard by RyanJon2040
* Visit: www.dynamiceffects.net
* Meet me on Facebook: www.facebook.com/satheeshpv
*/

class UTAttachment_GOWChainsaw extends UTWeaponAttachment;

DefaultProperties
{	
    Begin Object Name=SkeletalMeshComponent0
        SkeletalMesh=SkeletalMesh'WP_GOW.Mesh.WP_GOWRifleAssalto_P3'
        Scale=0.9000000
    End Object	
	
	//play pistol holding animations//
	//for UDK Ultimate as I do not want to use dual pistols, I overwrite the DualPistols Animations by the chainsaw holding animations
    WeapAnimType=EWAT_DualPistols

    bMakeSplash=True

    //<DO NOT MODIFY>
    Mesh=SkeletalMeshComponent0
    WeaponClass=Class'UTWeap_GOWChainsaw'
}
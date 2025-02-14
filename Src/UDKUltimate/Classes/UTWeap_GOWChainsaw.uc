class UTWeap_GOWChainsaw extends UTWeapon;

simulated function FireAmmunition()
{
	//this variables will be used to play the camera animation on the player controller
    local Playercontroller localplayer;

	if (HasAmmo(CurrentFireMode))
	{

	//this is the function to play the camera animation
	//we placed this function here because it must only play the animation if the enemy is hit by the chainsaw
    ForEach LocalPlayerControllers(class'PlayerController', localPlayer)
    {
	  
	  //this will freeze the player movement and other cool effects like slo-mo and invencibility. 
	  //I placed here because console command only works when accessed by player controller
	  UTPlayerController(localplayer).IgnoreLookInput(true);
      UTPlayerController(localplayer).IgnoreMoveInput(true);  
	  UTPlayerController(localplayer).ConsoleCommand("God");
	  SetTimer(3.0,false,nameof(ReturnNormal));
    }	
        
		//this function will play the custom animation once the player presses the fire button, i.e FireAmmunition
		//the animation node was setup on the UDKUltimate Pawn, because the animation will be played on the player pawn actor
		//and not on the weapon
		UDKUltimatePawn(Owner).FullBodyAnimSlot.SetActorAnimEndNotification(true);
        UDKUltimatePawn(Owner).FullBodyAnimSlot.PlayCustomAnim('Chainsaw_Anim_Attack_Final', 1.30);
		PlaySound(SoundCue'A_VoicePack_GEARSofWAR_Marcus.FuckYou_02_Cue', false, true, true, vect(0,0,0), false);
		super.FireAmmunition();
	}
}

//this function will make the player comeback to movement
simulated function ReturnNormal()
{
  local Playercontroller localplayer;

	//this is the function to play the camera animation
	//we placed this function here because it must only play the animation if the enemy is hit by the chainsaw
    ForEach LocalPlayerControllers(class'PlayerController', localPlayer)
    {	  
	  //this will freeze the player movement and other cool effects like slo-mo and invencibility. 
	  //I placed here because console command only works when accessed by player controller
	  UTPlayerController(localplayer).IgnoreLookInput(false);	
      UTPlayerController(localplayer).IgnoreMoveInput(false); 	  
	  UTPlayerController(localplayer).ConsoleCommand("God");
    }
}

defaultproperties
{
    Begin Object class=AnimNodeSequence Name=MeshSequenceA
        bCauseActorAnimEnd=true
    End Object
	
    Begin Object Name=PickupMesh
        SkeletalMesh=SkeletalMesh'WP_GOW.Mesh.WP_GOWRifleAssalto_P3'
        Scale=0.9000000
    End Object

    PivotTranslation=(Y=0.0)
	
	bInstantHit=true;	
    WeaponFireTypes(0)=EWFT_InstantHit
    InstantHitDamage(0)=30	
	InstantHitMomentum[0]=0
    WeaponRange=70	
	InstanthitDamageTypes(0)=class'UTDmgType_GOWChainsaw'

	//ammo fire speed functions///////////////////////////////
	FireInterval(0)=4
	//ammo fire speed functions///////////////////////////////
	
	//ammo properties
    MaxAmmoCount=10000
    AmmoCount=10000
   
	//sounds
    WeaponEquipSnd=SoundCue'WP_GOW.Sounds.ChainsawStart01_Cue'
    WeaponPutDownSnd=SoundCue'WP_GOW.Sounds.ChainsawStop01_Cue'
    WeaponFireSnd(0)=SoundCue'WP_GOW.Sounds.ChainsawAttack_Cue'
    PickupSound=SoundCue'WP_GOW.Sounds.ChainsawStall01_Cue'

    //CrosshairImage=Copy texture from Content Browser
	//setting it all to zero will hide the crosshair icon
    CrossHairCoordinates=(U=0,V=0,UL=0,VL=0)

    //<DO NOT MODIFY>
    Mesh=FirstPersonMesh
    DroppedPickupMesh=PickupMesh
    PickupFactoryMesh=PickupMesh
    AttachmentClass=Class'UTAttachment_GOWChainsaw'
	
}
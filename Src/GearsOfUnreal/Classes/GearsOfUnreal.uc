class GearsOfUnreal extends UTMutator 
    config( GearsOfUnreal );

simulated function InitMutator(string options, out string ErrorMessage)
{
    /** INFO (neoaez) 20071124: continue the mutator chain
	 * */
    Super.InitMutator( options, ErrorMessage );
}

simulated function ModifyPlayer( Pawn Other )
{
    local UTPawn P;
    local GOUPawn MP;
	local GOUPlayerController C;

    P = UTPawn( Other );
    if( P != None )
    {

        MP = GOUPawn( P );
        if ( MP != None )
        {
			MP.ClientInitInteraction();
			//MP.ClientSetStartingCameraView();
        }
		C = GOUPlayerController(P.Controller);
        if (C != None)
        {
            C.SetCameraMode( 'ThirdPerson' );
        }
    }
    Super.ModifyPlayer( Other );
}

simulated function PostBeginPlay()
{
	local UTGame Game;
	local int i;

    Super.PostBeginPlay();

	// replace default weapons
	Game = UTGame( WorldInfo.Game );
	if ( Game != None )
	{
        Game.DefaultPawnClass = class'GearsOfUnreal.GOUPawn';
		Game.PlayerControllerClass=Class'GearsOfUnreal.GOUPlayerController';
		
		for ( i = 0; i < Game.DefaultInventory.length; i++ )
		{
            /** INFO (neoaez) 20080610: look for instances of the UT Sniper rifle and replace it with our own
			 * This incorporates our fix that allows us to switch back to first person when zooming
			 * */
			if ( Game.DefaultInventory[i].Name == 'UTWeap_SniperRifle' )
			{
					Game.DefaultInventory[i] = class<UTWeapon>( DynamicLoadObject( "GearsOfUnreal.GOUWeap_SniperRifle", class'Class' ) );
			}
            else
            {
                //DEBUG
                //LogInternal( "MKLOG -> MK3rdPersonMutator.PostBeginPlay: Cycling through Game.DefaultInventory:"@Game.DefaultInventory[i] );
                //DEBUG
            }
            /** INFO (neoaez) 20080610: look for instances of the UT Enforcer and replace it with our own
			 * This allows us to incorporate a fix that stops the first person arm meshes from appearing when we are in 3rd person and equip
			 * dual enforcers for the first time
			 * */
			if ( Game.DefaultInventory[i].Name == 'UTWeap_Enforcer' )
			{
					Game.DefaultInventory[i] = class<UTWeapon>( DynamicLoadObject( "GearsOfUnreal.GOUWeap_Enforcer", class'Class' ) );
			}
            else
            {
                //DEBUG
                //LogInternal( "MKLOG -> MK3rdPersonMutator.PostBeginPlay: Cycling through Game.DefaultInventory:"@Game.DefaultInventory[i] );
                //DEBUG
            }            
		}
	}
}

simulated function bool CheckReplacement( Actor Other )
{
	local UTWeaponPickupFactory WeaponPickup;
	local UTWeaponLocker Locker;
    local int i;

    //DEBUG
    //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: CALLED" );
    //DEBUG

    WeaponPickup = UTWeaponPickupFactory( Other );
	if ( WeaponPickup != None )
	{
        //DEBUG
        //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: WeaponPickup:"@WeaponPickup );
        //DEBUG

        //if ( WeaponPickup.WeaponPickupClass == class'UTWeap_SniperRifle' )
		//{
            //DEBUG
            //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: UTWeap_SniperRifle found and is being replaced." );
            //DEBUG

			//WeaponPickup.WeaponPickupClass = class<UTWeapon>( DynamicLoadObject( "GearsOfUnreal.GOUWeap_SniperRifle", class'Class' ) );
			//WeaponPickup.InitializePickup();
		//}
        //else
        //{
            //DEBUG
            //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: WeaponPickup.WeaponPickupClass:"@WeaponPickup.WeaponPickupClass );
            //DEBUG
        //}
        
        //if ( WeaponPickup.WeaponPickupClass == class'UTWeap_Enforcer' )
		//{
            //DEBUG
            //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: UTWeap_Enforcer found and is being replaced." );
            //DEBUG

			//WeaponPickup.WeaponPickupClass = class<UTWeapon>( DynamicLoadObject( "GearsOfUnreal.GOUWeap_Enforcer", class'Class' ) );
			//WeaponPickup.InitializePickup();
		//}
        //else
        //{
            //DEBUG
            //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: WeaponPickup.WeaponPickupClass:"@WeaponPickup.WeaponPickupClass );
            //DEBUG
       //}
	}
	else
	{
		Locker = UTWeaponLocker( Other );
		if ( Locker != None )
		{
			for ( i = 0; i < Locker.Weapons.length; i++ )
			{
                //DEBUG
                //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: Locker.Weapons[i].WeaponClass:"@Locker.Weapons[i].WeaponClass );
                //DEBUG

                if ( Locker.Weapons[i].WeaponClass.IsA( 'UTWeap_SniperRifle' ) )
				{
                   Locker.ReplaceWeapon( i, class<UTWeapon>( DynamicLoadObject( "GearsOfUnreal.GOUWeap_SniperRifle", class'Class' ) ) );
				}
                
                if ( Locker.Weapons[i].WeaponClass.IsA( 'UTWeap_Enforcer' ) )
				{
                   Locker.ReplaceWeapon( i, class<UTWeapon>( DynamicLoadObject( "GearsOfUnreal.GOUWeap_Enforcer", class'Class' ) ) );
				}                
			}
		}
        else
        {
            //DEBUG
            //LogInternal( "MKLOG -> MK3rdPersonMutator.CheckReplacement: Locker == NONE" );
            //DEBUG
        }
	}
	return true;
}

defaultproperties
{
    Name="GearsOfUnreal"
}

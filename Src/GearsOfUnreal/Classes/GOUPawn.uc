class GOUPawn extends UTPawn;

/** INFO (neoaez) 20071128: config file variables
 * */
var config float CameraScaleAdjustSensitivity, MyCameraScale;
var() float CameraOffsetLength;
var float CurrentCameraDistance, CurrentCameraOffsetLength;
var float CameraScalePriorToFeignDeath, CurrentCameraScalePriorToFeignDeath, MinimumCameraScale, MaximumCameraScale;
var bool  bBehindViewPriorToFeignDeath;
var float newFOV;
var int CurrentViewRoll;

exec function ToggleCamera()
{    
    local GOUPlayerController PC;
    local bool bCurrentBehindView;
    local name NewMode;
    
    PC = GOUPlayerController( Controller );
    if ( PC != None )
    {
        bCurrentBehindView = PC.bBehindView;
        
        if( bCurrentBehindView )
        {
            NewMode = 'FirstPerson';
        }
        else
        {
            NewMode = 'ThirdPerson';
        }
        PC.SetCameraMode( NewMode );
    }
}
exec function SetFov(float fov)
{
	newFOV = fov;
}

exec function float GetFov()
{
	return newFOV;
}

simulated function bool CalcCamera( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{   
	//out_CamLoc = FixedViewLoc;
	if (bFixedView)
	{
		out_CamLoc = FixedViewLoc;
		out_CamRot = FixedViewRot;
	}
	else
	{
		if ( !IsFirstPerson() )	// Handle BehindView
		{
			CalcThirdPersonCam(fDeltaTime, out_CamLoc, out_CamRot, out_FOV);
		}
		else
		{
			// By default, we view through the Pawn's eyes..
			GetActorEyesViewPoint( out_CamLoc, out_CamRot );
		}

		if ( UTWeapon(Weapon) != none)
		{
			UTWeapon(Weapon).WeaponCalcCamera(fDeltaTime, out_CamLoc, out_CamRot);
		}
	}

	return true;
}

simulated function SetThirdPersonCamera(bool bNewBehindView)
{
    if ( bNewBehindView )
	{
		CameraZOffset = GetCollisionRadius() * 0.55;
	}

    SetMeshVisibility(bNewBehindView);
}

simulated function bool CalcThirdPersonCam( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
{
	local vector CamStart, HitLocation, HitNormal, CamDir;
	local float DesiredCameraZOffset;
	local Vector VectorX, VectorY, VectorZ, CamRotX, CamRotY, CamRotZ;

	//New Variables, delete them if no longer use for them
	local float DesiredCameraDistance;
	local bool bObstructed;
	local vector tempCamStart, tempCamEnd;
    //local float CameraOffsetRatio;
	local float fov;
	local Vector NewPos, VelBasedCamOffset, AngVel;
	local float VelSize;
	local int TargetRoll, DeltaRoll;

	fov = GetFov();
	
	VelBasedCamOffset.X = -0.05;
	VelBasedCamOffset.Y = 0.04f;
	VelBasedCamOffset.Z = 0;
	VelSize = FMin(VSize(Velocity), 800);
	NewPos = ((VelSize * VelBasedCamOffset) >> out_CamRot);

	ModifyRotForDebugFreeCam(out_CamRot);
	bObstructed = false;
	CamStart = Location;
	GetAxes(Normalize(out_CamRot), CamRotX, CamRotY, CamRotZ);

		if ( bWinnerCam )
		{
			// use "hero" cam
			SetHeroCam(out_CamRot);
		}
		else
		{
			/* Here we set Z value for camera, which means how height, camera will be pined.*/
			DesiredCameraZOffset = (Health > 0) ? GetCollisionRadius() * 0.35 : 0.f;
			CameraZOffset = (fDeltaTime < 0.2) ? DesiredCameraZOffset * 5 * fDeltaTime + (1 - 5*fDeltaTime) * CameraZOffset : DesiredCameraZOffset;
		}
		CamStart.Z += CameraZOffset;
		CamStart += WalkBob * 0.9;
		
		/*Playing with values there will result in diffrent camera behaviors.
		 *TODO: Camera still dont behave properly while looking up, need to be fixed*/
		//we calculate each set of vector, we dont use any kind of staic offsets there
		VectorX = CamRotX * GetCollisionRadius() * 3.55; //this vector determine depth of camera, how far from character it will be
		VectorY = CamRotY * GetCollisionRadius() * -1.9f; // this vector determine side of camera, negaive value pull character to left side, while positive to right side
		VectorZ = (GetCollisionRadius() /* FMax(0,(1.0-CamRotZ.Z))*/ * CamRotZ) * -1.55; //this value try to pull camera forward while pitching down, and back while pitching up, but pulling back seems to dont work
		CamDir = VectorX + VectorY + VectorZ;

		if ( (Health <= 0) || bFeigningDeath )
		{
			// adjust camera position to make sure it's not clipping into world
			// @todo fixmesteve.  Note that you can still get clipping if FindSpot fails (happens rarely)
			FindSpot(GetCollisionExtent(),CamStart);
		}
		
		//i have change from original code ">" to "<=", thats make possible of smoothy puling camera while pitching
		if (CamDir.Z <= GetCollisionRadius())
		{
			CamDir.Z *= Cos(out_CamRot.Pitch * 0.000000958738); // 0.0000958738 = 2*PI/65536	
		}
		
		out_CamLoc = (CamStart - CamDir) + NewPos;
	
		TargetRoll = 0;
	if(Mesh.BodyInstance != None)
	{
		AngVel = Mesh.BodyInstance.GetUnrealWorldAngularVelocity();
		TargetRoll = 0.4 * AngVel.Z * VelSize;
	}

	DeltaRoll = Clamp(TargetRoll - CurrentViewRoll, -100*fDeltaTime, 100*fDeltaTime);
	CurrentViewRoll += DeltaRoll;
	out_CamRot.Roll = CurrentViewRoll;

		//very hacky solution, will probably dont work in new game type or TC
		if (fov != 0)
			out_FOV = fov;

		//for debug only, draw small dots/crossairs on screen while moving around
		//DrawDebugCoordinateSystem(out_CamLoc, out_CamRot, 1.0, true);

		/* This code is from ActionGam, thanks for fall, for creating this. 
		 * It will determine back trace collision while closing to walls or sth like thaht*/
		if (Trace(HitLocation, HitNormal, out_CamLoc, CamStart, false, vect(12,12,12),,TRACEFLAG_Blocking) != None)
		{
    		DesiredCameraDistance = VSize(HitLocation-CamStart);
    		CurrentCameraDistance = (fDeltaTime < 0.5f) ? FClamp(DesiredCameraDistance * 2 * fDeltaTime + (1 - 2*fDeltaTime) * CurrentCameraDistance,0,DesiredCameraDistance) : DesiredCameraDistance;

    		HitLocation = CamStart + Normal(HitLocation-CamStart) * CurrentCameraDistance;

			//CameraOffsetRatio = CurrentCameraDistance/VSize(out_CamLoc - CamStart);
			out_CamLoc = HitLocation;
			bObstructed = true;
		}

		else
		{
    		DesiredCameraDistance = VSize(out_CamLoc-CamStart);
    		CurrentCameraDistance = (fDeltaTime < 0.5f) ? FClamp(DesiredCameraDistance * 2 * fDeltaTime + (1 - 2*fDeltaTime) * CurrentCameraDistance,0,DesiredCameraDistance) : DesiredCameraDistance;

			HitLocation = CamStart + Normal(out_CamLoc - CamStart) * CurrentCameraDistance;

			//CameraOffsetRatio = CurrentCameraDistance/VSize(out_CamLoc - CamStart);
			out_CamLoc = HitLocation;
		}

		if (Trace(HitLocation, HitNormal, out_CamLoc, CamStart, false, vect(12,12,12)) != None)
		{
			out_CamLoc = HitLocation;
			return false;
		}
		
		/*Again thanks for fall, for this. It just inside character collision detection*/
		tempCamStart = CamStart;
		tempCamStart.Z = 0;
		tempCamEnd = out_CamLoc;
		tempCamEnd.Z = 0;

		if(bObstructed && (VSize(tempCamEnd - tempCamStart) < CylinderComponent.CollisionRadius*1.25) && (out_CamLoc.Z<Location.Z+CylinderComponent.CollisionHeight) && (out_CamLoc.Z>Location.Z-CylinderComponent.CollisionHeight))
		{
			SetHidden(true);
		}
		else
			SetHidden(false);

		return !bObstructed;

}

state FeigningDeath
{
    /* ignores ServerHoverboard, SwitchWeapon, QuickPick, FaceRotation, ForceRagdoll, AdjustCameraScale, SetMovementPhysics; */

    exec simulated function FeignDeath()
    {
        if ( bFeigningDeath )
        {
            Global.FeignDeath();
        }
    }

    reliable server function ServerFeignDeath()
    {
        if ( Role == ROLE_Authority && !WorldInfo.GRI.bMatchIsOver && !IsTimerActive( 'FeignDeathDelayTimer' ) && bFeigningDeath )
        {
            bFeigningDeath = false;
            PlayFeignDeath();
        }
    }

    event bool EncroachingOn( Actor Other )
    {
        // don't abort moves in ragdoll
        return false;
    }

    simulated function bool CanThrowWeapon()
    {
        return false;
    }

    simulated function Tick( float DeltaTime )
    {
        local rotator NewRotation;

        if ( bPlayingFeignDeathRecovery && PlayerController( Controller ) != None )
        {
            // interpolate Controller yaw to our yaw so that we don't get our rotation snapped around when we get out of feign death
            NewRotation = Controller.Rotation;
            NewRotation.Yaw = RInterpTo( NewRotation, Rotation, DeltaTime, 2.0 ).Yaw;
            Controller.SetRotation( NewRotation );

            if ( WorldInfo.TimeSeconds - FeignDeathRecoveryStartTime > 0.8 )
            {
                CameraScale = 1.0;
            }
        }
    }

    simulated function bool CalcThirdPersonCam( float fDeltaTime, out vector out_CamLoc, out rotator out_CamRot, out float out_FOV )
    {
        local vector CamStart, HitLocation, HitNormal, CamDir;
        local RB_BodyInstance RootBodyInst;
        local matrix RootBodyTM;

        if ( CurrentCameraScale < CameraScale )
        {
            CurrentCameraScale = FMin( CameraScale, CurrentCameraScale + 5 * FMax( CameraScale - CurrentCameraScale, 0.3 )*fDeltaTime ); 
        }
        else if ( CurrentCameraScale > CameraScale )
        {
            CurrentCameraScale = FMax( CameraScale, CurrentCameraScale - 5 * FMax( CurrentCameraScale - CameraScale, 0.3 )*fDeltaTime );
        }

        //CamStart = Mesh.Bounds.Origin + vect(0,0,1) * BaseEyeHeight; (Replaced with below due to Bounds being updated 2x per frame 
        //which can result in jitter-cam
        CamStart = Mesh.GetPosition();
        if( Mesh.PhysicsAssetInstance != None )
        {
            RootBodyInst = Mesh.PhysicsAssetInstance.Bodies[Mesh.PhysicsAssetInstance.RootBodyIndex];
            if( RootBodyInst.IsValidBodyInstance() )
            {
                RootBodyTM = RootBodyInst.GetUnrealWorldTM();
                CamStart.X = RootBodyTM.WPlane.X;
                CamStart.Y = RootBodyTM.WPlane.Y;
                CamStart.Z = RootBodyTM.WPlane.Z;
            }
        }
        CamStart += vect( 0,0,1 ) * BaseEyeHeight;

        CamDir = vector( out_CamRot ) * GetCollisionRadius() * CurrentCameraScale;
//      `log("Mesh"@Mesh.Bounds.Origin@" --- Base Eye Height "@BaseEyeHeight);

        if ( CamDir.Z > GetCollisionHeight() )
        {
            CamDir *= square( cos( out_CamRot.Pitch * 0.0000958738 ) ); // 0.0000958738 = 2*PI/65536
        }
        out_CamLoc = CamStart - CamDir;
        if ( Trace( HitLocation, HitNormal, out_CamLoc, CamStart, false, vect( 12,12,12 ) ) != None )
        {
            out_CamLoc = HitLocation;
        }
        return true;
    }

    simulated event OnAnimEnd( AnimNodeSequence SeqNode, float PlayedTime, float ExcessTime )
    {
        if ( Physics != PHYS_RigidBody && !bPlayingFeignDeathRecovery )
        {
            // blend out of feign death animation
            if ( FeignDeathBlend != None )
            {
                FeignDeathBlend.SetBlendTarget( 0.0, 0.5 );
            }
            GotoState( 'Auto' );
        }
    }

    simulated event BeginState( name PreviousStateName )
    {
        local UTPlayerController PC;
        local UTWeapon UTWeap;

        bCanPickupInventory = false;
        StopFiring();
        bNoWeaponFiring = true;

        UTWeap = UTWeapon( Weapon );
        if ( UTWeap != None )
        {
            UTWeap.PlayWeaponPutDown();
        }
        if( UTWeap != none && PC != none )
        {
            UTPlayerController( Controller ).EndZoom();
        }

        PC = UTPlayerController( Controller );
        if ( PC != None )
        {
            CameraScalePriorToFeignDeath = CameraScale;
            CurrentCameraScalePriorToFeignDeath = CurrentCameraScale;
            bBehindViewPriorToFeignDeath = PC.bBehindView;
            PC.SetBehindView( true );
            CurrentCameraScale = 1.5;
            CameraScale = 2.25;
        }

        DropFlag();
    }

    simulated function EndState( name NextStateName )
    {
        local UTPlayerController PC;
        local UTPawn P;

        if ( NextStateName != 'Dying' )
        {
            bNoWeaponFiring = default.bNoWeaponFiring;
            bCanPickupInventory = default.bCanPickupInventory;
            Global.SetMovementPhysics();
            PC = UTPlayerController( Controller );
            if ( PC != None )
            {
               PC.SetBehindView( bBehindViewPriorToFeignDeath );
            }

             CurrentCameraScale = CurrentCameraScalePriorToFeignDeath;
            CameraScale = CameraScalePriorToFeignDeath;
            bForcedFeignDeath = false;
            bPlayingFeignDeathRecovery = false;

            // jump away from other feigning death pawns to make sure we don't get stuck
            foreach TouchingActors( class'UTPawn', P )
            {
                if ( P.IsInState( 'FeigningDeath' ) )
                {
                    JumpOffPawn();
                }
            }
        }
    }
}

/** INFO (neoaez) 20080610: Thanks to the ActionCam mutator for this fix. 
 * Correct aim to sync projectile trajectory with camera trace
 * */
simulated singular function Rotator GetBaseAimRotation()
{
    local vector    POVLoc;
    local rotator   POVRot;
    local UTPlayerController PC;

    local vector TargetLocation;
    local vector HitLocation, HitNormal;
    local int TraceScalar;

    // If we have a controller, by default we aim at the player's 'eyes' direction
    // that is by default Controller.Rotation for AI, and camera (crosshair) rotation for human players.
    if( Controller != None && !InFreeCam() )
    {
        Controller.GetPlayerViewPoint(POVLoc, POVRot);        
        PC = UTPlayerController( Controller );
        
        //DEBUG
        //LogInternal( "MKLOG -> MKPawn.GetBaseAimRotation: PC.bBehindView = "@PC.bBehindView );
        //DEBUG
        
        /** INFO (neoaez) 20080615: If we are in first-person then there is no need to correct the original
         * aim code provided by EPIC.  
         * */
        if ( !PC.bBehindView )
        {
            
            //DEBUG
            //LogInternal( "MKLOG -> MKPawn.GetBaseAimRotation: Was first-person so returning POVRot" );
            //DEBUG           
            return POVRot; 
        }
        //DEBUG
        //LogInternal( "MKLOG -> MKPawn.GetBaseAimRotation: Not first-person so adjusting POVRot" );
        //DEBUG           
        
        /** INFO (neoaez) 20080610: changed ActionCam code in the following line from this:        
         * TargetLocation = POVLoc + (vector(POVRot)*100000);
         * to remove arbitray scalar used in trace to that of the weapon equipped.  This follows the
         * example set out by the function GetWeaponAim in UTVehicle  Fixes an accuracy issue when
         * in third-person.
         * */
         if (Weapon.bMeleeWeapon) { TraceScalar = 100000; }
         else { TraceScalar = Weapon.GetTraceRange(); }
         TargetLocation = POVLoc + ( vector( POVRot ) * TraceScalar );
         if( Trace( HitLocation, HitNormal, TargetLocation, POVLoc, false,,,TRACEFLAG_Bullet ) == None )
         {
             HitLocation = TargetLocation;
         }
         POVRot = rotator( HitLocation - GetWeaponStartTraceLocation() );

        return POVRot;
    }

    // If we have no controller, we simply use our rotation
    POVRot = Rotation;

    // If our Pitch is 0, then use RemoveViewPitch
    if( POVRot.Pitch == 0 )
    {
        POVRot.Pitch = RemoteViewPitch << 8;
    }

    return POVRot;
}

reliable client function ClientInitInteraction()
{
    local PlayerController PC;
    local int i;
    local bool bFound;
    
    PC = PlayerController( Controller );
    if ( PC == None ) { return; }

    //DEBUG
    //LogInternal("MKLOG -> MKPawn.ClientInitInteraction: Inspecting "$PC$" for interactions :");
    //DEBUG
    
    for (i = 0; i < PC.Interactions.Length; i++)
    {
        //DEBUG
        //LogInternal("MKLOG -> MKPawn.ClientInitInteraction: ... found "$PC.Interactions[i]);
        //DEBUG
    
        if (GOUInteraction( PC.Interactions[i] ) != None) { bFound = true; }               
    }

    if (!bFound)
    {
        PC.Interactions.Insert( 0, 1 );
        PC.Interactions[0] = new( PC ) class'GOUInteraction';
        GOUInteraction( PC.Interactions[0] ).InitInteraction( PC );
        
        //DEBUG
        //LogInternal( "MKLOG -> MKPawn.ClientInitInteraction: PC added a MK3rdPersonMutatorInteraction" );
        //DEBUG
    }
}

defaultproperties
{
	CameraOffsetLength=20.000000
	CameraScale=4.000000
    CurrentCameraScale=1.00000
	ViewPitchMax=12000
	ViewPitchMin=-12000
	//bStartIn3rdPerson=True
}

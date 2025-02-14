/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class CastleGame extends MobileGame;

/** Set to true to allow attract mode */
var config bool bAllowAttractMode;


event OnEngineHasLoaded()
{
	// show a Start button on top of the loading movie that is still playing
	class'Engine'.static.AddOverlay(class'Engine'.static.GetLargeFont(), "Start", 0, 0.8, 2.0, 2.0, true);
}

/**
 * Don't allow dying in CastleGame!
 */
function bool PreventDeath(Pawn KilledPawn, Controller Killer, class<DamageType> DamageType, vector HitLocation)
{
	return true;
}

static event class<GameInfo> SetGameType(string MapName, string Options, string Portal)
{
	// We'll only force CastleGame game type for maps that we know were build for Epic Citadel (EpicCitadel).
	// Note that ignore any possible prefix on the map file name so that PIE and Play On still work with this.
	if( Right( MapName, 11 ) ~= "EpicCitadel" ||
		InStr( MapName, "EpicCitadel." ) != -1 )
	{
		return super.SetGameType(MapName, Options, Portal);
	}

	return class'MobileGame.MobileGame';
}

defaultproperties
{
	PlayerControllerClass=class'MobileGame.CastlePC'
	HUDType=class'MobileGame.MobileHUDExt'
}




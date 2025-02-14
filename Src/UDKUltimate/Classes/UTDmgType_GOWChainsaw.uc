/**
 * UTDmgType_Rocket
 *
 *
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

class UTDmgType_GOWChainsaw extends UTDmgType_Burning
	abstract;

defaultproperties
{
	KillStatsName=KILLS_ROCKETLAUNCHER
	DeathStatsName=DEATHS_ROCKETLAUNCHER
	SuicideStatsName=SUICIDES_ROCKETLAUNCHER
	RewardCount=15
	RewardEvent=REWARD_ROCKETSCIENTIST
	RewardAnnouncementSwitch=2
	DamageWeaponClass=class'UTWeap_GOWChainsaw'
	DamageWeaponFireMode=0
	VehicleMomentumScaling=4.0
	VehicleDamageScaling=0.8
	NodeDamageScaling=1.1
	bThrowRagdoll=true
	GibPerterbation=0.15
	AlwaysGibDamageThreshold=999
	CustomTauntIndex=7

	//udk ultimate blood splatter on wall//
	bCausesBloodSplatterDecals=True
	bCausesBlood=True
	//udk ultimate blood splatter on wall//
	
}

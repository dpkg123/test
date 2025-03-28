/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_HeadTrackingControl extends SequenceAction
	native(Sequence);

/** SkelControlLookAt name in the AnimTree of the SkeletalMesh **/
var() array<name>  TrackControllerName;

/** Will pick up actor within this radius **/
var() float LookAtActorRadius;

/** Interp back to zero strength if limit surpassed */
var() bool	bDisableBeyondLimit;

/** How long can one person to look at one **/
var() float MaxLookAtTime;

/** At least this time to look at one **/
var() float MinLookAtTime;

/** Once entered the radius, how long do I really care to look  ? This affects rating. It will give benefit to the person who just entered **/
var() float MaxInterestTime;

/** Quick check box for allowing it to look Pawn - due to Pawn not being listed in the Actor class **/
var(Target) bool bLookAtPawns;

/** Actor classes to look at as 0 index being the highest priority if you have anything specific **/
var(Target) array< class<Actor> >  ActorClassesToLookAt;

/** Target Bone Names, where to look at - priority from top to bottom, if not found, it will continue search **/
var(Target) array<name>     TargetBoneNames;

/** List of objects to call the handler function on */
var() array<Object> LookAtTargets;

/** Array of actor information **/
var private const transient native map{class AActor*,class UHeadTrackingComponent*} ActorToComponentMap;

cpptext
{
 	virtual void Activated();
};

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="HeadTracking Control"
	ObjCategory="Actor"

	InputLinks(0)=(LinkDesc="Enable")
	InputLInks(1)=(LinkDesc="Disable")

	OutputLinks(0)=(LinkDesc="Enabled")
	OutputLinks(1)=(LinkDesc="Disabled")

	TrackControllerName.Add("HeadLook")
	TrackControllerName.Add("LeftEyeLook")
	TrackControllerName.Add("RightEyeLook")

	ActorClassesToLookAt.Empty

	MinLookAtTime = 3.f
	MaxLookAtTime = 5.f
	MaxInterestTime = 7.f
	bLookAtPawns = true

	LookAtActorRadius = 500.f

	bDisableBeyondLimit	= true

	TargetBoneNames.Empty
	TargetBoneNames.Add("b_MF_Head")
	TargetBoneNames.Add("b_MF_Neck")

	bAutoActivateOutputLinks=false
	bCallHandler=false
}





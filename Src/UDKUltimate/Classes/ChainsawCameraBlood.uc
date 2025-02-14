/**
 * Pub Games Bloody Screen Tutorial
 * 
 * Sample lens effect
 *
 * http://www.pubgames.net.au
 */
 
class ChainsawCameraBlood extends EmitterCameraLensEffectBase;

defaultproperties
{
	// Reference the particle system to be used for this lens effect
	PS_CameraEffect=ParticleSystem'GOW_CameraBlood_UDK.Effects.P_FX_Chainsaw_Blood'

	// Allow multiple instances of the lens effect to exist
	bAllowMultipleInstances=true

	// Set the distance from the camera to high.
	DistFromCamera=870
}
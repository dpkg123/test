/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform mat4 LocalToWorld;
uniform mat4 ViewProjection;
uniform vec4 CameraRight;
uniform vec4 CameraUp;
uniform float AxisRotationVectorSourceIndex;
uniform vec4 AxisRotationVectors[2];
uniform vec3 ParticleUpRightResultScalars;

attribute vec4 Position;
attribute vec4 OldPosition;
attribute vec3 Size;
attribute float Rotation;
#if SUBUV_PARTICLES
	attribute vec4 TexCoords0;
#else
	attribute vec2 TexCoords0;
#endif
attribute vec4 ParticleColor;
#if SUBUV_PARTICLES
	attribute vec4 Interp_Sizer;
	VARYING_DEFAULT vec4 Interp_TexCoord;
#else	
	VARYING_DEFAULT vec2 Interp_TexCoord;
#endif
VARYING_LOW vec4 Interp_Color;
#if SUBUV_PARTICLES
	VARYING_LOW float Interp_BlendAlpha;
#endif

vec3 SafeNormalize(vec3 V)
{
	return V / sqrt(max(dot(V, V), 0.01));
}

void main()
{
	vec4 Pos = LocalToWorld * Position;
	vec4 OldPos = LocalToWorld * OldPosition;

	vec4 Right_Final;
	vec4 Up_Final;

#if PARTICLE_SCREEN_ALIGNMENT == PARTICLESCREENALIGN_CAMERAFACING
	Right_Final = (-1.0 * cos(Rotation) * CameraUp) + (sin(Rotation) * CameraRight);
	Up_Final = (sin(Rotation) * CameraUp) + (cos(Rotation) * CameraRight);
#else
	vec3 CameraDirection = SafeNormalize(CameraWorldPosition - Pos.xyz);
	#if PARTICLE_SCREEN_ALIGNMENT == PARTICLESCREENALIGN_VELOCITY
		vec3 ParticleDirection = SafeNormalize(Pos.xyz - OldPos.xyz);
		Right_Final = vec4(SafeNormalize(cross(CameraDirection, ParticleDirection)), 0.0);
		Up_Final = vec4(-ParticleDirection, 0.0);
	#else	// PARTICLESCREENALIGN_LOCKEDAXIS
		vec4 AxisRotationResultVectors[2];
		int ARVIndex = int(AxisRotationVectorSourceIndex);
		vec4 AxisSource = AxisRotationVectors[ARVIndex];
		
		vec4 Axis_Calculation = vec4(SafeNormalize(cross(CameraDirection, AxisSource.xyz)), 0.0) * AxisSource.w;
		
		Right_Final = (Axis_Calculation * AxisRotationVectorSourceIndex) + (AxisSource * (1.0 - AxisRotationVectorSourceIndex));
		Up_Final = Axis_Calculation * (1.0 - AxisRotationVectorSourceIndex) + (AxisSource * AxisRotationVectorSourceIndex);
		Right_Final.w = 0.0;
		Up_Final.w = 0.0;
	#endif
#endif

	Pos =	Pos +
#if SUBUV_PARTICLES
			Size.x * (Interp_Sizer.z - 0.5) * Right_Final +
			Size.y * (Interp_Sizer.w - 0.5) * Up_Final;
#else
			Size.x * (TexCoords0.x - 0.5) * Right_Final +
			Size.y * (TexCoords0.y - 0.5) * Up_Final;
#endif

	gl_Position = ViewProjection * Pos;

		
	Interp_TexCoord = BASE_TEX_COORD_XFORM(TexCoords0);
	Interp_Color = ParticleColor;
#if USE_PREMULTIPLIED_OPACITY
	Interp_Color.rgb *= Interp_Color.a;
#endif
#if SUBUV_PARTICLES
	Interp_BlendAlpha = Interp_Sizer.x;
#endif
}

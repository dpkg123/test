/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform mat4 LocalToWorld;
uniform mat4 ViewProjection;
uniform vec4 CameraRight;
uniform vec4 CameraUp;

attribute vec4 Position;
attribute vec3 Size;
attribute float Rotation;
attribute vec2 TexCoords0;
attribute vec4 ParticleColor;

VARYING_DEFAULT vec2 Interp_TexCoord;
VARYING_LOW vec4 Interp_Color;

void main()
{
	vec4 Pos = LocalToWorld * Position;

	vec4 Right	= (-1.0 * cos(Rotation) * CameraUp) + (sin(Rotation) * CameraRight);
	vec4 Up		= (      sin(Rotation) * CameraUp) + (cos(Rotation) * CameraRight);

	Pos =			Pos +
					Size.x * (TexCoords0.x - 0.5) * Right +
					Size.y * (TexCoords0.y - 0.5) * Up;

	gl_Position = ViewProjection * Pos;

		
	Interp_TexCoord = BASE_TEX_COORD_XFORM(TexCoords0);
	Interp_Color = ParticleColor;
#if USE_PREMULTIPLIED_OPACITY
	Interp_Color.rgb *= Interp_Color.a;
#endif
}

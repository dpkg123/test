/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform mat4 LocalToWorld;
uniform mat4 ViewProjection;

attribute vec4 Position;
attribute float Size;
attribute vec4 Color;

VARYING_LOW vec4 Interp_Color;

void main()
{
	vec4 Pos = LocalToWorld * Position;
	gl_Position = ViewProjection * Pos;
		
	Interp_Color = Color.bgra;
#if USE_PREMULTIPLIED_OPACITY
	Interp_Color.rgb = Interp_Color.a;
#endif
	gl_PointSize = Size;
}

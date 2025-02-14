/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform mat4 Transform;

attribute vec4 Position;
attribute vec2 TexCoords0;
attribute vec4 Color;

VARYING_DEFAULT vec2 UVBase;
VARYING_LOW vec4 PrimColor;

void main()
{
	gl_Position = Transform * Position;
	UVBase = BASE_TEX_COORD_XFORM(TexCoords0);
	PrimColor = Color;
}

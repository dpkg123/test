/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

attribute vec4 Position;
attribute vec2 TexCoords0;

VARYING_DEFAULT vec2 UVBase;

void main()
{
	gl_Position = Position;
	UVBase = BASE_TEX_COORD_XFORM(TexCoords0);
}

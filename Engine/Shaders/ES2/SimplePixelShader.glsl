/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform sampler2D TextureBase;

VARYING_DEFAULT vec2 UVBase;
VARYING_LOW vec4 PrimColor;

void main()
{
	lowp vec4 BaseColor = texture2D(TextureBase, UVBase);
	
	ALPHAKILL(BaseColor.w)
	
	lowp vec4 PolyColor = PrimColor * BaseColor;

	gl_FragColor = PolyColor;
}


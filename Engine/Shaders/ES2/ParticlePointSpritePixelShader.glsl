/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform sampler2D TextureBase;

VARYING_LOW vec4 Interp_Color;

// Embodies various global color changes (i.e. full screen effects, fog)
uniform vec4 FadeColorAndAmount;


void main()
{
	lowp vec4 BaseColor = texture2D(TextureBase, gl_PointCoord);
	
	// alpha kill if enabled
	ALPHAKILL(BaseColor.w)

	lowp vec4 PolyColor = BaseColor;

	PolyColor *= Interp_Color;
	PolyColor.xyz = mix( PolyColor.xyz, FadeColorAndAmount.xyz, FadeColorAndAmount.w );
	
	gl_FragColor = PolyColor;
}


/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform sampler2D TextureBase;

#if SUBUV_PARTICLES
	VARYING_DEFAULT vec4 Interp_TexCoord;
#else
	VARYING_DEFAULT vec2 Interp_TexCoord;
#endif
VARYING_LOW vec4 Interp_Color;
#if SUBUV_PARTICLES
	VARYING_LOW float Interp_BlendAlpha;
#endif

// Embodies various global color changes (i.e. full screen effects, fog)
uniform lowp vec4 FadeColorAndAmount;

void main()
{
#if SUBUV_PARTICLES
	lowp vec4 BaseColor1 = texture2D(TextureBase, Interp_TexCoord.xy);
	lowp vec4 BaseColor2 = texture2D(TextureBase, Interp_TexCoord.zw);
	lowp vec4 BaseColor = mix(BaseColor1, BaseColor2, Interp_BlendAlpha);
#else
	lowp vec4 BaseColor = texture2D(TextureBase, Interp_TexCoord);
#endif	

	// alpha kill if enabled
	ALPHAKILL(BaseColor.w)

	lowp vec4 PolyColor = BaseColor;

	PolyColor *= Interp_Color;
	PolyColor.xyz = mix( PolyColor.xyz, FadeColorAndAmount.xyz, FadeColorAndAmount.w );
	
	gl_FragColor = PolyColor;
}

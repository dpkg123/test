/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform sampler2D TextureBase;

uniform vec3 ColorScale;
uniform vec4 OverlayColor;
uniform float InverseGamma;

VARYING_DEFAULT vec2 UVBase;

void main()
{
	vec4 SceneColor = texture2D(TextureBase, UVBase);
	
//	vec3 LinearColor = mix(SceneColor.xyz * ColorScale, OverlayColor.xyz, OverlayColor.w);
//	gl_FragColor = vec4(pow(clamp(LinearColor, 0.0, 1.0), InverseGamma), ScaneColor.A);
	
	gl_FragColor = SceneColor;
}

/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

#if !DEPTH_ONLY

uniform sampler2D TextureBase;
VARYING_HIGH vec2 UVBase;


#if USE_LIGHTMAP
	uniform sampler2D TextureLightmap;
	#if !USE_LIGHTMAP_FIXED_SCALE
		// We must use at mediump in order to handle larger light map scale values
		uniform mediump vec4 LightMapScale;
	#endif
	VARYING_HIGH vec2 UVLightmap;
#endif


#if USE_VERTEX_LIGHTMAP
	VARYING_LOW vec3 PrelitColor;
#endif

//whether to bring in per vertex and/or uniform color
#if USE_UNIFORM_COLOR_MULTIPLY || USE_VERTEX_COLOR_MULTIPLY
	VARYING_LOW vec4 ColorMultiply;
#endif

#define NEED_EXTRA_COLOR_TEXTURE 0
#define NEED_MASK_TEXTURE 0
#define NEED_NORMAL_TEXTURE 0
#define NEED_PIXEL_WORLD_POSITION 0
#define NEED_PIXEL_TANGENT_TO_WORLD 0
#define NEED_PIXEL_TANGENT_HALF_VECTOR 0
#define NEED_PIXEL_TANGENT_LIGHT_DIRECTION 0
#define NEED_PIXEL_TANGENT_UP_VECTOR 0
#define NEED_PIXEL_TANGENT_CAMERA_VECTOR 0
#define NEED_LIGHTING_COLOR 0
#define NEED_DIRECTIONAL_LIGHT_UNIFORMS 0
#define NEED_PER_PIXEL_RIM_TERM 0


#if RENORMALIZE_INTERPOLATED_NORMALS
	#define OPTIONAL_NORMALIZE( Vec ) normalize( Vec )
#else
	#define OPTIONAL_NORMALIZE( Vec ) Vec
#endif


#if USE_DYNAMIC_DIRECTIONAL_LIGHT
	#if USE_NORMAL_MAPPING
		uniform lowp vec4 LightColorAndFalloffExponent;

		#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
			#undef NEED_PIXEL_TANGENT_LIGHT_DIRECTION
			#define NEED_PIXEL_TANGENT_LIGHT_DIRECTION 1
		#else
			#undef NEED_DIRECTIONAL_LIGHT_UNIFORMS
			#define NEED_DIRECTIONAL_LIGHT_UNIFORMS 1
		#endif
	#else
		#undef NEED_LIGHTING_COLOR
		#define NEED_LIGHTING_COLOR 1
	#endif
#endif


#if USE_DYNAMIC_SKY_LIGHT
	#if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_DYNAMIC_SKY_LIGHT
		#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
			#undef NEED_PIXEL_TANGENT_UP_VECTOR
			#define NEED_PIXEL_TANGENT_UP_VECTOR 1
		#endif
		
		// @todo: Currently these parameters will only be set for actors with light environments!!
		uniform lowp vec4 LowerSkyColor;
		uniform lowp vec4 UpperSkyColor;
	#else
		#undef NEED_LIGHTING_COLOR
		#define NEED_LIGHTING_COLOR 1
	#endif
#endif


#if USE_COLOR_TEXTURE_BLENDING
	#if TEXTURE_BLEND_FACTOR_SOURCE == TEXTURE_BLEND_FACTOR_SOURCE_VERTEX_COLOR
		VARYING_LOW float TextureBlendFactor;
	#endif
	
	#undef NEED_EXTRA_COLOR_TEXTURE
	#define NEED_EXTRA_COLOR_TEXTURE 1

	#if TEXTURE_BLEND_FACTOR_SOURCE == TEXTURE_BLEND_FACTOR_SOURCE_MASK_TEXTURE
		#undef NEED_MASK_TEXTURE
		#define NEED_MASK_TEXTURE 1
	#endif
#endif


#if USE_SPECULAR
	#if SPECULAR_MASK == SPECMASK_MASK_TEXTURE_RGB
		// Using specular mask from mask texture
		#undef NEED_MASK_TEXTURE
		#define NEED_MASK_TEXTURE 1
	#endif
#endif


// Embodies various global color changes (i.e. full screen effects, fog)
uniform lowp vec4 FadeColorAndAmount;


#if USE_GRADIENT_FOG
	VARYING_LOW vec4 FogColorAndAmount;
#endif


#if NEED_EXTRA_COLOR_TEXTURE
	uniform sampler2D TextureDetail;
	
	// @todo: We could opt to only include this when UVTransform features are enabled (UVs are actually different)
	VARYING_HIGH vec2 UVDetail;
#endif


#if USE_VERTEX_SPECULAR
	VARYING_LOW vec3 Specular;
#endif


#if USE_PIXEL_SPECULAR
	// NOTE: Power must be at least mediump to handle sharp specular highlights
	uniform mediump float SpecularPower;
	uniform lowp vec3 LightColorTimesSpecularColor;
#endif

#if USE_PIXEL_SPECULAR
	#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
		#undef NEED_PIXEL_TANGENT_HALF_VECTOR
		#define NEED_PIXEL_TANGENT_HALF_VECTOR 1

		#undef NEED_PIXEL_TANGENT_LIGHT_DIRECTION
		#define NEED_PIXEL_TANGENT_LIGHT_DIRECTION 1
	#else
		#undef NEED_DIRECTIONAL_LIGHT_UNIFORMS
		#define NEED_DIRECTIONAL_LIGHT_UNIFORMS 1
		
		#undef NEED_PIXEL_WORLD_POSITION 
		#define NEED_PIXEL_WORLD_POSITION 1
	#endif
#endif


#if USE_AMBIENT_OCCLUSION
	VARYING_LOW float AmbientOcclusion;
#endif


#if NEED_DIRECTIONAL_LIGHT_UNIFORMS
	// @todo: This is a little weird because for actors with light environments, this direction will be
	//   driven by the light environment direction, otherwise we use the brightest directional light
	//   in the scene.  However, for specular the color always comes from the brightest directional light.
	uniform lowp vec4 LightDirectionAndbDirectional;
#endif


#if USE_ENVIRONMENT_MAPPING
	uniform sampler2D TextureEnvironment;
	uniform vec3 EnvironmentColorScale;
	
	#if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_ENVIRONMENT_MAPPING
		uniform vec3 EnvironmentParameters;

		#undef NEED_PER_PIXEL_RIM_TERM
		#define NEED_PER_PIXEL_RIM_TERM 1
	   
		#if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_RED || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_GREEN || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_BLUE || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_ALPHA
			VARYING_LOW float EnvironmentMask;
		#endif
	#else
		VARYING_LOW vec4 EnvironmentVectorAndAmount;
	#endif
	
	#if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_RED || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_GREEN || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_BLUE || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_ALPHA
		#undef NEED_MASK_TEXTURE
		#define NEED_MASK_TEXTURE 1
	#endif

	#if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_NORMAL_TEXTURE_ALPHA
		#undef NEED_NORMAL_TEXTURE
		#define NEED_NORMAL_TEXTURE 1
	#endif
#endif


#if USE_EMISSIVE
	#if EMISSIVE_COLOR_SOURCE == EMISSIVE_COLOR_SOURCE_EMISSIVE_TEXTURE
		uniform sampler2D TextureEmissive;    
	#elif EMISSIVE_COLOR_SOURCE == EMISSIVE_COLOR_SOURCE_CONSTANT
		uniform vec4 ConstantEmissiveColor;
	#endif

	#if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_RED || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_GREEN || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_BLUE || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_ALPHA
		VARYING_LOW float EmissiveMask;
	#endif

	#if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_RED || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_GREEN || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_BLUE || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_ALPHA
		#undef NEED_MASK_TEXTURE
		#define NEED_MASK_TEXTURE 1
	#endif

	#if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_NORMAL_TEXTURE_ALPHA
		#undef NEED_NORMAL_TEXTURE
		#define NEED_NORMAL_TEXTURE 1
	#endif
#endif


#if USE_RIM_LIGHTING
	#if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_RIM_LIGHTING
		uniform vec4 RimLightingColorAndExponent;

		#undef NEED_PER_PIXEL_RIM_TERM
		#define NEED_PER_PIXEL_RIM_TERM 1

		#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
			VARYING_LOW float RimLightingMask;
		#endif
	#else
		#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_CONSTANT || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
			#undef NEED_LIGHTING_COLOR
			#define NEED_LIGHTING_COLOR 1
		#else    
			VARYING_LOW vec3 MaskableRimLighting;
		#endif
	#endif

	#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_ALPHA
		#undef NEED_MASK_TEXTURE
		#define NEED_MASK_TEXTURE 1
	#endif

	#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_NORMAL_TEXTURE_ALPHA
		#undef NEED_NORMAL_TEXTURE
		#define NEED_NORMAL_TEXTURE 1
	#endif
#endif


#if USE_NORMAL_MAPPING
	#undef NEED_NORMAL_TEXTURE
	#define NEED_NORMAL_TEXTURE 1

	#if !PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
		#undef NEED_PIXEL_TANGENT_TO_WORLD
		#define NEED_PIXEL_TANGENT_TO_WORLD 1
	#endif
#endif


#if USE_BUMP_OFFSET
	uniform float PreMultipliedBumpReferencePlane;
	uniform float BumpHeightRatio;

	#undef NEED_PIXEL_TANGENT_CAMERA_VECTOR
	#define NEED_PIXEL_TANGENT_CAMERA_VECTOR 1
	
	#undef NEED_MASK_TEXTURE
	#define NEED_MASK_TEXTURE 1
#endif

#if NEED_PER_PIXEL_RIM_TERM
	#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
		#undef NEED_PIXEL_TANGENT_CAMERA_VECTOR
		#define NEED_PIXEL_TANGENT_CAMERA_VECTOR 1
	#else
		#undef NEED_PIXEL_WORLD_POSITION 
		#define NEED_PIXEL_WORLD_POSITION 1
	#endif
#endif

#if NEED_PIXEL_TANGENT_CAMERA_VECTOR
	VARYING_LOW vec3 VarTangentCameraVector;
#endif

#if NEED_PIXEL_TANGENT_TO_WORLD
	VARYING_LOW vec3 VarWorldNormal;
	VARYING_LOW vec3 VarWorldTangent;
	#if !COMPUTE_BINORMAL_IN_PIXEL_SHADER
		VARYING_LOW vec3 VarWorldBinormal;
	#endif
#endif

#if NEED_PIXEL_WORLD_POSITION
	VARYING_MEDIUM vec3 VarWorldPosition;
#endif

#if NEED_PIXEL_TANGENT_LIGHT_DIRECTION
	VARYING_LOW vec3 VarTangentLightDirection;
#endif

#if NEED_PIXEL_TANGENT_UP_VECTOR
	VARYING_LOW vec3 VarTangentUpVector;
#endif

#if NEED_PIXEL_TANGENT_HALF_VECTOR
	VARYING_LOW vec3 VarTangentHalfVector;
#endif

#if NEED_NORMAL_TEXTURE
	uniform sampler2D TextureNormal;
#endif

#if NEED_MASK_TEXTURE
	uniform sampler2D TextureMask;
	VARYING_HIGH vec2 UVMask;
#endif

#if NEED_LIGHTING_COLOR
	VARYING_LOW vec3 LightingColor;
#endif

#endif // #if !DEPTH_ONLY

void main()
{
	// Use for debugging; will be optimized away by compiler if not needed
	// To draw ONLY this color, simply set a value then #define SHOW_DEBUG_COLOR
	vec3 DebugColor;


#if DEPTH_ONLY
	gl_FragColor = vec4(0,0,0,0);
#else
	highp vec2 FinalBaseUV = UVBase;
	
	#if NEED_PIXEL_TANGENT_HALF_VECTOR
		lowp vec3 TangentHalfVector = OPTIONAL_NORMALIZE( VarTangentHalfVector );
	#endif

	#if NEED_PIXEL_TANGENT_LIGHT_DIRECTION
		lowp vec3 TangentLightDirection = OPTIONAL_NORMALIZE( VarTangentLightDirection );
	#endif
	
	#if NEED_PIXEL_TANGENT_UP_VECTOR
		lowp vec3 TangentUpVector = OPTIONAL_NORMALIZE( VarTangentUpVector );
	#endif

	#if NEED_MASK_TEXTURE
		lowp vec4 MaskTextureColor = texture2D( TextureMask, UVMask );
	#endif
	
	#if NEED_DIRECTIONAL_LIGHT_UNIFORMS
		lowp vec3 WorldLightDirection = LightDirectionAndbDirectional.xyz;
	#endif
	
	#if NEED_PIXEL_TANGENT_CAMERA_VECTOR
		lowp vec3 TangentCameraVector = OPTIONAL_NORMALIZE( VarTangentCameraVector );    
	#endif
	
	#if USE_BUMP_OFFSET
	{
		// Bump offset height is stored in 'mask' texture red channel
		lowp float PixelHeight = MaskTextureColor.r;
		vec2 BumpUVOffset = TangentCameraVector.xy * ( BumpHeightRatio * PixelHeight + PreMultipliedBumpReferencePlane );

		FinalBaseUV += BumpUVOffset;
	}
	#endif
	

	lowp vec4 BaseColor = texture2D( TextureBase, FinalBaseUV, DEFAULT_LOD_BIAS );
	
	// Alpha test
	ALPHAKILL( BaseColor.w )
	

	lowp vec4 PolyColor = BaseColor;


	#if USE_COLOR_TEXTURE_BLENDING
	{
		#if TEXTURE_BLEND_FACTOR_SOURCE == TEXTURE_BLEND_FACTOR_SOURCE_MASK_TEXTURE
			// Texture blend factor is stored in 'mask' texture alpha channel
			lowp float TextureBlendFactor = MaskTextureColor.a;
		#endif
		
		// Sample the extra diffuse texture
		lowp vec4 DetailColor = texture2D( TextureDetail, UVDetail, DEFAULT_LOD_BIAS );

		// Blend the base color with the detail color using the blend factor        
		PolyColor = mix( PolyColor, DetailColor, TextureBlendFactor );
	}
	#endif
	
	
	// Compute tangent to world matrix per pixel
	#if NEED_PIXEL_TANGENT_TO_WORLD
	lowp mat3 TangentToWorld;
	{
		#if COMPUTE_BINORMAL_IN_PIXEL_SHADER
			// Compute the vertex binormal using the normal and tangent vectors
			lowp vec3 WorldBinormal = cross( VarWorldNormal, VarWorldTangent );
		#else
			lowp vec3 WorldBinormal = VarWorldBinormal;
		#endif
		TangentToWorld = mat3( VarWorldTangent, WorldBinormal, VarWorldNormal );
	}
	#endif
	
	
	#if NEED_NORMAL_TEXTURE
	lowp vec4 NormalColor;
	{
		highp vec2 NormalTexCoord = FinalBaseUV;
		NormalColor = texture2D( TextureNormal, NormalTexCoord, DEFAULT_LOD_BIAS );
	}
	#endif
	

	// Normal mapping
	#if USE_NORMAL_MAPPING
	lowp vec3 PerPixelNormal;
	{
		PerPixelNormal = ( NormalColor.xyz * 2.0 ) - 1.0;
	}
	#endif
	
	
	#if USE_NORMAL_MAPPING && !PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
	lowp vec3 PerPixelWorldNormal;
	{
		PerPixelWorldNormal = OPTIONAL_NORMALIZE( TangentToWorld * PerPixelNormal );
	}
	#endif
	
	
	#if NEED_PIXEL_WORLD_POSITION
	lowp vec3 WorldSpaceEyeToVertexDirection;
	{
		// NOTE: We're relying on the compiler to optimize these away if they're not needed
		mediump vec3 EyeToVertexVector = CameraWorldPosition - VarWorldPosition.xyz;
		WorldSpaceEyeToVertexDirection = normalize( EyeToVertexVector );
	}
	#endif


	#if USE_EMISSIVE
	#if EMISSIVE_COLOR_SOURCE == EMISSIVE_COLOR_SOURCE_EMISSIVE_TEXTURE
	lowp vec4 EmissiveTextureColor;
	{
		highp vec2 EmissiveTexCoord = FinalBaseUV;
		EmissiveTextureColor = texture2D( TextureEmissive, EmissiveTexCoord, DEFAULT_LOD_BIAS );
	}
	#endif
	#endif
	

	// Pre-declare specular amount variable if we need it.  We'll compute specular early but won't apply
	// it until near the very end of the shader
	#if USE_PIXEL_SPECULAR
		lowp vec3 Specular;
	#endif

	
	// Per-pixel environment reflection vector and rim term/fresnel
	#if NEED_PER_PIXEL_RIM_TERM
	lowp float PerPixelRimTerm;
	lowp vec3 PerPixelEnvironmentVector;
	{
		lowp vec3 EnvNormal;
		lowp vec3 EnvCameraVector;
		#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
			EnvNormal = PerPixelNormal;
			EnvCameraVector = TangentCameraVector;
		#else
			EnvNormal = PerPixelWorldNormal;
			EnvCameraVector = WorldSpaceEyeToVertexDirection;
		#endif
		

		// Compute environment reflection per pixel
		#if USE_REFLECTION_BASED_ENVIRONMENT_MAPS
			// Reflection-based
			PerPixelEnvironmentVector = reflect( EnvCameraVector, EnvNormal );
		#else
			// View direction-based
			PerPixelEnvironmentVector = EnvCameraVector;
		#endif
	   
		// NOTE: We're relying on the compiler's dead code removal to eliminate this when not needed
		lowp float NDotE = dot( EnvCameraVector, EnvNormal );
		PerPixelRimTerm = max( 0.01, 1.0 - abs( NDotE ) );
	}
	#endif
	
	
	// Environment mapping
	#if USE_ENVIRONMENT_MAPPING
	{
		lowp float EnvironmentAmount;
		lowp vec3 EnvironmentVector;
		
		#if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_ENVIRONMENT_MAPPING
		{
			EnvironmentAmount = EnvironmentParameters.x;
			EnvironmentVector = PerPixelEnvironmentVector;
		   
			// Apply fresnel
			#if USE_ENVIRONMENT_FRESNEL
			{
				lowp float FresnelAmount = EnvironmentParameters.y;
				lowp float FresnelExponent = EnvironmentParameters.z;
				
				lowp float FresnelTerm = pow( PerPixelRimTerm, FresnelExponent );
				lowp float ScaledFresnelTerm = ( 1.0 - FresnelAmount ) + FresnelAmount * FresnelTerm;
				
				EnvironmentAmount *= ScaledFresnelTerm;
			}
			#endif

			#if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_RED || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_GREEN || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_BLUE || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_ALPHA
				EnvironmentAmount *= EnvironmentMask;
			#endif
		}    
		#else
		{
			// Environment reflection computed per vertex, interpolated per pixel
			// NOTE: For reflection-based env maps, using a normalize is pretty important
			//     as the direction is already reflected by a per-vertex normal and every bit
			//     of accuracy we have left is crucial
			#if USE_REFLECTION_BASED_ENVIRONMENT_MAPS
				EnvironmentVector = normalize( EnvironmentVectorAndAmount.xyz );
			#else
				EnvironmentVector = OPTIONAL_NORMALIZE( EnvironmentVectorAndAmount.xyz );
			#endif
			EnvironmentAmount = EnvironmentVectorAndAmount.w;
		}    
		#endif
		
		
		// Sample spherical environment map texture
		lowp vec2 EnvironmentTexCoord = EnvironmentVector.xy * 0.5 + 0.5;
		lowp vec4 EnvironmentColor = texture2D( TextureEnvironment, EnvironmentTexCoord );
		
		
		// Apply masking!
		{
			#if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_BASE_TEXTURE_RED
				EnvironmentAmount *= BaseColor.r;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_BASE_TEXTURE_GREEN
				EnvironmentAmount *= BaseColor.g;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_BASE_TEXTURE_BLUE
				EnvironmentAmount *= BaseColor.b;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_BASE_TEXTURE_ALPHA
				EnvironmentAmount *= BaseColor.a;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_RED
				EnvironmentAmount *= MaskTextureColor.r;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_GREEN
				EnvironmentAmount *= MaskTextureColor.g;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_BLUE
				EnvironmentAmount *= MaskTextureColor.b;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_ALPHA
				EnvironmentAmount *= MaskTextureColor.a;
			#elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_NORMAL_TEXTURE_ALPHA
				EnvironmentAmount *= NormalColor.a;
			#endif
		}        

		
		// Apply environment color scaling
		lowp vec3 FinalEnvironmentColor = EnvironmentColor.rgb * EnvironmentColorScale;
		
		#if ENVIRONMENT_BLEND_MODE == ENVIRONMENT_BLEND_ADD
		
			// Add environment map contribution to base color.  This looks more like a mirror-reflection
			// or specular highlight.
			PolyColor.rgb += FinalEnvironmentColor * EnvironmentAmount;
			
		#elif ENVIRONMENT_BLEND_MODE == ENVIRONMENT_BLEND_LERP
		
			// Lerp between base texture color and environment map.  This looks like a metallic effect.
			PolyColor.rgb = mix( PolyColor.rgb, FinalEnvironmentColor, EnvironmentAmount );
			
		#endif
	}
	#endif
	

	#define SHOULD_APPLY_DIFFUSE_LIGHT 0
	lowp vec3 TotalDiffuseLight = vec3( 0.0, 0.0, 0.0 );


	// Per-pixel direct lighting
	#if USE_NORMAL_MAPPING && USE_DYNAMIC_DIRECTIONAL_LIGHT
	{
		#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
			lowp vec3 LightDirection = TangentLightDirection;
			lowp vec3 SurfaceNormal = PerPixelNormal;
		#else
			lowp vec3 LightDirection = WorldLightDirection;
			lowp vec3 SurfaceNormal = PerPixelWorldNormal;
		#endif
		
		// calculate how much the directional light affects this fragment
		lowp vec3 LightColor = LightColorAndFalloffExponent.xyz * 
			max( 0.0, dot( LightDirection, SurfaceNormal ) );
			
		TotalDiffuseLight += LightColor;
		
		#undef SHOULD_APPLY_DIFFUSE_LIGHT
		#define SHOULD_APPLY_DIFFUSE_LIGHT 1
	}
	#endif


	// Per-pixel sky lighting
	#if USE_NORMAL_MAPPING && USE_DYNAMIC_SKY_LIGHT && ALLOW_PER_PIXEL_DYNAMIC_SKY_LIGHT
	{
		lowp float NormalContribution;
		#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
			NormalContribution = dot( PerPixelNormal, TangentUpVector ) * 0.5 + 0.5;
		#else
			// calculate how much of the sky colors to use (normal.z, -1 = all lower, 1 = all upper, 0 = 50/50)
			NormalContribution = PerPixelWorldNormal.z * 0.5 + 0.5;
		#endif
		
		#if USE_FASTER_SKY_LIGHT
			// NOTE: 0.5 is to create results more similar to when the squared falloff is applied (below)
			lowp vec3 SkyColor = 0.5 * mix( LowerSkyColor.xyz, UpperSkyColor.xyz, NormalContribution );
		#else
			// Apply square sky light intensity falloff based on surface angle relative to either hemisphere
			lowp vec2 ContributionWeightsSqrt = vec2(0.5, 0.5) + vec2(0.5, -0.5) * NormalContribution;
			lowp vec2 ContributionWeights = ContributionWeightsSqrt * ContributionWeightsSqrt;
			lowp vec3 UpperLighting = UpperSkyColor.rgb * ContributionWeights.x;
			lowp vec3 LowerLighting = LowerSkyColor.rgb * ContributionWeights.y;
			lowp vec3 SkyColor = UpperLighting + LowerLighting;
		#endif

		#if USE_AMBIENT_OCCLUSION
			// Also apply AO from the vertex color to the sky light
			SkyColor *= AmbientOcclusion;
		#endif
		
		TotalDiffuseLight += SkyColor;

		#undef SHOULD_APPLY_DIFFUSE_LIGHT
		#define SHOULD_APPLY_DIFFUSE_LIGHT 1
	}
	#endif


	// Apply per-vertex lighting (direct, sky, unmasked rim) passed down from the vertex shader
	#if NEED_LIGHTING_COLOR
	{
		// Use diffuse/sky lighting computed in vertex shader
		TotalDiffuseLight += LightingColor;
		
		#undef SHOULD_APPLY_DIFFUSE_LIGHT
		#define SHOULD_APPLY_DIFFUSE_LIGHT 1
	}
	#endif



	#if SHOULD_APPLY_DIFFUSE_LIGHT
		PolyColor.rgb *= TotalDiffuseLight;
	#endif
	
	
	// Per-pixel specular
	#if USE_PIXEL_SPECULAR
	{
		#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
			// NOTE: With tangent space lighting, we only support half-vector specular currently
			// Half-vector specular (faster, big highlight, less realistic)
			lowp float BaseSpec = max( 0.0, dot( PerPixelNormal, TangentHalfVector ) );  // NDotH
		#else
			#if USE_HALF_VECTOR_SPECULAR_MODEL
				// Half-vector specular (faster, big highlight, less realistic)
				lowp vec3 WorldHalfVector = normalize( WorldSpaceEyeToVertexDirection + WorldLightDirection );
				lowp float BaseSpec = max( 0.0, dot( PerPixelWorldNormal, WorldHalfVector ) );  // NDotH
			#elif 1
				// Reflection-based specular (slower, higher quality)
				lowp vec3 WorldReflectionVector = reflect( WorldLightDirection, PerPixelWorldNormal );
				lowp float BaseSpec = max( 0.0, dot( WorldSpaceEyeToVertexDirection, WorldReflectionVector ) );   // VDotR
			#else
				// Diffuse lighting (for debugging only)
				lowp float BaseSpec = max( 0.0, dot( WorldLightDirection, PerPixelWorldNormal ) );  // NDotL
			#endif
		#endif
		

		#if USE_PIXEL_FIXED_SPECULAR_POWER_APPROXIMATION        
			// Fast specular approximation (http://www.gamasutra.com/view/feature/2972/a_noninteger_power_function_on_.php)
			// Basically pow( N, P ) can be approximated by pow( max( A * N + B ), M )
			//      - A and B are constants that must be tweaked to get artifact-free results
			//      - M can be really small in practice (2 looks good, can be replaced by single multiply)
			// This should result in a mad_sat instruction plus one multiply (2 instructions total!)
			
			// N = 18
			#define SpecApproxA 6.645
			#define SpecApproxB -5.645   
			lowp float SpecularAmount = clamp( SpecApproxA * BaseSpec + SpecApproxB, 0.0, 1.0 );
			SpecularAmount *= SpecularAmount;   // M = 2
		#else
			// Power function for specular curve
			lowp float SpecularAmount = pow( BaseSpec, SpecularPower );
		#endif        


		#if 0
			// Apply specular shadowing by multiplying with diffuse coefficient
			#if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
				lowp float ShadowTerm = max( 0.0, dot( TangentLightDirection, SurfaceNormal ) );
			#else
				lowp float ShadowTerm = max( 0.0, dot( WorldLightDirection, SurfaceNormal ) );
			#endif
			SpecularAmount *= ShadowTerm;
		#endif

	   
		Specular = SpecularAmount * LightColorTimesSpecularColor;
	}
	#endif

	
	// Add specular
	lowp vec3 PreSpecularPolyColor = PolyColor.rgb;
	#if USE_SPECULAR
	{
		// Note that we add specular to the base color (pre-light map multiply) so that shadows in light map
		// will affect the specular highlight
		
		// Scale specular amount by specific diffuse channel before adding to output color
		#if SPECULAR_MASK == SPECMASK_DIFFUSE_RED
			PolyColor.rgb += Specular * BaseColor.r;
		#elif SPECULAR_MASK == SPECMASK_DIFFUSE_GREEN
			PolyColor.rgb += Specular * BaseColor.g;
		#elif SPECULAR_MASK == SPECMASK_DIFFUSE_BLUE
			PolyColor.rgb += Specular * BaseColor.b;
		#elif SPECULAR_MASK == SPECMASK_DIFFUSE_ALPHA
			PolyColor.rgb += Specular * BaseColor.a;
		#elif SPECULAR_MASK == SPECMASK_LUMINANCE

			// Generate a fake spec mask using diffuse luminance
			vec3 LumConstants = vec3( .2126, 0.7152, 0.0722);	
			mediump float AvgLum = dot(LumConstants, BaseColor.rgb);
			PolyColor.rgb += Specular * AvgLum;
			
		#elif SPECULAR_MASK == SPECMASK_MASK_TEXTURE_RGB
			PolyColor.rgb += Specular * MaskTextureColor.rgb;
		#else   // SPECMASK_CONSTANT

			// Just add specular with no mask
			PolyColor.rgb += Specular;
			
		#endif
	}
	#endif


	// Texture light map
	#if USE_LIGHTMAP
	{
		lowp vec3 LightmapColor = texture2D( TextureLightmap, UVLightmap ).rgb;
		#if !LIGHTMAP_STORED_IN_LINEAR_SPACE
			// @todo: Remove the pow for now to remain consistent with previous behavior
			// @todo: Need to determine the right way to handle gamma in general on mobile devices
			// LightmapColor = pow( LightmapColor, vec4( 2.2 ) );
		#endif

		#if USE_LIGHTMAP_FIXED_SCALE
			PolyColor.rgb = PolyColor.rgb * LightmapColor;
			//values must be 1,2, or 4
			#if LIGHTMAP_FIXED_SCALE_VALUE == 2
				PolyColor.rgb += PolyColor.rgb;
			#elif LIGHTMAP_FIXED_SCALE_VALUE == 4
				PolyColor.rgb *= LIGHTMAP_FIXED_SCALE_VALUE;
			#endif
		#else
			PolyColor.rgb *= LightmapColor * LightMapScale.rgb;
		#endif
	}
	#endif


	// Vertex light map
	#if USE_VERTEX_LIGHTMAP
	{
		PolyColor.rgb *= PrelitColor;
	}
	#endif
	
	
	#if USE_RIM_LIGHTING
	{
		#define SHOULD_APPLY_RIM_LIGHTING 1
		lowp vec3 RimLighting;

		#if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_RIM_LIGHTING
		{
			// Note: The 'strength' of the rim light is pre-multiplied with the color
			lowp vec3 RimLightingColor = RimLightingColorAndExponent.xyz;
			lowp float RimLightingExponent = RimLightingColorAndExponent.w;
			
			RimLighting = RimLightingColor * pow( PerPixelRimTerm, RimLightingExponent );

			// Apply vertex color rim light mask if we need to
			#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
				RimLighting *= RimLightingMask;
			#endif
		}
		#else
		{
			// Apply masked per-vertex rim lighting, if enabled.  Note that if the rim lighting is unmasked,
			// the rim term will be added to the LightingColor value already instead of applied here
			#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_CONSTANT || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
				// Rim lighting was already applied to LightingColor in vertex shader, no need to do anything else
				#undef SHOULD_APPLY_RIM_LIGHTING
				#define SHOULD_APPLY_RIM_LIGHTING 0
			#else
				RimLighting = MaskableRimLighting;
			#endif  
		}
		#endif

		   
		// Apply masking!
		{
			#if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_BASE_TEXTURE_RED
				RimLighting *= BaseColor.r;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_BASE_TEXTURE_GREEN
				RimLighting *= BaseColor.g;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_BASE_TEXTURE_BLUE
				RimLighting *= BaseColor.b;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_BASE_TEXTURE_ALPHA
				RimLighting *= BaseColor.a;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_RED
				RimLighting *= MaskTextureColor.r;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_GREEN
				RimLighting *= MaskTextureColor.g;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_BLUE
				RimLighting *= MaskTextureColor.b;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_ALPHA
				RimLighting *= MaskTextureColor.a;
			#elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_NORMAL_TEXTURE_ALPHA
				RimLighting *= NormalColor.a;
			#endif
		}

		#if SHOULD_APPLY_RIM_LIGHTING
			// NOTE: Rim lighting is applied after light maps as we don't want the rim light to be
			// 		 shadowed by precomputed lighting
			PolyColor.rgb += RimLighting * PreSpecularPolyColor;
		#endif
	}
	#endif
	
	
	#if USE_EMISSIVE
	{
		lowp vec3 Emissive;
		
		
		// Grab the emissive color
		#if EMISSIVE_COLOR_SOURCE == EMISSIVE_COLOR_SOURCE_EMISSIVE_TEXTURE
		{
			Emissive = EmissiveTextureColor.rgb;
		}
		#elif EMISSIVE_COLOR_SOURCE == EMISSIVE_COLOR_SOURCE_BASE_TEXTURE
		{
			Emissive = BaseColor.rgb;
		}
		#elif EMISSIVE_COLOR_SOURCE == EMISSIVE_COLOR_SOURCE_CONSTANT
		{
			Emissive = ConstantEmissiveColor;
		}
		#endif
		

		// Apply emissive masking
		#if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_BASE_TEXTURE_RED
			Emissive *= BaseColor.r;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_BASE_TEXTURE_GREEN
			Emissive *= BaseColor.g;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_BASE_TEXTURE_BLUE
			Emissive *= BaseColor.b;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_BASE_TEXTURE_ALPHA
			Emissive *= BaseColor.a;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_RED
			Emissive *= MaskTextureColor.r;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_GREEN
			Emissive *= MaskTextureColor.g;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_BLUE
			Emissive *= MaskTextureColor.b;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_ALPHA
			Emissive *= MaskTextureColor.a;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_NORMAL_TEXTURE_ALPHA
			Emissive *= NormalColor.a;
		#elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_RED || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_GREEN || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_BLUE || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_ALPHA
			Emissive *= EmissiveMask;
		#endif
		

		// Add the emissve color to our pixel's color
		PolyColor.rgb += Emissive;
	}
	#endif


	// Color Multiply
	#if USE_UNIFORM_COLOR_MULTIPLY || USE_VERTEX_COLOR_MULTIPLY
	{
		PolyColor.rgba *= ColorMultiply;
	}
	#endif

	#if USE_GRADIENT_FOG
	{
		PolyColor.xyz = mix( PolyColor.xyz, FogColorAndAmount.xyz, FogColorAndAmount.w );
	}
	#endif

	// Final, constant color fade/blend, which is used to embody various global color changes (i.e. full screen effects, fog)
	PolyColor.xyz = mix( PolyColor.xyz, FadeColorAndAmount.xyz, FadeColorAndAmount.w );

	gl_FragColor = PolyColor;
	
	
	// Draw the debug color if we were asked to do that (for debugging purposes only!)
	#ifdef SHOW_DEBUG_COLOR
		gl_FragColor.rgb = DebugColor;	
	#endif
#endif //#if !DEPTH_ONLY
}


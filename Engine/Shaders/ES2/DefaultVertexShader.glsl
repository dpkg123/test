/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

uniform mat4 LocalToWorld;
uniform mat3 LocalToWorldRotation;
uniform mat4 ViewProjection;
uniform mat4 LocalToProjection;

attribute vec4 Position;


VARYING_HIGH vec2 UVBase;


#if USE_GPU_SKINNING
    uniform vec4 BoneMatrices[MAX_BONES * 3];
    
    // Depth only rendering forces one bone skinning
    #if !DEPTH_ONLY
		attribute vec4 BlendWeight;
	#endif
    attribute vec4 BlendIndices;
#endif

#if USE_VERTEX_MOVEMENT
	//time for tangent sine wave, max amplitude of the movement, time for vertical sine wave
	uniform vec3 VertexMovementConstants;
	// Sway transform matrix
	uniform mat4 VertexSwayMatrix;
#endif

#if USE_LIGHTMAP
    uniform vec4 LightmapCoordinateScaleBias;
    attribute vec2 LightMapCoordinate;
    VARYING_HIGH vec2 UVLightmap;
#endif


#if USE_VERTEX_LIGHTMAP
    #if !USE_LIGHTMAP_FIXED_SCALE
        uniform vec4 LightMapScale;
    #endif
    attribute vec4 LightMapA;
    VARYING_LOW vec3 PrelitColor;
#endif



#define NEED_VERTEX_TEXCOORDS0 0
#define NEED_VERTEX_TEXCOORDS1 0
#define NEED_VERTEX_TEXCOORDS2 0
#define NEED_VERTEX_TEXCOORDS3 0
#define NEED_NORMAL 0
#define NEED_WORLD_NORMAL 0
#define NEED_WORLD_TANGENT 0
#define NEED_LIGHTING_COLOR 0
#define NEED_TANGENT_SPACE 0
#define NEED_TANGENT_TO_WORLD 0
#define NEED_PIXEL_WORLD_POSITION 0
#define NEED_PIXEL_TANGENT_TO_WORLD 0
#define NEED_PIXEL_TANGENT_HALF_VECTOR 0
#define NEED_PIXEL_TANGENT_LIGHT_DIRECTION 0
#define NEED_PIXEL_TANGENT_UP_VECTOR 0
#define NEED_PIXEL_TANGENT_CAMERA_VECTOR 0
#define NEED_MASK_TEXTURE 0
#define NEED_EXTRA_COLOR_TEXTURE 0
#define NEED_VERTEX_COLOR 0
#define NEED_RIM_LIGHTING_COLOR_VARYING 0
#define NEED_DIRECTIONAL_LIGHT_UNIFORMS 0
#define NEED_PER_VERTEX_RIM_TERM 0
#define NEED_INTERMEDIATE_WORLD_POSITION 0


#if IS_DECAL
	// Transform local/world point to projected space for decal
	uniform mat4 DecalMatrix;

	// Local/world position of the decal
	uniform vec3 DecalLocation;

	// Offset of decal in texture space
	uniform vec2 DecalOffset;
#endif


// Currently, all primitives always have at least one set of texture coordinates
// NOTE: The exception is decal primitives, which auto-generated texture coordinates, but that case
//   is handled later on in this file
#undef NEED_VERTEX_TEXCOORDS0
#define NEED_VERTEX_TEXCOORDS0 1


#if USE_GRADIENT_FOG
	// Define hardcoded fog inputs and an output to send to pixel shader
	VARYING_LOW vec4 FogColorAndAmount;
	uniform float FogOneOverSquaredRange;
	uniform float FogStartSquared;
	uniform vec4 FogColor; 
	
	#undef NEED_INTERMEDIATE_WORLD_POSITION
	#define NEED_INTERMEDIATE_WORLD_POSITION 1
#endif

#if USE_VERTEX_MOVEMENT
    #undef NEED_VERTEX_COLOR
    #define NEED_VERTEX_COLOR 1
    
    #undef NEED_INTERMEDIATE_WORLD_POSITION
    #define NEED_INTERMEDIATE_WORLD_POSITION 1
#endif

#if USE_COLOR_TEXTURE_BLENDING
    #if TEXTURE_BLEND_FACTOR_SOURCE == TEXTURE_BLEND_FACTOR_SOURCE_VERTEX_COLOR
        #undef NEED_VERTEX_COLOR
        #define NEED_VERTEX_COLOR 1

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


#if USE_AMBIENT_OCCLUSION
    VARYING_LOW float AmbientOcclusion;
    #undef NEED_VERTEX_COLOR
    #define NEED_VERTEX_COLOR 1
#endif


#if USE_ENVIRONMENT_MAPPING
    #if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_ENVIRONMENT_MAPPING
        #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
            #undef NEED_PIXEL_TANGENT_CAMERA_VECTOR
            #define NEED_PIXEL_TANGENT_CAMERA_VECTOR 1
        #else
            #undef NEED_PIXEL_WORLD_POSITION 
            #define NEED_PIXEL_WORLD_POSITION 1
        #endif

        #if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_RED || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_GREEN || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_BLUE || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_ALPHA
            VARYING_LOW float EnvironmentMask;
        #endif
    #else
        uniform vec3 EnvironmentParameters;
        VARYING_LOW vec4 EnvironmentVectorAndAmount;

        #undef NEED_PER_VERTEX_RIM_TERM
        #define NEED_PER_VERTEX_RIM_TERM 1
    #endif

    #if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_RED || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_GREEN || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_BLUE || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_MASK_TEXTURE_ALPHA
        #undef NEED_MASK_TEXTURE
        #define NEED_MASK_TEXTURE 1
    #endif
#endif


#if USE_EMISSIVE
    #if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_RED || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_GREEN || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_BLUE || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_ALPHA
        VARYING_LOW float EmissiveMask;
    #endif

    #if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_RED || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_GREEN || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_BLUE || EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_MASK_TEXTURE_ALPHA
        #undef NEED_MASK_TEXTURE
        #define NEED_MASK_TEXTURE 1
    #endif
#endif



#if USE_RIM_LIGHTING
    #if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_RIM_LIGHTING
        #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
            #undef NEED_PIXEL_TANGENT_CAMERA_VECTOR
            #define NEED_PIXEL_TANGENT_CAMERA_VECTOR 1
        #else
            #undef NEED_PIXEL_WORLD_POSITION 
            #define NEED_PIXEL_WORLD_POSITION 1
        #endif

        #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
            VARYING_LOW float RimLightingMask;
        #endif
    #else
        uniform vec4 RimLightingColorAndExponent;

        #undef NEED_PER_VERTEX_RIM_TERM
        #define NEED_PER_VERTEX_RIM_TERM 1
        
        #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_CONSTANT || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
            #undef NEED_LIGHTING_COLOR
            #define NEED_LIGHTING_COLOR 1
        #else    
            VARYING_LOW vec3 MaskableRimLighting;
        #endif
    #endif
    
    #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
        #undef NEED_VERTEX_COLOR
        #define NEED_VERTEX_COLOR 1
    #endif
    
    #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_MASK_TEXTURE_ALPHA
        #undef NEED_MASK_TEXTURE
        #define NEED_MASK_TEXTURE 1
    #endif
#endif



#if USE_BUMP_OFFSET
    #undef NEED_PIXEL_TANGENT_CAMERA_VECTOR
    #define NEED_PIXEL_TANGENT_CAMERA_VECTOR 1

    #undef NEED_MASK_TEXTURE
    #define NEED_MASK_TEXTURE 1
#endif


#if BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS0
	#undef NEED_VERTEX_TEXCOORDS0
	#define NEED_VERTEX_TEXCOORDS0 1
#elif BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS1
	#undef NEED_VERTEX_TEXCOORDS1
	#define NEED_VERTEX_TEXCOORDS1 1
#elif BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS2
	#undef NEED_VERTEX_TEXCOORDS2
	#define NEED_VERTEX_TEXCOORDS2 1
#elif BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS3
	#undef NEED_VERTEX_TEXCOORDS3
	#define NEED_VERTEX_TEXCOORDS3 1
#endif


#if NEED_EXTRA_COLOR_TEXTURE
    #if DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS0
        #undef NEED_VERTEX_TEXCOORDS0
        #define NEED_VERTEX_TEXCOORDS0 1
    #elif DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS1
        #undef NEED_VERTEX_TEXCOORDS1
        #define NEED_VERTEX_TEXCOORDS1 1
    #elif DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS2
        #undef NEED_VERTEX_TEXCOORDS2
        #define NEED_VERTEX_TEXCOORDS2 1
    #elif DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS3
        #undef NEED_VERTEX_TEXCOORDS3
        #define NEED_VERTEX_TEXCOORDS3 1
    #endif


    // @todo: We could opt to only include this when UVTransform features are enabled (UVs are actually different)
    VARYING_HIGH vec2 UVDetail;
#endif


#if NEED_MASK_TEXTURE
    #if MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS0
        #undef NEED_VERTEX_TEXCOORDS0
        #define NEED_VERTEX_TEXCOORDS0 1
    #elif MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS1
        #undef NEED_VERTEX_TEXCOORDS1
        #define NEED_VERTEX_TEXCOORDS1 1
    #elif MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS2
        #undef NEED_VERTEX_TEXCOORDS2
        #define NEED_VERTEX_TEXCOORDS2 1
    #elif MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS3
        #undef NEED_VERTEX_TEXCOORDS3
        #define NEED_VERTEX_TEXCOORDS3 1
    #endif
#endif


#if USE_VERTEX_SPECULAR
    #undef NEED_WORLD_NORMAL
    #define NEED_WORLD_NORMAL 1

    uniform float SpecularPower;
    uniform vec3 LightColorTimesSpecularColor;

    VARYING_LOW vec3 Specular;

    #undef NEED_DIRECTIONAL_LIGHT_UNIFORMS
    #define NEED_DIRECTIONAL_LIGHT_UNIFORMS 1
    
    #undef NEED_INTERMEDIATE_WORLD_POSITION
    #define NEED_INTERMEDIATE_WORLD_POSITION 1
#endif


#if USE_NORMAL_MAPPING
    #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
        #undef NEED_TANGENT_TO_WORLD
        #define NEED_TANGENT_TO_WORLD 1
    #else
        #undef NEED_PIXEL_TANGENT_TO_WORLD
        #define NEED_PIXEL_TANGENT_TO_WORLD 1
    #endif
#endif


#if USE_PIXEL_SPECULAR
    #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
        #undef NEED_PIXEL_TANGENT_HALF_VECTOR
        #define NEED_PIXEL_TANGENT_HALF_VECTOR 1

        #undef NEED_PIXEL_TANGENT_LIGHT_DIRECTION
        #define NEED_PIXEL_TANGENT_LIGHT_DIRECTION 1
        
        #undef NEED_DIRECTIONAL_LIGHT_UNIFORMS
        #define NEED_DIRECTIONAL_LIGHT_UNIFORMS 1
    #else
        #undef NEED_PIXEL_TANGENT_TO_WORLD
        #define NEED_PIXEL_TANGENT_TO_WORLD 1
        
        #undef NEED_PIXEL_WORLD_POSITION 
        #define NEED_PIXEL_WORLD_POSITION 1
    #endif
    
    #undef NEED_INTERMEDIATE_WORLD_POSITION
    #define NEED_INTERMEDIATE_WORLD_POSITION 1
#endif


#if USE_DYNAMIC_DIRECTIONAL_LIGHT
    #if USE_NORMAL_MAPPING
        #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
            #undef NEED_PIXEL_TANGENT_LIGHT_DIRECTION
            #define NEED_PIXEL_TANGENT_LIGHT_DIRECTION 1
        #endif
    #else
        uniform vec4 LightColorAndFalloffExponent;
        
        #undef NEED_LIGHTING_COLOR
        #define NEED_LIGHTING_COLOR 1
    #endif

    #undef NEED_DIRECTIONAL_LIGHT_UNIFORMS
    #define NEED_DIRECTIONAL_LIGHT_UNIFORMS 1

    #undef NEED_WORLD_NORMAL
    #define NEED_WORLD_NORMAL 1
    
#endif


#if USE_DYNAMIC_SKY_LIGHT
    #if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_DYNAMIC_SKY_LIGHT
        #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
            #undef NEED_PIXEL_TANGENT_UP_VECTOR
            #define NEED_PIXEL_TANGENT_UP_VECTOR 1
        #endif
    #else
        // @todo: Currently these parameters will only be set for actors with light environments!!
        uniform vec4 LowerSkyColor;
        uniform vec4 UpperSkyColor;
        
        #undef NEED_LIGHTING_COLOR
        #define NEED_LIGHTING_COLOR 1
    #endif
    
    #undef NEED_WORLD_NORMAL
    #define NEED_WORLD_NORMAL 1
#endif


#if USE_VERTEX_COLOR_MULTIPLY
    #undef NEED_VERTEX_COLOR
    #define NEED_VERTEX_COLOR 1
#endif


#if NEED_LIGHTING_COLOR
    VARYING_LOW vec3 LightingColor;
#endif


#if NEED_DIRECTIONAL_LIGHT_UNIFORMS
    // @todo: This is a little weird because for actors with light environments, this direction will be
    //   driven by the light environment direction, otherwise we use the brightest directional light
    //   in the scene.  However, for specular the color always comes from the brightest directional light.
    uniform vec4 LightDirectionAndbDirectional;
#endif


#if NEED_PIXEL_TANGENT_CAMERA_VECTOR
    VARYING_LOW vec3 VarTangentCameraVector;
    
    #undef NEED_TANGENT_TO_WORLD
    #define NEED_TANGENT_TO_WORLD 1
    
    #undef NEED_INTERMEDIATE_WORLD_POSITION
    #define NEED_INTERMEDIATE_WORLD_POSITION 1
#endif


#if NEED_PIXEL_TANGENT_TO_WORLD
    #undef NEED_TANGENT_TO_WORLD
    #define NEED_TANGENT_TO_WORLD 1

    VARYING_LOW vec3 VarWorldNormal;
    VARYING_LOW vec3 VarWorldTangent;
    #if !COMPUTE_BINORMAL_IN_PIXEL_SHADER
        VARYING_LOW vec3 VarWorldBinormal;
    #endif
#endif


#if NEED_PIXEL_WORLD_POSITION
    VARYING_MEDIUM vec3 VarWorldPosition;
    
    #undef NEED_INTERMEDIATE_WORLD_POSITION
    #define NEED_INTERMEDIATE_WORLD_POSITION 1
#endif

#if NEED_PIXEL_TANGENT_HALF_VECTOR
    #undef NEED_TANGENT_TO_WORLD
    #define NEED_TANGENT_TO_WORLD 1

    VARYING_LOW vec3 VarTangentHalfVector;
#endif

#if NEED_PIXEL_TANGENT_LIGHT_DIRECTION
    #undef NEED_TANGENT_TO_WORLD
    #define NEED_TANGENT_TO_WORLD 1

    VARYING_LOW vec3 VarTangentLightDirection;
#endif


#if NEED_PIXEL_TANGENT_UP_VECTOR
    VARYING_LOW vec3 VarTangentUpVector;
#endif


#if NEED_TANGENT_TO_WORLD
    #undef NEED_TANGENT_SPACE
    #define NEED_TANGENT_SPACE 1
    
    #undef NEED_WORLD_NORMAL
    #define NEED_WORLD_NORMAL 1
    
    #undef NEED_WORLD_TANGENT
    #define NEED_WORLD_TANGENT 1
#endif


// Decals always auto-generate texture coordinates, so we don't declare attributes in that case
#if !IS_DECAL
	#if NEED_VERTEX_TEXCOORDS0
		attribute vec2 TexCoords0;
	#endif

	#if NEED_VERTEX_TEXCOORDS1
		attribute vec2 TexCoords1;
	#endif

	#if NEED_VERTEX_TEXCOORDS2
		attribute vec2 TexCoords2;
	#endif

	#if NEED_VERTEX_TEXCOORDS3
		attribute vec2 TexCoords3;
	#endif
#endif

#if NEED_VERTEX_COLOR
	// Note: Red and Blue are reversed here due to platform-specific byte order
	attribute vec4 VertexColor;
#endif

#if USE_UNIFORM_COLOR_MULTIPLY
	uniform vec4 UniformMultiplyColor;
#endif

//whether to bring in per vertex and/or uniform color
#if USE_UNIFORM_COLOR_MULTIPLY || USE_VERTEX_COLOR_MULTIPLY
	VARYING_LOW vec4 ColorMultiply;
#endif


#if NEED_PER_VERTEX_RIM_TERM
    #if USE_REFLECTION_BASED_ENVIRONMENT_MAPS
        #undef NEED_WORLD_NORMAL
        #define NEED_WORLD_NORMAL 1
    #endif
    
    #undef NEED_INTERMEDIATE_WORLD_POSITION
    #define NEED_INTERMEDIATE_WORLD_POSITION 1

#endif


#if NEED_MASK_TEXTURE
    VARYING_HIGH vec2 UVMask;
#endif


#if NEED_TANGENT_SPACE
    #undef NEED_NORMAL
    #define NEED_NORMAL 1
    attribute vec3 TangentX;
#endif


#if NEED_WORLD_NORMAL
    #undef NEED_NORMAL
    #define NEED_NORMAL 1
#endif


#if NEED_NORMAL
    // TangentZ.w contains sign of tangent basis determinant
    attribute vec4 TangentZ;
#endif


void main()
{
    vec4 WorldPosition;

#if NEED_NORMAL
    // Grab unit vertex normal by applying tangent bias
    vec3 Normal = TangentBias3( TangentZ.xyz );

    #if NEED_WORLD_NORMAL
        vec3 WorldNormal;
    #endif
#endif


#if NEED_TANGENT_SPACE
    // Grab unit vertex tangent by applying tangent bias
    vec3 Tangent = TangentBias3( TangentX );
    
    // Compute the vertex binormal using the normal and tangent vectors
    vec3 Binormal = cross( Normal, Tangent ) * TangentBias1( TangentZ.w );
    
    #if NEED_WORLD_TANGENT
        vec3 WorldTangent;
    #endif
#endif

    #if NEED_VERTEX_COLOR
        // Swap vertex color Red and Blue channel (reversed due to platform-specific byte order)
        vec4 VertexColorRGBA = VertexColor.bgra;
    #endif

    #if NEED_DIRECTIONAL_LIGHT_UNIFORMS
        vec3 WorldLightDirection = LightDirectionAndbDirectional.xyz;
    #endif


    #if USE_GPU_SKINNING
    {
		#if DEPTH_ONLY
			// Use one bone skinning when rendering depth only as it is significantly faster
			vec4 BlendWeight = vec4(1, 0, 0, 0);
		#endif
		
        // Apply matrix palette skinning
        ivec4 BlendIndicesInt = ivec4(BlendIndices);
        vec4 BoneMatR0 = BoneMatrices[BlendIndicesInt.x * 3 + 0] * BlendWeight.x;
        vec4 BoneMatR1 = BoneMatrices[BlendIndicesInt.x * 3 + 1] * BlendWeight.x;
        vec4 BoneMatR2 = BoneMatrices[BlendIndicesInt.x * 3 + 2] * BlendWeight.x;

        #if MAX_BONE_WEIGHTS > 1
        {
            BoneMatR0 += BoneMatrices[BlendIndicesInt.y * 3 + 0] * BlendWeight.y;
            BoneMatR1 += BoneMatrices[BlendIndicesInt.y * 3 + 1] * BlendWeight.y;
            BoneMatR2 += BoneMatrices[BlendIndicesInt.y * 3 + 2] * BlendWeight.y;
        }
        #endif
        #if MAX_BONE_WEIGHTS > 2
        {
            BoneMatR0 += BoneMatrices[BlendIndicesInt.z * 3 + 0] * BlendWeight.z;
            BoneMatR1 += BoneMatrices[BlendIndicesInt.z * 3 + 1] * BlendWeight.z;
            BoneMatR2 += BoneMatrices[BlendIndicesInt.z * 3 + 2] * BlendWeight.z;
        }				
        #endif
        #if MAX_BONE_WEIGHTS > 3
        {
            BoneMatR0 += BoneMatrices[BlendIndicesInt.w * 3 + 0] * BlendWeight.w;
            BoneMatR1 += BoneMatrices[BlendIndicesInt.w * 3 + 1] * BlendWeight.w;
            BoneMatR2 += BoneMatrices[BlendIndicesInt.w * 3 + 2] * BlendWeight.w;
        }
        #endif
	    
        // Compute transposed bone matrix (which was flipped for mat4x3 action)
        mat4 BoneToLocal = mat4(
	        BoneMatR0[0], BoneMatR1[0], BoneMatR2[0], 0.0,
	        BoneMatR0[1], BoneMatR1[1], BoneMatR2[1], 0.0,
	        BoneMatR0[2], BoneMatR1[2], BoneMatR2[2], 0.0,
	        BoneMatR0[3], BoneMatR1[3], BoneMatR2[3], 1.0
	        );
	    
        // Compute the bone to world matrix
        mat3 BoneToLocalRotation = mat3(BoneToLocal[0].xyz, BoneToLocal[1].xyz, BoneToLocal[2].xyz);
        vec4 LocalPosition = BoneToLocal * Position;
#if NEED_INTERMEDIATE_WORLD_POSITION
        // Transform vertex to world space
        WorldPosition = LocalToWorld * LocalPosition;
#else
        // Transform vertex to world space
        WorldPosition = LocalToProjection * LocalPosition;
#endif
        
        #if NEED_WORLD_NORMAL
            // Rotate the vertex normal to world space using just the rotation part of the transform
            vec3 LocalNormal = BoneToLocalRotation * Normal;
            WorldNormal = LocalToWorldRotation * LocalNormal;
        #endif

        #if NEED_NORMAL
        {
            // Rotate the bone-space tangent to local space using the bone matrix
            // @todo: Currently relying on the compiler to optimize this out if not used
            Normal = BoneToLocalRotation * Normal;
        }
        #endif

        #if NEED_TANGENT_SPACE
        {
            #if NEED_WORLD_TANGENT
                // Rotate the tangent to world space using just the rotation part of the transform
                vec3 LocalTangent = BoneToLocalRotation * Tangent;
                WorldTangent = LocalToWorldRotation * LocalTangent;
            #endif

            // Rotate the bone-space tangent to local space using the bone matrix
            Tangent = BoneToLocalRotation * Tangent;
            
            // Re-compute the vertex binormal using the normal and tangent vectors, now that they have been converted from bone to local
            Binormal = cross( Normal, Tangent ) * TangentBias1( TangentZ.w );
        }
        #endif
    }
    #else       // No skinning
    {
        // Transform vertex to world space
        #if NEED_INTERMEDIATE_WORLD_POSITION
            WorldPosition = LocalToWorld * Position;
        #else
            WorldPosition = LocalToProjection * Position;
        #endif
        
        #if NEED_WORLD_NORMAL
            // Rotate the vertex normal to world space using just the rotation part of the world transform
            WorldNormal = LocalToWorldRotation * Normal;
        #endif

        #if NEED_WORLD_TANGENT
            // Rotate the vertex tangent to world space using just the rotation part of the world transform
            WorldTangent = LocalToWorldRotation * Tangent;
        #endif
    }    
    #endif


    #if NEED_TANGENT_SPACE
        mat3 TangentToLocal = mat3( Tangent, Binormal, Normal );
    #endif


    #if NEED_TANGENT_TO_WORLD
    vec3 WorldBinormal;
    mat3 TangentToWorld;
    {
        // Compute the vertex binormal using the normal and tangent vectors
        WorldBinormal = LocalToWorldRotation * Binormal;
        TangentToWorld = mat3( WorldTangent, WorldBinormal, WorldNormal );
    }
    #endif


    #if NEED_PIXEL_TANGENT_TO_WORLD
    {
        VarWorldNormal = WorldNormal;
        VarWorldTangent = WorldTangent;
        #if !COMPUTE_BINORMAL_IN_PIXEL_SHADER
            VarWorldBinormal = WorldBinormal;
        #endif
    }
    #endif


    #if NEED_PIXEL_WORLD_POSITION
    {
        VarWorldPosition = WorldPosition.xyz;
    }
    #endif
    
    
	// Compute texture coordinates for decal if needed
	#if IS_DECAL
	vec2 DecalTexCoords;
	{
		// @todo: For faster shading performance we could simple pass decal texture coordinates down in the vertex stream
		vec4 LocalDecalLocation = Position - vec4( DecalLocation, 1.0 );
		vec2 DecalSpacePosition = ( DecalMatrix * LocalDecalLocation ).xy;
		DecalTexCoords = -DecalSpacePosition + vec2( 0.5, 0.5 ) + DecalOffset;
	}
	#endif


	// For decals, we only have one set of texture coordinates to pull data from (the autogenerated coords)
	#if IS_DECAL
		#if NEED_VERTEX_TEXCOORDS0
			vec2 TexCoords0 = DecalTexCoords;
		#endif

		#if NEED_VERTEX_TEXCOORDS1
			vec2 TexCoords1 = DecalTexCoords;
		#endif

		#if NEED_VERTEX_TEXCOORDS2
			vec2 TexCoords2 = DecalTexCoords;
		#endif

		#if NEED_VERTEX_TEXCOORDS3
			vec2 TexCoords3 = DecalTexCoords;
		#endif
	#endif

    
    #if USE_VERTEX_MOVEMENT
    {
	    //find the center of the object
	    vec3 WorldCenter = (LocalToWorld*vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    	
	    //find the direction from the center of the object to the vertex position
	    vec3 WorldDirectionFromCenter = WorldPosition.xyz - WorldCenter;
    	
	    //Get a tangent vector by crossing the vector with vertical and normalize
	    vec3 VertexMovementTangentVector = cross(WorldDirectionFromCenter, vec3(0.0, 0.0, 1.0));
	    //stops errors with the cross having 0 length
	    VertexMovementTangentVector.x += .0001;
	    VertexMovementTangentVector = normalize(VertexMovementTangentVector );

	    //find max amplitude and offset based on and world offset
	    float VertexMovementFrequencyOffset = (VertexMovementTangentVector.x + VertexMovementTangentVector.y);
	    //take into account attenuation of vertex color
	    float MaximumMovementAmplitude = VertexMovementConstants.y*VertexColorRGBA.r;
	    /**Where on the sine wave * Max intensity * how much it's being used */
	    float VertexTangentMovementAmplitude = sin(VertexMovementConstants.x + VertexMovementFrequencyOffset)*MaximumMovementAmplitude;
	    float VertexVerticalMovementAmplitude = sin(VertexMovementConstants.z + VertexMovementFrequencyOffset)*MaximumMovementAmplitude;
    	
	    //apply the vertex offset
	    WorldPosition.xyz += VertexMovementTangentVector*VertexTangentMovementAmplitude;
	    WorldPosition.xyz += vec3(0.0, 0.0, 1.0)*VertexVerticalMovementAmplitude;
    	
	    //SWAY - move to origin
	    WorldPosition.xyz -= WorldCenter;
	    //rotate by specified matrix
	    vec3 SwayedPosition = mat3(VertexSwayMatrix[0].xyz, VertexSwayMatrix[1].xyz, VertexSwayMatrix[2].xyz)*WorldPosition.xyz;
	    //green channel of vertex color controls interpolation
	    WorldPosition.xyz = mix(WorldPosition.xyz, SwayedPosition, VertexColorRGBA.g);
	    //SWAY - move back to propert location
	    WorldPosition.xyz += WorldCenter;
	 }
    #endif
    
#if NEED_INTERMEDIATE_WORLD_POSITION
    gl_Position = ViewProjection * WorldPosition;
#else
    //the transform to projection space has already been completed
    gl_Position = WorldPosition;
#endif


    // Setup base texture UVs
    {
        vec2 BaseTexCoords;
		#if IS_DECAL
		{
			BaseTexCoords = DecalTexCoords;
		}
		#else
		{
			#if BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS0
				BaseTexCoords = TexCoords0;
			#elif BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS1
				BaseTexCoords = TexCoords1;
			#elif BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS2
				BaseTexCoords = TexCoords2;
			#elif BASE_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS3
				BaseTexCoords = TexCoords3;
			#endif
		}
		#endif

        // Transform texture coordinates for sampling base texture
        UVBase = BASE_TEX_COORD_XFORM( BaseTexCoords );
    }
    
    
    #if USE_LIGHTMAP
    {
        UVLightmap = LightMapCoordinate * LightmapCoordinateScaleBias.xy + LightmapCoordinateScaleBias.wz;
    }
    #endif
    

    // Ambient occlusion from vertex    
    #if USE_AMBIENT_OCCLUSION
        float AmbientOcclusionAmount;
        
        #if AMBIENT_OCCLUSION_SOURCE == AOSOURCE_VERTEX_COLOR_RED
            AmbientOcclusionAmount = VertexColorRGBA.r;
        #elif AMBIENT_OCCLUSION_SOURCE == AOSOURCE_VERTEX_COLOR_GREEN
            AmbientOcclusionAmount = VertexColorRGBA.g;
        #elif AMBIENT_OCCLUSION_SOURCE == AOSOURCE_VERTEX_COLOR_BLUE
            AmbientOcclusionAmount = VertexColorRGBA.b;
        #elif AMBIENT_OCCLUSION_SOURCE == AOSOURCE_VERTEX_COLOR_ALPHA
            AmbientOcclusionAmount = VertexColorRGBA.a;
        #endif
        
        AmbientOcclusion = AmbientOcclusionAmount;
    #endif
    

    // Setup mask offset UVs
    // @todo: Ideally we should not use a separate interpolator unless the UVs are actually going to be different from UVBase (e.g. tex coord transform turned on or different UV source)
    #if NEED_MASK_TEXTURE
    {
        vec2 MaskTexCoords;
        #if MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS0
            MaskTexCoords = TexCoords0;
        #elif MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS1
            MaskTexCoords = TexCoords1;
        #elif MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS2
            MaskTexCoords = TexCoords2;
        #elif MASK_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS3
            MaskTexCoords = TexCoords3;
        #endif

        // Store the mask offset UVs.  Note that we do not transform these UVs.
        UVMask = MaskTexCoords;
    }
    #endif


    #if NEED_EXTRA_COLOR_TEXTURE
    // Setup detail texture UVs
    {
        vec2 DetailTexCoords;
        #if DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS0
            DetailTexCoords = TexCoords0;
        #elif DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS1
            DetailTexCoords = TexCoords1;
        #elif DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS2
            DetailTexCoords = TexCoords2;
        #elif DETAIL_TEXTURE_TEXCOORDS_SOURCE == TEXCOORD_SOURCE_TEXCOORDS3
            DetailTexCoords = TexCoords3;
        #endif

        // Transform texture coordinates for sampling detail texture
        UVDetail = DETAIL_TEX_COORD_XFORM( DetailTexCoords );
    }
    #endif

    #if NEED_INTERMEDIATE_WORLD_POSITION
        // NOTE: We're relying on the compiler to optimize these away if they're not needed
        vec3 EyeToVertexVector = CameraWorldPosition - WorldPosition.xyz;
        vec3 EyeToVertexDirection = normalize( EyeToVertexVector );
    #endif

    #if NEED_PIXEL_TANGENT_CAMERA_VECTOR
    {
        // Rotate camera vector to tangent space
        VarTangentCameraVector = ( EyeToVertexDirection * TangentToWorld );
    }
    #endif


    #if USE_VERTEX_LIGHTMAP
    {
        #if USE_LIGHTMAP_FIXED_SCALE
            PrelitColor = LightMapA.zyx * LIGHTMAP_FIXED_SCALE_VALUE;
        #else
            PrelitColor = LightMapA.zyx * LightMapScale.xyz;
        #endif
    }
    #endif

#if USE_UNIFORM_COLOR_MULTIPLY && USE_VERTEX_COLOR_MULTIPLY
	ColorMultiply.rgb = VertexColorRGBA.rgb * UniformMultiplyColor.rgb;
	ColorMultiply.w = 1.0;
#elif USE_UNIFORM_COLOR_MULTIPLY
	ColorMultiply = UniformMultiplyColor.rgba;
#elif USE_VERTEX_COLOR_MULTIPLY
	ColorMultiply.rgb = VertexColorRGBA.rgb;
	ColorMultiply.w = 1.0;
#endif


#if NEED_LIGHTING_COLOR
	// If a lightmap is bound we'll start off at 100% intensity so that static lighting is retained even
	// if rim lighting is enabled on the material
	#if USE_LIGHTMAP
		LightingColor = vec3( 1.0, 1.0, 1.0 );
	#else
		LightingColor = vec3( 0.0, 0.0, 0.0 );
	#endif
#endif

    #if NEED_PIXEL_TANGENT_LIGHT_DIRECTION
    {
        VarTangentLightDirection = WorldLightDirection * TangentToWorld;
    }
    #endif

    
    #if NEED_PIXEL_TANGENT_UP_VECTOR
    {
        VarTangentUpVector = vec3( 0.0, 0.0, 1.0 ) * TangentToWorld;
    }
    #endif


    #if USE_DYNAMIC_DIRECTIONAL_LIGHT && !USE_NORMAL_MAPPING
    {
        vec3 LightColor = LightColorAndFalloffExponent.xyz * 
	        max(0.0, dot(WorldLightDirection, WorldNormal));

        LightingColor += LightColor;
    }   
    #endif
    
    
    

    // Per-vertex environment reflection vector and rim term/fresnel
    #if NEED_PER_VERTEX_RIM_TERM
    float PerVertexRimTerm;
    vec3 PerVertexEnvironmentVector;
    {
        // Compute environment reflection per vertex
        #if USE_REFLECTION_BASED_ENVIRONMENT_MAPS
            // Reflection-based
            PerVertexEnvironmentVector = reflect( EyeToVertexDirection, WorldNormal );
        #else
            // View direction-based
            PerVertexEnvironmentVector = EyeToVertexDirection;
        #endif
        
       
        // NOTE: We're relying on the compiler's dead code removal to eliminate this when not needed
        float NDotE = dot( EyeToVertexDirection, WorldNormal );
        PerVertexRimTerm = max( 0.01, 1.0 - abs( NDotE ) );
    }
    #endif



    #if USE_RIM_LIGHTING
    {    
        #if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_RIM_LIGHTING
        {
            #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED
                RimLightingMask = VertexColorRGBA.r;
            #elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN
                RimLightingMask = VertexColorRGBA.g;
            #elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE
                RimLightingMask = VertexColorRGBA.b;
            #elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
                RimLightingMask = VertexColorRGBA.a;
            #endif            
        }
        #else
        {
			vec3 RimLighting;
			{
                // Note: The 'strength' of the rim light is pre-multiplied with the color
                vec3 RimLightingColor = RimLightingColorAndExponent.xyz;
                float RimLightingExponent = RimLightingColorAndExponent.w;
	        
  	            RimLighting = RimLightingColor * pow( PerVertexRimTerm, RimLightingExponent );
	        }

            #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_CONSTANT || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE || RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
            
                // Unmasked or vertex-color masked, so just combine rim lighting with the other lighting from vertex shader
                #if RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_RED
                    RimLighting *= VertexColorRGBA.r;
                #elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_GREEN
                    RimLighting *= VertexColorRGBA.g;
                #elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_BLUE
                    RimLighting *= VertexColorRGBA.b;
                #elif RIM_LIGHTING_MASK_SOURCE == RIM_LIGHTING_MASK_SOURCE_VERTEX_COLOR_ALPHA
                    RimLighting *= VertexColorRGBA.a;
                #endif            
                
			    LightingColor += RimLighting;
            #else
                MaskableRimLighting = RimLighting;
            #endif
        }
        #endif
     }
    #endif



    #if USE_DYNAMIC_SKY_LIGHT && !( USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_DYNAMIC_SKY_LIGHT )
    {
        // calculate how much of the sky colors to use (normal.z, -1 = all lower, 1 = all upper, 0 = 50/50)
		float NormalContribution = WorldNormal.z * 0.5 + 0.5;

		#if USE_FASTER_SKY_LIGHT
			// NOTE: 0.5 is to create results more similar to when the squared falloff is applied (below)
			vec3 SkyColor = 0.5 * mix( LowerSkyColor.xyz, UpperSkyColor.xyz, NormalContribution );
		#else
			// Apply square sky light intensity falloff based on surface angle relative to either hemisphere
			vec2 ContributionWeightsSqrt = vec2(0.5, 0.5) + vec2(0.5, -0.5) * NormalContribution;
			vec2 ContributionWeights = ContributionWeightsSqrt * ContributionWeightsSqrt;
			vec3 UpperLighting = UpperSkyColor.rgb * ContributionWeights.x;
			vec3 LowerLighting = LowerSkyColor.rgb * ContributionWeights.y;
			vec3 SkyColor = UpperLighting + LowerLighting;
		#endif

        #if USE_AMBIENT_OCCLUSION
            // Also apply AO from the vertex color to the sky light
            SkyColor *= AmbientOcclusionAmount;
        #endif
        
        LightingColor += SkyColor;
    }   
    #endif


    // Color texture blending
    #if USE_COLOR_TEXTURE_BLENDING
	#if TEXTURE_BLEND_FACTOR_SOURCE == TEXTURE_BLEND_FACTOR_SOURCE_VERTEX_COLOR
	{
   	    TextureBlendFactor = VertexColorRGBA.r;
	}
	#endif
    #endif
    
    
    // Per-vertex environment mapping
    #if USE_ENVIRONMENT_MAPPING
    {
        #if USE_NORMAL_MAPPING && ALLOW_PER_PIXEL_ENVIRONMENT_MAPPING 
        {
            #if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_RED
                EnvironmentMask = VertexColorRGBA.r;
            #elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_GREEN
                EnvironmentMask = VertexColorRGBA.g;
            #elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_BLUE
                EnvironmentMask = VertexColorRGBA.b;
            #elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_ALPHA
                EnvironmentMask = VertexColorRGBA.a;
            #endif            
        }
        #else
        {
        float EnvironmentAmount = EnvironmentParameters.x;
        
        // Apply fresnel
        #if USE_ENVIRONMENT_FRESNEL
        {
            float FresnelAmount = EnvironmentParameters.y;
            float FresnelExponent = EnvironmentParameters.z;
            
                float FresnelTerm = pow( PerVertexRimTerm, FresnelExponent );
            float ScaledFresnelTerm = ( 1.0 - FresnelAmount ) + FresnelAmount * FresnelTerm;
            
            EnvironmentAmount *= ScaledFresnelTerm;
        }
        #endif
        
            #if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_RED || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_GREEN || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_BLUE || ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_ALPHA
                #if ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_RED
                    EnvironmentAmount *= VertexColorRGBA.r;
                #elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_GREEN
                    EnvironmentAmount *= VertexColorRGBA.g;
                #elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_BLUE
                    EnvironmentAmount *= VertexColorRGBA.b;
                #elif ENVIRONMENT_MASK_SOURCE == ENVIRONMENT_MASK_SOURCE_VERTEX_COLOR_ALPHA
                    EnvironmentAmount *= VertexColorRGBA.a;
                #endif            
            #endif
            
            EnvironmentVectorAndAmount.xyz = PerVertexEnvironmentVector;
            EnvironmentVectorAndAmount.w = EnvironmentAmount;
        }
        #endif
    }
    #endif
    
    
    #if USE_EMISSIVE
    {
        #if EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_RED
            EmissiveMask = VertexColorRGBA.r;
        #elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_GREEN
            EmissiveMask = VertexColorRGBA.g;
        #elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_BLUE
            EmissiveMask = VertexColorRGBA.b;
        #elif EMISSIVE_MASK_SOURCE == EMISSIVE_MASK_SOURCE_VERTEX_COLOR_ALPHA
            EmissiveMask = VertexColorRGBA.a;
        #endif            
    }
    #endif


    // Per-vertex specular
    #if USE_VERTEX_SPECULAR
    {
        #if USE_HALF_VECTOR_SPECULAR_MODEL
            // Half-vector specular (faster, big highlight, less realistic)
            vec3 WorldHalfVector = normalize( WorldLightDirection + EyeToVertexDirection );
            float BaseSpec = max( 0.0, dot( WorldNormal, WorldHalfVector ) );  // NDotH
        #elif 1
            // Reflection-based specular (slower, higher quality)
            vec3 WorldReflectionVector = reflect( WorldLightDirection, WorldNormal );
            float BaseSpec = max( 0.0, dot( EyeToVertexDirection, WorldReflectionVector ) );   // VDotR
        #else
            // Diffuse lighting (for debugging only)
            float BaseSpec = max( 0.0, dot( WorldLightDirection, WorldNormal ) );  // NDotL
        #endif

        #if USE_VERTEX_FIXED_SPECULAR_POWER_APPROXIMATION
            // Fast specular approximation (http://www.gamasutra.com/view/feature/2972/a_noninteger_power_function_on_.php)
            // Basically pow( N, P ) can be approximated by pow( max( A * N + B ), M )
            //      - A and B are constants that must be tweaked to get artifact-free results
            //      - M can be really small in practice (2 looks good, can be replaced by single multiply)
            // This should result in a mad_sat instruction plus one multiply (2 instructions total!)

            #define SpecApproxA 6.645
            #define SpecApproxB -5.645   
            float SpecularAmount = clamp( SpecApproxA * BaseSpec + SpecApproxB, 0.0, 1.0 );
            SpecularAmount *= SpecularAmount;   // M = 2
        #else
            // Power function for specular curve
            float SpecularAmount = pow( BaseSpec, SpecularPower );
        #endif        

        #if 0
            // Apply specular shadowing by multiplying with diffuse coefficient
            float ShadowTerm = max( 0.0, dot( WorldLightDirection, WorldNormal ) );
            SpecularAmount *= ShadowTerm;
        #endif
       
        Specular = SpecularAmount * LightColorTimesSpecularColor;
    }
    #elif USE_PIXEL_SPECULAR
    {    
        #if PER_PIXEL_LIGHTING_IN_TANGENT_SPACE
        {
            // Half-vector specular (faster, big highlight, less realistic)
            vec3 WorldHalfVector = WorldLightDirection + EyeToVertexDirection ;
            VarTangentHalfVector = normalize( WorldHalfVector * TangentToWorld );
        }
        #endif
    }
    #endif
    
    #if USE_GRADIENT_FOG
    {
        // Compute the fog value (a lerped value between 0 and 1) for each vertex, but
        // the color always passes directly through unmodified
        FogColorAndAmount.xyz = FogColor.xyz;
        
        // We determine the fog factor by getting the vertex distance and then interpolating
        // that between the start and end ranges for fog.
        float VertDistSquared = dot(WorldPosition.xyz, WorldPosition.xyz);
        FogColorAndAmount.w = FogColor.w * clamp( ( VertDistSquared - FogStartSquared ) * FogOneOverSquaredRange, 0.0, 1.0);
    }
    #endif
}
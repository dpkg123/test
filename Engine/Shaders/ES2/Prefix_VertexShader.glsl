/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

// Tangent space bias (for unpacking packed vertex normals and such)
#define TangentBias1( Val ) ( ( Val /       127.5 )   -       1.0 )

// Tangent space bias (for unpacking packed vertex normals and such)
#define TangentBias3( Vec ) ( ( Vec / vec3( 127.5 ) ) - vec3( 1.0 ) )

//////////////////////////
// SHARED TEXTURE CODE
//////////////////////////

#if USE_TEXCOORD_XFORM
	#if TEXCOORD_XFORM_TARGET == TEXCOORD_XFORM_TARGET_BASE
		#define BASE_TEX_COORD_XFORM(InTextureCoord) (TextureTransform*vec3(InTextureCoord, 1.0)).xy
	#else
		#define BASE_TEX_COORD_XFORM(InTextureCoord) InTextureCoord
	#endif

	#if TEXCOORD_XFORM_TARGET == TEXCOORD_XFORM_TARGET_DETAIL
		#define DETAIL_TEX_COORD_XFORM(InTextureCoord) (TextureTransform*vec3(InTextureCoord, 1.0)).xy
	#else
		#define DETAIL_TEX_COORD_XFORM(InTextureCoord) InTextureCoord
	#endif

	// Texture transform matrix
	uniform mat3 TextureTransform;
#else

	#define BASE_TEX_COORD_XFORM(InTextureCoord) InTextureCoord
	#define DETAIL_TEX_COORD_XFORM(InTextureCoord) InTextureCoord
#endif
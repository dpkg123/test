/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#include <spu_intrinsics.h>
#include <string.h>
#include "edge/edge_stdint.h"
#include "edge/edge_assert.h"
#include "edge/post/edgePost.h"
#include "edge/post/edgePost_spu.h"
#include "edge/post/edgePost_framework_spu.h"
#include "PostParams.h"

extern "C"
void edgePostMain( EdgePostTileInfo* tileInfo )
{
	const EdgePostProcessStage& stage = *tileInfo->stage;
	PostParams* pParams = (PostParams*)tileInfo->parameters;

	// assumptions
	EDGE_ASSERT( stage.sources[0].width == stage.sources[1].width);
	EDGE_ASSERT( stage.sources[0].height == stage.sources[1].height);
	EDGE_ASSERT( stage.sources[0].bytesPerPixel == 4);
	
	// ilr buffer dimension
	const unsigned int w = stage.sources[0].width;
	const unsigned int h = stage.sources[0].height;

	// define tint array
	#define ELEMENT(r, g, b, a) { 0, r * pParams->m_ilrStrenght, g * pParams->m_ilrStrenght, b  * pParams->m_ilrStrenght}
	const vec_float4 tints[] = 
	{
		ELEMENT( 1.0f, 0.3f, 0.2f, -0.4f),
		ELEMENT( 1.0f, 0.6f, 0.3f, -0.2f),
		ELEMENT( 1.0f, 0.3,  0.2f, 0.65f),
		ELEMENT( 1.0f, 0.7f, 0.4f, 0.7f	),
		ELEMENT( 0.8f, 0.7f, 0.7f, 0.9f	),
		ELEMENT( 0.4f, 0.55f,0.8f, 1.3f	),
		ELEMENT( 0.55f,0.4f, 1.0f, 1.8f	),
		ELEMENT( 0.7f, 0.3f, 0.9f, 2.1f	),
	};

	// scales array
	#define SCALE( x, y) { -1.0f/x * 0.5f * w, -1.0f/y * 0.5f * h, 0, 0}
	const vec_float4 scales[] = 
	{
		SCALE( -.4f, -.4f),
		SCALE( -.2f, -2.f),
		SCALE( .65f, .65f),
		SCALE( .7f, .7f ),
		SCALE( .9f, .9f ),
		SCALE( 1.3f, 1.3f ),
		SCALE( 1.8f, 1.8f ),
		SCALE( 2.1f, 2.1f ),
	};
	
	// in-place vignette the input
	edgePostVignette( tileInfo->tiles[1], tileInfo->tiles[1], w, h );

	// call ILR
	edgePostIlr( tileInfo->tiles[0], tileInfo->tiles[1], w, h, tints, scales );
}

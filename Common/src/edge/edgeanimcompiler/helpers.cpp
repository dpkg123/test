/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "helpers.h"

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
	Collect curves that animate a particular node.
**/
//--------------------------------------------------------------------------------------------------

void GetAnimateds( FCDSceneNode* pNode, FCDAnimatedList& animateds )
{
    const FCDTransform** transforms = pNode->GetTransforms();

    // Collect all the animation curves
	for(unsigned i=0; i<pNode->GetTransformCount(); i++)
	{
        const FCDAnimated* animated = transforms[i]->GetAnimated();
        if (animated != NULL && animated->HasCurve()) animateds.push_back(animated);
    }
}

//--------------------------------------------------------------------------------------------------

void* LoadFile( const char* filename )
{
	void* data = NULL;

	FILE* fp = fopen( filename , "rb" );
	if( fp )
	{
		fseek( fp, 0, SEEK_END );
		unsigned int len = ftell( fp );
		data = new unsigned char[ len ];

		fseek( fp, 0, SEEK_SET );
		fread( data, 1, len, fp );
		fclose( fp );
	}

	return data;
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools

}   // namespace Edge

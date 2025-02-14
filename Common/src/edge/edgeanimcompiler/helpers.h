/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#ifndef HELPERS_H
#define HELPERS_H

//--------------------------------------------------------------------------------------------------

namespace Edge
{
	namespace Tools
	{
		void  GetAnimateds( FCDSceneNode* pNode, FCDAnimatedList& animateds );
		void* LoadFile( const char* filename );
	}
}

//--------------------------------------------------------------------------------------------------

#endif // HELPERS_H

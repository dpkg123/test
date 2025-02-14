/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include "joints.h"

#include <FCDocument/FCDSceneNode.h>

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
    Find the skeleton joints.
**/
//--------------------------------------------------------------------------------------------------

bool	FindSkeletonJointsRecursive(const FCDSceneNode* pNode, std::vector<const FCDSceneNode*>& joints, bool foundJointAbove )
{
	bool foundJointBelow = false;

	// Recurse - determine if any joints exist in children
	for( size_t i = pNode->GetChildrenCount(); i > 0; i-- )
	{
		if( FindSkeletonJointsRecursive( pNode->GetChild( i - 1 ), joints, ( pNode->GetJointFlag() || foundJointAbove ) ) )
		{
			foundJointBelow = true;
		}
	}

	// Add this node if its a joint or if joints are above & below it (intermediary node)
	if ( pNode->GetJointFlag() || ( foundJointAbove && foundJointBelow ) )
	{
		joints.push_back( pNode );
		foundJointBelow = true;
	}

	return foundJointBelow;
}

void	FindSkeletonJoints(const FCDSceneNode* pNode, std::vector<const FCDSceneNode*>& joints)
{
	// Buildup hierarchy (back to front)
	std::vector<const FCDSceneNode*> nodeJoints;
	FindSkeletonJointsRecursive( pNode, nodeJoints, false );

	// Add to joint list in reverse - puts parent first again
	std::vector<const FCDSceneNode*>::reverse_iterator rit;
	for( rit = nodeJoints.rbegin(); rit < nodeJoints.rend(); rit++ )
	{
		joints.push_back( *rit );
	}
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

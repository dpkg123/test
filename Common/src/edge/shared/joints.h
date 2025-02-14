/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef JOINTS_H
#define JOINTS_H

#include <vector>

class FCDSceneNode;

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

void FindSkeletonJoints(const FCDSceneNode* pNode, std::vector<const FCDSceneNode*>& joints);

}   // namespace Tools
}   // namespace Edge

#endif // JOINTS_H

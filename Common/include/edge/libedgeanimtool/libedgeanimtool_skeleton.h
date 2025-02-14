/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEANIMTOOL_SKELETON_H
#define LIBEDGEANIMTOOL_SKELETON_H

//--------------------------------------------------------------------------------------------------

#include <vector>
#include <set>
#include "edge/libedgeanimtool/libedgeanimtool_common.h"

#include "edge/anim/edgeanim_structs.h"
#include "edge/anim/edgeanim_common.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------

struct HierachyQuad
{
    int m_index[4];
    int m_indexParent[4];
};

//--------------------------------------------------------------------------------------------------

struct UserChannelInfo
{
    unsigned int        m_nodeNameHash;
    unsigned int        m_channelNameHash;
    unsigned char       m_componentIndex;
    unsigned char       m_flags;
};

//--------------------------------------------------------------------------------------------------

struct Skeleton
{
	unsigned int					m_locoJointIndex;
    unsigned int                    m_numJoints;
    unsigned int                    m_numUserChannels;
    std::vector<int>                m_parentIndices;    
	std::vector<Joint>              m_basePose;
	std::vector<bool>               m_scaleCompensateFlags;
    std::vector<unsigned int>       m_jointNameHashes;
    std::vector<UserChannelInfo>    m_userChannelInfoArray;
};

//--------------------------------------------------------------------------------------------------

void ExtractSkeleton( const EdgeAnimSkeleton* pSkel, Skeleton& skeleton );
void ExportSkeleton( const std::string skelBinFilename, const Skeleton& skeleton, bool bigEndian, CustomDataCallback customDataCallback = 0, void* customData = 0 );
void ExportSkeleton( const std::string skelBinFilename, std::ostream* pStream, const Skeleton& skeleton, bool bigEndian, CustomDataCallback customDataCallback = 0, void* customData = 0 );
void ParseUserChannels( const std::string filename, std::vector<Edge::Tools::UserChannelInfo>& userChannelInfoArray); 
void BuildSimdHierarchy( std::vector<HierachyQuad>& hierachyQuads, const Skeleton& skeleton, const std::set<int>& jointSet );

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

#endif // LIBEDGEANIMTOOL_SKELETON_H

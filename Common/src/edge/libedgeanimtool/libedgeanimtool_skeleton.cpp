/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#ifdef _MSC_VER
#pragma warning( disable : 4702 )       // unreachable code
#endif//_MSC_VER

#include    <set>
#include    <fstream>
#include    <iostream>
#include    <string>

#include    "edge/libedgeanimtool/libedgeanimtool_skeleton.h"
#include    "edge/libedgeanimtool/libedgeanimtool_filewriter.h"
#include    "edge/libedgeanimtool/libedgeanimtool_bitstream.h"

#include    "edge/anim/edgeanim_structs.h"
#include    "edge/anim/edgeanim_common.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{

namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
    Retrieves the skeleton from a binary (.skel) resource.
**/
//--------------------------------------------------------------------------------------------------

void    ExtractSkeleton(const EdgeAnimSkeleton* pSkel, Skeleton& skeleton)
{
#define EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(type,offset)                       \
    (                                                                         \
        (offset)? (type*)((intptr_t)(&offset)+(intptr_t)(Reverse(offset, bigEndian)))    \
                : (type*)0                                                    \
    )                                                                         \

    // check tag + determine endian-ness
	bool bigEndian = false;
	if( Reverse(pSkel->tag, bigEndian) != edgeAnimGetSkelTag() )
	{
		bigEndian = true;
		if( Reverse(pSkel->tag, bigEndian) != edgeAnimGetSkelTag() )
		{
			std::cerr << "Invalid edge skeleton binary" << std::endl;
			EDGEERROR_F();
		}
	}

	skeleton.m_locoJointIndex = Reverse(pSkel->locomotionJointIndex, bigEndian);
    skeleton.m_numJoints = Reverse(pSkel->numJoints, bigEndian);

	// parent indices
	const uint16_t* pParentIndices = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(uint16_t, pSkel->offsetParentIndicesArray);
	for(unsigned int i=0; i<skeleton.m_numJoints; i++)
	{
		const int parentIndex = (int)(short)Reverse(pParentIndices[i], bigEndian);
		skeleton.m_parentIndices.push_back(parentIndex);
	}

	// scale compensate flags
	const uint16_t* jointLinkage = pSkel->simdHierarchy;
	const unsigned int numSimdElements = 4 * Reverse(pSkel->numSimdHierarchyQuads, bigEndian);
	skeleton.m_scaleCompensateFlags.resize(skeleton.m_numJoints, false);
	for( unsigned int i=0; i<numSimdElements; i++)
	{
		const uint16_t idxJoint = Reverse(jointLinkage[2*i+0],bigEndian);
		const uint16_t idxParent = Reverse(jointLinkage[2*i+1],bigEndian);
		const bool compensateParent = (idxParent & 0x8000) == 0x8000;
		skeleton.m_scaleCompensateFlags[idxJoint] = compensateParent;
	}

	// base pose
    const Joint* pJoints = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(Joint, pSkel->offsetBasePose);
    for(unsigned int i=0; i<skeleton.m_numJoints; i++)
    {
        Float4 r = Reverse(pJoints[i].m_rotation, bigEndian);
        Float4 t = Reverse(pJoints[i].m_translation, bigEndian);
        Float4 s = Reverse(pJoints[i].m_scale, bigEndian);

        Joint j = {r, t, s};

        skeleton.m_basePose.push_back(j);
    }

    // joint hash names
    const unsigned int* pJointNames = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(unsigned int, pSkel->offsetJointNameHashArray);
    for(unsigned int i=0; i<skeleton.m_numJoints; i++) {
        unsigned int hash = Reverse(pJointNames[i], bigEndian);
        skeleton.m_jointNameHashes.push_back(hash);
    }

    // user channel info
    unsigned char idVal = EDGE_ANIM_USER_CHANNEL_FLAG_CPT_BIT1;
    unsigned char idBit = 0;
    while(idVal >>= 1) idBit++;
    unsigned char idMask = EDGE_ANIM_USER_CHANNEL_FLAG_CPT_BIT0 | EDGE_ANIM_USER_CHANNEL_FLAG_CPT_BIT1;
    skeleton.m_numUserChannels = Reverse(pSkel->numUserChannels, bigEndian);
    const unsigned int* pUserChannelNodeNames = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(unsigned int, pSkel->offsetUserChannelNodeNameHashArray);
    const unsigned int* pUserChannelNames = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(unsigned int, pSkel->offsetUserChannelNameHashArray);
    const unsigned char* pUserChannelFlags = EDGE_OFFSET_BIG_ENDIAN_GET_POINTER(unsigned char, pSkel->offsetUserChannelFlagsArray);
    for(unsigned int i=0; i<skeleton.m_numUserChannels; i++) {
        unsigned int nodeHash = Reverse(pUserChannelNodeNames[i], bigEndian);
        unsigned int chanHash = Reverse(pUserChannelNames[i], bigEndian);
        unsigned char flags = pUserChannelFlags[i];

        UserChannelInfo info;
        info.m_nodeNameHash = nodeHash;
        info.m_channelNameHash = chanHash;
        info.m_flags = flags & (~idMask);
        info.m_componentIndex = (unsigned char)(flags & idMask) >> idBit;
        skeleton.m_userChannelInfoArray.push_back(info);
    }

	// custom data warning
	const unsigned int sizeCustomData = Reverse( pSkel->sizeCustomData, bigEndian );
	if( sizeCustomData > 0 )
	{
		std::cerr << "WARNING: edge skeleton binary contains custom data that can't be extracted" << std::endl;
	}
}


//--------------------------------------------------------------------------------------------------
/**
    Export binary animation file
**/
//--------------------------------------------------------------------------------------------------

void    ExportSkeleton( std::string outputSkelFilename, const Skeleton& skeleton, bool bigEndian, CustomDataCallback customDataCallback, void* customData)
{
	std::ofstream outStream(outputSkelFilename.c_str(), std::ios_base::out|std::ios_base::binary);
	ExportSkeleton( outputSkelFilename, &outStream, skeleton, bigEndian, customDataCallback, customData); 
}

//--------------------------------------------------------------------------------------------------
/**
    Export the skeleton runtime data.
**/
//--------------------------------------------------------------------------------------------------

void    ExportSkeleton( std::string outputSkelFilename, std::ostream* pStream, const Skeleton& skeleton, bool bigEndian, CustomDataCallback customDataCallback, void* customData)
{
    // build SIMD hierarchy
    std::set<int> setTodo;
    for(unsigned int jointLoop = 0; jointLoop < skeleton.m_numJoints; jointLoop++) {
        setTodo.insert(jointLoop);
    }

    std::vector<HierachyQuad> hierarchyQuads;
	BuildSimdHierarchy( hierarchyQuads, skeleton, setTodo );

    // binary output : header
	FileWriter fileWriter(pStream, bigEndian);
    fileWriter.Label("Start");

    fileWriter.Write(edgeAnimGetSkelTag());
    fileWriter.WriteOffset32("End", "Start");
	fileWriter.WriteOffset32("EndCustomData", "StartCustomData");
	fileWriter.WriteOffset32("EndNames", "StartNames");
    fileWriter.Write((unsigned short) skeleton.m_numJoints);
    fileWriter.Write((unsigned short) skeleton.m_numUserChannels);
    fileWriter.Write((unsigned short) hierarchyQuads.size());
    fileWriter.Write((unsigned short) skeleton.m_locoJointIndex);
    fileWriter.WriteOffset32("BasePose");
    fileWriter.WriteOffset32("ParentIndices");
    fileWriter.WriteOffset32("JointNameHashes");
    fileWriter.WriteOffset32("UserChannelNameHashes");
    fileWriter.WriteOffset32("UserChannelNodeNameHashes");
    fileWriter.WriteOffset32("UserChannelFlags");
	fileWriter.WriteOffset32("CustomData");
	fileWriter.Write((unsigned int) 0);
	fileWriter.Write((unsigned int) 0);
	fileWriter.Write((unsigned int) 0);

    // binary output : simd hierarchy
	if( ( fileWriter.GetPos() % 16L ) != 0 )
	{
		std::cerr << "Error EdgeSkeleton SIMD Hierarchy must be aligned to a 16 byte boundary'" << std::endl;
		EDGEERROR_F();
	}

    fileWriter.Label("SimdHierarchy");
    for(std::vector<HierachyQuad>::const_iterator ite = hierarchyQuads.begin();
         ite != hierarchyQuads.end();
         ite++) {
        for(int eltLoop = 0; eltLoop < 4; eltLoop++) {
            fileWriter.Write((unsigned short) ite->m_index[eltLoop]);
            fileWriter.Write((unsigned short) ite->m_indexParent[eltLoop]);
        }
    }

    // binary output : base pose
    fileWriter.Align(16);
    fileWriter.Label("BasePose");
    for(std::vector<Joint>::const_iterator iteBasePoseJoints = skeleton.m_basePose.begin();
         iteBasePoseJoints != skeleton.m_basePose.end();
         iteBasePoseJoints++) {
        for(int eltLoop = 0; eltLoop < 4; eltLoop++)
            fileWriter.Write(iteBasePoseJoints->m_rotation[eltLoop]);
        for(int eltLoop = 0; eltLoop < 4; eltLoop++)
            fileWriter.Write(iteBasePoseJoints->m_translation[eltLoop]);
        for(int eltLoop = 0; eltLoop < 4; eltLoop++)
            fileWriter.Write(iteBasePoseJoints->m_scale[eltLoop]);
    }

    // binary output : parent indices
    fileWriter.Align(16);
    fileWriter.Label("ParentIndices");
    for (std::vector<int>::const_iterator ite = skeleton.m_parentIndices.begin(); 
        ite != skeleton.m_parentIndices.end(); ite++ ) {
        fileWriter.Write((unsigned short) *ite);
    }

	// binary output : user channel flags
	fileWriter.Label("UserChannelFlags");
	unsigned char idVal = EDGE_ANIM_USER_CHANNEL_FLAG_CPT_BIT1;
	unsigned int idBit = 0;
	while(idVal >>= 1) idBit++;
	for(unsigned int i=0; i<skeleton.m_userChannelInfoArray.size(); i++)
	{
		unsigned char flags = skeleton.m_userChannelInfoArray[i].m_flags;
		unsigned char cptIdx = skeleton.m_userChannelInfoArray[i].m_componentIndex;
		flags |= (cptIdx << idBit);
		fileWriter.Write(flags);
	}

	// binary output : custom data supplied by client code
	if ( customDataCallback )
	{
		fileWriter.Align(16);
		fileWriter.Label("StartCustomData");

		fileWriter.Label("CustomData");
		customDataCallback( fileWriter, customData );

		fileWriter.Align(16);
		fileWriter.Label("EndCustomData");
	}

	// binary output : joint name hashes
	fileWriter.Align(16);
	fileWriter.Label("StartNames");

	fileWriter.Label("JointNameHashes");
	for (std::vector<unsigned int>::const_iterator iteNames = skeleton.m_jointNameHashes.begin();
		iteNames != skeleton.m_jointNameHashes.end();
		iteNames++ ) {
			fileWriter.Write(*iteNames);
	}

	// binary output : user channel name hashes
	fileWriter.Label("UserChannelNameHashes");
	for(unsigned int i=0; i<skeleton.m_userChannelInfoArray.size(); i++)
		fileWriter.Write(skeleton.m_userChannelInfoArray[i].m_channelNameHash);

	// binary output : user channel node name hashes
	fileWriter.Label("UserChannelNodeNameHashes");
	for(unsigned int i=0; i<skeleton.m_userChannelInfoArray.size(); i++)
		fileWriter.Write(skeleton.m_userChannelInfoArray[i].m_nodeNameHash);

    // binary output : end label (to compute total size)
    fileWriter.Align(16);
	fileWriter.Label("EndNames");
    fileWriter.Label("End");

	// binary output : close the file & patch offsets
    fileWriter.PatchOffsets();
    fileWriter.Close();
}


//--------------------------------------------------------------------------------------------------
/**
    Parse user channel text file
**/
//--------------------------------------------------------------------------------------------------

void ParseUserChannels( const std::string filename, std::vector<Edge::Tools::UserChannelInfo>& userChannelInfoArray) 
{
    struct Flag {
        const char*     m_name;
        unsigned int    m_value;
    };

    Flag flags[] = {
        {"clamp01",     EDGE_ANIM_USER_CHANNEL_FLAG_CLAMP01},
        {"minmax",      EDGE_ANIM_USER_CHANNEL_FLAG_MINMAX}
    };

	std::ifstream fs(filename.c_str());
	while(fs.good())
    {
        Edge::Tools::UserChannelInfo info;

        std::string lineStr;
        std::getline(fs, lineStr);
        if(lineStr.size() == 0)
            continue;

        std::vector<std::string> tokArray;
        unsigned int lastPos = 0;
        unsigned int flagsIdx = 0;
        std::string delimiters = ".,[]";
        for(unsigned int i=0; i<lineStr.size(); i++) {
            unsigned char c = lineStr[i];

            // check for delimiter
            if(delimiters.find(c) != std::string::npos) {
                std::string last = lineStr.substr(lastPos, i - lastPos);
                tokArray.push_back(last);
                lastPos = i+1;
            }
            // check for end of string
            else if(i == lineStr.size() - 1) {
                std::string last = lineStr.substr(lastPos, i + 1 - lastPos);
                tokArray.push_back(last);
            }

            // record idx of first flag
            if(c == '[') {
                flagsIdx = (unsigned int) tokArray.size();
            }
        }

        // remove leading/trailing whitespace and populate name/flag arrays
        std::vector<std::string> nameArray;
        std::vector<std::string> flagArray;
        for(unsigned int i=0; i<tokArray.size(); i++) {
            std::string str = tokArray[i];
            std::string::size_type first = str.find_first_not_of(" ");
            std::string str0 = first == std::string::npos ? std::string() : str.substr(first, str.find_last_not_of(" ") - first + 1);
            if((flagsIdx == 0) || (i < flagsIdx))
                nameArray.push_back(str0);
            else
                flagArray.push_back(str0);
        }

        // get node/channel names
        if(!((nameArray.size() == 2) || (nameArray.size() == 3))) {
            std::cerr << "Error parsing user channels: invalid user channel definition '" << lineStr << "'" << std::endl;
			EDGEERROR_F();
        }
        info.m_nodeNameHash = edgeAnimGenerateNameHash(nameArray[0].c_str());
        info.m_channelNameHash = edgeAnimGenerateNameHash(nameArray[1].c_str());

        // get component
        unsigned char componentIndex = 0;
        if(nameArray.size() == 3) {
            if(nameArray[2] == "x")
                componentIndex = 0;
            else if(nameArray[2] == "y")
                componentIndex = 1;
            else if(nameArray[2] == "z")
                componentIndex = 2;
            else if(nameArray[2] == "w")
                componentIndex = 3;
            else
            {
                std::cerr << "Error parsing user channels: invalid component '" << nameArray[2] << "'" << std::endl;
				EDGEERROR_F();
            }
        }

        info.m_componentIndex = componentIndex;

        // parse flags
        unsigned char flag = 0;
        bool foundFlag = false;
        for(unsigned int i=0; i<flagArray.size(); i++) {
            for(unsigned int j=0; j<sizeof(flags)/sizeof(flags[0]); j++) {
                if(flagArray[i] == flags[j].m_name) {
                    flag |= flags[j].m_value;
                    foundFlag = true;
                    break;
                }
            }
            if(!foundFlag) {
				std::cerr << "Error parsing user channels: invalid flag '" << flagArray[i] << "'" << std::endl;
				EDGEERROR_F();
            }
        }
        info.m_flags = flag;

		// test for hash clashes
		for( size_t i = 0; i < userChannelInfoArray.size(); i++ ) {
			const Edge::Tools::UserChannelInfo& prev = userChannelInfoArray[i];
			if( ( prev.m_nodeNameHash == info.m_nodeNameHash ) && ( prev.m_channelNameHash == info.m_channelNameHash ) ) {
				std::cerr << "Error parsing user channels: name hash clash detected" << std::endl;
				EDGEERROR_F();
			}
		}

        userChannelInfoArray.push_back(info);
    }
    fs.close();
}

//--------------------------------------------------------------------------------------------------
/**
Build SIMD hierarchy for required joints (greedy algorithm, can be optimised)
**/
//--------------------------------------------------------------------------------------------------

void BuildSimdHierarchy( std::vector<HierachyQuad>& hierachyQuads, const Skeleton& skeleton, const std::set<int>& jointSet )
{
	std::set<int> setTodo = jointSet;

	while(!setTodo.empty()) {
		// current quad (structure is different from the runtime, to be easily indexed)
		HierachyQuad curQuad;
		int quadCurrentSize = 0;

		// find candidate joints...
		for(std::set<int>::const_iterator ite = setTodo.begin(); (ite != setTodo.end()) && (quadCurrentSize < 4); ite++) {
			int idxCurrent = *ite;
			int idxParent = skeleton.m_parentIndices[idxCurrent];

			// joint must not have his parent in the "todo" set
			if (setTodo.find(idxParent) != setTodo.end())
				continue;

			// scale compensate flag goes in high bit of parent index
			if(idxParent != -1) {
				if (idxCurrent < (int) skeleton.m_scaleCompensateFlags.size()) {
					idxParent |= skeleton.m_scaleCompensateFlags[idxCurrent] ? 0x8000 : 0;
				}
			}

			curQuad.m_index[quadCurrentSize] = idxCurrent;
			curQuad.m_indexParent[quadCurrentSize] = idxParent;     
			quadCurrentSize++;
		}

		// there is no candidate. it means there is circular dependency somewhere
		if( 0 == quadCurrentSize ) {
			std::cerr << "Error: unable to construct skeleton hierarchy (circular dependency in parentage?).\n";
			EDGEERROR_F();
		}

		// replicate last elements
		for( int i = quadCurrentSize; i < 4; i++ ) {          
			curQuad.m_index[i] = curQuad.m_index[i-1];
			curQuad.m_indexParent[i] = curQuad.m_indexParent[i-1];
		}

		// add the quad & remove from joint set
		hierachyQuads.push_back(curQuad);
		for( int i = 0; i < quadCurrentSize; i++ ) {
			setTodo.erase(curQuad.m_index[i]);
		}
	}
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <sstream>

#include "edge/anim/edgeanim_structs.h"
#include "edge/anim/edgeanim_common.h"

#include "edge/libedgeanimtool/libedgeanimtool_filewriter.h"
#include "edge/libedgeanimtool/libedgeanimtool_common.h"
#include "edge/libedgeanimtool/libedgeanimtool_skeleton.h"

#include "../shared/joints.h"
#include "skeleton.h"
#include "helpers.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

int GetHierarchyDepth( std::vector<const FCDSceneNode*>& joints, const FCDSceneNode* pAnimJoint )
{
	for( size_t jointLoop = 0; jointLoop < joints.size(); jointLoop++ )
	{
		const FCDSceneNode* pJoint = joints[jointLoop];
		if( pAnimJoint->GetParent() == pJoint )
		{
			return 1 + GetHierarchyDepth( joints, pJoint );
		}
	}
	return 0;
}

void PrintHierarchy( std::vector<const FCDSceneNode*>& joints )
{
	std::cout << std::endl << " Joint Hierarchy:" << std::endl;
	for( size_t jointLoop = 0; jointLoop < joints.size(); jointLoop++ )
	{
		const FCDSceneNode* pAnimJoint = joints[jointLoop];
		int depth = GetHierarchyDepth( joints, pAnimJoint );
		unsigned int jointNameHash = edgeAnimGenerateNameHash(pAnimJoint->GetName().c_str());

		std::cout << " " << jointLoop;
		for( int i = 0; i < depth; i++ ) { std::cout << "  "; }
		std::cout << "  " << pAnimJoint->GetName().c_str() << " = 0x" << std::hex << jointNameHash << std::dec << std::endl;
	}
	std::cout << std::endl;
}

//--------------------------------------------------------------------------------------------------
/**
    Retrieves the skeleton from the document.
**/
//--------------------------------------------------------------------------------------------------

void ExtractSkeleton( const FCDocument* pDocument, Skeleton& skeleton, const std::string& locoJointName, bool verbose, std::vector< std::string>* jointNames )
{
    // get the scene root
    const FCDSceneNode* pRoot = pDocument->GetVisualSceneInstance();
    EDGEERROR(pRoot);

    // gather joints
    std::vector<const FCDSceneNode*> joints;
	FindSkeletonJoints( pRoot, joints );

	// Verbose - Skeleton Hierarchy
	if( verbose ) { PrintHierarchy( joints ); }

    // evaluate base pose (TODO: FIX - at the moment evaluate at t=0.0 - this is wrong)
    float timeBasePose = 0.0f;
    std::vector<Joint> basePose;
	std::vector<bool> scaleCompensateFlags;
    std::vector<unsigned int> jointNameHashes;
    basePose.reserve(joints.size());
    jointNameHashes.reserve(joints.size());
	if( jointNames ) { jointNames->reserve(joints.size()); }
    for (size_t jointLoop = 0; jointLoop < joints.size(); jointLoop++) {
        // Iterate on all curves
        const FCDSceneNode* pAnimJoint = joints[jointLoop];
        FCDAnimatedList animateds;
        GetAnimateds(const_cast<FCDSceneNode*>(pAnimJoint), animateds);
    
        // Sample each animated, which changes the transform values directly
        for (FCDAnimatedList::iterator it = animateds.begin(); it != animateds.end(); ++it) {
            (*it)->Evaluate(timeBasePose);
        }

		// Warn if its not a joint - has been added as an intermediate node
		if( !pAnimJoint->GetJointFlag() ) {
			std::cout << "Warning: Intermediate Node " << pAnimJoint->GetName().c_str() << " added to joint hierarchy as it is parent to required joints." << std::endl;
		}

        // Retrieve the new transform matrix for the COLLADA scene node
        FMMatrix44 matrix = pAnimJoint->ToMatrix();

        // Now convert to a joint
        const float* pMatrix = (float*)matrix;      
        Joint tmpJoint = MatrixToJoint(pMatrix );
		basePose.push_back(tmpJoint);

		// Get scale compensation (Maya only for now)
		bool scaleCompensate = false;
		const FCDETechnique* mayaTechnique = pAnimJoint->GetExtra()->GetDefaultType()->FindTechnique(DAEMAYA_MAYA_PROFILE);
		if (mayaTechnique != NULL)
		{
			const FCDENode* flagNode = mayaTechnique->FindParameter(DAEMAYA_SEGMENTSCALECOMP_PARAMETER);
			if (flagNode != NULL)
			{
				scaleCompensate = FUStringConversion::ToBoolean(flagNode->GetContent());
			}
		}
		scaleCompensateFlags.push_back(scaleCompensate);

        // Compute the joint name hash
        unsigned int jointNameHash = edgeAnimGenerateNameHash(pAnimJoint->GetName().c_str());

        // Now make sure we don't already have it.. if we do, then we've got a collision
        for (size_t checkLoop = 0; checkLoop < jointNameHashes.size(); checkLoop++ ) {
            if ( jointNameHashes[ checkLoop ] == jointNameHash ) {
                std::cerr << "Joint name hash collision between '" << joints[checkLoop]->GetName().c_str() << "' and '" << pAnimJoint->GetName().c_str() << "'.";
                EDGEERROR_F();
            }
        }

        // Name does not collide.. add it
        jointNameHashes.push_back(jointNameHash);
		if( jointNames ) { jointNames->push_back(pAnimJoint->GetName().c_str()); }
    }

	// Now setup the locomotion joint
	skeleton.m_locoJointIndex = 0;
	if(locoJointName.size())
	{
		bool foundLocoJoint = false;
		unsigned int locoJointNameHash = edgeAnimGenerateNameHash( locoJointName.c_str() );
		for( unsigned int i = 0; i < jointNameHashes.size(); i++ )
		{
			if( jointNameHashes[i] == locoJointNameHash )
			{
				skeleton.m_locoJointIndex = i;
				foundLocoJoint = true;
				break;
			}
		}

		if( !foundLocoJoint )
		{
			std::cerr << "Failed to find locomotion joint: " << locoJointName << std::endl;
			EDGEERROR_F();
		}
	}

	// Now find the index of each joint's parent - the root's is -1

    // build map from FCollada object pointer to joint index
    std::map<const FCDSceneNode*, size_t> jointIndices;
    for (size_t i=0; i<joints.size(); ++i) {
        jointIndices[ joints[i] ] = i;
    }

    // build array of joint parent indices (-1 for no parent)
    std::vector<int> parentIndices(joints.size(), -1);
	std::vector<const FCDSceneNode*> rootJoints;
    for (size_t i=0; i<joints.size(); ++i) {
        const FCDSceneNode* pCurrent = joints[i];
        const FCDSceneNode* pParent = pCurrent->GetParent();

		std::map<const FCDSceneNode*, size_t>::const_iterator it = jointIndices.find(pParent);
		if( it == jointIndices.end() )
		{
			parentIndices[i] = -1;
			rootJoints.push_back( pCurrent );
		}
		else
		{
			parentIndices[i] = (int) it->second;
		}
    }

	// warn about multiple root joints
	if( rootJoints.size() != 1 )
	{
		for (size_t i=0; i<rootJoints.size(); ++i) {
			const FCDSceneNode* pRoot = rootJoints[i];
			std::cout << "Warning: Multiple Root Joints Detected: " << pRoot->GetName().c_str() << std::endl;
		}
	}

    // copy to output structure
	skeleton.m_numJoints = (unsigned int) joints.size();
	skeleton.m_numUserChannels = 0;
    skeleton.m_parentIndices = parentIndices;
    skeleton.m_basePose = basePose;
	skeleton.m_scaleCompensateFlags = scaleCompensateFlags;
    skeleton.m_jointNameHashes = jointNameHashes;
}

//--------------------------------------------------------------------------------------------------
/**
	Entire process for one file.
**/
//--------------------------------------------------------------------------------------------------

void ProcessSkeleton( const std::string& inputSkelFilename, const std::string& outputSkelFilename, const std::string& userChannelsFilename, const std::string& locoJointName, bool addJointNames, bool bigEndian, bool verbose )
{
	FCollada::Initialize();

    // load the collada skeleton file
	if( verbose ) { std::cout << "Loading Skeleton: " << inputSkelFilename << std::endl; }
    std::auto_ptr<FCDocument> pDocumentSkel(FCollada::NewTopDocument());
    if (!FCollada::LoadDocumentFromFile(pDocumentSkel.get(), FUStringConversion::ToFString(inputSkelFilename.c_str()))) {
		std::cerr << "Failed to load input skeleton file: " << inputSkelFilename << std::endl;
		EDGEERROR_F();
    }

    // extract the skeleton from the Collada doc
    Skeleton skeleton;
	std::vector< std::string > jointNames;
    ExtractSkeleton( pDocumentSkel.get(), skeleton, locoJointName, verbose, &jointNames );

	// parse user channel names
	std::vector<Edge::Tools::UserChannelInfo> userChannelInfoArray;
	if(userChannelsFilename.size())
	{
		Edge::Tools::ParseUserChannels(userChannelsFilename, userChannelInfoArray);
	}

	// user channel
	skeleton.m_numUserChannels = (unsigned int) userChannelInfoArray.size();
	skeleton.m_userChannelInfoArray = userChannelInfoArray;

	// joint custom data
	CustomDataCallback customDataCallback = NULL;
	CustomDataTable customDataTable;
	CustomDataJointNames customDataJointNames( jointNames );

	if( addJointNames )
	{
		customDataTable.m_CustomDataEntries.push_back( &customDataJointNames );
		customDataCallback = ExportCustomDataTable;
	}

    // export skeleton binary to file
    ExportSkeleton( outputSkelFilename, skeleton, bigEndian, customDataCallback, &customDataTable );
}

//--------------------------------------------------------------------------------------------------

void CustomDataJointNames::ExportData( FileWriter& fileWriter ) const
{
	// null-terminated string of joint names - separated with '/' token character
	for( std::vector< std::string >::const_iterator it = m_nameList.begin(); it != m_nameList.end(); it++ )
	{
		if( it != m_nameList.begin() )
		{
			fileWriter.Write( (unsigned char) '/' ); // uses '/' as a token separator
		}

		const char *cstr = (*it).c_str();
		while( *cstr )
		{
			unsigned char c = (unsigned char) *cstr++;
			fileWriter.Write( c );
		}
	}

	fileWriter.Write( (unsigned char) '\0' ); // null termination
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

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
#include "edge/libedgeanimtool/libedgeanimtool_animation.h"
#include "edge/libedgeanimtool/libedgeanimtool_skeleton.h"

#include "../shared/joints.h"
#include "skeleton.h"
#include "animation.h"
#include "helpers.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------
/**
	Look for any animated float channels that are specified in the bind skeleton's user channel list.
**/
//--------------------------------------------------------------------------------------------------

static void FindUserChannels(const FCDocument* pDocument, const Skeleton& bindSkeleton, FCDAnimatedList& anims)
{
	// work in progress.. arbitrary channels not supported yet
#if 0
	// Search through all animated channels.
	const FCDAnimationLibrary* pAnimLib = pDocument->GetAnimationLibrary();
	for(uint32_t i=0; i<pAnimLib->GetEntityCount(); i++)
	{
		const FCDAnimation* pAnim = pAnimLib->GetEntity(i);
		const FCDAnimationChannel* pChannel = pAnim->GetChannel(0);
		const fm::string& channelTarget = pChannel->GetTargetPointer();
	}
#endif

	// Now search through all animated morph targets. This seems to be necessary since I can't find a 
	// way of getting getting hold of a morph target from a FCDAnimation object. 
	const FCDControllerLibrary* pControllerLib = pDocument->GetControllerLibrary();
	for(uint32_t j=0; j<pControllerLib->GetEntityCount(); j++)
	{
		const FCDController* pController = pControllerLib->GetEntity(j);
		if(pController->IsMorph())
		{
			const FCDMorphController* pMorph = pController->GetMorphController();
			for(uint32_t k=0; k<pMorph->GetTargetCount(); k++)
			{
				const FCDMorphTarget* pMorphTarget = pMorph->GetTarget(k);
				const FCDAnimated* pAnim = pMorphTarget->GetWeight().GetAnimated();
				if(pAnim)
				{
					// We've found an animated blend shape weight. Create the hashed name pair 
					// (node, channel) where 'node' is the name of the controller, and 'shape' is the name of the 
					// target geometry.
					const fm::string& controllerName = pController->GetName();
					const fm::string& shapeName = pMorph->GetTarget(k)->GetGeometry()->GetName();
					fm::string channelName = controllerName + "." + shapeName;
					uint32_t nodeHash = edgeAnimGenerateNameHash(controllerName.c_str());
					uint32_t channelHash = edgeAnimGenerateNameHash(shapeName.c_str());

					// Now search for this pair in the list of user channels
					for(uint32_t l=0; l<bindSkeleton.m_numUserChannels; l++)
					{
						if(nodeHash == bindSkeleton.m_userChannelInfoArray[l].m_nodeNameHash &&
						   channelHash == bindSkeleton.m_userChannelInfoArray[l].m_channelNameHash)
						{
							anims[l] = const_cast<FCDAnimated*>(pAnim);
						}
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
/**
    Extract the animation data for the skeleton.

    @note Some of this code is based on FCDSceneNode::GenerateSampledMatrixAnimation().
    
    @note this is a non-const process - the transforms are changed by the sampling
          therefore, grab any information from the skeleton *before* doing this
          this should be reviewed
**/
//--------------------------------------------------------------------------------------------------

void ExtractAnimation( const FCDocument* pDocument, 
					   Animation& outputAnim, 
					   const Skeleton& bindSkeleton, 
					   const Skeleton& animSkeleton,
					   bool extractLocoDelta,
					   bool verbose )
{
    Animation anim;

    // Reset ouput
    anim.m_jointAnimations.clear();
	anim.m_jointAnimations.resize(bindSkeleton.m_numJoints);

	anim.m_userChannelAnimations.clear();
	anim.m_userChannelAnimations.resize(bindSkeleton.m_numUserChannels);

	// Collect the joint scene nodes in the Collada doc
	std::vector< const FCDSceneNode* > animJoints;
    const FCDSceneNode* pRoot = pDocument->GetVisualSceneInstance();
    EDGEERROR(pRoot);
	FindSkeletonJoints( pRoot, animJoints );
	if ( animJoints.size() != animSkeleton.m_numJoints ) {
		std::cerr << "Error mismatch in number of joints" << std::endl;
		EDGEERROR_F();
	}

    // Create a case senstive map to get the anim joints by name hash	
	std::map<unsigned int, const FCDSceneNode*> mapNameJoints;
    for (size_t jointLoop = 0; jointLoop < animSkeleton.m_numJoints; jointLoop++) {
        unsigned int name = animSkeleton.m_jointNameHashes[jointLoop];
        if (mapNameJoints.find(name) != mapNameJoints.end()) {  
            std::cout << "Warning: multiple joints with the same name hash (" << name << ")" << std::endl;
        }
        mapNameJoints[name] = animJoints[jointLoop];     
    }

	// get user channels
	FCDAnimatedList userAnims;
	userAnims.resize(bindSkeleton.m_numUserChannels);
	for(uint32_t i=0; i<bindSkeleton.m_numUserChannels; i++)
		userAnims[i] = NULL;
	FindUserChannels(pDocument, bindSkeleton, userAnims);

	// List of all animated objects
	FCDAnimatedList animateds;

	// Collect all FCDAnimated objects for joints present in both the bind skeleton and the animation
	for (size_t jointLoop = 0; jointLoop < bindSkeleton.m_numJoints; jointLoop++) {

		unsigned int jointName = bindSkeleton.m_jointNameHashes[jointLoop];
		std::map<unsigned int, const FCDSceneNode*>::const_iterator foundJoint = 
			mapNameJoints.find(jointName);

		if (foundJoint != mapNameJoints.end())  {
			const FCDSceneNode* pAnimJoint = foundJoint->second;
			GetAnimateds(const_cast<FCDSceneNode*>(pAnimJoint), animateds);
		}
	}

	// Collect all FCDAnimated objects for user channels present in both the bind skeleton and the animation
	for(uint32_t i=0; i<bindSkeleton.m_numUserChannels; i++)
	{
		if(userAnims[i])
			animateds.push_back(userAnims[i]);
	}

	// Global ordered list of sample times 
	std::vector<float> sampleKeys;

    // First pass: collect all possible sample times from our list of FCDAnimated objects
	for (FCDAnimatedList::iterator it = animateds.begin(); it != animateds.end(); ++it) {
		const FCDAnimationCurveListList& allCurves = (*it)->GetCurves();
		for (FCDAnimationCurveListList::const_iterator allCurveIt = allCurves.begin(); allCurveIt != allCurves.end(); ++allCurveIt) {
			const FCDAnimationCurveTrackList& curves = (*allCurveIt);
			if (curves.empty()) 
				continue;

			const FCDAnimationKey** pCurveKeys = curves.front()->GetKeys();
			size_t sampleKeyCount = sampleKeys.size();
			size_t curveKeyCount = curves.front()->GetKeyCount();

			// Merge this curve's keys in with the sample keys
			// This assumes both key lists are in increasing order
			size_t s = 0, c = 0;
			while (s < sampleKeyCount && c < curveKeyCount) {
				float sampleKey = sampleKeys[s], curveKey = pCurveKeys[c]->input;
				if (IsEquivalent(sampleKey, curveKey)) { 
					++s; 
					++c; 
				}
				else if (sampleKey < curveKey) { 
					++s; 
				}
				else {
					// Add this curve key to the sampling key list
					sampleKeys.insert(sampleKeys.begin() + (s++), pCurveKeys[c++]->input);
					sampleKeyCount++;
				}
			}

			// Add all the left-over curve keys to the sampling key list
			while (c < curveKeyCount) {
				sampleKeys.push_back(pCurveKeys[c++]->input);
			}
		}
	}

    // Animation time info
    anim.m_period = ComputePeriod(sampleKeys);
    if (sampleKeys.size()) {
        anim.m_startTime = sampleKeys.front();
        anim.m_endTime = sampleKeys.back();
    }
    else { 
        anim.m_startTime = 0.0f;
        anim.m_endTime = 0.0f;
    }

    // Second pass: actually generate / sample data
	if( verbose ) { std::cout << " Animated Joints" << std::endl; }
    for (size_t jointLoop = 0; jointLoop < bindSkeleton.m_numJoints; jointLoop++) {
        unsigned int jointName = bindSkeleton.m_jointNameHashes[jointLoop];
        size_t numAnimateds = 0;

        // Initialise with default : no data / item ignored
        anim.m_jointAnimations[jointLoop].m_jointName = jointName;
        anim.m_jointAnimations[jointLoop].m_jointWeight = 0.0f;

        // Is it present?
        std::map<unsigned int, const FCDSceneNode*>::const_iterator foundJoint = 
            mapNameJoints.find(jointName);
        if (foundJoint != mapNameJoints.end()) {
            // Give a weight of 1 to this joint 
            anim.m_jointAnimations[jointLoop].m_jointWeight = 1.0f;

            // Iterate on all curves
            const FCDSceneNode* pAnimJoint = foundJoint->second;
            FCDAnimatedList animateds;
            GetAnimateds(const_cast<FCDSceneNode*>(pAnimJoint), animateds);
			numAnimateds = animateds.size();
        
            // Reserve vectors
            anim.m_jointAnimations[jointLoop].m_rotationAnimation.reserve(sampleKeys.size());
            anim.m_jointAnimations[jointLoop].m_translationAnimation.reserve(sampleKeys.size());
            anim.m_jointAnimations[jointLoop].m_scaleAnimation.reserve(sampleKeys.size());
            
            // Sample the scene node transform
            for (std::vector<float>::const_iterator iteSampleKeys = sampleKeys.begin();
                  iteSampleKeys < sampleKeys.end();
                  iteSampleKeys++) {
                float sampleTime = *iteSampleKeys;

                // Sample each animated, which changes the transform values directly
                for (FCDAnimatedList::iterator it = animateds.begin(); it != animateds.end(); ++it) {
                    (*it)->Evaluate(sampleTime);
                }

                // Retrieve the new transform matrix for the COLLADA scene node
                FMMatrix44 matrix = pAnimJoint->ToMatrix();

                // Now convert to a joint
                const float* pMatrix = (float*)matrix;      
                Joint tmpJoint = MatrixToJoint(pMatrix );

                // And add keyframes for each channel
                AnimationKeyframe keyRot = { sampleTime - anim.m_startTime, tmpJoint.m_rotation };
                anim.m_jointAnimations[jointLoop].m_rotationAnimation.push_back(keyRot);

                AnimationKeyframe keyTrans = { sampleTime - anim.m_startTime, tmpJoint.m_translation };
                anim.m_jointAnimations[jointLoop].m_translationAnimation.push_back(keyTrans);

                AnimationKeyframe keyScale = { sampleTime - anim.m_startTime, tmpJoint.m_scale };
                anim.m_jointAnimations[jointLoop].m_scaleAnimation.push_back(keyScale);
            }
        }

		if( verbose ) { std::cout << "  " << jointLoop << "\t" << numAnimateds << " Channels" << std::endl; }
	}

	// generate / sample user data
    for (size_t userChannelLoop = 0; userChannelLoop < bindSkeleton.m_numUserChannels; userChannelLoop++) {
		unsigned int userChannelName = bindSkeleton.m_userChannelInfoArray[userChannelLoop].m_channelNameHash;
        
        // Initialise with default : no data / item ignored
        anim.m_userChannelAnimations[userChannelLoop].m_channelName = userChannelName;
        anim.m_userChannelAnimations[userChannelLoop].m_weight = 0.0f;

        // Is it present?
		if(userAnims[userChannelLoop] != NULL)
		{
            // Give a weight of 1 to this channel
            anim.m_userChannelAnimations[userChannelLoop].m_weight = 1.0f;

            // Reserve vectors
            anim.m_userChannelAnimations[userChannelLoop].m_animation.reserve(sampleKeys.size());

            // Sample the user channel
            for (std::vector<float>::const_iterator iteSampleKeys = sampleKeys.begin();
                  iteSampleKeys < sampleKeys.end();
                  iteSampleKeys++) {

				// check this is a float channel
				FCDAnimated* animated = userAnims[userChannelLoop];
				EDGEERROR(animated->GetValueCount() == 1);
				float value = *animated->GetValue(0);

				float sampleTime = *iteSampleKeys;
				userAnims[userChannelLoop]->Evaluate(sampleTime);

				Float4 val = {value,0,0,0};
				AnimationKeyframe keyUser = { sampleTime - anim.m_startTime, val };
				anim.m_userChannelAnimations[userChannelLoop].m_animation.push_back(keyUser);
            }
        }
    }

	// Animation locomotion deltas
	anim.m_enableLocoDelta = false;
	if (extractLocoDelta && sampleKeys.size()) {
		const unsigned int locoJointIndex = bindSkeleton.m_locoJointIndex;
		const size_t startFrame = 0;
		const size_t endFrame = anim.m_jointAnimations[ locoJointIndex ].m_rotationAnimation.size() - 1;

		FMQuaternion quatStart = FMQuaternion( (float*) &anim.m_jointAnimations[ locoJointIndex ].m_rotationAnimation[ startFrame ].m_keyData );
		FMVector3 transStart = FMVector3( (float*) &anim.m_jointAnimations[ locoJointIndex ].m_translationAnimation[ startFrame ].m_keyData );

		FMQuaternion quatEnd = FMQuaternion( (float*) &anim.m_jointAnimations[ locoJointIndex ].m_rotationAnimation[ endFrame ].m_keyData );
		FMVector3 transEnd = FMVector3( (float*) &anim.m_jointAnimations[ locoJointIndex ].m_translationAnimation[ endFrame ].m_keyData );

		FMQuaternion quatDelta = quatEnd * ( ~quatStart );
		FMVector3 transDelta = transEnd - transStart;

		anim.m_enableLocoDelta = true;
		anim.m_locoDeltaQuat[0] = quatDelta.x;
		anim.m_locoDeltaQuat[1] = quatDelta.y;
		anim.m_locoDeltaQuat[2] = quatDelta.z;
		anim.m_locoDeltaQuat[3] = quatDelta.w;
		anim.m_locoDeltaTrans[0] = transDelta.x;
		anim.m_locoDeltaTrans[1] = transDelta.y;
		anim.m_locoDeltaTrans[2] = transDelta.z;
		anim.m_locoDeltaTrans[3] = 0.0f;
	}
	
	// Update start / end time since we always remap the animation to the [0..[ range
    anim.m_endTime -= anim.m_startTime;
    anim.m_startTime = 0.0f;

	anim.m_numFrames = (unsigned int)ceil((anim.m_endTime / anim.m_period) + 0.5f);

    // copy to output structure
    outputAnim = anim;
}

//--------------------------------------------------------------------------------------------------
/**
    Entire process for one file.
**/
//--------------------------------------------------------------------------------------------------

void ProcessAnimation( const std::string& inputAnimFilename, 
					   const std::string& inputSkelFilename, 
					   const std::string& outputAnimFilename,
					   const std::string& locoJointName,
					   bool bigEndian,
					   bool stats,
					   bool verbose,
					   const CompressionInfo& compressionInfo,
					   bool optimize,
					   float optimizerTolerance)
{
	FCollada::Initialize();

	// load the skeleton binary resource
	void* pSkel = LoadFile( inputSkelFilename.c_str() );
	if( !pSkel ) {
		std::cerr << "Failed to load input skeleton file: " << inputSkelFilename << std::endl;
		EDGEERROR_F();
	}

	// extract the edge skeleton
	Skeleton skeletonSkel;
	ExtractSkeleton((EdgeAnimSkeleton*)pSkel, skeletonSkel);

	// load the collada animation file
	if( verbose ) { std::cout << "Loading Animation: " << inputAnimFilename << std::endl; }
    std::auto_ptr<FCDocument> pDocumentAnim(FCollada::NewTopDocument());
    if (!FCollada::LoadDocumentFromFile(pDocumentAnim.get(), FUStringConversion::ToFString(inputAnimFilename.c_str()))) {
		std::cerr << "Failed to load input animation file: " << inputAnimFilename << std::endl;
		EDGEERROR_F();
    }

	// extract the animation skeleton
    Skeleton skeletonAnim;
    ExtractSkeleton(pDocumentAnim.get(), skeletonAnim, locoJointName, verbose);

    // extract the animation - remapped to the bind skeleton (skeletonSkel)
    Animation boundAnimation;
	bool extractLocoDelta = ( locoJointName.size() > 0 );
    ExtractAnimation( pDocumentAnim.get(), boundAnimation, skeletonSkel, skeletonAnim, extractLocoDelta, verbose );
    
    // export the animation bound to the right skeleton
    ExportAnimation( outputAnimFilename, boundAnimation, skeletonSkel, bigEndian, stats, compressionInfo, optimize, optimizerTolerance );

	delete [] pSkel;
}

//--------------------------------------------------------------------------------------------------
/**
    Entire process for one additive animation file.
**/
//--------------------------------------------------------------------------------------------------

void ProcessAdditiveAnimation( const std::string& inputAnimBaseFilename, 
							   const std::string& inputAnimFilename,
                               const std::string& inputSkelFilename, 
							   const std::string& outputAnimFilename,
							   bool baseUsesFirstFrame,
							   const std::string& locoJointName,
							   bool bigEndian,
							   bool stats,
							   bool verbose,
							   const CompressionInfo& compressionInfo,
		   					   bool optimize,
							   float optimizerTolerance)
{
	FCollada::Initialize();

	// load the skeleton binary resource
	void* pSkel = LoadFile( inputSkelFilename.c_str() );
	if( !pSkel ) {
		std::cerr << "Failed to load input skeleton file: " << inputSkelFilename << std::endl;
		EDGEERROR_F();
	}

	// extract the edge skeleton
	Skeleton skeletonSkel;
	ExtractSkeleton((EdgeAnimSkeleton*)pSkel, skeletonSkel);
	
	// load the collada base animation file
	if( verbose ) { std::cout << "Loading Base Animation: " << inputAnimBaseFilename << std::endl; }
    std::auto_ptr<FCDocument> pDocumentBaseAnim(FCollada::NewTopDocument());
    if (!FCollada::LoadDocumentFromFile(pDocumentBaseAnim.get(), FUStringConversion::ToFString(inputAnimBaseFilename.c_str()))) {
		std::cerr << "Failed to load input base animation file: " << inputAnimBaseFilename << std::endl;
		EDGEERROR_F();
    }

	// extract the base animation skeleton
	Skeleton skeletonBaseAnim;
	ExtractSkeleton(pDocumentBaseAnim.get(), skeletonBaseAnim, locoJointName, verbose);

	// extract the base animation - remapped to the bind skeleton (skeletonSkel)
	Animation boundBaseAnimation;
	bool extractLocoDelta = ( locoJointName.size() > 0 );
	ExtractAnimation(pDocumentBaseAnim.get(), boundBaseAnimation, skeletonSkel, skeletonBaseAnim, extractLocoDelta, verbose);

	// load the collada animation file
	if( verbose ) { std::cout << "Loading Animation: " << inputAnimFilename << std::endl; }
    std::auto_ptr<FCDocument> pDocumentAnim(FCollada::NewTopDocument());
    if (!FCollada::LoadDocumentFromFile(pDocumentAnim.get(), FUStringConversion::ToFString(inputAnimFilename.c_str()))) {
		std::cerr << "Failed to load input animation file: " << inputAnimFilename << std::endl;
		EDGEERROR_F();
    }

	// extract the animation skeleton
	Skeleton skeletonAnim;
	ExtractSkeleton(pDocumentAnim.get(), skeletonAnim, locoJointName, verbose);

    // extract the animation - remapped to the bind skeleton (skeletonSkel)
    Animation boundAnimation;
    ExtractAnimation(pDocumentAnim.get(), boundAnimation, skeletonSkel, skeletonAnim, extractLocoDelta, verbose);

    // now create an animation with the deltas relative to base animation
    Animation boundDeltaAnimation;
    GenerateAdditiveAnimation(boundDeltaAnimation, boundBaseAnimation, boundAnimation, skeletonSkel, baseUsesFirstFrame);
    
    // export the delta animation bound to the right skeleton
	ExportAnimation( outputAnimFilename, boundDeltaAnimation, skeletonSkel, bigEndian, stats, compressionInfo, optimize, optimizerTolerance );

	delete [] pSkel;
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools

}   // namespace Edge

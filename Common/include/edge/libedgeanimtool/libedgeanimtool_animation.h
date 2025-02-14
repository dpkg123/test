/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEANIMTOOL_ANIMATION_H
#define LIBEDGEANIMTOOL_ANIMATION_H

//--------------------------------------------------------------------------------------------------

#include <vector>
#include "edge/libedgeanimtool/libedgeanimtool_common.h"
#include "edge/libedgeanimtool/libedgeanimtool_bitstream.h"
#include "edge/libedgeanimtool/libedgeanimtool_bitpacking.h"

#include "edge/anim/edgeanim_structs.h"
#include "edge/anim/edgeanim_common.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------

struct Skeleton;

//--------------------------------------------------------------------------------------------------

// Default global error tolerance
static const float  kDefaultTolerance = 0.001f;

// Default sample rate for non-sampled animation data
// Already sampled data should use its own sample rate
static const float	kDefaultSampleRate = 30.0f;

//--------------------------------------------------------------------------------------------------

struct JointFrameSet
{
    Float4 m_initialRData;
    Float4 m_initialTData;
    Float4 m_initialSData;

    unsigned int m_intraRFrame0;                // index of first intra rotation keyframe
    unsigned int m_intraTFrame0;                // index of first intra translation keyframe
    unsigned int m_intraSFrame0;                // index of first intra scale keyframe

	unsigned int m_numIntraRFrames;
	unsigned int m_numIntraTFrames;
	unsigned int m_numIntraSFrames;

    std::vector<unsigned char> m_hasRFrame;     // for each intra frame, 1 if this joint has a rotation keyframe, or 0
    std::vector<unsigned char> m_hasTFrame;     // for each intra frame, 1 if this joint has a translation keyframe, or 0
    std::vector<unsigned char> m_hasSFrame;     // for each intra frame, 1 if this joint has a scale keyframe, or 0
};

struct UserChannelFrameSet
{
    float m_initialData;

    unsigned int m_intraFrame0;                 // index of first intra keyframe
	unsigned int m_numIntraFrames;

    std::vector<unsigned char> m_hasFrame;      // for each intra frame, 1 if this channel has a keyframe, or 0
};

struct FrameSet
{
    unsigned int m_baseFrame;
    unsigned int m_numIntraFrames;

    std::vector<JointFrameSet> m_jointFrameSets;
    std::vector<UserChannelFrameSet> m_userChannelFrameSets;
};

struct AnimationKeyframe
{
    float   m_keyTime;  // keyframe time
    Float4  m_keyData;  // keyframe data
};

struct JointAnimation
{

    unsigned int                    m_jointName;            // name hash of joint
    float                           m_jointWeight;          // for partial animations / feathering

    std::vector<AnimationKeyframe>  m_rotationAnimation;    
    std::vector<AnimationKeyframe>  m_translationAnimation; 
    std::vector<AnimationKeyframe>  m_scaleAnimation;       
};

struct UserChannelAnimation
{

    unsigned int                    m_nodeName;             // name hash of node
    unsigned int                    m_channelName;          // name hash of channel
    float                           m_weight;               // for partial animations / feathering

    std::vector<AnimationKeyframe>  m_animation;    
};

struct Animation
{
	Animation() : m_startTime(0.0f), m_endTime(0.0f), m_period(0.0f), m_numFrames(0), m_enableLocoDelta(false) {}

    float                               m_startTime;                // start time
    float                               m_endTime;                  // end time
    float                               m_period;                   // sampling frequency
    unsigned int                        m_numFrames;                // number of frames

	bool								m_enableLocoDelta;
	Float4								m_locoDeltaQuat;			// locomotion rotation delta (start->end)
	Float4								m_locoDeltaTrans;			// locomotion translation delta (start->end)

    std::vector<JointAnimation>         m_jointAnimations;
    std::vector<UserChannelAnimation>   m_userChannelAnimations;
};

enum EdgeAnimCompressionType
{
	COMPRESSION_TYPE_NONE,				// uncompressed floats (non-rotations only)
	COMPRESSION_TYPE_SMALLEST_3,		// 48 bit quaternions (rotations only)
	COMPRESSION_TYPE_BITPACKED			// arbitrary per-component bit length
};

struct CompressionInfo
{
	EdgeAnimCompressionType	m_compressionTypeRotation;
	EdgeAnimCompressionType	m_compressionTypeTranslation;
	EdgeAnimCompressionType	m_compressionTypeScale;
	EdgeAnimCompressionType	m_compressionTypeUser;

	float					m_defaultToleranceRotation;			// default tolerance used by COMPRESSION_TYPE_BITPACKED
	float					m_defaultToleranceTranslation;		// default tolerance used by COMPRESSION_TYPE_BITPACKED
	float					m_defaultToleranceScale;			// default tolerance used by COMPRESSION_TYPE_BITPACKED
	float					m_defaultToleranceUser;				// default tolerance used by COMPRESSION_TYPE_BITPACKED

	std::vector<float>		m_jointTolerancesRotation;			// optional per-joint tolerances (if empty, default is used)
	std::vector<float>		m_jointTolerancesTranslation;		// optional per-joint tolerances (if empty, default is used)
	std::vector<float>		m_jointTolerancesScale;				// optional per-joint tolerances (if empty, default is used)
	std::vector<float>		m_userChannelTolerances;			// optional per-user channel tolerances (if empty, default is used)

	CompressionInfo() : 
		m_compressionTypeRotation(COMPRESSION_TYPE_SMALLEST_3), 
		m_compressionTypeTranslation(COMPRESSION_TYPE_NONE), 
		m_compressionTypeScale(COMPRESSION_TYPE_NONE), 
		m_compressionTypeUser(COMPRESSION_TYPE_NONE), 
		m_defaultToleranceRotation(kDefaultTolerance),
		m_defaultToleranceTranslation(kDefaultTolerance),
		m_defaultToleranceScale(kDefaultTolerance),
		m_defaultToleranceUser(kDefaultTolerance)
		{}
};

struct CompressedFrameSet
{
	unsigned int m_baseFrame;
	unsigned int m_numIntraFrames;

	std::vector<uint64_t>	m_initialRSmallest3;
	std::vector<Float4>		m_initialTRaw;
	std::vector<Float4>		m_initialSRaw;
	std::vector<float>		m_initialURaw;

	std::vector<uint8_t>	m_initialRBitpacked;
	std::vector<uint8_t>	m_initialTBitpacked;
	std::vector<uint8_t>	m_initialSBitpacked;
	std::vector<uint8_t>	m_initialUBitpacked;

	BitStream				m_intraBits;

	std::vector<uint64_t>	m_intraRSmallest3;
	std::vector<Float4>		m_intraTRaw;
	std::vector<Float4>		m_intraSRaw;
	std::vector<float>		m_intraURaw;

	std::vector<uint8_t>	m_intraRBitpacked;
	std::vector<uint8_t>	m_intraTBitpacked;
	std::vector<uint8_t>	m_intraSBitpacked;
	std::vector<uint8_t>	m_intraUBitpacked;

};

struct CompressedAnimation
{
	float					m_duration;					// duration
	float					m_sampleFrequency;			// sampling frequency
	unsigned int			m_numJoints;                // number of joints
	unsigned int			m_numFrames;                // number of frames
	unsigned int			m_numFrameSets;				// number of frame sets
	unsigned int			m_evalBufferSizeRequired;	// minimum size of buffer required for evaluation

	bool					m_enableLocoDelta;
	Float4					m_locoDeltaQuat;			// locomotion rotation delta (start->end)
	Float4					m_locoDeltaTrans;			// locomotion translation delta (start->end)

	bool					m_enableWeights;
	std::vector<uint8_t>	m_jointWeights;				// joint weights
	std::vector<uint8_t>	m_userChannelWeights;		// user channel weights

	unsigned int			m_numConstRChannels;		// number of constant rotation channels
	unsigned int			m_numConstTChannels;		// number of constant translation channels
	unsigned int			m_numConstSChannels;		// number of constant scale channels
	unsigned int			m_numConstUChannels;		// number of constant user channels
	unsigned int			m_numAnimRChannels;			// number of animated rotation channels
	unsigned int			m_numAnimTChannels;			// number of animated translation channels
	unsigned int			m_numAnimSChannels;			// number of animated scale channels
	unsigned int			m_numAnimUChannels;			// number of animated user channels

	CompressionInfo			m_compressionInfo;			// channel compression info

	std::vector<KeyframePackingSpec>	m_packingSpecsR;	// bit-packed rotation packing specs
	std::vector<KeyframePackingSpec>	m_packingSpecsT;	// bit-packed translation packing specs
	std::vector<KeyframePackingSpec>	m_packingSpecsS;	// bit-packed scale packing specs
	std::vector<KeyframePackingSpec>	m_packingSpecsU;	// bit-packed user packing specs

	std::vector<uint16_t>	m_constRChannels;
	std::vector<uint16_t>	m_constTChannels;
	std::vector<uint16_t>	m_constSChannels;
	std::vector<uint16_t>	m_constUChannels;
	std::vector<uint16_t>	m_animRChannels;
	std::vector<uint16_t>	m_animTChannels;
	std::vector<uint16_t>	m_animSChannels;
	std::vector<uint16_t>	m_animUChannels;

	std::vector<uint64_t>	m_constRSmallest3;
	std::vector<Float4>		m_constTRaw;
	std::vector<Float4>		m_constSRaw;
	std::vector<float>		m_constURaw;

	std::vector<uint8_t>	m_constRBitpacked;
	std::vector<uint8_t>	m_constTBitpacked;
	std::vector<uint8_t>	m_constSBitpacked;
	std::vector<uint8_t>	m_constUBitpacked;

	std::vector<CompressedFrameSet>		m_frameSets;		// 
};

//--------------------------------------------------------------------------------------------------

void    ExtractAnimation(const EdgeAnimAnimation* pAnim, const Skeleton& bindSkeleton, CompressedAnimation& animation);
float   ComputePeriod(const std::vector<float>& keyTimes, const double* pHints = NULL, unsigned int hintCount = 0);
void    GenerateAdditiveAnimation(Animation& outputAnim, const Animation& baseAnimation, const Animation& animation, const Skeleton& bindSkeleton, bool baseUsesFirstFrame);

void    ExportAnimation(const std::string outputAnimFilename, const Animation& animation, const Skeleton& bindSkeleton, bool bigEndian, bool writeStats, const CompressionInfo& compressionInfo, bool optimizeOuput = true, float optimizerTolerance = kDefaultTolerance, CustomDataCallback customDataCallback = 0, void* customData = 0 );
void    ExportAnimation(const std::string outputAnimFilename, std::ostream* pStream, const Animation& animation, const Skeleton& bindSkeleton, bool bigEndian, bool writeStats, const CompressionInfo& compressionInfo, bool optimizeOuput = true, float optimizerTolerance = kDefaultTolerance, CustomDataCallback customDataCallback = 0, void* customData = 0 );

void    ExportAnimation(const std::string outputAnimFilename, const CompressedAnimation& animation, bool bigEndian, bool writeStats, CustomDataCallback customDataCallback = 0, void* customData = 0 );
void    ExportAnimation(const std::string outputAnimFilename, std::ostream* pStream, const CompressedAnimation& animation, bool bigEndian, bool writeStats, CustomDataCallback customDataCallback = 0, void* customData = 0 );

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

#endif // LIBEDGEANIMTOOL_ANIMATION_H

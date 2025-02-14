/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef ANIMATION_H
#define ANIMATION_H

//--------------------------------------------------------------------------------------------------

namespace Edge
{
	namespace Tools
	{
		void ProcessAnimation( const std::string& inputAnimFilename, 
							   const std::string& inputSkelFilename, 
							   const std::string& outputAnimFilename,
							   const std::string& locoJointName,
							   bool bigEndian,
							   bool stats,
							   bool verbose,
							   const Edge::Tools::CompressionInfo& compressionInfo,
  		   					   bool optimize = true,
							   float optimizerTolerance = kDefaultTolerance);

		void ProcessAdditiveAnimation( const std::string& inputAnimBaseFilename, 
									   const std::string& inputAnimFilename,
									   const std::string& inputSkelFilename, 
									   const std::string& outputAnimFilename,
									   bool baseUsesFirstFrame,
									   const std::string& locoJointName,
									   bool bigEndian,
									   bool stats,
									   bool verbose,
									   const Edge::Tools::CompressionInfo& compressionInfo,
		  		   					   bool optimize = true,
									   float optimizerTolerance = kDefaultTolerance);
	}
}

//--------------------------------------------------------------------------------------------------

#endif // ANIMATION_H

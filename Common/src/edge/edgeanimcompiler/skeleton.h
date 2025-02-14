/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef SKELETON_H
#define SKELETON_H

//--------------------------------------------------------------------------------------------------

#include "edge/libedgeanimtool/libedgeanimtool_skeleton.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
	namespace Tools
	{
		class CustomDataJointNames : public CustomDataTableEntry
		{
		public:
			CustomDataJointNames( const std::vector< std::string >& nameList ) : m_nameList( nameList ) {}

			virtual	unsigned int	GetHashValue() const { return edgeAnimGenerateNameHash( "JointNames" );  }
			virtual	unsigned int	GetDataAlignment() const { return 4; }

			virtual void			ExportData( FileWriter& fileWriter ) const;

		private:
			std::vector< std::string > m_nameList;
		};

		void ExtractSkeleton( const FCDocument* pDocument, Skeleton& skeleton, const std::string& locoJointName, bool verbose, std::vector< std::string>* jointNames = NULL );
		void ProcessSkeleton( const std::string& inputSkelFilename, const std::string& outputSkelFilename, const std::string& userChannelFilename, const std::string& locoJointName, bool addJointNames, bool bigEndian, bool verbose );
	}
}

//--------------------------------------------------------------------------------------------------

#endif // SKELETON_H

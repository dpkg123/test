/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef GEOMCOMPILER_H
#define GEOMCOMPILER_H

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

#include <string>

// Set this bit if the input scene uses scaling (uniform or non-uniform) in its bone transforms.
// This will tell the runtime to use a slightly slower skinning path.
#define EDGE_GEOM_FLAG_ENABLE_SCALED_SKINNING           0x00000002
// Set this bit to output the inverse bind matrices to the file specified in the command line.
#define EDGE_GEOM_FLAG_OUTPUT_INV_BIND_MATRICES         0x00000004

struct EdgeGeomSegment;

namespace Edge
{
namespace Tools
{

// run the entire geometry export process for one file
void	GeomProcessFile( const std::string& inputFile, const std::string& outputFile, const std::string& invBindMatFile, const unsigned processingFlags = 0 );

// mainly for debug - writes out geometry data as a C++ header file
void DumpToFile( const char *filename, const EdgeGeomSegment *segments, uint32_t numSegments);

// write out the binary geometry file
void WriteGeomFile( const char *filename, const EdgeGeomSegment *segments, uint32_t numSegments);

}	// namespace Tools
}	// namespace Edge

#endif // GEOMCOMPILER_H

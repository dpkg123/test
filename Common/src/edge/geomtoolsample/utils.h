/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

// The utils.* files contain various utility functions used when processing geometry
// using libedgegeomtool.  They are general enough to be included in your own
// libedgegeomtool programs if necessary.

#if !defined(UTILS_H)
#define UTILS_H

#include "edge/libedgegeomtool/libedgegeomtool_wrap.h"

//
// NOTE: These file output routines are provided for reference only.  Feel free to discard them
// and output the Edge data structures using your own routines.
//

// Output a header file containing an array of pointers to segments
void DumpToFile( const char *filename, const EdgeGeomSegment *segments, uint32_t numSegments );
// Write a binary geometry file to disk.
void WriteGeomFile( const char *filename, const EdgeGeomSegment *segments, uint32_t numSegments );

#endif // UTILS_H

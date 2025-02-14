/* SCE CONFIDENTIAL
* PlayStation(R)Edge 1.2.0
* Copyright (C) 2009 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef POST_JOBHEADER_H
#define POST_JOBHEADER_H


// Header of an edgePostJob	
struct EdgePostJobHeader
{
	uint32_t guidData[4];			// debugger GUID
	uint32_t magicCode;				// == 0xda7aba5e
	uint32_t entryOffset;			// Offset to code entry
	uint32_t bssOffset;				// Offset to bss
	uint32_t bssSize;				// Bss size
	uint32_t ctorList;				// points to ctors
	uint32_t dtorList;				// points to dtors
};



#endif

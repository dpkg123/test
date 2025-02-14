/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __INFLATE_SEGS_FILE_QUEUE_ELEMENT_H__
#define __INFLATE_SEGS_FILE_QUEUE_ELEMENT_H__


//////////////////////////////////////////////////////////////////////////

/** 
 * @brief	One entry the queue which tells the Inflate Segs File Task to
 *			decompress a segs file
 *
 * @note	The Inflate Segs File Task will process an
 *			InflateSegsFileQueueElement and add queue entries (64K each
 *			typically) to the Inflate Queue which are then decompressed
 *			by the Inflate Task(s)
 */

typedef struct
{	// align 16, 32 bytes
	uint32_t		m_eaSegsFileData;				// Effective Address of segs file data, 16 aligned
	uint32_t		m_segsFileSize;					// size of the input segs file
	uint32_t		m_eaOutputUncompBuff;			// Effective Address of output buffer,
	uint32_t		m_outputBuffMaxSize;			// maximum size for the data sent to the output buffer
	uint32_t		m_eaInflateQueue;				// Effective Address of the Inflate Queue
	uint32_t		m_eaNumElementsToDecompress;	// Effective Address of counter of #elements to decompress
	uint32_t		m_eaEventFlag;					// Effective Address of eventFlag to trigger when all uncompressed data is stored
	uint16_t		m_eventFlagBits;				// value to set to the event flag
	uint16_t		m_segsHeaderAndTocSize;			// size of the header+TOC of the segs file
} InflateSegsFileQueueElement __attribute__((aligned(16)));

//////////////////////////////////////////////////////////////////////////

enum
{
	kInflateSegsFileTaskMaxHeaderAndTocSize = 32*1024,	//The maximum size of the header and TOC that the Inflate Segs Task can cope with
};

//////////////////////////////////////////////////////////////////////////


#endif // __INFLATE_SEGS_FILE_QUEUE_ELEMENT_H__

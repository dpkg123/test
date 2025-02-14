/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_ZLIB_STREAMING_SAMPLE_H__
#define __EDGE_ZLIB_STREAMING_SAMPLE_H__

//////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <cell/spurs/event_flag_types.h>
#include <time.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus

//////////////////////////////////////////////////////////////////////////
//
// Defines
//
//////////////////////////////////////////////////////////////////////////

struct StreamingQueueEntry;
struct StreamingReturnData;
typedef void (*StreamingCallBack)(	const StreamingQueueEntry* pStreamingQueueEntry,
									StreamingReturnData* pStreamingReturnData );

typedef void* EdgeZlibInflateQHandle;
struct CellSpurs;
struct _Ppu2PpuQueue;

//////////////////////////////////////////////////////////////////////////
//
// Structures
//
//////////////////////////////////////////////////////////////////////////

struct StreamingData/*NOT ON STACK!*/
{
	CellSpursEventFlag/*NOT ON STACK!*/	m_eventFlag;
    uint32_t/*NOT ON STACK!*/           m_numElementsToDecompress;
    CellSpurs*                          m_pSpurs;
	EdgeZlibInflateQHandle				m_inflateQueueHandle;
	_Ppu2PpuQueue*						m_pStreamingQueue;
	pthread_t							m_streamingThreadId;
} __attribute__((__aligned__(CELL_SPURS_EVENT_FLAG_ALIGN)));

struct StreamingQueueEntry
{   // 16 aligned, size is multiple of 16
	uint32_t							m_streamingQueueCommand;	// decompress or shut down
    StreamingData/*NOT ON STACK!*/*     m_pStreamingData;         
    const char*                         m_pSegsFileName;          // path & name & extension
    uint32_t/*NOT ON STACK!*/*          m_pNumElementsToDecompress;
    CellSpursEventFlag/*NOT ON STACK!*/*m_pEventFlag;
	StreamingCallBack					m_streamingCallBack;		// user callBack when file decompressed
	void*								m_pCallBackData;			// ptr to user data to/from callBack.
    uint32_t                            m_pad;
} __attribute__((__aligned__(16)));

// additional data set by streaming code after file decompressed and sent to user callback
struct StreamingReturnData
{
    uint32_t        m_totalUncompressedSize;
	unsigned char*  m_pOutputUncompBuff;
	clock_t         m_clockBeginDecompress;
	clock_t         m_clockEndDecompress;
    uint32_t        m_segsFileSize;
    uint32_t        m_numSegments;
    uint32_t        m_numNonCompressedSegs;
};


//////////////////////////////////////////////////////////////////////////
//
// Prototypes
//
//////////////////////////////////////////////////////////////////////////

void initStreaming(	StreamingData/*NOT ON STACK!*/*		pStreamingData,
					CellSpurs*							pSpurs,
					EdgeZlibInflateQHandle				inflateQueueHandle,
					uint32_t							numStreamingQueueEntries );

void shutDownStreaming( StreamingData/*NOT ON STACK!*/* pStreamingData );

void startStreamingDecompressInPlace(	StreamingData/*NOT ON STACK!*/*		pStreamingData,
										const char*							pSegsFileName,
										StreamingCallBack					streamingCallBack,
										void*								pCallBackData );


//////////////////////////////////////////////////////////////////////////
//
// Inline code
//
//////////////////////////////////////////////////////////////////////////

inline uint32_t RoundUp( uint32_t value, uint32_t alignment )
{
	assert( ((alignment & (alignment - 1)) == 0) && alignment );

	return (value + (alignment - 1)) & ~(alignment - 1);
};

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __EDGE_ZLIB_STREAMING_SAMPLE_H__ */

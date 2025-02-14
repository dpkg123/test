/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_ZLIB_LOCALMEMORY_STREAMING_SAMPLE_PPU2PPU_QUEUE_H__
#define __EDGE_ZLIB_LOCALMEMORY_STREAMING_SAMPLE_PPU2PPU_QUEUE_H__

//////////////////////////////////////////////////////////////////////////

#include <stdint.h>

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus


//////////////////////////////////////////////////////////////////////////
//
// Defines
//
//////////////////////////////////////////////////////////////////////////

enum // error codes
{
    kPpu2PpuQueue_queueMustExist = 1,
    kPpu2PpuQueue_entrySizeMustBeNonZero,
    kPpu2PpuQueue_numQueueEntriesMustBeNonZero,
    kPpu2PpuQueue_queueBufferAllocFailed,
    kPpu2PpuQueue_queueBufferIsMissing,
    kPpu2PpuQueue_queueEntryMustExist,
    kPpu2PpuQueue_memcpyFailed,
    kPpu2PpuQueue_conditionalVariableSignalFailed_butEntryPushedOnQueue,
    kPpu2PpuQueue_mutexUnlockFailed_butEntryPushedOnQueue,
    kPpu2PpuQueue_conditionalVariableSignalFailed_butEntryPoppedFromQueue,
    kPpu2PpuQueue_mutexUnlockFailed_butEntryPoppedFromQueue
    // Note: other error codes from low level system functions may be returned
};


//////////////////////////////////////////////////////////////////////////
//
// Structures
//
//////////////////////////////////////////////////////////////////////////

#define _PPU_2_PPU_QUEUE_ALIGN	4

struct _Ppu2PpuQueue   // PPU-to-PPU queue
{   // queue buffer location and sizes (no alignment or size restrictions)
    void*       m_pQueueBuf;            // pointer to queue buffer
    void*       m_pQueueBufEnd;         // pointer to end(exclusive) of queue buffer
    uint32_t    m_queueEntrySize;       // size of each queue entry
    uint32_t    m_numQueueEntries;      // number of queue entries allocated

    // how full queue is
    void*       m_pQueueEntryToPush;    // pointer to next queue entry to push to queue
    void*       m_pQueueEntryToPop;     // pointer to next queue entry to pop from queue
    bool        m_queueEmpty;
    bool        m_queueFull;

    // mutex and condition variables
	sys_mutex_t m_mutexId;              // limit access to a thread at a time
	sys_cond_t  m_condId_notEmpty;      // set if we just pushed an entry to the queue
	sys_cond_t  m_condId_notFull;       // set if we just popped an entry from the queue
} __attribute__((__aligned__(_PPU_2_PPU_QUEUE_ALIGN)));


//////////////////////////////////////////////////////////////////////////
//
// Prototypes
//
//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueueInitialize(	_Ppu2PpuQueue*	pQueue,					// pointer to queue identifier
                                    uint32_t        queueEntrySize,         // size of each queue entry
                                    uint32_t        numQueueEntries );      // number of queue entries allocated

//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueueShutDown(	_Ppu2PpuQueue*		pQueue );			// pointer to queue identifier

//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueuePush(	_Ppu2PpuQueue*	pQueue,				// pointer to queue identifier
	                        const void*     pQueueEntry,        // ptr to queue entry to push onto queue
                            usecond_t       timeout );          // timeout in microseconds, or 0 to disable

uint32_t _ppu2PpuQueuePop(	_Ppu2PpuQueue*	pQueue,				// pointer to queue identifier
	                        void*           pQueueEntry,        // ptr where to store popped queue entry
                            usecond_t       timeout );          // timeout in microseconds, or 0 to disable

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __EDGE_ZLIB_LOCALMEMORY_STREAMING_SAMPLE_PPU2PPU_QUEUE_H__ */

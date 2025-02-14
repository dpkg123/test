/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

//////////////////////////////////////////////////////////////////////////
//
// This ppu2ppu_queue code has been written to provide a generic queue
//  for PPU to PPU communication.  If it has to wait for a queue entry
//  to be available then it will wait, while allowing other threads to execute,
//  and will wake up when the queue entry is available.
//
//////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdint.h>
#include <sys/synchronization.h>
#include <stdlib.h>
#include <string.h>

#include "ppu2ppu_queue.h"

//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueueInitialize(	_Ppu2PpuQueue*	pQueue,					// pointer to queue identifier
                                    uint32_t        queueEntrySize,       // size of each queue entry, multiple of 16
                                    uint32_t        numQueueEntries )     // number of queue entries allocated
{
    // ensure queue exists
    if( pQueue == NULL )
    {
		printf("_ppu2PpuQueueInitialize error: queue must exist\n");
        return kPpu2PpuQueue_queueMustExist;
    }
    
    // ensure valid entry size
    if( queueEntrySize == 0 )
    {
		printf("_ppu2PpuQueueInitialize error: entry size must be non-zero\n");
        return kPpu2PpuQueue_entrySizeMustBeNonZero;
    }
    
    // ensure valid num queue entries
    if( numQueueEntries == 0 )
    {
		printf("_ppu2PpuQueueInitialize error: #queue entries must be non-zero\n");
        return kPpu2PpuQueue_numQueueEntriesMustBeNonZero;
    }    

	//////////////////////////////////////////////////////////////////////////
	//
	//	Allocate queue buffer
	//
	//////////////////////////////////////////////////////////////////////////

    uint32_t bufferSize = queueEntrySize * numQueueEntries;
    // Note: queue buffer has no alignment requirement, but to increase speed
    //  and ease of debugging it is aligned to 16 here
	void* pQueueBuf = memalign( 16, bufferSize );
    if( pQueueBuf == NULL )
    {
		printf("_ppu2PpuQueueInitialize error: queue buffer allocation failed\n");
        return kPpu2PpuQueue_queueBufferAllocFailed;
    }
   
	//////////////////////////////////////////////////////////////////////////
	//
	//	Create mutex and condition variables
	//
	//////////////////////////////////////////////////////////////////////////

    sys_mutex_attribute_t mutexAtrib;
    sys_mutex_attribute_initialize( mutexAtrib );
	int ret = sys_mutex_create( &pQueue->m_mutexId, &mutexAtrib );
	if( CELL_OK != ret )
	{
		printf("_ppu2PpuQueueInitialize error: mutex create failed\n");
        free(pQueueBuf);
        return ret;
    }

    sys_cond_attribute_t condAtrib;
    sys_cond_attribute_initialize( condAtrib );
	ret = sys_cond_create( &pQueue->m_condId_notEmpty, pQueue->m_mutexId, &condAtrib );
	if( CELL_OK != ret )
	{
		printf("_ppu2PpuQueueInitialize error: conditional variable create failed\n");
        (void)sys_mutex_destroy( pQueue->m_mutexId );
        free(pQueueBuf);
        return ret;
    }

	ret = sys_cond_create( &pQueue->m_condId_notFull,  pQueue->m_mutexId, &condAtrib );
	if( CELL_OK != ret )
	{
		printf("_ppu2PpuQueueInitialize error: conditional variable create failed\n");
        (void)sys_cond_destroy( pQueue->m_condId_notEmpty );
        (void)sys_mutex_destroy( pQueue->m_mutexId );
        free(pQueueBuf);
        return ret;
    }

	//////////////////////////////////////////////////////////////////////////
	//
	//	Set PPU-to-PPU queue data
	//
	//////////////////////////////////////////////////////////////////////////

    pQueue->m_pQueueBuf           = pQueueBuf;           
    pQueue->m_pQueueBufEnd        = (void*)( (uint32_t)pQueueBuf + bufferSize );
    pQueue->m_queueEntrySize      = queueEntrySize;
    pQueue->m_numQueueEntries     = numQueueEntries;

    pQueue->m_pQueueEntryToPush   = pQueueBuf;
    pQueue->m_pQueueEntryToPop    = pQueueBuf;
    pQueue->m_queueEmpty          = true;
    pQueue->m_queueFull           = false;

	return CELL_OK;
}

//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueueShutDown(	_Ppu2PpuQueue* pQueue )				// pointer to queue identifier
{
    // ensure queue exists
    if( pQueue == NULL )
    {
		printf("_ppu2PpuQueueShutDown error: queue must exist\n");
        return kPpu2PpuQueue_queueMustExist;
    }

	//////////////////////////////////////////////////////////////////////////
	//
	//	Destroy mutex and condition variables (in reverse order)
	//
	//////////////////////////////////////////////////////////////////////////

    int ret = sys_cond_destroy( pQueue->m_condId_notFull );
    if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueueShutDown error: conditional variable destroy failed\n");
        return ret;
    }

    ret = sys_cond_destroy( pQueue->m_condId_notEmpty );
    if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueueShutDown error: conditional variable destroy failed\n");
        return ret;
    }

    ret = sys_mutex_destroy( pQueue->m_mutexId );
    if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueueShutDown error: mutex variable destroy failed\n");
        return ret;
    }


	//////////////////////////////////////////////////////////////////////////
	//
	//	Free queue buffer, and clear PPU-to-PPU queue
	//
	//////////////////////////////////////////////////////////////////////////

	// ensure queue buffer not already free'd
	if( pQueue->m_pQueueBuf == NULL )
    {
		printf("_ppu2PpuQueueShutDown error: queue buffer is missing\n");
        return kPpu2PpuQueue_queueBufferIsMissing;
    }
		
    free( pQueue->m_pQueueBuf );

	memset( pQueue, 0, sizeof(_Ppu2PpuQueue) );

    return CELL_OK;
}

//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueuePush(	_Ppu2PpuQueue*	pQueue,				// pointer to queue identifier
	                        const void*     pQueueEntry,        // ptr to queue entry to push onto queue, 16 aligned
                            usecond_t       timeout )           // timeout in microseconds, or 0 to disable
{
    // ensure queue exists
    if( pQueue == NULL )
    {
		printf("_ppu2PpuQueuePush error: queue must exist\n");
        return kPpu2PpuQueue_queueMustExist;
    }

    // ensure queue entry exists
    if( pQueueEntry == NULL )
    {
		printf("_ppu2PpuQueuePush error: queue entry must exist\n");
        return kPpu2PpuQueue_queueEntryMustExist;
    }

    // Lock the mutex and enter the critical section.
    // If another thread already has locked this mutex, then we will sleep until code returns and we have it locked.
    // If sleeping then other threads execute.
	int ret = sys_mutex_lock( pQueue->m_mutexId, timeout );
	if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueuePush error: mutex lock failed\n");
        return ret;
    }
	
    // We now have exclusive access to this mutex

    while( pQueue->m_queueFull )
    {   // queue is full

        // Wait until the m_condId_notFull condition variable is set, which is done
        //  when an entry is popped from a queue.
        // Note: the mutex is temporarily released while we wait, and when this code wakes up,
        //  it automatically re-acquires the lock on the mutex.
	    ret = sys_cond_wait( pQueue->m_condId_notFull, timeout );
	    if( CELL_OK != ret )
        {
			printf("_ppu2PpuQueuePush error: conditional variable wait failed\n");
            (void)sys_mutex_unlock( pQueue->m_mutexId );
            return ret;
        }

        // We have woken up and have re-acquired the mutex exclusively.
        // But we have to re-check that a queue entry slot is free
        //  since another thread might have just filled it,
        //  and to avoid any possible "spurious wakeup".
    }

    // Push queue entry to queue
    if( memcpy( pQueue->m_pQueueEntryToPush, pQueueEntry, pQueue->m_queueEntrySize ) != pQueue->m_pQueueEntryToPush )
    {
		printf("_ppu2PpuQueuePush error: memcpy failed\n");
    	(void)sys_mutex_unlock( pQueue->m_mutexId );
        return kPpu2PpuQueue_memcpyFailed;
    }

    // Advance pointer.
    pQueue->m_pQueueEntryToPush = (void*)( (uint32_t)pQueue->m_pQueueEntryToPush + pQueue->m_queueEntrySize );

    // Check to wrap from end to start of buffer.
    if( pQueue->m_pQueueEntryToPush == pQueue->m_pQueueBufEnd )
    {   // we need to wrap from end to start of buffer

        pQueue->m_pQueueEntryToPush = pQueue->m_pQueueBuf;
    }

    // Declare the buffer is not empty
    pQueue->m_queueEmpty = false;

    if( pQueue->m_pQueueEntryToPush == pQueue->m_pQueueEntryToPop )
    {   // queue is now full

        pQueue->m_queueFull = true;
    }

    // NOTE: we have to update the queue data before waking up another thread.
    // But once we update the queue data, we can't safely "undo" setting the data.
    // Therefore, in the case that either the conditional signal or mutex unlock failed,
    //  we return an error but the entry was pushed onto the queue.
    
    // Set the condition variable declaring the buffer is not empty,
    //  which will wake up only one thread that is waiting to pop a queue entry from it
	ret = sys_cond_signal( pQueue->m_condId_notEmpty );
	if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueuePush error: conditional variable signal failed\n");
    	(void)sys_mutex_unlock( pQueue->m_mutexId );
        return kPpu2PpuQueue_conditionalVariableSignalFailed_butEntryPushedOnQueue;
    }

    // unlock the mutex and exit the critical section
	ret = sys_mutex_unlock( pQueue->m_mutexId );
	if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueuePush error: mutex unlock failed\n");
        return kPpu2PpuQueue_mutexUnlockFailed_butEntryPushedOnQueue;
    }

    return CELL_OK;
}

//////////////////////////////////////////////////////////////////////////

uint32_t _ppu2PpuQueuePop(	_Ppu2PpuQueue*	pQueue,				// pointer to queue identifier
	                        void*           pQueueEntry,        // ptr where to store popped queue entry, 16 aligned
                            usecond_t       timeout )           // timeout in microseconds, or 0 to disable
{
    // ensure queue exists
    if( pQueue == NULL )
    {
		printf("_ppu2PpuQueuePop error: queue must exist\n");
        return kPpu2PpuQueue_queueMustExist;
    }

    // ensure queue entry exists
    if( pQueueEntry == NULL )
    {
		printf("_ppu2PpuQueuePop error: queue entry must exist\n");
        return kPpu2PpuQueue_queueEntryMustExist;
    }

    // Lock the mutex and enter the critical section.
    // If another thread already has locked this mutex, then we will sleep until code returns and we have it locked.
    // If sleeping then other threads execute.
	int ret = sys_mutex_lock( pQueue->m_mutexId, timeout );
	if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueuePop error: mutex lock failed\n");
        return ret;
    }

    // We now have exclusive access to this mutex

    while( pQueue->m_queueEmpty )
    {   // queue is empty

        // Wait until the m_condId_notEmpty condition variable is set, which is done
        //  when an entry is pushed to a queue.
        // Note: the mutex is temporarily released while we wait, and when this code wakes up,
        //  it automatically re-acquires the lock on the mutex.
	    ret = sys_cond_wait( pQueue->m_condId_notEmpty, timeout );
	    if( CELL_OK != ret )
        {
			printf("_ppu2PpuQueuePop error: conditional variable wait failed\n");
            (void)sys_mutex_unlock( pQueue->m_mutexId );
            return ret;
        }

        // We have woken up and have re-acquired the mutex exclusively.
        // But we have to re-check that a queue entry is available
        //  since another thread might have just taken it,
        //  and to avoid any possible "spurious wakeup".
    }

    // Pop queue entry from queue
    if( memcpy( pQueueEntry, pQueue->m_pQueueEntryToPop, pQueue->m_queueEntrySize ) != pQueueEntry )
    {
		printf("_ppu2PpuQueuePop error: memcpy failed\n");
    	(void)sys_mutex_unlock( pQueue->m_mutexId );
        return kPpu2PpuQueue_memcpyFailed;
    }

    // Advance pointer.
    pQueue->m_pQueueEntryToPop = (void*)( (uint32_t)pQueue->m_pQueueEntryToPop + pQueue->m_queueEntrySize );

    // Check to wrap from end to start of buffer.
    if( pQueue->m_pQueueEntryToPop == pQueue->m_pQueueBufEnd )
    {   // we need to wrap from end to start of buffer

        pQueue->m_pQueueEntryToPop = pQueue->m_pQueueBuf;
    }

    // Declare the buffer is not full
    pQueue->m_queueFull = false;

    if( pQueue->m_pQueueEntryToPush == pQueue->m_pQueueEntryToPop )
    {   // queue is now empty

        pQueue->m_queueEmpty = true;
    }

    // NOTE: we have to update the queue data before waking up another thread.
    // But once we update the queue data, we can't safely "undo" setting the data.
    // Therefore, in the case that either the conditional signal or mutex unlock failed,
    //  we return an error but the entry was popped from the queue.
    
    // Set the condition variable declaring the buffer is not full,
    //  which will wake up only one thread that is waiting to push a queue entry to it
	ret = sys_cond_signal( pQueue->m_condId_notFull );
	if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueuePop error: conditional variable signal failed\n");
    	(void)sys_mutex_unlock( pQueue->m_mutexId );
        return kPpu2PpuQueue_conditionalVariableSignalFailed_butEntryPoppedFromQueue;
    }

    // unlock the mutex and exit the critical section
	ret = sys_mutex_unlock( pQueue->m_mutexId );
	if( CELL_OK != ret )
    {
		printf("_ppu2PpuQueuePush error: mutex unlock failed\n");
        return kPpu2PpuQueue_mutexUnlockFailed_butEntryPoppedFromQueue;
    }

    return CELL_OK;
}

//////////////////////////////////////////////////////////////////////////

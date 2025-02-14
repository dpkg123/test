/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_ZLIB_IN_PLACE_SAMPLE_INFLATE_SEGS_FILE_H__
#define __EDGE_ZLIB_IN_PLACE_SAMPLE_INFLATE_SEGS_FILE_H__

#ifdef __cplusplus
extern "C" 
{
#endif // __cplusplus


//////////////////////////////////////////////////////////////////////////

typedef void* InflateSegsFileQHandle;
typedef void* EdgeZlibInflateQHandle;

//////////////////////////////////////////////////////////////////////////

extern InflateSegsFileQHandle CreateInflateSegsFileQueue(
			CellSpursTaskset*					pTaskSet,
			uint32_t							maxNumQueueEntries,
			void*								pBuffer,
			uint32_t							bufferSize );

extern uint32_t GetInflateSegsFileQueueSize( uint32_t maxNumQueueEntries );

extern uint32_t GetInflateSegsFileTaskContextSaveSize( void );

extern CellSpursTaskId CreateInflateSegsFileTask(
		CellSpursTaskset*			pTaskSet,
		void*						pTaskContext,
		InflateSegsFileQHandle		handle );

extern void AddInflateSegsFileQueueElement(
		InflateSegsFileQHandle		inflateSegsFileQHandle,
		const unsigned char*		pSegsFileData,
		uint32_t					segsFileSize,
		void*						pOutputUncompBuff,
		uint32_t					outputBuffMaxSize,
		EdgeZlibInflateQHandle		inflateQHandle,
		uint32_t*					pWorkToDoCounter,
		CellSpursEventFlag*			pEventFlag,
		uint16_t					eventFlagBits );

extern void ShutdownInflateSegsFileQueue( InflateSegsFileQHandle handle );

//////////////////////////////////////////////////////////////////////////

enum
{
	kInflateSegsFileAlign = 128,	//The Inflate Segs File Queue must have at least this alignment
};

//////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif // __cplusplus

#endif	//__EDGE_ZLIB_IN_PLACE_SAMPLE_INFLATE_SEGS_FILE_H__

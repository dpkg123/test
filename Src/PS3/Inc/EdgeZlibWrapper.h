/* SCE CONFIDENTIAL
 * PLAYSTATION(R)Edge 0.3.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGEZLIBWRAPPER_H__
#define __EDGEZLIBWRAPPER_H__

#include <cell/spurs.h>

//////////////////////////////////////////////////////////////////////////

/// Set SPURS instance for Edge Zlib.
void edge_zlib_set_spurs(CellSpurs* pSpurs);

/// Set Taskset for Zlib task(s) -- optionally, call sample_zlib_create_taskset instead.
void edge_zlib_set_taskset_and_start(CellSpursTaskset* pTaskset, int numTasks=1);

/// Create dedicated Taskset for Zlib tasks.
CellSpursTaskset* edge_zlib_create_taskset_and_start(int numTasks=1);

/// Shutdown Edge Zlib.
void edge_zlib_shutdown();

/// Inflate a single compressed buffer. It must be less than 64KB.
/// Up to 16 deflation requests can be queued up at one time.
/// @return <0 if there are already 16 jobs queued up. Otherwise, handle for use with edge_zlib_wait.
int edge_zlib_inflate(const void* CompressedBuffer, int CompressedSize,
						void* UncompressedBuffer, int UncompressedSize);

/// Instead of uncompressing from source to dest, just memcpy using SPU DMA.
/// @return <0 if there are already 16 jobs queued up. Otherwise, handle for use with edge_zlib_wait.
int edge_zlib_memcpy(const void* SourceBuffer, void* DestBuffer, int Size);

/// Wait for the given deflation process to complete.
void edge_zlib_wait(int handle);

//////////////////////////////////////////////////////////////////////////

#endif

/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef EDGE_YVV_SAMPLE_MEMORY_LAYOUT_H
#define EDGE_YVV_SAMPLE_MEMORY_LAYOUT_H

#include <edge/edge_dma.h>
#include <edge/post/edgepost_direction_lock.h>
#include <edge/post/edgepost_inplace_transpose.h>

/*
 * We use Edge Post in-place transpose and thus need to have a memory layout for it as well.
 */

#define ALIGNED(a) __attribute__((aligned(a)))

#if YVV_MODE == YVV_XRGB
#define LINE_WIDTH			1280
#define BUFFER_LINES		16
#define TEMP_LINES			6
#else
#define LINE_WIDTH			2048
#define BUFFER_LINES		8
#define TEMP_LINES			4
#endif

#define BUFFER_LINE_MASK	(BUFFER_LINES-1)
#define YVV_TRANSPOSE_BLOCK_SIZE EDGEPOST_TRANSPOSE_MAX_BLOCK_SIZE

typedef struct YvvMemoryLayout
{
	uint32_t LineBuffer[BUFFER_LINES][LINE_WIDTH] ALIGNED(128);
	float FloatBuffer[TEMP_LINES][LINE_WIDTH*4] ALIGNED(16);
	EdgeDmaListElement DmaList[2*BUFFER_LINES][LINE_WIDTH/YVV_TRANSPOSE_BLOCK_SIZE] ALIGNED(8); //each buffer line gets its own get and put lists.
} YvvMemoryLayout;

typedef union MemoryLayout{
	YvvMemoryLayout yvv;
	EdgePostInplaceTranposeMemoryLayout transpose;
} ALIGNED(128) MemoryLayout;

#endif

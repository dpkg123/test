/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <spu_intrinsics.h>
#include <cell/dma.h>
#include <cell/spurs/common.h>
#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_context.h>
#include <edge/dxt/edgedxt.h>

extern "C" 
void cellSpursJobMain2(CellSpursJobContext2* jobContext,  CellSpursJob256 *jobDxt)
{
	// Grab the input buffer
	void *inOutBuffer __attribute__((aligned(16)));
	cellSpursJobGetPointerList(&inOutBuffer, &jobDxt->header, jobContext);

	// Grab user data (in a real job these would be packed better)
	uint32_t srcEa = jobDxt->workArea.userData[1];
	uint32_t destEa = jobDxt->workArea.userData[2];
	uint32_t blocksPerBatch = jobDxt->workArea.userData[3];
	uint32_t numBatches = jobDxt->workArea.userData[4];
	uint32_t waitLabelEa = jobDxt->workArea.userData[5];
	uint32_t dxtMode = jobDxt->workArea.userData[6];

	uint32_t inputBatchSize, outputBatchSize;
	uint32_t maxBatchSize;
	if( dxtMode & 0x1 )
	{
		// compression
		uint32_t bytesPerBlock = ((dxtMode == 1) || (dxtMode == 7)) ? 8 : 16;
		inputBatchSize = 4*16*blocksPerBatch;
		outputBatchSize = bytesPerBlock*blocksPerBatch;
	}
	else
	{
		// decompression
		uint32_t bytesPerBlock = ((dxtMode == 2) || (dxtMode == 8)) ? 8 : 16;
		inputBatchSize = bytesPerBlock*blocksPerBatch;
		outputBatchSize = 4*16*blocksPerBatch;
	}
	maxBatchSize = ( outputBatchSize > inputBatchSize ) ? outputBatchSize : inputBatchSize;

	uint32_t rowStride = 4*4*blocksPerBatch;

	// DMA pipeline loop
	uint8_t *workAreaSrc = (uint8_t*)inOutBuffer;
	uint8_t *workAreaDst = workAreaSrc + maxBatchSize;
	uint8_t *inputArea = workAreaDst + maxBatchSize;
	uint8_t *outputArea = inputArea + maxBatchSize;
	uint32_t workSrcDmaTag = 30;
	uint32_t workDstDmaTag = 29;
	uint32_t inputDmaTag = 28;
	uint32_t outputDmaTag = 27;

	uint32_t remainingBatches = numBatches;
	while( remainingBatches != 0 )
	{
		// reduce remaining count
		--remainingBatches;		

		// pre-fetch next batch
		if( remainingBatches != 0 )
		{
			// ensure we can use the area (i.e. prev output has finished)
			cellDmaWaitTagStatusAll(1 << inputDmaTag);
		
			// grab the next data
			srcEa += inputBatchSize;
			cellDmaGet(inputArea, srcEa, inputBatchSize, inputDmaTag, 0, 0);
		}

		// sync the current batch
		cellDmaWaitTagStatusAll(1 << workSrcDmaTag);

		// work on current batch
		switch( dxtMode )
		{
		case 1:
		default:
			edgeDxtCompress1(workAreaDst, workAreaSrc, rowStride, blocksPerBatch);
			break;
		case 2:
		case 8:
			edgeDxtDecompress1(workAreaSrc, workAreaDst, rowStride, blocksPerBatch);
			break;

		case 3:
			edgeDxtCompress3(workAreaDst, workAreaSrc, rowStride, blocksPerBatch);
			break;
		case 4:
			edgeDxtDecompress3(workAreaSrc, workAreaDst, rowStride, blocksPerBatch);
			break;

		case 5:
			edgeDxtCompress5(workAreaDst, workAreaSrc, rowStride, blocksPerBatch);
			break;
		case 6:
			edgeDxtDecompress5(workAreaSrc, workAreaDst, rowStride, blocksPerBatch);
			break;

		case 7:
			edgeDxtCompress1a(workAreaDst, workAreaSrc, rowStride, blocksPerBatch);
			break;
		}

		// cycle buffers
		uint8_t *tempArea = outputArea;
		outputArea = workAreaDst;
		workAreaDst = workAreaSrc;
		workAreaSrc = inputArea;
		inputArea = tempArea;

		uint32_t tempDmaTag = outputDmaTag;
		outputDmaTag = workDstDmaTag;
		workDstDmaTag = workSrcDmaTag;
		workSrcDmaTag = inputDmaTag;
		inputDmaTag = tempDmaTag;

		// save out the results
		cellDmaPut(outputArea, destEa, outputBatchSize, outputDmaTag, 0, 0);
		destEa += outputBatchSize;
	}

	// ensure our other DMAs have finished
	cellDmaWaitTagStatusAll((1 << inputDmaTag) | (1 << workSrcDmaTag) | (1 << workDstDmaTag) | (1 << outputDmaTag));

	// write the label value
	cellDmaPutUint32(1, waitLabelEa, jobContext->dmaTag, 0, 0);
}


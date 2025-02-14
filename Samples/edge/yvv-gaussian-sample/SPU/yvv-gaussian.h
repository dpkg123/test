/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef EDGE_YVV_SAMPLE_YVV_GAUSSIAN_H
#define EDGE_YVV_SAMPLE_YVV_GAUSSIAN_H

#define YVV_XRGB 1
#define YVV_ARGB 2

typedef __attribute__((aligned(16))) struct YvvParameters
{
	uint32_t	rsxLabelValue;				// if you want the task to write an RSX label when done, use
	uint32_t	rsxLabelAddress;			// these two parameters. You'll only want to set this for one SPU.
	uint32_t	taskCounterAddress;         // if you want each SPU to atomically increment a counter when done, set this.
	uint32_t	imageAddress;				// source address of the image.
	uint32_t	destAddress;				// the target address can be identical to imageAddress
	uint32_t	imageWidth;					// number of lines in the image
	uint32_t	imageHeight;				// number of lines in the image
	uint32_t	imagePitch;					// pitch in pixels, not bytes.
	uint32_t	barrierAddress;				// address to a SPURS barrier used to synchronize multi-SPU operation
	uint32_t	directionLockAddress;		// busy waiting direction lock used for transpose.
	float		sigmaH;						// horizontal sigma of the gaussian function
	float		sigmaV;						// vertical sigma of the gaussian function
	uint8_t		mode;						// set to 0 to disable filter
	uint8_t		spuId;						// number of SPU
	uint8_t		spuCount;					// total number of SPUs
	uint8_t		padding[13];
} YvvParameters;

#endif

/*=============================================================================
	PS3Landscape.h: Landscape definitions shared between PPU and SPU.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3LANDSCAPE_H__
#define __PS3LANDSCAPE_H__

#define LANDSCAPE_SPU_CMD_BUFFER_HOLE_SIZE 16
#define LANDSCAPE_NUM_BUFFERS 10

struct FLandscapeJobParameters
{
	FLOAT LodDistancesValues[4];
	INT CurrentLOD;
	INT SubsectionSizeVerts;
	DWORD HeightDataCurrentLodBase;
	DWORD HeightDataCurrentLodPitch;
	DWORD HeightDataNextLodBase;
	DWORD HeightDataNextLodPitch;
	DWORD OutputBase;
	DWORD MySpuRsxFence;
	DWORD CommandBufferHole;
	DWORD SpuRsxFencePtr;
};

#endif

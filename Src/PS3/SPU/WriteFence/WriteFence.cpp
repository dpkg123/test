/*=============================================================================
	WriteFence.cpp: SPURS Job to write out an SPU fence value
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "SPU.h"


void cellSpursJobMain2(CellSpursJobContext2 *JobContext, CellSpursJob256 *InJob)
{
	FSpursJobType* Job	= (FSpursJobType*) InJob;

	// Write the job's fence.
	if (JobContext->oBuffer == 0)  spu_printf("WF reserved WriteFence() buffer at 0x%p\n", JobContext->oBuffer);
	WriteJobFence(JobContext, Job, JobContext->ioBuffer);
}

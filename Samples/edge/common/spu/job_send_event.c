/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <cell/spurs/job_chain.h>
#include <cell/spurs/job_context.h>
#include <cell/spurs/event_flag.h>


void cellSpursJobMain2(CellSpursJobContext2* stInfo, CellSpursJob256 *job256)
{
	(void)stInfo;
	CellSpursJob64 *job = (CellSpursJob64 *)job256;
	uint64_t eaEventFlag = job->workArea.userData[0];
	uint16_t bitsEventFlag = (uint16_t) job->workArea.userData[1];	
	cellSpursEventFlagSet(eaEventFlag, bitsEventFlag);
}

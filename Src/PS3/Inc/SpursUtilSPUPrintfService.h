/* SCE CONFIDENTIAL
PLAYSTATION(R)3 Programmer Tool Runtime Library 190.002
* Copyright (C) 2007 Sony Computer Entertainment Inc.
* All Rights Reserved.
*/

#ifndef SPURS_PRINTF_UTIL_H
#define SPURS_PRINTF_UTIL_H


/* Lv2 OS headers */
#include <sys/event.h>
#include <sys/spu_thread.h>
#include <sys/ppu_thread.h>

#include <cell/spurs.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct SampleUtilSpursPrintfService
{
	CellSpurs*          spurs;
	sys_event_queue_t	equeue;
	sys_ppu_thread_t	spu_printf_handler;
	sys_event_port_t	terminating_port;
} SampleUtilSpursPrintfService;

	int sampleSpursUtilSpuPrintfServiceInitialize(SampleUtilSpursPrintfService *service, CellSpurs*, int prio);
	int sampleSpursUtilSpuPrintfServiceFinalize(SampleUtilSpursPrintfService *service);
	

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SPURS_PRINTF_UTIL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 4
 * End:
 * vim:sw=4:sts=4:ts=4
 */

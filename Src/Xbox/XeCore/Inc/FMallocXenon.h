/*=============================================================================
	FMallocXenon.h: Xenon allocators
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _F_MALLOC_XENON_H_
#define _F_MALLOC_XENON_H_

#if ALLOW_NON_APPROVED_FOR_SHIPPING_LIB
#pragma pack(push,8)
#include <xbdm.h>
#pragma pack(pop)
#endif

#define USE_FMALLOC_POOLED	1

#include "FMallocXenonPooled.h"
extern FMallocXenonPooled Malloc;

#endif //_F_MALLOC_XENON_H_



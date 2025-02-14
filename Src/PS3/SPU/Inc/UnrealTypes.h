/*=============================================================================
	UnrealTypes.h: Some types to easily map code between Unreal and SPU
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sdk_version.h>
#include <stdint.h>
#include <spu_printf.h>


#if CELL_SDK_VERSION < 0x080000
	#include <cell/mfc_io.h>
#else
	#include <spu_mfcio.h>
#endif


// Unsigned base types.
typedef uint8_t					BYTE;		// 8-bit  unsigned.
typedef uint16_t				WORD;		// 16-bit unsigned.
typedef uint32_t				UINT;		// 32-bit unsigned.
typedef unsigned long			DWORD;		// 32-bit unsigned.
typedef uint64_t				QWORD;		// 64-bit unsigned.

// Signed base types.
typedef int8_t					SBYTE;		// 8-bit  signed.
typedef int16_t					SWORD;		// 16-bit signed.
typedef int32_t					INT;		// 32-bit signed.
typedef long					LONG;		// 32-bit signed.
typedef int64_t					SQWORD;		// 64-bit unsigned.

// Character types.
typedef char					ANSICHAR;	// An ANSI character. normally a signed type.
typedef int16_t					UNICHAR;	// A unicode character. normally a signed type.

// Other base types.
typedef uint32_t				UBOOL;		// Boolean 0 (false) or 1 (true).
typedef int32_t					BOOL;		// Must be sint32 to match windows.h
typedef float					FLOAT;		// 32-bit IEEE floating point.
typedef double					DOUBLE;		// 64-bit IEEE double.
typedef unsigned long			SIZE_T;     // Should be size_t, but windows uses this
typedef intptr_t				PTRINT;		// Integer large enough to hold a pointer.
typedef uintptr_t				UPTRINT;	// Signed integer large enough to hold a pointer.

#define FALSE					0
#define TRUE					1

#define ARRAY_COUNT(Array) (sizeof(Array) / sizeof((Array)[0]))
#define STRUCT_OFFSET(struc, member)	(((DWORD)&((struc*)0x1)->member) - 0x1)

#define printf	spu_printf






#define DMARead(Dst, SrcHi, SrcLo, Size, Tag) spu_mfcdma64(Dst, SrcHi, SrcLo, Size, Tag, MFC_GET_CMD);
#define DMAWrite(DstHi, DstLo, Src, Size, Tag) spu_mfcdma64(Src, DstHi, DstLo, Size, Tag, MFC_PUT_CMD);

inline INT WaitForSignal(INT ChannelNumber=0)
{
	// spu_readch needs to be called with a literal, not a ?: operator
	return ChannelNumber ? spu_readch(SPU_RdSigNotify2) : spu_readch(SPU_RdSigNotify1);
}

inline void SendEvent(DWORD EventPortKey, DWORD Value1=0, DWORD Value2=0)
{
	sys_spu_thread_send_event(EventPortKey, Value1, Value2);
}

/*
 * Wait for completion of DMA commands with the specified tag number.
 * The sequence of commands executed in this function is described in 
 * BPA Book I-III 8.9 "SPU Tag Group Status Channles".
 *
 * Tag: DMA tag 
 */
inline void SyncDMA(DWORD Tag)
{
#if 0
	// @todo spu: this first block doesn't appear to be necessary, according
	// to any docs, but it can't hurt, since Sony wrote it for a reason :)
	// Clear MFC Tag update
	spu_writech(MFC_WrTagUpdate, 0x0);
	spu_readch(MFC_RdTagStat);
	// wait for the clear to finish
	while (spu_readchcnt(MFC_WrTagUpdate) != 1) {}

	// set which tag to sync on
	spu_writech(MFC_WrTagMask, (1 << Tag));
	// wait for all transfers in this tag group to finish
	mfc_wait_tag_status_all();
//spu_mfcstat(MFC_TAG_UPDATE_ALL);
#else
	// @todo spu: this first block doesn't appear to be necessary, according
	// to any docs, but it can't hurt, since Sony wrote it for a reason :)
	// Clear MFC Tag update
	spu_writech(MFC_WrTagUpdate, 0x0);
	// wait for the clear to finish
	while (spu_readchcnt(MFC_WrTagUpdate) != 1) {}
	spu_readch(MFC_RdTagStat);

	// set which tag to sync on
	spu_writech(MFC_WrTagMask, (1 << Tag));
	// not sure
	spu_writech(MFC_WrTagUpdate, 0x2);
	// wait until it's done
	spu_readch(MFC_RdTagStat);
#endif
}

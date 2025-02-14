/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2007 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef __EDGE_SEGCOMP_SAMPLE_ENDIAN_H__
#define __EDGE_SEGCOMP_SAMPLE_ENDIAN_H__

#include "edge/edge_stdint.h"

//////////////////////////////////////////////////////////////////////////

inline uint8_t	EdgeSegCompSampleEndianSwap(uint8_t a)	{ return a; }
inline uint16_t	EdgeSegCompSampleEndianSwap(uint16_t a)	{ return (a << 8) | (a >> 8); }
inline uint32_t	EdgeSegCompSampleEndianSwap(uint32_t a)	{ return (uint32_t(EdgeSegCompSampleEndianSwap(uint16_t(a))) << 16) | uint32_t(EdgeSegCompSampleEndianSwap(uint16_t(a >> 16))); }
inline uint64_t	EdgeSegCompSampleEndianSwap(uint64_t a)	{ return (uint64_t(EdgeSegCompSampleEndianSwap(uint32_t(a))) << 32) | uint64_t(EdgeSegCompSampleEndianSwap(uint32_t(a >> 32))); }

//////////////////////////////////////////////////////////////////////////

#if defined( __PPU__ ) || defined( __SPU__ )

//////////////////////////////////////////////////////////////////////////

	typedef uint32_t	EdgeSegCompSampleBigEndianU32;
	typedef uint16_t	EdgeSegCompSampleBigEndianU16;
	typedef uint8_t		EdgeSegCompSampleBigEndianU8;

//////////////////////////////////////////////////////////////////////////

#else

//////////////////////////////////////////////////////////////////////////

	class EdgeSegCompSampleBigEndianU32
	{
	public:
		//If we assign a uint32_t, do the appropriate conversion to make it big-endian in memory
		EdgeSegCompSampleBigEndianU32& operator= ( uint32_t val )
		{
			m_data = EdgeSegCompSampleEndianSwap( val );
			return *this;
		}

		//If we access this EdgeSegCompSampleBigEndianU32, then convert it to the processor's endianness
		operator uint32_t() const
		{
			return EdgeSegCompSampleEndianSwap( m_data );
		}

	private:
		uint32_t		m_data;			//This data is stored in memory as big-endian

	};

//////////////////////////////////////////////////////////////////////////

	class EdgeSegCompSampleBigEndianU16
	{
	public:
		//If we assign a uint16_t, do the appropriate conversion to make it big-endian in memory
		EdgeSegCompSampleBigEndianU16& operator= ( uint16_t val )
		{
			m_data = EdgeSegCompSampleEndianSwap( val );
			return *this;
		}

		//If we access this EdgeSegCompSampleBigEndianU16, then convert it to the processor's endianness
		operator uint16_t() const
		{
			return EdgeSegCompSampleEndianSwap( m_data );
		}

	private:
		uint16_t		m_data;			//This data is stored in memory as big-endian
	};

//////////////////////////////////////////////////////////////////////////

	typedef uint8_t EdgeSegCompSampleBigEndianU8;

//////////////////////////////////////////////////////////////////////////

#endif

#endif /* __EDGE_SEGCOMP_SAMPLE_ENDIAN_H__ */

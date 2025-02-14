/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#include    <memory.h>
#include    "edge/libedgeanimtool/libedgeanimtool_bitstream.h"

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

BitStream::BitStream()
{
    m_curBitCount = 0;
    m_curByte = 0;
}

void BitStream::AddByte(unsigned char a)
{
	m_buffer.push_back( a );
}

void BitStream::ByteAlign()
{
    if(m_curBitCount != 0) {
        AddByte((unsigned char)(m_curByte >> 56));
		m_curBitCount = 0;
    }
}

void BitStream::Write(uint64_t val, unsigned int numBits)
{
	val &= (1ll<<numBits)-1;
	unsigned int shift = 64 - m_curBitCount - numBits;
	m_curByte |= ( shift >= 64 ? 0 : val << shift );
    m_curBitCount += numBits;

    while(m_curBitCount >= 8) {
        m_curBitCount -= 8;
        AddByte((unsigned char)(m_curByte >> 56));
        m_curByte <<= 8;
    }
}

//--------------------------------------------------------------------------------------------------

} // namespace Tools
} // namespace Edge


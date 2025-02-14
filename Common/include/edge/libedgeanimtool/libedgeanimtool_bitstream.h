/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef LIBEDGEANIMTOOL_BITSTREAM_H
#define LIBEDGEANIMTOOL_BITSTREAM_H

#include <vector>
#include <edge/edge_stdint.h>

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

class BitStream
{
public:
    BitStream(void);

    void            Write(uint64_t data, unsigned int numBits);
    size_t			GetBufferSize();
    unsigned char	GetByte( unsigned i );
    void            ByteAlign();

private:

    void            AddByte(unsigned char a);

	std::vector<unsigned char>	m_buffer;
    unsigned int    m_curBitCount;
    uint64_t        m_curByte;
};

inline unsigned char BitStream::GetByte( unsigned i )
{
	return m_buffer[i];
}

inline size_t BitStream::GetBufferSize()
{
    return m_buffer.size();
}

//--------------------------------------------------------------------------------------------------

}   // namespace Tools
}   // namespace Edge

#endif // LIBEDGEANIMTOOL_BITSTREAM_H

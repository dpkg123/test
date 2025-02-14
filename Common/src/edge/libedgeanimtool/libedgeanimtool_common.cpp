/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


#include    <math.h>
#include    "edge/libedgeanimtool/libedgeanimtool_common.h"
#include    "edge/libedgeanimtool/libedgeanimtool_filewriter.h"
#include    "edge/libedgeanimtool/libedgeanimtool_bitstream.h"

//--------------------------------------------------------------------------------------------------

namespace
{

template<typename T>
T Sq(T val)
{
    return val * val;
}

void Normalize3(float* pVec3)
{
    const float kEpsilon = 0.0001f;
    float length = sqrtf(Sq(pVec3[ 0 ]) + Sq(pVec3[ 1 ]) + Sq(pVec3[ 2 ]));
    if (length > kEpsilon) {
        pVec3[ 0 ] /= length;
        pVec3[ 1 ] /= length;
        pVec3[ 2 ] /= length;
    }
}

void Cross3(float* pDest, const float* pVec0, const float* pVec1)
{
    float tmpX = ((pVec0[1] * pVec1[2]) - (pVec0[2] * pVec1[1]));
    float tmpY = ((pVec0[2] * pVec1[0]) - (pVec0[0] * pVec1[2]));
    float tmpZ = ((pVec0[0] * pVec1[1]) - (pVec0[1] * pVec1[0]));
    pDest[ 0 ] = tmpX;
    pDest[ 1 ] = tmpY;
    pDest[ 2 ] = tmpZ;
}

} // anonymous namespace

//--------------------------------------------------------------------------------------------------

namespace Edge
{
namespace Tools
{

//--------------------------------------------------------------------------------------------------

Joint MatrixToJoint(const float* pMatrix4x4)
{
    Joint joint;

    // Translation
    joint.m_translation[ 0 ] = pMatrix4x4[4 * 3 + 0];
    joint.m_translation[ 1 ] = pMatrix4x4[4 * 3 + 1];
    joint.m_translation[ 2 ] = pMatrix4x4[4 * 3 + 2];
    joint.m_translation[ 3 ] = 1.0f;

    // Scale
    joint.m_scale[ 0 ] = sqrtf(Sq(pMatrix4x4[4 * 0 + 0]) + Sq(pMatrix4x4[4 * 0 + 1]) + Sq(pMatrix4x4[4 * 0 + 2]));
    joint.m_scale[ 1 ] = sqrtf(Sq(pMatrix4x4[4 * 1 + 0]) + Sq(pMatrix4x4[4 * 1 + 1]) + Sq(pMatrix4x4[4 * 1 + 2]));
    joint.m_scale[ 2 ] = sqrtf(Sq(pMatrix4x4[4 * 2 + 0]) + Sq(pMatrix4x4[4 * 2 + 1]) + Sq(pMatrix4x4[4 * 2 + 2]));
    joint.m_scale[ 3 ] = 1.0f;

    // Ortho normalize rotation
    float rotMat[3][3];
    rotMat[ 0 ][ 0 ] = pMatrix4x4[4 * 0 + 0];
    rotMat[ 0 ][ 1 ] = pMatrix4x4[4 * 0 + 1];
    rotMat[ 0 ][ 2 ] = pMatrix4x4[4 * 0 + 2];
    Normalize3(rotMat[ 0 ]);
    Cross3(rotMat[ 2 ], rotMat[ 0 ], &pMatrix4x4[4 * 1 + 0]);
    Normalize3(rotMat[ 2 ]);
    Cross3(rotMat[ 1 ], rotMat[ 2 ], rotMat[ 0 ]);
    Normalize3(rotMat[ 1 ]);

    // Get quat from rotation (see target/common/Vectormath)
    float xx = rotMat[ 0 ][ 0 ] ;
    float yx = rotMat[ 0 ][ 1 ] ;
    float zx = rotMat[ 0 ][ 2 ] ;
    float xy = rotMat[ 1 ][ 0 ] ;
    float yy = rotMat[ 1 ][ 1 ] ;
    float zy = rotMat[ 1 ][ 2 ] ;
    float xz = rotMat[ 2 ][ 0 ] ;
    float yz = rotMat[ 2 ][ 1 ] ;
    float zz = rotMat[ 2 ][ 2 ] ;
    float trace = ((xx + yy) + zz);
    bool negTrace = (trace < 0.0f);
    bool ZgtX = zz > xx;
    bool ZgtY = zz > yy;
    bool YgtX = yy > xx;
    bool largestXorY = (!ZgtX || !ZgtY) && negTrace;
    bool largestYorZ = (YgtX || ZgtX) && negTrace;
    bool largestZorX = (ZgtY || !YgtX) && negTrace;
    
    if (largestXorY) {
        zz = -zz;
        xy = -xy;
    }
    if (largestYorZ) {
        xx = -xx;
        yz = -yz;
    }
    if (largestZorX) {
        yy = -yy;
        zx = -zx;
    }

    float radicand = (((xx + yy) + zz) + 1.0f);
    float scale = (0.5f * (1.0f / sqrtf(radicand)));

    float tmpx = ((zy - yz) * scale);
    float tmpy = ((xz - zx) * scale);
    float tmpz = ((yx - xy) * scale);
    float tmpw = (radicand * scale);
    float qx = tmpx;
    float qy = tmpy; 
    float qz = tmpz;
    float qw = tmpw;

    if (largestXorY) {
        qx = tmpw;
        qy = tmpz;
        qz = tmpy;
        qw = tmpx;
    }
    if (largestYorZ) {
        tmpx = qx;
        tmpz = qz;
        qx = qy;
        qy = tmpx;
        qz = qw;
        qw = tmpz;
    }

    joint.m_rotation[ 0 ] = qx;
    joint.m_rotation[ 1 ] = qy;
    joint.m_rotation[ 2 ] = qz;
    joint.m_rotation[ 3 ] = qw;

    return joint;
}

//--------------------------------------------------------------------------------------------------
/**
    Compress a quaternion to 48 bits using "smallest 3" compression

    Compressed quat is in least significant 48 bits of returned value
**/
//--------------------------------------------------------------------------------------------------

static const float kSqrt2 = 1.414213562f;
static const float kQuatScale = ((1 << 15) - 1) / kSqrt2;
static const float kQuatOffset = kQuatScale / kSqrt2;

uint64_t CompressQuat(const float* q)
{
    uint64_t r = 0;

    // get index of largest component
    float qa[4];
    qa[0] = fabsf(q[0]);
    qa[1] = fabsf(q[1]);
    qa[2] = fabsf(q[2]);
    qa[3] = fabsf(q[3]);

    unsigned short idx = 0;
    float l = qa[0];
    if(qa[1] > l) {
        idx = 1;
        l = qa[1];
    }
    if(qa[2] > l) {
        idx = 2;
        l = qa[2];
    }
    if(qa[3] > l) {
        idx = 3;
    }

    // negate if largest cpt is -ve
    float q0[4];
    if(q[idx] < 0) {
        q0[0] = -q[0];
        q0[1] = -q[1];
        q0[2] = -q[2];
        q0[3] = -q[3];
    }
    else {
        q0[0] = q[0]; 
        q0[1] = q[1]; 
        q0[2] = q[2]; 
        q0[3] = q[3];
    }

    // 15 bit quantised components
    unsigned int c = 0;
    for(unsigned int i = 0; i < 3; i++, c++) {
        if(i == idx) {
            c++;
        }
        uint64_t qc = (uint64_t)(q0[c] * kQuatScale + kQuatOffset);
        r |= qc << (32 - i*15);
    }

    // largest component index
    r |= idx;

    return r;
}

//--------------------------------------------------------------------------------------------------
/**
    Decompress a 48 bit "smallest 3" compressed quaternion

    Compressed quat is passed in the least significant 48 bits of q
**/
//--------------------------------------------------------------------------------------------------

void DecompressQuat(float* pRes, uint64_t q)
{
    unsigned int a = (unsigned int)((q >> 32) & ((1<<15)-1));
    unsigned int b = (unsigned int)((q >> 17) & ((1<<15)-1));
    unsigned int c = (unsigned int)((q >> 2) & ((1<<15)-1));
    unsigned int idx = (unsigned int)(q & 3);

    float fa = ((float)a - kQuatOffset) / kQuatScale;
    float fb = ((float)b - kQuatOffset) / kQuatScale;
    float fc = ((float)c - kQuatOffset) / kQuatScale;
    float fd = sqrtf(1 - fa*fa - fb*fb - fc*fc);

    switch(idx)
    {
    case 0:
        pRes[0] = fd;   pRes[1] = fa;   pRes[2] = fb;   pRes[3] = fc;
        break;
    case 1:
        pRes[0] = fa;   pRes[1] = fd;   pRes[2] = fb;   pRes[3] = fc;
        break;
    case 2:
        pRes[0] = fa;   pRes[1] = fb;   pRes[2] = fd;   pRes[3] = fc;
        break;
    case 3:
        pRes[0] = fa;   pRes[1] = fb;   pRes[2] = fc;   pRes[3] = fd;
        break;
    }
}

//--------------------------------------------------------------------------------------------------

Float4 Reverse(const Float4& a)
{ 
    Float4 r;

    r[0] = Reverse(a[0]);
    r[1] = Reverse(a[1]);
    r[2] = Reverse(a[2]);
    r[3] = Reverse(a[3]);

    return r;
}

//--------------------------------------------------------------------------------------------------

void WriteBitStreamToFile(BitStream& bitStream, FileWriter& fileWriter)
{
    bitStream.ByteAlign();

    size_t size = bitStream.GetBufferSize();
    for(unsigned j = 0; j < size; j++) {
        fileWriter.Write( bitStream.GetByte(j) );
    }
}

//--------------------------------------------------------------------------------------------------
/**
    Absolute error between quaternion rotations.
**/
//--------------------------------------------------------------------------------------------------

float QuatAbsError(const float* pQa, const float* pQb)
{
    float posAbsError = 0.0f;
    float negAbsError = 0.0f;
    for(int i = 0; i < 4; ++i) {
        float posCmpAbsError = fabsf(pQa[i] - pQb[i]);
        float negCmpAbsError = fabsf(pQa[i] + pQb[i]);
        if(posAbsError < posCmpAbsError)
            posAbsError = posCmpAbsError;
        if(negAbsError < negCmpAbsError)
            negAbsError = negCmpAbsError;
    }
    return (posAbsError < negAbsError) ? posAbsError : negAbsError;
}

//--------------------------------------------------------------------------------------------------
/**
    Absolute error between two 4-vectors.
**/
//--------------------------------------------------------------------------------------------------

float VecAbsError(const float* pVa, const float* pVb)
{
    float posAbsError = 0.0f;
    for(int i = 0; i < 4; ++i) {
        float posCmpAbsError = fabsf(pVa[i] - pVb[i]);
        if(posAbsError < posCmpAbsError)
            posAbsError = posCmpAbsError;
    }
    return posAbsError;
}

//--------------------------------------------------------------------------------------------------
/**
Export the custom data as a hash table of entries
**/
//--------------------------------------------------------------------------------------------------

void ExportCustomDataTable( FileWriter& fileWriter, void* customData )
{
	char startLabel[ 32 ], endLabel[ 32 ];
	const CustomDataTable* customDataTable = static_cast<const CustomDataTable*>( customData );
	const size_t numEntries = customDataTable->m_CustomDataEntries.size();

	// Test for hash clashes
	for( size_t i = 0; i < numEntries; i++ )
	{
		const CustomDataTableEntry* pEntryA = customDataTable->m_CustomDataEntries[ i ];
		const unsigned int hashA = pEntryA->GetHashValue();
		for( size_t j = i + 1; j < numEntries; j++ )
		{
			const CustomDataTableEntry* pEntryB = customDataTable->m_CustomDataEntries[ j ];
			const unsigned int hashB = pEntryB->GetHashValue();
			if( hashA == hashB )
			{
				std::cerr << "Custom Data Table: name hash clash error detected" << std::endl;
				EDGEERROR_F();
			}
		}
	}

	// Write Table Header
	fileWriter.Write( (uint32_t) numEntries );				// uint32_t	numEntries;			/* 0x00: Number of entries in table */
	fileWriter.WriteOffset32( "CustomDataHashArray" );		// uint32_t	offsetHashArray;	/* 0x04: Offset to hash names of custom data (uint32_t*) */
	fileWriter.WriteOffset32( "CustomDataEntrySizes" );		// uint32_t	offsetEntrySizes;	/* 0x08: Offset to sizes of custom data entries (uint32_t*) */
	fileWriter.WriteOffset32( "CustomDataEntryOffsets" );	// uint32_t	offsetEntries;		/* 0x0C: Offset to custom data entry offset (uint32_t*) */

	// Hash Array
	fileWriter.Label( "CustomDataHashArray" );
	for( size_t i = 0; i < numEntries; i++ )
	{
		const CustomDataTableEntry* pEntry = customDataTable->m_CustomDataEntries[ i ];
		fileWriter.Write( pEntry->GetHashValue() ); // TODO - Test for hash clashes
	}

	// Entry Sizes
	fileWriter.Label( "CustomDataEntrySizes" );
	for( size_t i = 0; i < numEntries; i++ )
	{
		sprintf( startLabel, "StartCustomData%d", i );
		sprintf( endLabel, "EndCustomData%d", i );
		fileWriter.WriteOffset32( endLabel, startLabel );
	}

	// Data Entry Offsets
	fileWriter.Label( "CustomDataEntryOffsets" );
	for( size_t i = 0; i < numEntries; i++ )
	{
		sprintf( startLabel, "StartCustomData%d", i );
		fileWriter.WriteOffset32( startLabel );
	}

	// Data Entries
	for( size_t i = 0; i < numEntries; i++ )
	{
		const CustomDataTableEntry* pEntry = customDataTable->m_CustomDataEntries[ i ];

		fileWriter.Align( pEntry->GetDataAlignment() );
		sprintf( startLabel, "StartCustomData%d", i );
		fileWriter.Label( startLabel );

		pEntry->ExportData( fileWriter );

		fileWriter.Align( pEntry->GetDataAlignment() );
		sprintf( endLabel, "EndCustomData%d", i );
		fileWriter.Label( endLabel );
	}
}

}   // namespace Tools
}   // namespace Edge

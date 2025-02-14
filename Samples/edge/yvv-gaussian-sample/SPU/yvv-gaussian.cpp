/* SCE CONFIDENTIAL
 * PlayStation(R)Edge 1.2.0
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#include <stdint.h>
#include <spu_intrinsics.h>
#include <cell/spurs.h>
#include <spu_printf.h>
#include <cell/dma.h>
#include <cell/atomic.h>
#include <stdlib.h>

#include <edge/edge_stdint.h>
#include <edge/edge_assert.h>
#include <edge/edge_dma.h>
#include <edge/post/edgepost_inplace_transpose.h>

#include "yvv-gaussian.h"

#define YVV_MODE YVV_XRGB

#include "memory_layout.h"

#define USE_SPA_VERSION 1

extern "C" void edgeYvvHorizontal(
	uint32_t* srcPtr0,
	uint32_t* srcPtr1,
	uint32_t* srcPtr2,
	uint32_t* srcPtr3,
	float*    scratchPtr0,
	float*    scratchPtr1,
	float*    scratchPtr2,
	float*    scratchPtr3,
	uint32_t  width,
	const float*    bPtr,
	const float*    mPtr
	);

extern "C" void edgeYvvHorizontal3(
	uint32_t* srcPtr0,
	uint32_t* srcPtr1,
	uint32_t* srcPtr2,
	uint32_t* srcPtr3,
	uint32_t* srcPtr4,
	uint32_t* srcPtr5,
	uint32_t* srcPtr6,
	uint32_t* srcPtr7,

	float*    scratchPtr0,
	float*    scratchPtr1,
	float*    scratchPtr2,
	float*    scratchPtr3,
	float*    scratchPtr4,
	float*    scratchPtr5,

	uint32_t  width,
	const float*    bPtr,
	const float*    mPtr
	);

MemoryLayout g_Mem;


static inline void _edgePostFillEvenSizedDmaListAligned(
	register char* list,
	register const uint32_t elements,
	register const uint32_t startEa,
	register const uint32_t eaUpdate,
	register const uint32_t fetchSize)
{
	register const vec_uint4 element_update = {0,2*eaUpdate,0,2*eaUpdate};
	register vec_uint4 two_elements         = {fetchSize,startEa,fetchSize,startEa+eaUpdate};

	register vec_uint4* buffer = (vec_uint4*)list;

	for(register int32_t i = elements; i > 0; i-=2)
	{
		*buffer=two_elements;
		two_elements+=element_update;
		++buffer;
	}
}


void FetchLine(
	uint32_t imageEa,  //in transposed mode, this always points to the start of a block. use BlockOffset to start within a block
	const  int32_t index,
	const uint32_t pitch, //in transposed mode, this is the "virtual" pitch, e.g 768 for a 720p target.
	const uint32_t height,
	const uint32_t transposedPitch, //in transposed mode, this is the pitch of the original image.
	const uint32_t transposedBlockOffset, //this is the offset within a transposed block where we start.
	const uint32_t mode)
{	
	(void) height;

	const uint32_t buffer_line=index&BUFFER_LINE_MASK;

	if(1 == mode)
	{
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
		spu_idisable();
		const uint32_t ea=index*pitch+imageEa;
		EDGE_DMA_GETF(&g_Mem.yvv.LineBuffer[buffer_line][0], ea, pitch, buffer_line, 0, 0);
		if( interrupts_enabled ) {spu_ienable();} 
	}
	else
	{

		const int32_t  total_block_offset   = transposedBlockOffset+index;
		const bool     negative             = total_block_offset < 0;
		const uint32_t abs_tbo              = abs(total_block_offset);

		const  int32_t abs_horizontal_block = (abs_tbo) / YVV_TRANSPOSE_BLOCK_SIZE;
		const  int32_t abs_vertical_line    = (abs_tbo) % YVV_TRANSPOSE_BLOCK_SIZE;

		const  int32_t horizontal_block     = negative?-(abs_horizontal_block+1):abs_horizontal_block;
		const  int32_t vertical_line        = negative?YVV_TRANSPOSE_BLOCK_SIZE-abs_vertical_line:abs_vertical_line;
		

		const uint32_t ea=
			horizontal_block * YVV_TRANSPOSE_BLOCK_SIZE * 4 // pick the right block to start with
			+ vertical_line * transposedPitch						// pick the right line within the block.
			+ imageEa;

		//we have to piece it together
		const uint32_t transfers = pitch / (YVV_TRANSPOSE_BLOCK_SIZE * 4);

		_edgePostFillEvenSizedDmaListAligned(
			(char*)g_Mem.yvv.DmaList[2*buffer_line+1],
			transfers,
			ea,
			YVV_TRANSPOSE_BLOCK_SIZE * transposedPitch,
			YVV_TRANSPOSE_BLOCK_SIZE * 4);

		{ 
			const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
			spu_idisable();
			EDGE_DMA_LIST_GETF(
				&g_Mem.yvv.LineBuffer[buffer_line][0],
				0,
				g_Mem.yvv.DmaList[2*buffer_line+1],
				transfers*sizeof(EdgeDmaListElement),
				buffer_line,
				0,0);
			if( interrupts_enabled ) {spu_ienable();} 
		}

	}
}


void StoreLine(
	uint32_t imageEa,  //in transposed mode, this always points to the start of a block. use BlockOffset to start within a block
	const  int32_t index, 
	const uint32_t pitch, //in transposed mode, this is the "virtual" pitch, e.g 768 for a 720p target.
	const uint32_t height, 
	const uint32_t transposedPitch, //in transposed mode, this is the pitch of the original image.
	const uint32_t transposedBlockOffset, //this is the offset within a transposed block where we start.
	const uint32_t mode,
	const uint32_t spuId)
{
	(void) height;
	(void) spuId;
	const uint32_t buffer_line=index&BUFFER_LINE_MASK;

	if(1 == mode)
	{ 
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
		spu_idisable();
		const uint32_t ea=index*pitch+imageEa;
		EDGE_DMA_PUTF(&g_Mem.yvv.LineBuffer[buffer_line][0],ea,pitch,buffer_line,0,0);
		if( interrupts_enabled ) {spu_ienable();} 
	}
	else
	{
		const uint32_t horizontal_block = (transposedBlockOffset + index) / YVV_TRANSPOSE_BLOCK_SIZE;
		const uint32_t vertical_line    = (transposedBlockOffset + index) % YVV_TRANSPOSE_BLOCK_SIZE;
		const uint32_t ea=
			horizontal_block * YVV_TRANSPOSE_BLOCK_SIZE * 4 // pick the right block to start with
			+ vertical_line * transposedPitch						// pick the right line within the block.
			+ imageEa;

		//we have to piece it together
		const uint32_t transfers = pitch / (YVV_TRANSPOSE_BLOCK_SIZE * 4);

		_edgePostFillEvenSizedDmaListAligned(
			(char*)g_Mem.yvv.DmaList[2*buffer_line],
			transfers,
			ea,
			YVV_TRANSPOSE_BLOCK_SIZE * transposedPitch,
			YVV_TRANSPOSE_BLOCK_SIZE * 4);

		{ 
			const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
			spu_idisable();
			EDGE_DMA_LIST_PUTF(
				&g_Mem.yvv.LineBuffer[buffer_line][0],
				0,
				g_Mem.yvv.DmaList[2*buffer_line],
				transfers*sizeof(EdgeDmaListElement),
				buffer_line,
				0,0);
			if( interrupts_enabled ) {spu_ienable();} 
		}
	}
}


// used to compute width of transposed image
static inline uint32_t align( const uint32_t value, uint32_t alignment)
{
	return ( value + (alignment - 1)) & ~(alignment - 1);
}


inline void TransposePass(const YvvParameters& parameter)
{
	{ 
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
		spu_idisable();
		EDGE_DMA_WAIT_TAG_STATUS_ALL(0xffff); //flush the queues before we transpose
		cellSpursBarrierNotify(parameter.barrierAddress);
		cellSpursBarrierWait(parameter.barrierAddress);	
		if( interrupts_enabled ) {spu_ienable();} 
	}


	const uint32_t transpose_height = align(parameter.imageHeight,YVV_TRANSPOSE_BLOCK_SIZE);
	const uint32_t lines_per_spu=transpose_height/2; //we only use two SPUs.


	if(parameter.spuId<2)
	{				
		edgePostTransposeInPlace(
			&g_Mem.transpose,
			parameter.imageAddress,
			parameter.imageAddress,
			parameter.imagePitch * 4,
			lines_per_spu,
			YVV_TRANSPOSE_BLOCK_SIZE,
			parameter.spuId,
			parameter.directionLockAddress,
			NULL,
			NULL,
			0);

		//if we only have one SPU, we need to do the lower parts as well.
		if(1==parameter.spuCount)
		{
			edgePostTransposeInPlace(
				&g_Mem.transpose,
				parameter.imageAddress,
				parameter.imageAddress,
				parameter.imagePitch * 4,
				lines_per_spu,
				YVV_TRANSPOSE_BLOCK_SIZE,
				1,
				parameter.directionLockAddress,
				NULL,
				NULL,
				0);
		}
	}
	{ 
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
		spu_idisable();
		EDGE_DMA_WAIT_TAG_STATUS_ALL(0xffff); //flush before we (potentially) release LS.
		cellSpursBarrierNotify(parameter.barrierAddress);
		cellSpursBarrierWait(parameter.barrierAddress);
		if( interrupts_enabled ) {spu_ienable();} 
	}

}

#define CLAMP(a) ((a)<0?0:(a))
#define MAX(a,b) ((a)>(b)?(a):(b))

void HorizontalFilter(const uint32_t line, const uint32_t width, const float* b, const float *M)
{
	const uint32_t line_index = line & BUFFER_LINE_MASK;
	{ 
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
		spu_idisable();
		EDGE_DMA_WAIT_TAG_STATUS_ALL(1<<line_index);
		if( interrupts_enabled ) {spu_ienable();} 
	}

	//const float B_root_0 = b[4];
	
	const float B_root_1 = b[11];
	const float b1_1     = b[8] ;
	const float b2_1     = b[9] ;
	const float b3_1     = b[10];
	//const float B_1	     = B_root_1*B_root_1;

	//const float B_x  = B_root_0*B_root_1;


	const float recsum0 = b[5];
	const float recsum1 = b[5]*b[6];


	unsigned char *channels = (unsigned char*)g_Mem.yvv.LineBuffer[line_index];

	const float first_a = channels[0];
	const float first_r = channels[1];
	const float first_g = channels[2];
	const float first_b = channels[3];

	// these need to be replaced by the correct boundary handling
	float w1_a = first_a;
	float w1_r = first_r;
	float w1_g = first_g;
	float w1_b = first_b;

	float w2_a = first_a;
	float w2_r = first_r;
	float w2_g = first_g;
	float w2_b = first_b;

	float w3_a = first_a;
	float w3_r = first_r;
	float w3_g = first_g;
	float w3_b = first_b;

	//current values

	float b1 = b1_1;
	float b2 = b2_1;
	float b3 = b3_1;
	float B  = B_root_1;


	//save the last pixel for Triggs correction
	const float last_a = channels[4*(width-1)];
	const float last_r = channels[4*(width-1)+1];
	const float last_g = channels[4*(width-1)+2];
	const float last_b = channels[4*(width-1)+3];


	float last_B;
	float last_b1;
	float last_b2;
	float last_b3;


	uint32_t i;
	for(i=0;i<width;++i)
	{
		const float n0_a = channels[4*i];
		const float n0_r = channels[4*i+1];
		const float n0_g = channels[4*i+2];
		const float n0_b = channels[4*i+3];


		last_B  = B;
		last_b1 = b1;
		last_b2 = b1;
		last_b3 = b1;


		g_Mem.yvv.FloatBuffer[5][4*i]=  B;
		g_Mem.yvv.FloatBuffer[5][4*i+1]=b1;
		g_Mem.yvv.FloatBuffer[5][4*i+2]=b2;
		g_Mem.yvv.FloatBuffer[5][4*i+3]=b3;


		float w0_a = B*n0_a + b1*w1_a + b2*w2_a + b3*w3_a;
		float w0_r = B*n0_r + b1*w1_r + b2*w2_r + b3*w3_r;
		float w0_g = B*n0_g + b1*w1_g + b2*w2_g + b3*w3_g;
		float w0_b = B*n0_b + b1*w1_b + b2*w2_b + b3*w3_b;

	
		g_Mem.yvv.FloatBuffer[4][4*i]=  w0_a;
		g_Mem.yvv.FloatBuffer[4][4*i+1]=w0_r;
		g_Mem.yvv.FloatBuffer[4][4*i+2]=w0_g;
		g_Mem.yvv.FloatBuffer[4][4*i+3]=w0_b;


		w3_a = w2_a;
		w2_a = w1_a;
		w1_a = w0_a;

		w3_r = w2_r;
		w2_r = w1_r;
		w1_r = w0_r;

		w3_g = w2_g;
		w2_g = w1_g;
		w1_g = w0_g;

		w3_b = w2_b;
		w2_b = w1_b;
		w1_b = w0_b;
	}

	const float a_plus1 = w1_a - last_a * recsum0;
	const float r_plus1 = w1_r - last_r * recsum0;
	const float g_plus1 = w1_g - last_g * recsum0;
	const float b_plus1 = w1_b - last_b * recsum0;

	const float a_plus2 = w2_a - last_a * recsum0;
	const float r_plus2 = w2_r - last_r * recsum0;
	const float g_plus2 = w2_g - last_g * recsum0;
	const float b_plus2 = w2_b - last_b * recsum0;

	const float a_plus3 = w3_a - last_a * recsum0;
	const float r_plus3 = w3_r - last_r * recsum0;
	const float g_plus3 = w3_g - last_g * recsum0;
	const float b_plus3 = w3_b - last_b * recsum0;


	w1_a = B * B * ( M[0] * a_plus1 + M[1] * a_plus2 + M[2] * a_plus3 + last_a * recsum1 );
	w1_r = B * B * ( M[0] * r_plus1 + M[1] * r_plus2 + M[2] * r_plus3 + last_r * recsum1 );
	w1_g = B * B * ( M[0] * g_plus1 + M[1] * g_plus2 + M[2] * g_plus3 + last_g * recsum1 );
	w1_b = B * B * ( M[0] * b_plus1 + M[1] * b_plus2 + M[2] * b_plus3 + last_b * recsum1 );
			   
	w2_a = B * B * ( M[3] * a_plus1 + M[4] * a_plus2 + M[5] * a_plus3 + last_a * recsum1 );
	w2_r = B * B * ( M[3] * r_plus1 + M[4] * r_plus2 + M[5] * r_plus3 + last_r * recsum1 );
	w2_g = B * B * ( M[3] * g_plus1 + M[4] * g_plus2 + M[5] * g_plus3 + last_g * recsum1 );
	w2_b = B * B * ( M[3] * b_plus1 + M[4] * b_plus2 + M[5] * b_plus3 + last_b * recsum1 );
			   
	w3_a = B * B * ( M[6] * a_plus1 + M[7] * a_plus2 + M[8] * a_plus3 + last_a * recsum1 );
	w3_r = B * B * ( M[6] * r_plus1 + M[7] * r_plus2 + M[8] * r_plus3 + last_r * recsum1 );
	w3_g = B * B * ( M[6] * g_plus1 + M[7] * g_plus2 + M[8] * g_plus3 + last_g * recsum1 );
	w3_b = B * B * ( M[6] * b_plus1 + M[7] * b_plus2 + M[8] * b_plus3 + last_b * recsum1 );


	i=width-1; //write the first pixel directly.

	channels[4*i]=  (unsigned char)w1_a;
	channels[4*i+1]=(unsigned char)w1_r;
	channels[4*i+2]=(unsigned char)w1_g;
	channels[4*i+3]=(unsigned char)w1_b;

	
	for(--i; i>0; --i)
	{
		const float n0_a = g_Mem.yvv.FloatBuffer[4][4*i];
		const float n0_r = g_Mem.yvv.FloatBuffer[4][4*i+1];
		const float n0_g = g_Mem.yvv.FloatBuffer[4][4*i+2];
		const float n0_b = g_Mem.yvv.FloatBuffer[4][4*i+3];

		last_B  = B;
		last_b1 = b1;
		last_b2 = b1;
		last_b3 = b1;


		B  = g_Mem.yvv.FloatBuffer[5][4*i];
		b1 = g_Mem.yvv.FloatBuffer[5][4*i+1];
		b2 = g_Mem.yvv.FloatBuffer[5][4*i+2];
		b3 = g_Mem.yvv.FloatBuffer[5][4*i+3];

		float w0_a = B * n0_a + b1*w1_a+b2*w2_a+b3*w3_a;
		float w0_r = B * n0_r + b1*w1_r+b2*w2_r+b3*w3_r;
		float w0_g = B * n0_g + b1*w1_g+b2*w2_g+b3*w3_g;
		float w0_b = B * n0_b + b1*w1_b+b2*w2_b+b3*w3_b;


		channels[4*i]   = (unsigned char)(w0_a>255.f?255.f: CLAMP(w0_a));
		channels[4*i+1] = (unsigned char)(w0_r>255.f?255.f: CLAMP(w0_r));
		channels[4*i+2] = (unsigned char)(w0_g>255.f?255.f: CLAMP(w0_g));
		channels[4*i+3] = (unsigned char)(w0_b>255.f?255.f: CLAMP(w0_b));

		w3_a = w2_a;
		w2_a = w1_a;
		w1_a = w0_a;

		w3_r = w2_r;
		w2_r = w1_r;
		w1_r = w0_r;

		w3_g = w2_g;
		w2_g = w1_g;
		w1_g = w0_g;

		w3_b = w2_b;
		w2_b = w1_b;
		w1_b = w0_b;
	}
}

// b[0] is 1.f/b0
void ComputeBVector(float* b, const float sigma)
{
	// Here come the magic numbers. Straight from the 2002 paper...

	const float m0 = 1.16680f;
	const float m1 = 1.10783f;
	const float m2 = 1.40586f;

	const float m1sq = m1*m1;
	const float m2sq = m2*m2;

	// first, compute q, based on sigma. This only works up to a sigma of 1.

	const float q = 1.31564f*(__builtin_sqrtf(1.f+0.490811f*sigma*sigma)-1.f);

	const float q_square = q * q;
	const float q_cube   = q * q * q;

	// this is the 2002 version, not the 1995 one.

	const float scale = (m0 + q) * (m1sq + m2sq + 2.f*m1*q + q_square);

	b[0] = scale;
	b[1] = q * (2.f * m0 * m1 + m1sq + m2sq + (2.f*m0 + 4.f*m1)*q + 3.f*q_square) / scale;
	b[2] = - q_square * (m0 + 2*m1 + 3.f*q) / scale;
	b[3] = q_cube / scale;
	// the last one is B, normally computed as "m0 * (m1sq + m2sq)/scale".
	// This is however way more precise and prevents overflow of colors, meaning we don't need to clamp.
	// Hotness!
	b[4] = 1.f - b[1] - b[2] - b[3];
}

void ComputeTriggsSums(float* b)
{
	b[5] = 1.f/(1.f - b[1] - b[2] - b[3]); //used for Triggs.
	b[6] = 1.f/(1.f - b[3] - b[2] - b[1]); //used for Triggs.

}

void ComputeTriggsM(float* M, float *b)
{

	//const float a0=-b[0];
	const float a1=-b[1];
	const float a2=-b[2];
	const float a3=-b[3];


	const float scale = 1.f/((1.f+a1-a2+a3)*(1.f-a1-a2-a3)*(1.f+a2+(a1-a3)*a3));
	M[0] = scale*(-a3*a1+1.f-a3*a3-a2);
	M[1] = scale*(a3+a1)*(a2+a3*a1);
	M[2] = scale*a3*(a1+a3*a2);
	M[3] = scale*(a1+a3*a2);
	M[4] = -scale*(a2-1.f)*(a2+a3*a1);
	M[5] = -scale*a3*(a3*a1+a3*a3+a2-1.f);
	M[6] = scale*(a3*a1+a2+a1*a1-a2*a2);
	M[7] = scale*(a1*a2+a3*a2*a2-a1*a3*a3-a3*a3*a3-a3*a2+a3);
	M[8] = scale*a3*(a1+a3*a2);

}

inline void HorizontalFilterSPA(
	uint32_t line0,
	uint32_t line1,
	uint32_t line2,
	uint32_t line3,
	uint32_t line4,
	uint32_t line5,
	uint32_t line6,
	uint32_t line7,
	uint32_t width,
	float* b,
	float*m )
{
	const uint32_t index0=line0&BUFFER_LINE_MASK;
	const uint32_t index1=line1&BUFFER_LINE_MASK;
	const uint32_t index2=line2&BUFFER_LINE_MASK;
	const uint32_t index3=line3&BUFFER_LINE_MASK;
	
#if YVV_MODE == YVV_XRGB

	const uint32_t index4=line4&BUFFER_LINE_MASK;
	const uint32_t index5=line5&BUFFER_LINE_MASK;
	const uint32_t index6=line6&BUFFER_LINE_MASK;
	const uint32_t index7=line7&BUFFER_LINE_MASK;

	{
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1; 
		spu_idisable();
		EDGE_DMA_WAIT_TAG_STATUS_ALL(1<<index0 | 1<<index1 | 1<<index2 | 1<<index3 | 1<<index4 | 1<<index5 | 1<<index6 | 1<<index7);
		if( interrupts_enabled ) {spu_ienable();} 
	}


	edgeYvvHorizontal3(
		g_Mem.yvv.LineBuffer[index0],
		g_Mem.yvv.LineBuffer[index1],
		g_Mem.yvv.LineBuffer[index2],
		g_Mem.yvv.LineBuffer[index3],
		g_Mem.yvv.LineBuffer[index4],
		g_Mem.yvv.LineBuffer[index5],
		g_Mem.yvv.LineBuffer[index6],
		g_Mem.yvv.LineBuffer[index7],
		g_Mem.yvv.FloatBuffer[0],
		g_Mem.yvv.FloatBuffer[1],
		g_Mem.yvv.FloatBuffer[2],
		g_Mem.yvv.FloatBuffer[3],
		g_Mem.yvv.FloatBuffer[4],
		g_Mem.yvv.FloatBuffer[5],
		width,
		b,
		m);
#else

	{
		const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1;
		spu_idisable();
		EDGE_DMA_WAIT_TAG_STATUS_ALL(1<<index0 | 1<<index1 | 1<<index2 | 1<<index3);
		if( interrupts_enabled ) {spu_ienable();}
	}


	edgeYvvHorizontal(
		g_Mem.yvv.LineBuffer[index0],
		g_Mem.yvv.LineBuffer[index1],
		g_Mem.yvv.LineBuffer[index2],
		g_Mem.yvv.LineBuffer[index3],
		g_Mem.yvv.FloatBuffer[0],
		g_Mem.yvv.FloatBuffer[1],
		g_Mem.yvv.FloatBuffer[2],
		g_Mem.yvv.FloatBuffer[3],
		width,
		b,
		m);
#endif
}

#define FETCH_LINE(number)				FetchLine(imageEa,number,pitch,lines,transposedPitch,transposedBlockOffset,mode)
#define STORE_LINE(number)				StoreLine(imageEa,number,pitch,lines,transposedPitch,transposedBlockOffset,mode,spuId)
#define	HORIZONTAL_FILTER(line0,line1,line2,line3,line4,line5,line6,line7) HorizontalFilterSPA(line0,line1,line2,line3,line4,line5,line6,line7,width,b_vector,m)

void YvvHorizontal(
	      uint32_t imageEa,
	const uint32_t lines,
	const uint32_t width,
	const uint32_t pitch,
	const uint32_t transposedPitch,
	const uint32_t transposedBlockOffset,
	const uint32_t mode,
	const uint32_t spuId,
	const bool lastSpu,
	const float sigma0,
	const float sigma1)
{
	(void)lastSpu;
	float b_vector[14] ALIGNED(16);
	float m[9] ALIGNED(16);

	ComputeBVector(b_vector,sigma0);
	ComputeTriggsSums(b_vector);
	ComputeBVector(b_vector+7,sigma1);
	ComputeTriggsSums(b_vector);
	ComputeTriggsM(m,b_vector);

	uint32_t line = 0;
	for(;line<BUFFER_LINES;++line)
	{
		FETCH_LINE(line);
	}
	
	for(line=0;line<lines-BUFFER_LINES;line+=BUFFER_LINES/2)
	{
		HORIZONTAL_FILTER(line,line+1,line+2,line+3,line+4,line+5,line+6,line+7);
		STORE_LINE(line);
		STORE_LINE(line+1);
		STORE_LINE(line+2);
		STORE_LINE(line+3);
#if YVV_MODE == YVV_XRGB
		STORE_LINE(line+4);
		STORE_LINE(line+5);
		STORE_LINE(line+6);
		STORE_LINE(line+7);
#endif

		FETCH_LINE(line+BUFFER_LINES);
		FETCH_LINE(line+BUFFER_LINES+1);
		FETCH_LINE(line+BUFFER_LINES+2);
		FETCH_LINE(line+BUFFER_LINES+3);
#if YVV_MODE == YVV_XRGB
		FETCH_LINE(line+BUFFER_LINES+4);
		FETCH_LINE(line+BUFFER_LINES+5);
		FETCH_LINE(line+BUFFER_LINES+6);
		FETCH_LINE(line+BUFFER_LINES+7);
#endif
	}

	for(;line<lines;line+=BUFFER_LINES/2)
	{
		HORIZONTAL_FILTER(line,line+1,line+2,line+3,line+4,line+5,line+6,line+7);
		STORE_LINE(line);
		if(line+1 < lines) STORE_LINE(line+1);
		if(line+2 < lines) STORE_LINE(line+2);
		if(line+3 < lines) STORE_LINE(line+3);
#if YVV_MODE == YVV_XRGB
		if(line+4 < lines) STORE_LINE(line+4);
		if(line+5 < lines) STORE_LINE(line+5);
		if(line+6 < lines) STORE_LINE(line+6);
		if(line+7 < lines) STORE_LINE(line+7);
#endif
	}
}

int cellSpursTaskMain(qword argTask, uint64_t argTaskset)
{
	(void)argTaskset;

	while (1)
	{
		// Wait for a signal
		cellSpursWaitSignal();

		const uint32_t	parameter_ea = spu_extract( (vec_uint4)argTask, 0 );
		YvvParameters	parameter;
		const uint32_t	parameter_size = sizeof(parameter);


		{
			const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1;
			spu_idisable();
			EDGE_DMA_GET(&parameter,parameter_ea,parameter_size,0,0,0);
			EDGE_DMA_WAIT_TAG_STATUS_ALL(1); //wait for data to arrive.
			if( interrupts_enabled ) {spu_ienable();} 
		}


		if(0 == parameter.mode)
		{
			const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1;
			spu_idisable();
			for(uint32_t i=0;i<parameter.imageHeight;++i)
			{
				const uint32_t index=i&BUFFER_LINE_MASK;
				EDGE_DMA_GETF(g_Mem.yvv.LineBuffer[index],parameter.imageAddress+i*parameter.imagePitch,parameter.imageWidth * 4, index,0,0);
				EDGE_DMA_PUTF(g_Mem.yvv.LineBuffer[index],parameter.destAddress+i*parameter.imagePitch,parameter.imageWidth * 4, index,0,0);
			}
			if( interrupts_enabled ) {spu_ienable();} 
		}
		else
		{
			const bool last_spu = parameter.spuId==parameter.spuCount-1;
			{
				//lines are actually line...
				const uint32_t lines_per_spu = parameter.imageHeight / parameter.spuCount;
				const uint32_t lines_for_last_spu =
					parameter.imageHeight - (parameter.spuCount - 1) * lines_per_spu;

				const uint32_t address_offset = sizeof(uint32_t) * parameter.spuId * lines_per_spu * parameter.imagePitch ;

				YvvHorizontal(
					parameter.imageAddress+address_offset,
					last_spu?lines_for_last_spu:lines_per_spu,
					parameter.imagePitch,
					parameter.imagePitch*4,
					0, //this is only needed for working on transposed buffers.
					0, //this is only needed for working on transposed buffers.
					1,
					parameter.spuId,
					last_spu,
					parameter.sigmaH,
					0);
			}			
			TransposePass(parameter);
			if(true)
			{
				//here things are a bit different, as we're on a tiled image

				const uint32_t lines_per_spu = parameter.imagePitch / parameter.spuCount;
				const uint32_t lines_for_last_spu = 
					parameter.imagePitch - (parameter.spuCount - 1) * lines_per_spu;

				const uint32_t base_address_offset = parameter.spuId * lines_per_spu;

				const uint32_t block_offset = base_address_offset % YVV_TRANSPOSE_BLOCK_SIZE;
				const uint32_t address_offset = sizeof(uint32_t) * (base_address_offset - block_offset);

				const uint32_t transpose_height = align(parameter.imageHeight,YVV_TRANSPOSE_BLOCK_SIZE);
				
				YvvHorizontal(
					parameter.imageAddress+address_offset,
					last_spu?lines_for_last_spu:lines_per_spu,
					parameter.imageHeight,
					transpose_height*4,
					parameter.imagePitch*4,
					block_offset,
					0,
					parameter.spuId,
					last_spu,
					parameter.sigmaV,
					0.f);

			}
			TransposePass(parameter);
		}

		if (parameter.rsxLabelAddress != 0)
		{
			const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1;
			spu_idisable();
			EDGE_DMA_PUT_UINT32(parameter.rsxLabelValue,parameter.rsxLabelAddress,0,0,0);
			if( interrupts_enabled ) {spu_ienable();}
		}
		
		if(parameter.taskCounterAddress)
		{
			const bool interrupts_enabled = spu_readch(SPU_RdMachStat) & 1;
			spu_idisable();
			ALIGNED(128) uint32_t atomic_buffer[32];
			EDGE_DMA_WAIT_TAG_STATUS_ALL(0xffff); //make sure all is done before we let anyone else into LS.
			cellAtomicIncr32(atomic_buffer,parameter.taskCounterAddress);
			if( interrupts_enabled ) {spu_ienable();}
		}
	}

	return 0;
}
